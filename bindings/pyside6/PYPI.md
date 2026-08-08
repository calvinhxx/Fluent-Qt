# FluentQt for Python

FluentQt is the official PySide6 compatibility distribution for
[Fluent-Qt](https://github.com/calvinhxx/Fluent-Qt), a cross-platform
Fluent / WinUI-style C++ component library built on Qt Widgets. The widgets
remain native C++ objects; Shiboken6 exposes the same implementation to Python
instead of reimplementing the UI library.

The C++ library remains the primary project. Python support is optional and
targets Qt 6.

## Install

```bash
python -m pip install FluentQt
```

Published wheels install the matching `PySide6-Essentials` and `shiboken6`
runtime automatically.

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

## Package contents

- Native Qt Widgets with Fluent, Material, and macOS-style theme presets.
- Qt properties, signals, enums, Python subclassing, and typed package stubs.
- Explicit ownership contracts for hosted widgets, overlays, models, and
  native windows.
- A single `_fluentqt` extension so themes, resources, and Qt object identity
  stay process-wide.

The example application is distributed separately as
[`FluentQt-Gallery`](https://pypi.org/project/FluentQt-Gallery/), keeping demo
code and assets out of the reusable UI library wheel.

## Compatibility

Official wheels target CPython 3.11–3.13 with matched Qt, PySide6, and
Shiboken6 6.9.3 runtimes.

| Platform | Architectures |
| --- | --- |
| Windows | x64, ARM64 |
| macOS 12+ | x64, ARM64 |
| Linux | x64; ARM64 for CPython 3.12–3.13 |

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
