"""Build the standalone, pure-Python FluentQt Gallery wheel.

The Gallery is a separate distribution which depends on the exact FluentQt
UILib version.  Keeping this builder independent from the native wheel makes
it impossible for Gallery code or assets to leak into the core package.
"""

import argparse
import base64
import csv
import hashlib
import io
from pathlib import Path
import sys
import zipfile


PACKAGE_NAME = "fluentqt_gallery"
DISTRIBUTION_NAME = "FluentQt-Gallery"
DIST_INFO_NAME = "fluentqt_gallery-{version}.dist-info"
WHEEL_TAG = "py3-none-any"
SUPPORTED_REQUIRES_PYTHON = {
    ">=3.10,<3.11",
    ">=3.11,<3.14",
}
REQUIRED_PACKAGE_FILES = {
    "__init__.py",
    "__main__.py",
    "application_controller.py",
    "app.py",
    "catalog.py",
    "contract.json",
    "foundation_pages.py",
    "identity.py",
    "intro_tour.py",
    "metrics.py",
    "native_samples.py",
    "native_samples_basic.py",
    "native_samples_collections.py",
    "native_samples_dialogs.py",
    "native_samples_navigation.py",
    "native_samples_scrolling.py",
    "native_samples_status.py",
    "native_samples_text_window.py",
    "samples.py",
    "settings.py",
    "single_instance.py",
    "update_checker.py",
    "visual.py",
    "window.py",
    "window_placement.py",
    "assets/app-icon.png",
    "assets/icon_aliases.json",
    "assets/icon_catalog.json",
    "assets/control_images/Placeholder.png",
    "assets/home_header_tiles/Header-WindowsDesign.png",
}


def validate_requires_python(value):
    if value not in SUPPORTED_REQUIRES_PYTHON:
        raise RuntimeError(
            "Unsupported Requires-Python policy: {0}".format(value)
        )
    return value


def package_files(package_dir):
    missing = sorted(
        name for name in REQUIRED_PACKAGE_FILES
        if not (package_dir / name).is_file()
    )
    if missing:
        raise RuntimeError(
            "Staged Gallery package is missing required files: {0}".format(
                ", ".join(missing)
            )
        )

    files = {}
    native_suffixes = {".so", ".pyd", ".dylib"}
    for source in sorted(package_dir.rglob("*")):
        if not source.is_file():
            continue
        relative = source.relative_to(package_dir)
        if "__pycache__" in relative.parts or source.suffix in {".pyc", ".pyo"}:
            continue
        if source.suffix.lower() in native_suffixes:
            raise RuntimeError(
                "The standalone Gallery wheel must remain pure Python: {0}".format(
                    source
                )
            )
        archive_path = Path(PACKAGE_NAME) / relative
        files[archive_path.as_posix()] = source.read_bytes()
    return files


def sha256_record(data):
    digest = hashlib.sha256(data).digest()
    encoded = base64.urlsafe_b64encode(digest).rstrip(b"=").decode("ascii")
    return "sha256={0}".format(encoded)


def record_contents(files, record_path):
    output = io.StringIO()
    writer = csv.writer(output, lineterminator="\n")
    for archive_path in sorted(files):
        data = files[archive_path]
        writer.writerow((archive_path, sha256_record(data), len(data)))
    writer.writerow((record_path, "", ""))
    return output.getvalue().encode("utf-8")


def zip_info(archive_path):
    info = zipfile.ZipInfo(archive_path, (1980, 1, 1, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED
    info.create_system = 3
    info.external_attr = 0o100644 << 16
    return info


def build_wheel(args):
    package_dir = Path(args.package_dir).resolve()
    output_dir = Path(args.output_dir).resolve()
    if not package_dir.is_dir():
        raise RuntimeError(
            "Gallery package directory does not exist: {0}".format(package_dir)
        )

    files = package_files(package_dir)
    dist_info = DIST_INFO_NAME.format(version=args.version)
    requires_python = validate_requires_python(args.requires_python)
    metadata = (
        "Metadata-Version: 2.1\n"
        "Name: {distribution}\n"
        "Version: {version}\n"
        "Summary: Standalone Gallery for the FluentQt PySide6 widget library\n"
        "License: MIT\n"
        "Requires-Python: {requires_python}\n"
        "Requires-Dist: FluentQt (=={version})\n"
        "\n"
    ).format(
        distribution=DISTRIBUTION_NAME,
        version=args.version,
        requires_python=requires_python,
    )
    wheel_metadata = (
        "Wheel-Version: 1.0\n"
        "Generator: FluentQt standalone Gallery wheel builder\n"
        "Root-Is-Purelib: true\n"
        "Tag: {0}\n"
        "\n"
    ).format(WHEEL_TAG)
    files["{0}/METADATA".format(dist_info)] = metadata.encode("utf-8")
    files["{0}/WHEEL".format(dist_info)] = wheel_metadata.encode("utf-8")

    for license_name in args.license_file:
        license_path = Path(license_name).resolve()
        if not license_path.is_file():
            raise RuntimeError(
                "License file does not exist: {0}".format(license_path)
            )
        archive_path = "{0}/licenses/{1}".format(dist_info, license_path.name)
        files[archive_path] = license_path.read_bytes()

    record_path = "{0}/RECORD".format(dist_info)
    files[record_path] = record_contents(files, record_path)

    output_dir.mkdir(parents=True, exist_ok=True)
    wheel_path = output_dir / "fluentqt_gallery-{0}-{1}.whl".format(
        args.version,
        WHEEL_TAG,
    )
    if wheel_path.exists():
        wheel_path.unlink()
    with zipfile.ZipFile(str(wheel_path), "w") as archive:
        for archive_path in sorted(files):
            archive.writestr(zip_info(archive_path), files[archive_path])
    with zipfile.ZipFile(str(wheel_path), "r") as archive:
        bad_member = archive.testzip()
        if bad_member:
            raise RuntimeError(
                "Gallery wheel CRC validation failed for {0}".format(bad_member)
            )
    print(str(wheel_path))


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--requires-python", required=True)
    parser.add_argument("--license-file", action="append", default=[])
    return parser.parse_args()


if __name__ == "__main__":
    try:
        build_wheel(parse_args())
    except Exception as error:
        sys.stderr.write("FluentQt Gallery wheel build failed: {0}\n".format(error))
        sys.exit(1)
