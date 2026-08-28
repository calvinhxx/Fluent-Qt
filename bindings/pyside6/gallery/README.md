# Python Gallery package

> **Status:** Current package boundary

<!-- docs-nav:top:start -->
[Documentation](../../../docs/README.md) › [Python bindings](../README.md) › Get started and examples

[← FluentQt PySide6 Hello World](../examples/hello_world/README.md) · [Contents](../../../docs/SUMMARY.md) · [Python bindings index](../README.md) · [Install FluentQt Gallery from PyPI →](PYPI.md)
<!-- docs-nav:top:end -->

This directory owns the standalone Python Gallery application.

## Package boundary

- Distribution: `FluentQt-Gallery`
- Import package: `fluentqt_gallery`
- Entry point: `python -m fluentqt_gallery`
- Runtime dependency: the exact matching `FluentQt` distribution version

The Gallery may import only public `fluentqt` APIs. The reusable UILib never
imports `fluentqt_gallery`, and the core wheel never contains Gallery source or
artwork. This keeps applications that only need FluentQt from installing demo
code and large Gallery assets.

The Gallery remains in the same repository because its generated catalog
contract, live samples, and parity tests must evolve atomically with the C++
Gallery and binding manifest.

## Build and run

Configure the PySide6 bindings with
`FLUENT_QT_BUILD_PYSIDE6_GALLERY=ON`, then build both distributions:

```bash
cmake --build build/pyside6 --target fluentqt_pyside6_wheels --parallel
```

Install both generated wheels into a clean virtual environment and launch:

```bash
python -m pip install \
  build/pyside6/wheelhouse/fluentqt-*.whl \
  build/pyside6/gallery-wheelhouse/fluentqt_gallery-*.whl
python -m fluentqt_gallery
```

Use `--verify-catalog --walk-routes` for deterministic headless acceptance.

<!-- docs-nav:bottom:start -->
---
[← FluentQt PySide6 Hello World](../examples/hello_world/README.md) · [Contents](../../../docs/SUMMARY.md) · [Python bindings index](../README.md) · [Install FluentQt Gallery from PyPI →](PYPI.md)
<!-- docs-nav:bottom:end -->
