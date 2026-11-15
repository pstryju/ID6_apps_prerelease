"""Exercise the generated deployment shell transactions, with injected failures."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class DeployTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tool_dir = tempfile.TemporaryDirectory()
        cls.helper = Path(cls.tool_dir.name) / "fileops"
        subprocess.run(["cc", "-Wall", "-Wextra", "-Werror",
                        str(ROOT / "source/native/fileops.c"), "-o", str(cls.helper)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tool_dir.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ("Plugins/BoostDeck", "run/stage/BoostDeck", "run/stage/ShiftDeck",
                     "run/backup", "run/removed", "run/failed", "lock"):
            (self.root / name).mkdir(parents=True, exist_ok=True)
        (self.root / "Plugins/BoostDeck/version").write_text("original")
        for app in ("BoostDeck", "ShiftDeck"):
            (self.root / f"run/stage/{app}/version").write_text("candidate")
        wrapper = self.root / "run/fileops"
        wrapper.write_text(
            '#!/bin/sh\n'
            'if test "$1" = move && test -n "$FAIL_SUFFIX"; then\n'
            '  case "$2" in *"$FAIL_SUFFIX")\n'
            '    if test ! -f "$FAIL_MARKER"; then : > "$FAIL_MARKER"; exit 23; fi;;\n'
            '  esac\n'
            'fi\n'
            f'exec {shlex.quote(str(self.helper))} "$@"\n')
        wrapper.chmod(0o700)

    def execute(self, mode, fail_suffix=""):
        text = (ROOT / f".local/deploy-tests/{mode}.sh").read_text().replace("/EVO_TEST", str(self.root))
        # QNX's restricted shell has no printf or rmdir executable. Block even
        # the Linux shell's printf builtin and remove external utilities from PATH.
        self.assertNotIn("printf", text)
        text = "printf() { echo 'printf unavailable' >&2; return 126; }\n" + text
        return subprocess.run(["/bin/sh", "-s"], input=text, text=True, capture_output=True,
                              env={**os.environ, "FAIL_SUFFIX": fail_suffix,
                                   "PATH": "/no-external-utilities",
                                   "FAIL_MARKER": str(self.root / "injected")})

    def test_install_and_uninstall_preserve_original(self):
        self.assertEqual(self.execute("install").returncode, 0)
        self.assertEqual((self.root / "Plugins/BoostDeck/version").read_text(), "candidate")
        self.assertEqual((self.root / "run/backup/BoostDeck/version").read_text(), "original")
        (self.root / "lock").mkdir()
        result = self.execute("uninstall")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.root / "Plugins/BoostDeck/version").read_text(), "original")
        self.assertFalse((self.root / "Plugins/ShiftDeck").exists())
        self.assertEqual((self.root / "run/removed/ShiftDeck/version").read_text(), "candidate")
        self.assertEqual((self.root / "run/STATUS").read_text().strip(), "uninstalled")

    def test_mid_install_failure_restores_all_originals(self):
        result = self.execute("install", "/stage/ShiftDeck")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((self.root / "Plugins/BoostDeck/version").read_text(), "original")
        self.assertFalse((self.root / "Plugins/ShiftDeck").exists())
        self.assertFalse((self.root / "lock").exists())
        self.assertEqual((self.root / "run/STATUS").read_text().strip(), "rolledback")

    def test_mid_uninstall_failure_restores_candidates(self):
        self.assertEqual(self.execute("install").returncode, 0)
        (self.root / "lock").mkdir()
        result = self.execute("uninstall", "/Plugins/ShiftDeck")
        self.assertNotEqual(result.returncode, 0)
        for app in ("BoostDeck", "ShiftDeck"):
            self.assertEqual((self.root / f"Plugins/{app}/version").read_text(), "candidate")
        self.assertEqual((self.root / "run/backup/BoostDeck/version").read_text(), "original")
        self.assertEqual((self.root / "run/STATUS").read_text().strip(), "installed")
        self.assertFalse((self.root / "lock").exists())

    def test_helper_never_overwrites_existing_destination(self):
        result = subprocess.run([str(self.helper), "move", str(self.root / "run/stage/BoostDeck"),
                                 str(self.root / "Plugins/BoostDeck")], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((self.root / "Plugins/BoostDeck/version").read_text(), "original")


if __name__ == "__main__":
    unittest.main()
