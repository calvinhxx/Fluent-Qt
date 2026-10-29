"""Prevent large or derived assets returning to the tracked checkout."""

from pathlib import Path
import subprocess
import tempfile
import unittest

from fluent_qt_repository_size import check, ignored, inventory


class RepositorySizeTest(unittest.TestCase):
    def setUp(self):
        self.budget = {"max_total_bytes": 500, "max_file_bytes": 100,
                       "large_files": {"res/icons.ttf": {"max_bytes": 200}}}

    def test_reviewed_runtime_budget_does_not_allow_other_large_assets(self):
        self.assertFalse(check({"res/icons.ttf": 200}, self.budget)["violations"])
        self.assertTrue(check({"unreviewed.png": 101}, self.budget)["violations"])
        self.assertTrue(check({"res/icons.ttf": 201}, self.budget)["violations"])

    def test_even_small_ignored_files_are_rejected(self):
        self.assertTrue(check({"site/api/index.html": 1}, self.budget, {"site/api/index.html"})["violations"])
        self.assertFalse(check({"site/api/catalog.json": 1}, self.budget, set())["violations"])

    def test_gitignore_rules_identify_tracked_outputs_and_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "--quiet", str(root)], check=True)
            (root / ".gitignore").write_text(
                "/site/api/**/index.html\n/site/zh-CN/api/\n/third_party/fonts/**/*.ttf\n")
            names = ["site/api/index.html", "site/api/button/index.html", "site/api/catalog.json",
                     "site/zh-CN/api/button/index.html", "third_party/fonts/inter/font.ttf",
                     "third_party/fonts/inter/LICENSE.txt", "src/widget.cpp"]
            self.assertEqual(ignored(root, names), {
                "site/api/index.html", "site/api/button/index.html",
                "site/zh-CN/api/button/index.html", "third_party/fonts/inter/font.ttf"})
            self.assertEqual(ignored(root, []), set())

    def test_total_budget_catches_many_small_files(self):
        self.assertTrue(check({f"source/{i}.cpp": 100 for i in range(6)}, self.budget)["violations"])

    def test_worktree_inventory_excludes_caches_untracked_files_and_pending_deletions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "--quiet", str(root)], check=True)
            (root / "keep.cpp").write_bytes(b"keep")
            (root / "removed.png").write_bytes(b"removed")
            subprocess.run(["git", "-C", str(root), "add", "keep.cpp", "removed.png"], check=True)
            (root / "removed.png").unlink()
            (root / "untracked.bin").write_bytes(b"x" * 1000)
            self.assertEqual(inventory(root), {"keep.cpp": 4})


if __name__ == "__main__":
    unittest.main()
