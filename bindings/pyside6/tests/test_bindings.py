import gc
import json
import os
from pathlib import Path
import unittest
import weakref

import fluentqt
import fluentqt._fluentqt as native
import PySide6
import shiboken6
import shiboken6_generator

fluentqt.prepare_high_dpi_application()

from fluentqt import (
    basicinput,
    collections,
    date_time,
    foundation,
    layout,
    navigation,
    scrolling,
    status_info,
    textfields,
    windowing,
)
from PySide6.QtCore import (
    QAbstractListModel,
    QByteArray,
    QCoreApplication,
    QDate,
    QEvent,
    QEventLoop,
    QItemSelectionModel,
    QLocale,
    QMargins,
    QModelIndex,
    QPoint,
    QPersistentModelIndex,
    QSize,
    QStandardPaths,
    QStringListModel,
    QTimer,
    Qt,
    QUrl,
    qVersion,
)
from PySide6.QtGui import QColor, QStandardItem, QStandardItemModel
from PySide6.QtTest import QTest
from PySide6.QtWidgets import (
    QApplication,
    QAbstractItemView,
    QCheckBox,
    QFrame,
    QLabel,
    QLineEdit,
    QListView,
    QPushButton,
    QRadioButton,
    QScrollArea,
    QScrollBar,
    QSlider,
    QStackedWidget,
    QStyledItemDelegate,
    QTreeView,
    QWidget,
)
from shiboken6 import Shiboken


def wait_for_events(duration_ms):
    loop = QEventLoop()
    QTimer.singleShot(duration_ms, loop.quit)
    loop.exec()


class FluentQtBindingTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        QStandardPaths.setTestModeEnabled(True)
        cls.app = QApplication.instance() or QApplication([])
        if not fluentqt.initialize_resources():
            raise RuntimeError("FluentQt resources could not be initialized")

    def _assert_item_view_gc_stress(self, view_type, model_factory):
        class PythonLifetimeDelegate(QStyledItemDelegate):
            pass

        for _ in range(25):
            view = view_type()
            model = model_factory()
            delegate = PythonLifetimeDelegate()
            selection = QItemSelectionModel(model)
            view.setModel(model)
            view.setItemDelegate(delegate)
            view.setSelectionModel(selection)

            model_ref = weakref.ref(model)
            delegate_ref = weakref.ref(delegate)
            selection_ref = weakref.ref(selection)
            del model
            del delegate
            del selection
            gc.collect()
            self.assertIs(view.model(), model_ref())
            self.assertIs(view.itemDelegate(), delegate_ref())
            self.assertIs(view.selectionModel(), selection_ref())

            view_ref = weakref.ref(view)
            del view
            self.app.processEvents()
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertIsNone(delegate_ref())
            self.assertIsNone(selection_ref())
            self.assertIsNone(model_ref())

    def test_public_types_and_build_versions(self):
        self.assertTrue(issubclass(fluentqt.Accordion, QWidget))
        self.assertTrue(issubclass(fluentqt.Avatar, QWidget))
        self.assertTrue(issubclass(fluentqt.Breadcrumb, QWidget))
        self.assertTrue(issubclass(fluentqt.Button, QPushButton))
        self.assertTrue(issubclass(fluentqt.CalendarView, QWidget))
        self.assertTrue(issubclass(fluentqt.CheckBox, QCheckBox))
        self.assertTrue(issubclass(fluentqt.ColorPicker, QWidget))
        self.assertTrue(
            issubclass(fluentqt.CompoundButton, fluentqt.Button)
        )
        self.assertTrue(issubclass(fluentqt.HyperlinkButton, QPushButton))
        self.assertTrue(issubclass(fluentqt.RadioButton, QRadioButton))
        self.assertTrue(issubclass(fluentqt.RatingControl, QWidget))
        self.assertTrue(issubclass(fluentqt.RepeatButton, QPushButton))
        self.assertTrue(issubclass(fluentqt.Slider, QSlider))
        self.assertTrue(issubclass(fluentqt.ToggleButton, QPushButton))
        self.assertTrue(issubclass(fluentqt.ToggleSwitch, QWidget))
        self.assertTrue(issubclass(fluentqt.Label, QLabel))
        self.assertTrue(issubclass(fluentqt.LineEdit, QLineEdit))
        self.assertTrue(issubclass(fluentqt.NumberBox, QLineEdit))
        self.assertTrue(issubclass(fluentqt.PasswordBox, QLineEdit))
        self.assertTrue(issubclass(fluentqt.Pivot, QWidget))
        self.assertTrue(issubclass(fluentqt.TextEdit, QWidget))
        self.assertTrue(issubclass(fluentqt.InfoBadge, QWidget))
        self.assertTrue(issubclass(fluentqt.InfoBar, QWidget))
        self.assertTrue(issubclass(fluentqt.ProgressBar, QWidget))
        self.assertTrue(issubclass(fluentqt.ProgressRing, QWidget))
        self.assertTrue(issubclass(fluentqt.Shimmer, QWidget))
        self.assertTrue(issubclass(fluentqt.Card, QFrame))
        self.assertTrue(issubclass(fluentqt.Divider, QWidget))
        self.assertTrue(
            issubclass(fluentqt.Expander, fluentqt.Card)
        )
        self.assertTrue(issubclass(fluentqt.FontIcon, QWidget))
        self.assertTrue(
            issubclass(fluentqt.AnnotatedScrollBar, QWidget)
        )
        self.assertTrue(issubclass(fluentqt.PipsPager, QWidget))
        self.assertTrue(issubclass(fluentqt.ScrollBar, QScrollBar))
        self.assertTrue(issubclass(fluentqt.ScrollView, QScrollArea))
        self.assertTrue(issubclass(fluentqt.SelectorBar, QWidget))
        self.assertTrue(issubclass(fluentqt.FlipView, QWidget))
        self.assertTrue(issubclass(fluentqt.FlowView, QAbstractItemView))
        self.assertTrue(issubclass(fluentqt.GridView, QListView))
        self.assertTrue(issubclass(fluentqt.ListView, QListView))
        self.assertTrue(issubclass(fluentqt.NavigationView, QWidget))
        self.assertTrue(issubclass(fluentqt.SplitView, QWidget))
        self.assertTrue(issubclass(fluentqt.StackContentHost, QWidget))
        self.assertTrue(issubclass(fluentqt.StackView, QStackedWidget))
        self.assertTrue(issubclass(fluentqt.TreeView, QTreeView))
        self.assertTrue(issubclass(fluentqt.TabView, QWidget))
        self.assertTrue(issubclass(fluentqt.Window, QWidget))
        self.assertIs(date_time.CalendarView, fluentqt.CalendarView)
        self.assertIs(basicinput.ColorPicker, fluentqt.ColorPicker)
        self.assertIs(basicinput.CompoundButton, fluentqt.CompoundButton)
        self.assertIs(basicinput.HyperlinkButton, fluentqt.HyperlinkButton)
        self.assertIs(basicinput.RatingControl, fluentqt.RatingControl)
        self.assertIs(basicinput.ToggleSwitch, fluentqt.ToggleSwitch)
        self.assertIs(layout.Card, fluentqt.Card)
        self.assertIs(layout.Divider, fluentqt.Divider)
        self.assertIs(layout.Expander, fluentqt.Expander)
        self.assertIs(layout.Accordion, fluentqt.Accordion)
        self.assertIs(
            scrolling.AnnotatedScrollBar,
            fluentqt.AnnotatedScrollBar,
        )
        self.assertIs(
            scrolling.AnnotatedScrollBarLabel,
            fluentqt.AnnotatedScrollBarLabel,
        )
        self.assertIs(scrolling.PipsPager, fluentqt.PipsPager)
        self.assertIs(scrolling.ScrollBar, fluentqt.ScrollBar)
        self.assertIs(scrolling.ScrollView, fluentqt.ScrollView)
        self.assertIs(collections.FlowView, fluentqt.FlowView)
        self.assertIs(collections.GridView, fluentqt.GridView)
        self.assertIs(collections.ListView, fluentqt.ListView)
        self.assertIs(collections.SelectionMode, fluentqt.SelectionMode)
        self.assertIs(collections.SplitView, fluentqt.SplitView)
        self.assertIs(
            collections.SplitViewPaneOptions,
            fluentqt.SplitViewPaneOptions,
        )
        self.assertIs(collections.StackView, fluentqt.StackView)
        self.assertIs(collections.TreeView, fluentqt.TreeView)
        self.assertIs(navigation.Breadcrumb, fluentqt.Breadcrumb)
        self.assertIs(navigation.BreadcrumbItem, fluentqt.BreadcrumbItem)
        self.assertIs(navigation.NavigationView, fluentqt.NavigationView)
        self.assertIs(navigation.Pivot, fluentqt.Pivot)
        self.assertIs(navigation.PivotItem, fluentqt.PivotItem)
        self.assertIs(navigation.SelectorBar, fluentqt.SelectorBar)
        self.assertIs(
            navigation.SelectorBarItem,
            fluentqt.SelectorBarItem,
        )
        self.assertIs(
            navigation.StackContentHost,
            fluentqt.StackContentHost,
        )
        self.assertIs(navigation.TabView, fluentqt.TabView)
        self.assertIs(navigation.TabViewItem, fluentqt.TabViewItem)
        self.assertIs(
            collections.WidgetOwnership,
            fluentqt.WidgetOwnership,
        )
        self.assertIs(
            scrolling.WidgetOwnership,
            fluentqt.WidgetOwnership,
        )
        self.assertIs(status_info.Avatar, fluentqt.Avatar)
        self.assertIs(status_info.InfoBadge, fluentqt.InfoBadge)
        self.assertIs(status_info.InfoBar, fluentqt.InfoBar)
        self.assertIs(status_info.ProgressRing, fluentqt.ProgressRing)
        self.assertIs(status_info.Shimmer, fluentqt.Shimmer)
        self.assertIs(textfields.NumberBox, fluentqt.NumberBox)
        self.assertIs(textfields.TextEdit, fluentqt.TextEdit)
        self.assertIs(windowing.Window, fluentqt.Window)
        self.assertIs(foundation.FontIcon, fluentqt.FontIcon)
        self.assertIs(foundation.Theme, fluentqt.Theme)
        self.assertIs(native.fluent.Avatar, fluentqt.Avatar)
        self.assertIs(native.fluent.Button, fluentqt.Button)
        self.assertIs(native.fluent.CalendarView, fluentqt.CalendarView)
        self.assertIs(native.fluent.ColorPicker, fluentqt.ColorPicker)
        self.assertIs(native.fluent.CompoundButton, fluentqt.CompoundButton)
        self.assertIs(native.fluent.FontIcon, fluentqt.FontIcon)
        self.assertIs(
            native.fluent.RatingControl,
            fluentqt.RatingControl,
        )
        self.assertIs(native.fluent.PipsPager, fluentqt.PipsPager)
        self.assertIs(
            native.fluent.AnnotatedScrollBar,
            fluentqt.AnnotatedScrollBar,
        )
        self.assertIs(
            native.fluent.AnnotatedScrollBarLabel,
            fluentqt.AnnotatedScrollBarLabel,
        )
        self.assertIs(native.fluent.ScrollBar, fluentqt.ScrollBar)
        self.assertTrue(
            issubclass(fluentqt.FlipView, native.fluent.FlipView)
        )
        self.assertIsNot(native.fluent.FlipView, fluentqt.FlipView)
        self.assertNotIn("addPage", native.fluent.FlipView.__dict__)
        self.assertNotIn("insertPage", native.fluent.FlipView.__dict__)
        self.assertNotIn("removePage", native.fluent.FlipView.__dict__)
        self.assertTrue(
            hasattr(native.fluent.FlipView, "_addPageWithOwnership")
        )
        self.assertTrue(
            hasattr(native.fluent.FlipView, "_insertPageWithOwnership")
        )
        self.assertTrue(
            hasattr(
                native.fluent.FlipView,
                "_releasePageWithOwnership",
            )
        )
        self.assertTrue(
            issubclass(fluentqt.SplitView, native.fluent.SplitView)
        )
        self.assertIsNot(native.fluent.SplitView, fluentqt.SplitView)
        self.assertIs(
            native.fluent.SplitViewPaneOptions,
            fluentqt.SplitViewPaneOptions,
        )
        for method_name in (
            "addPane",
            "insertPane",
            "removePane",
            "removePaneAt",
            "releasePane",
            "releasePaneAt",
        ):
            self.assertNotIn(
                method_name,
                native.fluent.SplitView.__dict__,
            )
        self.assertTrue(
            hasattr(native.fluent.SplitView, "_addPaneWithOwnership")
        )
        self.assertTrue(
            hasattr(native.fluent.SplitView, "_insertPaneWithOwnership")
        )
        self.assertTrue(
            hasattr(
                native.fluent.SplitView,
                "_releasePaneAtWithOwnership",
            )
        )
        self.assertTrue(
            issubclass(fluentqt.FlowView, native.fluent.FlowView)
        )
        self.assertTrue(
            issubclass(fluentqt.GridView, native.fluent.GridView)
        )
        self.assertTrue(
            issubclass(fluentqt.ListView, native.fluent.ListView)
        )
        self.assertTrue(
            issubclass(fluentqt.TreeView, native.fluent.TreeView)
        )
        self.assertIs(native.fluent.TextEdit, fluentqt.TextEdit)
        self.assertTrue(
            issubclass(fluentqt.Breadcrumb, native.fluent.Breadcrumb)
        )
        self.assertIsNot(native.fluent.Breadcrumb, fluentqt.Breadcrumb)
        self.assertNotIn("setItems", native.fluent.Breadcrumb.__dict__)
        self.assertIn("setItems", fluentqt.Breadcrumb.__dict__)
        self.assertIs(
            native.fluent.BreadcrumbItem,
            fluentqt.BreadcrumbItem,
        )
        self.assertIs(native.fluent.Pivot, fluentqt.Pivot)
        self.assertIs(native.fluent.PivotItem, fluentqt.PivotItem)
        self.assertIs(native.fluent.SelectorBar, fluentqt.SelectorBar)
        self.assertIs(
            native.fluent.SelectorBarItem,
            fluentqt.SelectorBarItem,
        )
        self.assertIs(native.fluent.TabView, fluentqt.TabView)
        self.assertIs(native.fluent.TabViewItem, fluentqt.TabViewItem)
        self.assertTrue(
            issubclass(
                fluentqt.NavigationView,
                native.fluent.NavigationView,
            )
        )
        self.assertIsNot(
            native.fluent.NavigationView,
            fluentqt.NavigationView,
        )
        self.assertIs(
            native.fluent.StackContentHost,
            fluentqt.StackContentHost,
        )
        for adapter_name in (
            "_insertPageWithOwnership",
            "_replacePageWithOwnership",
            "_releasePageWithOwnership",
            "_releaseAllPagesWithOwnership",
        ):
            self.assertTrue(
                hasattr(native.fluent.StackContentHost, adapter_name)
            )
        for adapter_name in (
            "_setHeaderChromeWidgetWithOwnership",
            "_setMainChromeWidgetWithOwnership",
            "_setFooterChromeWidgetWithOwnership",
            "_releaseHeaderChromeWidgetWithOwnership",
            "_releaseMainChromeWidgetWithOwnership",
            "_releaseFooterChromeWidgetWithOwnership",
        ):
            self.assertTrue(
                hasattr(native.fluent.NavigationView, adapter_name)
            )
        self.assertNotIn("TabStrip", dir(native.fluent))
        self.assertTrue(
            issubclass(fluentqt.InfoBar, native.fluent.InfoBar)
        )
        self.assertIsNot(native.fluent.InfoBar, fluentqt.InfoBar)
        self.assertNotIn("setActionWidget", native.fluent.InfoBar.__dict__)
        self.assertTrue(
            hasattr(native.fluent.InfoBar, "_setActionWidget")
        )
        self.assertTrue(
            issubclass(fluentqt.ScrollView, native.fluent.ScrollView)
        )
        self.assertIsNot(native.fluent.ScrollView, fluentqt.ScrollView)
        self.assertNotIn("setWidget", native.fluent.ScrollView.__dict__)
        self.assertNotIn("takeWidget", native.fluent.ScrollView.__dict__)
        self.assertTrue(
            hasattr(
                native.fluent.ScrollView,
                "_setContentWidgetWithOwnership",
            )
        )
        self.assertTrue(
            hasattr(fluentqt.ScrollView, "setOwnedContentWidget")
        )
        self.assertTrue(
            hasattr(fluentqt.ScrollView, "setBorrowedContentWidget")
        )
        self.assertTrue(
            hasattr(fluentqt.ScrollView, "setReparentedContentWidget")
        )
        self.assertTrue(
            issubclass(fluentqt.Expander, native.fluent.Expander)
        )
        self.assertIsNot(native.fluent.Expander, fluentqt.Expander)
        self.assertTrue(
            hasattr(
                native.fluent.Expander,
                "_setContentWidgetWithOwnership",
            )
        )
        self.assertFalse(hasattr(native.fluent.Expander, "headerButton"))
        self.assertTrue(
            issubclass(fluentqt.Accordion, native.fluent.Accordion)
        )
        self.assertIsNot(native.fluent.Accordion, fluentqt.Accordion)
        self.assertNotIn("addItem", native.fluent.Accordion.__dict__)
        self.assertNotIn("insertItem", native.fluent.Accordion.__dict__)
        self.assertTrue(
            hasattr(native.fluent.Accordion, "_addItemWithOwnership")
        )
        self.assertTrue(
            hasattr(native.fluent.Accordion, "_insertItemWithOwnership")
        )
        self.assertTrue(hasattr(fluentqt.Accordion, "addOwnedItem"))
        self.assertTrue(hasattr(fluentqt.Accordion, "addBorrowedItem"))
        self.assertTrue(hasattr(fluentqt.Accordion, "addReparentedItem"))
        self.assertTrue(
            issubclass(fluentqt.StackView, native.fluent.StackView)
        )
        self.assertIsNot(native.fluent.StackView, fluentqt.StackView)
        self.assertNotIn("push", native.fluent.StackView.__dict__)
        self.assertNotIn("replace", native.fluent.StackView.__dict__)
        self.assertNotIn("setInitialItem", native.fluent.StackView.__dict__)
        self.assertNotIn(
            "setCurrentWidget",
            native.fluent.StackView.__dict__,
        )
        self.assertNotIn(
            "defaultItemOwnership",
            native.fluent.StackView.__dict__,
        )
        self.assertTrue(
            hasattr(native.fluent.StackView, "_pushItemWithOwnership")
        )
        self.assertTrue(
            hasattr(native.fluent.StackView, "_pushItemsWithOwnership")
        )
        self.assertTrue(
            hasattr(
                native.fluent.StackView,
                "_replaceCurrentWithOwnership",
            )
        )
        self.assertTrue(
            hasattr(
                native.fluent.StackView,
                "_replaceAtWithOwnership",
            )
        )
        self.assertTrue(
            hasattr(
                native.fluent.StackView,
                "_setInitialItemWithOwnership",
            )
        )
        self.assertIs(native.fluent.windowing.Window, fluentqt.Window)

        info = fluentqt.binding_build_info()
        self.assertEqual(
            info["fluentqt_version"],
            os.environ["FLUENTQT_EXPECTED_VERSION"],
        )
        self.assertEqual(info["pyside6_version"], PySide6.__version__)
        self.assertEqual(info["shiboken6_version"], shiboken6.__version__)
        self.assertEqual(
            info["shiboken6_generator_version"],
            shiboken6_generator.__version__,
        )
        self.assertEqual(info["qt_compile_version"], qVersion())
        self.assertEqual(info["qt_runtime_version"], qVersion())

    def test_cpp_mixins_are_not_published_as_python_api(self):
        self.assertNotIn("FluentElement", dir(native.fluent))
        self.assertNotIn("QMLPlus", dir(native.fluent))
        self.assertFalse(hasattr(fluentqt.Button, "anchors"))
        self.assertFalse(hasattr(fluentqt.Button, "bind"))
        self.assertFalse(hasattr(fluentqt.Button, "setState"))
        self.assertFalse(hasattr(fluentqt.CompoundButton, "anchors"))
        self.assertFalse(hasattr(fluentqt.CompoundButton, "bind"))
        self.assertFalse(hasattr(fluentqt.CompoundButton, "setState"))
        self.assertFalse(hasattr(fluentqt.CalendarView, "anchors"))
        self.assertFalse(hasattr(fluentqt.CalendarView, "bind"))
        self.assertFalse(hasattr(fluentqt.CalendarView, "setState"))
        self.assertFalse(hasattr(fluentqt.ColorPicker, "anchors"))
        self.assertFalse(hasattr(fluentqt.ColorPicker, "bind"))
        self.assertFalse(hasattr(fluentqt.ColorPicker, "setState"))
        self.assertFalse(hasattr(fluentqt.Divider, "anchors"))
        self.assertFalse(hasattr(fluentqt.Divider, "bind"))
        self.assertFalse(hasattr(fluentqt.Divider, "setState"))
        self.assertFalse(hasattr(fluentqt.Card, "anchors"))
        self.assertFalse(hasattr(fluentqt.Card, "bind"))
        self.assertFalse(hasattr(fluentqt.Card, "setState"))
        self.assertFalse(hasattr(fluentqt.Expander, "anchors"))
        self.assertFalse(hasattr(fluentqt.Expander, "bind"))
        self.assertFalse(hasattr(fluentqt.Expander, "setState"))
        self.assertFalse(hasattr(fluentqt.Accordion, "anchors"))
        self.assertFalse(hasattr(fluentqt.Accordion, "bind"))
        self.assertFalse(hasattr(fluentqt.Accordion, "setState"))
        self.assertFalse(hasattr(fluentqt.StackView, "anchors"))
        self.assertFalse(hasattr(fluentqt.StackView, "bind"))
        self.assertFalse(hasattr(fluentqt.StackView, "setState"))
        self.assertFalse(hasattr(fluentqt.FlowView, "anchors"))
        self.assertFalse(hasattr(fluentqt.FlowView, "bind"))
        # QAbstractItemView has its own protected setState() virtual helper;
        # its presence is unrelated to the intentionally hidden QMLPlus mixin.
        self.assertFalse(hasattr(fluentqt.SplitView, "anchors"))
        self.assertFalse(hasattr(fluentqt.SplitView, "bind"))
        self.assertFalse(hasattr(fluentqt.SplitView, "setState"))
        self.assertFalse(hasattr(fluentqt.Breadcrumb, "anchors"))
        self.assertFalse(hasattr(fluentqt.Breadcrumb, "bind"))
        self.assertFalse(hasattr(fluentqt.Breadcrumb, "setState"))
        self.assertFalse(hasattr(fluentqt.Pivot, "anchors"))
        self.assertFalse(hasattr(fluentqt.Pivot, "bind"))
        self.assertFalse(hasattr(fluentqt.Pivot, "setState"))
        self.assertFalse(hasattr(fluentqt.SelectorBar, "anchors"))
        self.assertFalse(hasattr(fluentqt.SelectorBar, "bind"))
        self.assertFalse(hasattr(fluentqt.SelectorBar, "setState"))
        self.assertFalse(hasattr(fluentqt.TabView, "anchors"))
        self.assertFalse(hasattr(fluentqt.TabView, "bind"))
        self.assertFalse(hasattr(fluentqt.TabView, "setState"))
        self.assertFalse(hasattr(fluentqt.NavigationView, "anchors"))
        self.assertFalse(hasattr(fluentqt.NavigationView, "bind"))
        self.assertFalse(hasattr(fluentqt.NavigationView, "setState"))
        self.assertFalse(hasattr(fluentqt.StackContentHost, "anchors"))
        self.assertFalse(hasattr(fluentqt.StackContentHost, "bind"))
        self.assertFalse(hasattr(fluentqt.StackContentHost, "setState"))
        self.assertFalse(hasattr(fluentqt.FontIcon, "anchors"))
        self.assertFalse(hasattr(fluentqt.FontIcon, "bind"))
        self.assertFalse(hasattr(fluentqt.FontIcon, "setState"))
        self.assertFalse(hasattr(fluentqt.Avatar, "anchors"))
        self.assertFalse(hasattr(fluentqt.Avatar, "bind"))
        self.assertFalse(hasattr(fluentqt.Avatar, "setState"))
        self.assertFalse(hasattr(fluentqt.RatingControl, "anchors"))
        self.assertFalse(hasattr(fluentqt.RatingControl, "bind"))
        self.assertFalse(hasattr(fluentqt.RatingControl, "setState"))
        self.assertFalse(hasattr(fluentqt.ScrollBar, "anchors"))
        self.assertFalse(hasattr(fluentqt.ScrollBar, "bind"))
        self.assertFalse(hasattr(fluentqt.ScrollBar, "setState"))
        self.assertFalse(hasattr(fluentqt.PipsPager, "anchors"))
        self.assertFalse(hasattr(fluentqt.PipsPager, "bind"))
        self.assertFalse(hasattr(fluentqt.PipsPager, "setState"))
        self.assertFalse(hasattr(fluentqt.PipsPager, "HitKind"))
        self.assertFalse(
            hasattr(fluentqt.PipsPager, "selectedVisualOffset")
        )
        self.assertFalse(
            hasattr(fluentqt.PipsPager, "visibleWindowOffset")
        )
        self.assertFalse(hasattr(fluentqt.Shimmer, "anchors"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "bind"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "setState"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "elements"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "setElements"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "clearElements"))
        self.assertFalse(hasattr(fluentqt.InfoBar, "anchors"))
        self.assertFalse(hasattr(fluentqt.InfoBar, "bind"))
        self.assertFalse(hasattr(fluentqt.InfoBar, "setState"))
        self.assertFalse(hasattr(fluentqt.TextEdit, "anchors"))
        self.assertFalse(hasattr(fluentqt.TextEdit, "bind"))
        self.assertFalse(hasattr(fluentqt.TextEdit, "setState"))
        self.assertFalse(hasattr(fluentqt.ScrollView, "anchors"))
        self.assertFalse(hasattr(fluentqt.ScrollView, "bind"))
        self.assertFalse(hasattr(fluentqt.ScrollView, "setState"))
        self.assertFalse(
            hasattr(fluentqt.AnnotatedScrollBar, "anchors")
        )
        self.assertFalse(
            hasattr(fluentqt.AnnotatedScrollBar, "bind")
        )
        self.assertFalse(
            hasattr(fluentqt.AnnotatedScrollBar, "setState")
        )

    def test_api_manifest(self):
        manifest_path = Path(__file__).resolve().parents[1] / "api-manifest.json"
        with manifest_path.open(encoding="utf-8") as manifest_file:
            manifest = json.load(manifest_file)

        for section in ("classes", "enums", "functions"):
            with self.subTest(section=section):
                missing = [
                    name
                    for name in manifest[section]
                    if not hasattr(fluentqt, name)
                ]
                self.assertEqual(missing, [])

        for class_name, method_names in manifest["methods"].items():
            with self.subTest(class_name=class_name):
                bound_class = getattr(fluentqt, class_name)
                missing = [
                    name
                    for name in method_names
                    if not hasattr(bound_class, name)
                ]
                self.assertEqual(missing, [])

    def test_properties_and_signal(self):
        body_font = fluentqt.font_for_role(fluentqt.FontRole.Body)
        self.assertEqual(body_font.pixelSize(), 14)
        self.assertTrue(body_font.family())

        button = fluentqt.Button("Save")
        button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
        self.assertEqual(button.text(), "Save")
        self.assertEqual(
            button.fluentStyle(), fluentqt.Button.ButtonStyle.Accent
        )

        clicks = []
        button.clicked.connect(lambda: clicks.append(True))
        button.click()
        self.assertEqual(clicks, [True])

        line_edit = fluentqt.LineEdit()
        line_edit.setFontRole(fluentqt.FontRole.BodyStrong)
        self.assertEqual(line_edit.fontRole(), fluentqt.FontRole.BodyStrong)

    def test_font_icon_properties_and_signals(self):
        empty_icon = fluentqt.FontIcon()
        self.assertEqual(empty_icon.glyph(), "")

        icon = fluentqt.FontIcon("ic_fluent_settings_20_regular")
        self.assertEqual(
            icon.glyph(),
            "ic_fluent_settings_20_regular",
        )
        self.assertGreater(icon.iconSize(), 0)
        self.assertFalse(icon.color().isValid())
        self.assertAlmostEqual(icon.rotation(), 0.0)
        self.assertEqual(icon.sizeHint().width(), icon.iconSize())
        self.assertEqual(icon.sizeHint().height(), icon.iconSize())
        self.assertTrue(
            icon.testAttribute(Qt.WA_TransparentForMouseEvents)
        )

        glyph_changes = []
        size_changes = []
        color_changes = []
        rotation_changes = []
        icon.glyphChanged.connect(glyph_changes.append)
        icon.iconSizeChanged.connect(size_changes.append)
        icon.colorChanged.connect(color_changes.append)
        icon.rotationChanged.connect(rotation_changes.append)

        icon.setGlyph("ic_fluent_save_20_regular")
        icon.setGlyph("ic_fluent_save_20_regular")
        icon.setIconSize(0)
        icon.setIconSize(1)
        accent = QColor("#7f52ff")
        icon.setColor(accent)
        icon.setColor(accent)
        icon.setRotation(90.0)
        icon.setRotation(90.0)

        self.assertEqual(
            glyph_changes,
            ["ic_fluent_save_20_regular"],
        )
        self.assertEqual(size_changes, [1])
        self.assertEqual(color_changes, [accent])
        self.assertEqual(rotation_changes, [90.0])
        self.assertEqual(icon.glyph(), "ic_fluent_save_20_regular")
        self.assertEqual(icon.iconSize(), 1)
        self.assertEqual(icon.color(), accent)
        self.assertAlmostEqual(icon.rotation(), 90.0)
        self.assertEqual(icon.minimumSizeHint(), icon.sizeHint())

    def test_color_picker_public_properties_and_signals(self):
        picker = fluentqt.ColorPicker()
        self.assertEqual(picker.color(), QColor(255, 255, 255, 255))
        self.assertTrue(picker.alphaEnabled())

        for internal_name in (
            "hue",
            "saturation",
            "value",
            "setHueFromBar",
            "setSVFromSpectrum",
            "setValueFromSlider",
            "setAlphaFromSlider",
        ):
            with self.subTest(internal_name=internal_name):
                self.assertFalse(
                    hasattr(fluentqt.ColorPicker, internal_name)
                )

        color_changes = []
        alpha_changes = []
        picker.colorChanged.connect(color_changes.append)
        picker.alphaEnabledChanged.connect(alpha_changes.append)
        selected = QColor(0, 120, 212, 180)
        picker.setColor(selected)
        picker.setColor(selected)
        picker.setAlphaEnabled(False)
        picker.setAlphaEnabled(False)

        self.assertEqual(picker.color(), selected)
        self.assertFalse(picker.alphaEnabled())
        self.assertEqual(color_changes, [selected])
        self.assertEqual(alpha_changes, [False])

    def test_calendar_view_date_properties_signals_and_queries(self):
        calendar = fluentqt.CalendarView()
        self.assertFalse(calendar.selectedDate().isValid())
        self.assertTrue(calendar.visibleMonth().isValid())
        self.assertFalse(calendar.minDate().isValid())
        self.assertFalse(calendar.maxDate().isValid())
        self.assertTrue(calendar.isFrameVisible())
        self.assertEqual(
            calendar.contentLevel(),
            fluentqt.CalendarView.CalendarContentLevel.Day,
        )

        minimum = QDate(2026, 5, 10)
        maximum = QDate(2026, 5, 20)
        min_changes = []
        max_changes = []
        selected_changes = []
        frame_changes = []
        level_changes = []
        locale_changes = []
        first_day_changes = []
        calendar.minDateChanged.connect(min_changes.append)
        calendar.maxDateChanged.connect(max_changes.append)
        calendar.selectedDateChanged.connect(selected_changes.append)
        calendar.frameVisibleChanged.connect(frame_changes.append)
        calendar.contentLevelChanged.connect(level_changes.append)
        calendar.localeChanged.connect(locale_changes.append)

        calendar.setDateRange(minimum, maximum)
        calendar.setDateRange(minimum, maximum)
        calendar.setSelectedDate(QDate(2026, 5, 1))
        calendar.setSelectedDate(QDate(2026, 5, 1))
        calendar.setFrameVisible(False)
        calendar.setFrameVisible(False)
        month_level = fluentqt.CalendarView.CalendarContentLevel.Month
        calendar.setContentLevel(month_level)
        calendar.setContentLevel(month_level)
        us_locale = QLocale(QLocale.English, QLocale.UnitedStates)
        german_locale = QLocale(QLocale.German, QLocale.Germany)
        next_locale = (
            german_locale if calendar.locale() == us_locale else us_locale
        )
        calendar.setLocale(next_locale)
        calendar.setLocale(next_locale)
        calendar.firstDayOfWeekChanged.connect(first_day_changes.append)
        next_first_day = (
            Qt.Thursday
            if calendar.firstDayOfWeek() != Qt.Thursday
            else Qt.Sunday
        )
        calendar.setFirstDayOfWeek(next_first_day)
        calendar.setFirstDayOfWeek(next_first_day)

        self.assertEqual(calendar.minDate(), minimum)
        self.assertEqual(calendar.maxDate(), maximum)
        self.assertEqual(calendar.selectedDate(), minimum)
        self.assertEqual(min_changes, [minimum])
        self.assertEqual(max_changes, [maximum])
        self.assertEqual(selected_changes, [minimum])
        self.assertEqual(frame_changes, [False])
        self.assertEqual(level_changes, [month_level])
        self.assertEqual(locale_changes, [next_locale])
        self.assertEqual(first_day_changes, [next_first_day])

        calendar.setContentLevel(
            fluentqt.CalendarView.CalendarContentLevel.Day
        )
        calendar.resize(calendar.sizeHint())
        QCoreApplication.processEvents()
        selected_rect = calendar.dateCellRect(minimum)
        self.assertTrue(selected_rect.isValid())
        self.assertEqual(calendar.dateAt(selected_rect.center()), minimum)
        self.assertTrue(calendar.isDateSelectable(minimum))
        self.assertFalse(calendar.isDateSelectable(minimum.addDays(-1)))

    def test_phase_one_component_properties_and_signals(self):
        radio = fluentqt.RadioButton("Option")
        radio.setChecked(True)
        self.assertEqual(radio.text(), "Option")
        self.assertTrue(radio.isChecked())

        slider = fluentqt.Slider(Qt.Horizontal)
        slider.setRange(0, 100)
        slider.setValue(40)
        self.assertEqual(slider.value(), 40)

        toggle_button = fluentqt.ToggleButton("Tri-state")
        toggle_button.setThreeState(True)
        toggle_button.setCheckState(Qt.PartiallyChecked)
        self.assertTrue(toggle_button.isThreeState())
        self.assertEqual(toggle_button.checkState(), Qt.PartiallyChecked)

        toggle_values = []
        toggle_switch = fluentqt.ToggleSwitch()
        toggle_switch.toggled.connect(toggle_values.append)
        toggle_switch.setOnContent("On")
        toggle_switch.setOffContent("Off")
        toggle_switch.setIsOn(True)
        self.assertTrue(toggle_switch.isOn())
        self.assertEqual(toggle_values, [True])

        label = fluentqt.Label("Status")
        label.setFluentTypography(fluentqt.FontRole.BodyStrong)
        self.assertEqual(label.text(), "Status")
        self.assertEqual(
            label.fluentTypography(),
            fluentqt.FontRole.BodyStrong,
        )

        password = fluentqt.PasswordBox()
        password.setHeader("Password")
        password.setPassword("secret")
        password.setPasswordRevealMode(
            fluentqt.PasswordBox.PasswordRevealMode.Hidden
        )
        self.assertEqual(password.header(), "Password")
        self.assertEqual(password.password(), "secret")

        number = fluentqt.NumberBox()
        number.setRange(0.0, 10.0)
        number.setValue(4.5)
        number.setSmallChange(0.5)
        self.assertEqual(number.value(), 4.5)
        self.assertEqual(number.smallChange(), 0.5)

    def test_status_info_properties_and_signals(self):
        badge = fluentqt.InfoBadge()
        badge_values = []
        badge.valueChanged.connect(badge_values.append)
        badge.setValue(12)
        badge.setDisplayMode(
            fluentqt.InfoBadge.InfoBadgeDisplayMode.Value
        )
        badge.setStatus(
            fluentqt.InfoBadge.InfoBadgeStatus.Success
        )
        badge.setBadgeOpacity(0.75)
        self.assertEqual(badge_values, [12])
        self.assertEqual(badge.value(), 12)
        self.assertEqual(
            badge.effectiveDisplayMode(),
            fluentqt.InfoBadge.InfoBadgeDisplayMode.Value,
        )
        self.assertEqual(
            badge.status(),
            fluentqt.InfoBadge.InfoBadgeStatus.Success,
        )
        self.assertAlmostEqual(badge.badgeOpacity(), 0.75)

        bar = fluentqt.ProgressBar()
        bar.setRange(10.0, 110.0)
        bar_values = []
        bar.valueChanged.connect(bar_values.append)
        bar.setValue(60.0)
        bar.setShowPaused(True)
        bar.setRailVisible(False)
        self.assertEqual(bar_values, [60.0])
        self.assertAlmostEqual(bar.progressRatio(), 0.5)
        self.assertEqual(bar.progressText(), "60")
        self.assertTrue(bar.showPaused())
        self.assertFalse(bar.railVisible())

        ring = fluentqt.ProgressRing()
        ring.setIsIndeterminate(False)
        ring.setRange(0, 200)
        ring_values = []
        ring.valueChanged.connect(ring_values.append)
        ring.setValue(50)
        ring.setRingSize(
            fluentqt.ProgressRing.ProgressRingSize.Large
        )
        ring.setStatus(
            fluentqt.ProgressRing.ProgressRingStatus.Paused
        )
        ring.setBackgroundVisible(True)
        self.assertEqual(ring_values, [50])
        self.assertAlmostEqual(ring.progressRatio(), 0.25)
        self.assertEqual(
            ring.ringSize(),
            fluentqt.ProgressRing.ProgressRingSize.Large,
        )
        self.assertEqual(
            ring.status(),
            fluentqt.ProgressRing.ProgressRingStatus.Paused,
        )
        self.assertTrue(ring.backgroundVisible())

        shimmer = fluentqt.Shimmer()
        active_values = []
        progress_values = []
        shimmer.activeChanged.connect(active_values.append)
        shimmer.shimmerProgressChanged.connect(progress_values.append)
        shimmer.setActive(False)
        shimmer.setAnimationEnabled(False)
        shimmer.setShimmerProgress(1.25)
        shimmer.setCycleDuration(100)
        shimmer.setShimmerTemplate(
            fluentqt.Shimmer.ShimmerTemplate.ImageCard
        )
        self.assertEqual(active_values, [False])
        self.assertEqual(progress_values, [0.25])
        self.assertFalse(shimmer.isActive())
        self.assertFalse(shimmer.isAnimationEnabled())
        self.assertAlmostEqual(shimmer.shimmerProgress(), 0.25)
        self.assertEqual(shimmer.cycleDuration(), 250)
        self.assertEqual(
            shimmer.shimmerTemplate(),
            fluentqt.Shimmer.ShimmerTemplate.ImageCard,
        )

    def test_m2_leaf_component_properties_and_signals(self):
        compound = fluentqt.CompoundButton(
            "Install update",
            "Download and restart the app",
        )
        secondary_changes = []
        clicks = []
        compound.secondaryTextChanged.connect(secondary_changes.append)
        compound.clicked.connect(lambda: clicks.append(True))
        self.assertEqual(compound.text(), "Install update")
        self.assertEqual(
            compound.secondaryText(),
            "Download and restart the app",
        )
        self.assertEqual(
            compound.accessibleDescription(),
            "Download and restart the app",
        )
        self.assertEqual(
            compound.fluentSize(),
            fluentqt.Button.ButtonSize.Large,
        )
        two_line_size = compound.sizeHint()
        compound.setSecondaryText("Restart after downloading")
        compound.setSecondaryText("Restart after downloading")
        compound.click()
        self.assertEqual(
            secondary_changes,
            ["Restart after downloading"],
        )
        self.assertEqual(clicks, [True])
        self.assertEqual(
            compound.accessibleDescription(),
            "Restart after downloading",
        )
        compound.setSecondaryText("")
        self.assertLessEqual(
            compound.sizeHint().height(),
            two_line_size.height(),
        )

        repeat = fluentqt.RepeatButton("Hold")
        delay_changes = []
        interval_changes = []
        repeat.delayChanged.connect(lambda: delay_changes.append(repeat.delay()))
        repeat.intervalChanged.connect(
            lambda: interval_changes.append(repeat.interval())
        )
        repeat.setDelay(250)
        repeat.setInterval(40)
        self.assertTrue(repeat.autoRepeat())
        self.assertEqual(repeat.delay(), 250)
        self.assertEqual(repeat.interval(), 40)
        self.assertEqual(delay_changes, [250])
        self.assertEqual(interval_changes, [40])

        link = fluentqt.HyperlinkButton("FluentQt")
        url_changes = []
        underline_changes = []
        link.urlChanged.connect(lambda: url_changes.append(link.url()))
        link.showUnderlineChanged.connect(
            lambda: underline_changes.append(link.showUnderline())
        )
        target = QUrl("https://github.com/calvinhxx/Fluent-Qt")
        link.setUrl(target)
        link.setShowUnderline(True)
        self.assertEqual(link.url(), target)
        self.assertTrue(link.showUnderline())
        self.assertEqual(url_changes, [target])
        self.assertEqual(underline_changes, [True])

        divider = fluentqt.Divider(Qt.Vertical)
        orientation_changes = []
        inset_changes = []
        thickness_changes = []
        color_changes = []
        divider.orientationChanged.connect(orientation_changes.append)
        divider.leadingInsetChanged.connect(inset_changes.append)
        divider.thicknessChanged.connect(thickness_changes.append)
        divider.colorChanged.connect(color_changes.append)
        divider.setOrientation(Qt.Horizontal)
        divider.setLeadingInset(12)
        divider.setTrailingInset(20)
        divider.setThickness(2.0)
        color = QColor(12, 34, 56, 78)
        divider.setColor(color)
        self.assertEqual(divider.orientation(), Qt.Horizontal)
        self.assertEqual(divider.leadingInset(), 12)
        self.assertEqual(divider.trailingInset(), 20)
        self.assertEqual(divider.thickness(), 2.0)
        self.assertEqual(divider.color(), color)
        self.assertEqual(orientation_changes, [Qt.Horizontal])
        self.assertEqual(inset_changes, [12])
        self.assertEqual(thickness_changes, [2.0])
        self.assertEqual(color_changes, [color])

    def test_text_edit_properties_signals_and_multiline_contract(self):
        edit = fluentqt.TextEdit()
        text_changes = []
        margin_changes = []
        font_changes = []
        layout_changes = []
        chaining_changes = []
        edit.textChanged.connect(lambda: text_changes.append(edit.toPlainText()))
        edit.contentMarginsChanged.connect(
            lambda: margin_changes.append(edit.contentMargins())
        )
        edit.fontRoleChanged.connect(
            lambda: font_changes.append(edit.fontRole())
        )
        edit.layoutMetricsChanged.connect(
            lambda: layout_changes.append(
                (
                    edit.lineHeight(),
                    edit.minVisibleLines(),
                    edit.maxVisibleLines(),
                )
            )
        )
        edit.scrollChainingEnabledChanged.connect(
            lambda: chaining_changes.append(edit.isScrollChainingEnabled())
        )

        margins = QMargins(14, 6, 14, 6)
        edit.setContentMargins(margins)
        edit.setFontRole(fluentqt.FontRole.Subtitle)
        edit.setLineHeight(28)
        edit.setMinVisibleLines(2)
        edit.setMaxVisibleLines(3)
        edit.setScrollChainingEnabled(True)
        edit.setPlaceholderText("Write a note")
        edit.setPlainText("First line\nSecond line")
        edit.setReadOnly(True)

        self.assertEqual(edit.contentMargins(), margins)
        self.assertEqual(edit.fontRole(), fluentqt.FontRole.Subtitle)
        self.assertEqual(edit.lineHeight(), 28)
        self.assertEqual(edit.minVisibleLines(), 2)
        self.assertEqual(edit.maxVisibleLines(), 3)
        self.assertTrue(edit.isScrollChainingEnabled())
        self.assertEqual(edit.placeholderText(), "Write a note")
        self.assertEqual(edit.toPlainText(), "First line\nSecond line")
        self.assertTrue(edit.isReadOnly())
        self.assertIsInstance(edit.verticalScrollBar(), fluentqt.ScrollBar)
        self.assertIs(edit.verticalScrollBar().parent(), edit)
        self.assertEqual(margin_changes, [margins])
        self.assertEqual(font_changes, [fluentqt.FontRole.Subtitle])
        self.assertEqual(len(layout_changes), 3)
        self.assertEqual(chaining_changes, [True])
        self.assertTrue(text_changes)
        self.assertEqual(text_changes[-1], "First line\nSecond line")

        notification_count = len(text_changes)
        edit.clear()
        self.assertEqual(edit.toPlainText(), "")
        self.assertGreater(len(text_changes), notification_count)
        self.assertEqual(text_changes[-1], "")

    def test_card_and_expander_properties_and_signals(self):
        card = fluentqt.Card()
        appearance_changes = []
        border_changes = []
        card.appearanceChanged.connect(appearance_changes.append)
        card.borderVisibleChanged.connect(border_changes.append)

        self.assertEqual(
            card.appearance(),
            fluentqt.Card.Appearance.Layer,
        )
        self.assertTrue(card.isBorderVisible())
        card.setAppearance(fluentqt.Card.Appearance.Canvas)
        card.setBorderVisible(False)
        self.assertEqual(
            appearance_changes,
            [fluentqt.Card.Appearance.Canvas],
        )
        self.assertEqual(border_changes, [False])

        expander = fluentqt.Expander()
        header_changes = []
        expanded_changes = []
        animation_changes = []
        expander.headerTextChanged.connect(header_changes.append)
        expander.expandedChanged.connect(expanded_changes.append)
        expander.animationEnabledChanged.connect(animation_changes.append)

        self.assertEqual(expander.headerText(), "")
        self.assertFalse(expander.isExpanded())
        self.assertTrue(expander.isAnimationEnabled())
        self.assertEqual(
            expander.contentOwnership(),
            fluentqt.WidgetOwnership.Borrowed,
        )

        expander.setHeaderText("Details")
        expander.setAnimationEnabled(False)
        expander.setExpanded(True)
        self.assertEqual(header_changes, ["Details"])
        self.assertEqual(animation_changes, [False])
        self.assertEqual(expanded_changes, [True])
        self.assertTrue(expander.isExpanded())

    def test_avatar_properties_signals_and_composed_badge(self):
        avatar = fluentqt.Avatar()
        name_changes = []
        initials_changes = []
        shape_changes = []
        size_changes = []
        presence_changes = []
        background_changes = []
        foreground_changes = []
        avatar.nameChanged.connect(name_changes.append)
        avatar.initialsChanged.connect(initials_changes.append)
        avatar.shapeChanged.connect(shape_changes.append)
        avatar.avatarSizeChanged.connect(size_changes.append)
        avatar.presenceChanged.connect(presence_changes.append)
        avatar.backgroundColorChanged.connect(background_changes.append)
        avatar.foregroundColorChanged.connect(foreground_changes.append)

        avatar.setName("Ada Lovelace")
        avatar.setInitials(" AL ")
        avatar.setShape(fluentqt.Avatar.AvatarShape.Square)
        avatar.setAvatarSize(fluentqt.Avatar.AvatarSize.ExtraLarge)
        avatar.setPresence(fluentqt.Avatar.PresenceStatus.Available)
        background = QColor("#123456")
        foreground = QColor("#f0f1f2")
        avatar.setBackgroundColor(background)
        avatar.setForegroundColor(foreground)

        self.assertEqual(name_changes, ["Ada Lovelace"])
        self.assertEqual(initials_changes, ["AL"])
        self.assertEqual(
            shape_changes,
            [fluentqt.Avatar.AvatarShape.Square],
        )
        self.assertEqual(
            size_changes,
            [fluentqt.Avatar.AvatarSize.ExtraLarge],
        )
        self.assertEqual(
            presence_changes,
            [fluentqt.Avatar.PresenceStatus.Available],
        )
        self.assertEqual(background_changes, [background])
        self.assertEqual(foreground_changes, [foreground])
        self.assertEqual(avatar.effectiveInitials(), "AL")
        self.assertEqual(avatar.size(), avatar.sizeHint())
        self.assertEqual(avatar.size().width(), 56)

        badge = avatar.presenceBadge()
        self.assertIsInstance(badge, fluentqt.InfoBadge)
        self.assertIs(badge.parent(), avatar)
        self.assertTrue(Shiboken.isValid(badge))

    def test_rating_control_properties_and_signals(self):
        rating = fluentqt.RatingControl()
        value_changes = []
        placeholder_changes = []
        caption_changes = []
        clear_changes = []
        read_only_changes = []
        max_changes = []
        size_changes = []
        font_changes = []
        caption_font_changes = []
        rating.valueChanged.connect(value_changes.append)
        rating.placeholderValueChanged.connect(placeholder_changes.append)
        rating.captionChanged.connect(caption_changes.append)
        rating.isClearEnabledChanged.connect(clear_changes.append)
        rating.isReadOnlyChanged.connect(read_only_changes.append)
        rating.maxRatingChanged.connect(max_changes.append)
        rating.starSizeChanged.connect(size_changes.append)
        rating.fontRoleChanged.connect(
            lambda: font_changes.append(rating.fontRole())
        )
        rating.captionFontRoleChanged.connect(
            lambda: caption_font_changes.append(
                rating.captionFontRole()
            )
        )

        rating.setValue(3.5)
        rating.setPlaceholderValue(2.5)
        rating.setCaption("312 ratings")
        rating.setIsClearEnabled(False)
        rating.setIsReadOnly(True)
        rating.setMaxRating(7)
        rating.setStarSize(24)
        rating.setFontRole(fluentqt.FontRole.BodyStrong)
        rating.setCaptionFontRole(fluentqt.FontRole.BodyLarge)

        self.assertEqual(value_changes, [3.5])
        self.assertEqual(placeholder_changes, [2.5])
        self.assertEqual(caption_changes, ["312 ratings"])
        self.assertEqual(clear_changes, [False])
        self.assertEqual(read_only_changes, [True])
        self.assertEqual(max_changes, [7])
        self.assertEqual(size_changes, [24])
        self.assertEqual(font_changes, [fluentqt.FontRole.BodyStrong])
        self.assertEqual(
            caption_font_changes,
            [fluentqt.FontRole.BodyLarge],
        )

        rating.setValue(99.0)
        self.assertEqual(rating.value(), 7.0)

    def test_scroll_bar_properties_and_inherited_range_signal(self):
        scroll_bar = fluentqt.ScrollBar(Qt.Horizontal)
        thickness_changes = []
        value_changes = []
        scroll_bar.thicknessChanged.connect(
            lambda: thickness_changes.append(scroll_bar.thickness())
        )
        scroll_bar.valueChanged.connect(value_changes.append)

        self.assertEqual(scroll_bar.orientation(), Qt.Horizontal)
        self.assertEqual(scroll_bar.thickness(), 7)
        self.assertEqual(scroll_bar.opacity(), 0.0)

        scroll_bar.setThickness(11)
        scroll_bar.setOpacity(2.0)
        scroll_bar.setRange(0, 100)
        scroll_bar.setValue(42)

        self.assertEqual(thickness_changes, [11])
        self.assertEqual(scroll_bar.opacity(), 1.0)
        self.assertEqual(value_changes, [42])
        self.assertEqual(scroll_bar.sizeHint().height(), 11)

    def test_annotated_scroll_bar_labels_properties_and_static_detail(self):
        label_type = fluentqt.AnnotatedScrollBarLabel
        first = label_type("Start", 0, "Start detail")
        copied = label_type(first)
        self.assertEqual(copied, first)
        self.assertFalse(copied != first)
        with self.assertRaises(TypeError):
            hash(first)
        self.assertEqual(first.text, "Start")
        self.assertEqual(first.offset, 0)
        self.assertEqual(first.detailText, "Start detail")

        copied.text = "Beginning"
        copied.offset = 10
        copied.detailText = "Beginning detail"
        self.assertNotEqual(copied, first)

        bar = fluentqt.AnnotatedScrollBar()
        range_changes = []
        value_changes = []
        page_step_changes = []
        label_changes = []
        metric_changes = []
        detail_requests = []
        bar.rangeChanged.connect(
            lambda minimum, maximum: range_changes.append(
                (minimum, maximum)
            )
        )
        bar.valueChanged.connect(value_changes.append)
        bar.pageStepChanged.connect(page_step_changes.append)
        bar.labelsChanged.connect(lambda: label_changes.append(True))
        bar.layoutMetricsChanged.connect(
            lambda: metric_changes.append(True)
        )
        bar.detailLabelRequested.connect(detail_requests.append)

        bar.setRange(0, 1000)
        bar.setValue(250)
        bar.setPageStep(100)
        bar.setPreferredSize(QSize(144, 420))
        bar.setMinimumBarSize(QSize(72, 160))
        bar.setMinimumLabelSpacing(12)
        bar.setLabels(
            [
                label_type("Middle", 500, "Middle detail"),
                first,
                label_type("End", 1000, "End detail"),
            ]
        )

        self.assertEqual(range_changes, [(0, 1000)])
        self.assertEqual(value_changes, [250])
        self.assertEqual(page_step_changes, [100])
        self.assertEqual(label_changes, [True])
        self.assertEqual(len(metric_changes), 3)
        self.assertEqual(bar.preferredSize(), QSize(144, 420))
        self.assertEqual(bar.minimumBarSize(), QSize(72, 160))
        self.assertEqual(bar.minimumLabelSpacing(), 12)
        self.assertEqual(
            [label.text for label in bar.labels()],
            ["Middle", "Start", "End"],
        )

        bar.resize(144, 240)
        visible = bar.visibleLabels()
        self.assertEqual(
            [label.text for label in visible],
            ["Start", "Middle", "End"],
        )
        self.assertEqual(bar.visibleLabelCount(), 3)
        self.assertFalse(
            hasattr(bar, "setDetailLabelProvider")
        )
        self.assertFalse(
            hasattr(bar, "clearDetailLabelProvider")
        )
        self.assertFalse(
            hasattr(bar, "hasDetailLabelProvider")
        )

        bar.show()
        self.app.processEvents()
        QTest.mouseMove(bar, QPoint(12, 19))
        self.app.processEvents()
        self.assertTrue(bar.isDetailLabelVisible())
        self.assertEqual(bar.detailLabelText(), "Start detail")
        self.assertTrue(detail_requests)
        bar.hide()

        bar.clearLabels()
        self.assertEqual(label_changes, [True, True])
        self.assertEqual(bar.labels(), [])

    def test_annotated_scroll_bar_borrowed_scroll_view_link(self):
        class PythonScrollView(fluentqt.ScrollView):
            def __init__(self):
                super().__init__()
                self.marker = "python-scroll-view"

        view = PythonScrollView()
        content = QWidget()
        content.setFixedSize(420, 480)
        view.resize(180, 120)
        view.setContentWidget(content)
        view.show()
        self.app.processEvents()

        bar = fluentqt.AnnotatedScrollBar()
        bar.connectToScrollView(view)
        self.assertIs(bar.connectedScrollView(), view)
        self.assertEqual(
            bar.connectedScrollView().marker,
            "python-scroll-view",
        )
        self.assertIsNone(view.parent())
        self.assertEqual(
            (bar.minimum(), bar.maximum(), bar.pageStep()),
            (
                view.verticalScrollBar().minimum(),
                view.verticalScrollBar().maximum(),
                view.verticalScrollBar().pageStep(),
            ),
        )

        view.scrollTo(40, 80, False)
        self.app.processEvents()
        self.assertEqual(bar.value(), view.verticalOffset())
        bar.scrollRequested.emit(160)
        self.app.processEvents()
        self.assertEqual(view.verticalOffset(), 160)
        self.assertEqual(bar.value(), 160)

        previous = bar.value()
        bar.disconnectScrollView()
        self.assertIsNone(bar.connectedScrollView())
        view.scrollTo(40, 0, False)
        self.app.processEvents()
        self.assertEqual(bar.value(), previous)

        bar.connectToScrollView(view)
        view_ref = weakref.ref(view)
        del view
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertIsNone(bar.connectedScrollView())

    def test_pips_pager_properties_signals_and_navigation(self):
        pager = fluentqt.PipsPager()
        visibility = fluentqt.PipsPager.PipsPagerButtonVisibility
        page_count_changes = []
        selected_page_changes = []
        selected_index_changes = []
        visible_count_changes = []
        orientation_changes = []
        previous_visibility_changes = []
        next_visibility_changes = []
        pager.numberOfPagesChanged.connect(page_count_changes.append)
        pager.selectedPageIndexChanged.connect(
            selected_page_changes.append
        )
        pager.selectedIndexChanged.connect(
            lambda old, new: selected_index_changes.append((old, new))
        )
        pager.maxVisiblePipsChanged.connect(visible_count_changes.append)
        pager.orientationChanged.connect(orientation_changes.append)
        pager.previousButtonVisibilityChanged.connect(
            previous_visibility_changes.append
        )
        pager.nextButtonVisibilityChanged.connect(
            next_visibility_changes.append
        )

        pager.setSelectionAnimationEnabled(False)
        pager.setNumberOfPages(7)
        pager.setMaxVisiblePips(3)
        pager.setPreviousButtonVisibility(
            visibility.Visible
        )
        pager.setNextButtonVisibility(
            visibility.VisibleOnPointerOver
        )
        pager.setSelectedPageIndex(4)
        pager.setOrientation(Qt.Vertical)

        self.assertEqual(page_count_changes, [7])
        self.assertEqual(selected_page_changes, [4])
        self.assertEqual(selected_index_changes, [(0, 4)])
        self.assertEqual(visible_count_changes, [3])
        self.assertEqual(orientation_changes, [Qt.Vertical])
        self.assertEqual(
            previous_visibility_changes,
            [visibility.Visible],
        )
        self.assertEqual(
            next_visibility_changes,
            [visibility.VisibleOnPointerOver],
        )
        self.assertEqual(pager.visiblePipCount(), 3)
        self.assertEqual(pager.firstVisiblePage(), 3)
        self.assertFalse(pager.pipHitRect(0).isValid())
        self.assertTrue(pager.pipHitRect(4).isValid())
        self.assertTrue(pager.hasPreviousPage())
        self.assertTrue(pager.hasNextPage())
        self.assertTrue(pager.goToPreviousPage())
        self.assertEqual(pager.selectedPageIndex(), 3)
        self.assertTrue(pager.goToNextPage())
        self.assertEqual(pager.selectedPageIndex(), 4)

        configured = fluentqt.PipsPager(
            numberOfPages=9,
            selectedPageIndex=5,
            maxVisiblePips=5,
            selectionAnimationEnabled=False,
        )
        self.assertEqual(configured.numberOfPages(), 9)
        self.assertEqual(configured.selectedPageIndex(), 5)
        self.assertEqual(configured.maxVisiblePips(), 5)
        self.assertFalse(configured.selectionAnimationEnabled())
        meta_object = configured.metaObject()
        self.assertEqual(
            meta_object.indexOfProperty("selectedVisualOffset"),
            -1,
        )
        self.assertEqual(
            meta_object.indexOfProperty("visibleWindowOffset"),
            -1,
        )

    def test_info_bar_properties_signals_and_action_facade(self):
        action = fluentqt.Button("Details")
        bar = fluentqt.InfoBar(
            title="Bindings ready",
            message="Native action hosting",
            severity=fluentqt.InfoBar.InfoBarSeverity.Success,
            isClosable=False,
            actionWidget=action,
        )
        title_changes = []
        message_changes = []
        severity_changes = []
        action_changes = []
        bar.titleChanged.connect(title_changes.append)
        bar.messageChanged.connect(message_changes.append)
        bar.severityChanged.connect(severity_changes.append)
        bar.actionWidgetChanged.connect(action_changes.append)

        self.assertEqual(bar.title(), "Bindings ready")
        self.assertEqual(bar.message(), "Native action hosting")
        self.assertEqual(
            bar.severity(),
            fluentqt.InfoBar.InfoBarSeverity.Success,
        )
        self.assertFalse(bar.isClosable())
        self.assertIs(bar.actionWidget(), action)
        self.assertIs(bar._fluentqt_action_widget, action)
        self.assertIs(action.parentWidget(), bar)

        bar.setTitle("Updated")
        bar.setMessage("Updated message")
        bar.setSeverity(fluentqt.InfoBar.InfoBarSeverity.Warning)
        self.assertEqual(title_changes, ["Updated"])
        self.assertEqual(message_changes, ["Updated message"])
        self.assertEqual(
            severity_changes,
            [fluentqt.InfoBar.InfoBarSeverity.Warning],
        )

        replacement = fluentqt.HyperlinkButton("Learn more")
        bar.setActionWidget(replacement)
        self.assertIsNone(action.parent())
        self.assertTrue(Shiboken.isValid(action))
        self.assertIs(bar.actionWidget(), replacement)
        self.assertIs(bar._fluentqt_action_widget, replacement)
        self.assertEqual(action_changes, [replacement])

        taken = bar.takeActionWidget()
        self.assertIs(taken, replacement)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))
        self.assertIsNone(bar.actionWidget())
        self.assertIsNone(bar._fluentqt_action_widget)
        self.assertEqual(action_changes, [replacement, None])

    def test_info_bar_retains_python_action_and_tracks_external_delete(self):
        class PythonAction(fluentqt.Button):
            def __init__(self):
                super().__init__("Python action")
                self.marker = "python-subclass"

        bar = fluentqt.InfoBar()
        action = PythonAction()
        action_ref = weakref.ref(action)
        changes = []
        bar.actionWidgetChanged.connect(changes.append)
        bar.setActionWidget(action)

        del action
        gc.collect()
        hosted = bar.actionWidget()
        self.assertIs(hosted, action_ref())
        self.assertEqual(hosted.marker, "python-subclass")
        self.assertIs(bar._fluentqt_action_widget, hosted)

        # Exercise the supported Qt-owned external-destruction path. Direct
        # Shiboken.delete() on a still-parented Python subclass can fast-fail
        # inside PySide6 6.2.4 on Windows before Qt can finish its destroyed
        # signal chain, so that low-level wrapper operation is not part of the
        # public lifecycle contract.
        hosted.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(hosted))
        self.assertIsNone(bar.actionWidget())
        self.assertIsNone(bar._fluentqt_action_widget)
        self.assertEqual(len(changes), 2)
        self.assertIsNone(changes[-1])

    def test_info_bar_rejects_invalid_actions(self):
        parent = QWidget()
        bar = fluentqt.InfoBar(parent)

        with self.assertRaisesRegex(
            ValueError,
            "host or its ancestor",
        ):
            bar.setActionWidget(bar)
        with self.assertRaisesRegex(
            ValueError,
            "host or its ancestor",
        ):
            bar.setActionWidget(parent)
        self.assertIsNone(bar.actionWidget())

    def test_info_bar_hosted_action_gc_stress(self):
        for _ in range(25):
            bar = fluentqt.InfoBar()
            action = fluentqt.Button("Action")
            bar.setActionWidget(action)
            bar_ref = weakref.ref(bar)
            del bar
            gc.collect()
            self.assertIsNone(bar_ref())
            self.assertFalse(Shiboken.isValid(action))
            del action

    def test_theme_api(self):
        previous_theme = fluentqt.current_theme()
        try:
            fluentqt.reset_theme_tokens()
            initial_revision = fluentqt.theme_revision()

            fluentqt.set_theme(fluentqt.Theme.Dark)
            self.assertEqual(fluentqt.current_theme(), fluentqt.Theme.Dark)

            fluentqt.apply_style_theme(fluentqt.StyleTheme.Material)
            self.assertEqual(
                fluentqt.current_design_language(),
                fluentqt.DesignLanguage.DesignMaterial,
            )
            self.assertGreater(fluentqt.theme_revision(), initial_revision)

            accent_revision = fluentqt.theme_revision()
            accent = QColor("#7f52ff")
            fluentqt.set_accent_color(accent)
            self.assertGreater(fluentqt.theme_revision(), accent_revision)
            self.assertEqual(fluentqt.accent_color(), accent)

            fluentqt.set_font_scale(1.25)
            self.assertAlmostEqual(fluentqt.font_scale(), 1.25)
            resolved_font = fluentqt.font_for_role(
                fluentqt.FontRole.BodyStrong
            )
            self.assertEqual(resolved_font.pixelSize(), 18)

            label = fluentqt.Label()
            label.setFluentTypography(fluentqt.FontRole.BodyStrong)
            self.assertEqual(label.font(), resolved_font)
        finally:
            fluentqt.reset_theme_tokens()
            fluentqt.set_theme(previous_theme)

    def test_python_virtual_override(self):
        class EventButton(fluentqt.Button):
            def __init__(self):
                super().__init__("Event")
                self.user_events = 0

            def event(self, event):
                if event.type() == QEvent.User:
                    self.user_events += 1
                return super().event(event)

        button = EventButton()
        QCoreApplication.sendEvent(button, QEvent(QEvent.User))
        self.assertEqual(button.user_events, 1)

    def test_native_event_uses_safe_pyside_contract(self):
        self.assertNotIn("result", fluentqt.Window.nativeEvent.__doc__)

        class NativeEventWindow(fluentqt.Window):
            def nativeEvent(self, event_type, message):
                return False, 0

        window = NativeEventWindow()
        self.assertEqual(
            window.nativeEvent(QByteArray(b"fluentqt-test"), 0),
            (False, 0),
        )

    def test_window_content_parenting_and_release(self):
        window = fluentqt.Window()
        first = QWidget()
        second = QWidget()

        window.setContentWidget(first)
        self.assertIs(window.contentWidget(), first)
        self.assertIs(first.parent(), window.contentHost())

        window.setContentWidget(second)
        self.assertIsNone(first.parent())
        self.assertTrue(Shiboken.isValid(first))
        self.assertIs(window.contentWidget(), second)
        self.assertIs(second.parent(), window.contentHost())

        window.setContentWidget(None)
        gc.collect()
        self.assertIsNone(second.parent())
        self.assertTrue(Shiboken.isValid(second))

        owned_window = fluentqt.Window()
        owned_child = QWidget()
        owned_window.setContentWidget(owned_child)
        owned_window_ref = weakref.ref(owned_window)
        del owned_window
        gc.collect()
        self.assertIsNone(owned_window_ref())
        self.assertFalse(Shiboken.isValid(owned_child))

    def test_expander_constructor_routes_content_through_facade(self):
        content = QWidget()
        expander = fluentqt.Expander(contentWidget=content)

        self.assertIs(expander.contentWidget(), content)
        self.assertIs(expander._fluentqt_hosted_content, content)
        self.assertEqual(
            expander.contentOwnership(),
            fluentqt.WidgetOwnership.Borrowed,
        )

        expander_ref = weakref.ref(expander)
        del expander
        gc.collect()
        self.assertIsNone(expander_ref())
        self.assertTrue(Shiboken.isValid(content))
        self.assertIsNone(content.parent())

    def test_expander_owned_content_lifecycle(self):
        expander = fluentqt.Expander()
        first = QWidget()
        second = QWidget()

        expander.setOwnedContentWidget(first)
        self.assertIs(expander.contentWidget(), first)
        self.assertIs(expander._fluentqt_hosted_content, first)
        self.assertEqual(
            expander.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

        expander.setOwnedContentWidget(second)
        self.assertFalse(Shiboken.isValid(first))
        self.assertIs(expander.contentWidget(), second)

        expander.setOwnedContentWidget(None)
        self.assertFalse(Shiboken.isValid(second))
        self.assertIsNone(expander.contentWidget())
        self.assertIsNone(expander._fluentqt_hosted_content)
        self.assertEqual(
            expander.contentOwnership(),
            fluentqt.WidgetOwnership.Borrowed,
        )

        owned_expander = fluentqt.Expander()
        owned_content = QWidget()
        owned_expander.setOwnedContentWidget(owned_content)
        owned_expander_ref = weakref.ref(owned_expander)
        del owned_expander
        gc.collect()
        self.assertIsNone(owned_expander_ref())
        self.assertFalse(Shiboken.isValid(owned_content))

    def test_expander_borrowed_content_lifecycle(self):
        class PythonContent(QWidget):
            def __init__(self):
                super().__init__()
                self.marker = "expander-borrowed-subclass"

        expander = fluentqt.Expander()
        first = PythonContent()
        first_ref = weakref.ref(first)
        expander.setContentWidget(first)
        del first
        gc.collect()

        hosted = expander.contentWidget()
        self.assertIs(hosted, first_ref())
        self.assertEqual(hosted.marker, "expander-borrowed-subclass")
        self.assertIs(expander._fluentqt_hosted_content, hosted)
        self.assertEqual(
            expander.contentOwnership(),
            fluentqt.WidgetOwnership.Borrowed,
        )

        second = QWidget()
        expander.setBorrowedContentWidget(second)
        self.assertTrue(Shiboken.isValid(hosted))
        self.assertIsNone(hosted.parent())

        expander.setContentWidget(None)
        self.assertTrue(Shiboken.isValid(second))
        self.assertIsNone(second.parent())
        self.assertIsNone(expander.contentWidget())

        previous_parent = QWidget()
        previous_parent_ref = weakref.ref(previous_parent)
        parented = QWidget(previous_parent)
        parented_expander = fluentqt.Expander()
        parented_expander.setBorrowedContentWidget(parented)
        del previous_parent
        gc.collect()
        self.assertIsNone(previous_parent_ref())
        self.assertTrue(Shiboken.isValid(parented))

        parented_expander_ref = weakref.ref(parented_expander)
        del parented_expander
        gc.collect()
        self.assertIsNone(parented_expander_ref())
        self.assertTrue(Shiboken.isValid(parented))
        self.assertIsNone(parented.parent())

    def test_expander_reparented_content_lifecycle(self):
        first_parent = QWidget()
        first = QWidget(first_parent)
        second_parent = QWidget()
        second = QWidget(second_parent)
        expander = fluentqt.Expander()

        expander.setReparentedContentWidget(first)
        self.assertIsNot(first.parent(), first_parent)
        self.assertIs(expander._fluentqt_original_parent, first_parent)
        self.assertEqual(
            expander.contentOwnership(),
            fluentqt.WidgetOwnership.Reparented,
        )

        expander.setReparentedContentWidget(second)
        self.assertTrue(Shiboken.isValid(first))
        self.assertIs(first.parent(), first_parent)
        self.assertIsNot(second.parent(), second_parent)

        expander.setReparentedContentWidget(None)
        self.assertTrue(Shiboken.isValid(second))
        self.assertIs(second.parent(), second_parent)
        self.assertIsNone(expander.contentWidget())

        restore_parent = QWidget()
        restored = QWidget(restore_parent)
        restoring_expander = fluentqt.Expander()
        restoring_expander.setReparentedContentWidget(restored)
        restoring_expander_ref = weakref.ref(restoring_expander)
        del restoring_expander
        gc.collect()
        self.assertIsNone(restoring_expander_ref())
        self.assertTrue(Shiboken.isValid(restored))
        self.assertIs(restored.parent(), restore_parent)

    def test_expander_reparented_keeps_original_parent_alive(self):
        original_parent = QWidget()
        original_parent_ref = weakref.ref(original_parent)
        child = QWidget(original_parent)
        expander = fluentqt.Expander()
        expander.setReparentedContentWidget(child)

        del original_parent
        gc.collect()
        self.assertIsNotNone(original_parent_ref())
        self.assertIs(
            expander._fluentqt_original_parent,
            original_parent_ref(),
        )

        taken = expander.takeContentWidget()
        gc.collect()
        self.assertIs(taken, child)
        self.assertIsNone(taken.parent())
        self.assertIsNone(original_parent_ref())

    def test_expander_rejects_invalid_content_and_mode_changes(self):
        parent = QWidget()
        expander = fluentqt.Expander(parent)
        child = QWidget()
        expander.setBorrowedContentWidget(child)

        with self.assertRaisesRegex(
            ValueError,
            "takeContentWidget",
        ):
            expander.setOwnedContentWidget(child)
        with self.assertRaisesRegex(
            ValueError,
            "host or its ancestor",
        ):
            expander.setBorrowedContentWidget(expander)
        with self.assertRaisesRegex(
            ValueError,
            "host or its ancestor",
        ):
            expander.setBorrowedContentWidget(parent)

        self.assertIs(expander.parent(), parent)
        self.assertIs(expander.contentWidget(), child)

    def test_expander_take_handles_all_modes(self):
        for mode_name, setter_name in (
            ("Owned", "setOwnedContentWidget"),
            ("Borrowed", "setBorrowedContentWidget"),
            ("Reparented", "setReparentedContentWidget"),
        ):
            with self.subTest(mode=mode_name):
                original_parent = (
                    QWidget() if mode_name == "Reparented" else None
                )
                child = QWidget(original_parent)
                expander = fluentqt.Expander()
                getattr(expander, setter_name)(child)

                taken = expander.takeContentWidget()
                self.assertIs(taken, child)
                self.assertIsNone(taken.parent())
                self.assertTrue(Shiboken.ownedByPython(taken))
                self.assertIsNone(expander._fluentqt_hosted_content)
                self.assertIsNone(expander._fluentqt_original_parent)
                self.assertEqual(
                    expander.contentOwnership(),
                    fluentqt.WidgetOwnership.Borrowed,
                )

                expander_ref = weakref.ref(expander)
                del expander
                gc.collect()
                self.assertIsNone(expander_ref())
                self.assertTrue(Shiboken.isValid(taken))
                taken_ref = weakref.ref(taken)
                del taken
                del child
                gc.collect()
                self.assertIsNone(taken_ref())

    def test_expander_owned_gc_stress(self):
        for _ in range(25):
            expander = fluentqt.Expander()
            child = QWidget()
            expander.setOwnedContentWidget(child)
            expander_ref = weakref.ref(expander)
            del expander
            gc.collect()
            self.assertIsNone(expander_ref())
            self.assertFalse(Shiboken.isValid(child))
            del child

    def test_expander_borrowed_gc_stress(self):
        for _ in range(25):
            expander = fluentqt.Expander()
            child = QWidget()
            expander.setBorrowedContentWidget(child)
            expander_ref = weakref.ref(expander)
            del expander
            gc.collect()
            self.assertIsNone(expander_ref())
            self.assertTrue(Shiboken.isValid(child))
            self.assertIsNone(child.parent())
            del child
            gc.collect()

    def test_expander_reparented_gc_stress(self):
        for _ in range(25):
            original_parent = QWidget()
            child = QWidget(original_parent)
            expander = fluentqt.Expander()
            expander.setReparentedContentWidget(child)
            expander_ref = weakref.ref(expander)
            del expander
            gc.collect()
            self.assertIsNone(expander_ref())
            self.assertTrue(Shiboken.isValid(child))
            self.assertIs(child.parent(), original_parent)
            del original_parent
            gc.collect()
            self.assertFalse(Shiboken.isValid(child))
            del child

    def test_accordion_properties_signals_and_item_facade(self):
        class PythonItem(fluentqt.Expander):
            def __init__(self, title):
                super().__init__()
                self.setHeaderText(title)
                self.setAnimationEnabled(False)
                self.marker = "python-accordion-item"

        accordion = fluentqt.Accordion()
        self.assertEqual(
            accordion.expansionMode(),
            fluentqt.Accordion.ExpansionMode.Single,
        )
        self.assertEqual(accordion.count(), 0)

        mode_changes = []
        count_changes = []
        added = []
        removed = []
        expansion_changes = []
        accordion.expansionModeChanged.connect(mode_changes.append)
        accordion.countChanged.connect(count_changes.append)
        accordion.itemAdded.connect(
            lambda index, item: added.append((index, item))
        )
        accordion.itemRemoved.connect(
            lambda index, item: removed.append((index, item))
        )
        accordion.itemExpansionChanged.connect(
            lambda index, expanded: expansion_changes.append(
                (index, expanded)
            )
        )

        first = PythonItem("First")
        second = PythonItem("Second")
        self.assertTrue(accordion.addItem(first))
        self.assertTrue(accordion.insertOwnedItem(0, second))
        self.assertEqual(accordion.count(), 2)
        self.assertIs(accordion.itemAt(0), second)
        self.assertIs(accordion.itemAt(1), first)
        self.assertEqual(first.marker, "python-accordion-item")
        self.assertEqual(accordion.indexOf(second), 0)
        self.assertEqual(
            accordion.itemOwnershipAt(0),
            fluentqt.WidgetOwnership.Owned,
        )
        self.assertEqual(
            accordion.itemOwnershipAt(1),
            fluentqt.WidgetOwnership.Borrowed,
        )
        self.assertEqual(count_changes, [1, 2])
        self.assertEqual(added, [(0, first), (0, second)])

        first.setExpanded(True)
        second.setExpanded(True)
        self.assertFalse(first.isExpanded())
        self.assertTrue(second.isExpanded())
        self.assertGreaterEqual(len(expansion_changes), 3)

        accordion.setExpansionMode(
            fluentqt.Accordion.ExpansionMode.Multiple
        )
        accordion.setExpansionMode(
            fluentqt.Accordion.ExpansionMode.Multiple
        )
        self.assertEqual(
            mode_changes,
            [fluentqt.Accordion.ExpansionMode.Multiple],
        )
        first.setExpanded(True)
        self.assertTrue(first.isExpanded())
        self.assertTrue(second.isExpanded())

        taken = accordion.takeItem(0)
        self.assertIs(taken, second)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))
        self.assertEqual(removed, [(0, second)])
        self.assertTrue(accordion.removeItem(0))
        self.assertTrue(Shiboken.isValid(first))
        self.assertIsNone(first.parent())
        self.assertEqual(accordion.count(), 0)

    def test_accordion_owned_item_lifecycle(self):
        accordion = fluentqt.Accordion()
        removed_item = fluentqt.Expander()
        self.assertTrue(accordion.addOwnedItem(removed_item))
        self.assertTrue(accordion.removeItem(0))
        self.assertFalse(Shiboken.isValid(removed_item))

        owned_item = fluentqt.Expander()
        self.assertTrue(accordion.addOwnedItem(owned_item))
        accordion_ref = weakref.ref(accordion)
        del accordion
        gc.collect()
        self.assertIsNone(accordion_ref())
        self.assertFalse(Shiboken.isValid(owned_item))

    def test_accordion_borrowed_item_lifecycle(self):
        class PythonItem(fluentqt.Expander):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.marker = "borrowed-accordion-subclass"

        previous_parent = QWidget()
        previous_parent_ref = weakref.ref(previous_parent)
        item = PythonItem(previous_parent)
        item_ref = weakref.ref(item)
        accordion = fluentqt.Accordion()
        self.assertTrue(accordion.addBorrowedItem(item))
        self.assertIs(item.parentWidget(), accordion)

        del previous_parent
        del item
        gc.collect()
        self.assertIsNone(previous_parent_ref())
        hosted = accordion.itemAt(0)
        self.assertIs(hosted, item_ref())
        self.assertEqual(hosted.marker, "borrowed-accordion-subclass")

        accordion_ref = weakref.ref(accordion)
        del accordion
        gc.collect()
        self.assertIsNone(accordion_ref())
        self.assertTrue(Shiboken.isValid(hosted))
        self.assertIsNone(hosted.parent())

    def test_accordion_reparented_item_lifecycle(self):
        original_parent = QWidget()
        original_parent_ref = weakref.ref(original_parent)
        item = fluentqt.Expander(original_parent)
        accordion = fluentqt.Accordion()
        self.assertTrue(accordion.addReparentedItem(item))
        self.assertIs(item.parentWidget(), accordion)

        del original_parent
        gc.collect()
        self.assertIsNotNone(original_parent_ref())

        taken = accordion.takeItem(0)
        gc.collect()
        self.assertIs(taken, item)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))
        self.assertIsNone(original_parent_ref())

        restore_parent = QWidget()
        restored = fluentqt.Expander(restore_parent)
        restoring_accordion = fluentqt.Accordion()
        self.assertTrue(
            restoring_accordion.insertReparentedItem(0, restored)
        )
        restoring_ref = weakref.ref(restoring_accordion)
        del restoring_accordion
        gc.collect()
        self.assertIsNone(restoring_ref())
        self.assertTrue(Shiboken.isValid(restored))
        self.assertIs(restored.parent(), restore_parent)

    def test_accordion_rejects_duplicates_and_tracks_external_delete(self):
        accordion = fluentqt.Accordion()
        item = fluentqt.Expander()
        self.assertFalse(accordion.addItem(None))
        self.assertTrue(accordion.addItem(item))
        self.assertFalse(accordion.addOwnedItem(item))
        self.assertEqual(accordion.count(), 1)

        item.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(item))
        self.assertEqual(accordion.count(), 0)
        self.assertEqual(accordion._fluentqt_item_records, {})

    def test_accordion_owned_gc_stress(self):
        for _ in range(25):
            accordion = fluentqt.Accordion()
            item = fluentqt.Expander()
            accordion.addOwnedItem(item)
            accordion_ref = weakref.ref(accordion)
            del accordion
            gc.collect()
            self.assertIsNone(accordion_ref())
            self.assertFalse(Shiboken.isValid(item))
            del item

    def test_accordion_borrowed_gc_stress(self):
        for _ in range(25):
            accordion = fluentqt.Accordion()
            item = fluentqt.Expander()
            accordion.addBorrowedItem(item)
            accordion_ref = weakref.ref(accordion)
            del accordion
            gc.collect()
            self.assertIsNone(accordion_ref())
            self.assertTrue(Shiboken.isValid(item))
            self.assertIsNone(item.parent())
            del item
            gc.collect()

    def test_accordion_reparented_gc_stress(self):
        for _ in range(25):
            original_parent = QWidget()
            item = fluentqt.Expander(original_parent)
            accordion = fluentqt.Accordion()
            accordion.addReparentedItem(item)
            accordion_ref = weakref.ref(accordion)
            del accordion
            gc.collect()
            self.assertIsNone(accordion_ref())
            self.assertTrue(Shiboken.isValid(item))
            self.assertIs(item.parent(), original_parent)
            del original_parent
            gc.collect()
            self.assertFalse(Shiboken.isValid(item))
            del item

    def test_flip_view_properties_navigation_and_page_facade(self):
        class PythonPage(QWidget):
            def __init__(self, marker):
                super().__init__()
                self.marker = marker

        view = fluentqt.FlipView()
        self.assertIs(collections.FlipView, fluentqt.FlipView)
        self.assertEqual(view.pageCount(), 0)
        self.assertEqual(view.currentIndex(), -1)
        self.assertEqual(view.orientation(), Qt.Horizontal)
        self.assertTrue(view.showNavigationButtons())
        self.assertTrue(view.showPageIndicator())

        changes = []
        view.currentIndexChanged.connect(changes.append)
        first = PythonPage("owned-first")
        second = PythonPage("borrowed-second")
        self.assertTrue(view.addPage(first))
        self.assertTrue(view.insertBorrowedPage(0, second))
        self.assertEqual(view.pageCount(), 2)
        self.assertEqual(view.currentIndex(), 1)
        self.assertIs(view.pageAt(0), second)
        self.assertIs(view.pageAt(1), first)
        self.assertEqual(view.pageAt(0).marker, "borrowed-second")
        self.assertEqual(
            view.pageOwnershipAt(0),
            fluentqt.WidgetOwnership.Borrowed,
        )
        self.assertEqual(
            view.pageOwnershipAt(1),
            fluentqt.WidgetOwnership.Owned,
        )

        view.goPrevious()
        self.assertEqual(view.currentIndex(), 0)
        view.setOrientation(Qt.Vertical)
        view.setShowNavigationButtons(False)
        view.setShowPageIndicator(False)
        self.assertEqual(view.orientation(), Qt.Vertical)
        self.assertFalse(view.areNavigationButtonsVisible())
        self.assertFalse(view.isPageIndicatorVisible())

        taken = view.takePage(0)
        self.assertIs(taken, second)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))
        self.assertTrue(view.removePage(0))
        self.assertFalse(Shiboken.isValid(first))
        self.assertEqual(view.pageCount(), 0)
        self.assertEqual(view._fluentqt_page_records, {})
        self.assertGreaterEqual(len(changes), 2)

    def test_flip_view_owned_page_lifecycle(self):
        view = fluentqt.FlipView()
        page = QWidget()
        self.assertTrue(view.addOwnedPage(page))
        self.assertTrue(view.removePage(0))
        self.assertFalse(Shiboken.isValid(page))

        second_view = fluentqt.FlipView()
        second_page = QWidget()
        self.assertTrue(second_view.addPage(second_page))
        view_ref = weakref.ref(second_view)
        del second_view
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertFalse(Shiboken.isValid(second_page))

    def test_flip_view_borrowed_page_lifecycle(self):
        class PythonPage(QWidget):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.marker = "borrowed-flip-page"

        previous_parent = QWidget()
        previous_parent_ref = weakref.ref(previous_parent)
        page = PythonPage(previous_parent)
        page_ref = weakref.ref(page)
        view = fluentqt.FlipView()
        self.assertTrue(view.addBorrowedPage(page))
        self.assertIs(page.parent(), view)

        del previous_parent
        del page
        gc.collect()
        self.assertIsNone(previous_parent_ref())
        hosted = view.pageAt(0)
        self.assertIs(hosted, page_ref())
        self.assertEqual(hosted.marker, "borrowed-flip-page")

        view_ref = weakref.ref(view)
        del view
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertTrue(Shiboken.isValid(hosted))
        self.assertIsNone(hosted.parent())

    def test_flip_view_reparented_page_lifecycle(self):
        original_parent = QWidget()
        original_parent_ref = weakref.ref(original_parent)
        page = QWidget(original_parent)
        view = fluentqt.FlipView()
        self.assertTrue(view.addReparentedPage(page))
        self.assertIs(page.parent(), view)

        del original_parent
        gc.collect()
        restored_parent = original_parent_ref()
        self.assertIsNotNone(restored_parent)
        self.assertTrue(view.removePage(0))
        self.assertTrue(Shiboken.isValid(page))
        self.assertIs(page.parent(), restored_parent)

        second_parent = QWidget()
        second_page = QWidget(second_parent)
        second_view = fluentqt.FlipView()
        self.assertTrue(second_view.addReparentedPage(second_page))
        view_ref = weakref.ref(second_view)
        del second_view
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertTrue(Shiboken.isValid(second_page))
        self.assertIs(second_page.parent(), second_parent)

    def test_flip_view_rejects_duplicates_and_tracks_external_delete(self):
        view = fluentqt.FlipView()
        page = QWidget()
        self.assertFalse(view.addPage(None))
        self.assertTrue(view.addBorrowedPage(page))
        self.assertFalse(view.addOwnedPage(page))

        page.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(page))
        self.assertEqual(view.pageCount(), 0)
        self.assertEqual(view.currentIndex(), -1)
        self.assertEqual(view._fluentqt_page_records, {})

        ancestor = QWidget()
        nested_view = fluentqt.FlipView(ancestor)
        with self.assertRaisesRegex(ValueError, "host or its ancestor"):
            nested_view.addBorrowedPage(ancestor)

    def test_flip_view_owned_gc_stress(self):
        for _ in range(25):
            view = fluentqt.FlipView()
            page = QWidget()
            view.addOwnedPage(page)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertFalse(Shiboken.isValid(page))
            del page

    def test_flip_view_borrowed_gc_stress(self):
        for _ in range(25):
            view = fluentqt.FlipView()
            page = QWidget()
            view.addBorrowedPage(page)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertTrue(Shiboken.isValid(page))
            self.assertIsNone(page.parent())
            del page
            gc.collect()

    def test_flip_view_reparented_gc_stress(self):
        for _ in range(25):
            original_parent = QWidget()
            page = QWidget(original_parent)
            view = fluentqt.FlipView()
            view.addReparentedPage(page)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertTrue(Shiboken.isValid(page))
            self.assertIs(page.parent(), original_parent)
            del original_parent
            gc.collect()
            self.assertFalse(Shiboken.isValid(page))
            del page

    def test_split_view_options_properties_and_pane_facade(self):
        class PythonPane(QWidget):
            def __init__(self, marker):
                super().__init__()
                self.marker = marker

        options = fluentqt.SplitViewPaneOptions(40, 100, 260, False)
        self.assertEqual(options.minimumSize, 40)
        self.assertEqual(options.preferredSize, 100)
        self.assertEqual(options.maximumSize, 260)
        self.assertFalse(options.fill)
        self.assertEqual(fluentqt.SplitViewPaneOptions(options), options)
        with self.assertRaises(TypeError):
            hash(options)

        default_options = fluentqt.SplitViewPaneOptions()
        self.assertEqual(default_options.minimumSize, 48)
        self.assertEqual(default_options.preferredSize, 160)
        self.assertEqual(default_options.maximumSize, 16777215)
        self.assertFalse(default_options.fill)

        partial_options = fluentqt.SplitViewPaneOptions(24, 96)
        self.assertEqual(partial_options.minimumSize, 24)
        self.assertEqual(partial_options.preferredSize, 96)
        self.assertEqual(partial_options.maximumSize, 16777215)
        self.assertFalse(partial_options.fill)

        view = fluentqt.SplitView()
        self.assertIs(collections.SplitView, fluentqt.SplitView)
        self.assertIs(
            collections.SplitViewPaneOptions,
            fluentqt.SplitViewPaneOptions,
        )
        self.assertIs(view.PaneOptions, fluentqt.SplitViewPaneOptions)
        self.assertEqual(view.paneCount(), 0)
        self.assertEqual(view.orientation(), Qt.Horizontal)

        counts = []
        view.paneCountChanged.connect(counts.append)
        first = PythonPane("owned-first")
        second = PythonPane("borrowed-second")
        self.assertEqual(view.addPane(first, options), 0)
        self.assertEqual(
            view.insertBorrowedPane(
                0,
                second,
                fluentqt.SplitViewPaneOptions(30, 80, 180, True),
            ),
            0,
        )
        self.assertEqual(view.paneCount(), 2)
        self.assertIs(view.paneAt(0), second)
        self.assertIs(view.paneAt(1), first)
        self.assertEqual(view.paneAt(0).marker, "borrowed-second")
        self.assertEqual(
            view.paneOwnershipAt(0),
            fluentqt.WidgetOwnership.Borrowed,
        )
        self.assertEqual(
            view.paneOwnershipAt(1),
            fluentqt.WidgetOwnership.Owned,
        )
        self.assertTrue(view.isPaneFill(0))
        self.assertEqual(view.paneMinimumSize(1), 40)
        self.assertEqual(view.panePreferredSize(1), 100)
        self.assertEqual(view.paneMaximumSize(1), 260)

        view.setOrientation(Qt.Vertical)
        view.setPanePreferredSize(1, 120)
        view.setHandleWidth(10)
        view.setHandleVisualThickness(3)
        self.assertEqual(view.orientation(), Qt.Vertical)
        self.assertEqual(view.panePreferredSize(1), 120)
        self.assertEqual(view.handleWidth(), 10)
        self.assertEqual(view.handleVisualThickness(), 3)
        state = view.saveState()
        self.assertFalse(state.isEmpty())

        taken = view.takePaneAt(0)
        self.assertIs(taken, second)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))
        self.assertTrue(view.removePane(first))
        self.assertFalse(Shiboken.isValid(first))
        self.assertEqual(view.paneCount(), 0)
        self.assertEqual(view._fluentqt_pane_records, {})
        self.assertEqual(counts, [1, 2, 1, 0])

    def test_split_view_owned_pane_lifecycle(self):
        view = fluentqt.SplitView()
        pane = QWidget()
        self.assertEqual(view.addOwnedPane(pane), 0)
        self.assertTrue(view.removePaneAt(0))
        self.assertFalse(Shiboken.isValid(pane))

        second_view = fluentqt.SplitView()
        second_pane = QWidget()
        self.assertEqual(second_view.addPane(second_pane), 0)
        view_ref = weakref.ref(second_view)
        del second_view
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertFalse(Shiboken.isValid(second_pane))

    def test_split_view_borrowed_pane_lifecycle(self):
        class PythonPane(QWidget):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.marker = "borrowed-split-pane"

        previous_parent = QWidget()
        previous_parent_ref = weakref.ref(previous_parent)
        pane = PythonPane(previous_parent)
        pane_ref = weakref.ref(pane)
        view = fluentqt.SplitView()
        self.assertEqual(view.addBorrowedPane(pane), 0)
        self.assertIs(pane.parent(), view)

        del previous_parent
        del pane
        gc.collect()
        self.assertIsNone(previous_parent_ref())
        hosted = view.paneAt(0)
        self.assertIs(hosted, pane_ref())
        self.assertEqual(hosted.marker, "borrowed-split-pane")

        view_ref = weakref.ref(view)
        del view
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertTrue(Shiboken.isValid(hosted))
        self.assertIsNone(hosted.parent())

    def test_split_view_reparented_pane_lifecycle(self):
        original_parent = QWidget()
        original_parent_ref = weakref.ref(original_parent)
        pane = QWidget(original_parent)
        view = fluentqt.SplitView()
        self.assertEqual(view.addReparentedPane(pane), 0)
        self.assertIs(pane.parent(), view)

        del original_parent
        gc.collect()
        restored_parent = original_parent_ref()
        self.assertIsNotNone(restored_parent)
        self.assertTrue(view.removePaneAt(0))
        self.assertTrue(Shiboken.isValid(pane))
        self.assertIs(pane.parent(), restored_parent)

        second_parent = QWidget()
        second_pane = QWidget(second_parent)
        second_view = fluentqt.SplitView()
        self.assertEqual(second_view.addReparentedPane(second_pane), 0)
        view_ref = weakref.ref(second_view)
        del second_view
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertTrue(Shiboken.isValid(second_pane))
        self.assertIs(second_pane.parent(), second_parent)

    def test_split_view_rejects_duplicates_and_tracks_external_delete(self):
        view = fluentqt.SplitView()
        pane = QWidget()
        self.assertEqual(view.addPane(None), -1)
        self.assertEqual(view.addBorrowedPane(pane), 0)
        self.assertEqual(view.addOwnedPane(pane), -1)
        with self.assertRaisesRegex(TypeError, "SplitViewPaneOptions"):
            view.addOwnedPane(QWidget(), object())

        pane.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(pane))
        self.assertEqual(view.paneCount(), 0)
        self.assertEqual(view._fluentqt_pane_records, {})

        ancestor = QWidget()
        nested_view = fluentqt.SplitView(ancestor)
        with self.assertRaisesRegex(ValueError, "host or its ancestor"):
            nested_view.addBorrowedPane(ancestor)

    def test_split_view_owned_gc_stress(self):
        for _ in range(25):
            view = fluentqt.SplitView()
            pane = QWidget()
            view.addOwnedPane(pane)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertFalse(Shiboken.isValid(pane))
            del pane

    def test_split_view_borrowed_gc_stress(self):
        for _ in range(25):
            view = fluentqt.SplitView()
            pane = QWidget()
            view.addBorrowedPane(pane)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertTrue(Shiboken.isValid(pane))
            self.assertIsNone(pane.parent())
            del pane
            gc.collect()

    def test_split_view_reparented_gc_stress(self):
        for _ in range(25):
            original_parent = QWidget()
            pane = QWidget(original_parent)
            view = fluentqt.SplitView()
            view.addReparentedPane(pane)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertTrue(Shiboken.isValid(pane))
            self.assertIs(pane.parent(), original_parent)
            del original_parent
            gc.collect()
            self.assertFalse(Shiboken.isValid(pane))
            del pane

    def test_list_view_public_surface_properties_and_selection(self):
        self.assertTrue(issubclass(fluentqt.ListView, QListView))
        self.assertIs(collections.ListView, fluentqt.ListView)
        self.assertIs(collections.SelectionMode, fluentqt.SelectionMode)
        self.assertIs(
            fluentqt.ListView.SelectionMode,
            fluentqt.SelectionMode,
        )
        self.assertTrue(hasattr(fluentqt.SelectionMode, "None_"))

        view = fluentqt.ListView(
            selectionMode=fluentqt.SelectionMode.Multiple,
            headerText="Python model",
            footerText="2 selected",
            placeholderText="No rows",
        )
        self.assertEqual(
            view.selectionMode(),
            fluentqt.SelectionMode.Multiple,
        )
        self.assertEqual(view.headerText(), "Python model")
        self.assertEqual(view.footerText(), "2 selected")
        self.assertEqual(view.placeholderText(), "No rows")

        model = QStringListModel(["Alpha", "Beta", "Gamma"])
        view.setModel(model)
        view.selectionModel().select(
            model.index(0, 0),
            QItemSelectionModel.Select | QItemSelectionModel.Rows,
        )
        view.selectionModel().select(
            model.index(2, 0),
            QItemSelectionModel.Select | QItemSelectionModel.Rows,
        )
        self.assertEqual(view.selectedRows(), [0, 2])

        changes = []
        selection_view_ref = weakref.ref(view)
        view.selectionModeChanged.connect(
            lambda: changes.append(
                selection_view_ref().selectionMode()
            )
        )
        view.setSelectionMode(fluentqt.SelectionMode.Single)
        self.assertEqual(changes, [fluentqt.SelectionMode.Single])
        view.setSelectedIndex(1)
        self.assertEqual(view.selectedIndex(), 1)
        self.assertEqual(view.selectedRows(), [1])

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
            self.assertFalse(hasattr(view, unsupported), unsupported)
        with self.assertRaisesRegex(TypeError, "header QWidget hosting"):
            fluentqt.ListView(header=QWidget())
        with self.assertRaisesRegex(TypeError, "section grouping"):
            fluentqt.ListView(sectionEnabled=True)

        for getter_name in (
            "verticalFluentScrollBar",
            "horizontalFluentScrollBar",
        ):
            self.assertFalse(
                hasattr(native.fluent.ListView, getter_name),
                getter_name,
            )
            self.assertTrue(hasattr(fluentqt.ListView, getter_name))
        vertical_bar = view.verticalFluentScrollBar()
        horizontal_bar = view.horizontalFluentScrollBar()
        self.assertTrue(Shiboken.isValid(vertical_bar))
        self.assertTrue(Shiboken.isValid(horizontal_bar))
        self.assertFalse(Shiboken.ownedByPython(vertical_bar))
        self.assertFalse(Shiboken.ownedByPython(horizontal_bar))

    def test_flow_view_public_surface_properties_geometry_and_selection(self):
        self.assertTrue(issubclass(fluentqt.FlowView, QAbstractItemView))
        self.assertIs(collections.FlowView, fluentqt.FlowView)
        self.assertIs(
            fluentqt.FlowView.SelectionMode,
            fluentqt.SelectionMode,
        )

        margins = QMargins(5, 7, 9, 11)
        view = fluentqt.FlowView(
            selectionMode=fluentqt.SelectionMode.Multiple,
            headerText="Python adaptive flow",
            placeholderText="No cards",
            defaultItemSize=QSize(104, 58),
            minimumItemSize=QSize(72, 40),
            maximumItemSize=QSize(180, 100),
            horizontalSpacing=8,
            verticalSpacing=10,
            contentMargins=margins,
            canReorderItems=True,
            scrollChainingEnabled=False,
            overscrollEnabled=False,
        )
        self.assertEqual(
            view.selectionMode(),
            fluentqt.SelectionMode.Multiple,
        )
        self.assertEqual(view.headerText(), "Python adaptive flow")
        self.assertEqual(view.placeholderText(), "No cards")
        self.assertEqual(view.defaultItemSize(), QSize(104, 58))
        self.assertEqual(view.minimumItemSize(), QSize(72, 40))
        self.assertEqual(view.maximumItemSize(), QSize(180, 100))
        self.assertEqual(view.horizontalSpacing(), 8)
        self.assertEqual(view.verticalSpacing(), 10)
        self.assertEqual(view.contentMargins(), margins)
        self.assertTrue(view.canReorderItems())
        self.assertFalse(view.isScrollChainingEnabled())
        self.assertFalse(view.isOverscrollEnabled())

        model = QStringListModel(["Alpha", "Beta", "Gamma"])
        view.setModel(model)
        view.resize(360, 220)
        view.setAttribute(Qt.WA_DontShowOnScreen, True)
        view.show()
        self.app.processEvents()

        first_rect = view.visualRect(model.index(0, 0))
        second_rect = view.visualRect(model.index(1, 0))
        self.assertTrue(first_rect.isValid())
        self.assertTrue(second_rect.isValid())
        self.assertEqual(view.indexAt(second_rect.center()), model.index(1, 0))

        view.selectionModel().select(
            model.index(0, 0),
            QItemSelectionModel.Select | QItemSelectionModel.Rows,
        )
        view.selectionModel().select(
            model.index(2, 0),
            QItemSelectionModel.Select | QItemSelectionModel.Rows,
        )
        self.assertEqual(view.selectedRows(), [0, 2])

        changes = []
        selection_view_ref = weakref.ref(view)
        view.selectionModeChanged.connect(
            lambda: changes.append(
                selection_view_ref().selectionMode()
            )
        )
        view.setSelectionMode(fluentqt.SelectionMode.Single)
        view.setSelectionMode(fluentqt.SelectionMode.Single)
        self.assertEqual(changes, [fluentqt.SelectionMode.Single])
        view.setSelectedIndex(1)
        self.assertEqual(view.selectedIndex(), 1)
        self.assertEqual(view.selectedRows(), [1])

        for method_name in (
            "selectionMode",
            "setSelectionMode",
            "verticalFluentScrollBar",
        ):
            self.assertNotIn(method_name, native.fluent.FlowView.__dict__)
            self.assertIn(method_name, fluentqt.FlowView.__dict__)
        vertical_bar = view.verticalFluentScrollBar()
        self.assertTrue(Shiboken.isValid(vertical_bar))
        self.assertFalse(Shiboken.ownedByPython(vertical_bar))

        view.close()
        del vertical_bar
        del view
        gc.collect()
        self.assertIsNone(selection_view_ref())

    def test_flow_view_python_model_updates_and_persistent_indexes(self):
        size_role = int(Qt.UserRole) + 17

        class PythonFlowModel(QAbstractListModel):
            def __init__(self, values):
                super().__init__()
                self.values = list(values)
                self.data_calls = 0

            def rowCount(self, parent=QModelIndex()):
                return 0 if parent.isValid() else len(self.values)

            def data(self, index, role=Qt.DisplayRole):
                self.data_calls += 1
                if not index.isValid():
                    return None
                if role == Qt.DisplayRole:
                    return self.values[index.row()]
                if role == size_role:
                    return QSize(96 + index.row() * 12, 48)
                return None

            def insertValue(self, row, value):
                self.beginInsertRows(QModelIndex(), row, row)
                self.values.insert(row, value)
                self.endInsertRows()

            def removeValue(self, row):
                self.beginRemoveRows(QModelIndex(), row, row)
                self.values.pop(row)
                self.endRemoveRows()

            def resetValues(self, values):
                self.beginResetModel()
                self.values = list(values)
                self.endResetModel()

        model = PythonFlowModel(["Alpha", "Beta", "Gamma"])
        view = fluentqt.FlowView()
        view.setItemSizeRole(size_role)
        view.setModel(model)
        view.resize(360, 220)
        view.setAttribute(Qt.WA_DontShowOnScreen, True)
        view.show()
        self.app.processEvents()

        selected = model.index(1, 0)
        persistent = QPersistentModelIndex(selected)
        view.setSelectedIndex(1)
        self.assertEqual(model.data(selected), "Beta")
        self.assertGreater(model.data_calls, 0)
        self.assertEqual(view.visualRect(selected).size(), QSize(108, 48))

        model.insertValue(0, "Zero")
        self.assertTrue(persistent.isValid())
        self.assertEqual(persistent.row(), 2)
        self.assertEqual(view.selectedRows(), [2])

        model.removeValue(2)
        self.assertFalse(persistent.isValid())

        view.setSelectedIndex(1)
        model.resetValues(["Reset A", "Reset B"])
        self.assertEqual(view.model().rowCount(), 2)
        self.assertEqual(view.selectedIndex(), -1)

    def test_flow_view_retains_and_releases_item_view_dependencies(self):
        view = fluentqt.FlowView()

        first_model = QStringListModel(["First"])
        first_model_ref = weakref.ref(first_model)
        view.setModel(first_model)
        del first_model
        gc.collect()
        self.assertIs(view.model(), first_model_ref())

        second_model = QStringListModel(["Second", "Third"])
        view.setModel(second_model)
        gc.collect()
        self.assertIsNone(first_model_ref())

        class PythonFlowDelegate(QStyledItemDelegate):
            def __init__(self):
                super().__init__()
                self.size_hint_calls = 0
                self.paint_calls = 0

            def sizeHint(self, option, index):
                self.size_hint_calls += 1
                return QSize(132, 76)

            def paint(self, painter, option, index):
                self.paint_calls += 1
                super().paint(painter, option, index)

        delegate = PythonFlowDelegate()
        delegate_ref = weakref.ref(delegate)
        view.setItemDelegate(delegate)
        del delegate
        gc.collect()
        self.assertIs(view.itemDelegate(), delegate_ref())
        view.resize(360, 220)
        view.setAttribute(Qt.WA_DontShowOnScreen, True)
        view.show()
        self.app.processEvents()
        self.assertEqual(
            view.visualRect(second_model.index(0, 0)).size(),
            QSize(132, 76),
        )
        view.grab()
        self.assertGreater(delegate_ref().size_hint_calls, 0)
        self.assertGreater(delegate_ref().paint_calls, 0)

        replacement_delegate = PythonFlowDelegate()
        replacement_delegate_ref = weakref.ref(replacement_delegate)
        view.setItemDelegate(replacement_delegate)
        del replacement_delegate
        gc.collect()
        self.assertIsNone(delegate_ref())
        self.assertIs(view.itemDelegate(), replacement_delegate_ref())

        view.setItemDelegate(None)
        gc.collect()
        self.assertIsNone(replacement_delegate_ref())

        hosted_delegate = PythonFlowDelegate()
        hosted_delegate_ref = weakref.ref(hosted_delegate)
        view.setItemDelegate(hosted_delegate)
        del hosted_delegate
        gc.collect()
        self.assertIs(view.itemDelegate(), hosted_delegate_ref())

        selection = QItemSelectionModel(second_model)
        selection_ref = weakref.ref(selection)
        view.setSelectionModel(selection)
        del selection
        gc.collect()
        self.assertIs(view.selectionModel(), selection_ref())

        second_model_ref = weakref.ref(second_model)
        del second_model
        view_ref = weakref.ref(view)
        del view
        self.app.processEvents()
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertIsNone(hosted_delegate_ref())
        self.assertIsNone(selection_ref())
        self.assertIsNone(second_model_ref())

    def test_flow_view_dependency_gc_stress(self):
        self._assert_item_view_gc_stress(
            fluentqt.FlowView,
            lambda: QStringListModel(["Flow"]),
        )

    def test_flow_view_tracks_external_delegate_destruction(self):
        view = fluentqt.FlowView()
        delegate = QStyledItemDelegate()
        view.setItemDelegate(delegate)

        delegate.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(delegate))
        self.assertIsNone(view.itemDelegate())

    def test_flow_view_recovers_from_missed_delegate_destroyed_callback(self):
        view = fluentqt.FlowView()
        delegate = QStyledItemDelegate()
        view.setItemDelegate(delegate)

        callback = view._fluentqt_item_delegate_destroyed
        delegate.destroyed.disconnect(callback)
        delegate.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()

        self.assertFalse(Shiboken.isValid(delegate))
        self.assertIsNone(view.itemDelegate())
        self.assertIsNone(view._fluentqt_item_delegate)
        self.assertIsNone(view._fluentqt_item_delegate_destroyed)

    def test_flow_view_tracks_external_model_destruction(self):
        view = fluentqt.FlowView()
        model = QStringListModel(["External"])
        view.setModel(model)

        model.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(model))
        self.assertIsNone(view.model())
        self.assertEqual(view.selectedIndex(), -1)

    def test_flow_view_keyboard_rtl_and_python_virtual_dispatch(self):
        class TrackingFlowView(fluentqt.FlowView):
            def __init__(self):
                super().__init__()
                self.current_changes = []

            def currentChanged(self, current, previous):
                self.current_changes.append(
                    (
                        current.row() if current.isValid() else -1,
                        previous.row() if previous.isValid() else -1,
                    )
                )
                super().currentChanged(current, previous)

        view = TrackingFlowView()
        model = QStringListModel(["Alpha", "Beta", "Gamma", "Delta"])
        view.setModel(model)
        view.setDefaultItemSize(QSize(112, 56))
        view.setAccessibleName("Python adaptive flow")
        view.setLayoutDirection(Qt.RightToLeft)
        view.resize(320, 220)
        view.setAttribute(Qt.WA_DontShowOnScreen, True)
        view.show()
        view.setFocus()
        self.app.processEvents()

        view.setSelectedIndex(0)
        QTest.keyClick(view, Qt.Key_Right)
        self.assertTrue(view.current_changes)
        self.assertGreaterEqual(view.selectedIndex(), 0)
        self.assertEqual(view.layoutDirection(), Qt.RightToLeft)
        self.assertEqual(view.accessibleName(), "Python adaptive flow")
        self.assertNotEqual(view.focusPolicy(), Qt.NoFocus)

    def test_grid_view_public_surface_properties_and_selection(self):
        self.assertTrue(issubclass(fluentqt.GridView, QListView))
        self.assertIs(collections.GridView, fluentqt.GridView)
        self.assertIs(
            fluentqt.GridView.SelectionMode,
            fluentqt.SelectionMode,
        )

        view = fluentqt.GridView(
            selectionMode=fluentqt.SelectionMode.Multiple,
            headerText="Python photo grid",
            placeholderText="No photos",
            cellSize=QSize(144, 96),
            horizontalSpacing=8,
            verticalSpacing=10,
            maxColumns=3,
        )
        self.assertEqual(
            view.selectionMode(),
            fluentqt.SelectionMode.Multiple,
        )
        self.assertEqual(view.headerText(), "Python photo grid")
        self.assertEqual(view.placeholderText(), "No photos")
        self.assertEqual(view.cellSize(), QSize(144, 96))
        self.assertEqual(view.horizontalSpacing(), 8)
        self.assertEqual(view.verticalSpacing(), 10)
        self.assertEqual(view.maxColumns(), 3)

        model = QStringListModel(["Alpha", "Beta", "Gamma"])
        view.setModel(model)
        view.selectionModel().select(
            model.index(0, 0),
            QItemSelectionModel.Select | QItemSelectionModel.Rows,
        )
        view.selectionModel().select(
            model.index(2, 0),
            QItemSelectionModel.Select | QItemSelectionModel.Rows,
        )
        self.assertEqual(view.selectedRows(), [0, 2])

        changes = []
        selection_view_ref = weakref.ref(view)
        view.selectionModeChanged.connect(
            lambda: changes.append(
                selection_view_ref().selectionMode()
            )
        )
        view.setSelectionMode(fluentqt.SelectionMode.Single)
        view.setSelectionMode(fluentqt.SelectionMode.Single)
        self.assertEqual(changes, [fluentqt.SelectionMode.Single])
        view.setSelectedIndex(1)
        self.assertEqual(view.selectedIndex(), 1)
        self.assertEqual(view.selectedRows(), [1])

        for method_name in (
            "selectionMode",
            "setSelectionMode",
            "verticalFluentScrollBar",
        ):
            self.assertNotIn(
                method_name,
                native.fluent.GridView.__dict__,
            )
            self.assertIn(method_name, fluentqt.GridView.__dict__)
        vertical_bar = view.verticalFluentScrollBar()
        self.assertTrue(Shiboken.isValid(vertical_bar))
        self.assertFalse(Shiboken.ownedByPython(vertical_bar))

    def test_grid_view_python_model_updates_and_persistent_indexes(self):
        class PythonGridModel(QAbstractListModel):
            def __init__(self, values):
                super().__init__()
                self.values = list(values)
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

            def removeValue(self, row):
                self.beginRemoveRows(QModelIndex(), row, row)
                self.values.pop(row)
                self.endRemoveRows()

            def resetValues(self, values):
                self.beginResetModel()
                self.values = list(values)
                self.endResetModel()

        model = PythonGridModel(["Alpha", "Beta", "Gamma"])
        view = fluentqt.GridView()
        view.setModel(model)

        selected = model.index(1, 0)
        persistent = QPersistentModelIndex(selected)
        view.setSelectedIndex(1)
        self.assertEqual(model.data(selected), "Beta")
        self.assertGreater(model.data_calls, 0)

        model.insertValue(0, "Zero")
        self.assertTrue(persistent.isValid())
        self.assertEqual(persistent.row(), 2)
        self.assertEqual(view.selectedRows(), [2])

        model.removeValue(2)
        self.assertFalse(persistent.isValid())
        self.assertEqual(view.selectedRows(), [2])

        view.setSelectedIndex(1)
        model.resetValues(["Reset A", "Reset B"])
        self.assertEqual(view.model().rowCount(), 2)
        self.assertEqual(view.selectedIndex(), -1)

    def test_grid_view_retains_and_releases_item_view_dependencies(self):
        view = fluentqt.GridView()

        first_model = QStringListModel(["First"])
        first_model_ref = weakref.ref(first_model)
        view.setModel(first_model)
        del first_model
        gc.collect()
        self.assertIs(view.model(), first_model_ref())

        second_model = QStringListModel(["Second", "Third"])
        view.setModel(second_model)
        gc.collect()
        self.assertIsNone(first_model_ref())

        class PythonGridDelegate(QStyledItemDelegate):
            def __init__(self):
                super().__init__()
                self.size_hint_calls = 0

            def sizeHint(self, option, index):
                self.size_hint_calls += 1
                return QSize(128, 84)

        delegate = PythonGridDelegate()
        delegate_ref = weakref.ref(delegate)
        view.setItemDelegate(delegate)
        del delegate
        gc.collect()
        self.assertIs(view.itemDelegate(), delegate_ref())
        self.assertEqual(view.sizeHintForIndex(second_model.index(0, 0)), QSize(128, 84))
        self.assertGreater(delegate_ref().size_hint_calls, 0)

        selection = QItemSelectionModel(second_model)
        selection_ref = weakref.ref(selection)
        view.setSelectionModel(selection)
        del selection
        gc.collect()
        self.assertIs(view.selectionModel(), selection_ref())

        second_model_ref = weakref.ref(second_model)
        del second_model
        view_ref = weakref.ref(view)
        del view
        self.app.processEvents()
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertIsNone(delegate_ref())
        self.assertIsNone(selection_ref())
        self.assertIsNone(second_model_ref())

    def test_grid_view_dependency_gc_stress(self):
        self._assert_item_view_gc_stress(
            fluentqt.GridView,
            lambda: QStringListModel(["Grid"]),
        )

    def test_grid_view_tracks_external_delegate_destruction(self):
        view = fluentqt.GridView()
        delegate = QStyledItemDelegate()
        view.setItemDelegate(delegate)

        delegate.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(delegate))
        self.assertIsNone(view.itemDelegate())

    def test_grid_view_recovers_from_missed_delegate_destroyed_callback(self):
        view = fluentqt.GridView()
        delegate = QStyledItemDelegate()
        view.setItemDelegate(delegate)

        callback = view._fluentqt_item_delegate_destroyed
        delegate.destroyed.disconnect(callback)
        delegate.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()

        self.assertFalse(Shiboken.isValid(delegate))
        self.assertIsNone(view.itemDelegate())
        self.assertIsNone(view._fluentqt_item_delegate)
        self.assertIsNone(view._fluentqt_item_delegate_destroyed)

    def test_grid_view_tracks_external_model_destruction(self):
        view = fluentqt.GridView()
        model = QStringListModel(["External"])
        view.setModel(model)

        # Keep the external-destruction contract on Qt's event-loop path.
        # Immediate wrapper deletion can abort inside PySide6 6.2 on Windows
        # before QAbstractItemView finishes its destroyed-signal bookkeeping.
        model.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(model))
        self.assertIsNone(view.model())
        self.assertEqual(view.selectedIndex(), -1)

    def test_grid_view_keyboard_rtl_and_python_virtual_dispatch(self):
        class TrackingGridView(fluentqt.GridView):
            def __init__(self):
                super().__init__()
                self.current_changes = []

            def currentChanged(self, current, previous):
                self.current_changes.append(
                    (
                        current.row() if current.isValid() else -1,
                        previous.row() if previous.isValid() else -1,
                    )
                )
                super().currentChanged(current, previous)

        view = TrackingGridView()
        model = QStringListModel(["Alpha", "Beta", "Gamma", "Delta"])
        view.setModel(model)
        view.setCellSize(QSize(120, 80))
        view.setMaxColumns(2)
        view.setAccessibleName("Python photo grid")
        view.setLayoutDirection(Qt.RightToLeft)
        view.resize(320, 220)
        view.setAttribute(Qt.WA_DontShowOnScreen, True)
        view.show()
        view.setFocus()
        self.app.processEvents()

        view.setSelectedIndex(0)
        QTest.keyClick(view, Qt.Key_Right)
        self.assertTrue(view.current_changes)
        self.assertGreaterEqual(view.selectedIndex(), 0)
        self.assertEqual(view.layoutDirection(), Qt.RightToLeft)
        self.assertEqual(view.accessibleName(), "Python photo grid")
        self.assertNotEqual(view.focusPolicy(), Qt.NoFocus)

    def test_tree_view_public_surface_properties_and_selection(self):
        self.assertTrue(issubclass(fluentqt.TreeView, QTreeView))
        self.assertIs(collections.TreeView, fluentqt.TreeView)
        self.assertIs(
            fluentqt.TreeView.SelectionMode,
            fluentqt.SelectionMode,
        )

        view = fluentqt.TreeView(
            selectionMode=fluentqt.SelectionMode.Extended,
            headerText="Python hierarchy",
            placeholderText="No nodes",
            borderVisible=False,
            backgroundVisible=False,
        )
        self.assertEqual(
            view.selectionMode(),
            fluentqt.SelectionMode.Extended,
        )
        self.assertEqual(view.headerText(), "Python hierarchy")
        self.assertEqual(view.placeholderText(), "No nodes")
        self.assertFalse(view.borderVisible())
        self.assertFalse(view.backgroundVisible())
        self.assertTrue(
            hasattr(
                fluentqt.TreeView.IndicatorVerticalDirection,
                "None_",
            )
        )
        self.assertTrue(
            hasattr(
                fluentqt.TreeView.IndicatorHierarchyTransition,
                "Inward",
            )
        )

        model = QStandardItemModel()
        parent = QStandardItem("Parent")
        parent.appendRow(QStandardItem("Child"))
        model.appendRow(parent)
        view.setModel(model)
        parent_index = model.index(0, 0)
        child_index = model.index(0, 0, parent_index)
        view.expandAll()
        self.assertTrue(view.isExpanded(parent_index))
        view.setSelectedItem(child_index)
        self.assertEqual(view.selectedItem(), child_index)
        self.assertEqual(view.selectedItems(), [child_index])
        view.toggleExpanded(parent_index)
        self.assertFalse(view.isExpanded(parent_index))
        view.setSelectionIndicatorInset(7.5)
        self.assertEqual(view.selectionIndicatorInset(), 7.5)

        for method_name in (
            "selectionMode",
            "setSelectionMode",
            "verticalFluentScrollBar",
            "horizontalFluentScrollBar",
        ):
            self.assertNotIn(method_name, native.fluent.TreeView.__dict__)
            self.assertIn(method_name, fluentqt.TreeView.__dict__)
        for internal_style_method in (
            "selectionIndicatorStyle",
            "setSelectionIndicatorStyle",
        ):
            self.assertFalse(hasattr(view, internal_style_method))

        scroll_bars = (
            view.verticalFluentScrollBar(),
            view.horizontalFluentScrollBar(),
        )
        self.assertTrue(all(Shiboken.isValid(bar) for bar in scroll_bars))
        self.assertTrue(
            all(not Shiboken.ownedByPython(bar) for bar in scroll_bars)
        )

    def test_tree_view_hierarchical_updates_and_persistent_indexes(self):
        class PythonTreeModel(QStandardItemModel):
            def __init__(self):
                super().__init__()
                self.data_calls = 0

            def data(self, index, role=Qt.DisplayRole):
                self.data_calls += 1
                return super().data(index, role)

        model = PythonTreeModel()
        parent = QStandardItem("Workspace")
        first_child = QStandardItem("Design")
        parent.appendRow(first_child)
        model.appendRow(parent)

        view = fluentqt.TreeView()
        view.setIndicatorMotionAnimationEnabled(False)
        view.setModel(model)
        view.resize(360, 240)
        view.setAttribute(Qt.WA_DontShowOnScreen, True)
        view.show()
        view.expandAll()
        self.app.processEvents()
        view.grab()
        self.assertGreater(model.data_calls, 0)

        parent_index = model.index(0, 0)
        child_index = model.index(0, 0, parent_index)
        persistent = QPersistentModelIndex(child_index)
        view.setSelectedItem(child_index)
        self.assertEqual(view.selectedItem(), child_index)

        parent.insertRow(0, QStandardItem("Brief"))
        self.app.processEvents()
        self.assertTrue(persistent.isValid())
        self.assertEqual(persistent.row(), 1)
        self.assertEqual(view.selectedItem(), QModelIndex(persistent))

        parent.removeRow(1)
        self.app.processEvents()
        self.assertFalse(persistent.isValid())
        self.assertNotEqual(view.selectedItem().data(), "Design")

        replacement = QStandardItem("Replacement")
        replacement.appendRow(QStandardItem("Nested"))
        model.appendRow(replacement)
        view.expandAll()
        view.setSelectedItem(model.index(0, 0, model.index(1, 0)))
        self.assertTrue(view.indicatorMotionCurrentIndex().isValid())
        model.clear()
        self.app.processEvents()
        self.assertFalse(view.selectedItem().isValid())
        self.assertFalse(view.indicatorMotionCurrentIndex().isValid())

    def test_tree_view_retains_and_releases_item_view_dependencies(self):
        view = fluentqt.TreeView()

        first_model = QStandardItemModel()
        first_model.appendRow(QStandardItem("First"))
        first_model_ref = weakref.ref(first_model)
        view.setModel(first_model)
        del first_model
        gc.collect()
        self.assertIs(view.model(), first_model_ref())

        second_model = QStandardItemModel()
        second_model.appendRow(QStandardItem("Second"))
        view.setModel(second_model)
        gc.collect()
        self.assertIsNone(first_model_ref())

        class PythonTreeDelegate(QStyledItemDelegate):
            def __init__(self):
                super().__init__()
                self.size_hint_calls = 0

            def sizeHint(self, option, index):
                self.size_hint_calls += 1
                return QSize(220, 38)

        delegate = PythonTreeDelegate()
        delegate_ref = weakref.ref(delegate)
        view.setItemDelegate(delegate)
        del delegate
        gc.collect()
        self.assertIs(view.itemDelegate(), delegate_ref())
        self.assertEqual(
            view.sizeHintForIndex(second_model.index(0, 0)),
            QSize(220, 38),
        )
        self.assertGreater(delegate_ref().size_hint_calls, 0)

        selection = QItemSelectionModel(second_model)
        selection_ref = weakref.ref(selection)
        view.setSelectionModel(selection)
        del selection
        gc.collect()
        self.assertIs(view.selectionModel(), selection_ref())

        second_model_ref = weakref.ref(second_model)
        del second_model
        view_ref = weakref.ref(view)
        del view
        self.app.processEvents()
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertIsNone(delegate_ref())
        self.assertIsNone(selection_ref())
        self.assertIsNone(second_model_ref())

    def test_tree_view_dependency_gc_stress(self):
        def tree_model():
            model = QStandardItemModel()
            model.appendRow(QStandardItem("Tree"))
            return model

        self._assert_item_view_gc_stress(
            fluentqt.TreeView,
            tree_model,
        )

    def test_tree_view_tracks_external_delegate_destruction(self):
        view = fluentqt.TreeView()
        delegate = QStyledItemDelegate()
        view.setItemDelegate(delegate)

        delegate.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(delegate))
        self.assertIsNone(view.itemDelegate())

    def test_tree_view_recovers_from_missed_delegate_destroyed_callback(self):
        view = fluentqt.TreeView()
        delegate = QStyledItemDelegate()
        view.setItemDelegate(delegate)

        callback = view._fluentqt_item_delegate_destroyed
        delegate.destroyed.disconnect(callback)
        delegate.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()

        self.assertFalse(Shiboken.isValid(delegate))
        self.assertIsNone(view.itemDelegate())
        self.assertIsNone(view._fluentqt_item_delegate)
        self.assertIsNone(view._fluentqt_item_delegate_destroyed)

    def test_tree_view_tracks_external_model_destruction(self):
        view = fluentqt.TreeView()
        model = QStandardItemModel()
        model.appendRow(QStandardItem("External"))
        view.setModel(model)

        # Use Qt's supported external-destruction path. Forcing immediate
        # wrapper deletion while QTreeView is still connected to a
        # QStandardItemModel fast-fails in PySide6 6.2 on Windows.
        model.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(model))
        self.assertIsNone(view.model())
        self.assertFalse(view.selectedItem().isValid())

    def test_tree_view_check_state_selection_and_python_virtual_dispatch(self):
        class TrackingTreeView(fluentqt.TreeView):
            def __init__(self):
                super().__init__()
                self.current_changes = []

            def currentChanged(self, current, previous):
                self.current_changes.append(
                    (
                        current.row() if current.isValid() else -1,
                        previous.row() if previous.isValid() else -1,
                    )
                )
                super().currentChanged(current, previous)

        def checkable(text, state):
            item = QStandardItem(text)
            item.setCheckable(True)
            item.setCheckState(state)
            return item

        model = QStandardItemModel()
        parent = checkable("Parent", Qt.PartiallyChecked)
        first_child = checkable("First", Qt.Checked)
        second_child = checkable("Second", Qt.Unchecked)
        parent.appendRow(first_child)
        parent.appendRow(second_child)
        model.appendRow(parent)

        view = TrackingTreeView()
        view.setSelectionMode(fluentqt.SelectionMode.Multiple)
        view.setIndicatorMotionAnimationEnabled(False)
        view.setModel(model)
        view.setAccessibleName("Python hierarchy")
        view.setLayoutDirection(Qt.RightToLeft)
        view.resize(360, 240)
        view.setAttribute(Qt.WA_DontShowOnScreen, True)
        view.show()
        view.expandAll()
        view.setFocus()
        self.app.processEvents()

        parent_index = model.index(0, 0)
        first_index = model.index(0, 0, parent_index)
        second_index = model.index(1, 0, parent_index)
        view.selectionModel().select(
            parent_index,
            QItemSelectionModel.Select | QItemSelectionModel.Rows,
        )
        self.app.processEvents()
        self.assertEqual(parent.checkState(), Qt.Checked)
        self.assertEqual(first_child.checkState(), Qt.Checked)
        self.assertEqual(second_child.checkState(), Qt.Checked)
        self.assertTrue(view.selectionModel().isSelected(first_index))
        self.assertTrue(view.selectionModel().isSelected(second_index))

        view.selectionModel().select(
            first_index,
            QItemSelectionModel.Deselect | QItemSelectionModel.Rows,
        )
        self.app.processEvents()
        self.assertEqual(parent.checkState(), Qt.PartiallyChecked)
        self.assertEqual(first_child.checkState(), Qt.Unchecked)
        self.assertEqual(second_child.checkState(), Qt.Checked)

        view.setSelectedItem(second_index)
        QTest.keyClick(view, Qt.Key_Up)
        self.assertTrue(view.current_changes)
        self.assertEqual(view.layoutDirection(), Qt.RightToLeft)
        self.assertEqual(view.accessibleName(), "Python hierarchy")
        self.assertNotEqual(view.focusPolicy(), Qt.NoFocus)

    def test_list_view_python_model_updates_and_persistent_indexes(self):
        class PythonListModel(QAbstractListModel):
            def __init__(self, values):
                super().__init__()
                self.values = list(values)
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

            def removeValue(self, row):
                self.beginRemoveRows(QModelIndex(), row, row)
                self.values.pop(row)
                self.endRemoveRows()

            def resetValues(self, values):
                self.beginResetModel()
                self.values = list(values)
                self.endResetModel()

        model = PythonListModel(["Alpha", "Beta", "Gamma"])
        view = fluentqt.ListView()
        view.setSelectedIndicatorAnimationEnabled(False)
        view.setModel(model)

        selected = model.index(1, 0)
        persistent = QPersistentModelIndex(selected)
        view.setSelectedIndex(1)
        self.assertEqual(model.data(selected), "Beta")
        self.assertGreater(model.data_calls, 0)

        model.insertValue(0, "Zero")
        self.assertTrue(persistent.isValid())
        self.assertEqual(persistent.row(), 2)
        self.assertEqual(view.selectedRows(), [2])

        model.removeValue(2)
        self.assertFalse(persistent.isValid())
        # QItemSelectionModel keeps the row position selected when the
        # selected row is removed and a successor occupies that position.
        self.assertEqual(view.selectedRows(), [2])
        self.assertEqual(model.data(model.index(2, 0)), "Gamma")

        view.setSelectedIndex(1)
        model.resetValues(["Reset A", "Reset B"])
        self.assertEqual(view.model().rowCount(), 2)
        self.assertEqual(view.selectedIndex(), -1)

    def test_list_view_retains_and_releases_model_selection_and_delegate(self):
        view = fluentqt.ListView()

        first_model = QStringListModel(["First"])
        first_model_ref = weakref.ref(first_model)
        view.setModel(first_model)
        del first_model
        gc.collect()
        self.assertIs(view.model(), first_model_ref())

        second_model = QStringListModel(["Second", "Third"])
        view.setModel(second_model)
        gc.collect()
        self.assertIsNone(first_model_ref())

        class PythonDelegate(QStyledItemDelegate):
            def __init__(self):
                super().__init__()
                self.size_hint_calls = 0

            def sizeHint(self, option, index):
                self.size_hint_calls += 1
                return QSize(180, 38)

        delegate = PythonDelegate()
        delegate_ref = weakref.ref(delegate)
        view.setItemDelegate(delegate)
        del delegate
        gc.collect()
        self.assertIs(view.itemDelegate(), delegate_ref())
        self.assertEqual(view.sizeHintForRow(0), 38)
        self.assertGreater(delegate_ref().size_hint_calls, 0)

        replacement_delegate = PythonDelegate()
        replacement_delegate_ref = weakref.ref(replacement_delegate)
        view.setItemDelegate(replacement_delegate)
        del replacement_delegate
        gc.collect()
        self.assertIsNone(delegate_ref())
        self.assertIs(
            view.itemDelegate(),
            replacement_delegate_ref(),
        )

        view.setItemDelegate(None)
        gc.collect()
        self.assertIsNone(replacement_delegate_ref())

        hosted_delegate = PythonDelegate()
        hosted_delegate_ref = weakref.ref(hosted_delegate)
        view.setItemDelegate(hosted_delegate)
        del hosted_delegate
        gc.collect()
        self.assertIs(view.itemDelegate(), hosted_delegate_ref())

        selection = QItemSelectionModel(second_model)
        selection_ref = weakref.ref(selection)
        view.setSelectionModel(selection)
        del selection
        gc.collect()
        self.assertIs(view.selectionModel(), selection_ref())

        replacement = QItemSelectionModel(second_model)
        view.setSelectionModel(replacement)
        gc.collect()
        self.assertIsNone(selection_ref())

        replacement_ref = weakref.ref(replacement)
        second_model_ref = weakref.ref(second_model)
        del replacement
        del second_model
        gc.collect()
        self.assertIs(view.selectionModel(), replacement_ref())
        self.assertIs(view.model(), second_model_ref())

        view_ref = weakref.ref(view)
        del view
        self.app.processEvents()
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertIsNone(hosted_delegate_ref())
        self.assertIsNone(replacement_ref())
        self.assertIsNone(second_model_ref())

    def test_list_view_dependency_gc_stress(self):
        self._assert_item_view_gc_stress(
            fluentqt.ListView,
            lambda: QStringListModel(["List"]),
        )

    def test_list_view_tracks_external_delegate_destruction(self):
        view = fluentqt.ListView()
        delegate = QStyledItemDelegate()
        view.setItemDelegate(delegate)

        delegate.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()

        self.assertFalse(Shiboken.isValid(delegate))
        self.assertIsNone(view.itemDelegate())

    def test_list_view_recovers_from_missed_delegate_destroyed_callback(self):
        view = fluentqt.ListView()
        delegate = QStyledItemDelegate()
        view.setItemDelegate(delegate)

        callback = view._fluentqt_item_delegate_destroyed
        delegate.destroyed.disconnect(callback)
        delegate.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()

        self.assertFalse(Shiboken.isValid(delegate))
        self.assertIsNone(view.itemDelegate())
        self.assertIsNone(view._fluentqt_item_delegate)
        self.assertIsNone(view._fluentqt_item_delegate_destroyed)

    def test_list_view_tracks_external_model_destruction(self):
        view = fluentqt.ListView()
        model = QStringListModel(["External"])
        view.setModel(model)
        self.assertIs(view.model(), model)

        # Follow Qt's normal deferred-destruction path. Immediate wrapper
        # deletion while QAbstractItemView still owns signal connections can
        # fast-fail in PySide6 6.2 on Windows before Qt finishes disconnecting.
        model.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(model))
        self.assertIsNone(view.model())
        self.assertEqual(view.selectedIndex(), -1)

    def test_list_view_keyboard_focus_rtl_and_accessible_name(self):
        view = fluentqt.ListView()
        model = QStringListModel(["Alpha", "Beta", "Gamma"])
        view.setModel(model)
        view.setSelectedIndicatorAnimationEnabled(False)
        view.setAccessibleName("Python task list")
        view.setLayoutDirection(Qt.RightToLeft)
        view.resize(320, 180)
        view.setAttribute(Qt.WA_DontShowOnScreen, True)
        view.show()
        view.setFocus()
        self.app.processEvents()

        view.setSelectedIndex(0)
        QTest.keyClick(view, Qt.Key_Down)
        self.assertEqual(view.selectedIndex(), 1)
        QTest.keyClick(view, Qt.Key_End)
        self.assertEqual(view.selectedIndex(), 2)
        self.assertEqual(view.layoutDirection(), Qt.RightToLeft)
        self.assertEqual(view.accessibleName(), "Python task list")
        self.assertNotEqual(view.focusPolicy(), Qt.NoFocus)

    def test_list_view_python_virtual_current_changed_calls_super(self):
        class TrackingListView(fluentqt.ListView):
            def __init__(self):
                super().__init__()
                self.current_changes = []

            def currentChanged(self, current, previous):
                self.current_changes.append(
                    (
                        current.row() if current.isValid() else -1,
                        previous.row() if previous.isValid() else -1,
                    )
                )
                super().currentChanged(current, previous)

        view = TrackingListView()
        model = QStringListModel(["Alpha", "Beta", "Gamma"])
        view.setModel(model)
        view.setSelectedIndex(2)

        self.assertTrue(view.current_changes)
        self.assertEqual(view.current_changes[-1][0], 2)
        self.assertEqual(view.selectedRows(), [2])

    def test_breadcrumb_item_value_type_and_variant_data(self):
        empty = fluentqt.BreadcrumbItem()
        self.assertEqual(empty.text, "")
        self.assertIsNone(empty.data)
        self.assertTrue(empty.enabled)
        self.assertEqual(empty.accessibleName, "")

        payload = {"folder": 7, "segments": ["python", "qt"]}
        rich = fluentqt.BreadcrumbItem(
            "Projects",
            payload,
            False,
            "Projects folder",
        )
        copied = fluentqt.BreadcrumbItem(rich)
        self.assertEqual(copied, rich)
        self.assertFalse(copied != rich)
        self.assertEqual(copied.data, payload)
        with self.assertRaises(TypeError):
            hash(rich)

        copied.text = "Copy"
        copied.data = [1, "two", {"three": 3}]
        copied.enabled = True
        self.assertNotEqual(copied, rich)
        self.assertEqual(rich.text, "Projects")
        self.assertEqual(rich.data, payload)
        self.assertFalse(rich.enabled)

    def test_breadcrumb_items_properties_and_signals(self):
        breadcrumb = fluentqt.Breadcrumb()
        self.assertEqual(breadcrumb.itemCount(), 0)
        self.assertEqual(
            breadcrumb.breadcrumbSize(),
            fluentqt.Breadcrumb.BreadcrumbSize.Standard,
        )
        self.assertEqual(
            breadcrumb.overflowMode(),
            fluentqt.Breadcrumb.OverflowMode.None_,
        )
        self.assertFalse(breadcrumb.autoTruncateOnItemClick())
        self.assertEqual(
            breadcrumb.standardFontRole(),
            fluentqt.FontRole.Body,
        )
        self.assertEqual(
            breadcrumb.largeFontRole(),
            fluentqt.FontRole.Title,
        )

        item_changes = []
        size_changes = []
        overflow_changes = []
        truncate_changes = []
        standard_font_changes = []
        large_font_changes = []
        breadcrumb.itemsChanged.connect(lambda: item_changes.append(True))
        breadcrumb.breadcrumbSizeChanged.connect(size_changes.append)
        breadcrumb.overflowModeChanged.connect(overflow_changes.append)
        breadcrumb.autoTruncateOnItemClickChanged.connect(
            truncate_changes.append
        )
        breadcrumb.standardFontRoleChanged.connect(
            standard_font_changes.append
        )
        breadcrumb.largeFontRoleChanged.connect(large_font_changes.append)

        metadata = fluentqt.BreadcrumbItem(
            "FluentQt",
            {"route": "fluentqt"},
            True,
            "FluentQt project",
        )
        breadcrumb.setItems(
            [
                fluentqt.BreadcrumbItem(
                    "Home",
                    {"route": "home"},
                ),
                fluentqt.BreadcrumbItem(
                    "Projects",
                    {"route": "projects"},
                ),
            ]
        )
        self.assertEqual(
            [item.text for item in breadcrumb.items()],
            ["Home", "Projects"],
        )
        self.assertEqual(
            breadcrumb.itemAt(1).data,
            {"route": "projects"},
        )
        breadcrumb.setItems(["Home", "Projects"])
        with self.assertRaises(TypeError):
            breadcrumb.setItems("Home")
        with self.assertRaises(TypeError):
            breadcrumb.setItems(
                ["Home", fluentqt.BreadcrumbItem("Mixed")]
            )
        breadcrumb.appendItem(metadata)
        breadcrumb.insertItem(1, fluentqt.BreadcrumbItem("Workspace"))
        self.assertEqual(
            [item.text for item in breadcrumb.items()],
            ["Home", "Workspace", "Projects", "FluentQt"],
        )
        self.assertEqual(breadcrumb.itemAt(3), metadata)
        self.assertEqual(
            breadcrumb.itemAt(3).data,
            {"route": "fluentqt"},
        )
        self.assertEqual(breadcrumb.itemAt(99), fluentqt.BreadcrumbItem())
        self.assertTrue(breadcrumb.removeItemAt(1))
        self.assertFalse(breadcrumb.removeItemAt(99))

        breadcrumb.setBreadcrumbSize(
            fluentqt.Breadcrumb.BreadcrumbSize.Large
        )
        breadcrumb.setBreadcrumbSize(
            fluentqt.Breadcrumb.BreadcrumbSize.Large
        )
        breadcrumb.setOverflowMode(
            fluentqt.Breadcrumb.OverflowMode.Middle
        )
        breadcrumb.setOverflowMode(
            fluentqt.Breadcrumb.OverflowMode.Middle
        )
        breadcrumb.setAutoTruncateOnItemClick(True)
        breadcrumb.setAutoTruncateOnItemClick(True)
        breadcrumb.setStandardFontRole(fluentqt.FontRole.BodyStrong)
        breadcrumb.setStandardFontRole(fluentqt.FontRole.BodyStrong)
        breadcrumb.setLargeFontRole(fluentqt.FontRole.TitleLarge)
        breadcrumb.setLargeFontRole(fluentqt.FontRole.TitleLarge)

        self.assertEqual(len(item_changes), 5)
        self.assertEqual(
            size_changes,
            [fluentqt.Breadcrumb.BreadcrumbSize.Large],
        )
        self.assertEqual(
            overflow_changes,
            [fluentqt.Breadcrumb.OverflowMode.Middle],
        )
        self.assertEqual(truncate_changes, [True])
        self.assertEqual(standard_font_changes, [fluentqt.FontRole.BodyStrong])
        self.assertEqual(large_font_changes, [fluentqt.FontRole.TitleLarge])

    def test_breadcrumb_activation_overflow_and_python_virtual(self):
        class TrackingBreadcrumb(fluentqt.Breadcrumb):
            def __init__(self):
                super().__init__()
                self.keys = []

            def keyPressEvent(self, event):
                self.keys.append(event.key())
                super().keyPressEvent(event)

        breadcrumb = TrackingBreadcrumb()
        breadcrumb.setItems(
            ["Home", "Projects", "FluentQt", "Python", "Breadcrumb"]
        )
        breadcrumb.setOverflowMode(
            fluentqt.Breadcrumb.OverflowMode.Middle
        )
        breadcrumb.resize(150, 20)
        breadcrumb.setAttribute(Qt.WA_DontShowOnScreen, True)
        breadcrumb.show()
        breadcrumb.setFocus()
        self.app.processEvents()

        self.assertTrue(breadcrumb.hiddenItemIndexes())
        self.assertFalse(breadcrumb.overflowGeometry().isEmpty())
        self.assertGreater(breadcrumb.visibleItemCount(), 0)

        activated = []
        breadcrumb.itemActivated.connect(
            lambda index, item: activated.append((index, item))
        )
        first_geometry = breadcrumb.itemGeometry(0)
        self.assertFalse(first_geometry.isEmpty())
        QTest.mouseClick(
            breadcrumb,
            Qt.LeftButton,
            Qt.NoModifier,
            first_geometry.center(),
        )
        self.assertEqual(activated[0][0], 0)
        self.assertEqual(activated[0][1].text, "Home")

        QTest.keyClick(breadcrumb, Qt.Key_End)
        self.assertIn(Qt.Key_End, breadcrumb.keys)
        self.assertEqual(
            breadcrumb.accessibleName(),
            "Home > Projects > FluentQt > Python > Breadcrumb",
        )

    def test_pivot_and_selector_bar_value_types(self):
        empty_pivot = fluentqt.PivotItem()
        self.assertEqual(empty_pivot.header, "")
        self.assertEqual(empty_pivot.iconGlyph, "")
        self.assertTrue(empty_pivot.enabled)
        self.assertIsNone(empty_pivot.data)
        self.assertEqual(empty_pivot.accessibleName, "")

        pivot_payload = {"section": 7, "filters": ["unread", "urgent"]}
        pivot_item = fluentqt.PivotItem(
            "Unread",
            "mail-glyph",
            False,
            pivot_payload,
            "Unread messages",
        )
        self.assertEqual(fluentqt.PivotItem(pivot_item), pivot_item)
        self.assertEqual(pivot_item.data, pivot_payload)
        with self.assertRaises(TypeError):
            hash(pivot_item)

        pivot_copy = fluentqt.PivotItem(pivot_item)
        pivot_copy.header = "Copy"
        pivot_copy.data = [1, "two"]
        self.assertNotEqual(pivot_copy, pivot_item)
        self.assertEqual(pivot_item.header, "Unread")

        empty_selector = fluentqt.SelectorBarItem()
        self.assertEqual(empty_selector.text, "")
        self.assertEqual(empty_selector.iconGlyph, "")
        self.assertTrue(empty_selector.enabled)
        self.assertTrue(empty_selector.visible)
        self.assertFalse(empty_selector.selected)
        self.assertIsNone(empty_selector.data)
        self.assertEqual(empty_selector.accessibleName, "")

        selector_payload = {"route": "activity", "badge": 3}
        selector_item = fluentqt.SelectorBarItem(
            "Activity",
            "activity-glyph",
            True,
            False,
            selector_payload,
            "Activity timeline",
        )
        self.assertEqual(
            fluentqt.SelectorBarItem(selector_item),
            selector_item,
        )
        self.assertEqual(selector_item.data, selector_payload)
        with self.assertRaises(TypeError):
            hash(selector_item)

        selector_copy = fluentqt.SelectorBarItem(selector_item)
        selector_copy.visible = True
        selector_copy.selected = True
        self.assertNotEqual(selector_copy, selector_item)
        self.assertFalse(selector_item.visible)
        self.assertFalse(selector_item.selected)

    def test_pivot_items_properties_and_signals(self):
        pivot = fluentqt.Pivot()
        self.assertEqual(pivot.itemCount(), 0)
        self.assertEqual(pivot.selectedIndex(), -1)
        self.assertEqual(
            pivot.overflowBehavior(),
            fluentqt.Pivot.OverflowBehavior.ScrollButtons,
        )
        self.assertEqual(pivot.itemFontRole(), fluentqt.FontRole.Body)

        self.assertEqual(pivot.addItem("All"), 0)
        flagged = fluentqt.PivotItem(
            "Flagged",
            "flag-glyph",
            False,
            {"filter": "flagged"},
            "Flagged messages",
        )
        self.assertEqual(pivot.addItem(flagged), 1)
        self.assertTrue(
            pivot.insertItem(1, fluentqt.PivotItem("Unread"))
        )
        self.assertEqual(
            [item.header for item in pivot.items()],
            ["All", "Unread", "Flagged"],
        )
        self.assertEqual(pivot.itemAt(2), flagged)
        self.assertEqual(pivot.itemAt(99), fluentqt.PivotItem())

        self.assertTrue(pivot.setItemHeader(1, "Unread mail"))
        self.assertTrue(pivot.setItemIconGlyph(1, "mail-glyph"))
        self.assertTrue(pivot.setItemData(1, {"filter": "unread"}))
        self.assertTrue(
            pivot.setItemAccessibleName(1, "Unread messages")
        )
        self.assertTrue(pivot.setItemEnabled(2, True))
        self.assertFalse(pivot.setItemHeader(1, "Unread mail"))
        self.assertFalse(pivot.setItemEnabled(99, True))

        selected_changes = []
        current_changes = []
        overflow_changes = []
        font_changes = []
        icon_changes = []
        pivot.selectedIndexChanged.connect(selected_changes.append)
        pivot.currentChanged.connect(current_changes.append)
        pivot.overflowBehaviorChanged.connect(overflow_changes.append)
        pivot.itemFontRoleChanged.connect(font_changes.append)
        pivot.iconFontFamilyChanged.connect(icon_changes.append)

        pivot.setSelectedIndex(1)
        pivot.setSelectedIndex(1)
        pivot.setOverflowBehavior(
            fluentqt.Pivot.OverflowBehavior.MoreButton
        )
        pivot.setOverflowBehavior(
            fluentqt.Pivot.OverflowBehavior.MoreButton
        )
        pivot.setItemFontRole(fluentqt.FontRole.BodyStrong)
        pivot.setItemFontRole(fluentqt.FontRole.BodyStrong)
        pivot.setIconFontFamily("Python icon font")
        pivot.setIconFontFamily("Python icon font")

        self.assertEqual(selected_changes, [1])
        self.assertEqual(current_changes, [1])
        self.assertEqual(
            overflow_changes,
            [fluentqt.Pivot.OverflowBehavior.MoreButton],
        )
        self.assertEqual(font_changes, [fluentqt.FontRole.BodyStrong])
        self.assertEqual(icon_changes, ["Python icon font"])
        self.assertEqual(pivot.itemAt(1).data, {"filter": "unread"})
        pivot.clearSelection()
        self.assertEqual(pivot.selectedIndex(), -1)
        self.assertTrue(pivot.removeItem(1))
        self.assertFalse(pivot.removeItem(99))

    def test_selector_bar_items_properties_and_signals(self):
        selector = fluentqt.SelectorBar()
        self.assertEqual(selector.itemCount(), 0)
        self.assertEqual(selector.selectedIndex(), -1)
        self.assertEqual(
            selector.overflowBehavior(),
            fluentqt.SelectorBar.OverflowBehavior.ScrollButtons,
        )
        self.assertEqual(selector.itemFontRole(), fluentqt.FontRole.Body)

        self.assertEqual(selector.addItem("Overview"), 0)
        activity = fluentqt.SelectorBarItem(
            "Activity",
            "activity-glyph",
            True,
            True,
            {"route": "activity"},
            "Activity timeline",
        )
        self.assertEqual(selector.addItem(activity), 1)
        self.assertTrue(
            selector.insertItem(
                2,
                fluentqt.SelectorBarItem(
                    "Hidden",
                    "hidden-glyph",
                    True,
                    False,
                ),
            )
        )
        self.assertEqual(
            [item.text for item in selector.items()],
            ["Overview", "Activity", "Hidden"],
        )
        self.assertEqual(selector.itemAt(1).data, {"route": "activity"})
        self.assertEqual(
            selector.itemAt(99),
            fluentqt.SelectorBarItem(),
        )

        self.assertTrue(selector.setItemText(1, "Recent activity"))
        self.assertTrue(selector.setItemIconGlyph(1, "recent-glyph"))
        self.assertTrue(selector.setItemData(1, {"route": "recent"}))
        self.assertTrue(
            selector.setItemAccessibleName(1, "Recent activity")
        )
        self.assertTrue(selector.setItemVisible(2, True))
        self.assertFalse(selector.setItemText(1, "Recent activity"))
        self.assertFalse(selector.setItemVisible(99, True))

        selected_changes = []
        current_changes = []
        selection_changes = []
        overflow_changes = []
        selector.selectedIndexChanged.connect(selected_changes.append)
        selector.currentChanged.connect(current_changes.append)
        selector.selectionChanged.connect(
            lambda index, item: selection_changes.append((index, item))
        )
        selector.overflowBehaviorChanged.connect(overflow_changes.append)

        self.assertTrue(selector.setItemSelected(1, True))
        self.assertFalse(selector.setItemSelected(1, True))
        selector.setOverflowBehavior(
            fluentqt.SelectorBar.OverflowBehavior.MoreButton
        )
        selector.setOverflowBehavior(
            fluentqt.SelectorBar.OverflowBehavior.MoreButton
        )

        self.assertEqual(selected_changes, [1])
        self.assertEqual(current_changes, [1])
        self.assertEqual(len(selection_changes), 1)
        self.assertEqual(selection_changes[0][0], 1)
        self.assertEqual(selection_changes[0][1].text, "Recent activity")
        self.assertEqual(selector.selectedItem(), selector.itemAt(1))
        self.assertTrue(selector.selectedItem().selected)
        self.assertEqual(
            overflow_changes,
            [fluentqt.SelectorBar.OverflowBehavior.MoreButton],
        )
        selector.clearSelection()
        self.assertEqual(selector.selectedIndex(), -1)
        self.assertTrue(selector.removeItem(2))
        self.assertFalse(selector.removeItem(99))

    def test_pivot_and_selector_bar_overflow_and_python_virtuals(self):
        class TrackingPivot(fluentqt.Pivot):
            def __init__(self):
                super().__init__()
                self.keys = []

            def keyPressEvent(self, event):
                self.keys.append(event.key())
                super().keyPressEvent(event)

        class TrackingSelectorBar(fluentqt.SelectorBar):
            def __init__(self):
                super().__init__()
                self.keys = []

            def keyPressEvent(self, event):
                self.keys.append(event.key())
                super().keyPressEvent(event)

        pivot = TrackingPivot()
        selector = TrackingSelectorBar()
        for index in range(8):
            pivot.addItem(
                fluentqt.PivotItem(
                    "Mailbox section {0}".format(index),
                    "",
                    True,
                    {"section": index},
                    "Mailbox section {0}".format(index),
                )
            )
            selector.addItem(
                fluentqt.SelectorBarItem(
                    "Workspace {0}".format(index),
                    "",
                    True,
                    True,
                    {"workspace": index},
                    "Workspace {0}".format(index),
                )
            )

        for widget in (pivot, selector):
            widget.resize(230, 44)
            widget.setAttribute(Qt.WA_DontShowOnScreen, True)
            widget.show()
            widget.setFocus()
        self.app.processEvents()

        self.assertTrue(pivot.hiddenItemIndexes())
        self.assertFalse(pivot.overflowForwardGeometry().isEmpty())
        self.assertTrue(selector.hiddenItemIndexes())
        self.assertFalse(selector.overflowForwardGeometry().isEmpty())

        pivot_activated = []
        selector_activated = []
        pivot.itemActivated.connect(
            lambda index, item: pivot_activated.append((index, item))
        )
        selector.itemActivated.connect(
            lambda index, item: selector_activated.append((index, item))
        )
        pivot.clearSelection()
        selector.clearSelection()
        QTest.mouseClick(
            pivot,
            Qt.LeftButton,
            Qt.NoModifier,
            pivot.itemHeaderGeometry(0).center(),
        )
        QTest.mouseClick(
            selector,
            Qt.LeftButton,
            Qt.NoModifier,
            selector.itemGeometry(0).center(),
        )
        self.assertEqual(pivot_activated[0][1].data, {"section": 0})
        self.assertEqual(
            selector_activated[0][1].data,
            {"workspace": 0},
        )

        QTest.keyClick(pivot, Qt.Key_End)
        QTest.keyClick(selector, Qt.Key_End)
        self.assertIn(Qt.Key_End, pivot.keys)
        self.assertIn(Qt.Key_End, selector.keys)

        pivot_overflow = []
        selector_overflow = []
        pivot.overflowActivated.connect(pivot_overflow.append)
        selector.overflowActivated.connect(selector_overflow.append)
        pivot.setOverflowBehavior(
            fluentqt.Pivot.OverflowBehavior.MoreButton
        )
        selector.setOverflowBehavior(
            fluentqt.SelectorBar.OverflowBehavior.MoreButton
        )
        self.app.processEvents()
        QTest.mouseClick(
            pivot,
            Qt.LeftButton,
            Qt.NoModifier,
            pivot.overflowGeometry().center(),
        )
        QTest.mouseClick(
            selector,
            Qt.LeftButton,
            Qt.NoModifier,
            selector.overflowGeometry().center(),
        )
        self.assertEqual(list(pivot_overflow[0]), pivot.hiddenItemIndexes())
        self.assertEqual(
            list(selector_overflow[0]),
            selector.hiddenItemIndexes(),
        )

    def test_tab_view_item_value_type_and_variant_data(self):
        empty = fluentqt.TabViewItem()
        self.assertEqual(empty.text, "")
        self.assertEqual(empty.iconGlyph, "")
        self.assertTrue(empty.closable)
        self.assertTrue(empty.enabled)
        self.assertIsNone(empty.data)
        self.assertEqual(empty.accessibleName, "")

        payload = {"document": 7, "tags": ["python", "qt"]}
        rich = fluentqt.TabViewItem(
            "Document",
            "document-glyph",
            False,
            True,
            payload,
            "Document tab",
        )
        copied = fluentqt.TabViewItem(rich)
        self.assertEqual(copied, rich)
        self.assertFalse(copied != rich)
        self.assertEqual(copied.data, payload)
        with self.assertRaises(TypeError):
            hash(rich)

        copied.text = "Copy"
        copied.data = [1, "two", {"three": 3}]
        self.assertNotEqual(copied, rich)
        self.assertEqual(rich.text, "Document")
        self.assertEqual(rich.data, payload)

    def test_tab_view_metadata_properties_signals_and_indexes(self):
        tabs = fluentqt.TabView()
        self.assertEqual(tabs.tabCount(), 0)
        self.assertEqual(tabs.selectedIndex(), -1)
        self.assertEqual(
            tabs.tabWidthMode(),
            fluentqt.TabView.TabWidthMode.Equal,
        )
        self.assertEqual(
            tabs.closeButtonOverlayMode(),
            fluentqt.TabView.CloseButtonOverlayMode.Auto,
        )
        self.assertTrue(tabs.areTabsClosable())
        self.assertTrue(tabs.isAddTabButtonVisible())
        self.assertFalse(tabs.isTabReorderEnabled())
        self.assertTrue(tabs.areKeyboardAcceleratorsEnabled())

        tab_changes = []
        selections = []
        currents = []
        width_changes = []
        close_mode_changes = []
        closable_changes = []
        add_button_changes = []
        reorder_changes = []
        keyboard_changes = []
        font_changes = []
        icon_changes = []
        tabs.tabsChanged.connect(lambda: tab_changes.append(True))
        tabs.selectedIndexChanged.connect(selections.append)
        tabs.currentChanged.connect(currents.append)
        tabs.tabWidthModeChanged.connect(width_changes.append)
        tabs.closeButtonOverlayModeChanged.connect(
            close_mode_changes.append
        )
        tabs.tabsClosableChanged.connect(closable_changes.append)
        tabs.addTabButtonVisibleChanged.connect(add_button_changes.append)
        tabs.tabReorderEnabledChanged.connect(reorder_changes.append)
        tabs.keyboardAcceleratorsEnabledChanged.connect(
            keyboard_changes.append
        )
        tabs.tabFontRoleChanged.connect(font_changes.append)
        tabs.iconFontFamilyChanged.connect(icon_changes.append)

        self.assertEqual(tabs.addTab("First"), 0)
        rich = fluentqt.TabViewItem(
            "Rich",
            "rich-glyph",
            False,
            False,
            {"kind": "rich"},
            "Readable rich tab",
        )
        self.assertEqual(tabs.addTab(rich), 1)
        self.assertTrue(tabs.insertTab(1, "Inserted"))
        self.assertFalse(tabs.insertTab(-1, "Invalid"))
        self.assertFalse(tabs.insertTab(99, "Invalid"))
        self.assertEqual(
            [item.text for item in tabs.tabs()],
            ["First", "Inserted", "Rich"],
        )
        self.assertEqual(tabs.tabAt(2), rich)
        self.assertEqual(tabs.tabAt(99), fluentqt.TabViewItem())

        self.assertTrue(tabs.setTabText(1, "Renamed"))
        self.assertTrue(tabs.setTabIconGlyph(1, "edit-glyph"))
        self.assertTrue(tabs.setTabClosable(1, False))
        self.assertTrue(tabs.setTabEnabled(1, False))
        self.assertTrue(tabs.setTabData(1, {"revision": 2}))
        self.assertTrue(
            tabs.setTabAccessibleName(1, "Renamed document tab")
        )
        updated = tabs.tabAt(1)
        self.assertEqual(updated.text, "Renamed")
        self.assertEqual(updated.iconGlyph, "edit-glyph")
        self.assertFalse(updated.closable)
        self.assertFalse(updated.enabled)
        self.assertEqual(updated.data, {"revision": 2})
        self.assertEqual(updated.accessibleName, "Renamed document tab")
        self.assertFalse(tabs.setTabText(1, "Renamed"))
        self.assertFalse(tabs.setTabEnabled(99, True))

        tabs.setTabWidthMode(
            fluentqt.TabView.TabWidthMode.SizeToContent
        )
        tabs.setCloseButtonOverlayMode(
            fluentqt.TabView.CloseButtonOverlayMode.Always
        )
        tabs.setTabsClosable(False)
        tabs.setAddTabButtonVisible(False)
        tabs.setTabReorderEnabled(True)
        tabs.setKeyboardAcceleratorsEnabled(False)
        tabs.setTabFontRole(fluentqt.FontRole.BodyStrong)
        tabs.setIconFontFamily("Python test icons")

        self.assertEqual(width_changes, [
            fluentqt.TabView.TabWidthMode.SizeToContent
        ])
        self.assertEqual(close_mode_changes, [
            fluentqt.TabView.CloseButtonOverlayMode.Always
        ])
        self.assertEqual(closable_changes, [False])
        self.assertEqual(add_button_changes, [False])
        self.assertEqual(reorder_changes, [True])
        self.assertEqual(keyboard_changes, [False])
        self.assertEqual(font_changes, [fluentqt.FontRole.BodyStrong])
        self.assertEqual(icon_changes, ["Python test icons"])
        self.assertGreaterEqual(len(tab_changes), 9)
        self.assertEqual(selections, [0])
        self.assertEqual(currents, [0])

    def test_tab_view_close_move_and_navigation_signals(self):
        tabs = fluentqt.TabView()
        tabs.addTab("Alpha")
        tabs.addTab("Beta")
        tabs.addTab("Gamma")
        tabs.setSelectedIndex(1)

        moved = []
        selections = []
        currents = []
        tabs.tabMoved.connect(lambda start, end: moved.append((start, end)))
        tabs.selectedIndexChanged.connect(selections.append)
        tabs.currentChanged.connect(currents.append)

        self.assertTrue(tabs.moveTab(1, 2))
        self.assertEqual(moved, [(1, 2)])
        self.assertEqual(
            [item.text for item in tabs.tabs()],
            ["Alpha", "Gamma", "Beta"],
        )
        self.assertEqual(tabs.selectedIndex(), 2)
        self.assertEqual(selections, [2])
        self.assertEqual(currents, [2])
        self.assertFalse(tabs.moveTab(2, 2))
        self.assertFalse(tabs.moveTab(-1, 0))

        self.assertTrue(tabs.setTabClosable(1, False))
        self.assertFalse(tabs.closeTab(1))
        self.assertTrue(tabs.closeTab(2))
        self.assertEqual(tabs.selectedIndex(), 1)
        self.assertEqual(
            [item.text for item in tabs.tabs()],
            ["Alpha", "Gamma"],
        )
        self.assertTrue(tabs.removeTab(1))
        self.assertEqual(tabs.selectedIndex(), 0)
        tabs.clearTabs()
        self.assertEqual(tabs.tabCount(), 0)
        self.assertEqual(tabs.selectedIndex(), -1)

    def test_tab_view_keyboard_rtl_and_python_virtual_override(self):
        class PythonTabView(fluentqt.TabView):
            def __init__(self):
                super().__init__()
                self.key_presses = []

            def keyPressEvent(self, event):
                self.key_presses.append(event.key())
                super().keyPressEvent(event)

        tabs = PythonTabView()
        tabs.resize(680, 160)
        tabs.addTab("One")
        tabs.addTab("Two")
        tabs.addTab("Three")
        tabs.show()
        self.app.processEvents()
        tabs.setFocus()

        add_requests = []
        close_requests = []
        tabs.addTabRequested.connect(lambda: add_requests.append(True))
        tabs.tabCloseRequested.connect(close_requests.append)
        QTest.keyClick(tabs, Qt.Key_Right)
        QTest.keyClick(tabs, Qt.Key_Space)
        self.assertEqual(tabs.selectedIndex(), 1)
        QTest.keyClick(tabs, Qt.Key_T, Qt.ControlModifier)
        QTest.keyClick(tabs, Qt.Key_W, Qt.ControlModifier)
        QTest.keyClick(tabs, Qt.Key_Delete)
        self.assertEqual(add_requests, [True])
        self.assertEqual(close_requests, [1, 1])
        self.assertIn(Qt.Key_Right, tabs.key_presses)

        tabs.setLayoutDirection(Qt.RightToLeft)
        self.app.processEvents()
        self.assertGreater(
            tabs.tabGeometry(0).center().x(),
            tabs.tabGeometry(1).center().x(),
        )
        QTest.keyClick(tabs, Qt.Key_Home)
        QTest.keyClick(tabs, Qt.Key_Return)
        QTest.keyClick(tabs, Qt.Key_Left)
        QTest.keyClick(tabs, Qt.Key_Space)
        self.assertEqual(tabs.selectedIndex(), 1)

        tabs.setKeyboardAcceleratorsEnabled(False)
        QTest.keyClick(tabs, Qt.Key_T, Qt.ControlModifier)
        QTest.keyClick(tabs, Qt.Key_W, Qt.ControlModifier)
        self.assertEqual(add_requests, [True])
        self.assertEqual(close_requests, [1, 1])
        tabs.close()

    def test_tab_view_selection_drives_external_page_host(self):
        tabs = fluentqt.TabView()
        host = QStackedWidget()
        pages = [QWidget(), QWidget(), QWidget()]
        for index, page in enumerate(pages):
            tabs.addTab("Page {0}".format(index + 1))
            host.addWidget(page)
        tabs.currentChanged.connect(host.setCurrentIndex)
        host.setCurrentIndex(tabs.selectedIndex())

        self.assertEqual(host.currentIndex(), 0)
        tabs.setSelectedIndex(2)
        self.assertEqual(host.currentIndex(), 2)
        self.assertIs(host.currentWidget(), pages[2])
        self.assertIs(pages[2].parent(), host)

    def test_stack_content_host_properties_navigation_and_page_facade(self):
        class PythonPage(QWidget):
            def __init__(self):
                super().__init__()
                self.marker = "python-navigation-page"

        host = fluentqt.StackContentHost()
        self.assertIs(navigation.StackContentHost, fluentqt.StackContentHost)
        self.assertEqual(host.count(), 0)
        self.assertEqual(host.currentIndex(), -1)
        self.assertFalse(host.busy())
        self.assertTrue(host.transitionAnimationEnabled())
        self.assertEqual(
            host.transitionEffect(),
            fluentqt.StackContentHost.TransitionEffect.SlideFromLeft,
        )

        effect_changes = []
        current_changes = []
        host.transitionEffectChanged.connect(effect_changes.append)
        host.currentIndexChanged.connect(current_changes.append)
        host.setTransitionEffect(
            fluentqt.StackContentHost.TransitionEffect.SlideFromBottom
        )
        host.setTransitionAnimationEnabled(False)

        page = PythonPage()
        page_ref = weakref.ref(page)
        self.assertTrue(host.addBorrowedPage(page))
        del page
        gc.collect()

        hosted = host.pageWidget(0)
        self.assertIs(hosted, page_ref())
        self.assertEqual(hosted.marker, "python-navigation-page")
        self.assertIn(id(hosted), host._fluentqt_page_records)
        host.setCurrentIndex(0, 0, False)
        self.assertEqual(host.currentIndex(), 0)
        self.assertEqual(current_changes, [0])
        self.assertEqual(
            effect_changes,
            [fluentqt.StackContentHost.TransitionEffect.SlideFromBottom],
        )
        self.assertFalse(host.transitionAnimationEnabled())

        self.assertTrue(host.addPage(None))
        self.assertEqual(host.count(), 2)
        self.assertIsNone(host.pageWidget(1))
        self.assertTrue(host.removePage(1))

        taken = host.takePage(0)
        self.assertIs(taken, hosted)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))
        self.assertEqual(host._fluentqt_page_records, {})

    def test_stack_content_host_owned_page_lifecycle(self):
        host = fluentqt.StackContentHost()
        first = QWidget()
        second = QWidget()
        self.assertTrue(host.addPage(first))
        self.assertEqual(
            host.pageOwnershipAt(0),
            fluentqt.WidgetOwnership.Owned,
        )
        self.assertTrue(host.replaceOwnedPage(0, second))
        self.assertFalse(Shiboken.isValid(first))
        self.assertIs(host.pageWidget(0), second)
        self.assertTrue(host.removePage(0))
        self.assertFalse(Shiboken.isValid(second))

        owned_host = fluentqt.StackContentHost()
        owned_page = QWidget()
        self.assertTrue(owned_host.addOwnedPage(owned_page))
        host_ref = weakref.ref(owned_host)
        del owned_host
        gc.collect()
        self.assertIsNone(host_ref())
        self.assertFalse(Shiboken.isValid(owned_page))

    def test_stack_content_host_non_owned_page_lifecycle(self):
        host = fluentqt.StackContentHost()
        first = QWidget()
        second = QWidget()
        self.assertTrue(host.addBorrowedPage(first))
        self.assertTrue(host.replaceBorrowedPage(0, second))
        self.assertTrue(Shiboken.isValid(first))
        self.assertIsNone(first.parent())
        host.clearPages()
        self.assertTrue(Shiboken.isValid(second))
        self.assertIsNone(second.parent())

        original_parent = QWidget()
        reparented = QWidget(original_parent)
        self.assertTrue(host.addReparentedPage(reparented))
        self.assertIs(reparented.parent(), host)
        self.assertTrue(host.releasePage(0))
        self.assertIs(reparented.parent(), original_parent)

        take_parent = QWidget()
        taken_page = QWidget(take_parent)
        self.assertTrue(host.addReparentedPage(taken_page))
        self.assertIs(host.takePage(0), taken_page)
        self.assertIsNone(taken_page.parent())

    def test_stack_content_host_rejects_invalid_pages_and_tracks_delete(self):
        host = fluentqt.StackContentHost()
        page = QWidget()
        self.assertTrue(host.addBorrowedPage(page))
        self.assertFalse(host.addOwnedPage(page))

        candidate_parent = QWidget()
        candidate = QWidget(candidate_parent)
        self.assertFalse(host.insertOwnedPage(9, candidate))
        self.assertIs(candidate.parent(), candidate_parent)

        ancestor = QWidget()
        nested_host = fluentqt.StackContentHost(ancestor)
        with self.assertRaisesRegex(ValueError, "host or its ancestor"):
            nested_host.addBorrowedPage(ancestor)

        page.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(page))
        self.assertEqual(host.count(), 0)
        self.assertEqual(host._fluentqt_page_records, {})

    def test_stack_content_host_owned_gc_stress(self):
        for _ in range(10):
            host = fluentqt.StackContentHost()
            page = QWidget()
            host.addOwnedPage(page)
            host_ref = weakref.ref(host)
            del host
            gc.collect()
            self.assertIsNone(host_ref())
            self.assertFalse(Shiboken.isValid(page))
            del page

    def test_stack_content_host_borrowed_gc_stress(self):
        for _ in range(10):
            host = fluentqt.StackContentHost()
            page = QWidget()
            host.addBorrowedPage(page)
            host_ref = weakref.ref(host)
            del host
            gc.collect()
            self.assertIsNone(host_ref())
            self.assertTrue(Shiboken.isValid(page))
            self.assertIsNone(page.parent())
            del page
            gc.collect()

    def test_stack_content_host_reparented_gc_stress(self):
        for _ in range(10):
            original_parent = QWidget()
            page = QWidget(original_parent)
            host = fluentqt.StackContentHost()
            host.addReparentedPage(page)
            host_ref = weakref.ref(host)
            del host
            gc.collect()
            self.assertIsNone(host_ref())
            self.assertTrue(Shiboken.isValid(page))
            self.assertIs(page.parent(), original_parent)
            del original_parent
            gc.collect()
            self.assertFalse(Shiboken.isValid(page))
            del page

    def test_navigation_view_properties_internal_host_and_page_retention(self):
        nav = fluentqt.NavigationView()
        self.assertIs(navigation.NavigationView, fluentqt.NavigationView)
        self.assertIs(nav.contentHost(), nav._fluentqt_content_host)
        self.assertIsInstance(nav.contentHost(), fluentqt.StackContentHost)
        self.assertIs(nav.contentHost().parent(), nav)
        self.assertEqual(
            nav.displayMode(),
            fluentqt.NavigationView.DisplayMode.Auto,
        )
        self.assertTrue(nav.isPaneOpen())

        mode_changes = []
        nav.displayModeChanged.connect(mode_changes.append)
        nav.setDisplayMode(fluentqt.NavigationView.DisplayMode.Top)
        self.assertEqual(
            mode_changes,
            [fluentqt.NavigationView.DisplayMode.Top],
        )

        class PythonPage(QWidget):
            def __init__(self):
                super().__init__()
                self.marker = "navigation-content-host-page"

        page = PythonPage()
        page_ref = weakref.ref(page)
        self.assertTrue(nav.contentHost().addBorrowedPage(page))
        del page
        gc.collect()
        hosted = nav.contentHost().pageWidget(0)
        self.assertIs(hosted, page_ref())
        self.assertEqual(hosted.marker, "navigation-content-host-page")
        self.assertTrue(nav.contentHost().removePage(0))
        self.assertIsNone(hosted.parent())

    def test_navigation_view_chrome_ownership_lifecycle(self):
        nav = fluentqt.NavigationView()
        first = QWidget()
        second = QWidget()
        self.assertTrue(nav.setHeaderChromeWidget(first))
        self.assertEqual(
            nav.headerChromeWidgetOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )
        self.assertTrue(nav.setOwnedHeaderChromeWidget(second))
        self.assertFalse(Shiboken.isValid(first))
        self.assertIs(nav.headerChromeWidget(), second)
        self.assertTrue(nav.releaseHeaderChromeWidget())
        self.assertFalse(Shiboken.isValid(second))

        borrowed = QWidget()
        self.assertTrue(nav.setBorrowedMainChromeWidget(borrowed))
        self.assertTrue(nav.releaseMainChromeWidget())
        self.assertTrue(Shiboken.isValid(borrowed))
        self.assertIsNone(borrowed.parent())

        original_parent = QWidget()
        reparented = QWidget(original_parent)
        self.assertTrue(
            nav.setReparentedFooterChromeWidget(reparented)
        )
        self.assertIs(reparented.parent(), nav)
        self.assertTrue(nav.releaseFooterChromeWidget())
        self.assertIs(reparented.parent(), original_parent)

        taken = QWidget()
        self.assertTrue(nav.setBorrowedHeaderChromeWidget(taken))
        self.assertIs(nav.takeHeaderChromeWidget(), taken)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))

    def test_navigation_view_chrome_rejects_invalid_and_tracks_delete(self):
        ancestor = QWidget()
        nav = fluentqt.NavigationView(ancestor)
        header = QWidget()
        self.assertTrue(nav.setBorrowedHeaderChromeWidget(header))
        self.assertFalse(nav.setBorrowedMainChromeWidget(header))
        with self.assertRaisesRegex(ValueError, "host or its ancestor"):
            nav.setBorrowedMainChromeWidget(ancestor)
        with self.assertRaisesRegex(ValueError, "contentHost"):
            nav.setBorrowedMainChromeWidget(nav.contentHost())

        changes = []
        nav.headerChromeWidgetChanged.connect(changes.append)
        header.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(header))
        self.assertIsNone(nav.headerChromeWidget())
        self.assertNotIn("header", nav._fluentqt_chrome_records)
        self.assertEqual(len(changes), 1)
        self.assertIsNone(changes[0])

    def test_navigation_view_owned_chrome_gc_stress(self):
        for _ in range(10):
            nav = fluentqt.NavigationView()
            chrome = QWidget()
            nav.setOwnedHeaderChromeWidget(chrome)
            nav_ref = weakref.ref(nav)
            del nav
            gc.collect()
            self.assertIsNone(nav_ref())
            self.assertFalse(Shiboken.isValid(chrome))
            del chrome

    def test_navigation_view_borrowed_chrome_gc_stress(self):
        for _ in range(10):
            nav = fluentqt.NavigationView()
            chrome = QWidget()
            nav.setBorrowedMainChromeWidget(chrome)
            nav_ref = weakref.ref(nav)
            del nav
            gc.collect()
            self.assertIsNone(nav_ref())
            self.assertTrue(Shiboken.isValid(chrome))
            self.assertIsNone(chrome.parent())
            del chrome
            gc.collect()

    def test_navigation_view_reparented_chrome_gc_stress(self):
        for _ in range(10):
            original_parent = QWidget()
            chrome = QWidget(original_parent)
            nav = fluentqt.NavigationView()
            nav.setReparentedFooterChromeWidget(chrome)
            nav_ref = weakref.ref(nav)
            del nav
            gc.collect()
            self.assertIsNone(nav_ref())
            self.assertTrue(Shiboken.isValid(chrome))
            self.assertIs(chrome.parent(), original_parent)
            del original_parent
            gc.collect()
            self.assertFalse(Shiboken.isValid(chrome))
            del chrome

    def test_stack_view_properties_signals_and_navigation(self):
        class PythonPage(QWidget):
            def __init__(self):
                super().__init__()
                self.marker = "python-stack-page"

        stack = fluentqt.StackView()
        stack.setTransitionAnimationEnabled(False)
        self.assertEqual(stack.depth(), 0)
        self.assertIsNone(stack.currentItem())
        self.assertIsNone(stack.initialItem())
        self.assertFalse(stack.canPop())
        self.assertEqual(stack.orientation(), Qt.Horizontal)
        self.assertEqual(
            stack.transitionType(),
            fluentqt.StackView.StackViewTransitionType.SlideFade,
        )

        pushed = []
        popped = []
        depth_changes = []
        current_changes = []
        stack.itemPushed.connect(pushed.append)
        stack.itemPopped.connect(popped.append)
        stack.depthChanged.connect(depth_changes.append)
        stack.currentItemChanged.connect(current_changes.append)

        root = PythonPage()
        borrowed = PythonPage()
        self.assertTrue(stack.push(root))
        self.assertTrue(stack.pushBorrowedItem(borrowed))
        self.assertEqual(stack.depth(), 2)
        self.assertIs(stack.initialItem(), root)
        self.assertIs(stack.currentItem(), borrowed)
        self.assertEqual(borrowed.marker, "python-stack-page")
        self.assertEqual(
            stack.itemStatus(root),
            fluentqt.StackView.StackViewItemStatus.Inactive,
        )
        self.assertEqual(
            stack.itemStatus(borrowed),
            fluentqt.StackView.StackViewItemStatus.Active,
        )
        self.assertEqual(pushed, [root, borrowed])
        self.assertEqual(depth_changes, [1, 2])

        stack.setCurrentWidget(root)
        self.assertIs(stack.currentItem(), root)
        stack.setCurrentWidget(borrowed)
        self.assertIs(stack.currentItem(), borrowed)
        self.assertTrue(stack.pop())
        self.assertEqual(popped, [borrowed])
        self.assertEqual(stack.depth(), 1)
        self.assertIs(stack.currentItem(), root)
        self.assertIsNone(borrowed.parent())
        self.assertNotIn(id(borrowed), stack._fluentqt_page_records)
        self.assertGreaterEqual(len(current_changes), 3)

        replacement = QWidget()
        self.assertTrue(stack.replaceBorrowedItem(replacement))
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(root))
        self.assertIs(stack.currentItem(), replacement)
        self.assertEqual(stack.depth(), 1)

        stack.setOrientation(Qt.Vertical)
        stack.setTransitionDuration(-10)
        stack.setTransitionType(
            fluentqt.StackView.StackViewTransitionType.ScaleFade
        )
        self.assertEqual(stack.orientation(), Qt.Vertical)
        self.assertEqual(stack.transitionDuration(), 0)
        self.assertEqual(
            stack.transitionType(),
            fluentqt.StackView.StackViewTransitionType.ScaleFade,
        )

    def test_stack_view_constructor_routes_initial_page_through_facade(self):
        page = QWidget()
        stack = fluentqt.StackView(initialItem=page)
        stack.setTransitionAnimationEnabled(False)
        self.assertIs(stack.initialItem(), page)
        self.assertIs(stack.currentItem(), page)
        self.assertEqual(stack.depth(), 1)
        self.assertIn(id(page), stack._fluentqt_page_records)

        stack_ref = weakref.ref(stack)
        del stack
        gc.collect()
        self.assertIsNone(stack_ref())
        self.assertFalse(Shiboken.isValid(page))

    def test_stack_view_owned_page_lifecycle(self):
        stack = fluentqt.StackView()
        stack.setTransitionAnimationEnabled(False)
        root = QWidget()
        owned = QWidget()
        self.assertTrue(stack.pushBorrowedItem(root))
        self.assertTrue(stack.pushOwnedItem(owned))
        self.assertTrue(stack.pop())
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(owned))
        self.assertTrue(Shiboken.isValid(root))

        second_owned = QWidget()
        self.assertTrue(stack.pushOwnedItem(second_owned))
        stack_ref = weakref.ref(stack)
        del stack
        gc.collect()
        self.assertIsNone(stack_ref())
        self.assertFalse(Shiboken.isValid(second_owned))
        self.assertTrue(Shiboken.isValid(root))
        self.assertIsNone(root.parent())

    def test_stack_view_borrowed_page_lifecycle(self):
        class PythonPage(QWidget):
            def __init__(self):
                super().__init__()
                self.marker = "borrowed-stack-subclass"

        stack = fluentqt.StackView()
        stack.setTransitionAnimationEnabled(False)
        root = QWidget()
        page = PythonPage()
        page_ref = weakref.ref(page)
        self.assertTrue(stack.pushOwnedItem(root))
        self.assertTrue(stack.pushBorrowedItem(page))
        del page
        gc.collect()

        hosted = stack.currentItem()
        self.assertIs(hosted, page_ref())
        self.assertEqual(hosted.marker, "borrowed-stack-subclass")
        self.assertTrue(stack.pop())
        self.assertTrue(Shiboken.isValid(hosted))
        self.assertIsNone(hosted.parent())
        self.assertEqual(stack._fluentqt_page_records.keys(), {id(root)})

        stack_ref = weakref.ref(stack)
        del stack
        gc.collect()
        self.assertIsNone(stack_ref())
        self.assertTrue(Shiboken.isValid(hosted))
        self.assertIsNone(hosted.parent())

    def test_stack_view_reparented_page_lifecycle(self):
        original_parent = QWidget()
        original_parent_ref = weakref.ref(original_parent)
        page = QWidget(original_parent)
        stack = fluentqt.StackView()
        stack.setTransitionAnimationEnabled(False)
        self.assertTrue(stack.pushOwnedItem(QWidget()))
        self.assertTrue(stack.pushReparentedItem(page))
        self.assertIs(page.parent(), stack)

        del original_parent
        gc.collect()
        self.assertIsNotNone(original_parent_ref())
        restored_parent = original_parent_ref()
        self.assertTrue(stack.pop())
        self.assertIsNotNone(restored_parent)
        self.assertIs(page.parent(), restored_parent)
        self.assertNotIn(id(page), stack._fluentqt_page_records)

        second_stack = fluentqt.StackView()
        second_stack.setTransitionAnimationEnabled(False)
        second_parent = QWidget()
        second_page = QWidget(second_parent)
        self.assertTrue(
            second_stack.setInitialReparentedItem(second_page)
        )
        second_stack_ref = weakref.ref(second_stack)
        del second_stack
        gc.collect()
        self.assertIsNone(second_stack_ref())
        self.assertTrue(Shiboken.isValid(second_page))
        self.assertIs(second_page.parent(), second_parent)

    def test_stack_view_bulk_navigation_and_indexed_replacement(self):
        stack = fluentqt.StackView()
        stack.setTransitionAnimationEnabled(False)
        pages = [QWidget(), QWidget(), QWidget()]
        self.assertTrue(stack.pushBorrowedItems(pages))
        self.assertEqual(stack.depth(), 3)
        self.assertIs(stack.currentItem(), pages[-1])
        self.assertEqual(
            [stack.itemAt(index) for index in range(stack.depth())],
            pages,
        )

        self.assertTrue(stack.popToItem(pages[0]))
        self.assertEqual(stack.depth(), 1)
        self.assertIs(stack.currentItem(), pages[0])
        self.assertIsNone(pages[1].parent())
        self.assertIsNone(pages[2].parent())

        replacement_parent = QWidget()
        replacement = QWidget(replacement_parent)
        self.assertTrue(
            stack.replaceReparentedItemAt(0, replacement)
        )
        self.assertIsNone(pages[0].parent())
        self.assertIs(stack.currentItem(), replacement)
        self.assertTrue(stack.clear())
        self.assertIs(replacement.parent(), replacement_parent)
        self.assertEqual(stack._fluentqt_page_records, {})

    def test_stack_view_retains_page_until_animated_pop_finishes(self):
        stack = fluentqt.StackView()
        stack.resize(360, 220)
        stack.setTransitionDuration(40)
        root = QWidget()
        page = QWidget()
        self.assertTrue(stack.pushOwnedItem(root))
        self.assertTrue(stack.pushBorrowedItem(page))
        self.assertTrue(stack.busy())
        wait_for_events(80)
        self.assertFalse(stack.busy())

        self.assertTrue(stack.pop())
        self.assertTrue(stack.busy())
        self.assertIn(id(page), stack._fluentqt_page_records)
        self.assertIs(page.parent(), stack)
        wait_for_events(80)
        self.assertFalse(stack.busy())
        self.assertNotIn(id(page), stack._fluentqt_page_records)
        self.assertIsNone(page.parent())

    def test_stack_view_blocks_unsafe_base_mutation_and_rolls_back(self):
        stack = fluentqt.StackView()
        page = QWidget()
        self.assertTrue(stack.pushBorrowedItem(page))
        candidate_parent = QWidget()
        candidate = QWidget(candidate_parent)
        self.assertFalse(stack.replaceOwnedItemAt(5, candidate))
        self.assertIs(candidate.parent(), candidate_parent)
        self.assertEqual(
            stack._fluentqt_page_records.keys(),
            {id(page)},
        )

        self.assertFalse(stack.pushOwnedItem(page))
        self.assertIs(page.parent(), stack)
        with self.assertRaisesRegex(RuntimeError, "ownership-aware"):
            stack.insertWidget(0, QWidget())
        with self.assertRaisesRegex(RuntimeError, "pushOwnedItem"):
            stack.addWidget(QWidget())
        with self.assertRaisesRegex(RuntimeError, "pop"):
            stack.removeWidget(page)
        with self.assertRaisesRegex(ValueError, "already be hosted"):
            stack.setCurrentWidget(QWidget())
        with self.assertRaisesRegex(TypeError, "defaultItemOwnership"):
            fluentqt.StackView(
                defaultItemOwnership=fluentqt.WidgetOwnership.Borrowed
            )

        ancestor = QWidget()
        nested_stack = fluentqt.StackView(ancestor)
        with self.assertRaisesRegex(ValueError, "host or its ancestor"):
            nested_stack.pushBorrowedItem(ancestor)

    def test_stack_view_owned_gc_stress(self):
        for _ in range(25):
            stack = fluentqt.StackView()
            stack.setTransitionAnimationEnabled(False)
            page = QWidget()
            stack.pushOwnedItem(page)
            stack_ref = weakref.ref(stack)
            del stack
            gc.collect()
            self.assertIsNone(stack_ref())
            self.assertFalse(Shiboken.isValid(page))
            del page

    def test_stack_view_borrowed_gc_stress(self):
        for _ in range(25):
            stack = fluentqt.StackView()
            stack.setTransitionAnimationEnabled(False)
            page = QWidget()
            stack.pushBorrowedItem(page)
            stack_ref = weakref.ref(stack)
            del stack
            gc.collect()
            self.assertIsNone(stack_ref())
            self.assertTrue(Shiboken.isValid(page))
            self.assertIsNone(page.parent())
            del page
            gc.collect()

    def test_stack_view_reparented_gc_stress(self):
        for _ in range(25):
            original_parent = QWidget()
            page = QWidget(original_parent)
            stack = fluentqt.StackView()
            stack.setTransitionAnimationEnabled(False)
            stack.pushReparentedItem(page)
            stack_ref = weakref.ref(stack)
            del stack
            gc.collect()
            self.assertIsNone(stack_ref())
            self.assertTrue(Shiboken.isValid(page))
            self.assertIs(page.parent(), original_parent)
            del original_parent
            gc.collect()
            self.assertFalse(Shiboken.isValid(page))
            del page

    def test_scroll_view_constructor_routes_content_through_facade(self):
        content = QWidget()
        view = fluentqt.ScrollView(contentWidget=content)

        self.assertIs(view.contentWidget(), content)
        self.assertIs(view._fluentqt_hosted_content, content)
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

        view_ref = weakref.ref(view)
        del view
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertFalse(Shiboken.isValid(content))

    def test_scroll_view_owned_content_lifecycle(self):
        view = fluentqt.ScrollView()
        first = QWidget()
        second = QWidget()

        view.setContentWidget(first)
        self.assertIs(view.contentWidget(), first)
        self.assertIs(view._fluentqt_hosted_content, first)
        self.assertIs(first.parent(), view.viewport())
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

        view.setOwnedContentWidget(second)
        self.assertFalse(Shiboken.isValid(first))
        self.assertIs(view.contentWidget(), second)
        self.assertIs(view._fluentqt_hosted_content, second)

        view.setContentWidget(None)
        self.assertFalse(Shiboken.isValid(second))
        self.assertIsNone(view.contentWidget())
        self.assertIsNone(view._fluentqt_hosted_content)
        self.assertIsNone(view._fluentqt_original_parent)
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

        rejected = QWidget()
        with self.assertRaises(TypeError):
            view.setContentWidget(
                rejected,
                fluentqt.WidgetOwnership.Borrowed,
            )
        with self.assertRaises(TypeError):
            native.fluent.ScrollView.setContentWidget(
                view,
                rejected,
                fluentqt.WidgetOwnership.Borrowed,
            )
        self.assertTrue(Shiboken.isValid(rejected))
        rejected_ref = weakref.ref(rejected)
        del rejected
        gc.collect()
        self.assertIsNone(rejected_ref())
        view_ref = weakref.ref(view)
        del view
        gc.collect()
        self.assertIsNone(view_ref())

        previous_parent = QWidget()
        previous_parent_ref = weakref.ref(previous_parent)
        parented = QWidget(previous_parent)
        parented_view = fluentqt.ScrollView()
        parented_view.setContentWidget(parented)
        self.assertIs(parented.parent(), parented_view.viewport())
        del previous_parent
        gc.collect()
        self.assertIsNone(previous_parent_ref())
        self.assertTrue(Shiboken.isValid(parented))
        parented_view_ref = weakref.ref(parented_view)
        del parented_view
        gc.collect()
        self.assertIsNone(parented_view_ref())
        self.assertFalse(Shiboken.isValid(parented))

    def test_scroll_view_borrowed_content_lifecycle(self):
        class PythonContent(QWidget):
            def __init__(self):
                super().__init__()
                self.marker = "borrowed-subclass"

        view = fluentqt.ScrollView()
        first = PythonContent()
        first_ref = weakref.ref(first)
        view.setBorrowedContentWidget(first)
        del first
        gc.collect()

        hosted = view.contentWidget()
        self.assertIs(hosted, first_ref())
        self.assertEqual(hosted.marker, "borrowed-subclass")
        self.assertIs(view._fluentqt_hosted_content, hosted)
        self.assertIsNone(view._fluentqt_original_parent)
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Borrowed,
        )

        second = QWidget()
        view.setBorrowedContentWidget(second)
        self.assertTrue(Shiboken.isValid(hosted))
        self.assertIsNone(hosted.parent())
        self.assertIs(view.contentWidget(), second)

        view.setBorrowedContentWidget(None)
        self.assertTrue(Shiboken.isValid(second))
        self.assertIsNone(second.parent())
        self.assertIsNone(view.contentWidget())
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

        previous_parent = QWidget()
        previous_parent_ref = weakref.ref(previous_parent)
        parented = QWidget(previous_parent)
        parented_view = fluentqt.ScrollView()
        parented_view.setBorrowedContentWidget(parented)
        self.assertIs(parented.parent(), parented_view.viewport())
        del previous_parent
        gc.collect()
        self.assertIsNone(previous_parent_ref())
        self.assertTrue(Shiboken.isValid(parented))

        parented_view_ref = weakref.ref(parented_view)
        del parented_view
        gc.collect()
        self.assertIsNone(parented_view_ref())
        self.assertTrue(Shiboken.isValid(parented))
        self.assertIsNone(parented.parent())

    def test_scroll_view_reparented_content_lifecycle(self):
        first_parent = QWidget()
        first = QWidget(first_parent)
        second_parent = QWidget()
        second = QWidget(second_parent)
        view = fluentqt.ScrollView()

        view.setReparentedContentWidget(first)
        self.assertIs(first.parent(), view.viewport())
        self.assertIs(view._fluentqt_original_parent, first_parent)
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Reparented,
        )

        view.setReparentedContentWidget(second)
        self.assertTrue(Shiboken.isValid(first))
        self.assertIs(first.parent(), first_parent)
        self.assertIs(second.parent(), view.viewport())
        self.assertIs(view._fluentqt_original_parent, second_parent)

        view.setReparentedContentWidget(None)
        self.assertTrue(Shiboken.isValid(second))
        self.assertIs(second.parent(), second_parent)
        self.assertIsNone(view.contentWidget())
        self.assertIsNone(view._fluentqt_original_parent)
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

        restore_parent = QWidget()
        restored = QWidget(restore_parent)
        restoring_view = fluentqt.ScrollView()
        restoring_view.setReparentedContentWidget(restored)
        restoring_view_ref = weakref.ref(restoring_view)
        del restoring_view
        gc.collect()
        self.assertIsNone(restoring_view_ref())
        self.assertTrue(Shiboken.isValid(restored))
        self.assertIs(restored.parent(), restore_parent)

    def test_scroll_view_reparented_keeps_original_parent_alive(self):
        original_parent = QWidget()
        original_parent_ref = weakref.ref(original_parent)
        child = QWidget(original_parent)
        view = fluentqt.ScrollView()
        view.setReparentedContentWidget(child)

        del original_parent
        gc.collect()
        self.assertIsNotNone(original_parent_ref())
        self.assertIs(
            view._fluentqt_original_parent,
            original_parent_ref(),
        )

        taken = view.takeContentWidget()
        gc.collect()
        self.assertIs(taken, child)
        self.assertIsNone(taken.parent())
        self.assertIsNone(original_parent_ref())

    def test_scroll_view_requires_take_before_mode_change(self):
        view = fluentqt.ScrollView()
        child = QWidget()
        view.setBorrowedContentWidget(child)

        with self.assertRaisesRegex(
            ValueError,
            "takeContentWidget",
        ):
            view.setOwnedContentWidget(child)
        with self.assertRaisesRegex(
            ValueError,
            "takeContentWidget",
        ):
            view.setReparentedContentWidget(child)

        taken = view.takeContentWidget()
        view.setOwnedContentWidget(taken)
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

    def test_scroll_view_take_returns_python_ownership(self):
        class PythonContent(QWidget):
            def __init__(self):
                super().__init__()
                self.marker = "python-subclass"

        view = fluentqt.ScrollView()
        content = PythonContent()
        view.setWidget(content)
        del content
        gc.collect()

        hosted = view.contentWidget()
        self.assertIsInstance(hosted, PythonContent)
        self.assertEqual(hosted.marker, "python-subclass")
        taken = view.takeWidget()
        self.assertIs(taken, hosted)
        self.assertIsNone(view._fluentqt_hosted_content)
        self.assertIsNone(view._fluentqt_original_parent)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))

        view_ref = weakref.ref(view)
        del view
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertTrue(Shiboken.isValid(taken))
        taken_ref = weakref.ref(taken)
        del taken
        del hosted
        gc.collect()
        self.assertIsNone(taken_ref())

    def test_scroll_view_take_handles_non_owned_modes(self):
        for mode_name, setter_name in (
            ("Borrowed", "setBorrowedContentWidget"),
            ("Reparented", "setReparentedContentWidget"),
        ):
            with self.subTest(mode=mode_name):
                original_parent = (
                    QWidget() if mode_name == "Reparented" else None
                )
                child = QWidget(original_parent)
                view = fluentqt.ScrollView()
                getattr(view, setter_name)(child)

                taken = view.takeContentWidget()
                self.assertIs(taken, child)
                self.assertIsNone(taken.parent())
                self.assertTrue(Shiboken.ownedByPython(taken))
                self.assertIsNone(view._fluentqt_hosted_content)
                self.assertIsNone(view._fluentqt_original_parent)
                self.assertEqual(
                    view.contentOwnership(),
                    fluentqt.WidgetOwnership.Owned,
                )

                view_ref = weakref.ref(view)
                del view
                gc.collect()
                self.assertIsNone(view_ref())
                self.assertTrue(Shiboken.isValid(taken))
                taken_ref = weakref.ref(taken)
                del taken
                del child
                gc.collect()
                self.assertIsNone(taken_ref())

    def test_qscrollarea_owned_gc_stress(self):
        for _ in range(25):
            view = QScrollArea()
            child = QWidget()
            view.setWidget(child)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertFalse(Shiboken.isValid(child))
            del child

    def test_scroll_view_facade_gc_stress_without_content(self):
        for _ in range(25):
            view = fluentqt.ScrollView()
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())

    def test_scroll_view_native_owned_gc_stress(self):
        for _ in range(25):
            view = native.fluent.ScrollView()
            child = QWidget()
            view.setContentWidget(child)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertFalse(Shiboken.isValid(child))
            del child

    def test_scroll_view_owned_gc_stress(self):
        for _ in range(25):
            view = fluentqt.ScrollView()
            child = QWidget()
            view.setContentWidget(child)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertFalse(Shiboken.isValid(child))
            del child

    def test_scroll_view_borrowed_gc_stress(self):
        for _ in range(25):
            view = fluentqt.ScrollView()
            child = QWidget()
            view.setBorrowedContentWidget(child)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertTrue(Shiboken.isValid(child))
            self.assertIsNone(child.parent())
            del child
            gc.collect()

    def test_scroll_view_reparented_gc_stress(self):
        for _ in range(25):
            original_parent = QWidget()
            child = QWidget(original_parent)
            view = fluentqt.ScrollView()
            view.setReparentedContentWidget(child)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertTrue(Shiboken.isValid(child))
            self.assertIs(child.parent(), original_parent)
            del original_parent
            gc.collect()
            self.assertFalse(Shiboken.isValid(child))
            del child

    def test_scroll_view_qscrollarea_base_owned_gc_stress(self):
        for _ in range(25):
            view = native.fluent.ScrollView()
            child = QWidget()
            QScrollArea.setWidget(view, child)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertFalse(Shiboken.isValid(child))
            del child


if __name__ == "__main__":
    unittest.main()
