#!/usr/bin/env python3

from contextlib import redirect_stderr, redirect_stdout
import importlib.util
import io
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


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

    def test_release_reuses_the_local_integration_gates(self):
        temporary, root = make_repository()
        self.addCleanup(temporary.cleanup)
        context = MODULE.collect_release_context(root, "main")
        shared = MODULE.integration_checks(root)
        checks = MODULE.lightweight_checks(context, root / "notes.md")
        self.assertEqual(checks[:len(shared)], shared)
        self.assertEqual(sum(label == "Gallery wheel builder" for label, _ in checks), 1)
        self.assertTrue({"Gallery artwork validator", "Gallery artwork source contracts", "Gallery artwork inventory"}.issubset(dict(shared)))

    def test_integration_checks_start_with_compatibility_boundaries_and_self_tests(self):
        checks = MODULE.integration_checks(Path("build/preflight-test"))
        self.assertEqual(checks[0], (
            "compatibility boundaries",
            [sys.executable, "tools/quality/check_compatibility_boundaries.py",
             "--project-root", ".", "--self-test"],
        ))
        self.assertEqual(sum(label == "compatibility boundaries" for label, _ in checks), 1)

    def test_release_stops_and_preserves_a_boundary_checker_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            checker = root / "tools/quality/check_compatibility_boundaries.py"
            checker.parent.mkdir(parents=True)
            checker.write_text("raise SystemExit(17)\n", encoding="utf-8")
            context = MODULE.ReleaseContext(
                "1.8.0", "release/1.8.x", "main", "a" * 40,
                "v1.7.2", "v1.8.0", root / "notes.md",
            )
            errors = io.StringIO()
            with mock.patch.object(MODULE, "ROOT", root), \
                    mock.patch.object(MODULE, "parse_args", return_value=mock.Mock(refresh=False, base_ref="main")), \
                    mock.patch.object(MODULE, "collect_release_context", return_value=context), \
                    mock.patch.object(MODULE, "run", wraps=MODULE.run) as runner, \
                    redirect_stdout(io.StringIO()), redirect_stderr(errors):
                self.assertEqual(MODULE.main(), 17)
            runner.assert_called_once_with(
                [sys.executable, "tools/quality/check_compatibility_boundaries.py",
                 "--project-root", ".", "--self-test"],
                root=root,
            )
            self.assertIn("compatibility boundaries failed", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
