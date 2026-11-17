import pathlib
import socket
import struct
import subprocess
import threading
import unittest

WORKER=str(pathlib.Path(__file__).resolve().parents[1]/'build/xdrive-control')
def exact(conn,n):
    data=b''
    while len(data)<n:
        chunk=conn.recv(n-len(data))
        if not chunk: raise AssertionError('worker closed before expected request')
        data+=chunk
    return data

ID=('29','22F150','62F1500F2850')
CAP=('29','22DBE1','62DBE100')
AWD=('19','22DB32','62DB320000000000000020')
RWD=('19','22DB32','62DB3200000000000000C0')
PARK=('18','22DA2E','62DA2E0000')
RPM=('12','010C','410C0000')
SPEED=('12','010D','410D00')

class XDrive(unittest.TestCase):
    def exchange(self,args,script):
        errors=[]
        with socket.socket() as server:
            server.bind(('127.0.0.1',0));server.listen();server.settimeout(3)
            def run():
                try:
                    conn,_=server.accept()
                    with conn:
                        conn.settimeout(3)
                        for ecu,request,response in script:
                            n,kind=struct.unpack('>IH',exact(conn,6));self.assertEqual(kind,1)
                            self.assertEqual(exact(conn,n),bytes.fromhex('F4'+ecu+request))
                            if response is not None:
                                body=bytes.fromhex(ecu+'F4'+response)
                                conn.sendall(struct.pack('>IH',len(body),1)+body)
                        self.assertEqual(conn.recv(1),b'', 'unexpected extra read/write')
                except Exception as e:errors.append(e)
            thread=threading.Thread(target=run);thread.start()
            result=subprocess.run([WORKER,'--host','127.0.0.1',*args,'--port',str(server.getsockname()[1])],capture_output=True,text=True,timeout=7)
            thread.join(4);self.assertFalse(thread.is_alive());self.assertEqual(result.returncode,0,result.stderr)
            if errors:raise errors[0]
            return result.stdout

    def test_status_is_read_only(self):
        self.assertIn('reports AWD',self.exchange(['--status','0'],[ID,CAP,AWD]))

    def test_unsupported_dsc_sends_no_control(self):
        self.assertIn('DSC 123456 is outside the supported control list',self.exchange(['--drive-mode','0'],[('29','22F150','62F150123456')]))

    def test_identification_failure_is_distinguished_without_writes(self):
        for response,message in ((None,'No DSC reply at 0x29'),
                                 ('7F2231','DSC identification refused (NRC 31)'),
                                 ('62F1500000','unrecognized identification format'),
                                 ('62F1500F2100','DSC 0F2100 is outside the supported control list')):
            self.assertIn(message,self.exchange(['--status','0'],[('29','22F150',response)]))

    def test_unavailable_park_or_running_engine_or_speed_blocks_write(self):
        for tail in ([('18','22DA2E','7F2231')],
                     [PARK,('12','010C','410C0B1A')],
                     [PARK,RPM,('12','010D','410D01')]):
            self.assertIn('engine off',self.exchange(['--drive-mode','0'],[ID,CAP,AWD,*tail]))

    def test_missing_capability_blocks_write(self):
        self.assertIn('DSC mode read refused (NRC 31)',self.exchange(['--drive-mode','0'],[ID,('29','22DBE1','7F2231')]))

    def test_ack_requires_transfer_case_readback(self):
        command=('29','2EDBE100','6EDBE1')
        self.assertIn('confirms RWD',self.exchange(['--drive-mode','0'],[ID,CAP,AWD,PARK,RPM,SPEED,command,RWD]))
        self.assertIn('unconfirmed',self.exchange(['--drive-mode','0'],[ID,CAP,AWD,PARK,RPM,SPEED,command,AWD]))

    def test_restore_requires_supported_dsc_and_is_checked(self):
        old=('29','22F150','62F1500F2510')
        self.assertIn('Cycle the ignition',self.exchange(['--drive-mode','1'],[old,CAP,RWD]))
        self.assertIn('confirms AWD',self.exchange(['--drive-mode','1'],[ID,CAP,RWD,PARK,RPM,SPEED,('29','2EDBE101','6EDBE1'),AWD]))

if __name__=='__main__':unittest.main()
