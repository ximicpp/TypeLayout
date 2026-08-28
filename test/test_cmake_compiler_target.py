import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "cmake" / "ResolveCompilerTarget.cmake"
CMAKE = shutil.which("cmake")


class ResolveCompilerTargetTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(CMAKE, "cmake is required for the target helper tests")
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def fake_compiler(self, targeted_output, bare_output):
        script = self.root / "fake_compiler.py"
        script.write_text(
            textwrap.dedent(
                f"""\
                import sys

                if sys.argv[1:] == ["--target=arm64-apple-macosx15.0.0", "-dumpmachine"]:
                    print({targeted_output!r})
                elif sys.argv[1:] == ["-dumpmachine"]:
                    print({bare_output!r})
                else:
                    raise SystemExit(f"unexpected compiler arguments: {{sys.argv[1:]!r}}")
                """
            ),
            encoding="utf-8",
        )
        return script

    def run_helper(self, compiler, requested):
        driver = self.root / "driver.cmake"
        driver.write_text(
            textwrap.dedent(
                f"""\
                include([[{MODULE.as_posix()}]])
                set(fake_compiler [[{Path(sys.executable).as_posix()}]] [[{compiler.as_posix()}]])
                typelayout_probe_compiler_target(
                    actual "${{fake_compiler}}" [[{requested}]] [[arm64-Darwin]])
                file(WRITE [[{(self.root / 'actual.txt').as_posix()}]] "${{actual}}")
                """
            ),
            encoding="utf-8",
        )
        return subprocess.run(
            [CMAKE, "-P", str(driver)],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )

    def test_locked_target_uses_targeted_dumpmachine_not_bare_darwin(self):
        compiler = self.fake_compiler(
            "arm64-apple-macosx15.0.0", "arm64-apple-darwin24.0.0"
        )
        result = self.run_helper(compiler, "arm64-apple-macosx15.0.0")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            (self.root / "actual.txt").read_text(encoding="utf-8"),
            "arm64-apple-macosx15.0.0",
        )

    def test_locked_target_rejects_mismatched_targeted_dumpmachine(self):
        compiler = self.fake_compiler(
            "arm64-apple-darwin24.0.0", "arm64-apple-darwin24.0.0"
        )
        result = self.run_helper(compiler, "arm64-apple-macosx15.0.0")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "does not match locked compiler target",
            " ".join(result.stderr.split()),
        )


if __name__ == "__main__":
    unittest.main()
