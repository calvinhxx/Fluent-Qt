#!/usr/bin/env python3
"""Regressions for automatic website generation, cache reuse and publication."""

from contextlib import redirect_stdout
import io
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import build_site
import generate_component_previews as previews


class SiteArtifactTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.site = self.root / "site"
        self.files = (".nojekyll", "index.html", "zh-CN/index.html", "api/index.html", "api/catalog.json",
                      "api/button/index.html", "zh-CN/api/button/index.html", "sitemap.xml",
                      "assets/component-previews/manifest.json", "assets/component-previews/button/light.png")
        for name in self.files:
            path = self.site / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("current " + name)
        self.receipt = {"schema_version": 1, "commit": "current-sha", "files": build_site.checksums(self.site)}
        (self.site / build_site.RECEIPT).write_text(json.dumps(self.receipt))

    def test_artifact_must_match_commit_and_all_contents(self):
        build_site.verify(self.site, "current-sha")
        with self.assertRaisesRegex(ValueError, "requested commit"):
            build_site.verify(self.site, "older-sha")
        image = self.site / self.files[-1]
        image.write_bytes(b"old or corrupted screenshot")
        with self.assertRaisesRegex(ValueError, "contents differ"):
            build_site.verify(self.site, "current-sha")

    def test_missing_and_injected_files_are_rejected(self):
        (self.site / "api/button/index.html").unlink()
        with self.assertRaisesRegex(ValueError, "contents differ"):
            build_site.verify(self.site, "current-sha")
        (self.site / "extra.html").write_text("unexpected page")
        with self.assertRaisesRegex(ValueError, "contents differ"):
            build_site.verify(self.site, "current-sha")

    def test_symlink_cannot_replace_a_verified_file(self):
        path = self.site / "index.html"
        path.unlink()
        target = self.root / "outside.html"
        target.write_text("current index.html")
        path.symlink_to(target)
        with self.assertRaisesRegex(ValueError, "symlinks"):
            build_site.verify(self.site, "current-sha")

    def test_staging_removes_retired_documents_and_previews(self):
        for name in ("api/retired/index.html", "zh-CN/api/retired/index.html",
                     "assets/component-previews/retired/light.png", "gallery/local-only.wasm"):
            path = self.site / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("obsolete")
        manifest = {"input_digest": "digest", "images": [{"path": self.files[-1]}]}
        pages = {name: "html" for name in self.files if name.endswith(".html")}
        output = self.root / "build/site-pages"
        with patch.object(build_site, "ROOT", self.root), \
             patch.object(build_site, "validated_manifest", return_value=manifest), \
             patch("generate_api_reference.generate_reference", return_value={}), \
             patch("component_docs.collect", return_value=[]), \
             patch("component_docs.generated_outputs", return_value=pages):
            build_site.stage(output, "current-sha")
            build_site.stage(output, "current-sha")  # Repeat builds replace only their own artifact.
            with self.assertRaisesRegex(ValueError, "separate"):
                build_site.stage(self.site, "current-sha")
        build_site.verify(output, "current-sha")
        self.assertFalse((output / "api/retired").exists())
        self.assertFalse((output / "assets/component-previews/retired").exists())
        self.assertFalse((output / "gallery").exists())
        self.assertTrue((output / ".nojekyll").exists())
        self.assertTrue((self.site / "api/retired/index.html").exists())


