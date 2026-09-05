#!/usr/bin/env python3
"""Check Gallery/public-header and UILib/platform dependency boundaries."""

from __future__ import annotations

import posixpath
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
PRIVATE_HEADER = re.compile(r"(^|/)(private/|[^/]+_p\.h$)")
UILIB_PREFIXES = ("compatibility/", "components/", "design/", "utils/")
SOURCE_SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".mm"}


def source_include(root: Path, path: Path, include: str) -> str:
    """Normalize installed, source-root, and relative UILib include spellings."""
    normalized = posixpath.normpath(include.replace("\\", "/"))
    canonical = normalized.removeprefix("FluentQt/").removeprefix("src/")
    if normalized.startswith(("FluentQt/", "src/")) or canonical.startswith(
        UILIB_PREFIXES + ("app/", "support/", "spdlog/", "fmt/")
    ):
        return canonical
    try:
        relative = (path.parent / normalized).resolve().relative_to(root.resolve())
    except ValueError:
        return canonical
    return relative.as_posix().removeprefix("src/")


def validate(root: Path) -> list[str]:
    manifest = (root / "cmake/FluentQtInstallHeaders.cmake").read_text(encoding="utf-8")
    public_headers = {
        entry.removeprefix("src/")
        for entry in re.findall(r"^\s+(src/[^\s)]+\.h)\s*$", manifest, re.MULTILINE)
    }
    if not public_headers:
        return ["could not read the FluentQt installed-header allowlist"]

    violations: list[str] = []
    for directory in ("app", "src"):
        for path in sorted((root / directory).rglob("*")):
            if path.suffix not in SOURCE_SUFFIXES or not path.is_file():
                continue
            relative = path.relative_to(root).as_posix()
            for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                match = INCLUDE.match(line)
                if not match:
                    continue
                include = match.group(1).replace("\\", "/")
                source = source_include(root, path, include)
                reason = ""
                if directory == "app":
                    if (
                        PRIVATE_HEADER.search(include)
                        or PRIVATE_HEADER.search(source)
                        or include.startswith("src/")
                        or (source.startswith(UILIB_PREFIXES) and source not in public_headers)
                    ):
                        reason = "Gallery must use installed public UILib headers"
                elif source.startswith(("app/", "support/logging/", "spdlog/", "fmt/")):
                    reason = "UILib must not depend on Gallery or application logging"
                elif relative.startswith("src/compatibility/") and (
                    source.startswith("components/")
                    or (include.startswith("FluentQt/") and not source.startswith("compatibility/"))
                ):
                    reason = "platform compatibility must not depend on component headers"
                if reason:
                    violations.append(f"{relative}:{number}: {reason}: {include!r}")
    return violations


def main() -> int:
    try:
        violations = validate(ROOT)
    except OSError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    if violations:
        print("\n".join(violations), file=sys.stderr)
        return 1
    print("Gallery, UILib, and platform include boundaries are clean.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
