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

The Gallery imports only public `fluentqt` APIs. The core `FluentQt` wheel stays independent of Gallery source code and assets, so apps do not install demo code.

This Gallery stays in the main repository so its catalog, examples, and parity tests evolve together with the C++ Gallery.

## Build and run

Configure the PySide6 bindings with `FLUENT_QT_BUILD_PYSIDE6_GALLERY=ON`, then build both wheels:

```bash
cmake --build build/pyside6 --target fluentqt_pyside6_wheels --parallel
```

Install both wheels into a clean virtual environment and launch:

```bash
python -m pip install \
  build/pyside6/wheelhouse/fluentqt-*.whl \
  build/pyside6/gallery-wheelhouse/fluentqt_gallery-*.whl
python -m fluentqt_gallery
```

Use `--verify-catalog --walk-routes` for deterministic headless checks.

## Interface zoom

Press Ctrl/Cmd + `+`, `-`, or `0` (or use the command palette) to zoom Gallery between 50% and 200%. Like a browser, zooming briefly shows a zoom bubble with `+`, `-`, and Reset while windows keep their size and relayout inside. Both C++ and Python Galleries share this native zoom backend, which requires matching Qt Core/Gui/Widgets private headers.

Keep `FLUENT_QT_INTERFACE_ZOOM_LIVE=ON` when building Gallery; missing private headers fail CMake configuration with setup instructions. Ordinary `FluentQt` C++ and Python library builds do not require private Qt headers (WebAssembly uses browser zoom).

## QML+ foundation

**Foundation → QML+** provides twelve examples covering bindings, pause/resume, states, animations, and anchors, running directly on the shared native QMLPlus runtime.

CTest separates core contract checks (`test_pyside6_gallery`) from full route and theme sweeps (`test_pyside6_gallery_routes`):

```bash
ctest --test-dir <build> -R '^test_pyside6_gallery(_routes)?$' --output-on-failure
```

You can also run `gallery/tests/test_gallery.py` directly with standard `unittest` filters.

## Spatial mode

Release wheels include Spatial starting in 1.9.1 (upgrade both `FluentQt` and `FluentQt-Gallery` if on 1.9.0). For source builds, enable Spatial explicitly:

```bash
cmake -S . -B build/pyside6 \
  -DFLUENT_QT_BUILD_SPATIAL=ON \
  -DFLUENT_QT_BUILD_PYSIDE6_GALLERY=ON
python3 tools/dev/fluent_qt_build.py build/pyside6 --target fluentqt_pyside6_wheels
PYTHONPATH=build/pyside6/python .venv-pyside/bin/python -m fluentqt_gallery
```

Enable **Settings → 3D Gallery** to turn on 3D mode for navigation and all **Plugins → Spatial** examples. When hardware acceleration is available on startup, Gallery starts in 3D unless you switch it back to 2D (failing initialization falls back to 2D cleanly while keeping widgets and values intact).

Under **Plugins → QmlBridge → QmlView**, six cards cover QML loading, `fluentTheme` tokens, context objects, QML actions, lifecycle, and load errors alongside the standalone [shared-model example](../examples/qml_bridge.py). In 3D mode on OpenGL hosts, `QmlTextureLayer` renders QML pages to textures cleanly; hosts without OpenGL texture import display QML pages in 2D without disabling 3D elsewhere. Native Window Mica/Acrylic backdrops, overlays, reduced motion, and high-contrast fallback work identically to C++. Use `FLUENT_QT_GALLERY_DISABLE_3D=1` to test the 2D fallback path.

Run headless Spatial fallback checks or native GPU checks cleanly:

```bash
# Headless fallback check (or omit QT_QPA_PLATFORM for interactive native desktop check)
QT_QPA_PLATFORM=offscreen PYTHONPATH=build/pyside6/python .venv-pyside/bin/python \
  bindings/pyside6/gallery/tests/test_gallery_spatial.py

# Dedicated native backdrop-teardown check (run on a real desktop session)
ctest --test-dir build/pyside6 -R '^test_pyside6_gallery_spatial_backdrop_native$' --output-on-failure
```

See [Spatial](../../../docs/architecture/spatial-view.md) for supported widgets and fallback behavior.

<!-- docs-nav:bottom:start -->
---
[← FluentQt PySide6 Hello World](../examples/hello_world/README.md) · [Contents](../../../docs/SUMMARY.md) · [Python bindings index](../README.md) · [Install FluentQt Gallery from PyPI →](PYPI.md)
<!-- docs-nav:bottom:end -->
