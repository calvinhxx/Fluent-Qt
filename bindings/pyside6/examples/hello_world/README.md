# FluentQt PySide6 Hello World

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../../../../docs/README.md) › [Python bindings](../../README.md) › Get started and examples

[← Install FluentQt from PyPI](../../PYPI.md) · [Contents](../../../../docs/SUMMARY.md) · [Python bindings index](../../README.md) · [Python Gallery package →](../../gallery/README.md)
<!-- docs-nav:top:end -->

The 2D `main.py` mirrors
[`examples/hello_world/main.cpp`](../../../../examples/hello_world/main.cpp): it
prepares High DPI before `QApplication`, initializes bundled resources, applies
the FluentQt application font, creates a FluentQt window, and adds one accent
button.

Run it against the build-tree package:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python bindings/pyside6/examples/hello_world/main.py
```

With a wheel installed in the active environment, no `PYTHONPATH` is needed:

```bash
python bindings/pyside6/examples/hello_world/main.py
```

For a binding built with `FLUENT_QT_BUILD_SPATIAL=ON`, `spatial.py` shows the
same window and button inside `SpatialView`. It imports `fluentqt.spatial` and
uses `addOwnedWidget()`; the button keeps its normal `clicked` signal.

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python bindings/pyside6/examples/hello_world/spatial.py
```

<!-- docs-nav:bottom:start -->
---
[← Install FluentQt from PyPI](../../PYPI.md) · [Contents](../../../../docs/SUMMARY.md) · [Python bindings index](../../README.md) · [Python Gallery package →](../../gallery/README.md)
<!-- docs-nav:bottom:end -->
