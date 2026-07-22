# High-DPI Workflow

Use this workflow when changing application startup, fonts, icons, raster
buffers, custom painting, window chrome, or display-change behavior.

## Integration Contract

Qt owns the conversion from logical widget coordinates to the current screen's
device pixels. FluentQt geometry and design tokens are logical DIPs; callers
must not multiply widget sizes, margins, font sizes, or saved window geometry by
the device-pixel ratio.

The host application has one startup responsibility: call the FluentQt helper
before constructing `QApplication`.

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

`prepareHighDpiApplication()` enables Qt 5 device-independent scaling and
high-resolution pixmaps and selects pass-through fractional scaling on Qt
5.15+/Qt 6. It is `void`: a late call logs a warning and leaves the already
created application unchanged. An application that deliberately needs another
rounding policy may override it after this call, but still before constructing
`QApplication`.

## FluentQt Paint Contract

FluentQt owns the parts that cannot be delegated to the host:

- custom-painted strokes are aligned from the active `QPainter` device
  transform, including fractional DPR and redirected/off-screen painting;
- rounded surfaces paint their fill and border from one device-aligned
  geometry, so adjacent widgets do not leave backing-store edge pixels;
- opaque or software-painted backdrop modes deterministically paint their
  complete owned surface; transparent clearing is restricted to the typed
  `CompositedTransparent` backdrop mode;
- ordinary child widgets rely on Qt's shared backing store rather than marking
  themselves as translucent top-level surfaces;
- raster assets are created for the target physical extent, tagged with the
  exact DPR, and invalidated after a display-scale change.

The private paint helpers live below `components/foundation/private`; they are
an implementation detail, not installed public API. Reusable controls should
use the shared surface path rather than adding platform- or Gallery-specific
pixel offsets.

## Gallery Window Placement

The Gallery follows the operating-system/Qt scale. It does not apply a second
application multiplier and does not restart to change scale.

Window sizes and saved placement remain in Qt logical coordinates. On first
launch the Gallery chooses a bounded fraction of the available screen. Saved
geometry is restored on the previous screen and clamped to the screen's current
available geometry, so a changed scale or disconnected display cannot strand
the window off-screen.

## Automated Fractional and Integer Checks

The `high_dpi` CTest label runs the same bootstrap, raster, paint-alignment, and
pure-FluentQt composition checks at 110%, 125%, 150%, 175%, 200%, and 300%.

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

These checks validate logical geometry, process policy, physical pixmap
density, device-aligned surface edges, and opaque component composition. They
do not replace a real mixed-monitor review.

## Manual Gallery Review

Close every running Gallery instance first because the application is
single-instance. Prefer native display settings for acceptance testing. A
`QT_SCALE_FACTOR` override is useful only as an isolated developer diagnostic;
it multiplies the native DPR rather than replacing it.

Windows example:

```powershell
$env:QT_SCALE_FACTOR='1.25'
.\build\vcpkg-windows\app\Debug\fluent_qt_gallery.exe
Remove-Item Env:QT_SCALE_FACTOR
```

Repeat at 1.1, 1.5, 1.75, 2, and 3. Also move the app between monitors with
different native scale factors and confirm that Qt updates the DPR without an
application restart.

Linux examples:

```bash
QT_SCALE_FACTOR=1.25 ./build/vcpkg-linux/app/fluent_qt_gallery
QT_SCALE_FACTOR=2 ./build/vcpkg-linux/app/fluent_qt_gallery
QT_SCALE_FACTOR=3 ./build/vcpkg-linux/app/fluent_qt_gallery
```

On macOS, validate normal Retina and scaled display modes in System Settings. A
forced factor on an already-Retina display is multiplicative and therefore a
stress test, not a simulation of the corresponding macOS “Looks like” choice.

Review all of the following:

- text and icon-font edges remain sharp;
- `QIcon` images and the title-bar app icon use the target physical density;
- control sizes remain usable and text is not clipped at every tested factor;
- source-code expand/collapse and scrolling remain stable;
- popup, flyout, tooltip, menu, navigation, and custom window-chrome edges have
  no bright or transparent seams;
- resize/move works before and after a monitor-DPR transition;
- moving between displays refreshes icons and materials without a size jump or
  stale low-resolution frame.

Applications with their own executable manifest remain responsible for keeping
its platform DPI declaration consistent with the Qt version they ship.

## Raster Asset Rule

Prefer font/SVG/vector painting for reusable component icons. When a raster
asset is unavoidable, request or generate it for the target physical extent,
set its device-pixel ratio, and invalidate the cached result after a display
scale change. Gallery-only artwork may use `@2x`/`@3x` variants, but reusable
FluentQt controls must not depend on Gallery assets.
