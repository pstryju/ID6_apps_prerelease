"""Test the actual native receiver and SHA implementation against hashlib."""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[3]


class BundleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory()
        cls.helper=Path(cls.temp.name)/'fileops'
        subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',str(ROOT/'source/native/fileops.c'),'-o',str(cls.helper)],check=True)

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def setUp(self):
        self.folder=tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root=Path(self.folder.name)

    def packet(self,name,data,expected=None):
        expected=expected or hashlib.sha256(data).hexdigest()
        return f'FILE {len(data)} {expected} {name}\n'.encode()+data

    def receive(self,data):
        return subprocess.run([str(self.helper),'receive',str(self.root)],input=data,capture_output=True)

    def test_binary_bundle_and_sha_boundary_vectors(self):
        vectors=[b'',b'abc']+[bytes(i%256 for i in range(n)) for n in (55,56,63,64,65,16384,1000000)]
        packet=b''.join(self.packet(f'stage/App/file{i}',value) for i,value in enumerate(vectors))+b'END\n'
        result=self.receive(packet)
        self.assertEqual(result.returncode,0,result.stderr)
        for i,value in enumerate(vectors):
            path=self.root/f'stage/App/file{i}'
            self.assertEqual(path.read_bytes(),value)
            verified=subprocess.run([str(self.helper),'verify',str(path),hashlib.sha256(value).hexdigest()],capture_output=True)
            self.assertEqual(verified.returncode,0,verified.stderr)

    def test_corruption_and_truncation_stop_before_execution(self):
        result=self.receive(self.packet('deploy.sh',b'never execute',expected='0'*64)+b'END\n')
        self.assertNotEqual(result.returncode,0)
        result=self.receive(b'FILE 100 '+b'0'*64+b' truncated\nshort')
        self.assertNotEqual(result.returncode,0)

    def test_traversal_and_symlink_escape_are_rejected(self):
        for name in ('../escape','/absolute','stage/../escape','stage//escape'):
            self.assertNotEqual(self.receive(self.packet(name,b'x')+b'END\n').returncode,0)
        (self.root/'link').symlink_to(self.root.parent,target_is_directory=True)
        self.assertNotEqual(self.receive(self.packet('link/escape',b'x')+b'END\n').returncode,0)

    def test_existing_files_and_trailing_data_are_rejected(self):
        (self.root/'original').write_bytes(b'old')
        self.assertNotEqual(self.receive(self.packet('original',b'new')+b'END\n').returncode,0)
        self.assertEqual((self.root/'original').read_bytes(),b'old')
        self.assertNotEqual(self.receive(b'END\nextra').returncode,0)

    def test_firmware_text_accepts_crlf_but_not_extra_values(self):
        path=self.root/'firmware'
        path.write_bytes(b'NBTevo_Y24073J\r\n')
        command=[str(self.helper),'text',str(path),'NBTevo_Y24073J']
        self.assertEqual(subprocess.run(command,capture_output=True).returncode,0)
        path.write_bytes(b'NBTevo_Y24073J\r\nother')
        self.assertNotEqual(subprocess.run(command,capture_output=True).returncode,0)


if __name__=='__main__': unittest.main()
