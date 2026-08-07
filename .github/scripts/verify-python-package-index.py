#!/usr/bin/env python3
"""Verify a FluentQt release bundle against PyPI or TestPyPI JSON metadata."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
import time
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen


DISTRIBUTIONS = ("FluentQt", "FluentQt-Gallery")
MODES = ("absent", "subset", "complete")


class IndexVerificationError(RuntimeError):
    """Raised when package-index state differs from the release manifest."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_manifest(path: Path) -> dict[str, Any]:
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise IndexVerificationError(
            f"cannot read release manifest {path}: {error}"
        ) from error
    if not isinstance(manifest, dict) or manifest.get("schema_version") != 1:
        raise IndexVerificationError("release manifest must use schema version 1")
    version = manifest.get("version")
    if not isinstance(version, str) or not version:
        raise IndexVerificationError("release manifest has no version")
    files = manifest.get("files")
    if not isinstance(files, list) or len(files) != 18:
        raise IndexVerificationError("release manifest must contain 18 wheel files")
    audits = manifest.get("audits")
    if not isinstance(audits, list) or len(audits) != 5:
        raise IndexVerificationError("release manifest must contain five audits")
    return manifest


def expected_files(
    manifest: dict[str, Any], bundle_dir: Path | None = None
) -> dict[str, dict[str, str]]:
    expected = {distribution: {} for distribution in DISTRIBUTIONS}
    for item in manifest["files"]:
        if not isinstance(item, dict):
            raise IndexVerificationError("release manifest file entries must be objects")
        distribution = item.get("distribution")
        filename = item.get("filename")
        digest = item.get("sha256")
        if distribution not in expected:
            raise IndexVerificationError(
                f"release manifest contains unexpected distribution {distribution!r}"
            )
        if not isinstance(filename, str) or not filename.endswith(".whl"):
            raise IndexVerificationError("release manifest has an invalid wheel filename")
        if not isinstance(digest, str) or len(digest) != 64:
            raise IndexVerificationError(
                f"release manifest has an invalid SHA256 for {filename}"
            )
        if filename in expected[distribution]:
            raise IndexVerificationError(f"duplicate release filename: {filename}")
        expected[distribution][filename] = digest
    if len(expected["FluentQt"]) != 17:
        raise IndexVerificationError("manifest must contain 17 FluentQt wheels")
    if len(expected["FluentQt-Gallery"]) != 1:
        raise IndexVerificationError("manifest must contain one Gallery wheel")
    if bundle_dir is not None:
        validate_local_bundle(manifest, bundle_dir, expected)
    return expected


def directory_files(path: Path, description: str) -> dict[str, Path]:
    if not path.is_dir():
        raise IndexVerificationError(f"bundle has no {description} directory")
    entries = sorted(path.iterdir(), key=lambda item: item.name)
    non_files = [entry.name for entry in entries if not entry.is_file()]
    if non_files:
        raise IndexVerificationError(
            f"bundle {description} contains non-files: " + ", ".join(non_files)
        )
    return {entry.name: entry for entry in entries}


def validate_local_bundle(
    manifest: dict[str, Any],
    bundle_dir: Path,
    expected: dict[str, dict[str, str]],
) -> None:
    expected_wheels = {
        filename: digest
        for distribution in DISTRIBUTIONS
        for filename, digest in expected[distribution].items()
    }
    actual_wheels = directory_files(bundle_dir / "dist", "dist")
    if set(actual_wheels) != set(expected_wheels):
        missing = sorted(set(expected_wheels) - set(actual_wheels))
        extra = sorted(set(actual_wheels) - set(expected_wheels))
        details = []
        if missing:
            details.append("missing: " + ", ".join(missing))
        if extra:
            details.append("unexpected: " + ", ".join(extra))
        raise IndexVerificationError(
            "bundle dist file set mismatch (" + "; ".join(details) + ")"
        )
    for filename, digest in expected_wheels.items():
        actual_digest = sha256_file(actual_wheels[filename])
        if actual_digest != digest:
            raise IndexVerificationError(
                f"bundle wheel {filename} has SHA256 {actual_digest}, expected {digest}"
            )

    expected_audits: dict[str, str] = {}
    for item in manifest["audits"]:
        if not isinstance(item, dict):
            raise IndexVerificationError("release manifest audit entries must be objects")
        filename = item.get("filename")
        digest = item.get("sha256")
        if not isinstance(filename, str) or not filename.endswith(".json"):
            raise IndexVerificationError("release manifest has an invalid audit filename")
        if not isinstance(digest, str) or len(digest) != 64:
            raise IndexVerificationError(
                f"release manifest has an invalid audit SHA256 for {filename}"
            )
        if filename in expected_audits:
            raise IndexVerificationError(f"duplicate audit filename: {filename}")
        expected_audits[filename] = digest
    actual_audits = directory_files(bundle_dir / "audits", "audits")
    if set(actual_audits) != set(expected_audits):
        raise IndexVerificationError("bundle audit file set does not match the manifest")
    for filename, digest in expected_audits.items():
        actual_digest = sha256_file(actual_audits[filename])
        if actual_digest != digest:
            raise IndexVerificationError(
                f"bundle audit {filename} has SHA256 {actual_digest}, expected {digest}"
            )

    checksums = bundle_dir / "PYTHON_SHA256SUMS.txt"
    expected_checksum_text = "".join(
        f"{digest}  dist/{filename}\n"
        for filename, digest in sorted(expected_wheels.items())
    )
    try:
        actual_checksum_text = checksums.read_text(encoding="utf-8")
    except OSError as error:
        raise IndexVerificationError(
            f"cannot read bundle checksum file: {error}"
        ) from error
    if actual_checksum_text != expected_checksum_text:
        raise IndexVerificationError(
            "PYTHON_SHA256SUMS.txt does not exactly match the release manifest"
        )


