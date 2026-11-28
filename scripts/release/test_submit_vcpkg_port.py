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
            if args[:4] == ("gh", "api", "--method", "POST"):
                self.assertEqual(args[4], "repos/microsoft/vcpkg/pulls")
                self.assertIn("base=master", args)
                self.assertIn("head=calvinhxx:release/fluent-qt-v1.12.3", args)
                body_file = args[args.index("-F") + 1].removeprefix("body=@")
                reader = "import pathlib, sys; sys.stdout.buffer.write(pathlib.Path(sys.argv[1]).read_bytes())"
                bodies.append(subprocess.check_output([sys.executable, "-c", reader, body_file]).decode("utf-8"))
                return '{"state":"open","html_url":"https://example.org/pr/123"}'
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

    def test_update_after_a_successful_push_only_needs_the_pull_request_rest_endpoint(self):
        pr = {"number": 123, "branch": "existing", "url": "https://example.org/pr/123",
              "title": "Add version 1.12.3", "body": "### New port checklist\n\n- [x] 验证 fluent-qt"}
        calls = []

        def registry_command(*args):
            if args[:2] == ("git", "ls-remote"):
                return "existing head"
            if args[:2] == ("git", "show"):
                return "<!-- NEW PORT CHECKLIST\n- [ ] 验证 REPLACE_WITH_PORT_NAME\nEND OF NEW PORT CHECKLIST -->"
            if args[0] == "gh":
                self.assertEqual(args[:5], ("gh", "api", "--method", "PATCH", "repos/microsoft/vcpkg/pulls/123"))
                self.assertIn("title=[fluent-qt] Add version 1.12.4", args)
                message = args[args.index("-F") + 1]
                self.assertTrue(message.startswith("body=@"))
                calls.append(Path(message[6:]).read_text(encoding="utf-8"))
                return '{"state":"open","html_url":"https://example.org/pr/123"}'
            return ""

        with tempfile.TemporaryDirectory() as directory:
            previous = Path.cwd()
            try:
                os.chdir(directory)
                Path(".vcpkg-root").touch()
                submitter.BASELINE.parent.mkdir()
                submitter.BASELINE.write_text("{}")
                with patch.object(submitter, "open_prs", return_value=[pr]), \
                     patch.object(submitter, "version", return_value=(1, 12, 4)), \
                     patch.object(submitter, "same_archive", return_value=True), \
                     patch.object(submitter, "run", side_effect=registry_command):
                    submitter.submit("v1.12.4", Path("overlay"), dry_run=False)
            finally:
                os.chdir(previous)
        self.assertEqual(len(calls), 1)
        self.assertIn(pr["body"], calls[0])


if __name__ == "__main__":
    unittest.main()
