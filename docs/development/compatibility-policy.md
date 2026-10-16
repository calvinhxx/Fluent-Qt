# Compatibility Policy

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › API, policy, and writing

[← Technical debt roadmap](technical-debt-roadmap.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Source Comment Style →](comment-style.md)
<!-- docs-nav:top:end -->

This policy applies from FluentQt 1.7 onward. It keeps the public contract
predictable without promising binary compatibility that Qt toolchains cannot
provide.

## Public API

- Patch and minor releases remain source compatible within major version 1.
- New overloads, properties, signals, and components may be added compatibly.
- Removing or changing an established public API requires the next major
  release. The Fluent-only cleanup in 1.7 was the documented final exception
  before this policy.
- Installed headers listed by `cmake/FluentQtInstallHeaders.cmake`, exported
  CMake targets, and documented PySide6 names are public. Private headers,
  Gallery internals, tests, and generated implementation details are not.

## Binary ABI

FluentQt does not promise a stable C++ binary ABI across releases, compilers,
standard libraries, build modes, or Qt versions. Rebuild FluentQt and the
consumer with the same Qt/toolchain when upgrading. Release packages and
PySide6 wheels are matched to their documented platform, architecture, Qt,
Python, and Shiboken runtimes.

## Deprecation

- A planned public removal first gains a compiler/runtime deprecation that
  names the replacement and is documented in release notes.
- Deprecated APIs remain available for at least one minor release and are
  tested until removal in the next major release.
- A security issue, undefined behavior, or an upstream Qt break may require a
  faster change; the release notes must state the reason and migration.

## Supported Qt lines

- Native C++: C++17 with Qt Widgets 5.15+ or Qt 6.2+.
- CI samples Qt 5.15.2 and Qt 6.2 compatibility alongside the current Qt 6
  release line. Platform/architecture package coverage is listed in the
  repository README and release assets.
- PySide6 source compatibility starts at Qt/PySide/Shiboken 6.2.4; published
  wheels use the matched runtimes listed in the
  [binding policy](../../bindings/pyside6/API_COMPATIBILITY.md).
- WebAssembly is a separate, pinned browser toolchain: Qt 6.9.3
  `wasm_singlethread` with Emscripten 3.1.70.

Raising a minimum Qt, C++, CMake, or Python version is an incompatible build
contract change and follows the major-release process unless an upstream
security or distribution constraint makes that impossible.

## Source compatibility layer

`src/compatibility/` owns Qt API differences and reusable platform adaptations.
Components implement one interaction flow and call these adapters. Moving a
version comparison into a `FLUENT_*` macro does not make a second component
implementation part of that common flow.

| Location | Responsibility |
| --- | --- |
| `QtCompat.h` | Backward-compatible umbrella; existing helper names and synthetic-event macros remain available |
| `InputEventCompat.h`, `EventConstructionCompat.h` | Event signatures/coordinates, and separately, synthetic event construction |
| `ObjectCompat.h`, `WidgetCompat.h` | Connections/metatypes, widget signals, layout and item-view signatures |
| `ApplicationCompat.h`, `QtFeatures.h` | Startup/theme/display changes and lightweight capability declarations |
| `AccessibilityCompat.h` | Accessibility event and announcement differences |
| `FontCompat.h`, `PaintCompat.h`, `TextPaintCompat.h` | Font APIs, DPR-aware pixmaps and text painting |
| `WindowChromeCompat.h/.cpp`, `WindowBackdropTypes.h/.cpp` | Public window facade and backdrop value types |
| `private/` | Non-installed adapters, including pointer forwarding, date/time feature fallbacks, optional accessibility interfaces and widget presentation |
| `private/quick/`, `private/spatial/` | Adapters owned by the optional QmlBridge and Spatial targets |
| `private/spatial/renderer/` | Application-only RHI backend, built by `FluentQtSpatialRenderer`; excluded from the public Spatial target and installed SDK |
| `platform/` | Native window backends implementing `private/WindowChromeBackend_p.h` |

Implementation files include the domain they use. Keep the umbrella compatible;
do not add another unrelated helper to it. Installed domain headers must be
self-contained and appear in `cmake/FluentQtInstallHeaders.cmake`. They must not
include private implementation headers or bring optional Quick/OpenGL
dependencies into the Widgets core.

Keep established public namespaces and names. Private adapters use
`::compatibility::detail` so they do not introduce competing `fluent::compat`
and `fluent::compatibility` namespaces. They accept Qt types or capability data,
not component classes. Ownership, reentrancy guards and business decisions stay
with the component; coordinate translation and preservation of event metadata
stay with the adapter.

Use the pointer adapter for both direct mouse handlers and Qt event dispatch.
Reconstructing a mouse event in a component can lose its fractional coordinates,
input source, device or timestamp even when its constructor compiles on every
supported version.

Qt feature availability such as `QT_CONFIG(accessibility)` can guard an entire
optional implementation. Differences between Qt versions or operating systems
belong inside the compatibility layer. When an old Qt cannot expose an
accessibility interface, the adapter keeps that native interface unavailable
while the component retains the same selection or link implementation.

The Python package follows the same boundary. Private `_qt_compat.py` owns
QObject lifetime and context-owned timers; `_qt_events.py` owns event
availability; `_qt_painting.py` owns paint metrics, enum conversion and legacy
paint-engine call shapes; `_qt_widgets.py` owns wrapper and widget API
adaptations, including system color-scheme availability. Category facades and
Gallery import these adapters. Catching `AttributeError` around a newer Qt API
is an availability adaptation and belongs in a private adapter too. Gallery
policies such as close behavior, platform labels and update-package selection
remain with the application. Include new adapter modules and their generated
stubs in both the CMake installation and wheel inventories.

`test_qt_compat` compiles every installed compatibility header separately, checks
installed-header dependencies, and rejects Qt version checks throughout project
C++ sources, including checks assigned to constants or hidden in macros. It also
rejects indirect capability guards and OS branches in components. C++
shared renderer support is subject to the same rule: CMake-selected Qt
capabilities such as RHI availability may only branch inside an adapter.
Component runtime version queries and direct pointer-event reconstruction are
also rejected. Python's `test_pyside6_qt_compat` checks the package and Gallery
for version comparisons, Qt capability lookups and availability exception
handlers outside private adapters. Diagnostic version strings and code samples
remain allowed.

Runtime tests cover translated coordinates, input metadata, single-shot signal
arguments and queued delivery, DPR metrics, and the owning component contracts.
Fallback paths that can run on the current Qt are tested alongside the native
path; that does not replace compiling and running against older Qt SDKs. Run the
[local static gate](testing-workflow.md#local-static-gate) for formatting and
the owning tests for behavior; neither replaces the supported SDK/OS matrix.

<!-- docs-nav:bottom:start -->
---
[← Technical debt roadmap](technical-debt-roadmap.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Source Comment Style →](comment-style.md)
<!-- docs-nav:bottom:end -->