def fetch_release(
    index_json_url: str, distribution: str, version: str
) -> dict[str, Any] | None:
    url = "{0}/{1}/{2}/json".format(
        index_json_url.rstrip("/"), quote(distribution), quote(version)
    )
    request = Request(url, headers={"User-Agent": "FluentQt-release-verifier/1"})
    try:
        with urlopen(request, timeout=30) as response:
            value = json.load(response)
    except HTTPError as error:
        if error.code == 404:
            return None
        raise IndexVerificationError(
            f"package index returned HTTP {error.code} for {url}"
        ) from error
    except (OSError, URLError, json.JSONDecodeError) as error:
        raise IndexVerificationError(f"cannot query package index {url}: {error}") from error
    if not isinstance(value, dict):
        raise IndexVerificationError(f"package index response for {url} is not an object")
    return value


def actual_files(release: dict[str, Any] | None) -> dict[str, dict[str, str]]:
    if release is None:
        return {}
    urls = release.get("urls")
    if not isinstance(urls, list):
        raise IndexVerificationError("package index response has no urls array")
    actual: dict[str, dict[str, str]] = {}
    for item in urls:
        if not isinstance(item, dict):
            raise IndexVerificationError("package index file entries must be objects")
        filename = item.get("filename")
        digests = item.get("digests")
        url = item.get("url")
        if not isinstance(filename, str) or not isinstance(digests, dict):
            raise IndexVerificationError("package index returned invalid file metadata")
        digest = digests.get("sha256")
        if not isinstance(digest, str):
            raise IndexVerificationError(
                f"package index file {filename} has no SHA256 digest"
            )
        if not isinstance(url, str) or not url.startswith("https://"):
            raise IndexVerificationError(
                f"package index file {filename} has an invalid download URL"
            )
        if filename in actual:
            raise IndexVerificationError(
                f"package index returned duplicate filename {filename}"
            )
        actual[filename] = {"sha256": digest, "url": url}
    return actual


def compare_release(
    distribution: str,
    expected: dict[str, str],
    actual: dict[str, dict[str, str]],
    mode: str,
) -> None:
    if mode not in MODES:
        raise IndexVerificationError(f"unsupported verification mode: {mode}")
    expected_names = set(expected)
    actual_names = set(actual)
    if mode == "absent":
        if actual_names:
            raise IndexVerificationError(
                f"{distribution} release already contains files: "
                + ", ".join(sorted(actual_names))
            )
        return
    unexpected = sorted(actual_names - expected_names)
    if unexpected:
        raise IndexVerificationError(
            f"{distribution} release contains unexpected files: "
            + ", ".join(unexpected)
        )
    mismatched = sorted(
        filename
        for filename in actual_names & expected_names
        if actual[filename]["sha256"] != expected[filename]
    )
    if mismatched:
        raise IndexVerificationError(
            f"{distribution} release has SHA256 conflicts: "
            + ", ".join(mismatched)
        )
    if mode == "complete":
        missing = sorted(expected_names - actual_names)
        if missing:
            raise IndexVerificationError(
                f"{distribution} release is missing files: " + ", ".join(missing)
            )


def verify_index(
    *,
    manifest: dict[str, Any],
    index_json_url: str,
    mode: str,
) -> dict[str, Any]:
    expected = expected_files(manifest)
    version = manifest["version"]
    reports = []
    urls: list[str] = []
    for distribution in DISTRIBUTIONS:
        release = fetch_release(index_json_url, distribution, version)
        actual = actual_files(release)
        compare_release(distribution, expected[distribution], actual, mode)
        urls.extend(item["url"] for item in actual.values())
        reports.append(
            {
                "distribution": distribution,
                "expected_files": len(expected[distribution]),
                "actual_files": len(actual),
            }
        )
    return {
        "schema_version": 1,
        "index_json_url": index_json_url,
        "mode": mode,
        "version": version,
        "distributions": reports,
        "download_urls": sorted(urls),
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--bundle", type=Path)
    parser.add_argument("--index-json-url", required=True)
    parser.add_argument("--mode", choices=MODES, required=True)
    parser.add_argument("--attempts", type=int, default=1)
    parser.add_argument("--delay-seconds", type=float, default=0.0)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--url-file", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.attempts < 1 or args.delay_seconds < 0:
            raise IndexVerificationError("attempts and delay must be non-negative")
        manifest = load_manifest(args.manifest)
        expected_files(manifest, args.bundle)
        last_error: IndexVerificationError | None = None
        report: dict[str, Any] | None = None
        for attempt in range(1, args.attempts + 1):
            try:
                report = verify_index(
                    manifest=manifest,
                    index_json_url=args.index_json_url,
                    mode=args.mode,
                )
                last_error = None
                break
            except IndexVerificationError as error:
                last_error = error
                if attempt < args.attempts:
                    print(
                        f"Package-index verification attempt {attempt} failed: {error}",
                        file=sys.stderr,
                    )
                    time.sleep(args.delay_seconds)
        if last_error is not None or report is None:
            raise last_error or IndexVerificationError("verification did not run")
        if args.report is not None:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(
                json.dumps(report, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
        if args.url_file is not None:
            args.url_file.parent.mkdir(parents=True, exist_ok=True)
            args.url_file.write_text(
                "\n".join(report["download_urls"])
                + ("\n" if report["download_urls"] else ""),
                encoding="utf-8",
            )
    except (IndexVerificationError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(
        f"Verified {report['version']} against {report['index_json_url']} "
        f"in {report['mode']} mode."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
