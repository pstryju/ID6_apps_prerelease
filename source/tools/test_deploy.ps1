# Generate the actual remote transaction scripts for the Linux fixture tests.
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/EvoDeploy.ps1"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$folder=Join-Path $root '.local/deploy-tests'
New-Item -ItemType Directory -Force $folder | Out-Null
# Exercise the real Windows process transport without starting SSH.
$fixture=Join-Path $folder ('ssh-fixture-'+[Guid]::NewGuid().ToString('N')+'.exe')
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Diagnostics;
public class SshFixture {
    public static void Main(string[] args) {
        if (args[0] == "--slow") {
            System.Threading.Thread.Sleep(6000);
            Console.Write("ssh-delay-ok");
        } else if (args[0] == "--stream") {
            Console.WriteLine("phase one"); Console.Out.Flush();
            System.Threading.Thread.Sleep(200);
            Console.WriteLine("phase two"); Console.Out.Flush();
        } else if (args[0] == "--echo") {
            Console.OpenStandardInput().CopyTo(Console.OpenStandardOutput());
        } else if (args[0] == "--askpass" || args[0] == "--reject-prompt") {
            var info = new ProcessStartInfo(Environment.GetEnvironmentVariable("SSH_ASKPASS"));
            info.UseShellExecute=false;
            info.RedirectStandardOutput=true;
            info.Arguments=args[0] == "--askpass" ? "\"root@fixture password:\"" : "\"Are you sure you want to continue connecting?\"";
            using (var child=Process.Start(info)) {
                child.StandardOutput.BaseStream.CopyTo(Console.OpenStandardOutput());
                child.WaitForExit();
                Environment.ExitCode=child.ExitCode;
            }
        } else if (args[0] == "--error") {
            Console.Error.WriteLine("fixture authentication failed");
            Environment.ExitCode=17;
        } else {
            for (int i=1; i<args.Length; i++)
                Console.WriteLine(Convert.ToBase64String(Encoding.UTF8.GetBytes(args[i])));
        }
    }
}
'@ -OutputAssembly $fixture -OutputType ConsoleApplication
$script:Ssh=$fixture
$script:SshArgs=@('--echo')
[byte[]]$payload=0..255
$received=Invoke-Evo -Command 'ignored' -InputBytes $payload -Bytes
if ((Get-BytesHash $received) -ne (Get-BytesHash $payload)) { throw 'Binary SSH transport changed bytes.' }
$script:SshArgs=@('--argv', 'value with spaces', 'trailing\', 'double"quote')
$command='printf ''%s'' "$value"; echo "a b"'
$result=Invoke-Evo -Command $command
$expected=(@($script:SshArgs | Select-Object -Skip 1)+@($command) | ForEach-Object {
    [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($_))
}) -join "`n"
if ($result.Replace("`r`n","`n") -ne $expected) { throw 'Windows SSH argument quoting changed arguments.' }
Write-Host 'Windows process transport: all 256 byte values and quoted arguments preserved.'
Initialize-EvoConnection -Head '169.254.199.119' -Password (ConvertTo-SecureString 'fixture-default' -AsPlainText -Force)
if ('HostKeyAlgorithms=+ssh-rsa' -notin $script:SshArgs -or 'MACs=+hmac-sha1' -notin $script:SshArgs) {
    throw 'Verified Evo SSH options are not enabled by default.'
}
if ('ConnectTimeout=15' -notin $script:SshArgs -or
    'PreferredAuthentications=password,keyboard-interactive' -notin $script:SshArgs) {
    throw 'Connection settings differ from the working deploy-evo.ps1 script.'
}
if (@($script:SshArgs | Where-Object { $_ -match '^(KexAlgorithms|Ciphers)=' }).Count) {
    throw 'Unexpected KEX/cipher overrides.'
}
if (-not $script:EvoPassword.Length) { throw 'Explicit password was not loaded.' }
$script:EvoPassword.Dispose()
$script:EvoPassword=$null
$script:Ssh=$fixture
$script:SshArgs=@('--error')
$reported=$false
try { $null=Invoke-Evo -Command 'ignored' }
catch { $reported=$_.Exception.Message.Contains('fixture authentication failed') }
if (-not $reported) { throw 'SSH stderr was not included in the failure message.' }
Write-Host 'Default Evo SSH options, explicit password loading and SSH error reporting verified.'
Initialize-EvoAskPass
$testPassword='fixture&value"with spaces\end'
$script:EvoPassword=ConvertTo-SecureString $testPassword -AsPlainText -Force
$script:SshArgs=@('--askpass')
$answer=Invoke-Evo -Command 'ignored'
if ($answer -cne $testPassword) { throw 'Askpass did not preserve the synthetic password.' }
if ($env:SSH_ASKPASS -eq $script:EvoAskPass) { throw 'Askpass environment leaked to the parent process.' }
$script:SshArgs=@('--reject-prompt')
$rejected=$false
try { $null=Invoke-Evo -Command 'ignored' } catch { $rejected=$true }
if (-not $rejected) { throw 'Askpass answered a non-password prompt.' }
$script:EvoPassword.Dispose()
$script:EvoPassword=$null
Write-Host 'Askpass: synthetic password passed through a user-only pipe; host-key prompt rejected.'
$script:SshArgs=@('--slow')
$observed=@(Invoke-Evo -Command 'ignored' -Activity 'Test oczekiwania' 6>&1)
$notices=@($observed | Where-Object { $_ -is [Management.Automation.InformationRecord] } | ForEach-Object { $_.MessageData.ToString() })
$returned=@($observed | Where-Object { $_ -isnot [Management.Automation.InformationRecord] })
if (-not @($notices | Where-Object { $_ -match 'nadal czekam.*Test oczekiwania' }).Count) { throw 'SSH wait heartbeat missing.' }
if ($returned.Count -ne 1 -or $returned[0] -cne 'ssh-delay-ok') { throw 'Progress messages contaminated the SSH return value.' }
Write-Host 'Progress: wait heartbeat emitted; SSH return value preserved.'
$script:SshArgs=@('--stream')
$streamed=@(Invoke-Evo -Command 'ignored' -StreamText -Activity 'Test komunikatow Evo' 6>&1)
$data=@($streamed | Where-Object { $_ -isnot [Management.Automation.InformationRecord] })
if ($data.Count -ne 1 -or $data[0].Replace("`r`n","`n") -cne "phase one`nphase two") { throw 'Streamed SSH output changed.' }
if (-not @($streamed | Where-Object { $_ -is [Management.Automation.InformationRecord] -and $_.MessageData.ToString().Contains('Evo: phase one') }).Count) { throw 'Remote progress not displayed.' }
$script:Plugins='/EVO_TEST/Plugins'
$script:Lock='/EVO_TEST/lock'
$items=@(@{name='BoostDeck';hadOriginal=$true}, @{name='ShiftDeck';hadOriginal=$false})
foreach ($mode in @('install','uninstall')) {
    $text=Get-EvoTransaction -Run '/EVO_TEST/run' -Items $items -Uninstall:($mode -eq 'uninstall')
    [IO.File]::WriteAllText((Join-Path $folder "$mode.sh"), $text.Replace("`r`n","`n")+"`n", [Text.UTF8Encoding]::new($false))
}
Write-Host 'Generated .local/deploy-tests/{install,uninstall}.sh; run native/tests/test_deploy.py in the host-test container.'
. "$PSScriptRoot/EvoBatch.ps1"
# Bootstrap fixtures use a dummy helper prefix; Linux tests replace it with
# the compiled native host helper before running the actual generated scripts.
function Get-EvoFrameworkChecks([string]$Helper) { return "echo 'Fixture target verification'" }
$stub=Join-Path $folder 'helper.stub'
[IO.File]::WriteAllBytes($stub,[Text.Encoding]::ASCII.GetBytes('EVOSTUB'))
$candidateBytes=[Text.Encoding]::ASCII.GetBytes('candidate')
$hash=Get-BytesHash $candidateBytes
$items=@(@{name='BoostDeck';hadOriginal=$true;files=@(@{name='version';sha256=$hash})},
    @{name='ShiftDeck';hadOriginal=$false;files=@(@{name='version';sha256=$hash})})
foreach($mode in @('install','uninstall')) {
    $run=if($mode -eq 'install'){'/EVO_TEST/run'}else{'/EVO_TEST/uninstall'}
    $plan=if($mode -eq 'install'){Get-EvoInstallScript -Run $run -Items $items}else{Get-EvoUninstallScript -Run '/EVO_TEST/run' -Items $items -Helper "$run/fileops"}
    $entries=@(@{name='deploy.sh';bytes=[Text.Encoding]::UTF8.GetBytes($plan+"`n")})
    if($mode -eq 'install'){
        foreach($item in $items){$entries+=@{name="stage/$($item.name)/version";bytes=$candidateBytes}}
    }
    [IO.File]::WriteAllBytes((Join-Path $folder "$mode.bundle"),(New-EvoBundle -Helper $stub -Entries $entries))
    [IO.File]::WriteAllText((Join-Path $folder "$mode-bootstrap.sh"),(Get-EvoBatchBootstrap -Run $run -Helper $stub),[Text.UTF8Encoding]::new($false))
}
Write-Host 'Generated batch bootstrap fixtures for source/native/tests/test_batch_deploy.py.'
