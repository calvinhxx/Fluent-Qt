#!/usr/bin/env python3

"""Generate the searchable website index for FluentQt's installed public API."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
INSTALL_HEADERS = ROOT / "cmake" / "FluentQtInstallHeaders.cmake"
AI_CATALOG = ROOT / "docs" / "ai" / "generated" / "fluentqt-ai-catalog.json"
OUTPUT = ROOT / "site" / "api" / "catalog.json"
REPOSITORY_BLOB = "https://github.com/calvinhxx/Fluent-Qt/blob/main/"
GALLERY_BASE = "https://calvinhxx.github.io/Fluent-Qt/gallery/"
INSTALL_BLOCK = re.compile(
    r"set\(FLUENT_QT_INSTALL_HEADERS\s*(.*?)\n\)", re.DOTALL
)
BRIEF = re.compile(r"@brief\s+([^\n*]+)")
TYPE_DECLARATION = re.compile(
    r"^\s*(?:class|struct)\s+(?:[A-Z][A-Z0-9_]*\s+)?([A-Za-z_]\w*)\b",
    re.MULTILINE,
)
ENUM_DECLARATION = re.compile(
    r"^\s*enum(?:\s+class)?\s+([A-Za-z_]\w*)\b", re.MULTILINE
)


def _installed_header_paths() -> list[str]:
    source = INSTALL_HEADERS.read_text(encoding="utf-8")
    match = INSTALL_BLOCK.search(source)
    if match is None:
        raise ValueError("Could not read FLUENT_QT_INSTALL_HEADERS")
    paths = [
        line.split("#", 1)[0].strip()
        for line in match.group(1).splitlines()
        if line.split("#", 1)[0].strip()
    ]
    if len(paths) != len(set(paths)):
        raise ValueError("FLUENT_QT_INSTALL_HEADERS contains duplicate entries")
    missing = [path for path in paths if not (ROOT / path).is_file()]
    if missing:
        raise ValueError("Installed public headers are missing: " + ", ".join(missing))
    return paths


def _installed_include(source_path: str) -> str:
    path = Path(source_path)
    if path.parts[0] == "include":
        relative = Path(*path.parts[1:])
    elif path.parts[0] == "src":
        relative = Path("FluentQt", *path.parts[1:])
    else:
        raise ValueError(f"Unsupported installed header path: {source_path}")
    return f"<{relative.as_posix()}>"


def _header_summary(contents: str, stem: str) -> str:
    match = BRIEF.search(contents)
    if match is not None:
        return " ".join(match.group(1).strip().split())
    return f"Public declarations provided by {stem}."


def _declarations(contents: str) -> list[str]:
    values = TYPE_DECLARATION.findall(contents) + ENUM_DECLARATION.findall(contents)
    return list(dict.fromkeys(value for value in values if value not in {"final"}))


def _public_headers(paths: list[str]) -> list[dict[str, object]]:
    headers: list[dict[str, object]] = []
    for source_path in paths:
        path = ROOT / source_path
        contents = path.read_text(encoding="utf-8")
        headers.append(
            {
                "include": _installed_include(source_path),
                "source": source_path,
                "summary": _header_summary(contents, path.stem),
                "declarations": _declarations(contents),
                "source_url": REPOSITORY_BLOB + source_path,
            }
        )
    return headers


def _component_records(
    catalog: dict[str, object], installed_paths: set[str]
) -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    for raw in catalog["components"]:
        component = dict(raw)
        cpp = component["cpp"]
        gallery = component["gallery"]
        tests = component["tests"]
        declaration = cpp["declaration"]
        if declaration not in installed_paths:
            raise ValueError(
                f"Catalog component {component['id']} is not in the installed-header allowlist"
            )
        if gallery["route_id"] != component["id"]:
            raise ValueError(f"Catalog route drift for {component['id']}")
        if not tests:
            raise ValueError(f"Catalog component {component['id']} has no focused test")
        for test in tests:
            if not (ROOT / test["source"]).is_file():
                raise ValueError(
                    f"Catalog component {component['id']} references a missing test"
                )
        records.append(
            {
                "id": component["id"],
                "title": component["title"],
                "category_id": component["category_id"],
                "description": component["description"],
                "capabilities": component["capabilities"],
                "cpp": {
                    "public_header": cpp["public_header"],
                    "installed_declaration_header": _installed_include(declaration),
                    "qualified_type": cpp["qualified_type"],
                    "target": cpp["cmake_target"],
                    "declaration_url": REPOSITORY_BLOB + declaration,
                },
                "python": component["python"],
                "gallery": {
                    "route_id": gallery["route_id"],
                    "url": GALLERY_BASE + "?route=" + gallery["route_id"],
                    "sample_source_url": REPOSITORY_BLOB + gallery["sample_source"],
                },
                "tests": [
                    {
                        "target": test["target"],
                        "ctest_label": test["ctest_label"],
                        "source_url": REPOSITORY_BLOB + test["source"],
                    }
                    for test in tests
                ],
                "sample_count": len(component["samples"]),
            }
        )
    return records


def generate_reference() -> dict[str, object]:
    catalog = json.loads(AI_CATALOG.read_text(encoding="utf-8"))
    if catalog.get("schema_version") != 2:
        raise ValueError("AI catalog schema_version must be 2")
    paths = _installed_header_paths()
    components = _component_records(catalog, set(paths))
    categories = [
        {
            "id": category["id"],
            "title": category["title"],
            "component_count": len(category["components"]),
        }
        for category in catalog["categories"]
        if any(component["category_id"] == category["id"] for component in components)
    ]
    return {
        "schema_version": 1,
        "project": catalog["project"],
        "generated_from": [
            "cmake/FluentQtInstallHeaders.cmake",
            "docs/ai/generated/fluentqt-ai-catalog.json",
        ],
        "summary": {
            "components": len(components),
            "public_headers": len(paths),
            "categories": len(categories),
        },
        "categories": categories,
        "components": components,
        "public_headers": _public_headers(paths),
    }


def _encoded(reference: dict[str, object]) -> str:
    return json.dumps(reference, ensure_ascii=False, indent=2) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Fail when the committed website index is stale",
    )
    arguments = parser.parse_args()
    expected = _encoded(generate_reference())
    current = OUTPUT.read_text(encoding="utf-8") if OUTPUT.is_file() else None
    digest = hashlib.sha256(expected.encode("utf-8")).hexdigest()[:12]
    if current == expected:
        print(f"ok {OUTPUT.relative_to(ROOT)} {digest}")
        return 0
    if arguments.check:
        print(f"stale {OUTPUT.relative_to(ROOT)}")
        return 1
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(expected, encoding="utf-8")
    print(f"wrote {OUTPUT.relative_to(ROOT)} {digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
