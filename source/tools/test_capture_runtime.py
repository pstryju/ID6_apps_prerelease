"""Offline checks for binary-safe runtime collection; no SSH connection."""
import struct
import unittest
from capture_headunit import RUNTIME_LIBRARIES, capture_runtime


def elf():
    data = bytearray(32)
    data[:6] = b"\x7fELF\x01\x01"
    struct.pack_into("<HH", data, 16, 3, 40)
    return bytes(data)


class RuntimeCaptureTest(unittest.TestCase):
    def test_fallback_preserves_binary_bytes(self):
        calls = []
        def read(command, optional=False):
            calls.append(command)
            self.assertTrue(optional)
            return elf() if command.startswith("cat /usr/lib/") else b""
        report, files = capture_runtime(read)
        self.assertEqual(set(files), set(RUNTIME_LIBRARIES))
        self.assertTrue(all(data == elf() for data in files.values()))
        self.assertTrue(all(item["source_path"].startswith("/usr/lib/") for item in report.values()))
        self.assertEqual(len(calls), 8)

    def test_missing_libraries_are_explicit(self):
        report, files = capture_runtime(lambda command, optional=False: b"")
        self.assertFalse(files)
        self.assertTrue(all(item == {"missing": True} for item in report.values()))

    def test_wrong_architecture_is_rejected(self):
        with self.assertRaises(ValueError):
            capture_runtime(lambda command, optional=False: b"not an ARM library")


if __name__ == "__main__":
    unittest.main()
