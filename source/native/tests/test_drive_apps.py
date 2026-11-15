"""Verify the two new workers on a simulated HSFZ gateway."""
from pathlib import Path
import socket
import struct
import subprocess
import threading
import unittest

from test_transport import exact, frame

BUILD = Path(__file__).resolve().parents[1] / "build"


class DriveAppsTest(unittest.TestCase):
    def test_signal_probe_is_bounded_and_read_only(self):
        expected=['010c','0105','224300','2256d7','0123','015c','0142','0104','224201','224205','010b','0133']
        errors=[]
        with socket.socket() as listener:
            listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(5)
            def serve():
                try:
                    for request in expected:
                        conn,_=listener.accept()
                        with conn:
                            conn.settimeout(2)
                            size,control=struct.unpack('>IH',exact(conn,6))
                            self.assertEqual(control,1)
                            self.assertEqual(exact(conn,size),bytes.fromhex('f412'+request))
                            raw=bytes.fromhex(request)
                            response=bytes.fromhex('7f2231') if request=='224300' else bytes([raw[0]+0x40])+raw[1:]+b'\x00'
                            conn.sendall(frame(1,b'\x12\xf4'+response))
                except Exception as error: errors.append(error)
            thread=threading.Thread(target=serve);thread.start()
            result=subprocess.run([BUILD/'diagnostic-probe','--host','127.0.0.1','--port',str(listener.getsockname()[1]),'--signals'],capture_output=True,text=True,timeout=8)
            thread.join(5)
            self.assertFalse(thread.is_alive())
            if errors: raise errors[0]
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertIn('READ BMW-coolant request=224300 result=-2 NRC=31',result.stdout)
            self.assertIn('READ oil-temperature request=015C',result.stdout)

    def test_probe_uses_only_three_obd_reads(self):
        result=self.run_worker("diagnostic-probe", [], [
            ("010c", ["410c1f40"]), ("010d", ["410d00"]), ("0105", ["41055a"])])
        self.assertIn("TCP CONNECTED",result)
        self.assertIn("READ RPM request=010C result=4 response=410C1F40",result)
        self.assertIn("READ coolant request=0105 result=3 response=41055A",result)

    def test_stock_dme_reads_faults_with_standard_uds(self):
        result = self.run_worker("engine-codes", ["--codes"],
                                 [("1902af", ["5902af12020801"])])
        self.assertIn("Engine: 1 code", result)
        self.assertIn("120208  status 01", result)

    def test_clear_rechecks_fault_memory_after_positive_ack(self):
        result = self.run_worker("engine-codes", ["--clear"],
                                 [("14ffffff", ["54"]), ("1902af", ["5902af"])])
        self.assertIn("Engine: 0 codes after clear", result)

    def test_clear_retries_only_after_extended_session_ack(self):
        result = self.run_worker("engine-codes", ["--clear"], [
            ("14ffffff", ["7f1422"]), ("1003", ["5003"]),
            ("14ffffff", ["54"]), ("1001", ["5001"]),
            ("1902af", ["5902af"])])
        self.assertIn("Engine: 0 codes after clear", result)

    def test_clear_refused_session_does_not_repeat_clear(self):
        result = self.run_worker("engine-codes", ["--clear"], [
            ("14ffffff", ["7f1422"]), ("1003", ["7f1033"])])
        self.assertIn("Clear refused (NRC 22)", result)

    def run_worker(self, worker, args, transactions):
        errors = []
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            listener.settimeout(3)

            def gateway():
                try:
                    conn, _ = listener.accept()
                    with conn:
                        conn.settimeout(3)
                        for request, responses in transactions:
                            size, control = struct.unpack(">IH", exact(conn, 6))
                            self.assertEqual(control, 1)
                            self.assertEqual(exact(conn, size), bytes.fromhex("f412"+request))
                            for response in responses:
                                conn.sendall(frame(1, bytes.fromhex("12f4"+response)))
                        self.assertEqual(conn.recv(1), b"", "Unexpected repeat or extra command")
                except Exception as error:
                    errors.append(error)

            thread = threading.Thread(target=gateway)
            thread.start()
            result = subprocess.run(
                [BUILD/worker, "--host", "127.0.0.1", "--port", str(listener.getsockname()[1]), *args],
                capture_output=True, text=True, check=True, timeout=6)
            thread.join(4)
            self.assertFalse(thread.is_alive())
            if errors:
                raise errors[0]
            return result.stdout

    def test_map_two_has_only_one_request_and_requires_matching_echo(self):
        result = self.run_worker("mhd-maps", ["--map", "2"],
                                 [("2f0101", ["7f2f78", "6f0100", "6f0101"])])
        self.assertEqual(result, "Confirmed: Map 2\n")

    def test_wrong_echo_does_not_confirm_and_refusal_is_reported(self):
        result = self.run_worker("mhd-maps", ["--map", "1"], [("2f0100", ["6f0101"])])
        self.assertIn("Map result unknown", result)
        result = self.run_worker("mhd-maps", ["--map", "1"], [("2f0100", ["7f2f31"])])
        self.assertIn("refused by DME (NRC 31)", result)

    def test_maps_three_and_four_use_their_own_slot_bytes(self):
        for slot, index in ((1, "00"), (3, "02"), (4, "03")):
            with self.subTest(slot=slot):
                result = self.run_worker("mhd-maps", ["--map", str(slot)],
                                         [("2f01"+index, ["6f01"+index])])
                self.assertEqual(result, f"Confirmed: Map {slot}\n")

    def test_shift_worker_decodes_live_rpm_and_speed(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as listener:
            listener.bind(("127.0.0.1", 0))
            listener.settimeout(2)
            self.run_worker("shift-deck", ["--cycles", "1", "--udp-port", str(listener.getsockname()[1])],
                            [("010c", ["410c61a8"]), ("010d", ["410d64"])])
            sampled_ms, rpm, kph = struct.unpack("=Qii", listener.recv(64))
            self.assertGreater(sampled_ms, 0)
            self.assertEqual((rpm,kph), (6250,100))


    def test_shift_worker_waits_for_a_busy_gateway(self):
        with socket.socket() as gateway, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as lock:
            gateway.bind(("127.0.0.1", 0))
            gateway.listen()
            gateway.settimeout(0.2)
            lock.bind(("127.0.0.1", 42861))
            with subprocess.Popen(
                [BUILD/"shift-deck", "--host", "127.0.0.1",
                 "--port", str(gateway.getsockname()[1]), "--cycles", "6"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            ) as worker:
                try:
                    with self.assertRaises(TimeoutError):
                        gateway.accept()
                    self.assertIsNone(worker.poll())
                    lock.close()
                    gateway.settimeout(2)
                    conn, _ = gateway.accept()
                    with conn:
                        conn.settimeout(2)
                        length, ctrl = struct.unpack(">IH", exact(conn, 6))
                        self.assertEqual(ctrl, 1)
                        self.assertEqual(exact(conn, length), bytes.fromhex("f412010c"))
                finally:
                    if worker.poll() is None:
                        worker.terminate()


if __name__ == "__main__":
    unittest.main()
