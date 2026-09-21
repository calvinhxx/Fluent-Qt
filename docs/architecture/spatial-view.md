# Spatial: compose existing widgets in depth

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Files and feedback](files-and-feedback.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md)
<!-- docs-nav:top:end -->

`Spatial` is a first-level component category in `src/components/spatial`, using
`fluent::spatial` and the installed `<FluentQt/Spatial.h>` header. Application
examples can include `<FluentQt/FluentQt.h>` when linked to the optional
`FluentQt::Spatial` target. The base `FluentQt::FluentQt` target keeps its
Widgets-only module dependencies.

## Two components, one ordinary widget composition

| Type | Responsibility | How to use it |
| --- | --- | --- |
| `SpatialView` | A QWidget viewport with perspective, camera and pointer response | Add it to your normal window layout |
| `SpatialItem` | Pose and optional surface finish for one widget and all its children | Use the QObject handle returned by `view->addWidget()` |

Build an ordinary `layout::Card` with `basicinput::Button`, `ToggleSwitch`,
`status_info::ProgressBar` or a chart. Add the complete card to the view. All
children move together, retaining their normal properties, signals and layouts.
Add separate cards as separate items when they need independent depth or rotation.

```text
Normal window layout
├── Navigation and parameter controls
└── SpatialView
    ├── SpatialItem → Card → Label + ToggleSwitch + Button
    └── SpatialItem → Card → Sparkline + ChartModel
```

This is useful for a small component showcase, a preview card or a few status
panels. Keep primary editing, navigation and large data views in normal layouts.
The API composes planar widgets; it does not load meshes, provide a stereoscopic
display, or automatically split a widget's paint layers.

## Minimal integration

### Ordinary widgets only

Use an installed SDK with this `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(spatial_card LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(FluentQt CONFIG REQUIRED)
add_executable(spatial_card main.cpp)
target_link_libraries(spatial_card PRIVATE FluentQt::FluentQt)
```

This `main.cpp` opens a Fluent window with an ordinary Card and Button:

```cpp
#include <FluentQt/FluentQt.h>
#include <QApplication>
#include <QVBoxLayout>

int main(int argc, char** argv)
{
    fluent::prepareHighDpiApplication();
    QApplication app(argc, argv);
    fluent::initializeResources();
    using namespace fluent;

    windowing::Window window;
    window.resize(640, 420);
    auto* card = new layout::Card;
    auto* content = new QVBoxLayout(card);
    auto* button = new basicinput::Button("Run", card);
    button->setFluentStyle(basicinput::Button::Accent);
    content->addWidget(button, 0, Qt::AlignCenter);
    QObject::connect(button, &basicinput::Button::clicked, button,
                     [button] { button->setText("Done"); });

    window.setContentWidget(card);
    window.show();
    return app.exec();
}
```

### The same widgets with Spatial

The SDK must have been built with `FLUENT_QT_BUILD_SPATIAL=ON`. Replace the
`find_package` and `target_link_libraries` lines with:

```cmake
find_package(FluentQt CONFIG REQUIRED COMPONENTS Spatial)
target_link_libraries(spatial_card PRIVATE FluentQt::Spatial)
```

`FluentQt::Spatial` links `FluentQt::FluentQt` publicly. It already provides the
ordinary widgets, so listing both targets is unnecessary. It also exports
`FLUENT_QT_HAS_SPATIAL=1`, enabling Spatial in `<FluentQt/FluentQt.h>`; do not set
that definition manually.

In the same `main.cpp`, replace `window.setContentWidget(card);` with:

```cpp
card->setFixedSize(300, 180);
auto* view = new spatial::SpatialView;
auto* item = view->addWidget(card, WidgetOwnership::Owned);
item->setRotation(QVector3D(6, -12, 0));
item->setSurfaceIntensity(0.6);
view->setMaximumTilt(QPointF(3, 5));
window.setContentWidget(view);
```

The Button and its signal connection are unchanged. The Window owns the view,
and the view owns the card. `view->setSpatialEnabled(false)` returns the same
card to a native 2D layout.

