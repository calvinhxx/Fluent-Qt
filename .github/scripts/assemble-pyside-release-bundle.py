#!/usr/bin/env python3
"""Assemble the canonical FluentQt Python release bundle from CI artifacts."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from email.parser import Parser
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
from typing import Any
import zipfile


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CATALOG = ROOT / "bindings" / "pyside6" / "wheel-matrix.json"
DEFAULT_VERSION_FILE = ROOT / "CMakeLists.txt"
RELEASE_REQUIRES_PYTHON = ">=3.11,<3.14"
CORE_DISTRIBUTION = "FluentQt"
GALLERY_DISTRIBUTION = "FluentQt-Gallery"
GALLERY_TAG = "py3-none-any"
REQUIRED_PROJECT_URL_LABELS = {
    "Changelog",
    "Documentation",
    "Homepage",
    "Issues",
    "Repository",
}
REQUIRED_LICENSE_FILES = {
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "TRADEMARKS.md",
}
COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
VERSION_PATTERN = re.compile(
    r"project\s*\(\s*FluentQt\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)",
    re.MULTILINE,
)


class BundleError(RuntimeError):
    """Raised when release artifacts do not satisfy the publication contract."""


@dataclass(frozen=True)
class WheelRecord:
    path: Path
    filename: str
    distribution: str
    scenario_id: str | None
    sha256: str
    size: int


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise BundleError(f"cannot read valid JSON from {path}: {error}") from error
    if not isinstance(value, dict):
        raise BundleError(f"{path} must contain a JSON object")
    return value


def read_project_version(path: Path) -> str:
    try:
        contents = path.read_text(encoding="utf-8")
    except OSError as error:
        raise BundleError(f"cannot read project version from {path}: {error}") from error
    match = VERSION_PATTERN.search(contents)
    if match is None:
        raise BundleError(f"cannot find project(FluentQt VERSION ...) in {path}")
    return match.group(1)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def normalized_distribution(value: str) -> str:
    return re.sub(r"[-_.]+", "-", value).lower()


def read_wheel_metadata(wheel: Path) -> tuple[Any, Any]:
    try:
        with zipfile.ZipFile(wheel) as archive:
            bad_member = archive.testzip()
            if bad_member is not None:
                raise BundleError(
                    f"wheel {wheel.name} failed CRC validation at {bad_member}"
                )
            names = archive.namelist()
            metadata_names = [
                name for name in names if name.endswith(".dist-info/METADATA")
            ]
            wheel_names = [
                name for name in names if name.endswith(".dist-info/WHEEL")
            ]
            if len(metadata_names) != 1 or len(wheel_names) != 1:
                raise BundleError(
                    f"wheel {wheel.name} must contain one METADATA and one WHEEL"
                )
            metadata = Parser().parsestr(
                archive.read(metadata_names[0]).decode("utf-8")
            )
            wheel_metadata = Parser().parsestr(
                archive.read(wheel_names[0]).decode("utf-8")
            )
            return metadata, wheel_metadata
    except (OSError, UnicodeDecodeError, zipfile.BadZipFile) as error:
        raise BundleError(f"cannot inspect wheel {wheel}: {error}") from error


def require_wheel_metadata(
    wheel: Path,
    *,
    distribution: str,
    version: str,
    tag: str,
    requirements: set[str],
    purelib: bool,
) -> None:
    metadata, wheel_metadata = read_wheel_metadata(wheel)
    actual_distribution = metadata.get("Name", "")
    if normalized_distribution(actual_distribution) != normalized_distribution(
        distribution
    ):
        raise BundleError(
            f"wheel {wheel.name} declares Name {actual_distribution!r}, "
            f"expected {distribution!r}"
        )
    if metadata.get("Version") != version:
        raise BundleError(
            f"wheel {wheel.name} declares Version {metadata.get('Version')!r}, "
            f"expected {version!r}"
        )
    if metadata.get("Requires-Python") != RELEASE_REQUIRES_PYTHON:
        raise BundleError(
            f"wheel {wheel.name} must declare Requires-Python: "
            f"{RELEASE_REQUIRES_PYTHON}"
        )
    if metadata.get("Metadata-Version") != "2.4":
        raise BundleError(
            f"wheel {wheel.name} must use Core Metadata 2.4"
        )
    if metadata.get("License-Expression") != "MIT":
        raise BundleError(
            f"wheel {wheel.name} must declare License-Expression: MIT"
        )
    actual_license_files = set(metadata.get_all("License-File", []))
    if actual_license_files != REQUIRED_LICENSE_FILES:
        raise BundleError(
            f"wheel {wheel.name} has unexpected License-File metadata: "
            f"{sorted(actual_license_files)}"
        )
    content_type = metadata.get("Description-Content-Type", "")
    if content_type.split(";", 1)[0].strip().lower() != "text/markdown":
        raise BundleError(
            f"wheel {wheel.name} must declare a Markdown description"
        )
    description = metadata.get_payload()
    if not isinstance(description, str) or len(description.strip()) < 200:
        raise BundleError(
            f"wheel {wheel.name} must contain a complete Markdown description"
        )
    project_url_labels = set()
    for value in metadata.get_all("Project-URL", []):
        label, separator, url = value.partition(",")
        if separator and url.strip().startswith("https://"):
            project_url_labels.add(label.strip())
    missing_project_urls = REQUIRED_PROJECT_URL_LABELS - project_url_labels
    if missing_project_urls:
        raise BundleError(
            f"wheel {wheel.name} is missing Project-URL labels: "
            f"{sorted(missing_project_urls)}"
        )
    classifiers = set(metadata.get_all("Classifier", []))
    if "Programming Language :: Python :: 3" not in classifiers:
        raise BundleError(
            f"wheel {wheel.name} must declare its Python 3 classifier"
        )
    actual_requirements = set(metadata.get_all("Requires-Dist", []))
    if actual_requirements != requirements:
        raise BundleError(
            f"wheel {wheel.name} has unexpected Requires-Dist metadata: "
            f"{sorted(actual_requirements)}"
        )
    tags = wheel_metadata.get_all("Tag", [])
    if tags != [tag]:
        raise BundleError(
            f"wheel {wheel.name} declares tags {tags}, expected {[tag]}"
        )
    expected_purelib = "true" if purelib else "false"
    if wheel_metadata.get("Root-Is-Purelib", "").lower() != expected_purelib:
        raise BundleError(
            f"wheel {wheel.name} has an invalid Root-Is-Purelib value"
        )


def release_scenarios(catalog: dict[str, Any]) -> list[dict[str, Any]]:
    scenarios = catalog.get("scenarios")
    if not isinstance(scenarios, list):
        raise BundleError("wheel matrix must contain a scenarios array")
    selected = [
        scenario
        for scenario in scenarios
        if isinstance(scenario, dict) and scenario.get("release") is True
    ]
    if len(selected) != 17:
        raise BundleError(
            f"wheel matrix must define exactly 17 release scenarios, found {len(selected)}"
        )
    return sorted(selected, key=lambda scenario: str(scenario.get("id", "")))


def expected_artifact_name(scenario: dict[str, Any]) -> str:
    suffix = scenario.get("artifact_suffix")
    if not isinstance(suffix, str) or not suffix:
        raise BundleError(f"scenario {scenario.get('id')!r} has no artifact suffix")
    return f"fluentqt-pyside6-{suffix}"


def only_file(paths: list[Path], description: str) -> Path:
    if len(paths) != 1:
        raise BundleError(f"expected one {description}, found {len(paths)}")
    return paths[0]


def validate_audit_report(
    report_path: Path,
    scenario: dict[str, Any],
    wheel: Path,
    version: str,
) -> dict[str, Any]:
    report = load_json(report_path)
    scenario_id = str(scenario["id"])
    expected_platform = str(scenario["publish_wheel_suffix"]).split("-", 2)[2]
    expected = {
        "schema_version": 1,
        "output_wheel": wheel.name,
        "output_sha256": sha256_file(wheel),
        "policy": scenario["manylinux_policy"],
        "architecture": scenario["binary_arch"],
        "target_platform": expected_platform,
    }
    for key, value in expected.items():
        if report.get(key) != value:
            raise BundleError(
                f"manylinux audit for {scenario_id} has {key}={report.get(key)!r}, "
                f"expected {value!r}"
            )
    archive = report.get("archive")
    if not isinstance(archive, dict) or archive.get("metadata_version") != version:
        raise BundleError(
            f"manylinux audit for {scenario_id} does not record version {version}"
        )
    return report


def validate_source_identity(
    repository: str, commit: str, ci_run_id: str, ci_run_attempt: str
) -> None:
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
        raise BundleError(f"invalid GitHub repository identity: {repository!r}")
    if COMMIT_PATTERN.fullmatch(commit) is None:
        raise BundleError("commit must be a lowercase 40-character SHA")
    if not ci_run_id.isdigit() or not ci_run_attempt.isdigit():
        raise BundleError("CI run id and attempt must be positive integers")
    if int(ci_run_id) <= 0 or int(ci_run_attempt) <= 0:
        raise BundleError("CI run id and attempt must be positive integers")


def assemble_bundle(
    *,
    input_dir: Path,
    output_dir: Path,
    catalog_path: Path,
    version_file: Path,
    repository: str,
    commit: str,
    ci_run_id: str,
    ci_run_attempt: str,
) -> dict[str, Any]:
    validate_source_identity(repository, commit, ci_run_id, ci_run_attempt)
    if not input_dir.is_dir():
        raise BundleError(f"input artifact directory does not exist: {input_dir}")
    if output_dir.exists():
        raise BundleError(f"output bundle directory already exists: {output_dir}")

    catalog = load_json(catalog_path)
    scenarios = release_scenarios(catalog)
    version = read_project_version(version_file)
    expected_artifacts = {
        expected_artifact_name(scenario): scenario for scenario in scenarios
    }
    actual_entries = sorted(input_dir.iterdir(), key=lambda path: path.name)
    files_at_root = [path.name for path in actual_entries if not path.is_dir()]
    if files_at_root:
        raise BundleError(
            "release artifact root must contain directories only: "
            + ", ".join(files_at_root)
        )
    actual_artifacts = {path.name: path for path in actual_entries}
    missing = sorted(set(expected_artifacts) - set(actual_artifacts))
    extra = sorted(set(actual_artifacts) - set(expected_artifacts))
    if missing or extra:
        details = []
        if missing:
            details.append("missing: " + ", ".join(missing))
        if extra:
            details.append("unexpected: " + ", ".join(extra))
        raise BundleError("release artifact set mismatch (" + "; ".join(details) + ")")

    core_records: list[WheelRecord] = []
    gallery_candidates: list[Path] = []
    audit_records: list[tuple[str, Path]] = []
    for artifact_name, scenario in sorted(expected_artifacts.items()):
        artifact_dir = actual_artifacts[artifact_name]
        scenario_id = str(scenario["id"])
        publish_suffix = scenario.get("publish_wheel_suffix")
        if not isinstance(publish_suffix, str) or not publish_suffix:
            raise BundleError(f"release scenario {scenario_id} has no publish suffix")
        expected_core_name = f"fluentqt-{version}-{publish_suffix}.whl"
        expected_gallery_name = (
            f"fluentqt_gallery-{version}-{GALLERY_TAG}.whl"
        )
        wheels = sorted(artifact_dir.rglob("*.whl"))
        core_wheel = only_file(
            [wheel for wheel in wheels if wheel.name == expected_core_name],
            f"{scenario_id} core wheel named {expected_core_name}",
        )
        gallery_wheel = only_file(
            [wheel for wheel in wheels if wheel.name == expected_gallery_name],
            f"{scenario_id} Gallery wheel named {expected_gallery_name}",
        )
        if len(wheels) != 2:
            raise BundleError(
                f"artifact {artifact_name} must contain exactly the core and Gallery "
                f"wheels; found {[wheel.name for wheel in wheels]}"
            )

        pyside_version = str(scenario["pyside_version"])
        shiboken_version = str(scenario["shiboken_version"])
        require_wheel_metadata(
            core_wheel,
            distribution=CORE_DISTRIBUTION,
            version=version,
            tag=publish_suffix,
            requirements={
                f"PySide6-Essentials (=={pyside_version})",
                f"shiboken6 (=={shiboken_version})",
            },
            purelib=False,
        )
        require_wheel_metadata(
            gallery_wheel,
            distribution=GALLERY_DISTRIBUTION,
            version=version,
            tag=GALLERY_TAG,
            requirements={f"FluentQt (=={version})"},
            purelib=True,
        )
        core_records.append(
            WheelRecord(
                path=core_wheel,
                filename=core_wheel.name,
                distribution=CORE_DISTRIBUTION,
                scenario_id=scenario_id,
                sha256=sha256_file(core_wheel),
                size=core_wheel.stat().st_size,
            )
        )
        gallery_candidates.append(gallery_wheel)

        audits = sorted(artifact_dir.rglob("manylinux-audit.json"))
        if scenario.get("platform") == "linux":
            audit_path = only_file(audits, f"{scenario_id} manylinux audit")
            validate_audit_report(audit_path, scenario, core_wheel, version)
            audit_records.append((scenario_id, audit_path))
        elif audits:
            raise BundleError(
                f"non-Linux artifact {artifact_name} contains a manylinux audit"
            )

    core_filenames = [record.filename for record in core_records]
    if len(set(core_filenames)) != 17:
        raise BundleError("release core wheel filenames must be unique")
    if len(audit_records) != 5:
        raise BundleError(
            f"release bundle must contain five manylinux audits, found {len(audit_records)}"
        )

    gallery_hashes = {sha256_file(path) for path in gallery_candidates}
    if len(gallery_candidates) != 17 or len(gallery_hashes) != 1:
        raise BundleError(
            "all 17 release lanes must produce a byte-identical Gallery wheel"
        )
    gallery_source = sorted(gallery_candidates, key=lambda path: str(path))[0]
    gallery_record = WheelRecord(
        path=gallery_source,
        filename=gallery_source.name,
        distribution=GALLERY_DISTRIBUTION,
        scenario_id=None,
        sha256=sha256_file(gallery_source),
        size=gallery_source.stat().st_size,
    )

    output_dir.parent.mkdir(parents=True, exist_ok=True)
    staging_path = Path(
        tempfile.mkdtemp(prefix="fluentqt-python-release-", dir=output_dir.parent)
    )
    try:
        dist_dir = staging_path / "dist"
        audits_dir = staging_path / "audits"
        dist_dir.mkdir()
        audits_dir.mkdir()
        all_records = sorted(
            [*core_records, gallery_record], key=lambda record: record.filename
        )
        for record in all_records:
            shutil.copyfile(record.path, dist_dir / record.filename)
        audit_manifest: list[dict[str, Any]] = []
        for scenario_id, source in sorted(audit_records):
            destination = audits_dir / f"{scenario_id}.json"
            shutil.copyfile(source, destination)
            audit_manifest.append(
                {
                    "filename": destination.name,
                    "scenario_id": scenario_id,
                    "sha256": sha256_file(destination),
                }
            )

        checksum_lines = [
            f"{record.sha256}  dist/{record.filename}" for record in all_records
        ]
        (staging_path / "PYTHON_SHA256SUMS.txt").write_text(
            "\n".join(checksum_lines) + "\n", encoding="utf-8"
        )
        manifest = {
            "schema_version": 1,
            "version": version,
            "source": {
                "repository": repository,
                "commit": commit,
                "ci_run_id": int(ci_run_id),
                "ci_run_attempt": int(ci_run_attempt),
            },
            "policy": {
                "core_wheel_count": 17,
                "gallery_wheel_count": 1,
                "manylinux_audit_count": 5,
                "requires_python": RELEASE_REQUIRES_PYTHON,
            },
            "files": [
                {
                    "filename": record.filename,
                    "distribution": record.distribution,
                    "scenario_id": record.scenario_id,
                    "sha256": record.sha256,
                    "size": record.size,
                }
                for record in all_records
            ],
            "audits": audit_manifest,
        }
        (staging_path / "python-release-manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        staging_path.replace(output_dir)
        return manifest
    except Exception:
        shutil.rmtree(staging_path, ignore_errors=True)
        raise


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    parser.add_argument("--version-file", type=Path, default=DEFAULT_VERSION_FILE)
    parser.add_argument(
        "--repository", default=os.environ.get("GITHUB_REPOSITORY", "")
    )
    parser.add_argument("--commit", default=os.environ.get("GITHUB_SHA", ""))
    parser.add_argument(
        "--ci-run-id", default=os.environ.get("GITHUB_RUN_ID", "")
    )
    parser.add_argument(
        "--ci-run-attempt", default=os.environ.get("GITHUB_RUN_ATTEMPT", "")
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        manifest = assemble_bundle(
            input_dir=args.input,
            output_dir=args.output,
            catalog_path=args.catalog,
            version_file=args.version_file,
            repository=args.repository,
            commit=args.commit,
            ci_run_id=args.ci_run_id,
            ci_run_attempt=args.ci_run_attempt,
        )
    except (BundleError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(
        "Assembled FluentQt Python release bundle: "
        f"{len(manifest['files'])} wheel(s), {len(manifest['audits'])} audit(s)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
