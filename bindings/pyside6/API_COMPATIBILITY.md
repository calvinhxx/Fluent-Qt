# PySide6 API compatibility policy

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../../docs/README.md) › [Python bindings](README.md) › Compatibility and delivery

[Contents](../../docs/SUMMARY.md) · [Python bindings index](README.md) · [PySide6 manylinux build and audit policy →](MANYLINUX.md)
<!-- docs-nav:top:end -->

This policy applies to the public Python surface exported by `fluentqt` and
its documented category modules. The committed [`api-manifest.json`](api-manifest.json)
is the machine-readable public API ledger; generated Shiboken implementation
helpers, private names, and undocumented native mixins are not compatibility
contracts.

## Versions

- The root CMake `project(FluentQt VERSION MAJOR.MINOR.PATCH)` declaration is
  the single version source for the C++ library, Python package, and wheel.
- `fluentqt.__version__` is the full package version and must match both the
  wheel metadata and `binding_build_info()["fluentqt_version"]`.
- `fluentqt.__api_version__` is the `MAJOR.MINOR` public API line recorded as
  `api_version` in `api-manifest.json`.
- A patch release may fix behavior without removing or changing a documented
  Python call contract. A minor release may add backward-compatible API. A
  removal, rename, incompatible signature change, ownership-policy change, or
  packaging-contract break requires a new major version.
- The exact PySide6, Shiboken6, and Qt dependency versions in a wheel are a
  native-runtime compatibility constraint, not a separate FluentQt API
  version.

## Deprecations

1. Add an active entry to `api-manifest.json` before shipping a deprecation.
   Each entry names the public symbol, the first deprecating release, the next
   major release that may remove it, an optional replacement, and a reason.
2. Keep the old symbol behavior-compatible for the remainder of its major
   release line. A Python facade emits `DeprecationWarning` with `stacklevel=2`
   when the deprecated entry is used; native adapters must provide an
   equivalent Python-visible warning before forwarding to the replacement.
3. Document the replacement in the binding guide and release notes. A rename
   keeps the old spelling as a deprecated forwarding alias; it is never a
   silent removal.
4. Remove the symbol only in the declared later major release. Remove its
   active ledger entry in the same change and record the breaking change in
   the curated release notes.

### 1.7 Fluent-only reset

Version 1.7 is an explicitly approved one-time breaking cleanup made before the
binding reached broad adoption. It removes `DesignLanguage`, `StyleTheme`,
`current_design_language()`, `FluentWidget.design_language()`, and
`apply_style_theme()` instead of carrying no-op compatibility shells. Use
Light/Dark `Theme`, Fluent semantic tokens, `set_accent_color()`, and
`apply_user_theme()` instead. These removed symbols are absent from the 1.7 API
manifest rather than listed as active deprecations. Future removals follow the
major-version policy above.

## New C++ surface decisions

