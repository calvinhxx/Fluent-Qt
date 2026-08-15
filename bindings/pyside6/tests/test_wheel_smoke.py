"""Smoke-test an installed FluentQt wheel without using the source tree."""

import gc
import ast
from importlib import metadata
import os
from pathlib import Path
import sys
import weakref

import fluentqt
import fluentqt._fluentqt as native
import fluentqt.basicinput as basicinput
import fluentqt.collections as collections
import fluentqt.date_time as date_time
import fluentqt.dialogs_flyouts as dialogs_flyouts
import fluentqt.menus_toolbars as menus_toolbars
import fluentqt.navigation as navigation
import fluentqt.status_info as status_info
import fluentqt.textfields as textfields
import fluentqt.windowing as windowing
import PySide6
import shiboken6

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import (
    QAbstractListModel,
    QCoreApplication,
    QDate,
    QEvent,
    QEventLoop,
    QItemSelectionModel,
    QModelIndex,
    QPoint,
    QRectF,
    QSize,
    QSizeF,
    QStringListModel,
    QTime,
    QTimer,
    Qt,
    qVersion,
)
from PySide6.QtGui import QAction, QColor, QStandardItem, QStandardItemModel
from PySide6.QtTest import QTest
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QLineEdit,
    QListView,
    QMenu,
    QMenuBar,
    QStyledItemDelegate,
    QWidget,
    QWidgetAction,
)
from shiboken6 import Shiboken


def wait_for_events(duration_ms):
    loop = QEventLoop()
    QTimer.singleShot(duration_ms, loop.quit)
    loop.exec()


def require_installed_below_prefix(path):
    prefix = Path(sys.prefix).resolve()
    resolved = Path(path).resolve()
    try:
        common = os.path.commonpath((str(prefix), str(resolved)))
    except ValueError:
        common = ""
    if os.path.normcase(common) != os.path.normcase(str(prefix)):
        raise AssertionError(
            "Expected {0} below clean environment {1}".format(resolved, prefix)
        )


def windows_loaded_modules():
    if sys.platform != "win32":
        return []

    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    get_current_process = kernel32.GetCurrentProcess
    get_current_process.argtypes = ()
    get_current_process.restype = wintypes.HANDLE
    process = get_current_process()
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    enum_modules = psapi.EnumProcessModules
    enum_modules.argtypes = (
        wintypes.HANDLE,
        ctypes.POINTER(wintypes.HMODULE),
        wintypes.DWORD,
        ctypes.POINTER(wintypes.DWORD),
    )
    enum_modules.restype = wintypes.BOOL
    module_filename = psapi.GetModuleFileNameExW
    module_filename.argtypes = (
        wintypes.HANDLE,
        wintypes.HMODULE,
        wintypes.LPWSTR,
        wintypes.DWORD,
    )
    module_filename.restype = wintypes.DWORD

    capacity = 256
    while True:
        modules = (wintypes.HMODULE * capacity)()
        needed = wintypes.DWORD()
        if not enum_modules(
            process,
            modules,
            ctypes.sizeof(modules),
            ctypes.byref(needed),
        ):
            raise ctypes.WinError(ctypes.get_last_error())
        count = needed.value // ctypes.sizeof(wintypes.HMODULE)
        if count <= capacity:
            break
        capacity = count

    paths = []
    for module in modules[:count]:
        buffer = ctypes.create_unicode_buffer(32768)
        if module_filename(process, module, buffer, len(buffer)):
            paths.append(Path(buffer.value).resolve())
    return paths


def verify_windows_runtime_dependencies():
    if sys.platform != "win32":
        return

    loaded = windows_loaded_modules()
    by_name = {path.name.lower(): path for path in loaded}
    required_qt = ("qt6core.dll", "qt6gui.dll", "qt6widgets.dll")
    missing_qt = [name for name in required_qt if name not in by_name]
    if missing_qt:
        raise AssertionError(
            "Expected loaded Qt dependencies: {0}".format(
                ", ".join(missing_qt)
            )
        )

    runtime_paths = [by_name[name] for name in required_qt]
    for prefix in ("pyside6", "shiboken6"):
        matches = [
            path
            for name, path in by_name.items()
            if name.startswith(prefix) and name.endswith(".dll")
        ]
        if not matches:
            raise AssertionError(
                "Expected a loaded {0} runtime DLL".format(prefix)
            )
        runtime_paths.extend(matches)

    runtime_paths = sorted(set(runtime_paths), key=lambda path: str(path).lower())
    for path in runtime_paths:
        require_installed_below_prefix(path)
        print("Windows wheel dependency: {0}".format(path))


def macos_loaded_images():
    if sys.platform != "darwin":
        return []

    import ctypes

    dyld = ctypes.CDLL(None)
    image_count = dyld._dyld_image_count
    image_count.argtypes = ()
    image_count.restype = ctypes.c_uint32
    image_name = dyld._dyld_get_image_name
    image_name.argtypes = (ctypes.c_uint32,)
    image_name.restype = ctypes.c_char_p

    paths = []
    for index in range(image_count()):
        value = image_name(index)
        if value:
            paths.append(Path(os.fsdecode(value)).resolve())
    return paths


def verify_macos_runtime_dependencies():
    if sys.platform != "darwin":
        return

    loaded = macos_loaded_images()
    requirements = {
        "QtCore": lambda path: (
            path.name == "QtCore" and "QtCore.framework" in path.parts
        ),
        "QtGui": lambda path: (
            path.name == "QtGui" and "QtGui.framework" in path.parts
        ),
        "QtWidgets": lambda path: (
            path.name == "QtWidgets" and "QtWidgets.framework" in path.parts
        ),
        "PySide6": lambda path: (
            path.name.startswith("libpyside6") and path.suffix == ".dylib"
        ),
        "Shiboken6": lambda path: (
            path.name.startswith("libshiboken6") and path.suffix == ".dylib"
        ),
    }

    runtime_paths = []
    for name, predicate in requirements.items():
        matches = [path for path in loaded if predicate(path)]
        if not matches:
            raise AssertionError(
                "Expected a loaded {0} runtime dependency".format(name)
            )
        runtime_paths.extend(matches)

    runtime_paths = sorted(set(runtime_paths), key=lambda path: str(path).lower())
    for path in runtime_paths:
        require_installed_below_prefix(path)
        print("macOS wheel dependency: {0}".format(path))


def report_stage(name):
    print("FluentQt wheel smoke stage: {0}".format(name), flush=True)


