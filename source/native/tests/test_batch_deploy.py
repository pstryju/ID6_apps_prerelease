"""End-to-end bootstrap, bundle receive, verify, install and restore on Linux."""
import hashlib
import io
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[3]
FIXTURES=ROOT/'.local/deploy-tests'


class BatchDeployTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tool=tempfile.TemporaryDirectory()
        cls.helper=Path(cls.tool.name)/'fileops'
        subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',str(ROOT/'source/native/fileops.c'),'-o',str(cls.helper)],check=True)
        cls.helper_bytes=cls.helper.read_bytes()

    @classmethod
    def tearDownClass(cls): cls.tool.cleanup()

    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        (self.root/'Plugins/BoostDeck').mkdir(parents=True)
        (self.root/'Plugins/BoostDeck/version').write_text('original')

    def execute(self,mode,mutate=None,corrupt=False):
        script=(FIXTURES/f'{mode}-bootstrap.sh').read_text().replace('/EVO_TEST',str(self.root))
        script=script.replace('count=7 ',f'count={len(self.helper_bytes)} ')
        script=script.replace(hashlib.sha256(b'EVOSTUB').hexdigest(),hashlib.sha256(self.helper_bytes).hexdigest())
        stream=io.BytesIO((FIXTURES/f'{mode}.bundle').read_bytes()[7:])
        packet=self.helper_bytes
        while True:
            line=stream.readline()
            if line==b'END\n': break
            _,length,_,name=line.decode().strip().split(' ')
            data=stream.read(int(length))
            if name=='deploy.sh':
                data=data.replace(b'/EVO_TEST',str(self.root).encode())
                if mutate: data=mutate(data)
            digest=hashlib.sha256(data).hexdigest()
            if corrupt and name=='stage/ShiftDeck/version': data=b'X'+data[1:]
            packet+=f'FILE {len(data)} {digest} {name}\n'.encode()+data
        packet+=b'END\n'
        self.assertNotIn('printf',script)
        return subprocess.run(['/bin/sh','-c',script],input=packet,capture_output=True)

    def test_complete_install_and_restore(self):
        result=self.execute('install')
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn(b'EVO_BATCH_INSTALLED',result.stdout)
        self.assertEqual((self.root/'Plugins/BoostDeck/version').read_text(),'candidate')
        result=self.execute('uninstall')
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn(b'EVO_BATCH_UNINSTALLED',result.stdout)
        self.assertEqual((self.root/'Plugins/BoostDeck/version').read_text(),'original')
        self.assertFalse((self.root/'Plugins/ShiftDeck').exists())

    def test_corrupt_package_never_modifies_plugins(self):
        result=self.execute('install',corrupt=True)
        self.assertNotEqual(result.returncode,0)
        self.assertEqual((self.root/'Plugins/BoostDeck/version').read_text(),'original')
        self.assertFalse((self.root/'Plugins/ShiftDeck').exists())
        self.assertFalse((self.root/'lock').exists())

    def test_failed_final_verification_rolls_back_all_apps(self):
        def mutate(data):
            return data.replace(b"echo 'Verifying installed files in Plugins'",
                b"echo corrupt > '"+str(self.root/'Plugins/BoostDeck/version').encode()+b"'")
        result=self.execute('install',mutate=mutate)
        self.assertNotEqual(result.returncode,0)
        self.assertEqual((self.root/'Plugins/BoostDeck/version').read_text(),'original')
        self.assertFalse((self.root/'Plugins/ShiftDeck').exists())
        self.assertFalse((self.root/'lock').exists())
        self.assertEqual((self.root/'run/STATUS').read_text().strip(),'rolledback')

    def test_modified_or_extra_hidden_file_prevents_uninstall(self):
        self.assertEqual(self.execute('install').returncode,0)
        (self.root/'Plugins/BoostDeck/.modified').write_text('keep')
        result=self.execute('uninstall')
        self.assertNotEqual(result.returncode,0)
        self.assertEqual((self.root/'Plugins/BoostDeck/version').read_text(),'candidate')
        self.assertTrue((self.root/'Plugins/ShiftDeck').exists())
        self.assertFalse((self.root/'lock').exists())


if __name__=='__main__': unittest.main()
