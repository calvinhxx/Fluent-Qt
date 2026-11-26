"""Package the released Widgets port using the same tag archive as vcpkg_from_github."""

import argparse
import hashlib
import json
from pathlib import Path
import re
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo


PORT = Path(__file__).resolve().parents[2] / "ports/fluent-qt"
STABLE_TAG = re.compile(r"v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)")
# A port may also pin checksums for downloaded backports.
SOURCE_HASH = re.compile(r"(?m)^(vcpkg_from_github\([^)]*?^[ \t]*SHA512[ \t]+)([0-9a-fA-F]{128})[ \t]*$")


def package(tag, tarball, output):
    if not STABLE_TAG.fullmatch(tag):
        raise ValueError(f"Expected a stable vX.Y.Z tag, got {tag!r}")
    digest = hashlib.sha512()
    with tarball.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    sha = digest.hexdigest()
    manifest = json.loads((PORT / "vcpkg.json").read_bytes())
    recipe = (PORT / "portfile.cmake").read_text(encoding="utf-8")
    hashes = SOURCE_HASH.findall(recipe)
    if len(hashes) != 1 or 'REF "v${VERSION}"' not in recipe:
        raise ValueError("Expected one SHA512 and a versioned GitHub tag in the portfile")
    version = tag[1:]
    if manifest["version-semver"] == version and hashes[0][1].lower() != sha:
        raise ValueError(f"The archive for {tag} differs from the checked-in SHA512")
    manifest["version-semver"] = version
    manifest.pop("port-version", None)
    replacements = {
        "vcpkg.json": (json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode(),
        "portfile.cmake": SOURCE_HASH.sub(lambda m: m[1] + sha, recipe).encode(),
    }
    with ZipFile(output, "w") as archive:
        for path in sorted(PORT.rglob("*")):
            if path.is_file():
                name = path.relative_to(PORT).as_posix()
                info = ZipInfo(f"fluent-qt/{name}")
                info.create_system = 3
                info.external_attr = 0o100644 << 16
                archive.writestr(info, replacements.get(name, path.read_bytes()), ZIP_DEFLATED)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tag")
    parser.add_argument("tarball", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    package(args.tag, args.tarball, args.output)
