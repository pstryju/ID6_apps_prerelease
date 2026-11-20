"""Exercise selected channels through the actual MOST-only worker and fake DME."""
from pathlib import Path
import os
import socket
import subprocess
import time
import unittest

WORKER=Path(__file__).resolve().parents[1]/'build/test-boost-configurable'

class ConfigurableMost(unittest.TestCase):
    def collect(self, mask, diesel=False, mode='up'):
        frames=[]
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as udp:
            udp.bind(('127.0.0.1',0));udp.settimeout(.1)
            args=[str(WORKER),'--signals',str(mask),'--udp-port',str(udp.getsockname()[1]),'--cycles','1','--trace']
            if diesel: args.append('--diesel')
            with subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                                  env=dict(os.environ,BOOST_MOST_FAKE=mode)) as process:
                deadline=time.monotonic()+10
                while time.monotonic()<deadline:
                    try:
                        data=udp.recv(1024).decode().split()
                        self.assertEqual(data[0],'NBG7')
                        self.assertEqual(len(data),5+31*2)
                        frames.append((int(data[3]),[float(data[5+2*i]) for i in range(31)]))
                    except socket.timeout:
                        if process.poll() is not None: break
                out,err=process.communicate(timeout=2)
                self.assertEqual(process.returncode,0,err.decode())
        self.assertTrue(frames)
        self.assertEqual(frames[-1][0],0)
        return frames,err.decode()

    def test_only_selected_rail_and_voltage_are_queried(self):
        frames,log=self.collect((1<<5)|(1<<10))
        best=max(frames,key=lambda item:item[0].bit_count())
        self.assertEqual(best[0],(1<<5)|(1<<10))
        self.assertAlmostEqual(best[1][5],102.6)
        self.assertAlmostEqual(best[1][10],14.2)
        for query in ('224201','224205','225889','224A49','22580F','22586F','010C','010D','0105','015C','14','2F','10'):
            self.assertIn(query+'=0 ',log)
        self.assertIn('0123=1 ',log);self.assertIn('0142=1 ',log)

    def test_refused_selected_channel_is_blank(self):
        frames,log=self.collect(1<<10,mode='reject-voltage')
        self.assertFalse(any(mask for mask,values in frames))

    def test_diesel_boost_requires_fresh_independent_barometer(self):
        frames,log=self.collect(1,diesel=True)
        best=max(frames,key=lambda item:item[0])
        self.assertTrue(best[0]&1)
        self.assertAlmostEqual(best[1][0],50*.145037738,places=5)
        self.assertIn('224205=0 ',log)
        frames,log=self.collect(1,diesel=True,mode='reject-baro')
        self.assertFalse(any(mask&1 for mask,values in frames))

if __name__=='__main__': unittest.main()
