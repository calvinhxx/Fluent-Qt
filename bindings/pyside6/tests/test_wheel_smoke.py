"""Smoke-test an installed FluentQt wheel without using the source tree."""

import gc
from importlib import metadata
import os
from pathlib import Path
import sys
import weakref

import fluentqt
import fluentqt._fluentqt as native
import fluentqt.collections as collections
import fluentqt.navigation as navigation
import PySide6
import shiboken6

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import (
    QAbstractListModel,
    QCoreApplication,
    QDate,
    QEvent,
    QItemSelectionModel,
    QModelIndex,
    QSize,
    Qt,
    qVersion,
)
from PySide6.QtGui import QColor, QStandardItem, QStandardItemModel
from PySide6.QtWidgets import QApplication, QStyledItemDelegate, QWidget
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


def report_stage(name):
    print("FluentQt wheel smoke stage: {0}".format(name), flush=True)


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

    report_stage("public controls")
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
    ]
    if any(not Shiboken.isValid(control) for control in controls):
        raise AssertionError("A wheel-installed component has an invalid wrapper")
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
    for unsupported in (
        "header",
        "setHeader",
        "footer",
        "setFooter",
        "sectionEnabled",
        "isSectionEnabled",
        "setSectionEnabled",
        "setSectionKeyFunction",
    ):
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
