#!/usr/bin/env python3

import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("preflight.py")
SPEC = importlib.util.spec_from_file_location("release_preflight", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def git(root: Path, *args: str) -> None:
    subprocess.run(
        ["git", *args],
        cwd=root,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def make_repository() -> tuple[tempfile.TemporaryDirectory[str], Path]:
    temporary = tempfile.TemporaryDirectory(prefix="fluentqt-preflight-test-")
    root = Path(temporary.name)
    git(root, "init", "-b", "main")
    git(root, "config", "user.name", "FluentQt Test")
    git(root, "config", "user.email", "test@example.invalid")
    (root / "CMakeLists.txt").write_text(
        "project(FluentQt VERSION 1.8.0 LANGUAGES CXX)\n", encoding="utf-8"
    )
    (root / "docs" / "releases").mkdir(parents=True)
    (root / "docs" / "releases" / "v1.8.0.md").write_text(
        "# FluentQt 1.8.0\n", encoding="utf-8"
    )
    git(root, "add", ".")
    git(root, "commit", "-m", "chore: seed release test")
    git(root, "tag", "v1.7.2")
    git(root, "switch", "-c", "release/1.8.x")
    return temporary, root


class ReleasePreflightTest(unittest.TestCase):
    def test_collects_clean_current_release_context(self):
        temporary, root = make_repository()
        self.addCleanup(temporary.cleanup)

        context = MODULE.collect_release_context(root, "main")

        self.assertEqual(context.version, "1.8.0")
        self.assertEqual(context.branch, "release/1.8.x")
        self.assertEqual(context.previous_tag, "v1.7.2")
        self.assertEqual(context.target_tag, "v1.8.0")

    def test_rejects_release_branch_behind_base(self):
        temporary, root = make_repository()
        self.addCleanup(temporary.cleanup)
        git(root, "switch", "main")
        (root / "base-change.txt").write_text("new base\n", encoding="utf-8")
        git(root, "add", ".")
        git(root, "commit", "-m", "fix: advance main")
        git(root, "switch", "release/1.8.x")

        with self.assertRaisesRegex(MODULE.PreflightError, "does not contain current main"):
            MODULE.collect_release_context(root, "main")

    def test_rejects_wrong_release_branch(self):
        temporary, root = make_repository()
        self.addCleanup(temporary.cleanup)
        git(root, "switch", "-c", "release/1.9.x")

        with self.assertRaisesRegex(MODULE.PreflightError, "must be promoted from"):
            MODULE.collect_release_context(root, "main")

    def test_checklist_keeps_changelog_last(self):
        temporary, root = make_repository()
        self.addCleanup(temporary.cleanup)
        context = MODULE.collect_release_context(root, "main")
        checks = MODULE.lightweight_checks(context, root / "notes.md")
        self.assertEqual(checks[-1][0], "curated changelog")
        self.assertIn("--require-curated", checks[-1][1])


if __name__ == "__main__":
    unittest.main()