def main():
    require_installed_below_prefix(fluentqt.__file__)
    require_installed_below_prefix(native.__file__)

    package_dir = Path(fluentqt.__file__).resolve().parent
    if (package_dir / "gallery").exists():
        raise AssertionError("Reusable FluentQt wheel contains Gallery files")
    expected_stubs = {
        "__init__.pyi",
        "_fluentqt.pyi",
        "basicinput.pyi",
        "collections.pyi",
        "date_time.pyi",
        "design.pyi",
        "dialogs_flyouts.pyi",
        "foundation.pyi",
        "layout.pyi",
        "menus_toolbars.pyi",
        "navigation.pyi",
        "scrolling.pyi",
        "status_info.pyi",
        "textfields.pyi",
        "windowing.pyi",
    }
    installed_stubs = {path.name for path in package_dir.glob("*.pyi")}
    if installed_stubs != expected_stubs:
        raise AssertionError(
            "Installed wheel type stubs differ: expected {0}, found {1}".format(
                sorted(expected_stubs),
                sorted(installed_stubs),
            )
        )
    for stub_name in sorted(expected_stubs):
        stub_path = package_dir / stub_name
        ast.parse(stub_path.read_text(encoding="utf-8"), filename=str(stub_path))
    if not (package_dir / "_icon_aliases.json").is_file():
        raise AssertionError("Installed semantic icon alias data is missing")
    if fluentqt.Typography.Icons.Add != "ic_fluent_add_20_regular":
        raise AssertionError("Installed semantic icon facade is invalid")
    if fluentqt.Spacing.Border.Focused != 2:
        raise AssertionError("Installed spacing facade is invalid")

    expected_version = os.environ["FLUENTQT_EXPECTED_VERSION"]
    if metadata.version("FluentQt") != expected_version:
        raise AssertionError("Installed wheel metadata has the wrong version")
    if fluentqt.__version__ != expected_version:
        raise AssertionError("Public package version does not match the wheel")
    if fluentqt.__api_version__ != ".".join(expected_version.split(".")[:2]):
        raise AssertionError("Public API version does not match the wheel")

    app = QApplication.instance() or QApplication([])
    if not fluentqt.initialize_resources():
        raise AssertionError("FluentQt resources could not be initialized")

    info = fluentqt.binding_build_info()
    if info["fluentqt_version"] != expected_version:
        raise AssertionError("Native FluentQt version does not match the wheel")
    if info["pyside6_version"] != PySide6.__version__:
        raise AssertionError("PySide6 build and runtime versions differ")
    if info["shiboken6_version"] != shiboken6.__version__:
        raise AssertionError("Shiboken6 build and runtime versions differ")
    if info["qt_compile_version"] != qVersion():
        raise AssertionError("Qt build and runtime versions differ")

    report_stage("public controls")
    controls = [
        fluentqt.Accordion(),
        fluentqt.AnnotatedScrollBar(),
        fluentqt.Avatar("Ada Lovelace"),
        fluentqt.Button("Button"),
        fluentqt.CalendarDatePicker(),
        fluentqt.CalendarView(),
        fluentqt.CheckBox("CheckBox"),
        fluentqt.CoachMark(),
        fluentqt.ColorPicker(),
        fluentqt.ComboBox(),
        fluentqt.CompoundButton("Install", "Download and restart"),
        fluentqt.DropDownButton("DropDownButton"),
        fluentqt.FontIcon("ic_fluent_settings_20_regular"),
        fluentqt.HyperlinkButton("HyperlinkButton"),
        fluentqt.RadioButton("RadioButton"),
        fluentqt.RepeatButton("RepeatButton"),
        fluentqt.Slider(Qt.Horizontal),
        fluentqt.SplitButton("SplitButton"),
        fluentqt.ToggleButton("ToggleButton"),
        fluentqt.ToggleSplitButton("ToggleSplitButton"),
        fluentqt.ToggleSwitch(),
        fluentqt.Label("Label"),
        fluentqt.LineEdit(),
        fluentqt.AutoSuggestBox(),
        fluentqt.NumberBox(),
        fluentqt.PasswordBox(),
        fluentqt.TextEdit(),
        fluentqt.TimePicker(),
        fluentqt.InfoBadge(),
        fluentqt.InfoBar(),
        fluentqt.ProgressBar(),
        fluentqt.ProgressRing(),
        fluentqt.RatingControl(),
        fluentqt.Shimmer(),
        fluentqt.Toast(),
        fluentqt.ToolTip(),
        fluentqt.Card(),
        fluentqt.Divider(),
        fluentqt.Dialog(),
        fluentqt.ContentDialog(),
        fluentqt.DatePicker(),
        fluentqt.DrawerView(),
        fluentqt.Flyout(),
        fluentqt.Popup(),
        fluentqt.Expander(),
        fluentqt.Field(),
        fluentqt.FlipView(),
        fluentqt.FlowView(),
        fluentqt.GridView(),
        fluentqt.ListView(),
        fluentqt.NavigationView(),
        fluentqt.SplitView(),
        fluentqt.StackContentHost(),
        fluentqt.TreeView(),
        fluentqt.PipsPager(),
        fluentqt.Pivot(),
        fluentqt.ScrollBar(Qt.Horizontal),
        fluentqt.SelectorBar(),
        fluentqt.TabView(),
        fluentqt.TeachingTip(),
    ]
    if any(not Shiboken.isValid(control) for control in controls):
        raise AssertionError("A wheel-installed component has an invalid wrapper")

    shimmer = next(
        control for control in controls if isinstance(control, fluentqt.Shimmer)
    )
    shimmer_elements = [
        fluentqt.Shimmer.Element(
            fluentqt.Shimmer.Shape.Circle,
            QRectF(8, 8, 32, 32),
        ),
        fluentqt.Shimmer.Element(
            fluentqt.Shimmer.Shape.RoundedRect,
            QRectF(52, 8, 180, 24),
            6,
        ),
    ]
    shimmer.setElements(shimmer_elements)
    if shimmer.elements() != shimmer_elements:
        raise AssertionError("Shimmer did not preserve typed custom elements")
    shimmer.clearElements()
    if shimmer.elements():
        raise AssertionError("Shimmer did not clear typed custom elements")

    editing_scope = QWidget()
    editing_target = fluentqt.LineEdit(editing_scope)
    editing_target.setText("wheel command")
    editing_target.setGeometry(8, 8, 220, 36)
    editing_router = fluentqt.EditingCommandRouter(
        editing_scope, editing_scope
    )
    editing_scope.show()
    editing_target.setFocus(Qt.FocusReason.OtherFocusReason)
    app.processEvents()
    editing_router.refresh()
    if not editing_router.hasActiveTarget():
        raise AssertionError("EditingCommandRouter did not find the focused editor")
    if not editing_router.execute(
        fluentqt.EditingCommandRouter.Command.SelectAll
    ):
        raise AssertionError("EditingCommandRouter could not select editor text")
    editing_router.refresh()
    if not editing_router.execute(fluentqt.EditingCommandRouter.Command.Copy):
        raise AssertionError("EditingCommandRouter could not copy editor text")
    if QApplication.clipboard().text() != "wheel command":
        raise AssertionError("EditingCommandRouter copied the wrong text")
    editing_scope.close()

    class WheelZoomCanvas(fluentqt.ScrollViewZoomAwareWidget):
        def __init__(self):
            super().__init__()
            self.factors = []

        def scrollViewUnscaledSize(self):
            return QSizeF(560, 360)

        def setScrollViewZoomFactor(self, factor):
            self.factors.append(float(factor))
            self.resize(round(560 * factor), round(360 * factor))

    zoom_view = fluentqt.ScrollView()
    zoom_view.setZoomMode(fluentqt.ScrollView.ZoomMode.Enabled)
    zoom_canvas = WheelZoomCanvas()
    zoom_view.setOwnedContentWidget(zoom_canvas)
    zoom_view.zoomTo(1.5, False)
    if (
        zoom_canvas.factors != [1.0, 1.5]
        or zoom_canvas.size() != QSize(840, 540)
    ):
        raise AssertionError(
            "ScrollView did not dispatch through ScrollViewZoomAwareWidget"
        )
    plain_flow_view = next(
        control
        for control in controls
        if isinstance(control, fluentqt.FlowView)
    )
    controls.remove(plain_flow_view)
    plain_flow_view_ref = weakref.ref(plain_flow_view)
    del plain_flow_view
    gc.collect()
    if plain_flow_view_ref() is not None:
        raise AssertionError("Plain FlowView survived Python GC")
    plain_flip_view = next(
        control
        for control in controls
        if isinstance(control, fluentqt.FlipView)
    )
    controls.remove(plain_flip_view)
    plain_flip_view_ref = weakref.ref(plain_flip_view)
    del plain_flip_view
    gc.collect()
    if plain_flip_view_ref() is not None:
        raise AssertionError("Plain FlipView survived Python GC")
    plain_split_view = next(
        control
        for control in controls
        if isinstance(control, fluentqt.SplitView)
    )
    controls.remove(plain_split_view)
    plain_split_view_ref = weakref.ref(plain_split_view)
    del plain_split_view
    gc.collect()
    if plain_split_view_ref() is not None:
        raise AssertionError("Plain SplitView survived Python GC")
    plain_drawer_view = next(
        control
        for control in controls
        if isinstance(control, fluentqt.DrawerView)
    )
    controls.remove(plain_drawer_view)
    plain_drawer_view_ref = weakref.ref(plain_drawer_view)
    del plain_drawer_view
    gc.collect()
    if plain_drawer_view_ref() is not None:
        raise AssertionError("Plain DrawerView survived Python GC")
    plain_popup = next(
        control
        for control in controls
        if type(control) is fluentqt.Popup
    )
    controls.remove(plain_popup)
    plain_popup_ref = weakref.ref(plain_popup)
    del plain_popup
    gc.collect()
    if plain_popup_ref() is not None:
        raise AssertionError("Plain Popup survived Python GC")
    plain_flyout = next(
        control
        for control in controls
        if isinstance(control, fluentqt.Flyout)
    )
    controls.remove(plain_flyout)
    plain_flyout_ref = weakref.ref(plain_flyout)
    del plain_flyout
    gc.collect()
    if plain_flyout_ref() is not None:
        raise AssertionError("Plain Flyout survived Python GC")
    plain_coach_mark = next(
        control
        for control in controls
        if isinstance(control, fluentqt.CoachMark)
    )
    controls.remove(plain_coach_mark)
    plain_coach_mark_ref = weakref.ref(plain_coach_mark)
    del plain_coach_mark
    gc.collect()
    if plain_coach_mark_ref() is not None:
        raise AssertionError("Plain CoachMark survived Python GC")
    plain_teaching_tip = next(
        control
        for control in controls
        if isinstance(control, fluentqt.TeachingTip)
    )
    controls.remove(plain_teaching_tip)
    plain_teaching_tip_ref = weakref.ref(plain_teaching_tip)
    del plain_teaching_tip
    gc.collect()
    if plain_teaching_tip_ref() is not None:
        raise AssertionError("Plain TeachingTip survived Python GC")

    report_stage("AutoSuggestBox keyboard and popup lifecycle")
    if textfields.AutoSuggestBox is not fluentqt.AutoSuggestBox:
        raise AssertionError(
            "Text-fields module did not re-export AutoSuggestBox"
        )
    if native.fluent.AutoSuggestBox is not fluentqt.AutoSuggestBox:
        raise AssertionError("AutoSuggestBox lost its native binding identity")
    if not issubclass(fluentqt.AutoSuggestBox, fluentqt.LineEdit):
        raise AssertionError("AutoSuggestBox lost its Fluent LineEdit base")
    if hasattr(fluentqt.AutoSuggestBox, "onThemeUpdated"):
        raise AssertionError("AutoSuggestBox exposed its C++ theme hook")
    for internal_name in (
        "SuggestionListPopup",
        "AutoSuggestItemDelegate",
    ):
        if internal_name in dir(native.fluent):
            raise AssertionError(
                "AutoSuggestBox exposed internal type {0}".format(
                    internal_name
                )
            )

    suggest_host = QWidget()
    suggest_host.resize(520, 360)
    suggest_box = fluentqt.AutoSuggestBox(suggest_host)
    suggest_box.setGeometry(48, 48, 240, suggest_box.sizeHint().height())
    suggest_box.setSuggestions(["Alpha", "Alpine", "Azure"])
    text_reasons = []
    chosen_suggestions = []
    submitted_queries = []
    suggest_box.textChangedWithReason.connect(
        lambda text, reason: text_reasons.append((text, reason))
    )
    suggest_box.suggestionChosen.connect(chosen_suggestions.append)
    suggest_box.querySubmitted.connect(
        lambda text, item: submitted_queries.append((text, item))
    )
    suggest_host.show()
    suggest_box.show()
    suggest_box.setFocus()
    app.processEvents()
    QTest.keyClicks(suggest_box, "a")
    app.processEvents()
    if not suggest_box.isSuggestionListOpen():
        raise AssertionError("AutoSuggestBox did not open its suggestions")
    suggest_popup = suggest_host.findChild(
        native.fluent.Flyout,
        "AutoSuggestBoxSuggestionPopup",
    )
    if suggest_popup is None or not suggest_popup.isVisible():
        raise AssertionError("AutoSuggestBox did not create its native Flyout")
    if suggest_popup.window() is not suggest_host or suggest_popup.isWindow():
        raise AssertionError("AutoSuggestBox created a separate popup window")
    QTest.keyClick(suggest_box, Qt.Key_Down)
    QTest.keyClick(suggest_box, Qt.Key_Return)
    app.processEvents()
    if suggest_box.text() != "Alpha":
        raise AssertionError("AutoSuggestBox selection did not update text")
    if chosen_suggestions != ["Alpha", "Alpha"]:
        raise AssertionError("AutoSuggestBox choice signals did not round-trip")
    if submitted_queries != [("Alpha", "Alpha")]:
        raise AssertionError("AutoSuggestBox query signal did not round-trip")
    if (
        text_reasons[-1][1]
        != fluentqt.AutoSuggestBox.TextChangeReason.SuggestionChosen
    ):
        raise AssertionError("AutoSuggestBox lost its text change reason")
    suggest_host.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    app.processEvents()
    if Shiboken.isValid(suggest_box) or Shiboken.isValid(suggest_popup):
        raise AssertionError("AutoSuggestBox popup survived its Qt host")
    report_stage("AutoSuggestBox lifecycle complete")

    report_stage("ComboBox dropdown, model, and editor lifetime")
    if basicinput.ComboBox is not fluentqt.ComboBox:
        raise AssertionError("Basic-input module did not re-export ComboBox")
    if not issubclass(fluentqt.ComboBox, QComboBox):
        raise AssertionError("ComboBox lost its native QComboBox base")
    if "ComboBoxItemDelegate" in dir(native.fluent):
        raise AssertionError("Wheel exposed the internal ComboBox delegate")

    combo_host = QWidget()
    combo_host.resize(520, 360)
    combo = fluentqt.ComboBox(combo_host)
    combo.setGeometry(48, 48, 190, 32)
    combo.addItems(["Alpha", "Beta", "Gamma"])
    combo_host.show()
    combo.showPopup()
    app.processEvents()
    combo_popup = combo_host.findChild(QWidget, "ComboBoxPopup")
    if combo_popup is None or not combo_popup.isVisible():
        raise AssertionError("ComboBox did not open its native dropdown")
    if combo_popup.window() is not combo_host or combo_popup.isWindow():
        raise AssertionError("ComboBox created a separate popup window")
    combo_popup_view = combo_popup.findChild(
        QListView,
        "ComboBoxPopupListView",
    )
    if combo_popup_view is None or combo_popup_view.model() is not combo.model():
        raise AssertionError("ComboBox dropdown lost its installed model")
    target = combo_popup_view.model().index(2, 0)
    QTest.mouseClick(
        combo_popup_view.viewport(),
        Qt.LeftButton,
        Qt.NoModifier,
        combo_popup_view.visualRect(target).center(),
    )
    app.processEvents()
    if combo.currentIndex() != 2 or combo.currentText() != "Gamma":
        raise AssertionError("ComboBox dropdown selection did not round-trip")
    if combo_popup.isVisible():
        raise AssertionError("ComboBox dropdown stayed open after selection")

    first_model = QStringListModel(["One", "Two"])
    first_model_ref = weakref.ref(first_model)
    combo.setModel(first_model)
    del first_model
    gc.collect()
    if combo.model() is not first_model_ref():
        raise AssertionError("ComboBox did not retain its caller-owned model")
    replacement_model = QStringListModel(["Red", "Green"])
    combo.setModel(replacement_model)
    app.processEvents()
    gc.collect()
    if first_model_ref() is not None:
        raise AssertionError("ComboBox leaked its replaced model wrapper")

    first_editor = QLineEdit()
    combo.setLineEdit(first_editor)
    if combo.lineEdit() is not first_editor or first_editor.parent() is not combo:
        raise AssertionError("ComboBox did not adopt its line editor")
    second_editor = QLineEdit()
    combo.setLineEdit(second_editor)
    if Shiboken.isValid(first_editor):
        raise AssertionError("ComboBox did not delete its replaced editor")
    combo.setEditable(False)
    if Shiboken.isValid(second_editor) or combo.lineEdit() is not None:
        raise AssertionError("ComboBox did not release editable state")
    for unsupported in (
        lambda: combo.view(),
        lambda: combo.itemDelegate(),
    ):
        try:
            unsupported()
        except NotImplementedError:
            pass
        else:
            raise AssertionError(
                "ComboBox exposed an unused QComboBox customization surface"
            )

    combo_host.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    app.processEvents()
    report_stage("ComboBox lifecycle complete")

    report_stage("Fluent menu buttons and dependency lifetime")
    if basicinput.DropDownButton is not fluentqt.DropDownButton:
        raise AssertionError("Basic-input module did not re-export DropDownButton")
    if basicinput.SplitButton is not fluentqt.SplitButton:
        raise AssertionError("Basic-input module did not re-export SplitButton")
    if basicinput.ToggleSplitButton is not fluentqt.ToggleSplitButton:
        raise AssertionError(
            "Basic-input module did not re-export ToggleSplitButton"
        )
    if menus_toolbars.FluentMenu is not fluentqt.FluentMenu:
        raise AssertionError("Menus module did not re-export FluentMenu")
    if menus_toolbars.FluentMenuItem is not fluentqt.FluentMenuItem:
        raise AssertionError("Menus module did not re-export FluentMenuItem")
    if not issubclass(fluentqt.FluentMenu, QMenu):
        raise AssertionError("FluentMenu lost its native QMenu base")
    if not issubclass(fluentqt.FluentMenuItem, QWidgetAction):
        raise AssertionError("FluentMenuItem lost its QWidgetAction base")

    menu_buttons = (
        fluentqt.DropDownButton("Export"),
        fluentqt.SplitButton("Save"),
        fluentqt.ToggleSplitButton("Pin"),
    )
    primary_clicks = []
    toggle_changes = []
    menu_buttons[1].clicked.connect(lambda: primary_clicks.append(True))
    menu_buttons[2].toggled.connect(toggle_changes.append)
    menu_refs = []
    for index, button in enumerate(menu_buttons):
        menu = fluentqt.FluentMenu("Actions")
        item = fluentqt.FluentMenuItem("Action {0}".format(index), menu)
        menu.addAction(item)
        if menu.actions() != [item]:
            raise AssertionError("FluentMenu lost its installed action")
        button.setMenu(menu)
        if button.menu() is not menu:
            raise AssertionError("Menu button lost its installed menu identity")
        close_ref = weakref.ref(menu)
        menu.aboutToShow.connect(
            lambda current_ref=close_ref: QTimer.singleShot(
                0,
                current_ref().close,
            )
            if current_ref() is not None
            else None
        )
        button.resize(170, 36)
        button.show()
        menu_refs.append((weakref.ref(menu), weakref.ref(item)))
        del item
        del menu
    gc.collect()
    if any(reference() is None for pair in menu_refs for reference in pair):
        raise AssertionError("Menu button did not retain its menu dependency")

    QTest.mouseClick(menu_buttons[0], Qt.LeftButton)
    wait_for_events(1)
    for button in menu_buttons[1:]:
        QTest.mouseClick(
            button,
            Qt.LeftButton,
            Qt.NoModifier,
            QPoint(button.width() - 8, button.height() // 2),
        )
        wait_for_events(1)
    if primary_clicks or toggle_changes or menu_buttons[2].isChecked():
        raise AssertionError("A secondary menu click triggered the primary command")

    first_menu_ref, first_item_ref = menu_refs[0]
    menu_buttons[0].setMenu(None)
    gc.collect()
    if first_menu_ref() is not None or first_item_ref() is not None:
        raise AssertionError("setMenu(None) retained the replaced menu graph")

    final_menu = fluentqt.FluentMenu("Final")
    menu_buttons[0].setMenu(final_menu)
    final_menu_ref = weakref.ref(final_menu)
    del final_menu
    del menu_buttons
    del menu_refs
    gc.collect()
    if final_menu_ref() is not None:
        raise AssertionError("Deleting a menu button retained its menu")
    report_stage("Fluent menu button lifecycle complete")

    report_stage("Command surfaces and borrowed QAction lifetime")
    for class_name in (
        "CommandBar",
        "CommandBarFlyout",
        "FluentMenuBar",
    ):
        if getattr(menus_toolbars, class_name) is not getattr(
            fluentqt, class_name
        ):
            raise AssertionError(
                "Menus module did not re-export {0}".format(class_name)
            )
    if not issubclass(fluentqt.CommandBar, QWidget):
        raise AssertionError("CommandBar lost its QWidget base")
    if not issubclass(
        fluentqt.CommandBarFlyout, native.fluent.Flyout
    ):
        raise AssertionError("CommandBarFlyout lost its Flyout base")
    if not issubclass(fluentqt.FluentMenuBar, QMenuBar):
        raise AssertionError("FluentMenuBar lost its QMenuBar base")

    class InstalledCommandBar(fluentqt.CommandBar):
        pass

    command_bar = InstalledCommandBar()
    command_flyout = fluentqt.CommandBarFlyout()
    shared_command = QAction("Shared")
    shared_command_ref = weakref.ref(shared_command)
    if not command_bar.addPrimaryAction(shared_command):
        raise AssertionError("CommandBar rejected a primary QAction")
    if not command_flyout.addSecondaryAction(shared_command):
        raise AssertionError("CommandBarFlyout rejected a secondary QAction")
    if shared_command.parent() is not None:
        raise AssertionError("A command surface reparented a borrowed QAction")
    del shared_command
    gc.collect()
    if shared_command_ref() is None:
        raise AssertionError("A command surface lost a borrowed QAction wrapper")
    command_bar.clearPrimaryActions()
    gc.collect()
    if shared_command_ref() is None:
        raise AssertionError("Clearing one surface broke a shared QAction")
    command_flyout.clearSecondaryActions()
    gc.collect()
    if shared_command_ref() is not None:
        raise AssertionError("Command surfaces leaked a removed QAction")

    generated_calls = []
    generated_command = command_bar.addAction(
        "Generated",
        lambda: generated_calls.append(True),
    )
    generated_command.trigger()
    if generated_calls != [True]:
        raise AssertionError("CommandBar callable addAction did not trigger")
    if generated_command.parent() is not command_bar:
        raise AssertionError("Generated CommandBar QAction lost Qt ownership")

    flyout_calls = []
    generated_flyout_command = command_flyout.addAction(
        "Generated flyout",
        lambda: flyout_calls.append(True),
    )
    generated_flyout_command.trigger()
    if flyout_calls != [True]:
        raise AssertionError(
            "CommandBarFlyout callable addAction did not trigger"
        )
    if generated_flyout_command.parent() is not command_flyout:
        raise AssertionError(
            "Generated CommandBarFlyout QAction lost Qt ownership"
        )

    command_anchor = QWidget()
    command_anchor_ref = weakref.ref(command_anchor)
    command_flyout.setAnchor(command_anchor)
    del command_anchor
    gc.collect()
    if command_flyout.anchor() is not command_anchor_ref():
        raise AssertionError("CommandBarFlyout did not retain its anchor")
    command_flyout.setAnchor(None)
    gc.collect()
    if command_anchor_ref() is not None:
        raise AssertionError("CommandBarFlyout leaked its replaced anchor")
    try:
        type(
            "InvalidCommandBarFlyoutSubclass",
            (fluentqt.CommandBarFlyout,),
            {},
        )
    except TypeError:
        pass
    else:
        raise AssertionError("Final CommandBarFlyout became subclassable")

    fluent_menu_bar = fluentqt.FluentMenuBar()
    fluent_menu_bar.setFontStyle(fluentqt.FontRole.BodyStrong)
    menu_bar_menu = fluent_menu_bar.addMenu("File")
    if menu_bar_menu.parent() is not fluent_menu_bar:
        raise AssertionError("FluentMenuBar did not own its QMenu")
    del menu_bar_menu
    del fluent_menu_bar
    del generated_command
    del generated_flyout_command
    del command_bar
    del command_flyout
    gc.collect()
    report_stage("Command surface lifecycle complete")

    report_stage("FlipView page ownership")
    flip_view = fluentqt.FlipView()
    owned_page = QWidget()
    borrowed_page = QWidget()
    original_parent = QWidget()
    reparented_page = QWidget(original_parent)
    if not flip_view.addOwnedPage(owned_page):
        raise AssertionError("FlipView rejected an Owned page")
    if not flip_view.addBorrowedPage(borrowed_page):
        raise AssertionError("FlipView rejected a Borrowed page")
    if not flip_view.addReparentedPage(reparented_page):
        raise AssertionError("FlipView rejected a Reparented page")
    if flip_view.pageAt(1) is not borrowed_page:
        raise AssertionError("FlipView did not preserve page identity")
    if (
        flip_view.pageOwnershipAt(2)
        != fluentqt.WidgetOwnership.Reparented
    ):
        raise AssertionError("FlipView lost its page ownership policy")

    taken_page = flip_view.takePage(1)
    if taken_page is not borrowed_page or taken_page.parent() is not None:
        raise AssertionError("FlipView did not transfer a taken page")
    if not Shiboken.ownedByPython(taken_page):
        raise AssertionError("A taken FlipView page is not Python-owned")
    if not flip_view.removePage(1):
        raise AssertionError("FlipView did not release a Reparented page")
    if reparented_page.parent() is not original_parent:
        raise AssertionError("FlipView did not restore the original parent")
    if not flip_view.removePage(0) or Shiboken.isValid(owned_page):
        raise AssertionError("FlipView did not delete an Owned page")

    surviving_borrowed = QWidget()
    surviving_reparented = QWidget(original_parent)
    flip_view.addBorrowedPage(surviving_borrowed)
    flip_view.addReparentedPage(surviving_reparented)
    flip_view_ref = weakref.ref(flip_view)
    del flip_view
    gc.collect()
    if flip_view_ref() is not None:
        raise AssertionError("FlipView survived Python GC")
    if not Shiboken.isValid(surviving_borrowed):
        raise AssertionError("Borrowed FlipView page was deleted with host")
    if surviving_borrowed.parent() is not None:
        raise AssertionError("Borrowed FlipView page was not detached")
    if surviving_reparented.parent() is not original_parent:
        raise AssertionError("Reparented FlipView page was not restored")
    del owned_page
    del borrowed_page
    del taken_page
    del surviving_borrowed
    del surviving_reparented
    del reparented_page
    del original_parent
    gc.collect()
    report_stage("FlipView lifecycle complete")

    report_stage("SplitView pane ownership")
    if collections.SplitView is not fluentqt.SplitView:
        raise AssertionError("Collections module did not re-export SplitView")
    if (
        collections.SplitViewPaneOptions
        is not fluentqt.SplitViewPaneOptions
    ):
        raise AssertionError(
            "Collections module did not re-export SplitViewPaneOptions"
        )
    pane_options = fluentqt.SplitViewPaneOptions(40, 100, 260, False)
    if fluentqt.SplitViewPaneOptions(pane_options) != pane_options:
        raise AssertionError("SplitViewPaneOptions lost value equality")
    try:
        hash(pane_options)
    except TypeError:
        pass
    else:
        raise AssertionError(
            "Mutable SplitViewPaneOptions unexpectedly remained hashable"
        )

    split_view = fluentqt.SplitView()
    owned_pane = QWidget()
    borrowed_pane = QWidget()
    pane_parent = QWidget()
    reparented_pane = QWidget(pane_parent)
    if split_view.addOwnedPane(owned_pane, pane_options) != 0:
        raise AssertionError("SplitView rejected an Owned pane")
    if split_view.addBorrowedPane(borrowed_pane) != 1:
        raise AssertionError("SplitView rejected a Borrowed pane")
    if split_view.addReparentedPane(reparented_pane) != 2:
        raise AssertionError("SplitView rejected a Reparented pane")
    if split_view.paneAt(1) is not borrowed_pane:
        raise AssertionError("SplitView did not preserve pane identity")
    if (
        split_view.paneOwnershipAt(2)
        != fluentqt.WidgetOwnership.Reparented
    ):
        raise AssertionError("SplitView lost its pane ownership policy")
    if split_view.panePreferredSize(0) != 100:
        raise AssertionError("SplitView lost pane sizing options")

    taken_pane = split_view.takePaneAt(1)
    if taken_pane is not borrowed_pane or taken_pane.parent() is not None:
        raise AssertionError("SplitView did not transfer a taken pane")
    if not Shiboken.ownedByPython(taken_pane):
        raise AssertionError("A taken SplitView pane is not Python-owned")
    if not split_view.removePaneAt(1):
        raise AssertionError("SplitView did not release a Reparented pane")
    if reparented_pane.parent() is not pane_parent:
        raise AssertionError("SplitView did not restore the original parent")
    if not split_view.removePane(owned_pane) or Shiboken.isValid(owned_pane):
        raise AssertionError("SplitView did not delete an Owned pane")

    surviving_borrowed_pane = QWidget()
    surviving_reparented_pane = QWidget(pane_parent)
    split_view.addBorrowedPane(surviving_borrowed_pane)
    split_view.addReparentedPane(surviving_reparented_pane)
    split_view_ref = weakref.ref(split_view)
    del split_view
    gc.collect()
    if split_view_ref() is not None:
        raise AssertionError("SplitView survived Python GC")
    if not Shiboken.isValid(surviving_borrowed_pane):
        raise AssertionError("Borrowed SplitView pane was deleted with host")
    if surviving_borrowed_pane.parent() is not None:
        raise AssertionError("Borrowed SplitView pane was not detached")
    if surviving_reparented_pane.parent() is not pane_parent:
        raise AssertionError("Reparented SplitView pane was not restored")
    del owned_pane
    del borrowed_pane
    del taken_pane
    del surviving_borrowed_pane
    del surviving_reparented_pane
    del reparented_pane
    del pane_parent
    gc.collect()
    report_stage("SplitView lifecycle complete")

    report_stage("DrawerView overlay and content ownership")
    if collections.DrawerView is not fluentqt.DrawerView:
        raise AssertionError("Collections module did not re-export DrawerView")

    overlay_host = QWidget()
    overlay_host.resize(640, 480)
    overlay_host.show()
    overlay_drawer = fluentqt.DrawerView(overlay_host)
    overlay_drawer.setAnimationEnabled(False)
    overlay_drawer.setEdge(fluentqt.DrawerView.DrawerEdge.Right)
    close_policy = (
        fluentqt.DrawerView.CloseFlag.CloseOnPressOutside
        | fluentqt.DrawerView.CloseFlag.CloseOnEscape
    )
    overlay_drawer.setClosePolicy(close_policy)
    overlay_drawer.open()
    app.processEvents()
    if not overlay_drawer.isOpen() or overlay_drawer.position() != 1.0:
        raise AssertionError("DrawerView did not open its native overlay")
    if overlay_drawer.window() is not overlay_host:
        raise AssertionError("DrawerView created a separate top-level window")
    if overlay_drawer.closePolicy() != close_policy:
        raise AssertionError("DrawerView lost its close-policy flags")
    overlay_drawer.close()
    app.processEvents()
    if overlay_drawer.isOpen() or overlay_drawer.position() != 0.0:
        raise AssertionError("DrawerView did not close its native overlay")
    overlay_host.close()
    del overlay_drawer
    del overlay_host
    gc.collect()

    drawer = fluentqt.DrawerView()
    owned_content = QWidget()
    borrowed_content = QWidget()
    first_restore_parent = QWidget()
    reparented_content = QWidget(first_restore_parent)
    if not drawer.setOwnedContentWidget(owned_content):
        raise AssertionError("DrawerView rejected Owned content")
    if not drawer.setBorrowedContentWidget(borrowed_content):
        raise AssertionError("DrawerView rejected Borrowed content")
    if Shiboken.isValid(owned_content):
        raise AssertionError("DrawerView did not delete replaced Owned content")
    if not drawer.setReparentedContentWidget(reparented_content):
        raise AssertionError("DrawerView rejected Reparented content")
    if borrowed_content.parent() is not None:
        raise AssertionError("DrawerView did not detach Borrowed content")
    taken_content = drawer.takeContentWidget()
    if taken_content is not reparented_content:
        raise AssertionError("DrawerView did not preserve content identity")
    if taken_content.parent() is not None:
        raise AssertionError("DrawerView did not detach taken content")
    if not Shiboken.ownedByPython(taken_content):
        raise AssertionError("Taken DrawerView content is not Python-owned")

    second_restore_parent = QWidget()
    restored_content = QWidget(second_restore_parent)
    drawer.setReparentedContentWidget(restored_content)
    drawer_ref = weakref.ref(drawer)
    del drawer
    gc.collect()
    if drawer_ref() is not None:
        raise AssertionError("DrawerView survived Python GC")
    if restored_content.parent() is not second_restore_parent:
        raise AssertionError("DrawerView did not restore Reparented content")
    del owned_content
    del borrowed_content
    del taken_content
    del reparented_content
    del first_restore_parent
    del restored_content
    del second_restore_parent
    gc.collect()
    report_stage("DrawerView lifecycle complete")

    report_stage("Popup overlay and dependency lifetime")
    if dialogs_flyouts.Popup is not fluentqt.Popup:
        raise AssertionError("Dialogs module did not re-export Popup")

    popup_host = QWidget()
    popup_host.resize(640, 480)
    popup_anchor = fluentqt.Button("Open", popup_host)
    popup_anchor.setGeometry(80, 72, 120, 36)
    popup_passthrough = fluentqt.Button("Toolbar", popup_host)
    popup_passthrough.setGeometry(440, 24, 120, 36)
    popup_anchor.show()
    popup_passthrough.show()
    popup_host.show()

    overlay_popup = fluentqt.Popup(popup_host)
    overlay_popup.resize(320, 180)
    overlay_popup.setAnimationEnabled(False)
    overlay_popup.setExitAnimationEnabled(False)
    overlay_popup.setModal(True)
    overlay_popup.setDim(True)
    overlay_popup.setLightDismissConsumesPress(True)
    overlay_popup.setPosition(
        popup_anchor,
        QPoint(0, popup_anchor.height() + 8),
    )
    overlay_popup.setThemeSource(popup_anchor)
    overlay_popup.addLightDismissPassthrough(popup_passthrough)
    popup_close_policy = (
        fluentqt.Popup.CloseFlag.CloseOnPressOutside
        | fluentqt.Popup.CloseFlag.CloseOnEscape
    )
    overlay_popup.setClosePolicy(popup_close_policy)
    overlay_popup.open()
    app.processEvents()
    if not overlay_popup.isOpen() or overlay_popup.popupProgress() != 1.0:
        raise AssertionError("Popup did not open its native overlay")
    if overlay_popup.window() is not popup_host:
        raise AssertionError("Popup created a separate top-level window")
    if overlay_popup.closePolicy() != popup_close_policy:
        raise AssertionError("Popup lost its close-policy flags")
    if popup_host.findChild(QWidget, "PopupScrim") is None:
        raise AssertionError("Modal Popup did not create a same-window scrim")
    overlay_popup.close()
    app.processEvents()
    if overlay_popup.isOpen() or overlay_popup.popupProgress() != 0.0:
        raise AssertionError("Popup did not close its native overlay")
    popup_host.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    app.processEvents()

    dependency_popup = fluentqt.Popup()
    dependency_anchor = QWidget()
    dependency_theme = QWidget()
    dependency_passthrough = QWidget()
    dependency_popup.setPosition(dependency_anchor, QPoint(4, 8))
    dependency_popup.setThemeSource(dependency_theme)
    dependency_popup.addLightDismissPassthrough(dependency_passthrough)
    dependency_refs = tuple(
        weakref.ref(widget)
        for widget in (
            dependency_anchor,
            dependency_theme,
            dependency_passthrough,
        )
    )
    del dependency_anchor
    del dependency_theme
    del dependency_passthrough
    gc.collect()
    if not all(reference() is not None for reference in dependency_refs):
        raise AssertionError("Popup did not retain a QWidget dependency")
    dependency_popup_ref = weakref.ref(dependency_popup)
    del dependency_popup
    gc.collect()
    if dependency_popup_ref() is not None:
        raise AssertionError("Popup survived Python GC")
    if not all(reference() is None for reference in dependency_refs):
        raise AssertionError("Popup leaked a QWidget dependency")
    report_stage("Popup lifecycle complete")

    report_stage("Flyout placement and anchor lifetime")
    if dialogs_flyouts.Flyout is not fluentqt.Flyout:
        raise AssertionError("Dialogs module did not re-export Flyout")

    flyout_host = QWidget()
    flyout_host.resize(640, 480)
    flyout_anchor = fluentqt.Button("Open", flyout_host)
    flyout_anchor.setGeometry(260, 180, 120, 36)
    flyout_anchor.show()
    flyout_host.show()

    overlay_flyout = fluentqt.Flyout(flyout_host)
    overlay_flyout.setFixedSize(320, 180)
    overlay_flyout.setAnimationEnabled(False)
    overlay_flyout.setExitAnimationEnabled(False)
    overlay_flyout.setPlacement(fluentqt.Flyout.Placement.Bottom)
    overlay_flyout.setAnchorOffset(12)
    overlay_flyout.setClampToWindow(True)
    overlay_flyout.showAt(flyout_anchor)
    app.processEvents()
    if not overlay_flyout.isOpen():
        raise AssertionError("Flyout did not open its native overlay")
    if overlay_flyout.window() is not flyout_host:
        raise AssertionError("Flyout created a separate top-level window")
    if overlay_flyout.anchor() is not flyout_anchor:
        raise AssertionError("Flyout did not preserve anchor identity")
    visible_card = overlay_flyout.geometry().adjusted(16, 16, -16, -16)
    if (
        visible_card.top()
        != flyout_anchor.geometry().bottom() + overlay_flyout.anchorOffset()
    ):
        raise AssertionError("Flyout lost its anchor-relative placement")
    flyout_scrim = flyout_host.findChild(QWidget, "PopupScrim")
    if flyout_scrim is not None and flyout_scrim.isVisible():
        raise AssertionError("Default non-modal Flyout created a visible scrim")
    overlay_flyout.close()
    app.processEvents()
    flyout_host.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    app.processEvents()

    dependency_flyout = fluentqt.Flyout()
    dependency_anchor = QWidget()
    dependency_theme = QWidget()
    dependency_passthrough = QWidget()
    dependency_flyout.setAnchor(dependency_anchor)
    dependency_flyout.setThemeSource(dependency_theme)
    dependency_flyout.addLightDismissPassthrough(dependency_passthrough)
    dependency_refs = tuple(
        weakref.ref(widget)
        for widget in (
            dependency_anchor,
            dependency_theme,
            dependency_passthrough,
        )
    )
    del dependency_anchor
    del dependency_theme
    del dependency_passthrough
    gc.collect()
    if not all(reference() is not None for reference in dependency_refs):
        raise AssertionError("Flyout did not retain a QWidget dependency")
    dependency_flyout_ref = weakref.ref(dependency_flyout)
    del dependency_flyout
    gc.collect()
    if dependency_flyout_ref() is not None:
        raise AssertionError("Flyout survived Python GC")
    if not all(reference() is None for reference in dependency_refs):
        raise AssertionError("Flyout leaked a QWidget dependency")
    report_stage("Flyout lifecycle complete")

    report_stage("CoachMark and TeachingTip same-window lifecycle")
    if dialogs_flyouts.CoachMark is not fluentqt.CoachMark:
        raise AssertionError("Dialogs module did not re-export CoachMark")
    if dialogs_flyouts.TeachingTip is not fluentqt.TeachingTip:
        raise AssertionError("Dialogs module did not re-export TeachingTip")
    if not issubclass(fluentqt.TeachingTip, fluentqt.Popup):
        raise AssertionError("TeachingTip lost native Popup inheritance")
    for public_type in (fluentqt.Popup, fluentqt.CoachMark, fluentqt.TeachingTip):
        if hasattr(public_type, "onThemeUpdated"):
            raise AssertionError("Guidance overlay exposed its C++ theme hook")

    guidance_host = QWidget()
    guidance_host.resize(760, 560)
    coach_target = fluentqt.Button("Coach", guidance_host)
    coach_target.setGeometry(140, 120, 120, 36)
    teaching_target = fluentqt.Button("Teach", guidance_host)
    teaching_target.setGeometry(500, 120, 120, 36)
    coach_target.show()
    teaching_target.show()
    guidance_host.show()

    coach = fluentqt.CoachMark(guidance_host)
    coach.setCardSize(QSize(280, 140))
    coach.setPlacement(fluentqt.CoachMark.Placement.Bottom)
    coach.setTarget(coach_target)
    coach_content = fluentqt.Label("Coach content", coach.contentHost())
    coach_content.show()
    coach.open()
    app.processEvents()
    if not coach.isOpen() or coach.window() is not guidance_host:
        raise AssertionError("CoachMark did not open in its owning window")
    if coach.contentHost().parent() is not coach:
        raise AssertionError("CoachMark lost its Qt-owned content host")
    coach.close()

    tip = fluentqt.TeachingTip(guidance_host)
    tip.setAnimationEnabled(False)
    tip.setExitAnimationEnabled(False)
    tip.setCardSize(QSize(320, 160))
    tip.setPreferredPlacement(
        fluentqt.TeachingTip.PreferredPlacement.Bottom
    )
    tip.setLightDismissEnabled(True)
    tip_content = fluentqt.Label("Teaching content", tip.contentHost())
    tip_content.show()
    reasons = []
    tip.closing.connect(reasons.append)
    tip.showAt(teaching_target)
    app.processEvents()
    if not tip.isOpen() or tip.window() is not guidance_host:
        raise AssertionError("TeachingTip did not open in its owning window")
    if tip.target() is not teaching_target:
        raise AssertionError("TeachingTip lost its target identity")
    tip.closeWithReason(fluentqt.TeachingTip.CloseReason.ActionButton)
    app.processEvents()
    if tip.isOpen() or reasons != [fluentqt.TeachingTip.CloseReason.ActionButton]:
        raise AssertionError("TeachingTip lost its semantic close reason")

    coach_content_host = coach.contentHost()
    tip_content_host = tip.contentHost()
    guidance_host.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    app.processEvents()
    for wrapper in (coach, coach_content_host, tip, tip_content_host):
        if Shiboken.isValid(wrapper):
            raise AssertionError("A guidance overlay survived its Qt host")

    dependency_coach = fluentqt.CoachMark()
    dependency_tip = fluentqt.TeachingTip()
    dependency_coach_target = QWidget()
    dependency_tip_target = QWidget()
    dependency_coach.setTarget(dependency_coach_target)
    dependency_tip.setTarget(dependency_tip_target)
    dependency_refs = (
        weakref.ref(dependency_coach_target),
        weakref.ref(dependency_tip_target),
    )
    del dependency_coach_target
    del dependency_tip_target
    gc.collect()
    if not all(reference() is not None for reference in dependency_refs):
        raise AssertionError("A guidance overlay did not retain its target")
    del dependency_coach
    del dependency_tip
    gc.collect()
    if not all(reference() is None for reference in dependency_refs):
        raise AssertionError("A guidance overlay leaked its target")
    report_stage("CoachMark and TeachingTip lifecycle complete")

    report_stage("Dialog and ContentDialog lifecycle")
    if dialogs_flyouts.Dialog is not fluentqt.Dialog:
        raise AssertionError("Dialogs module did not re-export Dialog")
    if dialogs_flyouts.ContentDialog is not fluentqt.ContentDialog:
        raise AssertionError("Dialogs module did not re-export ContentDialog")
    if not issubclass(fluentqt.ContentDialog, fluentqt.Dialog):
        raise AssertionError("ContentDialog lost native Dialog inheritance")

    dialog_host = QWidget()
    dialog_host.resize(640, 480)
    dialog_host.show()
    overlay_dialog = fluentqt.Dialog(dialog_host)
    overlay_dialog.setFixedSize(320, 200)
    overlay_dialog.setAnimationEnabled(False)
    overlay_dialog.setSmokeEnabled(True)
    overlay_dialog.open()
    app.processEvents()
    if overlay_dialog.window() is not dialog_host:
        raise AssertionError("Dialog created a separate top-level window")
    dialog_scrim = dialog_host.findChild(QWidget, "DialogSmokeScrim")
    if dialog_scrim is None or not dialog_scrim.isVisible():
        raise AssertionError("Dialog did not create its same-window scrim")
    overlay_dialog.done(fluentqt.ContentDialog.ResultNone)
    app.processEvents()
    if dialog_host.findChild(QWidget, "DialogSmokeScrim") is not None:
        raise AssertionError("Dialog did not release its same-window scrim")

    dependency_dialog = fluentqt.Dialog()
    dependency_source = QWidget()
    dependency_dialog.setThemeSource(dependency_source)
    dependency_source_ref = weakref.ref(dependency_source)
    del dependency_source
    gc.collect()
    if dependency_source_ref() is None:
        raise AssertionError("Dialog did not retain its theme source wrapper")
    dependency_dialog_ref = weakref.ref(dependency_dialog)
    del dependency_dialog
    gc.collect()
    if dependency_dialog_ref() is not None:
        raise AssertionError("Dialog survived Python GC")
    if dependency_source_ref() is not None:
        raise AssertionError("Dialog leaked its theme source wrapper")

    content_dialog = fluentqt.ContentDialog(dialog_host)
    content_dialog.setAnimationEnabled(False)
    content_dialog.setTitle("Remove this item?")
    content_dialog.setPrimaryButtonText("Remove")
    content_dialog.setSecondaryButtonText("Keep")
    content_dialog.setDefaultButton(
        fluentqt.ContentDialogButton.Primary
    )
    if content_dialog.title() != "Remove this item?":
        raise AssertionError("ContentDialog lost its title")
    if content_dialog.defaultButton() != int(
        fluentqt.ContentDialogButton.Primary
    ):
        raise AssertionError("ContentDialog lost its default button")

    first_content = QWidget()
    content_dialog.setContent(first_content)
    if content_dialog.content() is not first_content:
        raise AssertionError("ContentDialog lost content identity")
    if first_content.parent() is not content_dialog:
        raise AssertionError("ContentDialog did not adopt installed content")
    taken_content = content_dialog.takeContent()
    if taken_content is not first_content or taken_content.parent() is not None:
        raise AssertionError("ContentDialog did not return parentless content")
    if not Shiboken.ownedByPython(taken_content):
        raise AssertionError("Taken ContentDialog content is not Python-owned")

    owned_content = QWidget()
    content_dialog.setContent(owned_content)
    content_dialog.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    app.processEvents()
    if Shiboken.isValid(content_dialog):
        raise AssertionError("ContentDialog deferred deletion did not finish")
    if Shiboken.isValid(owned_content):
        raise AssertionError("ContentDialog did not destroy installed content")
    dialog_host.close()
    report_stage("Dialog and ContentDialog lifecycle complete")

    annotated = next(
        control
        for control in controls
        if isinstance(control, fluentqt.AnnotatedScrollBar)
    )
    label_type = fluentqt.AnnotatedScrollBarLabel
    labels = [
        label_type("Start", 0, "Start detail"),
        label_type("Middle", 500, "Middle detail"),
        label_type("End", 1000, "End detail"),
    ]
    if label_type(labels[0]) != labels[0]:
        raise AssertionError(
            "AnnotatedScrollBarLabel did not preserve value equality"
        )
    try:
        hash(labels[0])
    except TypeError:
        pass
    else:
        raise AssertionError(
            "Mutable AnnotatedScrollBarLabel unexpectedly remained hashable"
        )
    annotated.setRange(0, 1000)
    annotated.setLabels(labels)
    if [
        (label.text, label.offset, label.detailText)
        for label in annotated.labels()
    ] != [
        ("Start", 0, "Start detail"),
        ("Middle", 500, "Middle detail"),
        ("End", 1000, "End detail"),
    ]:
        raise AssertionError(
            "AnnotatedScrollBar did not preserve label value types"
        )
    provider_offsets = []
    annotated.setDetailLabelProvider(
        lambda offset: provider_offsets.append(offset)
        or "Offset {0}".format(offset)
    )
    if not annotated.hasDetailLabelProvider():
        raise AssertionError(
            "AnnotatedScrollBar did not install its Python detail provider"
        )
    annotated.detailLabelRequested.emit(250)
    if provider_offsets != [0, 250]:
        raise AssertionError(
            "AnnotatedScrollBar did not synchronously dispatch its provider"
        )
    annotated.clearDetailLabelProvider()
    if annotated.hasDetailLabelProvider():
        raise AssertionError(
            "AnnotatedScrollBar did not clear its Python detail provider"
        )

    linked_scroll_view = fluentqt.ScrollView()
    annotated.connectToScrollView(linked_scroll_view)
    if annotated.connectedScrollView() is not linked_scroll_view:
        raise AssertionError(
            "AnnotatedScrollBar did not preserve linked wrapper identity"
        )
    if linked_scroll_view.parent() is not None:
        raise AssertionError(
            "AnnotatedScrollBar reparented its borrowed ScrollView"
        )
    annotated.disconnectScrollView()
    if annotated.connectedScrollView() is not None:
        raise AssertionError(
            "AnnotatedScrollBar did not clear its borrowed ScrollView"
        )

    calendar = next(
        control
        for control in controls
        if isinstance(control, fluentqt.CalendarView)
    )
    minimum_date = QDate(2026, 5, 10)
    maximum_date = QDate(2026, 5, 20)
    selected_dates = []
    calendar.selectedDateChanged.connect(selected_dates.append)
    calendar.setDateRange(minimum_date, maximum_date)
    calendar.setSelectedDate(QDate(2026, 5, 1))
    calendar.setContentLevel(
        fluentqt.CalendarView.CalendarContentLevel.Month
    )
    if calendar.selectedDate() != minimum_date:
        raise AssertionError("CalendarView did not clamp its selected QDate")
    if selected_dates != [minimum_date]:
        raise AssertionError("CalendarView did not emit its date signal")
    if (
        calendar.contentLevel()
        != fluentqt.CalendarView.CalendarContentLevel.Month
    ):
        raise AssertionError("CalendarView did not preserve its content level")
    if not calendar.isDateSelectable(maximum_date):
        raise AssertionError("CalendarView rejected an in-range date")

    report_stage("date and time picker popup lifecycle")
    if date_time.CalendarDatePicker is not fluentqt.CalendarDatePicker:
        raise AssertionError(
            "Date-time module did not re-export CalendarDatePicker"
        )
    if date_time.DatePicker is not fluentqt.DatePicker:
        raise AssertionError("Date-time module did not re-export DatePicker")
    if date_time.TimePicker is not fluentqt.TimePicker:
        raise AssertionError("Date-time module did not re-export TimePicker")

    picker_host = QWidget()
    picker_host.resize(720, 520)
    calendar_picker = fluentqt.CalendarDatePicker(picker_host)
    date_picker = fluentqt.DatePicker(picker_host)
    time_picker = fluentqt.TimePicker(picker_host)
    calendar_picker.move(24, 24)
    date_picker.move(24, 88)
    time_picker.move(24, 152)
    picker_host.show()
    app.processEvents()

    calendar_picker.setDateRange(minimum_date, maximum_date)
    calendar_picker.setDate(QDate(2026, 5, 1))
    calendar_picker.openCalendar()
    app.processEvents()
    internal_calendar = calendar_picker.calendarView()
    if not calendar_picker.isCalendarOpen() or internal_calendar is None:
        raise AssertionError("CalendarDatePicker did not open its calendar")
    if internal_calendar.selectedDate() != minimum_date:
        raise AssertionError("CalendarDatePicker did not sync its calendar")
    calendar_picker.closeCalendar()

    date_picker.setDateRange(minimum_date, maximum_date)
    date_picker.setSelectedDate(QDate(2026, 5, 30))
    date_picker.openPicker()
    app.processEvents()
    if date_picker.selectedDate() != maximum_date:
        raise AssertionError("DatePicker did not clamp its selected QDate")
    if not date_picker.isDropDownOpen():
        raise AssertionError("DatePicker did not open its native dropdown")
    date_picker.closePicker()

    time_picker.setMinuteIncrement(15)
    time_picker.setSelectedTime(QTime(9, 58))
    time_picker.openPicker()
    app.processEvents()
    if time_picker.selectedTime() != QTime(9, 45):
        raise AssertionError("TimePicker did not snap its selected QTime")
    if not time_picker.isDropDownOpen():
        raise AssertionError("TimePicker did not open its native dropdown")
    time_picker.closePicker()

    picker_host.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    app.processEvents()
    if any(
        Shiboken.isValid(item)
        for item in (
            calendar_picker,
            date_picker,
            time_picker,
            internal_calendar,
        )
    ):
        raise AssertionError("A date/time picker survived its Qt host")
    report_stage("date and time picker lifecycle complete")

    compound = next(
        control
        for control in controls
        if isinstance(control, fluentqt.CompoundButton)
    )
    compound_changes = []
    compound.secondaryTextChanged.connect(compound_changes.append)
    compound.setSecondaryText("Restart after downloading")
    if compound.text() != "Install":
        raise AssertionError("CompoundButton did not preserve primary text")
    if compound.secondaryText() != "Restart after downloading":
        raise AssertionError("CompoundButton did not preserve secondary text")
    if compound_changes != ["Restart after downloading"]:
        raise AssertionError("CompoundButton did not emit its property signal")

    color_picker = next(
        control
        for control in controls
        if isinstance(control, fluentqt.ColorPicker)
    )
    picker_changes = []
    color_picker.colorChanged.connect(picker_changes.append)
    selected_color = QColor(0, 120, 212, 180)
    color_picker.setColor(selected_color)
    color_picker.setAlphaEnabled(False)
    if color_picker.color() != selected_color:
        raise AssertionError("ColorPicker did not preserve its QColor value")
    if color_picker.alphaEnabled():
        raise AssertionError("ColorPicker did not disable alpha editing")
    if picker_changes != [selected_color]:
        raise AssertionError("ColorPicker did not emit its property signal")
    for internal_name in (
        "hue",
        "saturation",
        "value",
        "setHueFromBar",
        "setSVFromSpectrum",
        "setValueFromSlider",
        "setAlphaFromSlider",
    ):
        if hasattr(fluentqt.ColorPicker, internal_name):
            raise AssertionError(
                "ColorPicker exposed internal helper: {0}".format(
                    internal_name
                )
            )

    font_icon = next(
        control
        for control in controls
        if isinstance(control, fluentqt.FontIcon)
    )
    icon_size_changes = []
    font_icon.iconSizeChanged.connect(icon_size_changes.append)
    font_icon.setIconSize(24)
    font_icon.setColor(QColor("#7f52ff"))
    font_icon.setRotation(45.0)
    if font_icon.glyph() != "ic_fluent_settings_20_regular":
        raise AssertionError("FontIcon did not preserve its catalog name")
    if font_icon.sizeHint().width() != 24:
        raise AssertionError("FontIcon did not update its optical size")
    if icon_size_changes != [24]:
        raise AssertionError("FontIcon did not emit its property signal")
    if font_icon.color() != QColor("#7f52ff"):
        raise AssertionError("FontIcon did not preserve its explicit color")
    if font_icon.rotation() != 45.0:
        raise AssertionError("FontIcon did not preserve its rotation")

    avatar = next(
        control
        for control in controls
        if isinstance(control, fluentqt.Avatar)
    )
    avatar.setPresence(fluentqt.Avatar.PresenceStatus.Available)
    if avatar.effectiveInitials() != "AL":
        raise AssertionError("Avatar did not preserve its initials contract")

    rating = next(
        control
        for control in controls
        if isinstance(control, fluentqt.RatingControl)
    )
    rating.setValue(3.5)
    if rating.value() != 3.5:
        raise AssertionError("RatingControl did not preserve its value")

    pager = next(
        control
        for control in controls
        if isinstance(control, fluentqt.PipsPager)
    )
    pager.setSelectionAnimationEnabled(False)
    pager.setNumberOfPages(7)
    pager.setMaxVisiblePips(3)
    pager.setSelectedPageIndex(4)
    pager.setNextButtonVisibility(
        fluentqt.PipsPager.PipsPagerButtonVisibility.Visible
    )
    if pager.firstVisiblePage() != 3 or not pager.hasNextPage():
        raise AssertionError("PipsPager did not preserve its page window")
    for internal_name in (
        "HitKind",
        "selectedVisualOffset",
        "visibleWindowOffset",
    ):
        if hasattr(fluentqt.PipsPager, internal_name):
            raise AssertionError(
                "PipsPager exposed internal API: {0}".format(
                    internal_name
                )
            )

    scroll_bar = next(
        control
        for control in controls
        if isinstance(control, fluentqt.ScrollBar)
    )
    scroll_bar.setThickness(11)
    scroll_bar.setRange(0, 100)
    scroll_bar.setValue(42)
    if scroll_bar.thickness() != 11 or scroll_bar.value() != 42:
        raise AssertionError("ScrollBar did not preserve its Qt properties")

    text_edit = next(
        control
        for control in controls
        if isinstance(control, fluentqt.TextEdit)
    )
    text_edit.setMinVisibleLines(2)
    text_edit.setMaxVisibleLines(3)
    text_edit.setPlainText("First line\nSecond line")
    text_edit.setScrollChainingEnabled(True)
    if text_edit.toPlainText() != "First line\nSecond line":
        raise AssertionError("TextEdit did not preserve its plain text")
    if text_edit.minVisibleLines() != 2 or text_edit.maxVisibleLines() != 3:
        raise AssertionError("TextEdit did not preserve visible-line bounds")
    if not text_edit.isScrollChainingEnabled():
        raise AssertionError("TextEdit did not preserve scroll chaining")
    if (
        not Shiboken.isValid(text_edit.verticalScrollBar())
        or text_edit.verticalScrollBar().parent() is not text_edit
    ):
        raise AssertionError("TextEdit exposed an invalid Fluent scroll bar")

    info_bar = fluentqt.InfoBar(
        title="Bindings ready",
        severity=fluentqt.InfoBar.InfoBarSeverity.Success,
    )
    info_action = fluentqt.Button("Details")
    info_bar.setActionWidget(info_action)
    taken_info_action = info_bar.takeActionWidget()
    if taken_info_action is not info_action:
        raise AssertionError("InfoBar did not preserve action identity")
    if taken_info_action.parent() is not None:
        raise AssertionError("InfoBar take did not detach its action")
    if not Shiboken.ownedByPython(taken_info_action):
        raise AssertionError("InfoBar take did not return Python ownership")
    info_bar.setActionWidget(taken_info_action)
    info_bar_ref = weakref.ref(info_bar)
    del info_bar
    gc.collect()
    if info_bar_ref() is not None:
        raise AssertionError("InfoBar survived Python GC")
    if Shiboken.isValid(info_action):
        raise AssertionError("InfoBar did not delete its hosted action")

    if status_info.Toast is not fluentqt.Toast:
        raise AssertionError("status_info did not re-export Toast")
    if status_info.ToolTip is not fluentqt.ToolTip:
        raise AssertionError("status_info did not re-export ToolTip")
    status_host = QWidget()
    status_host.resize(520, 300)
    status_anchor = QWidget(status_host)
    status_host.show()
    app.processEvents()

    tooltip = fluentqt.ToolTip.attach(status_anchor, "Installed tooltip")
    if tooltip.parent() is not status_anchor:
        raise AssertionError("ToolTip attachment lost target ownership")
    if fluentqt.ToolTip.attach(status_anchor, "Updated") is not tooltip:
        raise AssertionError("ToolTip attachment did not reuse its wrapper")

    direct_toast = fluentqt.Toast()
    direct_action = QAction("Open")
    direct_action_ref = weakref.ref(direct_action)
    direct_toast.setAction(direct_action)
    direct_toast.setAnimationEnabled(False)
    del direct_action
    gc.collect()
    if direct_toast.action() is not direct_action_ref():
        raise AssertionError("Toast did not retain its borrowed QAction")
    if not direct_toast.present(status_anchor):
        raise AssertionError("Direct Toast presentation failed")
    if direct_toast.parentWidget() is not status_host:
        raise AssertionError("Direct Toast used the child anchor as its host")
    direct_toast.dismiss()

    managed_toast = fluentqt.Toast.showOrUpdateToast(
        status_anchor,
        "wheel-status",
        "Preparing",
        durationMs=0,
    )
    updated_toast = fluentqt.Toast.showOrUpdateToast(
        status_anchor,
        "wheel-status",
        "Complete",
        severity=fluentqt.Toast.Severity.Success,
        durationMs=0,
    )
    if managed_toast is not updated_toast:
        raise AssertionError("Managed Toast update replaced its wrapper")
    if managed_toast.message() != "Complete":
        raise AssertionError("Managed Toast update lost its message")
    if managed_toast.parentWidget() is not status_host:
        raise AssertionError("Managed Toast did not use the actual host")

    status_anchor.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    app.processEvents()
    if not Shiboken.isValid(managed_toast):
        raise AssertionError("Managed Toast followed a transient child anchor")
    if Shiboken.isValid(tooltip):
        raise AssertionError("ToolTip outlived its target")
    managed_toast.setAnimationEnabled(False)
    managed_toast.dismiss()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    app.processEvents()
    if Shiboken.isValid(managed_toast):
        raise AssertionError("Managed Toast did not self-delete")

    status_host.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    app.processEvents()
    if Shiboken.isValid(direct_toast):
        raise AssertionError("Direct Toast outlived its top-level host")
    gc.collect()
    if direct_action_ref() is not None:
        raise AssertionError("Toast did not release its QAction wrapper")

    scroll_view = fluentqt.ScrollView()
    scroll_content = QWidget()
    scroll_view.setContentWidget(scroll_content)
    taken_content = scroll_view.takeContentWidget()
    if taken_content is not scroll_content:
        raise AssertionError("ScrollView did not preserve Python wrapper identity")
    if not Shiboken.ownedByPython(taken_content):
        raise AssertionError("ScrollView take did not return Python ownership")
    scroll_view_ref = weakref.ref(scroll_view)
    del scroll_view
    gc.collect()
    if scroll_view_ref() is not None:
        raise AssertionError("Taken ScrollView host survived Python GC")
    if not Shiboken.isValid(taken_content):
        raise AssertionError("Taken ScrollView content did not survive its host")
    taken_content_ref = weakref.ref(taken_content)
    del taken_content
    del scroll_content
    gc.collect()
    if taken_content_ref() is not None:
        raise AssertionError("Taken ScrollView content survived Python GC")

    owned_scroll_view = fluentqt.ScrollView()
    owned_scroll_content = QWidget()
    owned_scroll_view.setContentWidget(owned_scroll_content)
    owned_scroll_view_ref = weakref.ref(owned_scroll_view)
    del owned_scroll_view
    gc.collect()
    if owned_scroll_view_ref() is not None:
        raise AssertionError("Owned ScrollView host survived Python GC")
    if Shiboken.isValid(owned_scroll_content):
        raise AssertionError("ScrollView did not delete its owned content")

    borrowed_scroll_view = fluentqt.ScrollView()
    borrowed_scroll_content = QWidget()
    borrowed_scroll_view.setBorrowedContentWidget(
        borrowed_scroll_content
    )
    borrowed_scroll_view_ref = weakref.ref(borrowed_scroll_view)
    del borrowed_scroll_view
    gc.collect()
    if borrowed_scroll_view_ref() is not None:
        raise AssertionError("Borrowed ScrollView host survived Python GC")
    if not Shiboken.isValid(borrowed_scroll_content):
        raise AssertionError("ScrollView deleted borrowed content")
    if borrowed_scroll_content.parent() is not None:
        raise AssertionError("Borrowed content was not detached")

    original_parent = QWidget()
    reparented_scroll_content = QWidget(original_parent)
    reparented_scroll_view = fluentqt.ScrollView()
    reparented_scroll_view.setReparentedContentWidget(
        reparented_scroll_content
    )
    reparented_scroll_view_ref = weakref.ref(reparented_scroll_view)
    del reparented_scroll_view
    gc.collect()
    if reparented_scroll_view_ref() is not None:
        raise AssertionError("Reparented ScrollView host survived Python GC")
    if not Shiboken.isValid(reparented_scroll_content):
        raise AssertionError("ScrollView deleted reparented content")
    if reparented_scroll_content.parent() is not original_parent:
        raise AssertionError("ScrollView did not restore the original parent")

    expander = fluentqt.Expander()
    expander_content = QWidget()
    expander.setContentWidget(expander_content)
    taken_expander_content = expander.takeContentWidget()
    if taken_expander_content is not expander_content:
        raise AssertionError("Expander did not preserve wrapper identity")
    if taken_expander_content.parent() is not None:
        raise AssertionError("Expander take did not detach content")
    if not Shiboken.ownedByPython(taken_expander_content):
        raise AssertionError("Expander take did not return Python ownership")

    owned_expander = fluentqt.Expander()
    owned_expander_content = QWidget()
    owned_expander.setOwnedContentWidget(owned_expander_content)
    owned_expander_ref = weakref.ref(owned_expander)
    del owned_expander
    gc.collect()
    if owned_expander_ref() is not None:
        raise AssertionError("Owned Expander survived Python GC")
    if Shiboken.isValid(owned_expander_content):
        raise AssertionError("Expander did not delete owned content")

    borrowed_expander = fluentqt.Expander()
    borrowed_expander_content = QWidget()
    borrowed_expander.setBorrowedContentWidget(
        borrowed_expander_content
    )
    borrowed_expander_ref = weakref.ref(borrowed_expander)
    del borrowed_expander
    gc.collect()
    if borrowed_expander_ref() is not None:
        raise AssertionError("Borrowed Expander survived Python GC")
    if not Shiboken.isValid(borrowed_expander_content):
        raise AssertionError("Expander deleted borrowed content")
    if borrowed_expander_content.parent() is not None:
        raise AssertionError("Borrowed Expander content was not detached")

    expander_parent = QWidget()
    reparented_expander_content = QWidget(expander_parent)
    reparented_expander = fluentqt.Expander()
    reparented_expander.setReparentedContentWidget(
        reparented_expander_content
    )
    reparented_expander_ref = weakref.ref(reparented_expander)
    del reparented_expander
    gc.collect()
    if reparented_expander_ref() is not None:
        raise AssertionError("Reparented Expander survived Python GC")
    if reparented_expander_content.parent() is not expander_parent:
        raise AssertionError("Expander did not restore the original parent")

    field = fluentqt.Field()
    field_editor = fluentqt.LineEdit()
    field_editor.setText("keep-me")
    field.setValidationState(fluentqt.Field.ValidationState.Error)
    field.setValidationMessage("Required")
    field.setEditor(field_editor)
    taken_field_editor = field.takeEditor()
    if taken_field_editor is not field_editor:
        raise AssertionError("Field did not preserve editor wrapper identity")
    if taken_field_editor.text() != "keep-me":
        raise AssertionError("Field validation mutated the editor value")
    if taken_field_editor.parent() is not None:
        raise AssertionError("Field take did not detach the editor")
    if not Shiboken.ownedByPython(taken_field_editor):
        raise AssertionError("Field take did not return Python ownership")

    owned_field = fluentqt.Field()
    owned_field_editor = fluentqt.LineEdit()
    owned_field.setOwnedEditor(owned_field_editor)
    owned_field_ref = weakref.ref(owned_field)
    del owned_field
    gc.collect()
    if owned_field_ref() is not None:
        raise AssertionError("Owned Field survived Python GC")
    if Shiboken.isValid(owned_field_editor):
        raise AssertionError("Field did not delete its owned editor")

    field_parent = QWidget()
    reparented_field_editor = fluentqt.LineEdit(field_parent)
    reparented_field = fluentqt.Field()
    reparented_field.setReparentedEditor(reparented_field_editor)
    reparented_field_ref = weakref.ref(reparented_field)
    del reparented_field
    gc.collect()
    if reparented_field_ref() is not None:
        raise AssertionError("Reparented Field survived Python GC")
    if reparented_field_editor.parent() is not field_parent:
        raise AssertionError("Field did not restore the original parent")

    accordion = fluentqt.Accordion()
    borrowed_item = fluentqt.Expander()
    owned_item = fluentqt.Expander()
    if not accordion.addBorrowedItem(borrowed_item):
        raise AssertionError("Accordion rejected a borrowed item")
    if not accordion.insertOwnedItem(0, owned_item):
        raise AssertionError("Accordion rejected an owned item")
    if accordion.itemAt(0) is not owned_item:
        raise AssertionError("Accordion did not preserve item identity")
    if (
        accordion.itemOwnershipAt(0)
        != fluentqt.WidgetOwnership.Owned
    ):
        raise AssertionError("Accordion lost its owned item policy")
    taken_item = accordion.takeItem(0)
    if taken_item is not owned_item or taken_item.parent() is not None:
        raise AssertionError("Accordion take did not detach its item")
    if not Shiboken.ownedByPython(taken_item):
        raise AssertionError("Accordion take did not return Python ownership")
    accordion_ref = weakref.ref(accordion)
    del accordion
    gc.collect()
    if accordion_ref() is not None:
        raise AssertionError("Borrowed Accordion host survived Python GC")
    if not Shiboken.isValid(borrowed_item):
        raise AssertionError("Accordion deleted its borrowed item")
    if borrowed_item.parent() is not None:
        raise AssertionError("Accordion did not detach its borrowed item")

    owned_accordion = fluentqt.Accordion()
    deleted_item = fluentqt.Expander()
    owned_accordion.addOwnedItem(deleted_item)
    owned_accordion_ref = weakref.ref(owned_accordion)
    del owned_accordion
    gc.collect()
    if owned_accordion_ref() is not None:
        raise AssertionError("Owned Accordion host survived Python GC")
    if Shiboken.isValid(deleted_item):
        raise AssertionError("Accordion did not delete its owned item")

    item_parent = QWidget()
    reparented_item = fluentqt.Expander(item_parent)
    reparenting_accordion = fluentqt.Accordion()
    reparenting_accordion.addReparentedItem(reparented_item)
    reparenting_ref = weakref.ref(reparenting_accordion)
    del reparenting_accordion
    gc.collect()
    if reparenting_ref() is not None:
        raise AssertionError("Reparenting Accordion survived Python GC")
    if reparented_item.parent() is not item_parent:
        raise AssertionError("Accordion did not restore the item parent")

    if collections.FlowView is not fluentqt.FlowView:
        raise AssertionError("Collections module did not re-export FlowView")
    if collections.GridView is not fluentqt.GridView:
        raise AssertionError("Collections module did not re-export GridView")
    if collections.ListView is not fluentqt.ListView:
        raise AssertionError("Collections module did not re-export ListView")
    if collections.TreeView is not fluentqt.TreeView:
        raise AssertionError("Collections module did not re-export TreeView")
    if collections.SelectionMode is not fluentqt.SelectionMode:
        raise AssertionError(
            "Collections module did not re-export SelectionMode"
        )

    class WheelListModel(QAbstractListModel):
        def __init__(self):
            super().__init__()
            self.values = ["Alpha", "Beta"]
            self.data_calls = 0

        def rowCount(self, parent=QModelIndex()):
            return 0 if parent.isValid() else len(self.values)

        def data(self, index, role=Qt.DisplayRole):
            self.data_calls += 1
            if (
                index.isValid()
                and role == Qt.DisplayRole
                and 0 <= index.row() < len(self.values)
            ):
                return self.values[index.row()]
            return None

        def insertValue(self, row, value):
            self.beginInsertRows(QModelIndex(), row, row)
            self.values.insert(row, value)
            self.endInsertRows()

    class WheelListDelegate(QStyledItemDelegate):
        def sizeHint(self, option, index):
            del option, index
            return QSize(180, 37)

    report_stage("FlowView model and delegate")
    flow_view = fluentqt.FlowView(
        selectionMode=fluentqt.SelectionMode.Multiple,
        headerText="Wheel adaptive flow",
        defaultItemSize=QSize(180, 64),
    )
    flow_model = WheelListModel()
    flow_delegate = WheelListDelegate()
    flow_view.setModel(flow_model)
    flow_view.setItemDelegate(flow_delegate)
    flow_view.resize(420, 220)
    flow_view.show()
    app.processEvents()
    flow_model_ref = weakref.ref(flow_model)
    flow_delegate_ref = weakref.ref(flow_delegate)
    del flow_model
    del flow_delegate
    gc.collect()
    if flow_view.model() is not flow_model_ref():
        raise AssertionError("FlowView did not retain its caller-owned model")
    if flow_view.itemDelegate() is not flow_delegate_ref():
        raise AssertionError(
            "FlowView did not retain its caller-owned delegate"
        )
    if (
        flow_view.visualRect(flow_model_ref().index(0, 0)).size()
        != QSize(180, 37)
    ):
        raise AssertionError("FlowView did not dispatch Python sizeHint")
    flow_model_ref().insertValue(1, "Inserted")
    flow_view.setSelectedIndex(1)
    if (
        flow_model_ref().rowCount() != 3
        or flow_view.selectedRows() != [1]
        or flow_view.selectionMode() != fluentqt.SelectionMode.Multiple
    ):
        raise AssertionError(
            "FlowView did not preserve model insertion and selection"
        )
    flow_selection = QItemSelectionModel(flow_model_ref())
    flow_selection_ref = weakref.ref(flow_selection)
    flow_view.setSelectionModel(flow_selection)
    del flow_selection
    gc.collect()
    if flow_view.selectionModel() is not flow_selection_ref():
        raise AssertionError(
            "FlowView did not retain its caller-owned selection model"
        )
    flow_scroll_bar = flow_view.verticalFluentScrollBar()
    if not Shiboken.isValid(flow_scroll_bar):
        raise AssertionError("FlowView exposed an invalid Fluent scroll bar")
    if Shiboken.ownedByPython(flow_scroll_bar):
        raise AssertionError(
            "FlowView transferred its internal scroll bar to Python"
        )
    flow_view_ref = weakref.ref(flow_view)
    flow_view.close()
    del flow_scroll_bar
    del flow_view
    app.processEvents()
    gc.collect()
    if flow_view_ref() is not None:
        raise AssertionError("FlowView survived Python GC")
    if flow_model_ref() is not None:
        raise AssertionError("FlowView model survived after host release")
    if flow_delegate_ref() is not None:
        raise AssertionError("FlowView delegate survived after host release")
    if flow_selection_ref() is not None:
        raise AssertionError(
            "FlowView selection model survived after host release"
        )

    stale_flow_view = fluentqt.FlowView()
    stale_flow_delegate = QStyledItemDelegate()
    stale_flow_view.setItemDelegate(stale_flow_delegate)
    stale_flow_callback = stale_flow_view._fluentqt_item_delegate_destroyed
    stale_flow_delegate.destroyed.disconnect(stale_flow_callback)
    stale_flow_delegate.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    app.processEvents()
    if Shiboken.isValid(stale_flow_delegate):
        raise AssertionError("FlowView delegate survived deferred deletion")
    if stale_flow_view.itemDelegate() is not None:
        raise AssertionError("FlowView returned an invalid retained delegate")
    del stale_flow_callback
    del stale_flow_delegate
    del stale_flow_view
    gc.collect()
    report_stage("FlowView lifecycle complete")

    report_stage("GridView model and delegate")
    grid_view = fluentqt.GridView(
        selectionMode=fluentqt.SelectionMode.Multiple,
        headerText="Wheel grid",
        cellSize=QSize(120, 84),
        maxColumns=2,
    )
    grid_model = WheelListModel()
    grid_delegate = WheelListDelegate()
    grid_view.setModel(grid_model)
    grid_view.setItemDelegate(grid_delegate)
    grid_model_ref = weakref.ref(grid_model)
    grid_delegate_ref = weakref.ref(grid_delegate)
    del grid_model
    del grid_delegate
    gc.collect()
    if grid_view.model() is not grid_model_ref():
        raise AssertionError("GridView did not retain its caller-owned model")
    if grid_view.itemDelegate() is not grid_delegate_ref():
        raise AssertionError(
            "GridView did not retain its caller-owned delegate"
        )
    if (
        grid_view.sizeHintForIndex(grid_model_ref().index(0, 0))
        != QSize(180, 37)
    ):
        raise AssertionError("GridView did not dispatch Python sizeHint")
    grid_model_ref().insertValue(1, "Inserted")
    grid_view.setSelectedIndex(1)
    if (
        grid_model_ref().rowCount() != 3
        or grid_view.selectedRows() != [1]
        or grid_view.selectionMode() != fluentqt.SelectionMode.Multiple
    ):
        raise AssertionError(
            "GridView did not preserve model insertion and selection"
        )
    grid_selection = QItemSelectionModel(grid_model_ref())
    grid_selection_ref = weakref.ref(grid_selection)
    grid_view.setSelectionModel(grid_selection)
    del grid_selection
    gc.collect()
    if grid_view.selectionModel() is not grid_selection_ref():
        raise AssertionError(
            "GridView did not retain its caller-owned selection model"
        )
    grid_scroll_bar = grid_view.verticalFluentScrollBar()
    if not Shiboken.isValid(grid_scroll_bar):
        raise AssertionError("GridView exposed an invalid Fluent scroll bar")
    if Shiboken.ownedByPython(grid_scroll_bar):
        raise AssertionError(
            "GridView transferred its internal scroll bar to Python"
        )
    grid_view_ref = weakref.ref(grid_view)
    report_stage("GridView host release")
    grid_view.close()
    del grid_scroll_bar
    del grid_view
    app.processEvents()
    gc.collect()
    if grid_view_ref() is not None:
        raise AssertionError("GridView survived Python GC")
    if grid_model_ref() is not None:
        raise AssertionError("GridView model survived after host release")
    if grid_delegate_ref() is not None:
        raise AssertionError("GridView delegate survived after host release")
    if grid_selection_ref() is not None:
        raise AssertionError(
            "GridView selection model survived after host release"
        )
    report_stage("GridView external delegate release")

    stale_delegate_view = fluentqt.GridView()
    stale_delegate = QStyledItemDelegate()
    stale_delegate_view.setItemDelegate(stale_delegate)
    stale_callback = stale_delegate_view._fluentqt_item_delegate_destroyed
    stale_delegate.destroyed.disconnect(stale_callback)
    stale_delegate.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    app.processEvents()
    if Shiboken.isValid(stale_delegate):
        raise AssertionError("GridView delegate survived deferred deletion")
    if stale_delegate_view.itemDelegate() is not None:
        raise AssertionError("GridView returned an invalid retained delegate")
    del stale_callback
    del stale_delegate
    del stale_delegate_view
    gc.collect()
    report_stage("GridView lifecycle complete")

    report_stage("TreeView model and delegate")
    tree_view = fluentqt.TreeView(
        selectionMode=fluentqt.SelectionMode.Multiple,
        headerText="Wheel hierarchy",
    )
    tree_model = QStandardItemModel()
    tree_parent = QStandardItem("Parent")
    tree_parent.appendRow(QStandardItem("Child"))
    tree_model.appendRow(tree_parent)
    tree_delegate = WheelListDelegate()
    tree_view.setModel(tree_model)
    tree_view.setItemDelegate(tree_delegate)
    tree_model_ref = weakref.ref(tree_model)
    tree_delegate_ref = weakref.ref(tree_delegate)
    del tree_model
    del tree_delegate
    gc.collect()
    if tree_view.model() is not tree_model_ref():
        raise AssertionError("TreeView did not retain its caller-owned model")
    if tree_view.itemDelegate() is not tree_delegate_ref():
        raise AssertionError(
            "TreeView did not retain its caller-owned delegate"
        )
    tree_parent_index = tree_model_ref().index(0, 0)
    tree_child_index = tree_model_ref().index(0, 0, tree_parent_index)
    tree_view.expandAll()
    tree_view.setSelectedItem(tree_child_index)
    if tree_view.selectedItem() != tree_child_index:
        raise AssertionError("TreeView lost hierarchical selection")
    if tree_model_ref().rowCount(tree_parent_index) != 1:
        raise AssertionError("TreeView did not preserve child rows")
    if tree_view.sizeHintForIndex(tree_child_index) != QSize(180, 37):
        raise AssertionError("TreeView did not dispatch Python sizeHint")
    tree_selection = QItemSelectionModel(tree_model_ref())
    tree_selection_ref = weakref.ref(tree_selection)
    tree_view.setSelectionModel(tree_selection)
    del tree_selection
    gc.collect()
    if tree_view.selectionModel() is not tree_selection_ref():
        raise AssertionError(
            "TreeView did not retain its caller-owned selection model"
        )
    tree_scroll_bars = (
        tree_view.verticalFluentScrollBar(),
        tree_view.horizontalFluentScrollBar(),
    )
    if any(not Shiboken.isValid(bar) for bar in tree_scroll_bars):
        raise AssertionError("TreeView exposed an invalid Fluent scroll bar")
    if any(Shiboken.ownedByPython(bar) for bar in tree_scroll_bars):
        raise AssertionError(
            "TreeView transferred an internal scroll bar to Python"
        )
    if hasattr(tree_view, "selectionIndicatorStyle"):
        raise AssertionError("TreeView exposed its internal indicator style")
    del tree_parent, tree_parent_index, tree_child_index
    tree_view_ref = weakref.ref(tree_view)
    report_stage("TreeView host release")
    tree_view.close()
    del tree_scroll_bars
    del tree_view
    app.processEvents()
    gc.collect()
    if tree_view_ref() is not None:
        raise AssertionError("TreeView survived Python GC")
    if tree_model_ref() is not None:
        raise AssertionError("TreeView model survived after host release")
    if tree_delegate_ref() is not None:
        raise AssertionError("TreeView delegate survived after host release")
    if tree_selection_ref() is not None:
        raise AssertionError(
            "TreeView selection model survived after host release"
        )
    report_stage("TreeView external delegate release")

    stale_tree_view = fluentqt.TreeView()
    stale_tree_delegate = QStyledItemDelegate()
    stale_tree_view.setItemDelegate(stale_tree_delegate)
    stale_tree_callback = stale_tree_view._fluentqt_item_delegate_destroyed
    stale_tree_delegate.destroyed.disconnect(stale_tree_callback)
    stale_tree_delegate.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    app.processEvents()
    if Shiboken.isValid(stale_tree_delegate):
        raise AssertionError("TreeView delegate survived deferred deletion")
    if stale_tree_view.itemDelegate() is not None:
        raise AssertionError("TreeView returned an invalid retained delegate")
    del stale_tree_callback
    del stale_tree_delegate
    del stale_tree_view
    gc.collect()
    report_stage("TreeView lifecycle complete")

    report_stage("ListView model and delegate")
    list_view = fluentqt.ListView(
        selectionMode=fluentqt.SelectionMode.Single
    )
    list_model = WheelListModel()
    list_delegate = WheelListDelegate()
    list_view.setModel(list_model)
    list_view.setItemDelegate(list_delegate)
    list_model_ref = weakref.ref(list_model)
    list_delegate_ref = weakref.ref(list_delegate)
    del list_model
    del list_delegate
    gc.collect()
    if list_view.model() is not list_model_ref():
        raise AssertionError("ListView did not retain its caller-owned model")
    if list_view.itemDelegate() is not list_delegate_ref():
        raise AssertionError(
            "ListView did not retain its caller-owned delegate"
        )
    if list_view.sizeHintForRow(0) != 37:
        raise AssertionError("ListView did not dispatch Python sizeHint")
    first_index = list_view.model().index(0, 0)
    if list_view.model().data(first_index, Qt.DisplayRole) != "Alpha":
        raise AssertionError("ListView did not read Python model data")
    if list_model_ref().data_calls == 0:
        raise AssertionError("ListView did not dispatch Python model data")

    list_model_ref().insertValue(1, "Inserted")
    list_view.setSelectedIndex(1)
    if (
        list_model_ref().rowCount() != 3
        or list_view.selectedRows() != [1]
        or list_view.selectionMode() != fluentqt.SelectionMode.Single
    ):
        raise AssertionError(
            "ListView did not preserve model insertion and selection"
        )

    list_selection = QItemSelectionModel(list_model_ref())
    list_selection_ref = weakref.ref(list_selection)
    list_view.setSelectionModel(list_selection)
    del list_selection
    gc.collect()
    if list_view.selectionModel() is not list_selection_ref():
        raise AssertionError(
            "ListView did not retain its caller-owned selection model"
        )
    list_scroll_bars = (
        list_view.verticalFluentScrollBar(),
        list_view.horizontalFluentScrollBar(),
    )
    if any(not Shiboken.isValid(bar) for bar in list_scroll_bars):
        raise AssertionError("ListView exposed an invalid Fluent scroll bar")
    if any(Shiboken.ownedByPython(bar) for bar in list_scroll_bars):
        raise AssertionError(
            "ListView transferred an internal scroll bar to Python"
        )
    list_view.setSectionKeyFunction(
        lambda row: list_model_ref().data(
            list_model_ref().index(row, 0), Qt.DisplayRole
        )[0]
    )
    list_view.setSectionEnabled(True)
    if not list_view.sectionEnabled() or not list_view.isSectionEnabled():
        raise AssertionError("ListView did not enable Python section grouping")
    list_model_ref().insertValue(0, "Another")
    list_view.setSectionKeyFunction(None)
    list_view.setSectionEnabled(False)
    for unsupported in ("header", "setHeader", "footer", "setFooter"):
        if hasattr(list_view, unsupported):
            raise AssertionError(
                "ListView exposed unsupported API: {0}".format(
                    unsupported
                )
            )

    list_view_ref = weakref.ref(list_view)
    report_stage("ListView host release")
    list_view.close()
    del list_scroll_bars
    del list_view
    app.processEvents()
    gc.collect()
    if list_view_ref() is not None:
        raise AssertionError("ListView survived Python GC")
    if list_model_ref() is not None:
        raise AssertionError("ListView model survived after host release")
    if list_delegate_ref() is not None:
        raise AssertionError("ListView delegate survived after host release")
    if list_selection_ref() is not None:
        raise AssertionError(
            "ListView selection model survived after host release"
        )
    report_stage("ListView lifecycle complete")

    stack_view = fluentqt.StackView()
    stack_view.setTransitionAnimationEnabled(False)
    borrowed_page = QWidget()
    stack_parent = QWidget()
    reparented_page = QWidget(stack_parent)
    owned_page = QWidget()
    if not stack_view.pushBorrowedItem(borrowed_page):
        raise AssertionError("StackView rejected a borrowed page")
    if not stack_view.pushReparentedItem(reparented_page):
        raise AssertionError("StackView rejected a reparented page")
    if stack_view.currentItem() is not reparented_page:
        raise AssertionError("StackView lost current page identity")
    if not stack_view.pop():
        raise AssertionError("StackView could not pop a hosted page")
    if reparented_page.parent() is not stack_parent:
        raise AssertionError("StackView did not restore the page parent")
    if not stack_view.pushOwnedItem(owned_page):
        raise AssertionError("StackView rejected an owned page")
    if stack_view.depth() != 2:
        raise AssertionError("StackView reported an unexpected depth")
    stack_view_ref = weakref.ref(stack_view)
    del stack_view
    gc.collect()
    if stack_view_ref() is not None:
        raise AssertionError("StackView survived Python GC")
    if Shiboken.isValid(owned_page):
        raise AssertionError("StackView did not delete its owned page")
    if not Shiboken.isValid(borrowed_page):
        raise AssertionError("StackView deleted its borrowed page")
    if borrowed_page.parent() is not None:
        raise AssertionError("StackView did not detach its borrowed page")

    report_stage("NavigationView page and chrome ownership")
    if navigation.NavigationView is not fluentqt.NavigationView:
        raise AssertionError(
            "Navigation module did not re-export NavigationView"
        )
    if navigation.StackContentHost is not fluentqt.StackContentHost:
        raise AssertionError(
            "Navigation module did not re-export StackContentHost"
        )

    navigation_view = fluentqt.NavigationView()
    content_host = navigation_view.contentHost()
    if not isinstance(content_host, fluentqt.StackContentHost):
        raise AssertionError(
            "NavigationView did not expose its native StackContentHost"
        )

    owned_navigation_page = QWidget()
    borrowed_navigation_page = QWidget()
    page_parent = QWidget()
    reparented_navigation_page = QWidget(page_parent)
    if not content_host.addOwnedPage(owned_navigation_page):
        raise AssertionError("StackContentHost rejected an Owned page")
    if not content_host.addBorrowedPage(borrowed_navigation_page):
        raise AssertionError("StackContentHost rejected a Borrowed page")
    if not content_host.addReparentedPage(reparented_navigation_page):
        raise AssertionError("StackContentHost rejected a Reparented page")
    content_host.setCurrentIndex(2, 1, False)
    if content_host.currentIndex() != 2:
        raise AssertionError("StackContentHost did not navigate to the page")
    if (
        content_host.pageOwnershipAt(2)
        != fluentqt.WidgetOwnership.Reparented
    ):
        raise AssertionError("StackContentHost lost its ownership policy")

    owned_chrome = QWidget()
    borrowed_chrome = QWidget()
    chrome_parent = QWidget()
    reparented_chrome = QWidget(chrome_parent)
    if not navigation_view.setOwnedHeaderChromeWidget(owned_chrome):
        raise AssertionError("NavigationView rejected Owned header chrome")
    if not navigation_view.setBorrowedMainChromeWidget(borrowed_chrome):
        raise AssertionError("NavigationView rejected Borrowed main chrome")
    if not navigation_view.setReparentedFooterChromeWidget(
        reparented_chrome
    ):
        raise AssertionError(
            "NavigationView rejected Reparented footer chrome"
        )

    navigation_view_ref = weakref.ref(navigation_view)
    del content_host
    del navigation_view
    gc.collect()
    if navigation_view_ref() is not None:
        raise AssertionError("NavigationView survived Python GC")
    if Shiboken.isValid(owned_navigation_page):
        raise AssertionError("StackContentHost did not delete its Owned page")
    if not Shiboken.isValid(borrowed_navigation_page):
        raise AssertionError("StackContentHost deleted its Borrowed page")
    if borrowed_navigation_page.parent() is not None:
        raise AssertionError("StackContentHost did not detach Borrowed page")
    if reparented_navigation_page.parent() is not page_parent:
        raise AssertionError(
            "StackContentHost did not restore Reparented page parent"
        )
    if Shiboken.isValid(owned_chrome):
        raise AssertionError("NavigationView did not delete Owned chrome")
    if not Shiboken.isValid(borrowed_chrome):
        raise AssertionError("NavigationView deleted Borrowed chrome")
    if borrowed_chrome.parent() is not None:
        raise AssertionError("NavigationView did not detach Borrowed chrome")
    if reparented_chrome.parent() is not chrome_parent:
        raise AssertionError(
            "NavigationView did not restore Reparented chrome parent"
        )
    report_stage("NavigationView lifecycle complete")

    if navigation.Breadcrumb is not fluentqt.Breadcrumb:
        raise AssertionError("Navigation module did not re-export Breadcrumb")
    if navigation.BreadcrumbItem is not fluentqt.BreadcrumbItem:
        raise AssertionError(
            "Navigation module did not re-export BreadcrumbItem"
        )

    breadcrumb_item = fluentqt.BreadcrumbItem(
        "Projects",
        {"source": "wheel"},
        True,
        "Projects folder",
    )
    if fluentqt.BreadcrumbItem(breadcrumb_item) != breadcrumb_item:
        raise AssertionError("BreadcrumbItem copy lost value equality")
    if breadcrumb_item.data != {"source": "wheel"}:
        raise AssertionError("BreadcrumbItem lost QVariant metadata")
    try:
        hash(breadcrumb_item)
    except TypeError:
        pass
    else:
        raise AssertionError(
            "Mutable BreadcrumbItem unexpectedly remained hashable"
        )

    breadcrumb = fluentqt.Breadcrumb()
    breadcrumb.setItems(
        [
            fluentqt.BreadcrumbItem("Home", {"source": "metadata-list"}),
            breadcrumb_item,
        ]
    )
    if [item.text for item in breadcrumb.items()] != ["Home", "Projects"]:
        raise AssertionError(
            "Breadcrumb metadata list dispatch produced empty labels"
        )
    if breadcrumb.itemAt(1).data != {"source": "wheel"}:
        raise AssertionError("Breadcrumb metadata list lost QVariant data")
    breadcrumb.setItems(["Home", "Workspace"])
    breadcrumb.appendItem(breadcrumb_item)
    breadcrumb.setOverflowMode(
        fluentqt.Breadcrumb.OverflowMode.Middle
    )
    breadcrumb.resize(120, 20)
    breadcrumb.show()
    app.processEvents()
    if breadcrumb.itemAt(2) != breadcrumb_item:
        raise AssertionError("Breadcrumb did not preserve item metadata")
    if not breadcrumb.hiddenItemIndexes():
        raise AssertionError("Breadcrumb did not expose overflow indexes")
    if breadcrumb.overflowGeometry().isEmpty():
        raise AssertionError("Breadcrumb did not expose overflow geometry")

    for name in ("Pivot", "PivotItem", "SelectorBar", "SelectorBarItem"):
        public_type = getattr(fluentqt, name)
        if getattr(navigation, name) is not public_type:
            raise AssertionError(
                "Navigation module did not re-export {0}".format(name)
            )
        if getattr(native.fluent, name) is not public_type:
            raise AssertionError(
                "Native module identity changed for {0}".format(name)
            )

    pivot_item = fluentqt.PivotItem(
        "Unread",
        "mail-glyph",
        True,
        {"filter": "unread"},
        "Unread messages",
    )
    selector_item = fluentqt.SelectorBarItem(
        "Activity",
        "activity-glyph",
        True,
        True,
        {"route": "activity"},
        "Activity timeline",
    )
    for value_type, value, expected_data in (
        (fluentqt.PivotItem, pivot_item, {"filter": "unread"}),
        (
            fluentqt.SelectorBarItem,
            selector_item,
            {"route": "activity"},
        ),
    ):
        if value_type(value) != value or value.data != expected_data:
            raise AssertionError(
                "{0} lost value equality or QVariant metadata".format(
                    value_type.__name__
                )
            )
        try:
            hash(value)
        except TypeError:
            pass
        else:
            raise AssertionError(
                "Mutable {0} unexpectedly remained hashable".format(
                    value_type.__name__
                )
            )

    pivot = fluentqt.Pivot()
    pivot.addItem("All messages")
    pivot.addItem(pivot_item)
    pivot.insertItem(1, fluentqt.PivotItem("Flagged"))
    for index in range(4):
        pivot.addItem("Mailbox section {0}".format(index))
    pivot.setSelectedIndex(2)
    pivot.setOverflowBehavior(fluentqt.Pivot.OverflowBehavior.MoreButton)
    pivot.resize(220, 44)
    pivot.show()

    selector = fluentqt.SelectorBar()
    selector.addItem("Overview")
    selector.addItem(selector_item)
    selector.insertItem(1, fluentqt.SelectorBarItem("Files"))
    for index in range(4):
        selector.addItem("Workspace {0}".format(index))
    selector.setItemSelected(2, True)
    selector.setOverflowBehavior(
        fluentqt.SelectorBar.OverflowBehavior.MoreButton
    )
    selector.resize(220, 44)
    selector.show()
    app.processEvents()

    if pivot.itemAt(2).data != {"filter": "unread"}:
        raise AssertionError("Pivot lost QVariant item metadata")
    if pivot.selectedIndex() != 2 or not pivot.hiddenItemIndexes():
        raise AssertionError("Pivot lost selection or overflow state")
    if pivot.overflowGeometry().isEmpty():
        raise AssertionError("Pivot did not expose MoreButton geometry")
    if selector.itemAt(2).data != {"route": "activity"}:
        raise AssertionError("SelectorBar lost QVariant item metadata")
    if selector.selectedIndex() != 2 or not selector.hiddenItemIndexes():
        raise AssertionError("SelectorBar lost selection or overflow state")
    if selector.overflowGeometry().isEmpty():
        raise AssertionError("SelectorBar did not expose MoreButton geometry")

    if navigation.TabView is not fluentqt.TabView:
        raise AssertionError("Navigation module did not re-export TabView")
    if navigation.TabViewItem is not fluentqt.TabViewItem:
        raise AssertionError("Navigation module did not re-export TabViewItem")
    if "TabStrip" in dir(native.fluent):
        raise AssertionError("Wheel exposed the internal TabStrip")

    tab_item = fluentqt.TabViewItem(
        "Metadata",
        "metadata-glyph",
        True,
        True,
        {"source": "wheel"},
        "Metadata tab",
    )
    if fluentqt.TabViewItem(tab_item) != tab_item:
        raise AssertionError("TabViewItem copy lost value equality")
    if tab_item.data != {"source": "wheel"}:
        raise AssertionError("TabViewItem lost QVariant metadata")
    try:
        hash(tab_item)
    except TypeError:
        pass
    else:
        raise AssertionError("Mutable TabViewItem unexpectedly remained hashable")

    tab_view = fluentqt.TabView()
    current_changes = []
    moved_tabs = []
    tab_view.currentChanged.connect(current_changes.append)
    tab_view.tabMoved.connect(
        lambda start, end: moved_tabs.append((start, end))
    )
    if tab_view.addTab("First") != 0 or tab_view.addTab(tab_item) != 1:
        raise AssertionError("TabView rejected tab metadata")
    tab_view.setSelectedIndex(1)
    if tab_view.tabAt(1) != tab_item:
        raise AssertionError("TabView did not preserve TabViewItem metadata")
    if not tab_view.moveTab(1, 0) or moved_tabs != [(1, 0)]:
        raise AssertionError("TabView did not emit a native reorder")
    if tab_view.selectedIndex() != 0:
        raise AssertionError("TabView did not move the selected index")
    if not tab_view.closeTab(0) or tab_view.tabCount() != 1:
        raise AssertionError("TabView did not close a metadata tab")
    if current_changes != [0, 1, 0, 0]:
        raise AssertionError(
            "TabView emitted unexpected selection navigation: {0}".format(
                current_changes
            )
        )

    previous_theme = fluentqt.current_theme()
    try:
        fluentqt.set_theme(fluentqt.Theme.Dark)
        fluentqt.apply_style_theme(fluentqt.StyleTheme.Material)
        if (
            fluentqt.current_design_language()
            != fluentqt.DesignLanguage.DesignMaterial
        ):
            raise AssertionError("Installed theme adapter did not update tokens")
    finally:
        fluentqt.reset_theme_tokens()
        fluentqt.set_theme(previous_theme)

    if windowing.TitleBar is not fluentqt.TitleBar:
        raise AssertionError("TitleBar category export does not match root API")

    window = fluentqt.Window()
    title_bar = window.titleBar()
    if not isinstance(title_bar, fluentqt.TitleBar):
        raise AssertionError("Window did not expose its native TitleBar")
    if title_bar.window() is not window or Shiboken.ownedByPython(title_bar):
        raise AssertionError("Window TitleBar lost its Qt-owned lifecycle")
    if hasattr(fluentqt.TitleBar, "onThemeUpdated"):
        raise AssertionError("TitleBar exposed its internal theme hook")

    title_content = QWidget()
    title_bar.setContentWidget(title_content)
    if title_bar.contentWidget() is not title_content:
        raise AssertionError("TitleBar did not retain installed content")
    if title_content.parent() is not title_bar:
        raise AssertionError("TitleBar content has the wrong Qt parent")

    window.setBackdropEffect(fluentqt.BackdropEffect.Solid)
    solid_state = window.backdropState()
    if (
        solid_state.backend != fluentqt.BackdropBackend.Solid
        or solid_state.fidelity != fluentqt.BackdropFidelity.Solid
        or solid_state.surfaceMode
        != fluentqt.BackdropSurfaceMode.SolidOpaque
        or solid_state.platformApplied
    ):
        raise AssertionError("Window Solid backdrop state is inconsistent")
    window.setBackdropEffect(fluentqt.BackdropEffect.Mica)
    mica_state = window.backdropState()
    if mica_state.requestedEffect != fluentqt.BackdropEffect.Mica:
        raise AssertionError("Window lost the requested Mica effect")

    child = QWidget()
    window.setContentWidget(child)
    window_ref = weakref.ref(window)
    del window
    gc.collect()
    if window_ref() is not None:
        raise AssertionError("Window survived Python GC")
    if Shiboken.isValid(child):
        raise AssertionError("Window did not own its installed content widget")
    if Shiboken.isValid(title_bar) or Shiboken.isValid(title_content):
        raise AssertionError("Window did not destroy its TitleBar hierarchy")

    report_stage("runtime dependencies")
    verify_windows_runtime_dependencies()
    verify_macos_runtime_dependencies()

    print(
        "FluentQt {0} wheel smoke passed with PySide6 {1} / Qt {2}".format(
            expected_version,
            PySide6.__version__,
            qVersion(),
        ),
        flush=True,
    )
    app.processEvents()


if __name__ == "__main__":
    main()
