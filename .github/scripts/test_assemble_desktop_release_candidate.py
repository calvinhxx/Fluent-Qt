#!/usr/bin/env python3
"""Regression tests for immutable desktop release candidates."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import shutil
import sys
import tempfile
import unittest


SCRIPT_PATH = Path(__file__).with_name("assemble-desktop-release-candidate.py")
SPEC = importlib.util.spec_from_file_location(
    "assemble_desktop_release_candidate", SCRIPT_PATH
)
ASSEMBLER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = ASSEMBLER
SPEC.loader.exec_module(ASSEMBLER)


class DesktopReleaseCandidateTest(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name)
        self.input_dir = self.root / "input"
        self.output_dir = self.root / "candidate"
        self.version = "1.7.2"
        self.repository = "calvinhxx/Fluent-Qt"
        self.commit = "a" * 40
        self.run_id = "12345"
        self.run_attempt = "2"
        self.catalog = ASSEMBLER.load_json(ASSEMBLER.DEFAULT_CATALOG)
        self.create_input("standard")

    def tearDown(self):
        self.temp_dir.cleanup()

    def create_input(self, package_set):
        self.input_dir.mkdir(parents=True, exist_ok=True)
        for scenario in ASSEMBLER.selected_scenarios(self.catalog, package_set):
            artifact = self.input_dir / ASSEMBLER.expected_artifact_name(scenario)
            artifact.mkdir(parents=True)
            package = artifact / ASSEMBLER.expected_package_filename(
                scenario, self.version
            )
            package.write_bytes(f"package:{scenario['id']}".encode("utf-8"))

    def assemble(self, package_set="standard"):
        return ASSEMBLER.assemble_candidate(
            input_dir=self.input_dir,
            output_dir=self.output_dir,
            catalog_path=ASSEMBLER.DEFAULT_CATALOG,
            version=self.version,
            package_set=package_set,
            repository=self.repository,
            commit=self.commit,
            ci_run_id=self.run_id,
            ci_run_attempt=self.run_attempt,
        )

    def verify(self, package_set="standard"):
        return ASSEMBLER.verify_candidate(
            candidate_dir=self.output_dir,
            catalog_path=ASSEMBLER.DEFAULT_CATALOG,
            version=self.version,
            package_set=package_set,
            repository=self.repository,
            commit=self.commit,
            ci_run_id=self.run_id,
            ci_run_attempt=self.run_attempt,
        )

    def test_standard_candidate_contains_nine_verified_packages(self):
        manifest = self.assemble()

        self.assertEqual(len(manifest["packages"]), 9)
        self.assertEqual(self.verify(), manifest)
        self.assertEqual(len(list((self.output_dir / "dist").iterdir())), 9)

    def test_smoke_candidate_contains_two_packages(self):
        shutil.rmtree(self.input_dir)
        self.create_input("smoke")

        manifest = self.assemble("smoke")

        self.assertEqual(len(manifest["packages"]), 2)
        self.assertEqual(self.verify("smoke"), manifest)

    def test_missing_package_artifact_is_rejected(self):
        artifact = next(self.input_dir.iterdir())
        for package in artifact.iterdir():
            package.unlink()
        artifact.rmdir()

        with self.assertRaisesRegex(ASSEMBLER.CandidateError, "missing"):
            self.assemble()

    def test_unexpected_file_name_is_rejected(self):
        artifact = next(self.input_dir.iterdir())
        package = next(artifact.iterdir())
        package.rename(artifact / "unexpected.pkg")

        with self.assertRaisesRegex(ASSEMBLER.CandidateError, "must contain only"):
            self.assemble()

    def test_tampered_package_is_rejected(self):
        self.assemble()
        package = next((self.output_dir / "dist").iterdir())
        package.write_bytes(b"tampered")

        with self.assertRaisesRegex(ASSEMBLER.CandidateError, "mismatch"):
            self.verify()

    def test_wrong_source_run_is_rejected(self):
        self.assemble()
        self.run_id = "54321"

        with self.assertRaisesRegex(ASSEMBLER.CandidateError, "source identity"):
            self.verify()


if __name__ == "__main__":
    unittest.main()
