#!/usr/bin/env python3
"""Validate Gallery artwork sources and Qt resource aliases without Qt."""

from __future__ import annotations

from pathlib import Path
import sys
import xml.etree.ElementTree as ET

from export_control_images import ROOT, validate_source


def validate(project_root: Path) -> list[str]:
    source_root = project_root / "tools/gallery/artwork"
    qrc_path = project_root / "app/gallery_resources.qrc"
    failures = []
    sources = sorted(source_root.rglob("*.svg"))
    if not sources:
        return [f"{source_root}: no SVG masters found"]

    expected = {}
    for source in sources:
        try:
            validate_source(source)
        except (ValueError, ET.ParseError) as error:
            failures.append(str(error))
        expected[source.resolve()] = "assets/control_images/" + source.relative_to(source_root).as_posix()

    try:
        tree = ET.parse(qrc_path)
    except (OSError, ET.ParseError) as error:
        return failures + [str(error)]
    registered = set()
    aliases = set()
    for resource in tree.findall("qresource"):
        for node in resource.findall("file"):
            if not node.text:
                continue
            source = (qrc_path.parent / node.text).resolve()
            alias = node.get("alias", node.text)
            if not (source.is_relative_to(source_root.resolve())
                    or alias.startswith("assets/control_images/")):
                continue
            if source not in expected:
                failures.append(f"{node.text}: resource has no matching SVG master")
                continue
            if resource.get("prefix") != "/app" or alias != expected[source]:
                failures.append(f"{node.text}: expected /app/{expected[source]} resource alias")
            if source in registered or alias in aliases:
                failures.append(f"{node.text}: duplicate artwork resource")
            registered.add(source)
            aliases.add(alias)
    for source in sorted(expected.keys() - registered):
        failures.append(f"{source}: missing from app/gallery_resources.qrc")
    return failures


def main() -> int:
    failures = validate(ROOT)
    if failures:
        print("Gallery artwork validation failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1
    print("Gallery SVG masters and resource aliases are consistent.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
