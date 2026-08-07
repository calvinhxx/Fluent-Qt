#!/usr/bin/env python3
"""Regression tests for PyPI/TestPyPI release-state verification."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


SCRIPT_PATH = Path(__file__).with_name("verify-python-package-index.py")
SPEC = importlib.util.spec_from_file_location(
    "verify_python_package_index", SCRIPT_PATH
)
VERIFIER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = VERIFIER
SPEC.loader.exec_module(VERIFIER)


class PackageIndexVerifierTest(unittest.TestCase):
    def setUp(self):
        self.expected = {
            "fluentqt-1.6.0-cp311-cp311-win_amd64.whl": "a" * 64,
            "fluentqt-1.6.0-cp312-cp312-win_amd64.whl": "b" * 64,
        }

    def actual(self, filenames=None):
        names = list(self.expected) if filenames is None else filenames
        return {
            name: {
                "sha256": self.expected[name],
                "url": f"https://files.pythonhosted.org/{name}",
            }
            for name in names
        }

    def make_bundle(self, root: Path):
        dist = root / "dist"
        audits = root / "audits"
        dist.mkdir()
        audits.mkdir()
        files = []
        for index in range(17):
            filename = f"fluentqt-1.6.0-cp311-cp311-test_{index:02d}.whl"
            wheel = dist / filename
            wheel.write_bytes(f"core-{index}".encode("ascii"))
            files.append(
                {
                    "distribution": "FluentQt",
                    "filename": filename,
                    "sha256": VERIFIER.sha256_file(wheel),
                }
            )
        gallery = dist / "fluentqt_gallery-1.6.0-py3-none-any.whl"
        gallery.write_bytes(b"gallery")
        files.append(
            {
                "distribution": "FluentQt-Gallery",
                "filename": gallery.name,
                "sha256": VERIFIER.sha256_file(gallery),
            }
        )
        audit_entries = []
        for index in range(5):
            audit = audits / f"linux-{index}.json"
            audit.write_text(f'{{"index": {index}}}\n', encoding="utf-8")
            audit_entries.append(
                {
                    "filename": audit.name,
                    "sha256": VERIFIER.sha256_file(audit),
                }
            )
        (root / "PYTHON_SHA256SUMS.txt").write_text(
            "".join(
                f"{item['sha256']}  dist/{item['filename']}\n"
                for item in sorted(files, key=lambda item: item["filename"])
            ),
            encoding="utf-8",
        )
        return {
            "schema_version": 1,
            "version": "1.6.0",
            "files": files,
            "audits": audit_entries,
        }

    def test_complete_release_is_accepted(self):
        VERIFIER.compare_release(
            "FluentQt", self.expected, self.actual(), "complete"
        )

    def test_complete_release_rejects_missing_file(self):
        with self.assertRaisesRegex(
            VERIFIER.IndexVerificationError, "missing files"
        ):
            VERIFIER.compare_release(
                "FluentQt",
                self.expected,
                self.actual([next(iter(self.expected))]),
                "complete",
            )

    def test_subset_release_accepts_partial_matching_upload(self):
        VERIFIER.compare_release(
            "FluentQt",
            self.expected,
            self.actual([next(iter(self.expected))]),
            "subset",
        )

    def test_subset_release_rejects_hash_conflict(self):
        actual = self.actual()
        actual[next(iter(actual))]["sha256"] = "0" * 64
        with self.assertRaisesRegex(
            VERIFIER.IndexVerificationError, "SHA256 conflicts"
        ):
            VERIFIER.compare_release(
                "FluentQt", self.expected, actual, "subset"
            )

    def test_release_rejects_unexpected_file(self):
        actual = self.actual()
        actual["fluentqt-1.6.0.tar.gz"] = {
            "sha256": "c" * 64,
            "url": "https://files.pythonhosted.org/fluentqt-1.6.0.tar.gz",
        }
        with self.assertRaisesRegex(
            VERIFIER.IndexVerificationError, "unexpected files"
        ):
            VERIFIER.compare_release(
                "FluentQt", self.expected, actual, "subset"
            )

    def test_absent_mode_rejects_existing_files(self):
        with self.assertRaisesRegex(
            VERIFIER.IndexVerificationError, "already contains files"
        ):
            VERIFIER.compare_release(
                "FluentQt", self.expected, self.actual(), "absent"
            )

    def test_absent_mode_accepts_missing_release(self):
        VERIFIER.compare_release("FluentQt", self.expected, {}, "absent")

    def test_local_bundle_accepts_exact_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = self.make_bundle(root)
            VERIFIER.expected_files(manifest, root)

    def test_local_bundle_rejects_unexpected_dist_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = self.make_bundle(root)
            (root / "dist" / "unexpected.whl").write_bytes(b"unexpected")
            with self.assertRaisesRegex(
                VERIFIER.IndexVerificationError, "dist file set mismatch"
            ):
                VERIFIER.expected_files(manifest, root)

    def test_local_bundle_rejects_changed_audit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = self.make_bundle(root)
            (root / "audits" / "linux-0.json").write_text(
                "{}\n", encoding="utf-8"
            )
            with self.assertRaisesRegex(
                VERIFIER.IndexVerificationError, "bundle audit"
            ):
                VERIFIER.expected_files(manifest, root)


if __name__ == "__main__":
    unittest.main()
