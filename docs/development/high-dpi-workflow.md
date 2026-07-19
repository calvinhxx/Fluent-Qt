# High-DPI Workflow

Use this workflow when changing application startup, fonts, icons, raster
buffers, custom window chrome, or display-scale behavior.

## Integration Contract

High-DPI support is shared between FluentQt and the host application. FluentQt
owns device-independent component geometry, DPR-aware raster buffers, icon
rendering, and display-change cache invalidation. The host still owns process
startup and any platform-specific packaging metadata.

Call the startup helper before constructing `QApplication`:

```cpp
#include <FluentQt/FluentQt.h>
#include <QApplication>

int main(int argc, char** argv)
{
    fluent::prepareHighDpiApplication();
    QApplication app(argc, argv);
    fluent::initializeResources();
    // ...
    return app.exec();
}
```

`prepareHighDpiApplication()` enables the Qt 5 scaling and high-resolution
pixmap attributes and uses pass-through fractional scaling on Qt 5.15+/Qt 6.
An application that deliberately needs another rounding policy may override it
after this call, but still before constructing `QApplication`.

## Gallery Window and Display Scale

The Gallery sizes and restores its top-level window in Qt logical pixels. On a
first launch it uses roughly 72% of the screen width and 78% of the available
height, with sensible minimum and maximum bounds. Saved geometry is restored on
the previous screen, then clamped to the screen's current available geometry.
This keeps the window usable when the operating-system scale changes or a saved
monitor is disconnected.

`Settings > Display scale` is an optional Gallery-only multiplier on top of the
operating-system scale. `Follow system (100%)` is the default and is the right
choice for most users. Changing the value saves it immediately and opens a
`ContentDialog` with `Restart now` and `Later`; the selected scale is applied
before the next `QApplication` is constructed so widgets, dialogs, fonts, and
icons all use one coherent scale from their first frame.

The Gallery intentionally exposes only 100% and larger multipliers. Several Qt
platform backends clamp raster DPR to 1 while still shrinking the top-level
surface for a factor below 1; on WSLg/Wayland that combination produces clipped
content rather than a uniformly smaller UI. Older persisted 75%/85%/90% values
are migrated to `Follow system (100%)` at startup.

The Gallery preference does not replace the host application's normal DPI
integration and does not change the desktop environment. UILib applications
can reuse `prepareHighDpiApplication()`, while an
application-specific scale selector, restart policy, and window persistence
remain host-app concerns.

## Automated 200% and 300% Checks

The `high_dpi` CTest label runs the same focused component checks with exact 2x
and 3x offscreen scale factors:

```powershell
cmake --preset vcpkg-windows
cmake --build --preset vcpkg-windows --target test_high_dpi
ctest --preset vcpkg-windows -L '^high_dpi$' --output-on-failure
```

```bash
cmake --preset vcpkg-linux
cmake --build --preset vcpkg-linux --target test_high_dpi
ctest --preset vcpkg-linux -L '^high_dpi$' --output-on-failure
```

```bash
cmake --preset vcpkg-osx
cmake --build --preset vcpkg-osx --target test_high_dpi
ctest --preset vcpkg-osx -L '^high_dpi$' --output-on-failure
```

These checks validate logical window geometry, process policy, physical pixmap
density, and QWidget capture density. They do not replace a real mixed-monitor
review.

## Manual Gallery Review

Close every running Gallery instance first because the application is
single-instance. On a 100% Windows display, force an isolated 2x or 3x review:

```powershell
$env:QT_SCALE_FACTOR='2'
.\build\vcpkg-windows\app\Debug\fluent_qt_gallery.exe
Remove-Item Env:QT_SCALE_FACTOR
```

Repeat with `QT_SCALE_FACTOR=3`. If the operating system is already scaled,
prefer changing the native display setting to 200%/300%: `QT_SCALE_FACTOR`
multiplies the native DPR rather than replacing it.

Also validate the user-facing path at native scale:

1. Open `Settings` and change `Display scale` from `Follow system (100%)` to
   `125%`.
2. Confirm the restart dialog appears immediately and that `Later` keeps the
   current process unchanged.
3. Change the value again, choose `Restart now`, and confirm a new Gallery
   process opens at the selected scale without a second-instance warning.
4. Return the setting to `Follow system (100%)` and restart.
5. Resize and move the Gallery, restart it, then disconnect or change the scale
   of that display and confirm the restored window remains fully reachable.

Linux uses the equivalent commands:

```bash
QT_SCALE_FACTOR=2 ./build/vcpkg-linux/app/fluent_qt_gallery
QT_SCALE_FACTOR=3 ./build/vcpkg-linux/app/fluent_qt_gallery
```

On macOS, validate normal Retina and scaled display modes through System
Settings. A forced `QT_SCALE_FACTOR=2` on a native 2x Retina display produces an
effective 4x scale, so it is useful as a stress test but not as a normal 200%
simulation.

Review all of the following:

- text and icon-font edges remain sharp;
- `QIcon` button images and the title-bar app icon are not upscaled from 1x;
- control sizes remain usable and text is not clipped at 200% or 300%;
- source-code expand/collapse and scrolling remain stable;
- popup, flyout, tooltip, and custom window-chrome geometry remains attached;
- moving the window between displays refreshes icons and materials without a
  size jump or stale low-resolution frame.

For Windows packaging review, move the running app between monitors with
different native scale factors and confirm that Qt reports the new DPR without
restarting. Applications with their own executable manifest remain responsible
for keeping its DPI declaration consistent with the Qt version they ship.

## Raster Asset Rule

Prefer font/SVG/vector painting for reusable component icons. When a raster
asset is unavoidable, request or generate it for the target physical extent,
set its device pixel ratio, and invalidate the cached result after a display
scale change. Gallery-only artwork may use `@2x`/`@3x` variants, but reusable
FluentQt controls must not depend on Gallery assets.
