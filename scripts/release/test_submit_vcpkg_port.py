import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import submit_vcpkg_port as submitter


class VcpkgSubmissionTest(unittest.TestCase):
    def test_reads_utf8_command_output(self):
        text = "更新 ✓"
        command = f"import sys; sys.stdout.buffer.write({text.encode('utf-8')!r})"
        self.assertEqual(submitter.run(sys.executable, "-c", command), text)

    def test_external_reader_can_open_the_complete_pr_body(self):
        bodies = []

        def registry_command(*args):
            if args[:2] == ("git", "show"):
                return "<!-- NEW PORT CHECKLIST\n- [ ] 验证 REPLACE_WITH_PORT_NAME\nEND OF NEW PORT CHECKLIST -->"
            if args[:3] == ("gh", "pr", "create"):
                body_file = args[args.index("--body-file") + 1]
                reader = "import pathlib, sys; sys.stdout.buffer.write(pathlib.Path(sys.argv[1]).read_bytes())"
                bodies.append(subprocess.check_output([sys.executable, "-c", reader, body_file]).decode("utf-8"))
            return ""

        with tempfile.TemporaryDirectory() as directory:
            previous = Path.cwd()
            try:
                os.chdir(directory)
                Path(".vcpkg-root").touch()
                submitter.BASELINE.parent.mkdir()
                submitter.BASELINE.write_text("{}", encoding="utf-8")
                overlay = Path("overlay")
                overlay.mkdir()
                (overlay / "vcpkg.json").write_text('{"version-semver": "1.12.3"}', encoding="utf-8")
                with patch.object(submitter, "run", side_effect=registry_command):
                    submitter.submit("v1.12.3", overlay, dry_run=False)
            finally:
                os.chdir(previous)
        self.assertEqual(len(bodies), 1)
        self.assertIn("### New port checklist\n\n- [ ] 验证 fluent-qt", bodies[0])


if __name__ == "__main__":
    unittest.main()