Build either variant from its directory:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/fluentqt-sdk;/path/to/Qt/kit"
cmake --build build
```

For source integration, replace `find_package` with
`add_subdirectory(path/to/Fluent-QT fluentqt)`. Configure with
`-DFLUENT_QT_BUILD_SPATIAL=ON` when linking Spatial; leave it off for the base
variant. The C++ code and target names stay the same. The repository's
[hello_world](../../examples/hello_world/README.md) provides separate 2D and
Spatial executable targets, each with its own window implementation. The base
example contains only a Window and Button. Interactive parameter demonstrations
live in Gallery.

## Try the Gallery

Use **Settings → 3D Gallery** to switch the shell and every Spatial example together.
Then open **Controls → Spatial**. The component pages link back to that setting.
There are no per-example mode switches, and switching modes preserves values and models.

Start with the main examples:

- **SpatialView / Scene perspective**: adjust camera distance, scene zoom and
  pointer following. The sliders stay outside the projection. Animation and
  rendering settings keep their defaults.
- **SpatialView / Two cards at different depths**: a settings card and a
  Sparkline card have separate poses. The chart retains its ordinary model.
- **SpatialView / File preview**: keep the dropdown
  outside the scene and connect its selection to the projected card.
- **SpatialItem / Card finish and pose**: the settings card contains just three
  sliders for surface intensity, Y rotation and Z position. The
  preview uses a real Sparkline. Settings keep their own pose while dragging;
  narrow boards stack the cards vertically.

**More component combinations** is collapsed by default. It contains Slider/CheckBox,
ListView/InfoBadge, CalendarView, SelectorBar with a text preview, RadioButton/Rating,
a file browser and DonutChart. These examples follow the same setting, including
when hidden or created after a mode change.

Source code opens on **Key usage**, a short Spatial integration excerpt with
explicit prerequisites. **Full example** includes the controls, layout, signals
and ownership. Copy follows the selected source. The file browser reuses the
Gallery [TreeRowDelegate](../../app/view/widgets/samples/CollectionSampleDelegates.h)
for its row styling; it is an application delegate, not a Spatial dependency.
The shared setting opens navigation and content into opposing surfaces while the
native title bar stays stable. Gallery checks GPU support and creates its shell
OpenGL surface only when 3D is first enabled, including a saved enabled preference.
Default 2D startup does neither. The startup Splash and logo handoff finish before
the shell redirects widget painting.

The shell redirects widget painting into two cached GPU textures through an
OpenGL painter. Content changes redraw the affected surface; pointer motion reuses
the textures. It does not capture and upload a full-page CPU bitmap each frame.
The panel caches use up to twice the output density in each axis to preserve
glyph detail through perspective filtering. Their combined allocation has a
192 MiB estimated budget (12 bytes per pixel, allowing for separate color,
depth and stencil storage). The planner also checks the context's texture,
renderbuffer and viewport limits. It reduces **extra** sampling in quarter steps
when necessary and never renders below the window's native pixel density.
Allocation failures retry smaller caches. If even native density cannot fit,
Gallery returns to 2D; the GPU remains available for another attempt after resizing
or freeing resources. This budget excludes Qt's final window framebuffer and
other application/GPU resources.

The panel caches have no multisample buffers; the final surface retains MSAA.
Both caches are released on return to 2D. The C++, Python and WebAssembly shells
use the same policy. Web keeps native output resolution by default; a lower
resolution remains an explicit choice.

On macOS, native style primitives that require CGContext use raster images.
Frames, buttons and base style options are painted in a local rectangle so a
small primitive far from the widget origin does not allocate a widget-sized
image. Other native option types retain their original coordinates. The rest of
the widget tree paints into the GPU texture. Transparent hosts retain their
background rather than acquiring a forced palette fill.

Returning to 2D stops capture and input redirection, clears cached textures, and
hides the GPU surface for reuse. After the first opt-in, Qt may retain the window's
GL composition resources until it closes. Live widgets keep their parents, focus
and values. Reduced motion and high contrast select 2D. Shell angles, animation
timing and antialiasing choices belong to
[`GallerySpatialController.cpp`](../../app/view/shell/GallerySpatialController.cpp),
not the library's API contract.
`SpatialView` uses a regular widget viewport under an ancestor graphics effect,
avoiding nested OpenGL surfaces while keeping controls and 3D poses live. In the
Gallery shell that viewport paints through the outer OpenGL painter, so its
perspective transform no longer needs an intermediate full-page CPU bitmap. A
regular raster capture in another application still uses software painting. It also
releases its GPU viewport while hidden or fully clipped and restores it when
visible. Applications no longer need a visibility/backend adapter for this.
`renderMode()` remains the requested policy; `activeBackend()` identifies the
view's own viewport. Under Gallery composition, `Raster` denotes its regular
widget viewport, whose redirected painter uses the outer GPU surface. A standalone
visible view can use its own OpenGL viewport normally.
Tab/Escape in a Gallery scene restores 2D throughout the Gallery for native input.
The original Home Hero remains intact. Web Gallery uses the same compositor
through WebGL and falls back to 2D if the renderer is unavailable. It checks the
actual canvas context instead of creating an offscreen desktop OpenGL probe.
See the [browser validation workflow](../development/webassembly-workflow.md#spatial-webgl-validation).

Python source builds can enable `fluentqt.spatial`; see the
[binding guide](../../bindings/pyside6/README.md#optional-spatial-module) and
[coverage reference](../../bindings/pyside6/ROADMAP.md#spatial-composition).
Python calls configure the same C++ scene and connect ordinary widget signals;
projection, caching and pointer animation stay in C++.

Mica/Acrylic and 3D have separate responsibilities: the window backdrop supplies
the bottom material, while the 3D compositor positions the navigation and content
surfaces above it. Their translucent areas and gutters reveal that backdrop;
opaque cards cover it. The compositor does not add another desktop blur.
Intro and other modal overlays stay above the compositor. Intro freezes pointer
following and maps its spotlight and CoachMark anchor to the displayed panel
bounds, using the active Left or Top navigation.

## Ownership and component composition

Hosted content inherits the view's local theme, while explicit content overrides
remain intact. Releasing content removes the temporary theme bridge.

`surfaceIntensity` adds a soft shadow and edge light around an intact rectangular
Card. `hoverLift` briefly raises it on pointer hover, without changing `position`.
The lift freezes while a button is held or a slider is dragged. Both effects
exist only in 3D; returning to 2D preserves the original Fluent widgets. Shadow
masks are cached and idle cards do not require an animation loop.

A Card's children share one plane. Do not register its already-hosted children
again as independent items. For a chart, `setModel()` still borrows its model;
Spatial does not change that contract. Parent an example model to its card if
both should share a lifetime.

The view owns each SpatialItem. `Borrowed` content is detached on release,
`Owned` content is deleted, and `Reparented` content returns to its original
parent without restoring the original layout slot. `takeWidget()` deletes the
item and transfers a parentless widget to the caller. Unsupported content
returns null without transferring ownership.

`setSpatialEnabled(false)` preserves the same controls and values, arranging
them in a native scrollable vertical layout. It does not reconstruct the
caller's previous custom grid. Keep a visible mode switch outside the scene.
Use 2D for text entry, popups and platform accessibility; native-window and
OpenGL child widgets cannot be embedded.

## Which components can use 3D?

Spatial transforms a QWidget subtree, rather than providing a second 3D version
of every component. Embedding successfully is not a guarantee for every interaction.

| Content | Current scope |
| --- | --- |
| Card, Label, Button, ToggleSwitch, CheckBox, Slider, RadioButton, RatingControl | Small painted compositions; projected pointer interaction is exercised in Gallery |
| ListView, TreeView, CalendarView, SelectorBar, StackContentHost | Small examples support selection, expansion, dates and tabs; keep editing and large datasets in native layouts |
| Sparkline, DonutChart | Existing caller-owned models drive the same views; other chart types use the shared renderer but require their own scenario review |
| LineEdit, text editors, ComboBox, menus, pickers, flyouts and dialogs | Keep IME, popup placement and keyboard-heavy workflows in the normal 2D layout; connect them to the 3D preview as the hybrid sample does |
| Native child windows, QOpenGLWidget, paint-on-screen surfaces | Rejected by `addWidget()`; this also applies when nested inside a card |
| SpatialView nested inside SpatialView; entire Window or NavigationView shells | Outside the supported composition scope |
| Models, themes, animations and other non-widget helpers | Continue using their normal APIs; they have no surface to transform |

This is a tested subset, not an all-components compatibility certification.
Qt also documents limitations for proxy widgets with an OpenGL viewport:
[QGraphicsView](https://doc.qt.io/qt-6/qgraphicsview.html).

## Public parameters

The website [API reference](https://calvinhxx.github.io/Fluent-Qt/api/?q=Spatial)
provides a collapsible parameter list generated from the public header comments.
These defaults belong to UILib; the example presets deliberately change some
of them. Slider ranges are convenient review ranges, not the full API limits.

| Scope | Parameter | Default | Meaning |
|---|---|---|---|
| Item | `position` | `(0, 0, 0)` | Pivot position in logical pixels relative to view center; X right, Y down, Z toward camera |
| Item | `rotation` | `(0, 0, 0)` | X/Y/Z degrees, applied in that order |
| Item | `scale` | `1` | Uniform scale, clamped to 0.05–8 |
| Item | `pivot` | `(0.5, 0.5)` | Normalized content anchor and rotation origin, each axis clamped to 0–1 |
| Item | `visible` | `true` | Visibility in both spatial and native layouts |
| Item | `surfaceIntensity` | `0` | Rounded-card shadow and edge light, clamped to 0–1; 3D only |
| Item | `hoverLift` | `0` | Temporary upward hover offset, clamped to 0–16 logical pixels; freezes during input |
| View | `cameraDistance` | `1000` | Perspective distance in logical pixels, clamped to 100–10000 |
| View | `zoom` | `1` | Whole-scene magnification, clamped to 0.1–4 |
| View | `pointerTrackingEnabled` | `true` | Pointer-driven scene tilt |
| View | `maximumTilt` | `(10, 16)` | Maximum pointer pitch/yaw in degrees, each clamped to 0–45 |
| View | `responseTime` | `140 ms` | Exponential follow time constant, 0 immediate; clamped to 0–1000 |
| View | `maximumFrameRate` | `60` | Pointer animation update cap, clamped to 15–120; not measured display FPS |
| View | `cacheEnabled` | `true` | Cache widget surfaces and repaint changed content |
| View | `renderMode` | `Auto` | Auto, raster, or OpenGL request; unavailable OpenGL falls back |
| View | `spatialEnabled` | `true` when permitted | Switch between perspective and native vertical layout |

Read-only `activeBackend`, `rendererName`, and `fallbackReason` report the
actual rendering path after the window is exposed. `projectedPolygon()` gives
current corners in view coordinates. Non-finite numeric inputs are ignored;
unchanged values emit no change signal. Use the item's visibility property
instead of hiding its root widget directly.

Start with position, rotation and maximumTilt. See the public API reference
for advanced pivot, backend and cache settings.

## Rendering and dependency boundary

Spatial is off by default in source builds. Enable it with
`-DFLUENT_QT_BUILD_SPATIAL=ON`; the development and Gallery packaging presets opt
in explicitly. Setting it to `OFF` also builds a 2D-only Gallery without Spatial
routes or an OpenGL canvas.

An ordinary `find_package(FluentQt CONFIG REQUIRED)` loads only the base target,
even when the SDK contains Spatial. It does not discover Qt OpenGLWidgets.
Qt itself may still link platform graphics libraries through QtGui, as it did
before Spatial; this split removes the additional Qt OpenGL module dependency.
A Spatial-enabled application must still deploy its Qt OpenGL runtime libraries;
CPU fallback cannot repair a missing loader dependency.

### How Gallery links Spatial

[CMakePresets.json](../../CMakePresets.json) enables
`FLUENT_QT_BUILD_SPATIAL` in the Gallery development and packaging presets.
The root build then creates `FluentQt::Spatial` through
[FluentQtSpatial.cmake](../../cmake/FluentQtSpatial.cmake).
[app/CMakeLists.txt](../../app/CMakeLists.txt) conditionally links it to both
`FluentQtGalleryCore` (the object library) and `fluent_qt_gallery` (the executable).
When Spatial is off, Gallery compiles its 2D fallback and omits Spatial samples.

Gallery also links `Qt6::OpenGLWidgets` directly to its core on Qt 6 because its
whole-window compositor uses `QOpenGLWidget` itself. Applications that only use
SpatialView do not need this extra CMake line. The component samples use
SpatialView; the Gallery shell has a separate compositor. Both follow the one
Settings mode. The shell compositor remains Gallery application code, not a
public Spatial API.

The [Python Gallery](../../bindings/pyside6/gallery/README.md#spatial-mode)
follows the same boundary: `fluentqt.spatial` supplies the native SpatialView and
SpatialItem bindings, while a Python shell controller owns the shared OpenGL
canvas, projected input and overlay ordering. Its Settings switch drives both
the shell and the same eleven live examples. The C++, Python and WebAssembly
Galleries share their sample catalog and display complete Python example source.
Installing only the base Python binding leaves Spatial routes and Qt OpenGL
imports out of the Gallery runtime.

Gallery requires acceleration for its whole-window composition. It checks the
renderer before creating the canvas and again after the real canvas initializes.
Unavailable/software rendering or context loss restores the original native 2D
widgets and stops both transition and pointer animations. The Settings switch
shows the effective availability. All Spatial examples follow that effective
mode; the saved preference remains available when acceleration returns. Set `FLUENT_QT_GALLERY_DISABLE_3D=1` to
start in 2D without creating a shell OpenGL canvas for troubleshooting.


The library maps each widget's rectangle through rotation, translation, and
perspective, then applies the resulting projective transform to a private
Qt Graphics View proxy. Qt maps pointer input back to the live widget. With
OpenGL, cached surfaces are composed by Qt's OpenGL paint engine. Widget
painting, layout, input handling, and transform calculation still use CPU time.

Only the optional Spatial target requires OpenGL support. Qt 6 uses
`OpenGLWidgets` and its transitive `OpenGL` dependency. Qt 5.15
uses `QOpenGLWidget` from `Widgets`; no legacy QtOpenGL module is required.
No Qt Quick, Qt3D, or third-party rendering library is added. OpenGL viewport
creation waits until SpatialView is visible with 3D enabled. Constructing hidden
pages or configuring their contents does not change the host's native surface.
The source uses the Qt 5.15 / 6.2 API boundary; passing the current host checks
does not replace the other Qt/platform CI lanes.

Auto selects raster for offscreen/minimal/VNC, failed context initialization,
or known software OpenGL renderers. `FLUENT_QT_SPATIAL_RENDERER=raster` overrides
Auto for diagnostics. Explicit OpenGL requests also fall back on failure.
Raster describes this view's drawing path, not the operating system compositor.
No global Qt attributes, surface format, or pixmap cache limit are changed.

Pointer motion stops once settled, when hidden, or when the application loses
activation. A held pointer interaction freezes the transform; backend and
presentation changes wait until that interaction finishes. Camera-plane
crossings and degenerate projections are hidden instead of drawing invalid
geometry. Overlapping planes are ordered by their transformed center depth;
intersecting meshes, volumetric lighting, and stereoscopic displays are outside
this component's scope.

## Native layout and review boundary

`setSpatialEnabled(false)` moves the same widgets into a native scrollable
vertical layout. Values and objects survive the transition. Tab/Backtab/Escape
from the scene, reduced/disabled motion, and high contrast select this mode.
Returning to full motion does not automatically re-enable spatial presentation.
Applications should expose a visible mode switch, as the example does.

Use native mode for text input, popups, and platform assistive technology.
Widgets with native windows, paint-on-screen behavior, or embedded OpenGL
children are rejected. Add such application content outside SpatialView.
Small pointer-driven card compositions are the intended scope; complex forms,
large model/view collections, and arbitrary overlay trees need separate review.

Focused contracts live in
[TestSpatialView.cpp](../../tests/components/spatial/TestSpatialView.cpp):

```bash
python3 tools/dev/fluent_qt_build.py --preset vcpkg-osx --target test_spatial_view
ctest --preset vcpkg-osx -L '^test_spatial_view$' --output-on-failure
```

Native visual acceptance is still human-required. Inspect light/dark, normal
and narrow windows, real control input, parameter changes, and the mode switch.
Qt 5.15, Windows, Linux, screen readers, and IME require their own runtime
evidence before broad compatibility claims. The Web build has a separate
[WebGL runtime check](../development/webassembly-workflow.md#spatial-webgl-validation)
for pointer input, idle rendering, mode changes, and software-renderer fallback.

For Python, run `bindings/pyside6/tools/benchmark_spatial.py --output <directory>`
with a Spatial-enabled package in a native desktop session. It records the
renderer, frame submission intervals, process CPU time, and idle frame count.
Keep the benchmark window active and run one workload at a time. A single-card
measurement and the whole Gallery compositor have different costs; compare
matching workloads, window sizes, and device pixel ratios.

## Run the Gallery

```bash
python3 tools/dev/fluent_qt_build.py --preset vcpkg-osx --target fluent_qt_gallery
open "build/vcpkg-osx/app/Fluent-Qt Gallery.app"
```

Enable **Settings → 3D Gallery**, then open **Spatial → SpatialView** or
**Spatial → SpatialItem**. The one Gallery setting controls its shell and Spatial
examples. Other native hosts use their configured preset. The Gallery owns the
interactive examples; `examples/hello_world` is the minimal library consumer.

<!-- docs-nav:bottom:start -->
---
[← Files and feedback](files-and-feedback.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md)
<!-- docs-nav:bottom:end -->
