# QmlBridge: application QML in a QWidget host

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Particles: optional decorative effects](particles.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Window Chrome Architecture →](window-chrome.md)
<!-- docs-nav:top:end -->

QmlBridge embeds an application QML page inside a FluentQt QWidget application.
It is an optional plugin module under Plugins, with its own Qt QML and Quick
dependencies. FluentQt controls remain native Widgets; the hosted page uses
Qt Quick items and explicitly binds to the supplied Fluent theme tokens.

The public types live in `src/components/qmlbridge`, use `fluent::qmlbridge`,
and are installed through `<FluentQt/QmlBridge.h>`. Applications link
`FluentQt::QmlBridge`; its public dependency is `FluentQt::Plugins`. The Qt Qml,
Quick, and QuickWidgets implementation dependencies stay with this module.
The core Widgets target and the QWidget-only [QMLPlus](qmlplus.md) foundation
retain their existing dependency boundary.

## Build and consume

The source-build option defaults to off. Enable it explicitly:

```bash
cmake -S . -B build/qml-bridge \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt \
  -DFLUENT_QT_BUILD_QML_BRIDGE=ON
python3 tools/dev/fluent_qt_build.py build/qml-bridge \
  --config Release --target FluentQtQmlBridge fluentqt_qml_bridge
cmake --install build/qml-bridge --config Release \
  --component Development --prefix /path/to/install
```

QmlBridge automatically includes Plugins. Native source compatibility follows
the C++17 and Qt 5.15+/6.2+ baseline; the selected Qt kit must also supply Qml,
Quick, and QuickWidgets. For source integration, set the option before
`add_subdirectory()` or `FetchContent_MakeAvailable()`.

Source builds with Qt 6.2–6.5 also need the matching Qt Quick private headers.
Those versions require a private render-window adapter to forward keyboard
focus into a texture-hosted QML scene; Qt 6.6+ and Qt 5 use the public path.
This build-only dependency is confined to QmlBridge and is not exported to
installed consumers or the Widgets core.

Installed consumers request the component:

```cmake
find_package(FluentQt CONFIG REQUIRED COMPONENTS QmlBridge)
target_link_libraries(my_app PRIVATE FluentQt::QmlBridge)
```

Only that component request discovers its Qt Quick dependencies and loads its
exported target and Plugins dependency. A base `find_package(FluentQt CONFIG
REQUIRED)` continues to discover Widgets alone. Public QmlBridge headers are
installed only when the module is enabled. Deploy the Qt libraries and QML
imports required by the application's page; declaring the CMake target does
not bundle arbitrary QML imports.

The [minimal C++ example](../../examples/qml_bridge/) builds the
`fluentqt_qml_bridge` target. The
[Python example](../../bindings/pyside6/examples/qml_bridge.py) uses the matched
optional native binding. Gallery groups focused examples under
**Plugins → QmlBridge → QmlView**. Browser applications compile the same C++
plugin and host; this introduces no dynamic-library loading or JavaScript
plugin manager. API availability, a successful build, actual graphics/input
validation, and platform visual acceptance are separate evidence.

The WebAssembly example opens at
`build/wasm/examples/qml_bridge/index.html`; Gallery builds also stage it at
`build/wasm/app/qml-bridge/index.html`. Use the
[browser build workflow](../development/webassembly-workflow.md) for the pinned
Qt/Emscripten kit and runtime integration.

## Gallery example cards

Open **Plugins → QmlBridge → QmlView** to learn one integration step at a time.
The C++, Python, and WebAssembly Galleries use the same card order and QML assets:

