# FluentQt widgets for Qt Designer

Search the Widget Box for `Fluent`. Controls are grouped by their Gallery category and use the matching Gallery icon. Drag a control onto the form, edit its properties, and save it. C++ and PySide6 use the same class names and can open the same `.ui` file when both have the required components installed.

<p>
  <img src="../../docs/assets/readme/qt-designer.webp" width="760" alt="Qt Designer showing the FluentQt Widget Box, live Workspace Preferences form, and FluentButton Property Editor">
</p>

With all modules installed, the two designers cover the Gallery's 99 entries: 95 Widget Box entries and four New Form templates. The core build includes 92 Widget Box entries and three templates. Particles adds `FluentParticleBackdrop`, Spatial adds `FluentSpatialView` and the SpatialItem template, and QmlBridge adds `FluentQmlView`.

## Using the Widget Box

The 84 embeddable controls use their normal form classes. Use `FluentCard` for child controls and layouts. Field accepts one editor, and Expander and ScrollView accept a content widget.
Studio form exports use these slots and standard Qt grid and box layouts.
List, chart and DataGrid form properties store sample data with the form. A DataGrid
uses its own sample model for the column headers and rows until the application
supplies its model. Exported pages inherit the host theme and accent.

Eleven entries have a `Button` suffix because the button supplies a trigger and anchor for a real FluentQt surface. Edit the button caption with `text`. In form preview, click it to try the component.

| Widget Box entry | Designer properties |
|---|---|
| `FluentPopupButton`, `FluentFlyoutButton`, `FluentTeachingTipButton`, `FluentCoachMarkButton`, `FluentDrawerViewButton` | `surfaceText` sets the displayed message. |
| `FluentToastButton`, `FluentSplashScreenButton` | `surfaceText` sets the message or splash title. `duration` sets the display time in milliseconds. Use zero to dismiss in code. `durationChanged(int)` reports changes. |
| `FluentToolTipButton` | `surfaceText` sets the tooltip. Hovering or clicking shows it. |
| `FluentMenuButton`, `FluentCommandBarFlyoutButton`, `FluentCommandPaletteButton` | Edit the action labels with `actionTexts`. Connect `actionTriggered(QString)` to handle the chosen action. |

Each button also provides the `openSurface()` slot and a `surface()` accessor for configuring the underlying component in application code. The button manages its lifetime. The sample form uses a menu selection to set the project name and a separate button to test a notification.

## Starting from a template

In the Qt Designer form template chooser, select **FluentQt Window**, **FluentQt Dialog**, **FluentQt ContentDialog** or **FluentQt SpatialItem**. You can also open the corresponding file from [templates](templates/) and save a copy. Edit the body using ordinary layouts. Designer's built-in preview shows that editable body.

The runtime helper reads the body's `fluentFormHost` property and attaches it to its native host. Window uses the body's `windowTitle`. Dialog connects the standard `buttonBox` accept and reject actions. ContentDialog uses `windowTitle`, `fluentPrimaryText` and `fluentCloseText`. SpatialItem puts the body inside a SpatialView and applies `fluentItemScale` and `fluentItemRotationY`. Use `addSpatialForm(view, body)` in C++ or `add_spatial_form(view, body)` in Python to add an edited item to an existing scene.

For a generated C++ form, pass a heap-allocated body after `setupUi()`. The returned host owns the body. Dialogs also accept an existing window as the second argument to `makeFormHost()`.

```cpp
#include "DesignerForms.h"
#include "ui_Window.h"

auto* body = new QWidget;
Ui::WindowContent form;
form.setupUi(body);
auto* window = fluentqt_designer::makeFormHost(body);
fluentqt_designer::showFormHost(window);
```

For generated Python forms, call `make_form_host(body, owner=None)` and `show_form_host(host)` from `fluentqt_designer_forms`. Keep the returned host for the lifetime of the window. The included preview runners do this for you.

## C++ with Qt Creator

Use a FluentQt SDK and a Qt kit compatible with the editor. Configure and build from the repository or extracted source archive root.

```sh
cmake -S tools/designer -B build/designer -DCMAKE_PREFIX_PATH="/path/to/FluentQt;/path/to/Qt"
cmake --build build/designer --config Release
```

On macOS or Linux, launch Qt Creator from the same directory.

```sh
QT_PLUGIN_PATH="$PWD/build/designer/plugins" qtcreator tools/designer/preview.ui
```

In a Windows Qt kit shell, launch Designer with the plugin directory set for this PowerShell session.

```powershell
$env:QT_PLUGIN_PATH = (Resolve-Path build/designer/plugins).Path
designer tools/designer/preview.ui
```

To include optional controls, add `-DFLUENTQT_DESIGNER_EXTENSIONS="Particles;Spatial;QmlBridge"` when configuring. Request only components installed with the SDK. Configuration uses Python 3.9+ with its standard library.

Run `build/designer/fluentqt_form_preview ContentDialog` to try a template on a single-configuration build. The executable also accepts `Window`, `Dialog` and `SpatialItem`.

Start a fresh editor process with this environment. Standalone Qt Designer can use the same plugin when its host allows local plugins. On macOS the signed SDK Designer may reject plugins signed by a different team. Use the Qt Creator design editor in that case.

For C++ applications using AUTOUIC, add this directory with `add_subdirectory()` and link `fluentqt_form_widgets`. It supplies the generated `fluentqt_designer.h` and the Qt meta-objects used by saved forms. Configure with `FLUENTQT_BUILD_DESIGNER_PLUGIN=OFF` if you only need the form runtime.

## Python with PySide6

In a Python environment with PySide6 and matching FluentQt bindings, run the following from the repository or extracted source archive root on macOS or Linux:

```sh
PYSIDE_DESIGNER_PLUGINS="$PWD/tools/designer" \
  PYTHONPATH="$PWD/tools/designer:$(python -c 'import os, site; print(os.pathsep.join(site.getsitepackages()))')" \
  pyside6-designer tools/designer/preview.ui
python tools/designer/preview.py tools/designer/preview.ui
python tools/designer/preview.py tools/designer/templates/ContentDialog.ui
```

Including the active environment's site-packages lets older Linux Designer hosts find the installed bindings. In an activated Windows PowerShell environment, run the following commands.

```powershell
$env:PYSIDE_DESIGNER_PLUGINS = (Resolve-Path tools/designer).Path
$env:PYTHONPATH = $env:PYSIDE_DESIGNER_PLUGINS
pyside6-designer tools/designer/preview.ui
```

The toolbox includes the optional modules supplied by that installation. The form runner loads the classes used in the form. To generate a Python module, put `tools/designer` on the application's import path and run:

```sh
python tools/designer/compile_ui.py tools/designer/preview.ui -o ui_preview.py
```

This uses the active environment's `pyside6-uic`. Qt 6.2's Python generator emits C++ expressions for string lists such as chart series and action labels. The helper converts those expressions to Python lists and preserves the generated translations.

## Updating the catalog

Run `python tools/designer/prepare_catalog.py` with PySide6 to refresh the inventory and the 72 × 72 PNGs from the Gallery catalog and its [SVG artwork](../gallery/artwork). Both designers use PNGs because the Widget Box expects icons with declared pixel sizes. The generator records a fingerprint of each SVG, the shared exporter and its PNG in `artwork-fingerprints.json`. It renders only stale icons. Run the same command with `--check` to verify the committed inputs and images across Qt versions and to check the Python template resources against their editable forms.

With matching FluentQt bindings installed, run `python -m unittest discover -s tools/designer/tests` to load each available control and exercise the sample form connection.