class SiteGenerationTest(unittest.TestCase):
    def invoke(self, *arguments):
        return patch("sys.argv", ["build_site.py", *arguments])

    def test_explicit_gallery_rebuild_invalidates_local_capture_reuse(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "gallery"
            executable.write_bytes(b"first Qt kit")
            manifest = {"binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest()}
            with patch.object(build_site, "validated_manifest", return_value=manifest):
                self.assertTrue(build_site.current_previews(executable))
                executable.write_bytes(b"rebuilt with another Qt kit")
                self.assertFalse(build_site.current_previews(executable))
                executable.unlink()
                self.assertFalse(build_site.current_previews(executable))

    def test_source_and_recipe_edits_invalidate_previews(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "Button.cpp"
            source.write_text("old rendering")
            recipe = [{"id": "button", "previews": [{"sample": "basic", "id": "basic"}]}]
            with patch.object(previews, "ROOT", root), \
                 patch.object(previews, "capture_inputs", return_value=["Button.cpp"]), \
                 patch.object(previews, "config", return_value=recipe):
                before = previews.capture_digest()
                source.write_text("new rendering")
                self.assertNotEqual(before, previews.capture_digest())
                before = previews.capture_digest()
                recipe[0]["previews"][0]["sample"] = "another"
                self.assertNotEqual(before, previews.capture_digest())

    def test_real_input_discovery_covers_bootstrap_headers_and_capture_configuration(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sources = (
                "CMakeLists.txt", "CMakePresets.json", "resources.qrc",
                "tools/site/generate_component_previews.py", ".github/workflows/ci-site.yml",
                ".github/scripts/install-linux-dependencies.sh",
                "src/components/basicinput/Button.cpp", "app/main.cpp",
                "app/platform/desktop/GalleryApplication.cpp", "app/view/widgets/GallerySampleCard.h",
                "app/view/support/GalleryStyleSupport.h", "include/FluentQt/BasicInput.h",
            )
            for name in sources:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("original")
            recipe = [{"id": "button", "previews": [{"id": "basic", "sample": "basic"}]}]
            with patch.object(previews, "ROOT", root), patch.object(previews, "config", return_value=recipe):
                original = previews.capture_digest()
                for name in sources:
                    with self.subTest(path=name):
                        (root / name).write_text("changed")
                        self.assertNotEqual(original, previews.capture_digest())
                        (root / name).write_text("original")
                (root / "README.md").write_text("prose only")
                self.assertEqual(original, previews.capture_digest())
                with patch.dict(os.environ, {"QT_VERSION": "6.9.3", "QT_SCALE_FACTOR": "2",
                                             "QT_QPA_PLATFORM": "xcb", "LIBGL_ALWAYS_SOFTWARE": "1"}):
                    original = previews.capture_digest()
                    for name, value in (("QT_VERSION", "6.10.0"), ("QT_SCALE_FACTOR", "1"),
                                        ("QT_QPA_PLATFORM", "cocoa"), ("LIBGL_ALWAYS_SOFTWARE", "0")):
                        with self.subTest(setting=name), patch.dict(os.environ, {name: value}):
                            self.assertNotEqual(original, previews.capture_digest())
                    with patch.dict(os.environ, {"DISPLAY": ":99"}):
                        self.assertEqual(original, previews.capture_digest())

    def test_generated_outputs_are_untracked_but_site_sources_remain(self):
        if not (build_site.ROOT / ".git").exists():
            self.skipTest("Git index migration is checked in the repository checkout")
        ignored_tracked = subprocess.check_output(
            ["git", "ls-files", "-ci", "--exclude-standard", "site"], cwd=build_site.ROOT, text=True)
        self.assertEqual(ignored_tracked.strip(), "", "Generated website files are still tracked")
        tracked = set(subprocess.check_output(
            ["git", "ls-files"], cwd=build_site.ROOT, text=True).splitlines())
        self.assertTrue({"site/404.html", "site/app/index.html", "site/api/catalog.json",
                         "site/api/component.js", "site/api/component.css",
                         "tools/site/index.template.html"}.issubset(tracked))

    def build(self, *arguments, current=True, failing=None):
        def run(*command):
            if failing and failing in command:
                raise subprocess.CalledProcessError(1, command)
        with self.invoke(*arguments), patch.object(build_site, "current_previews", return_value=current), \
             patch.object(build_site, "run", side_effect=run) as runner, \
             patch.object(build_site, "stage") as self.stage, \
             patch.object(build_site.subprocess, "check_output", return_value="sha\n"), \
             redirect_stdout(io.StringIO()):
            try:
                build_site.main()
            finally:
                self.commands = [call.args for call in runner.call_args_list]

    def test_capture_retries_once_after_a_stall(self):
        recovered = [subprocess.TimeoutExpired("gallery", 60), subprocess.CompletedProcess([], 0, "ok", "")]
        crashed = subprocess.CompletedProcess([], 1, "", "crash")
        with tempfile.TemporaryDirectory() as directory, redirect_stdout(io.StringIO()):
            log = Path(directory) / "button-styles-light.log"
            with patch.object(previews.subprocess, "run", side_effect=recovered) as run:
                self.assertTrue(previews.run_capture(["gallery"], log))
            self.assertEqual(run.call_count, 2)
            with patch.object(previews.subprocess, "run", return_value=crashed) as run:
                self.assertFalse(previews.run_capture(["gallery"], log))
            self.assertEqual(run.call_count, 2)
            self.assertEqual(log.read_text(), "crash")

    def test_fresh_previews_skip_native_capture_but_regenerate_documents(self):
        self.build()
        self.assertIn(("tools/site/generate_localized_site.py",), self.commands)
        self.assertIn(("tools/site/generate_api_reference.py", "--require-current-previews"), self.commands)
        self.assertFalse(any("tools/site/generate_component_previews.py" in command for command in self.commands))
        self.stage.assert_called_once()

    def test_catalogs_are_generated_before_checks_and_invalid_sources_still_fail(self):
        self.build()
        self.assertEqual(self.commands[:4], [
            ("tools/ai/generate_ai_catalog.py", "--project-root", str(build_site.ROOT)),
            ("tools/site/generate_api_reference.py", "--catalog-only"),
            ("tools/ai/generate_ai_catalog.py", "--project-root", str(build_site.ROOT), "--check"),
            ("tools/site/generate_api_reference.py", "--check")])
        self.stage.assert_called_once()
        with self.assertRaises(subprocess.CalledProcessError):
            self.build(failing="tools/ai/generate_ai_catalog.py")
        self.stage.assert_not_called()

    def test_failed_recapture_never_stages_a_stale_site(self):
        with self.assertRaises(subprocess.CalledProcessError):
            self.build("--gallery", "/tmp/current-gallery", current=False,
                       failing="tools/site/generate_component_previews.py")
        self.assertEqual(self.commands[-1][0], "tools/site/generate_component_previews.py")
        self.stage.assert_not_called()

if __name__ == "__main__":
    unittest.main()
