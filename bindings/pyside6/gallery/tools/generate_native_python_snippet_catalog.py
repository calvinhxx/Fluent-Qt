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
import fluentqt_gallery

fluentqt.prepare_high_dpi_application()

from PySide6.QtWidgets import QApplication

from fluentqt_gallery.catalog import CONTRACT, ENTRIES
from fluentqt_gallery.samples import build_sample


_GENERATOR_APPLICATION: QApplication | None = None
_GENERATED_RESULTS: list[object] = []


def _stale_runtime_package_files() -> list[str]:
    """Return Gallery modules not refreshed into the active build package."""

    source_dir = Path(__file__).resolve().parents[1] / "src/fluentqt_gallery"
    runtime_dir = Path(fluentqt_gallery.__file__).resolve().parent
    stale = [
        source.name
        for source in sorted(source_dir.glob("*.py"))
        if not (runtime_dir / source.name).is_file()
        or (runtime_dir / source.name).read_bytes() != source.read_bytes()
    ]
    atomic_source = (Path(__file__).resolve().parents[4]
                     / "app/view/widgets/samples/qmlbridge")
    for name in ("Basic.qml", "Theme.qml", "Context.qml", "Action.qml"):
        source = atomic_source / name
        packaged = runtime_dir / "assets/qml_bridge/atomic" / name
        if not packaged.is_file() or packaged.read_bytes() != source.read_bytes():
            stale.append("assets/qml_bridge/atomic/" + name)
    return stale


def generate_catalog() -> dict[str, object]:
    """Build a deterministic catalog from the tested PySide6 sample sources."""

    stale_files = _stale_runtime_package_files()
    if stale_files:
        raise RuntimeError(
            "PySide6 Gallery runtime package is stale; rebuild the configured "
            "PySide6 tree before generating snippets: {0}".format(
                ", ".join(stale_files)
            )
        )

    global _GENERATOR_APPLICATION
    _GENERATOR_APPLICATION = QApplication.instance() or QApplication([])
    app = _GENERATOR_APPLICATION
    app.setProperty("fluentqtGalleryAutomated", True)
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")

    samples: list[dict[str, str]] = []
    for entry in ENTRIES:
        if entry.category_id in {"spatial", "qml-bridge"}:
            continue
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

    # Optional extension sources are packaged even with a base binding. Their
    # binding tests execute these same examples when the modules are built.
    # Exporting teaching text does not initialize OpenGL or a Quick engine.
    from fluentqt_gallery.native_samples_spatial import full_example_source as spatial_source
    from fluentqt_gallery.native_samples_qmlbridge import teaching_source as qml_bridge_source
    from fluentqt_gallery.native_samples_qmlbridge import example_source_files as qml_bridge_files
    for component in CONTRACT["components"]:
        category = component["category_id"]
        if category not in {"spatial", "qml-bridge"}:
            continue
        for sample in component["samples"]:
            entry = {
                "route_id": component["id"],
                "sample_id": sample["id"],
                "source": (spatial_source(sample["id"])
                           if category == "spatial" else qml_bridge_source(sample["id"])),
            }
            if category == "qml-bridge":
                entry["source_files"] = qml_bridge_files(sample["id"])
            samples.append(entry)
    from fluentqt_gallery.qmlplus_samples import python_source, sample_ids
    foundation = CONTRACT["foundation_samples"]["foundation-qmlplus"]
    if tuple(sample["id"] for sample in foundation) != sample_ids():
        raise RuntimeError("Python QMLPlus samples differ from the native atomic sample order")
    for sample in foundation:
        samples.append({"route_id": "foundation-qmlplus", "sample_id": sample["id"],
                        "source": python_source(sample["id"])})
    order = {(c["id"], s["id"]): i for i, (c, s) in enumerate(
        (c, s) for c in CONTRACT["components"] for s in c["samples"]
    )}
    order.update({("foundation-qmlplus", sample["id"]): len(order) + index
                  for index, sample in enumerate(foundation)})
    samples.sort(key=lambda sample: order[(sample["route_id"], sample["sample_id"])])

    return {
        "schema_version": 1,
        "canonical_source": (
            "bindings/pyside6/gallery/src/fluentqt_gallery/samples.py"
        ),
        "summary": {
            "component_count": len(CONTRACT["components"]),
            "foundation_topic_count": len(CONTRACT["foundation_samples"]),
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
