import os
import pathlib
import signal
import socket
import subprocess
import tempfile
import time
import unittest

WORKER = str(pathlib.Path(__file__).resolve().parents[1] / 'build/test-boost-most')

class MostBoost(unittest.TestCase):
    def run_worker(self, mode='up', cycles='1', stop=False, worker=WORKER, profile=None):
        frames = []
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
            udp.bind(('127.0.0.1', 0))
            udp.settimeout(.1)
            args = [worker, '--udp-port', str(udp.getsockname()[1]), '--trace']
            if profile:
                args += ['--profile', profile]
            if cycles:
                args += ['--cycles', cycles]
            p = subprocess.Popen(args, stderr=subprocess.PIPE, stdout=subprocess.PIPE,
                                 env=dict(os.environ, BOOST_MOST_FAKE=mode))
            try:
                deadline = time.monotonic() + 14
                while time.monotonic() < deadline:
                    try:
                        fields = udp.recv(512).decode().split()
                        self.assertEqual(fields[0], 'NBG5')
                        frames.append((int(fields[3]), [float(fields[5+2*i]) for i in range(9)]))
                        if stop and frames[-1][0]:
                            p.send_signal(signal.SIGTERM)
                            stop = False
                    except socket.timeout:
                        if p.poll() is not None:
                            break
                out, err = p.communicate(timeout=2)
            finally:
                if p.poll() is None:
                    p.kill()
                    p.communicate()
        return p.returncode, frames, err.decode()

    def test_captured_n20_readings_and_allowlist(self):
        rc, frames, err = self.run_worker()
        self.assertEqual(rc, 0, err)
        mask, values = max(frames, key=lambda f: f[0].bit_count())
        self.assertEqual(mask, 111)  # boost, AFR, timing, IAT, rail, oil only
        self.assertAlmostEqual(values[0], (98.6015625-98.53515625)*.145037738, places=5)
        self.assertAlmostEqual(values[1], 0x1017*16/65535*14.7, places=5)
        self.assertEqual(values[2:4], [-2, 33])
        self.assertAlmostEqual(values[5], 102.6)
        self.assertAlmostEqual(values[6], 2.517)
        self.assertEqual(frames[-1][0], 0)
        self.assertIn('target=1 client=1 free=1', err)
        for request in ('2256D7', '22DA12', '14', '2F', '10'):
            self.assertIn(request+'=0 ', err)

    def test_ambient_rejection_never_fabricates_boost(self):
        rc, frames, err = self.run_worker('reject-ambient')
        self.assertEqual(rc, 0, err)
        self.assertTrue(any(f[0] & (1<<5) for f in frames))
        self.assertFalse(any(f[0] & 1 for f in frames))

    def test_rejected_rail_is_blank(self):
        rc, frames, err = self.run_worker('reject-rail')
        self.assertEqual(rc, 0, err)
        self.assertFalse(any(f[0] & (1<<5) for f in frames))

    def test_link_loss_and_missing_service(self):
        for mode in ('drop', 'missing', 'no-target'):
            rc, frames, err = self.run_worker(mode)
            self.assertEqual(rc, 1, (mode, err))
            self.assertEqual(frames[-1][0], 0)

    def test_stop_on_slow_tunnel(self):
        rc, frames, err = self.run_worker('slow', cycles=None, stop=True)
        self.assertEqual(rc, 0, err)
        self.assertEqual(frames[-1][0], 0)
        self.assertIn('target=1 client=1 free=1', err)

    def test_hud_producer_reads_boost_only(self):
        worker=str(pathlib.Path(WORKER).with_name('test-hud-boost'))
        rc, frames, err=self.run_worker(worker=worker)
        self.assertEqual(rc,0,err)
        self.assertEqual(max(f[0] for f in frames),1)
        for request in ('225889','224A49','22580F','22586F','0123'):
            self.assertIn(request+'=0 ',err)

    def test_auto_requires_explicit_unambiguous_engine_identification(self):
        rc, frames, err = self.run_worker('auto-n20', profile='auto')
        self.assertEqual(rc, 0, err)
        self.assertEqual(max(f[0] for f in frames), 111)
        self.assertIn('read-only n20 profile', err)
        for request in ('22F150', '22F187', '22F189'):
            self.assertIn(request+'=1 ', err)
        for mode in ('up', 'auto-conflict', 'auto-b48'):
            rc, frames, err = self.run_worker(mode, profile='auto')
            self.assertEqual(rc, 3, err)
            self.assertFalse(any(f[0] for f in frames))
            for request in ('224201','224205','225889','0123','14','2F','10'):
                self.assertIn(request+'=0 ', err)

    def test_unvalidated_manual_profile_never_opens_most(self):
        for profile in ('b48','n13','unknown'):
            rc, frames, err = self.run_worker(profile=profile)
            self.assertEqual(rc, 2 if profile=='unknown' else 3, err)
            self.assertFalse(frames)
            self.assertNotIn('FAKE_MOST_CLOSE', err)

    def test_profile_setting_beside_worker_and_explicit_override(self):
        with tempfile.TemporaryDirectory() as directory:
            worker = pathlib.Path(directory)/'worker'
            worker.symlink_to(WORKER)
            setting = pathlib.Path(directory)/'engine-profile.txt'
            setting.write_bytes(b'n20\r\n')
            rc, frames, err = self.run_worker(worker=str(worker))
            self.assertEqual(rc, 0, err)
            self.assertEqual(max(f[0] for f in frames), 111)
            for invalid in (b'', b'n20\nauto', b'n20\0', b'unknown', b'x'*32):
                setting.write_bytes(invalid)
                rc, frames, err = self.run_worker(worker=str(worker))
                self.assertEqual(rc, 2, err)
                self.assertNotIn('FAKE_MOST_CLOSE', err)
            setting.write_bytes(b'b48\n')
            rc, frames, err = self.run_worker(worker=str(worker), profile='n20')
            self.assertEqual(rc, 0, err)
            self.assertEqual(max(f[0] for f in frames), 111)

if __name__ == '__main__':
    unittest.main()
