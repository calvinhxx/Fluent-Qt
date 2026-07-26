#!/usr/bin/env python3

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
PRIVATE_HEADER = re.compile(r"(^|/)(private/|[^/]+_p\.h$)")
UILIB_PREFIXES = ("compatibility/", "components/", "design/", "utils/")

install_manifest = (ROOT / "cmake/FluentQtInstallHeaders.cmake").read_text(
    encoding="utf-8"
)
public_source_headers = {
    entry.removeprefix("src/")
    for entry in re.findall(r"^\s+(src/[^\s)]+\.h)\s*$", install_manifest, re.MULTILINE)
}
if not public_source_headers:
    print("error: could not read the FluentQt installed-header allowlist", file=sys.stderr)
    raise SystemExit(1)

violations: list[str] = []
for path in sorted((ROOT / "app").rglob("*")):
    if path.suffix not in {".h", ".hpp", ".cpp", ".cc"}:
        continue
    for line_number, line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        match = INCLUDE.match(line)
        if not match:
            continue
        include = match.group(1).replace("\\", "/")
        source_style_include = include.removeprefix("src/")
        is_uilib_header = source_style_include.startswith(UILIB_PREFIXES)
        if (
            PRIVATE_HEADER.search(include)
            or include.startswith("src/")
            or (is_uilib_header and source_style_include not in public_source_headers)
        ):
            violations.append(
                f"{path.relative_to(ROOT)}:{line_number}: private UILib include {include!r}"
            )

if violations:
    print(
        "Gallery must validate FluentQt through public component APIs; "
        "private implementation headers are not allowed:",
        file=sys.stderr,
    )
    print("\n".join(violations), file=sys.stderr)
    raise SystemExit(1)

print("Gallery/UILib include boundary is clean.")
