#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import io
import os
import stat
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import Mock, patch


MODULE_PATH = Path(__file__).with_name("check_cpp_format.py")
PROJECT_ROOT = MODULE_PATH.parents[2]
SPEC = importlib.util.spec_from_file_location("check_cpp_format", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
check_cpp_format = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(check_cpp_format)


class CheckCppFormatTest(unittest.TestCase):
    def test_parse_version_ignores_npm_wrapper_banner(self) -> None:
        output = (
            "clang-format NPM version 1.8.0\n"
            "clang-format version 15.0.0 (https://github.com/llvm/llvm-project.git abc)\n"
        )
        self.assertEqual(check_cpp_format.parse_clang_format_version(output), "15.0.0")

    def test_formatter_override_uses_repository_environment_variable(self) -> None:
        with patch.dict(os.environ, {"FLUENTQT_CLANG_FORMAT": "/opt/tools/clang-format"}):
            self.assertEqual(
                check_cpp_format.default_formatter_command(),
                "/opt/tools/clang-format",
            )

    def test_empty_formatter_override_falls_back_to_path_default(self) -> None:
        with patch.dict(os.environ, {"FLUENTQT_CLANG_FORMAT": ""}):
            self.assertEqual(check_cpp_format.default_formatter_command(), "clang-format")

    def test_normalize_cpp_files_filters_and_deduplicates(self) -> None:
        selected = check_cpp_format.normalize_cpp_files(
            [
                "src/components/basicinput/MultiSelectComboBox.h",
                "README.md",
                "src/components/basicinput/MultiSelectComboBox.h",
            ],
            require_exists=True,
        )
        self.assertEqual(
            selected, [Path("src/components/basicinput/MultiSelectComboBox.h")]
        )

    def test_outside_repository_path_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "outside the repository"):
            check_cpp_format.normalize_cpp_files(["/private/tmp/outside.cpp"], require_exists=False)

    @patch.object(check_cpp_format.subprocess, "run")
    def test_changed_files_trust_committed_cpp_paths_over_working_tree(self, run) -> None:
        run.return_value.returncode = 0
        run.return_value.stdout = (
            "README.md\n"
            "src/components/basicinput/MultiSelectComboBox.h\n"
            "src/components/basicinput/Removed.cpp\n"
        )
        run.return_value.stderr = ""
        self.assertEqual(
            check_cpp_format.changed_files_from("origin/main"),
            [
                Path("src/components/basicinput/MultiSelectComboBox.h"),
                Path("src/components/basicinput/Removed.cpp"),
            ],
        )
        self.assertEqual(
            run.call_args.args[0],
            [
                "git",
                "diff",
                "--name-only",
                "--diff-filter=ACMR",
                "origin/main...HEAD",
                "--",
            ],
        )

    @patch.object(check_cpp_format.subprocess, "run")
    def test_changed_files_accept_explicit_pushed_revision(self, run) -> None:
        run.return_value.returncode = 0
        run.return_value.stdout = "src/example.cpp\n"
        run.return_value.stderr = ""

        self.assertEqual(
            check_cpp_format.changed_files_from("origin/main", "abc123"),
            [Path("src/example.cpp")],
        )
        self.assertEqual(
            run.call_args.args[0],
            [
                "git",
                "diff",
                "--name-only",
                "--diff-filter=ACMR",
                "origin/main...abc123",
                "--",
            ],
        )

    @patch.object(check_cpp_format.subprocess, "run")
    def test_working_tree_includes_untracked_cpp_files(self, run) -> None:
        tracked = Mock(stdout="README.md\nsrc/components/basicinput/ComboBox.cpp\n")
        untracked = Mock(
            stdout="src/components/date_time/private/PickerWheel_p.h\n"
        )
        run.side_effect = [tracked, untracked]

        self.assertEqual(
            check_cpp_format.working_tree_files(),
            [
                Path("src/components/basicinput/ComboBox.cpp"),
                Path("src/components/date_time/private/PickerWheel_p.h"),
            ],
        )
        self.assertEqual(
            run.call_args_list[1].args[0],
            ["git", "ls-files", "--others", "--exclude-standard", "--"],
        )

    @patch.object(check_cpp_format.shutil, "which", return_value="/usr/bin/clang-format")
    @patch.object(check_cpp_format.subprocess, "run")
    def test_formatter_version_must_match(self, run, _which) -> None:
        run.return_value.returncode = 0
        run.return_value.stdout = "clang-format version 18.1.8\n"
        run.return_value.stderr = ""
        with self.assertRaisesRegex(RuntimeError, "15.0.0 is required"):
            check_cpp_format.resolve_formatter("clang-format")

    @patch.object(check_cpp_format.subprocess, "run")
    def test_git_file_check_uses_exact_head_blob(self, run) -> None:
        original = b"int value;\n"
        run.side_effect = [
            Mock(returncode=0, stdout=original, stderr=b""),
            Mock(returncode=0, stdout=original, stderr=b""),
        ]

        with redirect_stdout(io.StringIO()):
            self.assertEqual(
                check_cpp_format.check_git_files(
                    "/usr/bin/clang-format",
                    [Path("src/example.cpp")],
                    revision="HEAD",
                ),
                0,
            )
        self.assertEqual(
            run.call_args_list[0].args[0],
            ["git", "show", "HEAD:src/example.cpp"],
        )
        self.assertEqual(
            run.call_args_list[1].args[0],
            [
                "/usr/bin/clang-format",
                "--style=file",
                "--assume-filename=src/example.cpp",
            ],
        )
        self.assertEqual(run.call_args_list[1].kwargs["input"], original)

    @patch.object(check_cpp_format.subprocess, "run")
    def test_git_file_check_reports_format_mismatch(self, run) -> None:
        run.side_effect = [
            Mock(returncode=0, stdout=b"int  value;\n", stderr=b""),
            Mock(returncode=0, stdout=b"int value;\n", stderr=b""),
        ]

        with redirect_stderr(io.StringIO()):
            self.assertEqual(
                check_cpp_format.check_git_files(
                    "/usr/bin/clang-format",
                    [Path("src/example.cpp")],
                    revision="INDEX",
                ),
                1,
            )
        self.assertEqual(
            run.call_args_list[0].args[0],
            ["git", "show", ":src/example.cpp"],
        )

    @patch.object(check_cpp_format, "check_git_files", return_value=0)
    @patch.object(check_cpp_format, "resolve_formatter", return_value="/usr/bin/clang-format")
    @patch.object(
        check_cpp_format,
        "staged_files",
        return_value=[Path("src/example.cpp")],
    )
    def test_staged_mode_checks_index_snapshot(self, _files, _resolve, check) -> None:
        with redirect_stdout(io.StringIO()):
            self.assertEqual(check_cpp_format.main(["--staged"]), 0)
        check.assert_called_once_with(
            "/usr/bin/clang-format",
            [Path("src/example.cpp")],
            revision="INDEX",
        )

    def test_fix_rejects_an_arbitrary_pushed_revision(self) -> None:
        with redirect_stderr(io.StringIO()):
            with self.assertRaisesRegex(SystemExit, "2"):
                check_cpp_format.main(
                    [
                        "--changed-from",
                        "origin/main",
                        "--head",
                        "abc123",
                        "--fix",
                    ]
                )

    def test_repository_hooks_are_executable_and_share_the_checker(self) -> None:
        expected = {
            "pre-commit": "--staged",
            "pre-push": "--changed-from",
        }
        for name, selection in expected.items():
            with self.subTest(hook=name):
                path = PROJECT_ROOT / ".githooks" / name
                script = path.read_text(encoding="utf-8")
                self.assertTrue(path.stat().st_mode & stat.S_IXUSR)
                self.assertIn("tools/quality/check_cpp_format.py", script)
                self.assertIn(selection, script)
                self.assertIn("git diff", script)

    @patch.object(check_cpp_format, "resolve_formatter")
    @patch.object(check_cpp_format, "working_tree_files", return_value=[])
    def test_empty_selection_does_not_require_formatter(self, _files, resolve) -> None:
        self.assertEqual(check_cpp_format.main(["--working-tree"]), 0)
        resolve.assert_not_called()


if __name__ == "__main__":
    unittest.main()
