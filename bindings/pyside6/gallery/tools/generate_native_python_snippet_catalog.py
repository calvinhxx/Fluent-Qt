#!/usr/bin/env python3
"""Generate Python teaching snippets consumed by the WebAssembly Gallery.

The PySide6 Gallery already validates every displayed snippet against the
canonical C++ sample card.  This tool serializes those validated snippets so
the WebAssembly C++ Gallery can present an equivalent Python view without
embedding or importing a Python runtime.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Iterable

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtWidgets import QApplication

from fluentqt_gallery.catalog import ENTRIES
from fluentqt_gallery.samples import build_sample


_GENERATOR_APPLICATION: QApplication | None = None
_GENERATED_RESULTS: list[object] = []


def generate_catalog() -> dict[str, object]:
    """Build a deterministic catalog from the tested PySide6 sample sources."""

    global _GENERATOR_APPLICATION
    _GENERATOR_APPLICATION = QApplication.instance() or QApplication([])
    app = _GENERATOR_APPLICATION
    app.setProperty("fluentqtGalleryAutomated", True)
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")

    samples: list[dict[str, str]] = []
    for entry in ENTRIES:
        for sample in entry.samples:
            result = build_sample(entry.route_id, sample.id)
            try:
                if result.route_id != entry.route_id or result.sample_id != sample.id:
                    raise RuntimeError(
                        "Python sample identity mismatch for {0}/{1}".format(
                            entry.route_id,
                            sample.id,
                        )
                    )
                if result.parity_level != "native-equivalent":
                    raise RuntimeError(
                        "Python sample is only {0}, not native-equivalent: {1}/{2}".format(
                            result.parity_level,
                            entry.route_id,
                            sample.id,
                        )
                    )
                if entry.name not in result.covered_types:
                    raise RuntimeError(
                        "Python sample does not cover {0}: {1}/{2}".format(
                            entry.name,
                            entry.route_id,
                            sample.id,
                        )
                    )
                source = result.source
                if not source.strip():
                    raise RuntimeError(
                        "Empty Python source for {0}/{1}".format(
                            entry.route_id,
                            sample.id,
                        )
                    )
                samples.append(
                    {
                        "route_id": entry.route_id,
                        "sample_id": sample.id,
                        "source": source,
                    }
                )
            finally:
                result.widget.close()
                # Several parity previews own queued native callbacks.  Keep
                # their wrappers alive for this short-lived generator process
                # instead of forcing DeferredDelete between samples; immediate
                # teardown can invalidate a callback-owned mutex.
                _GENERATED_RESULTS.append(result)
                QApplication.processEvents()

    return {
        "schema_version": 1,
        "canonical_source": (
            "bindings/pyside6/gallery/src/fluentqt_gallery/samples.py"
        ),
        "summary": {
            "component_count": len(ENTRIES),
            "sample_count": len(samples),
        },
        "samples": samples,
    }


def serialized_catalog() -> str:
    return json.dumps(
        generate_catalog(),
        ensure_ascii=False,
        indent=2,
    ) + "\n"


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Fail when the checked-in catalog differs from generated output.",
    )
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    output = args.output.resolve()
    serialized = serialized_catalog()
    if args.check:
        if not output.is_file() or output.read_text(encoding="utf-8") != serialized:
            raise SystemExit(
                "Native Python snippet catalog is stale: {0}".format(output)
            )
        return 0

    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.is_file() or output.read_text(encoding="utf-8") != serialized:
        output.write_text(serialized, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