| Card ID | Focus | QML asset |
|---|---|---|
| `qml-view-load` | Activate a session, create a view, and load a page | [Basic.qml](../../app/view/widgets/samples/qmlbridge/Basic.qml) |
| `qml-view-theme` | Bind the page to Fluent colors, typography, and radii | [Theme.qml](../../app/view/widgets/samples/qmlbridge/Theme.qml) |
| `qml-view-context` | Supply a caller-owned context object before loading | [Context.qml](../../app/view/widgets/samples/qmlbridge/Context.qml) |
| `qml-view-action` | Change a bound Widgets control from a QML action | [Action.qml](../../app/view/widgets/samples/qmlbridge/Action.qml) |
| `qml-view-lifecycle` | Deactivate, reactivate, and explicitly reload the view | Basic.qml |
| `qml-view-error` | Observe a load error and retry a valid source | Basic.qml |

C++ Gallery shows C++ host code; Python Gallery shows Python host code.
WebAssembly Gallery provides the **C++ / Python** selector. Switching languages
selects `main.cpp` or `main.py`. The **File** selector separates the host source
from its QML page. **Key usage**
shows the relevant C++ or Python operation; **Full example** shows the complete
host file, including application startup and layout. Select the `.qml` file
to read its bindings, layout, or event handlers with QML highlighting. Copy
copies only the selected file.

The C++ example consists of `main.cpp`, its `.qml` page, `qml.qrc`, and
`CMakeLists.txt`. The resource file packages the QML page; the host loads its
`qrc:/` URL. Save all four files in one directory and build with CMake. The
Python example consists of `main.py` and its `.qml` page; save both files in
one directory and run with the matched optional binding. These examples use
the same QML assets as the live cards.

For browser entry points and event-loop lifetime, use the shared
[WebAssembly example](../../examples/qml_bridge/platform/wasm/QmlBridgeApplication.cpp).

Context and action cards use a caller-owned `LineEdit` or `ProgressBar`.
The theme card follows the page's preview theme button. **Reload QML** explicitly
unloads the page before loading it again, even when the session is already
active. Each card has its own session and controls, so these operations affect
only that card.
The four Gallery assets are packaged with the optional Python Gallery module.

## Run the standalone example

With the macOS development preset configured, build and run the native example:

```bash
python3 tools/dev/fluent_qt_build.py --preset vcpkg-osx --target fluentqt_qml_bridge
./build/vcpkg-osx/examples/qml_bridge/fluentqt_qml_bridge
```

For Python, activate the matched PySide6 environment used by the binding build:

```bash
PYTHONPATH=build/vcpkg-osx/python python3 bindings/pyside6/examples/qml_bridge.py
```

The standalone C++ and Python launchers accept `--smoke-test`, `--theme dark`, `--size 420x760`, and
`--capture /path/to/image.png`. The form and QML preview share a title and
progress value. **Advance +10** changes the model from QML; **Deactivate**
unloads QML while keeping the form editable; **Reload QML** restores the page
with the current model values. Use another host preset and executable suffix
where appropriate. A smoke pass checks behavior, not native visual acceptance.

Use the [WebAssembly workflow](../development/webassembly-workflow.md) to build
the same target for the browser. Serve `build/wasm` over HTTP and open
`app/qml-bridge/index.html` or the standalone
`examples/qml_bridge/index.html`. The query `?theme=dark` selects Dark, and
`?smoke-test` runs the shared lifecycle assertions. With Playwright installed,
the [browser gate](../../.github/scripts/run-wasm-qml-bridge-smoke.py) verifies
the standalone example and the focused Gallery cards, saving Light/Dark/narrow captures at
DPR 2. The Gallery check requires a hardware 3D renderer: it starts on a 3D
Widgets page, operates the projected QML preview through real canvas input,
and checks the page and unload/reload transitions without changing the
preference. The local hardware run uses headed Chrome; an automation browser
may select a software renderer,
and its 2D fallback does not pass this 3D gate:

```bash
python3 .github/scripts/run-wasm-qml-bridge-smoke.py --root build/wasm --headed --channel chrome
```

