#!/usr/bin/env python3
"""Install major-version-matched Clang builtin headers required by Shiboken."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import io
from pathlib import Path, PurePosixPath
import tarfile
import urllib.request


INSTALL_MARKER = Path(".fluentqt-package-sha256")


@dataclass(frozen=True)
class PackageSpec:
    url: str
    sha256: str
    package_prefix: PurePosixPath
    resource_version: str
    required_headers: tuple[str, ...]

    @property
    def include_prefix(self) -> PurePosixPath:
        return (
            self.package_prefix
            / "lib"
            / "clang"
            / self.resource_version
            / "include"
        )

    @property
    def required_paths(self) -> tuple[Path, ...]:
        base = Path("lib") / "clang" / self.resource_version / "include"
        return tuple(base / header for header in self.required_headers)


PACKAGE_SPECS = {
    "10": PackageSpec(
        url=(
            "https://archive.ubuntu.com/ubuntu/pool/universe/l/llvm-toolchain-10/"
            "libclang-common-10-dev_10.0.0-4ubuntu1_amd64.deb"
        ),
        sha256=(
            "6384d8c91362957b88e8080ddbec8ff441e3afb1fed8272bbbd57ba6bcd6ce56"
        ),
        package_prefix=PurePosixPath("usr/lib/llvm-10"),
        resource_version="10.0.0",
        required_headers=("limits.h", "stddef.h", "stdint.h", "vadefs.h"),
    ),
    # Shiboken 6.9.3 embeds libclang 19.1.0 and resolves builtin headers from
    # lib/clang/19. Debian's patched 19.1 release keeps that resource contract.
    "19": PackageSpec(
        url=(
            "https://deb.debian.org/debian/pool/main/l/llvm-toolchain-19/"
            "libclang-common-19-dev_19.1.7-3~deb12u1_amd64.deb"
        ),
        sha256=(
            "811c2d398ad772204ec05ffde485cbbc4c210abc5dcf0068ff9ca3e647591ce7"
        ),
        package_prefix=PurePosixPath("usr/lib/llvm-19"),
        resource_version="19",
        required_headers=(
            "arm_neon.h",
            "arm_vector_types.h",
            "limits.h",
            "stddef.h",
            "stdint.h",
            "vadefs.h",
        ),
    ),
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Install architecture-neutral Clang builtin headers matched to the "
            "selected Shiboken generator."
        )
    )
    parser.add_argument(
        "--clang-major",
        choices=sorted(PACKAGE_SPECS),
        default="10",
        help="Clang resource-header major version (default: 10).",
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


def package_bytes(spec: PackageSpec, archive_path: Path | None) -> bytes:
    if archive_path is not None:
        return archive_path.read_bytes()

    request = urllib.request.Request(
        spec.url,
        headers={"User-Agent": "FluentQt-CI/1.0"},
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read()


def verify_package(spec: PackageSpec, package: bytes) -> None:
    actual_hash = hashlib.sha256(package).hexdigest()
    if actual_hash != spec.sha256:
        raise RuntimeError(
            "Unexpected Clang header package SHA-256: "
            f"expected {spec.sha256}, got {actual_hash}."
        )


def normalized_member_path(member_name: str) -> PurePosixPath:
    normalized = member_name.removeprefix("./")
    path = PurePosixPath(normalized)
    if path.is_absolute() or ".." in path.parts:
        raise RuntimeError(f"Unsafe path in Clang header package: {member_name}")
    return path


def extract_builtin_headers(spec: PackageSpec, package: bytes, output: Path) -> None:
    data_archive = read_ar_member(package, "data.tar.xz")
    output_root = output.resolve()

    extracted_files = 0
    with tarfile.open(fileobj=io.BytesIO(data_archive), mode="r:xz") as archive:
        for member in archive:
            member_path = normalized_member_path(member.name)
            if not member.isfile() or not member_path.is_relative_to(
                spec.include_prefix
            ):
                continue

            relative_path = member_path.relative_to(spec.package_prefix)
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
    (output / INSTALL_MARKER).write_text(f"{spec.sha256}\n", encoding="utf-8")


def validate_installation(spec: PackageSpec, output: Path) -> None:
    marker = output / INSTALL_MARKER
    if (
        not marker.is_file()
        or marker.read_text(encoding="utf-8").strip() != spec.sha256
    ):
        raise RuntimeError(
            "The Clang builtin header installation is incomplete or stale."
        )
    missing = [
        str(path) for path in spec.required_paths if not (output / path).is_file()
    ]
    if missing:
        raise RuntimeError(f"Missing Clang builtin headers: {', '.join(missing)}")


def main() -> int:
    args = parse_args()
    spec = PACKAGE_SPECS[args.clang_major]
    output = args.output.expanduser().resolve()
    output.mkdir(parents=True, exist_ok=True)

    try:
        validate_installation(spec, output)
    except RuntimeError:
        package = package_bytes(spec, args.archive)
        verify_package(spec, package)
        extract_builtin_headers(spec, package, output)
        validate_installation(spec, output)

    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
