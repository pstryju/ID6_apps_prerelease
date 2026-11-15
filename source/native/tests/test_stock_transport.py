"""Real stock worker, with captured N20 problem channels and simulated others."""
import json
from pathlib import Path
import socket
import struct
import subprocess
import threading
import unittest
from test_transport import exact,frame

ROOT=Path(__file__).resolve().parents[1]


class StockTransportTest(unittest.TestCase):
    def run_capture(self,oil_reply=None):
        captured=json.loads((ROOT/'tests/fixtures/n20-readings.json').read_text())
        replies=captured['responses']
        replies.update({'225889':'625889104e','224a49':'624a49004e','22580f':'62580f82','22da12':'62da12008e','22586f':'62586f09c1'})
        if oil_reply is not None: replies['015c']=oil_reply
        requests=['224201','224205','225889','224a49','22580f','22da12','2256d7','0123','22586f','0105','015c']
        errors=[]
        with socket.socket() as listener,socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as udp:
            listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(3)
            udp.bind(('127.0.0.1',0));udp.settimeout(3)
            def serve():
                try:
                    conn,_=listener.accept()
                    with conn:
                        for request in requests:
                            size,control=struct.unpack('>IH',exact(conn,6))
                            ecu=0x18 if request=='22da12' else 0x12
                            self.assertEqual(control,1)
                            self.assertEqual(exact(conn,size),bytes([0xf4,ecu])+bytes.fromhex(request))
                            conn.sendall(frame(1,bytes([ecu,0xf4])+bytes.fromhex(replies[request])))
                        self.assertEqual(conn.recv(1),b'')
                except Exception as error: errors.append(error)
            thread=threading.Thread(target=serve);thread.start()
            result=subprocess.run([ROOT/'build/stock-collector','--host','127.0.0.1','--port',str(listener.getsockname()[1]),'--udp-port',str(udp.getsockname()[1]),'--cycles','1'],capture_output=True,text=True,timeout=5)
            thread.join(4)
            self.assertFalse(thread.is_alive())
            if errors: raise errors[0]
            self.assertEqual(result.returncode,0,result.stderr)
            fields=udp.recv(512).decode().split()
            self.assertEqual(fields[0],'NBG5')
            return fields

    def test_captured_coolant_rail_and_oil_are_displayable(self):
        fields=self.run_capture()
        self.assertEqual(int(fields[3]),511)
        self.assertAlmostEqual(float(fields[5+2*5]),104)
        self.assertAlmostEqual(float(fields[5+2*7]),90)
        self.assertAlmostEqual(float(fields[5+2*8]),89)
        self.assertAlmostEqual(float(fields[5]),-.01953125*.145037738,places=5)

    def test_rejected_oil_temperature_does_not_fabricate_a_value(self):
        fields=self.run_capture('7f0131')
        self.assertEqual(int(fields[3]),255)


if __name__=='__main__': unittest.main()
