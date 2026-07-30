#!/usr/bin/env python3
"""Install the Clang 10 builtin headers required by Shiboken 6.2."""

from __future__ import annotations

import argparse
import hashlib
import io
from pathlib import Path, PurePosixPath
import tarfile
import urllib.request


PACKAGE_URL = (
    "https://archive.ubuntu.com/ubuntu/pool/universe/l/llvm-toolchain-10/"
    "libclang-common-10-dev_10.0.0-4ubuntu1_amd64.deb"
)
PACKAGE_SHA256 = "6384d8c91362957b88e8080ddbec8ff441e3afb1fed8272bbbd57ba6bcd6ce56"
PACKAGE_PREFIX = PurePosixPath("usr/lib/llvm-10")
INSTALL_MARKER = Path(".fluentqt-package-sha256")
REQUIRED_HEADERS = (
    Path("lib/clang/10.0.0/include/limits.h"),
    Path("lib/clang/10.0.0/include/stddef.h"),
    Path("lib/clang/10.0.0/include/stdint.h"),
    Path("lib/clang/10.0.0/include/vadefs.h"),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Install the architecture-neutral Clang 10 builtin headers used by "
            "the PySide6 6.2 Shiboken generator."
        )
    )
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="Directory that will become CLANG_INSTALL_DIR.",
    )
    parser.add_argument(
        "--archive",
        type=Path,
        help="Use an existing Debian archive instead of downloading it.",
    )
    return parser.parse_args()


def read_ar_member(archive: bytes, wanted_name: str) -> bytes:
    if not archive.startswith(b"!<arch>\n"):
        raise RuntimeError("The Clang header package is not a Debian/ar archive.")

    offset = 8
    while offset + 60 <= len(archive):
        header = archive[offset : offset + 60]
        if header[58:60] != b"`\n":
            raise RuntimeError("The Clang header package contains an invalid ar header.")

        name = header[:16].decode("ascii").strip().removesuffix("/")
        try:
            size = int(header[48:58].decode("ascii").strip())
        except ValueError as error:
            raise RuntimeError(
                "The Clang header package contains an invalid member size."
            ) from error

        data_start = offset + 60
        data_end = data_start + size
        if data_end > len(archive):
            raise RuntimeError("The Clang header package contains a truncated member.")
        if name == wanted_name:
            return archive[data_start:data_end]

        offset = data_end + (size % 2)

    raise RuntimeError(f"The Clang header package does not contain {wanted_name}.")


def package_bytes(archive_path: Path | None) -> bytes:
    if archive_path is not None:
        return archive_path.read_bytes()

    request = urllib.request.Request(
        PACKAGE_URL,
        headers={"User-Agent": "FluentQt-CI/1.0"},
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read()


def verify_package(package: bytes) -> None:
    actual_hash = hashlib.sha256(package).hexdigest()
    if actual_hash != PACKAGE_SHA256:
        raise RuntimeError(
            "Unexpected Clang header package SHA-256: "
            f"expected {PACKAGE_SHA256}, got {actual_hash}."
        )


def normalized_member_path(member_name: str) -> PurePosixPath:
    normalized = member_name.removeprefix("./")
    path = PurePosixPath(normalized)
    if path.is_absolute() or ".." in path.parts:
        raise RuntimeError(f"Unsafe path in Clang header package: {member_name}")
    return path


def extract_builtin_headers(package: bytes, output: Path) -> None:
    data_archive = read_ar_member(package, "data.tar.xz")
    include_prefix = PACKAGE_PREFIX / "lib/clang/10.0.0/include"
    output_root = output.resolve()

    extracted_files = 0
    with tarfile.open(fileobj=io.BytesIO(data_archive), mode="r:xz") as archive:
        for member in archive:
            member_path = normalized_member_path(member.name)
            if not member.isfile() or not member_path.is_relative_to(include_prefix):
                continue

            relative_path = member_path.relative_to(PACKAGE_PREFIX)
            destination = (output_root / Path(*relative_path.parts)).resolve()
            if output_root not in destination.parents:
                raise RuntimeError(f"Unsafe extraction target: {destination}")

            source = archive.extractfile(member)
            if source is None:
                raise RuntimeError(f"Unable to read {member.name} from the package.")
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(source.read())
            extracted_files += 1

    if extracted_files == 0:
        raise RuntimeError("No Clang builtin headers were extracted.")
    (output / INSTALL_MARKER).write_text(f"{PACKAGE_SHA256}\n", encoding="utf-8")


def validate_installation(output: Path) -> None:
    marker = output / INSTALL_MARKER
    if not marker.is_file() or marker.read_text(encoding="utf-8").strip() != PACKAGE_SHA256:
        raise RuntimeError("The Clang builtin header installation is incomplete or stale.")
    missing = [str(path) for path in REQUIRED_HEADERS if not (output / path).is_file()]
    if missing:
        raise RuntimeError(f"Missing Clang builtin headers: {', '.join(missing)}")


def main() -> int:
    args = parse_args()
    output = args.output.expanduser().resolve()
    output.mkdir(parents=True, exist_ok=True)

    try:
        validate_installation(output)
    except RuntimeError:
        package = package_bytes(args.archive)
        verify_package(package)
        extract_builtin_headers(package, output)
        validate_installation(output)

    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
