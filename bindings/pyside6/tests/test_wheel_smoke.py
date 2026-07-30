"""Smoke-test an installed FluentQt wheel without using the source tree."""

import gc
from importlib import metadata
import os
from pathlib import Path
import sys
import weakref

import fluentqt
import fluentqt._fluentqt as native
import PySide6
import shiboken6

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QDate, Qt, qVersion
from PySide6.QtGui import QColor
from PySide6.QtWidgets import QApplication, QWidget
from shiboken6 import Shiboken


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


def main():
    require_installed_below_prefix(fluentqt.__file__)
    require_installed_below_prefix(native.__file__)

    expected_version = os.environ["FLUENTQT_EXPECTED_VERSION"]
    if metadata.version("FluentQt") != expected_version:
        raise AssertionError("Installed wheel metadata has the wrong version")

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

    controls = [
        fluentqt.Accordion(),
        fluentqt.AnnotatedScrollBar(),
        fluentqt.Avatar("Ada Lovelace"),
        fluentqt.Button("Button"),
        fluentqt.CalendarView(),
        fluentqt.CheckBox("CheckBox"),
        fluentqt.ColorPicker(),
        fluentqt.CompoundButton("Install", "Download and restart"),
        fluentqt.FontIcon("ic_fluent_settings_20_regular"),
        fluentqt.HyperlinkButton("HyperlinkButton"),
        fluentqt.RadioButton("RadioButton"),
        fluentqt.RepeatButton("RepeatButton"),
        fluentqt.Slider(Qt.Horizontal),
        fluentqt.ToggleButton("ToggleButton"),
        fluentqt.ToggleSwitch(),
        fluentqt.Label("Label"),
        fluentqt.LineEdit(),
        fluentqt.NumberBox(),
        fluentqt.PasswordBox(),
        fluentqt.TextEdit(),
        fluentqt.InfoBadge(),
        fluentqt.InfoBar(),
        fluentqt.ProgressBar(),
        fluentqt.ProgressRing(),
        fluentqt.RatingControl(),
        fluentqt.Shimmer(),
        fluentqt.Card(),
        fluentqt.Divider(),
        fluentqt.Expander(),
        fluentqt.PipsPager(),
        fluentqt.ScrollBar(Qt.Horizontal),
    ]
    if any(not Shiboken.isValid(control) for control in controls):
        raise AssertionError("A wheel-installed component has an invalid wrapper")

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
    for provider_method in (
        "setDetailLabelProvider",
        "clearDetailLabelProvider",
        "hasDetailLabelProvider",
    ):
        if hasattr(annotated, provider_method):
            raise AssertionError(
                "AnnotatedScrollBar exposed unsupported provider API: {0}".format(
                    provider_method
                )
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

    window = fluentqt.Window()
    child = QWidget()
    window.setContentWidget(child)
    window_ref = weakref.ref(window)
    del window
    gc.collect()
    if window_ref() is not None:
        raise AssertionError("Window survived Python GC")
    if Shiboken.isValid(child):
        raise AssertionError("Window did not own its installed content widget")

    verify_windows_runtime_dependencies()
    verify_macos_runtime_dependencies()

    print(
        "FluentQt {0} wheel smoke passed with PySide6 {1} / Qt {2}".format(
            expected_version,
            PySide6.__version__,
            qVersion(),
        )
    )
    app.processEvents()


if __name__ == "__main__":
    main()