`--channel chrome` selects installed Chrome instead of bundled Chromium. For a
standalone example-only artifact, use `--scope example`; that scope does not
validate the Gallery composition boundary. Gallery's narrow case uses 620×860
with adaptive navigation; the standalone example's narrow case uses 420×860.

## Application setup

1. Choose a graphics API before creating the first Qt Quick widget or window.
   For an OpenGL host, `QmlSession::prepareApplication()` retains its existing
   OpenGL selection. The overload accepts `QmlSession::GraphicsApi::Direct3D11`
   or `Metal` for a matching native host; `Automatic` restores Qt's platform
   and environment selection. Qt 5 retains its OpenGL/software backend.
   `isGraphicsApiCompatible()` checks the current selection without changing it.
   A `QApplication` may already exist, but no Quick surface may have been created.
   This is application-wide policy; session activation never selects a backend.
2. Construct `QmlSession(host, parent)` on the GUI thread and check `activate()`.
   The host is borrowed; the QObject parent owns the session. Activation alone
   does not load a QML page.
3. Check `createView(parent)` for null, or check `attachView(existingView)` for
   failure. The application owns the QWidget through its usual parent or local
   lifetime. A view can join only one plugin scope at a time.
4. Before loading, bind each application model with
   `setContextObject(name, object)`. Check its result. Names must be QML
   identifiers, and `fluentTheme` is reserved for the view's theme object.
5. Connect `statusChanged` and `errorOccurred`, then call `load()` with an absolute
   URL, such as `QUrl::fromLocalFile(path)` or `QUrl("qrc:/Main.qml")`. Check the
   immediate result and observe the final status when loading is asynchronous.

Keep ordinary application content available if activation, attachment, or
creation fails. A successful `load()` means Ready or an accepted asynchronous
load; wait for Ready before replacing content that must remain usable on an
asynchronous failure. `status()` distinguishes Unloaded, Loading, Ready, and
Error. `errors()` and `lastError()` expose diagnostics without Qt Quick types
in the public API.

`rootObject()` is borrowed. Unloading or replacing a page invalidates that root
and its scene-owned children; retain guarded pointers rather than using them
after a lifecycle change. Loading the same Ready source is a no-op. Call
`unload()` before explicitly reloading that file.

## Composition and Gallery mode

QmlView clears to transparent in both native and texture presentation. Paint the
page background in QML with `fluentTheme.surface` and `fluentTheme.cornerRadius`;
use `antialiasing: true` for smooth rounded edges. Unpainted pixels remain
transparent in the QML texture. Native Quick retains Qt's QWidget stacking
rules; transparency can expose the window background rather than a painted
parent surface, and does not expose arbitrary Widgets stacked underneath.

Qt's native widget compositor clears the backing store beneath each Quick
rectangle. For a rounded page inside a Gallery card, the examples paint
`fluentTheme.surfaceAlt` behind that page to match the card's inset surface.
This keeps the corners correct when the preview and application use different
themes. The QML backdrop is part of those examples; QmlView's clear color and
GPU texture alpha remain transparent for pages that do not paint a backdrop.

