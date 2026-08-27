#!/usr/bin/env python3
"""Materialize one Gallery catalog sample as an editable Live Scene file."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import sys
from textwrap import indent
from typing import Iterable


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--route", required=True)
    parser.add_argument("--sample", required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args(list(argv) if argv is not None else None)


def _atomic_write_text(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(".{0}.{1}.tmp".format(path.name, os.getpid()))
    try:
        temporary.write_text(value, encoding="utf-8")
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def _root_name(result: object) -> str:
    widget = result.widget
    namespace = getattr(widget, "_fluentqt_gallery_source_namespace", {})
    names = sorted(
        name
        for name, value in namespace.items()
        if name != "gallery_parent" and value is widget
    )
    if len(names) != 1:
        raise RuntimeError(
            "Catalog sample {0}/{1} exposes {2} root names; expected one".format(
                result.route_id, result.sample_id, len(names)
            )
        )
    return names[0]


def scene_source(
    *,
    route: str,
    sample: str,
    title: str,
    preview_source: str,
    root_name: str,
) -> str:
    body = preview_source.rstrip() + "\n"
    return (
        '"""Live Scene generated from the FluentQt Gallery catalog."""\n\n'
        + "SCENE_TITLE = {0!r}\n".format(title)
        + "CATALOG_ROUTE = {0!r}\n".format(route)
        + "CATALOG_SAMPLE = {0!r}\n".format(sample)
        + "gallery_parent = None\n\n\n"
        + "def build(parent):\n"
        + "    # The source below is the exact PySide6 sample implementation.\n"
        + "    # Add stable objectName values to preserve interaction state on save.\n"
        + "    global gallery_parent\n"
        + "    gallery_parent = parent\n"
        + "    try:\n"
        + indent(body, "        ")
        + "        return {0}\n".format(root_name)
        + "    finally:\n"
        + "        gallery_parent = None\n"
    )


def materialize(args: argparse.Namespace) -> Path:
    output = args.output.expanduser().resolve()

    import fluentqt

    fluentqt.prepare_high_dpi_application()
    from PySide6.QtWidgets import QApplication, QWidget

    from fluentqt_gallery.catalog import ENTRY_BY_ROUTE_ID
    from fluentqt_gallery.samples import build_sample

    app = QApplication.instance() or QApplication(sys.argv[:1])
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")

    entry = ENTRY_BY_ROUTE_ID.get(args.route)
    if entry is None:
        raise KeyError("Unknown Gallery route: {0}".format(args.route))
    sample_entry = next(
        (candidate for candidate in entry.samples if candidate.id == args.sample),
        None,
    )
    if sample_entry is None:
        raise KeyError(
            "Unknown Gallery sample: {0}/{1}".format(args.route, args.sample)
        )

    staging = QWidget()
    result = build_sample(args.route, args.sample, staging)
    try:
        if result.parity_level != "native-equivalent":
            raise RuntimeError(
                "Catalog sample {0}/{1} is not native-equivalent".format(
                    args.route, args.sample
                )
            )
        root_name = _root_name(result)
        source = scene_source(
            route=args.route,
            sample=args.sample,
            title="{0} · {1}".format(entry.title, sample_entry.title),
            preview_source=result.preview_source,
            root_name=root_name,
        )
        _atomic_write_text(output, source)
    finally:
        result.widget.close()
        staging.close()
        app.processEvents()
    return output


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        output = materialize(args)
        print("[catalog-scene] scene: {0}".format(output), flush=True)
        return 0
    except (KeyError, OSError, RuntimeError) as error:
        print("fluent_qt_catalog_scene: {0}".format(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
