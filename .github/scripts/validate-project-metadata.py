#!/usr/bin/env python3

import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def fail(message: str) -> None:
    print(f"error: {message}", file=sys.stderr)
    raise SystemExit(1)


cmake_text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
project_match = re.search(
    r"project\s*\(\s*FluentQt\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)",
    cmake_text,
)
if not project_match:
    fail("could not read FluentQt's project version from CMakeLists.txt")
project_version = project_match.group(1)

manifest = json.loads((ROOT / "vcpkg.json").read_text(encoding="utf-8"))
manifest_version = manifest.get("version-string")
if manifest_version != project_version:
    fail(
        "vcpkg.json version-string "
        f"{manifest_version!r} does not match CMake project version {project_version!r}"
    )

expected_tag = f"v{project_version}"
for relative_path in ("README.md", "README.zh-CN.md", "site/index.html"):
    text = (ROOT / relative_path).read_text(encoding="utf-8")
    tags = re.findall(r"GIT_TAG(?:\s|<[^>]+>)+([^<\s]+)", text)
    if tags != [expected_tag]:
        fail(
            f"{relative_path} must contain exactly one FetchContent GIT_TAG "
            f"{expected_tag!r}; found {tags!r}"
        )

print(f"Project metadata is aligned at {project_version}.")