A visible QQuickWidget requires direct composition outside ancestor
`QGraphicsEffect` captures. Qt applies a widget's graphics effect to its
descendants and does not support these effects for QQuickWidget or QOpenGLWidget.
See the
[Qt 6.9.3 QWidget contract](https://github.com/qt/qtbase/blob/v6.9.3/src/widgets/kernel/qwidget.cpp).
Selecting a compatible graphics backend does not remove this restriction.

`fluent::qmlbridge::QmlTextureLayer` (Python:
`fluentqt.qmlbridge.QmlTextureLayer`) lets an OpenGL compositor include QML without capturing a
visible QQuickWidget. It redirects the existing QML root to an offscreen
QQuickRenderControl scene, hides the native Quick child, and composes its GPU
texture into the Widgets panel before that panel is projected. The QML scene
itself uses ordinary Qt Quick 2D items. Rendering does not use Qt Quick 3D or
per-frame framebuffer readback. Source, models, and QML state survive the
presentation change; unloading still follows the lifecycle below.

Gallery uses this layer for supported OpenGL composition, so QML and the
surrounding Widgets follow **Settings → 3D Gallery** together. Unsupported
composition or a texture failure retains the native 2D path without changing
the requested preference. Cached hidden pages do not prevent the visible page
from using 3D. Motion and contrast policy still govern the effective mode.

The layer belongs to `FluentQt::QmlBridge` and the matched Python
`fluentqt.qmlbridge` module. It borrows its root QWidget and operates on the GUI
thread. Set the selected page with `setRootWidget()` and call `synchronize()`
before installing capture effects. Already delegated cached views retain their
texture mode until `release()`; an unloaded view can accept a later explicit
load in that mode. `prepareFrame()` requires the compositor's OpenGL context
to be current and a finite device pixel ratio, source rectangle, and byte budget.
The source rectangle uses selected-page coordinates; map the panel origin into
that page when the panel and page origins differ. Reserve `requiredBytes()`
alongside the Widgets and particle caches before allocating a frame.
On Qt 5.15 and Qt 6.2, texture DPR must match the view's native window DPR;
Qt 6.3+ supports the explicit target DPR used by the cache sampling policy.
`compose()` accepts a resolved panel texture and returns a borrowed composed
texture. Treat texture IDs as invalid after release, resize, or context change.
`clearFrameCaches()` releases GPU targets while retaining scene delegation and
QML state. `release()` also restores native Quick presentation.
Handle `renderingFailed` by stopping ancestor captures before releasing the
layer and restoring native Quick presentation.

The lower-level `QmlView` texture methods expose the same rendering mode for
custom OpenGL hosts. They retain the original scene owner and forward input to
the delegated Quick window. Software Quick, incompatible graphics APIs, and
unsupported overlapping foreground widgets or effects require native
composition. Hosts must preserve their 2D content when texture setup fails.

The helper does not import OpenGL textures into a Direct3D or Metal RHI host.
Gallery preserves Spatial's platform default and the explicit
`FLUENT_QT_SPATIAL_BACKEND` preference. After creating `QApplication` and before
creating any window, it selects a matching Quick API: OpenGL for the GL host,
Direct3D11 for Windows RHI, or Metal for macOS RHI. Explicit Qt backend
environment settings are preserved; an incompatible combination disables 3D
composition and keeps the native QML page available. Without Spatial, Gallery
leaves Quick's backend selection unchanged. If RHI preflight fails, Gallery
tries OpenGL only when Quick can share that host API. Otherwise it uses 2D.
Gallery builds without QmlBridge retain their existing OpenGL fallback.

With an RHI host, QML pages use native 2D; other pages can still use RHI 3D.
The saved 3D preference and application models are retained. With an OpenGL
host, a failed QML frame preserves the last completed frame until queued 2D
restoration. A new or resized framebuffer draws the ordinary panels while
waiting for that restoration. Projected input pauses during this interval and
resumes with native presentation. Errors already pending on the borrowed GL
context are reported separately from errors raised during QML rendering.

Other hosts must choose compatible graphics APIs before Quick initialization.
This integration does not add QML support to the ordinary
`SpatialView::addWidget()` graphics-proxy path.

## Ownership and unloading

Each loaded view owns its internal Quick widget, engine, and scene. Context
models remain application-owned: binding neither reparents them nor transfers
their ownership to QML. They use C++ QML ownership and must remain on the GUI
thread. Destroying a bound model automatically unloads the corresponding view.
QML bindings should tolerate a null context object during teardown.
Passing null to `setContextObject()` removes that binding before loading.

`unload()` immediately removes the current Quick surface from the layout and
hides it, clears
`source` and `rootObject()`, and reports Unloaded. Context bindings remain for
an explicit later load. The scene and engine are scheduled for deferred deletion
on the next event-loop turn, after the current QML callback has returned. This
also applies to ordinary runtime callbacks, not only page completion. Use
`deleteLater()` to destroy hosts from QML callbacks, and allow normal
deferred-delete delivery before application teardown.

Session deactivation immediately rejects new loads and clears the tracked hosts'
page state, then releases their Quick resources through the deferred deletion
above. It does not delete their host QWidgets or caller models. Reactivation
permits loading again; it does not restore the previous source or reload old
pages. Unloading a page does not turn it into a native QWidget implementation of
that page. An application that needs
a 2D alternative supplies ordinary Fluent widgets as its fallback content.

Keep the session alive while its views are in use, and deactivate or destroy it
before dismantling its borrowed host. Do not parent a session to a tracked view
or an object in that view's subtree. All calls, context objects, and lifecycle
hooks use QApplication's GUI thread. The shared [Plugins contract](plugins.md)
owns dependency ordering, failure rollback, callback reentrancy, and generic
manager lifetime rules.

`QmlPlugin` exposes ID `fluentqt.qmlbridge` and widget key `qml-view` for an
application-managed plugin graph. `QmlSession` owns a local manager and plugin
for applications that need a scoped convenience entry. Both use the same linked
plugin lifecycle. Load trusted application QML; this host provides no script
sandbox or permission boundary.

## Fluent theme and motion

Each view supplies the read-only `fluentTheme` context object, also returned by
`themeObject()`. QML bindings observe its `changed` signal.

| Tokens | Properties |
|---|---|
| Colors | `background`, `surface`, `surfaceAlt`, `text`, `secondaryText`, `accent`, `onAccent`, `border`, `dark` |
| Typography and shape | `fontFamily`, `fontPixelSize`, `cornerRadius` |
| Motion | `animationsEnabled`, `transitionDuration` |

For an application model exposing a notifying `message` property, a small page
can opt into those tokens:

```qml
import QtQuick 2.15

Rectangle {
    implicitWidth: 320
    implicitHeight: 200
    color: fluentTheme.surface
    radius: fluentTheme.cornerRadius

    Text {
        anchors.centerIn: parent
        text: appModel ? appModel.message : ""
        color: fluentTheme.text
        font.family: fluentTheme.fontFamily
        font.pixelSize: fluentTheme.fontPixelSize
    }
}
```

The application chooses how its QML page uses theme and motion values. Existing
Quick controls are not automatically restyled. Bind animation enablement and
duration explicitly, and retain readable focus, contrast, and settled state
when motion is reduced or disabled. QmlBridge does not duplicate the FluentQt
Widget component library in Quick.

## Input and accessibility

Native presentation delegates focus to the internal QQuickWidget; texture
presentation forwards input to the delegated Quick window. The QML page owns
its keyboard behavior, focus order, Accessible names, roles, and actions;
ordinary Qt Quick accessibility semantics supply the scene interfaces.
Unloading removes the old scene rather than retaining stale interactive content.
Caller models and surrounding native controls keep their normal ownership.

The host's lifecycle and theme contracts do not certify platform screen-reader,
IME, or assistive-technology behavior for arbitrary QML. Review the actual page
and target platform, including High Contrast and Reduced/Disabled motion.
The owning contracts are in
[TestQmlBridge.cpp](../../tests/components/qmlbridge/TestQmlBridge.cpp) and the
[accessibility inventory](../development/accessibility-inventory.md).

Loading introduces the view's Quick engine and rendering resources in addition
to plugin lifecycle bookkeeping. Measure scene loading, rendering, and cleanup
on the target workload; source/API parity does not establish a performance
budget or cross-platform visual parity.

<!-- docs-nav:bottom:start -->
---
[← Particles: optional decorative effects](particles.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Window Chrome Architecture →](window-chrome.md)
<!-- docs-nav:bottom:end -->
