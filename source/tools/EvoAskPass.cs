// OpenSSH askpass bridge. Passwords travel over a per-request, user-only pipe.
using System;
using System.IO;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using System.Security;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Text;
using System.Threading.Tasks;

namespace EvoAuth {
    public sealed class Request : IDisposable {
        public readonly string PipeName = "evo-ssh-" + Guid.NewGuid().ToString("N");
        public readonly string Token = Guid.NewGuid().ToString("N");
        private readonly NamedPipeServerStream pipe;
        private readonly SecureString password;
        private readonly Task worker;

        public Request(SecureString value) {
            password = value.Copy();
            password.MakeReadOnly();
            var security = new PipeSecurity();
            security.SetAccessRuleProtection(true, false);
            security.AddAccessRule(new PipeAccessRule(WindowsIdentity.GetCurrent().User,
                PipeAccessRights.FullControl, AccessControlType.Allow));
            pipe = new NamedPipeServerStream(PipeName, PipeDirection.InOut, 1,
                PipeTransmissionMode.Byte, PipeOptions.Asynchronous, 4096, 4096, security);
            worker = Task.Run((Action)Serve);
        }

        private void Serve() {
            IntPtr plain = IntPtr.Zero;
            char[] chars = null;
            byte[] bytes = null;
            try {
                pipe.WaitForConnection();
                // Read bytewise to avoid a StreamReader buffering the outgoing phase.
                var token = new StringBuilder();
                for (int i=0; i<64; i++) {
                    int b = pipe.ReadByte();
                    if (b < 0) return;
                    if (b == 10) break;
                    token.Append((char)b);
                }
                if (token.ToString() != Token) return;
                plain = Marshal.SecureStringToGlobalAllocUnicode(password);
                chars = new char[password.Length];
                for (int i=0; i<chars.Length; i++) chars[i]=(char)Marshal.ReadInt16(plain,i*2);
                bytes = Encoding.UTF8.GetBytes(chars);
                pipe.Write(bytes, 0, bytes.Length);
                pipe.WriteByte(10);
                pipe.Flush();
            } catch (IOException) { }
              catch (ObjectDisposedException) { }
            finally {
                if (plain != IntPtr.Zero) Marshal.ZeroFreeGlobalAllocUnicode(plain);
                if (chars != null) Array.Clear(chars, 0, chars.Length);
                if (bytes != null) Array.Clear(bytes, 0, bytes.Length);
            }
        }

        public void Dispose() {
            pipe.Dispose();
            // Closing the pipe unblocks a request if SSH never invoked askpass.
            if (worker.Wait(5000)) password.Dispose();
        }
    }

    public static class AskPass {
        public static int Main(string[] args) {
            // Never answer host-key confirmations or private-key passphrase prompts.
            if (args.Length != 1 || args[0].IndexOf("password", StringComparison.OrdinalIgnoreCase)<0)
                return 1;
            string name = Environment.GetEnvironmentVariable("EVO_ASKPASS_PIPE");
            string token = Environment.GetEnvironmentVariable("EVO_ASKPASS_TOKEN");
            if (String.IsNullOrEmpty(name) || String.IsNullOrEmpty(token)) return 1;
            try {
                using (var pipe = new NamedPipeClientStream(".", name, PipeDirection.InOut)) {
                    pipe.Connect(10000);
                    byte[] key = Encoding.ASCII.GetBytes(token + "\n");
                    pipe.Write(key,0,key.Length);
                    pipe.Flush();
                    Array.Clear(key,0,key.Length);
                    using (var output = Console.OpenStandardOutput()) {
                        for (int i=0; i<4096; i++) {
                            int b=pipe.ReadByte();
                            if (b<0) return 1;
                            output.WriteByte((byte)b);
                            if (b==10) { output.Flush(); return 0; }
                        }
                    }
                }
            } catch (IOException) { }
              catch (TimeoutException) { }
            return 1;
        }
    }
}
