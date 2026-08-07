#!/usr/bin/env python3
"""Regression tests for the canonical PySide6 release-bundle contract."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile


SCRIPT_PATH = Path(__file__).with_name("assemble-pyside-release-bundle.py")
SPEC = importlib.util.spec_from_file_location(
    "assemble_pyside_release_bundle", SCRIPT_PATH
)
ASSEMBLER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = ASSEMBLER
SPEC.loader.exec_module(ASSEMBLER)


def zip_info(name: str) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED
    info.create_system = 3
    info.external_attr = 0o100644 << 16
    return info


def write_wheel(
    path: Path,
    *,
    distribution: str,
    version: str,
    tag: str,
    requirements: list[str],
    purelib: bool,
    marker: bytes = b"same Gallery payload",
) -> None:
    normalized = distribution.lower().replace("-", "_")
    dist_info = f"{normalized}-{version}.dist-info"
    metadata = (
        "Metadata-Version: 2.1\n"
        f"Name: {distribution}\n"
        f"Version: {version}\n"
        "License: MIT\n"
        f"Requires-Python: {ASSEMBLER.RELEASE_REQUIRES_PYTHON}\n"
        + "".join(f"Requires-Dist: {item}\n" for item in requirements)
        + "\n"
    ).encode("utf-8")
    wheel_metadata = (
        "Wheel-Version: 1.0\n"
        f"Root-Is-Purelib: {'true' if purelib else 'false'}\n"
        f"Tag: {tag}\n\n"
    ).encode("utf-8")
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr(zip_info(f"{dist_info}/METADATA"), metadata)
        archive.writestr(zip_info(f"{dist_info}/WHEEL"), wheel_metadata)
        archive.writestr(zip_info(f"{normalized}/payload.bin"), marker)


class ReleaseBundleAssemblerTest(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name)
        self.input_dir = self.root / "input"
        self.output_dir = self.root / "bundle"
        self.catalog_path = ASSEMBLER.DEFAULT_CATALOG
        self.version_file = ASSEMBLER.DEFAULT_VERSION_FILE
        self.catalog = ASSEMBLER.load_json(self.catalog_path)
        self.scenarios = ASSEMBLER.release_scenarios(self.catalog)
        self.version = ASSEMBLER.read_project_version(self.version_file)
        self.create_complete_input()

    def tearDown(self):
        self.temp_dir.cleanup()

    def artifact_dir(self, scenario):
        return self.input_dir / ASSEMBLER.expected_artifact_name(scenario)

    def core_wheel(self, scenario):
        return self.artifact_dir(scenario) / (
            f"fluentqt-{self.version}-{scenario['publish_wheel_suffix']}.whl"
        )

    def gallery_wheel(self, scenario):
        return self.artifact_dir(scenario) / (
            f"fluentqt_gallery-{self.version}-py3-none-any.whl"
        )

    def create_complete_input(self):
        for scenario in self.scenarios:
            core = self.core_wheel(scenario)
            gallery = self.gallery_wheel(scenario)
            write_wheel(
                core,
                distribution="FluentQt",
                version=self.version,
                tag=scenario["publish_wheel_suffix"],
                requirements=[
                    f"PySide6-Essentials (=={scenario['pyside_version']})",
                    f"shiboken6 (=={scenario['shiboken_version']})",
                ],
                purelib=False,
                marker=scenario["id"].encode("utf-8"),
            )
            write_wheel(
                gallery,
                distribution="FluentQt-Gallery",
                version=self.version,
                tag="py3-none-any",
                requirements=[f"FluentQt (=={self.version})"],
                purelib=True,
            )
            if scenario["platform"] == "linux":
                target_platform = scenario["publish_wheel_suffix"].split(
                    "-", 2
                )[2]
                report = {
                    "schema_version": 1,
                    "output_wheel": core.name,
                    "output_sha256": ASSEMBLER.sha256_file(core),
                    "policy": scenario["manylinux_policy"],
                    "architecture": scenario["binary_arch"],
                    "target_platform": target_platform,
                    "archive": {"metadata_version": self.version},
                }
                (self.artifact_dir(scenario) / "manylinux-audit.json").write_text(
                    json.dumps(report), encoding="utf-8"
                )

    def assemble(self):
        return ASSEMBLER.assemble_bundle(
            input_dir=self.input_dir,
            output_dir=self.output_dir,
            catalog_path=self.catalog_path,
            version_file=self.version_file,
            repository="calvinhxx/Fluent-Qt",
            commit="a" * 40,
            ci_run_id="12345",
            ci_run_attempt="1",
        )

    def test_complete_matrix_produces_eighteen_wheel_bundle(self):
        manifest = self.assemble()

        self.assertEqual(len(manifest["files"]), 18)
        self.assertEqual(len(manifest["audits"]), 5)
        self.assertEqual(len(list((self.output_dir / "dist").glob("*.whl"))), 18)
        self.assertEqual(len(list((self.output_dir / "audits").glob("*.json"))), 5)
        self.assertTrue((self.output_dir / "PYTHON_SHA256SUMS.txt").is_file())
        written = json.loads(
            (self.output_dir / "python-release-manifest.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(written, manifest)
        self.assertEqual(written["source"]["commit"], "a" * 40)

    def test_missing_release_wheel_is_rejected(self):
        self.core_wheel(self.scenarios[0]).unlink()

        with self.assertRaisesRegex(ASSEMBLER.BundleError, "expected one"):
            self.assemble()

    def test_compatibility_wheel_is_rejected(self):
        artifact = self.artifact_dir(self.scenarios[0])
        write_wheel(
            artifact / f"fluentqt-{self.version}-cp310-cp310-win_amd64.whl",
            distribution="FluentQt",
            version=self.version,
            tag="cp310-cp310-win_amd64",
            requirements=[
                "PySide6-Essentials (==6.9.3)",
                "shiboken6 (==6.9.3)",
            ],
            purelib=False,
        )

        with self.assertRaisesRegex(ASSEMBLER.BundleError, "exactly the core"):
            self.assemble()

    def test_unrepaired_linux_wheel_is_rejected(self):
        scenario = next(
            item for item in self.scenarios if item["platform"] == "linux"
        )
        expected = self.core_wheel(scenario)
        raw = expected.with_name(
            f"fluentqt-{self.version}-{scenario['expected_wheel_suffix']}.whl"
        )
        expected.rename(raw)

        with self.assertRaisesRegex(ASSEMBLER.BundleError, "expected one"):
            self.assemble()

    def test_wrong_wheel_version_is_rejected(self):
        scenario = self.scenarios[0]
        wheel = self.core_wheel(scenario)
        write_wheel(
            wheel,
            distribution="FluentQt",
            version="9.9.9",
            tag=scenario["publish_wheel_suffix"],
            requirements=[
                f"PySide6-Essentials (=={scenario['pyside_version']})",
                f"shiboken6 (=={scenario['shiboken_version']})",
            ],
            purelib=False,
        )

        with self.assertRaisesRegex(ASSEMBLER.BundleError, "declares Version"):
            self.assemble()

    def test_manylinux_hash_conflict_is_rejected(self):
        scenario = next(
            item for item in self.scenarios if item["platform"] == "linux"
        )
        report_path = self.artifact_dir(scenario) / "manylinux-audit.json"
        report = json.loads(report_path.read_text(encoding="utf-8"))
        report["output_sha256"] = "0" * 64
        report_path.write_text(json.dumps(report), encoding="utf-8")

        with self.assertRaisesRegex(ASSEMBLER.BundleError, "output_sha256"):
            self.assemble()

    def test_gallery_byte_drift_is_rejected(self):
        scenario = self.scenarios[0]
        write_wheel(
            self.gallery_wheel(scenario),
            distribution="FluentQt-Gallery",
            version=self.version,
            tag="py3-none-any",
            requirements=[f"FluentQt (=={self.version})"],
            purelib=True,
            marker=b"different Gallery payload",
        )

        with self.assertRaisesRegex(ASSEMBLER.BundleError, "byte-identical"):
            self.assemble()

    def test_unexpected_artifact_directory_is_rejected(self):
        (self.input_dir / "fluentqt-pyside6-qt624-cp310-linux-x64").mkdir()

        with self.assertRaisesRegex(ASSEMBLER.BundleError, "unexpected"):
            self.assemble()


if __name__ == "__main__":
    unittest.main()
