"""Verify offline recovery, explicit networking and input integrity."""

import hashlib
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from fetch_font_sources import FontSources, load_assets


class FontSourcesTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        subprocess.run(["git", "init", "--quiet", str(self.root)], check=True)
        self.data = b"verified font input"
        self.relative = "third_party/fonts/example/font.ttf"
        self.blob = subprocess.check_output(["git", "-C", str(self.root), "hash-object", "--stdin"],
                                            input=self.data).decode().strip()
        self.asset = {"path": self.relative, "role": "text", "sha256": hashlib.sha256(self.data).hexdigest(),
                      "git_blob": self.blob, "bytes": len(self.data), "url": "https://example.invalid/font.ttf"}
        self.sources = FontSources(self.root, self.root / "cache", {self.relative: self.asset})

    def test_cached_input_needs_neither_git_nor_network(self):
        path = self.sources.path(self.relative)
        path.parent.mkdir(parents=True)
        path.write_bytes(self.data)
        with patch.object(self.sources, "_git") as git, patch("fetch_font_sources.urlopen") as network:
            self.assertEqual(self.sources.prepare(), [path])
        git.assert_not_called()
        network.assert_not_called()

    def test_local_object_restores_a_missing_or_corrupt_cache_offline(self):
        subprocess.run(["git", "-C", str(self.root), "hash-object", "-w", "--stdin"],
                       input=self.data, capture_output=True, check=True)
        with patch("fetch_font_sources.urlopen") as network:
            path = self.sources.prepare()[0]
            self.assertEqual(path.read_bytes(), self.data)
            path.write_bytes(b"corrupt")
            self.sources.prepare()
            self.assertEqual(path.read_bytes(), self.data)
        network.assert_not_called()

    def test_missing_local_object_is_not_requested_from_a_promisor_remote(self):
        with patch.object(self.sources, "_git", return_value=b"") as git, \
                patch("fetch_font_sources.urlopen") as network:
            with self.assertRaisesRegex(FileNotFoundError, "--download"):
                self.sources.prepare()
        git.assert_called_once_with("cat-file", "--batch-all-objects", "--unordered", "--batch-check=%(objectname)")
        network.assert_not_called()

    def test_real_partial_clone_does_not_lazily_fetch_a_historical_font(self):
        font = self.root / "font.ttf"
        font.write_bytes(self.data)
        subprocess.run(["git", "-C", str(self.root), "add", "font.ttf"], check=True)
        commit = ["git", "-C", str(self.root), "-c", "core.hooksPath=/dev/null",
                  "-c", "commit.gpgsign=false", "-c", "user.name=Fixture",
                  "-c", "user.email=fixture@example.invalid", "commit", "--quiet", "-m"]
        subprocess.run([*commit, "Original font"], check=True)
        font.unlink()
        (self.root / "README").write_text("Current tree")
        subprocess.run(["git", "-C", str(self.root), "add", "-A"], check=True)
        subprocess.run([*commit, "Move input out of the tree"], check=True)
        subprocess.run(["git", "-C", str(self.root), "config", "uploadpack.allowFilter", "true"], check=True)
        clone = self.root / "partial"
        subprocess.run(["git", "clone", "--quiet", "--no-checkout", "--filter=blob:none",
                        self.root.as_uri(), str(clone)], check=True)
        sources = FontSources(clone, self.root / "partial-cache", {self.relative: self.asset})
        before = sources._git("cat-file", "--batch-all-objects", "--batch-check=%(objectname)")
        self.assertNotIn(self.blob.encode(), before.splitlines())
        with patch("fetch_font_sources.urlopen") as network:
            with self.assertRaises(FileNotFoundError):
                sources.prepare()
        after = sources._git("cat-file", "--batch-all-objects", "--batch-check=%(objectname)")
        self.assertEqual(set(before.splitlines()), set(after.splitlines()))
        network.assert_not_called()

    def test_download_must_be_explicit_and_exact(self):
        with patch("fetch_font_sources.urlopen", return_value=io.BytesIO(self.data)) as network:
            path = self.sources.prepare(download=True)[0]
            self.assertEqual(path.read_bytes(), self.data)
        network.assert_called_once_with(self.asset["url"], timeout=60)

    def test_bad_download_does_not_replace_or_publish_a_cache_entry(self):
        path = self.sources.path(self.relative)
        path.parent.mkdir(parents=True)
        path.write_bytes(b"old corrupt input")
        for data in (b"wrong", self.data + b"extra", b"x" * len(self.data)):
            with self.subTest(data=data), patch("fetch_font_sources.urlopen", return_value=io.BytesIO(data)):
                with self.assertRaisesRegex(ValueError, "SHA-256/size"):
                    self.sources.prepare(download=True)
            self.assertEqual(path.read_bytes(), b"old corrupt input")
            self.assertEqual(list(path.parent.iterdir()), [path])

    def test_manifest_rejects_escape_and_duplicate_paths(self):
        manifest = self.root / "sources.json"
        for assets in ([dict(self.asset, path="../escape")], [self.asset, self.asset]):
            manifest.write_text(json.dumps({"schema_version": 1, "assets": assets}))
            with self.assertRaisesRegex(ValueError, "path"):
                load_assets(manifest)

    def test_manifest_requires_a_known_role(self):
        manifest = self.root / "sources.json"
        for asset in (dict(self.asset, role="desktop"), {k: v for k, v in self.asset.items() if k != "role"}):
            manifest.write_text(json.dumps({"schema_version": 1, "assets": [asset]}))
            with self.assertRaisesRegex(ValueError, "metadata"):
                load_assets(manifest)
        manifest.write_text(json.dumps({"schema_version": 1, "assets": [self.asset]}))
        sources = FontSources(self.root, self.root / "cache", load_assets(manifest))
        self.assertEqual(sources.paths("text"), [self.relative])
        self.assertEqual(sources.paths("web-fallback"), [])


if __name__ == "__main__":
    unittest.main()
