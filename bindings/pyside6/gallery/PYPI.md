# FluentQt Gallery for Python

`FluentQt-Gallery` is the installable Python Gallery for the
[`FluentQt`](https://pypi.org/project/FluentQt/) PySide6 compatibility package.
It demonstrates the native Fluent-Qt C++ widgets through their public Python
API and is distributed separately from the reusable UI library.

## Install and run

```bash
python -m pip install FluentQt-Gallery
python -m fluentqt_gallery
```

Installing the Gallery also installs the exact matching `FluentQt` version.
No separate `.exe`, `.dmg`, or `.deb` is required for the Python application.

## What it includes

- The same component categories and ordered routes as the native C++ Gallery.
- Live Python examples for input, collections, date and time, overlays,
  navigation, scrolling, status, text fields, and windowing.
- Light and dark themes, Fluent / Material / macOS-style presets, responsive
  navigation, search, isolated settings, and native window integration.
- Packaged artwork and source snippets without adding Gallery assets to the
  core `FluentQt` wheel.

## Headless verification

The installed Gallery can also validate its catalog without opening a desktop
window:

```bash
QT_QPA_PLATFORM=offscreen \
  python -m fluentqt_gallery --verify-catalog --walk-routes
```

Native backdrop, title-bar, drag, and compositor effects still require a real
Windows, macOS, or Linux desktop session.

## Compatibility

The Gallery is a pure-Python wheel and follows the platform, architecture,
Python, and Qt runtime support of the exact `FluentQt` version it installs.
Official 1.6.x releases support CPython 3.11–3.13, Windows and macOS x64/ARM64,
and Linux x64/ARM64 where a matching native FluentQt wheel is published.

## Project links

- [FluentQt package](https://pypi.org/project/FluentQt/)
- [Documentation](https://github.com/calvinhxx/Fluent-Qt#readme)
- [Source](https://github.com/calvinhxx/Fluent-Qt)
- [Issue tracker](https://github.com/calvinhxx/Fluent-Qt/issues)
- [Release notes](https://github.com/calvinhxx/Fluent-Qt/releases)
- [Project website](https://calvinhxx.github.io/Fluent-Qt/)

FluentQt Gallery is released under the MIT License.
