# Install FluentQt from PyPI

> **Status:** Current package guide

<!-- docs-nav:top:start -->
[Documentation](../../docs/README.md) › [Python bindings](README.md) › Get started and examples

[Contents](../../docs/SUMMARY.md) · [Python bindings index](README.md) · [FluentQt PySide6 Hello World →](examples/hello_world/README.md)
<!-- docs-nav:top:end -->

[FluentQt](https://github.com/calvinhxx/Fluent-Qt) is an MIT-licensed Qt Widgets
library for PySide6. Shiboken6 exposes its C++17 implementation to Python.
The Gallery presents 99 entries covering inputs, navigation, charts, dialogs
and components from the optional Particles, Spatial and QmlBridge modules.

## Install

```bash
python -m pip install FluentQt
```

Published wheels install the matching `PySide6-Essentials` and `shiboken6`
runtime automatically. Install the standalone example application with:

```bash
python -m pip install FluentQt-Gallery
python -m fluentqt_gallery
```

## Minimal example

```python
import sys

import fluentqt
from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication, QVBoxLayout, QWidget
from fluentqt.basicinput import Button
from fluentqt.windowing import Window


fluentqt.prepare_high_dpi_application()
app = QApplication(sys.argv)
fluentqt.initialize_resources()
app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

window = Window()
window.setWindowTitle("FluentQt Hello World")
window.resize(480, 320)

content = QWidget()
layout = QVBoxLayout(content)
button = Button("Hello from FluentQt", content)
button.setFluentStyle(Button.ButtonStyle.Accent)
layout.addWidget(button, 0, Qt.AlignCenter)

window.setContentWidget(content)
window.show()
sys.exit(app.exec())
```

## Explore and design visually

- **WebAssembly Gallery**: Try all controls, themes, and 3D views in your
  browser at [calvinhxx.github.io/Fluent-Qt/gallery/](https://calvinhxx.github.io/Fluent-Qt/gallery/).
- **Property preview**: Adjust live Qt properties and copy generated PySide6
  code in the [interactive playground](https://calvinhxx.github.io/Fluent-Qt/api/checkbox/?playground=1).
- **Page Studio**: Arrange controls visually in the browser and export PySide6
  or `.ui` layouts from [Page Studio](https://calvinhxx.github.io/Fluent-Qt/studio/?lang=en).
- **Qt Designer**: Drag widgets from the Widget Box or select a template in
  New Form. Share the resulting `.ui` file with Qt Creator. See the
  [Qt Designer guide](https://github.com/calvinhxx/Fluent-Qt/blob/main/tools/designer/README.md).

## Package contents

- Native controls with Light, Dark and High Contrast themes.
- Qt properties, signals, enums, Python subclassing, and typed package stubs.
- Explicit ownership contracts for hosted widgets, overlays, models, and
  native windows.
- A single `_fluentqt` `abi3` extension so themes, resources, and Qt object
  identity stay process-wide.

The example application is distributed separately as
[`FluentQt-Gallery`](https://pypi.org/project/FluentQt-Gallery/), keeping demo
code and assets out of the reusable UI library wheel.

## Compatibility

Each supported platform and architecture has a CPython stable-ABI (`abi3`)
wheel for the matched Qt, PySide6, and Shiboken6 6.9.3 runtimes. The wheels
support CPython 3.11–3.13. On Linux ARM64 they require CPython 3.12–3.13 and glibc 2.39 or newer.

| Platform | Architectures |
| --- | --- |
| Windows | x64, ARM64 |
| macOS 12+ | x64, ARM64 |
| Linux | x64, ARM64 |

The source binding keeps Qt/PySide/Shiboken 6.2.4 as its minimum compatibility
baseline. All Qt for Python components and the C++ Qt SDK must use the same
version and architecture.

## Project links

- [Documentation](https://github.com/calvinhxx/Fluent-Qt#readme)
- [Source](https://github.com/calvinhxx/Fluent-Qt)
- [Issue tracker](https://github.com/calvinhxx/Fluent-Qt/issues)
- [Release notes](https://github.com/calvinhxx/Fluent-Qt/releases)
- [Project website](https://calvinhxx.github.io/Fluent-Qt/)

FluentQt is released under the MIT License. Microsoft product names and Fluent
design references are used only to describe compatibility and visual style.

<!-- docs-nav:bottom:start -->
---
[Contents](../../docs/SUMMARY.md) · [Python bindings index](README.md) · [FluentQt PySide6 Hello World →](examples/hello_world/README.md)
<!-- docs-nav:bottom:end -->