QMLPlus reactive authoring is available through Python `bind_managed`,
`bind_computed`, `BindingHandle`, `set_behavior`, and the extended `StateGroup`.
These adapters use the native runtime. Python expressions are Qt-owned by the
target or state controller; dependency objects remain caller-owned. The
[coverage ledger](ROADMAP.md#qmlplus-reactive-authoring) records the callback,
teardown, and motion contracts.

Existing `fluentqt.bind()` calls use the same native managed runtime. Both
endpoints must belong to the calling Qt thread, and each observed direction
requires a NOTIFY signal. Native registration failure raises `ValueError`;
the adapter does not report success from argument validation alone. A valid
replacement disconnects the previous binding on that target property; an invalid
registration leaves it intact. The legacy C++ `bind()` initial synchronization
without NOTIFY is not part of the Python binding contract.

`Timeline`, `Stepper` and `StepperItem` are supported in Python in the same
slice as C++. Timeline borrows the native Qt model and delegate; Stepper owns
only its navigation metadata and emits requests without owning application
pages or workflow rules. Their Gallery routes share the C++ sample contract.

Every new installed C++ component or non-trivial public API records one of
these decisions before release:

- **Supported in Python:** the Shiboken export, API manifest, typing/runtime
  checks, ownership tests, and a Python example land in the same release slice
  as the public C++ API.
- **Intentionally C++-only:** the limitation and reason are documented in the
  release roadmap and binding coverage ledger, and generated guidance must not
  advertise a Python import.

Private prototypes may stay C++-only while an API is still changing. They are
not installed, exported from `<FluentQt/FluentQt.h>`, or represented as public
catalog entries. Do not publish a C++ API first and silently treat Python as an
unscheduled follow-up; either finish the selected surface or keep the feature
private/preview.

For item views, Python ownership follows Qt's model/view/delegate contracts.
Bindings must not introduce a Python-owned mirror of the model or a persistent
widget per item to compensate for missing native API.

`DataGrid` is supported in Python in the same slice as its C++ API. The facade
retains Python wrappers for the caller-supplied model, selection model, and
delegate while Qt remains the native object owner; replacement and garbage-
collection tests guard that boundary. Its Python Gallery routes use the same
three read-only, selection/column, and editing/validation scenarios as C++.

## Gates

On Qt/PySide6 6.2.4, use `QObject.deleteLater()` for application object teardown.
The bundled Shiboken has an
[upstream `delete()` defect](https://github.com/pyside/pyside-setup/commit/fca1416b4f4aedc640f05a8183e04b67ea117495)
that can mistake an ordinary wrapper for `QApplication` and destroy the application.
Lifetime regressions explicitly deliver Qt's `DeferredDelete` events when they
need synchronous destruction inside a callback, and check that the application
remains alive.

`optional_modules` in the manifest records APIs enabled by a source-build option.
Their public names are imported from the declared category module and validated
when that module is packaged. Default builds need not export these names.
`fluentqt.spatial` is enabled by `FLUENT_QT_BUILD_SPATIAL`; it uses explicit
owned, borrowed, reparented and take methods around native widget ownership.
`SpatialSession` exposes activation, tracked views, particle adapters, and
hidden-window preparation. Python activation and deactivation return `bool`;
the optional C++ error output is omitted. Session-created views use the same
Python ownership bookkeeping as directly constructed `SpatialView` objects.
Stopping or deleting a session restores tracked surfaces to 2D and releases
particle delegation without deleting their caller-owned outputs or models.
`fluentqt.plugins` is enabled by `FLUENT_QT_BUILD_PLUGINS` and exposes the native
metadata, context, plugin, and manager lifecycle. Python subclasses use the same
dependency, rollback, no-op, and reentrancy contracts as C++. Registration
borrows the native object and does not retain an unparented Python plugin's
wrapper; the application must keep its reference or provide a Qt owner.
Collecting a plugin unregisters it and stops surviving dependents. Widget
factories must preserve caller ownership for both parented and parentless
outputs, including across garbage collection and manager or plugin destruction.
Invalid returned parents are
rejected and cleaned up by the native manager.

The raw `WidgetPluginInterface` is a C++ Qt interface. Python exposes its
factory capability through `WidgetPlugin`, whose native bridge declares the
interface; Python subclasses do not duplicate Qt interface registration.

Spatial-enabled builds also export `SpatialPlugin` for registration with a
generic manager. `SpatialSession` remains the convenience scope over that
lifecycle. WebAssembly consumers compile the shared C++ implementation into
their executable; the Python category module is not a browser runtime and the
plugin contract does not promise dynamic shared-library loading.

`fluentqt.qmlbridge` is enabled by `FLUENT_QT_BUILD_QML_BRIDGE` and exports
`QmlPlugin`, `QmlSession`, `QmlTheme`, `QmlTextureLayer`, and `QmlView`. These classes use the same
native lifecycle as C++ and WebAssembly. The bridge module imports PySide's
QML/Quick converters explicitly; the base package keeps its import boundary.
Context objects are borrowed without QObject reparenting or Python wrapper
retention. Factory views remain caller-owned. Stopping a session clears its
public scene immediately and schedules the Quick widget and engine for Qt
deferred deletion, so a running QML callback can finish. Reactivation permits
explicit loading and preserves the model bindings. Python omits the optional
C++ error output arguments and exposes bool results plus view error accessors
and signals. `QmlTheme` is view-owned and read-only, with no public constructor.
The manifest also records the shared OpenGL layer's cache, composition, and
failure APIs and `QmlView`'s texture mode, rendering, and update signals.
Both native and facade stubs must expose those contracts when QmlBridge is
enabled. Texture presentation preserves scene/model ownership and has no
Qt Quick 3D dependency; the [composition contract](../../docs/architecture/qml-bridge.md#composition-and-gallery-mode)
defines backend, DPR, budget, and 2D fallback limits.

`tools/verify_api_policy.py` rejects version drift, malformed or duplicate
ledger entries, missing public version variables, deprecations of unknown
symbols, replacements that are not public, future deprecation dates, and
same-major removals. Stub generation verifies the version variables and the
rest of the manifest, while build-tree and clean-wheel tests require runtime,
native, metadata, and typing versions to agree.


## Charts

`ChartData`, `ChartModel`, and `ChartView` are exported by `fluentqt` and
`fluentqt.charts`. The native model remains caller-owned; the Python view
retains model wrappers for borrowed series. Snapshot, batch append, all eight
presentations, viewport, cursor, and aggregation APIs match C++. See the
[Charts contract](../../docs/architecture/charts.md) for costs and limits.

<!-- docs-nav:bottom:start -->
---
[Contents](../../docs/SUMMARY.md) · [Python bindings index](README.md) · [PySide6 manylinux build and audit policy →](MANYLINUX.md)
<!-- docs-nav:bottom:end -->
