#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path
from unittest.mock import Mock, patch


MODULE_PATH = Path(__file__).with_name("check_cpp_format.py")
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
    def test_changed_files_are_filtered_to_existing_cpp_files(self, run) -> None:
        run.return_value.returncode = 0
        run.return_value.stdout = (
            "README.md\n"
            "src/components/basicinput/MultiSelectComboBox.h\n"
            "src/components/basicinput/Removed.cpp\n"
        )
        run.return_value.stderr = ""
        self.assertEqual(
            check_cpp_format.changed_files_from("origin/main"),
            [Path("src/components/basicinput/MultiSelectComboBox.h")],
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

    @patch.object(check_cpp_format, "resolve_formatter")
    @patch.object(check_cpp_format, "working_tree_files", return_value=[])
    def test_empty_selection_does_not_require_formatter(self, _files, resolve) -> None:
        self.assertEqual(check_cpp_format.main(["--working-tree"]), 0)
        resolve.assert_not_called()


if __name__ == "__main__":
    unittest.main()
