# FluentQt PySide6 Hello World

This is the single Python Hello World and mirrors
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
