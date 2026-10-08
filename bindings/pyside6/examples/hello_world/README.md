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

For a binding built with `FLUENT_QT_BUILD_SPATIAL=ON`, `spatial.py` follows the
[C++ Spatial window](../../../../examples/hello_world/HelloWorldSpatialWindow.cpp):
a 720 × 520 window, a centered 320 × 200 Card, and one accent Button. It
activates `SpatialSession`, adds the card with `addOwnedWidget()`, and uses the
same rotation `(6, -12, 0)`, surface intensity `0.6`, and pointer tilt `(3, 5)`.
Clicking the button changes `Hello from FluentQt` to `Hello, you!`.
It checks activation and `createView()` before using the result. Failed
activation or a `None` view deactivates the session and retains the same Card
and Button in a centered ordinary QWidget layout. `prepareWindow()` is
best-effort after activation; its return value alone does not choose the
fallback or establish GPU availability.

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python bindings/pyside6/examples/hello_world/spatial.py
```

The browser counterpart compiles the C++ window and is staged at
`build/wasm/app/hello-world-spatial/index.html`. It is the minimal application;
the Gallery's `?showcase=spatial` entry includes additional scene controls.
See the [C++ example guide](../../../../examples/hello_world/README.md) for
native and browser build commands.

<!-- docs-nav:bottom:start -->
---
[← Install FluentQt from PyPI](../../PYPI.md) · [Contents](../../../../docs/SUMMARY.md) · [Python bindings index](../../README.md) · [Python Gallery package →](../../gallery/README.md)
<!-- docs-nav:bottom:end -->
