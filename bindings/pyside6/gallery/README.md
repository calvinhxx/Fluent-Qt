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

## Spatial mode

Enable the optional binding and Gallery in the same matched Python/Qt build:

```bash
cmake -S . -B build/pyside6 \
  -DFLUENT_QT_BUILD_SPATIAL=ON \
  -DFLUENT_QT_BUILD_PYSIDE6_GALLERY=ON
python3 tools/dev/fluent_qt_build.py build/pyside6 --target fluentqt_pyside6_wheels
PYTHONPATH=build/pyside6/python .venv-pyside/bin/python -m fluentqt_gallery
```

**Settings → 3D Gallery** controls both the navigation/content surfaces and all
examples under **Spatial**. The component pages link back to that setting;
**Key usage / Full example** separates the integration calls from the complete
working Python example. The same widget instances and values survive mode changes.

The shell uses a shared Qt OpenGL canvas with cached widget surfaces. Pointer
input maps back to the live widgets, and native overlays remain above the canvas.
Splash and Intro retain their normal order. The material behind the canvas still
belongs to the Window's Mica/Acrylic setting.

Without the optional binding, the Gallery omits Spatial routes and imports no Qt
OpenGL modules. With the binding but no supported renderer, it retains those
examples in 2D, disables the switch and shows the red support badge. Use
`FLUENT_QT_GALLERY_DISABLE_3D=1` to exercise this fallback. Reduced motion and high
contrast also restore native 2D.

Run the focused regression under `QT_QPA_PLATFORM=offscreen`, or omit that
variable in a native desktop session to check GPU input, scrolling and overlays:

```bash
PYTHONPATH=build/pyside6/python .venv-pyside/bin/python \
  bindings/pyside6/gallery/tests/test_gallery_spatial.py
```

The native test saves snapshots under `build/spatial-validation/python-gallery`.
Offscreen results cover fallback behavior; they do not establish native visual
or performance parity. See [Spatial](../../../docs/architecture/spatial-view.md)
for supported content and platform review boundaries.

<!-- docs-nav:bottom:start -->
---
[← FluentQt PySide6 Hello World](../examples/hello_world/README.md) · [Contents](../../../docs/SUMMARY.md) · [Python bindings index](../README.md) · [Install FluentQt Gallery from PyPI →](PYPI.md)
<!-- docs-nav:bottom:end -->
