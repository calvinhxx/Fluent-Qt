"""Select the vcpkg dependency archives shared by different FluentQt overlays."""

import hashlib
import os
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET


def dependency_archives(plan: Path, triplet: str) -> list[str]:
    packages = ET.parse(plan).getroot().findall("package")
    port_id = f"fluent-qt_{triplet}"
    if not any(pkg.get("id") == port_id for pkg in packages):
        raise ValueError(f"The vcpkg install plan is missing {port_id}")
    archives = []
    for pkg in packages:
        if pkg.get("id") == port_id:
            continue
        match = re.fullmatch(r".+-vcpkg([0-9a-f]{40}|[0-9a-f]{64})", pkg.attrib["version"])
        if not match:
            raise ValueError(f"Missing vcpkg ABI for {pkg.attrib['id']}")
        abi = match[1]
        archives.append(f".vcpkg-binary-cache/{abi[:2]}/{abi}.zip")
    if not archives:
        raise ValueError("The vcpkg install plan contains no dependencies")
    return sorted(set(archives))


if __name__ == "__main__":
    paths = dependency_archives(Path(sys.argv[1]), os.environ["TRIPLET"])
    entries = "\n".join(paths)
    digest = hashlib.sha256(entries.encode()).hexdigest()
    with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as output:
        print(f"key=port-deps-v1-{os.environ['TRIPLET']}-{digest}", file=output)
        print(f"archives<<END\n{entries}\nEND", file=output)
