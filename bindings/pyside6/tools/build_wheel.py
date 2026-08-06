"""Build a binary wheel from a staged FluentQt PySide6 package.

The script intentionally uses only the Python standard library so the Qt 6.2
baseline does not depend on a newer Python packaging backend.
"""

import argparse
import base64
import csv
import hashlib
import io
from pathlib import Path
import re
import subprocess
import sys
import sysconfig
import zipfile


PACKAGE_NAME = "fluentqt"
DIST_INFO_NAME = "fluentqt-{version}.dist-info"
REQUIRED_PACKAGE_FILES = {
    "__init__.py",
    "__init__.pyi",
    "_fluentqt.pyi",
    "basicinput.py",
    "basicinput.pyi",
    "collections.pyi",
    "date_time.pyi",
    "design.py",
    "design.pyi",
    "dialogs_flyouts.pyi",
    "foundation.py",
    "foundation.pyi",
    "layout.pyi",
    "menus_toolbars.py",
    "menus_toolbars.pyi",
    "navigation.pyi",
    "py.typed",
    "scrolling.py",
    "scrolling.pyi",
    "status_info.py",
    "status_info.pyi",
    "textfields.py",
    "textfields.pyi",
    "windowing.py",
    "windowing.pyi",
    "_icon_aliases.json",
}


def pyside_runtime_requirement(version):
    match = re.match(r"^(\d+)\.(\d+)(?:[.\-+]|$)", version)
    if not match:
        raise RuntimeError("Invalid PySide6 version: {0}".format(version))

    major_minor = (int(match.group(1)), int(match.group(2)))
    distribution = (
        "PySide6"
        if major_minor < (6, 3)
        else "PySide6-Essentials"
    )
    return "{0} (=={1})".format(distribution, version)


def normalized_platform_tag(extension):
    platform_tag = sysconfig.get_platform()
    if sys.platform == "darwin":
        match = re.match(
            r"^macosx-(\d+)(?:\.(\d+))?-(.+)$",
            platform_tag,
        )
        if match:
            declared = (int(match.group(1)), int(match.group(2) or 0))
            actual = macos_deployment_target(extension)
            # The extension uses Python's dynamic symbol lookup on macOS, so
            # its Mach-O deployment target is the wheel's real OS floor. The
            # build interpreter may itself have been compiled on a newer host.
            minimum = actual or declared
            architecture = macos_architecture(extension) or match.group(3)
            platform_tag = "macosx-{0}.{1}-{2}".format(
                minimum[0],
                minimum[1],
                architecture,
            )
    return platform_tag.replace("-", "_").replace(".", "_")


def macos_deployment_target(extension):
    try:
        output = subprocess.check_output(
            ["otool", "-l", str(extension)],
            universal_newlines=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None

    versions = []
    for value in re.findall(r"^\s*minos\s+(\d+(?:\.\d+)*)", output, re.M):
        parts = [int(part) for part in value.split(".")]
        versions.append((parts[0], parts[1] if len(parts) > 1 else 0))
    return max(versions) if versions else None


def macos_architecture(extension):
    try:
        output = subprocess.check_output(
            ["lipo", "-archs", str(extension)],
            universal_newlines=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None

    architectures = set(output.split())
    if {"arm64", "x86_64"}.issubset(architectures):
        return "universal2"
    if len(architectures) == 1:
        return architectures.pop()
    return None


def python_wheel_tag(extension):
    if sys.implementation.name != "cpython":
        raise RuntimeError("FluentQt wheels currently support CPython only")
    python_tag = "cp{0}{1}".format(sys.version_info[0], sys.version_info[1])
    return "{0}-{0}-{1}".format(
        python_tag,
        normalized_platform_tag(extension),
    )


def package_files(package_dir):
    files = {}
    extension_candidates = []
    for source in sorted(package_dir.rglob("*")):
        if not source.is_file():
            continue
        relative = source.relative_to(package_dir)
        if "__pycache__" in relative.parts or source.suffix in {".pyc", ".pyo"}:
            continue
        if relative.parts and relative.parts[0] == "gallery":
            raise RuntimeError(
                "The reusable FluentQt wheel must not contain Gallery files: "
                "{0}".format(source)
            )
        archive_path = Path(PACKAGE_NAME) / relative
        files[archive_path.as_posix()] = source.read_bytes()
        if (
            source.name.startswith("_fluentqt")
            and source.suffix.lower() in {".so", ".pyd", ".dylib"}
        ):
            extension_candidates.append(source)

    missing = sorted(
        name for name in REQUIRED_PACKAGE_FILES
        if not (package_dir / name).is_file()
    )
    if missing:
        raise RuntimeError(
            "Staged package is missing required files: {0}".format(
                ", ".join(missing)
            )
        )
    if len(extension_candidates) != 1:
        raise RuntimeError(
            "Expected one _fluentqt extension, found {0}".format(
                len(extension_candidates)
            )
        )
    return files, extension_candidates[0]


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
    executable = archive_path.startswith("{0}/_fluentqt".format(PACKAGE_NAME))
    info.external_attr = (0o100755 if executable else 0o100644) << 16
    return info


def build_wheel(args):
    package_dir = Path(args.package_dir).resolve()
    output_dir = Path(args.output_dir).resolve()
    if not package_dir.is_dir():
        raise RuntimeError(
            "Package directory does not exist: {0}".format(package_dir)
        )

    files, extension = package_files(package_dir)
    tag = python_wheel_tag(extension)
    dist_info = DIST_INFO_NAME.format(version=args.version)
    pyside_requirement = pyside_runtime_requirement(args.pyside_version)

    metadata = (
        "Metadata-Version: 2.1\n"
        "Name: FluentQt\n"
        "Version: {version}\n"
        "Summary: PySide6 bindings for the FluentQt widget library\n"
        "License: MIT\n"
        "Requires-Python: >=3.10\n"
        "Requires-Dist: {pyside_requirement}\n"
        "Requires-Dist: shiboken6 (=={shiboken_version})\n"
        "\n"
    ).format(
        version=args.version,
        pyside_requirement=pyside_requirement,
        shiboken_version=args.shiboken_version,
    )
    wheel_metadata = (
        "Wheel-Version: 1.0\n"
        "Generator: FluentQt CMake wheel builder\n"
        "Root-Is-Purelib: false\n"
        "Tag: {0}\n"
        "\n"
    ).format(tag)
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
    wheel_path = output_dir / "fluentqt-{0}-{1}.whl".format(args.version, tag)
    if wheel_path.exists():
        wheel_path.unlink()

    with zipfile.ZipFile(str(wheel_path), "w") as archive:
        for archive_path in sorted(files):
            archive.writestr(zip_info(archive_path), files[archive_path])

    with zipfile.ZipFile(str(wheel_path), "r") as archive:
        bad_member = archive.testzip()
        if bad_member:
            raise RuntimeError("Wheel CRC validation failed for {0}".format(bad_member))

    print(str(wheel_path))


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--pyside-version", required=True)
    parser.add_argument("--shiboken-version", required=True)
    parser.add_argument("--license-file", action="append", default=[])
    return parser.parse_args()


if __name__ == "__main__":
    try:
        build_wheel(parse_args())
    except Exception as error:
        sys.stderr.write("FluentQt wheel build failed: {0}\n".format(error))
        sys.exit(1)
