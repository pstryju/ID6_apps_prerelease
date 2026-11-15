"""Configuration tests use host C code; no network or ECU needed."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[3]


class GatewayConfigTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory()
        cls.binary=Path(cls.temp.name)/'gateway'
        source=Path(cls.temp.name)/'main.c'
        source.write_text('#include "gateway_config.h"\nint main(int argc,char **argv){char out[16];if(argc!=2||!bd_load_gateway(argv[1],out))return 1;puts(out);return 0;}\n')
        subprocess.run(['cc','-Wall','-Wextra','-Werror','-I',str(ROOT/'source/native/src'),str(source),'-o',str(cls.binary)],check=True)

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def test_reference_fallback_and_user_gateway(self):
        with tempfile.TemporaryDirectory() as directory:
            worker=str(Path(directory)/'worker')
            self.assertEqual(subprocess.check_output([str(self.binary),worker],text=True).strip(),'169.254.21.3')
            for address in ('169.254.20.21','160.48.199.99'):
                (Path(directory)/'gateway-ip.txt').write_text(address+'\n')
                self.assertEqual(subprocess.check_output([str(self.binary),worker],text=True).strip(),address)

    def test_malformed_config_never_falls_back(self):
        with tempfile.TemporaryDirectory() as directory:
            for value in ('','::1','invalid','169.254.20.21 extra','999.1.2.3'):
                (Path(directory)/'gateway-ip.txt').write_text(value)
                self.assertNotEqual(subprocess.run([str(self.binary),str(Path(directory)/'worker')],capture_output=True).returncode,0)


if __name__=='__main__': unittest.main()
