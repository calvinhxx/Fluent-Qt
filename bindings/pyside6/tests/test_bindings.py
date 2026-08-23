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
    dialogs_flyouts,
    foundation,
    layout,
    menus_toolbars,
    navigation,
    scrolling,
    status_info,
    textfields,
    windowing,
)
from PySide6.QtCore import (
    QAbstractListModel,
    QAbstractTableModel,
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
    QSizeF,
    QRectF,
    QStandardPaths,
    QStringListModel,
    QTime,
    QTimer,
    Qt,
    QUrl,
    qVersion,
)
from PySide6.QtGui import (
    QAction,
    QColor,
    QIcon,
    QKeySequence,
    QPalette,
    QStandardItem,
    QStandardItemModel,
)
from PySide6.QtTest import QSignalSpy, QTest
from PySide6.QtWidgets import (
    QApplication,
    QAbstractItemView,
    QCheckBox,
    QComboBox,
    QDialog,
    QFrame,
    QLabel,
    QLineEdit,
    QListView,
    QMenu,
    QMenuBar,
    QPushButton,
    QRadioButton,
    QScrollArea,
    QScrollBar,
    QSlider,
    QStackedWidget,
    QStyledItemDelegate,
    QTableView,
    QToolButton,
    QTreeView,
    QWidget,
    QWidgetAction,
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

    def tearDown(self):
        QApplication.processEvents()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QApplication.processEvents()
        gc.collect()

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
        self.assertTrue(
            issubclass(fluentqt.CalendarDatePicker, fluentqt.Button)
        )
        self.assertTrue(issubclass(fluentqt.CalendarView, QWidget))
        self.assertTrue(issubclass(fluentqt.CheckBox, QCheckBox))
        self.assertTrue(issubclass(fluentqt.ColorPicker, QWidget))
        self.assertTrue(issubclass(fluentqt.ComboBox, QComboBox))
        self.assertTrue(
            issubclass(fluentqt.DropDownButton, fluentqt.Button)
        )
        self.assertTrue(
            issubclass(fluentqt.SplitButton, fluentqt.Button)
        )
        self.assertTrue(
            issubclass(
                fluentqt.ToggleSplitButton,
                fluentqt.SplitButton,
            )
        )
        self.assertTrue(issubclass(fluentqt.FluentMenu, QMenu))
        self.assertTrue(issubclass(fluentqt.CommandBar, QWidget))
        self.assertTrue(
            issubclass(
                fluentqt.CommandBarFlyout,
                native.fluent.Flyout,
            )
        )
        self.assertTrue(issubclass(fluentqt.FluentMenuBar, QMenuBar))
        self.assertTrue(
            issubclass(fluentqt.FluentMenuItem, QWidgetAction)
        )
        self.assertTrue(issubclass(fluentqt.Popup, QWidget))
        self.assertTrue(issubclass(fluentqt.Flyout, fluentqt.Popup))
        self.assertIsInstance(fluentqt.Flyout(), fluentqt.Popup)
        self.assertTrue(issubclass(fluentqt.CoachMark, QWidget))
        self.assertTrue(issubclass(fluentqt.TeachingTip, fluentqt.Popup))
        self.assertIsInstance(fluentqt.TeachingTip(), fluentqt.Popup)
        self.assertTrue(issubclass(fluentqt.Dialog, QDialog))
        self.assertTrue(
            issubclass(fluentqt.ContentDialog, fluentqt.Dialog)
        )
        self.assertIsInstance(fluentqt.ContentDialog(), fluentqt.Dialog)
        self.assertTrue(issubclass(fluentqt.DatePicker, fluentqt.Button))
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
        self.assertTrue(
            issubclass(fluentqt.AutoSuggestBox, fluentqt.LineEdit)
        )
        self.assertTrue(issubclass(fluentqt.NumberBox, QLineEdit))
        self.assertTrue(issubclass(fluentqt.PasswordBox, QLineEdit))
        self.assertTrue(issubclass(fluentqt.Pivot, QWidget))
        self.assertTrue(issubclass(fluentqt.TextEdit, QWidget))
        self.assertTrue(issubclass(fluentqt.TimePicker, fluentqt.Button))
        self.assertTrue(issubclass(fluentqt.InfoBadge, QWidget))
        self.assertTrue(issubclass(fluentqt.InfoBar, QWidget))
        self.assertTrue(issubclass(fluentqt.ProgressBar, QWidget))
        self.assertTrue(issubclass(fluentqt.ProgressRing, QWidget))
        self.assertTrue(issubclass(fluentqt.Shimmer, QWidget))
        self.assertTrue(issubclass(fluentqt.Toast, QWidget))
        self.assertTrue(issubclass(fluentqt.ToolTip, QWidget))
        self.assertTrue(issubclass(fluentqt.Card, QFrame))
        self.assertTrue(issubclass(fluentqt.Divider, QWidget))
        self.assertTrue(
            issubclass(fluentqt.Expander, fluentqt.Card)
        )
        self.assertTrue(issubclass(fluentqt.Field, QWidget))
        self.assertTrue(issubclass(fluentqt.FontIcon, QWidget))
        self.assertTrue(
            issubclass(fluentqt.AnnotatedScrollBar, QWidget)
        )
        self.assertTrue(issubclass(fluentqt.PipsPager, QWidget))
        self.assertTrue(issubclass(fluentqt.ScrollBar, QScrollBar))
        self.assertTrue(issubclass(fluentqt.ScrollView, QScrollArea))
        self.assertTrue(
            issubclass(fluentqt.ScrollViewZoomAwareWidget, QWidget)
        )
        self.assertTrue(issubclass(fluentqt.SelectorBar, QWidget))
        self.assertTrue(issubclass(fluentqt.FlipView, QWidget))
        self.assertTrue(issubclass(fluentqt.DataGrid, QTableView))
        self.assertTrue(issubclass(fluentqt.FlowView, QAbstractItemView))
        self.assertTrue(issubclass(fluentqt.GridView, QListView))
        self.assertTrue(issubclass(fluentqt.ListView, QListView))
        self.assertTrue(issubclass(fluentqt.NavigationView, QWidget))
        self.assertTrue(issubclass(fluentqt.SplitView, QWidget))
        self.assertTrue(issubclass(fluentqt.StackContentHost, QWidget))
        self.assertTrue(issubclass(fluentqt.StackView, QStackedWidget))
        self.assertTrue(issubclass(fluentqt.TreeView, QTreeView))
        self.assertTrue(issubclass(fluentqt.TabView, QWidget))
        self.assertTrue(issubclass(fluentqt.TitleBar, QWidget))
        self.assertTrue(issubclass(fluentqt.Window, QWidget))
        self.assertIs(
            date_time.CalendarDatePicker,
            fluentqt.CalendarDatePicker,
        )
        self.assertIs(date_time.CalendarView, fluentqt.CalendarView)
        self.assertIs(date_time.DatePicker, fluentqt.DatePicker)
        self.assertIs(date_time.TimePicker, fluentqt.TimePicker)
        self.assertIs(basicinput.ColorPicker, fluentqt.ColorPicker)
        self.assertIs(basicinput.ComboBox, fluentqt.ComboBox)
        self.assertIs(basicinput.CompoundButton, fluentqt.CompoundButton)
        self.assertIs(
            basicinput.DropDownButton,
            fluentqt.DropDownButton,
        )
        self.assertIs(basicinput.HyperlinkButton, fluentqt.HyperlinkButton)
        self.assertIs(basicinput.RatingControl, fluentqt.RatingControl)
        self.assertIs(basicinput.SplitButton, fluentqt.SplitButton)
        self.assertIs(
            basicinput.ToggleSplitButton,
            fluentqt.ToggleSplitButton,
        )
        self.assertIs(basicinput.ToggleSwitch, fluentqt.ToggleSwitch)
        self.assertIs(
            menus_toolbars.CommandBar,
            fluentqt.CommandBar,
        )
        self.assertIs(
            menus_toolbars.CommandBarFlyout,
            fluentqt.CommandBarFlyout,
        )
        self.assertIs(
            menus_toolbars.FluentMenu,
            fluentqt.FluentMenu,
        )
        self.assertIs(
            menus_toolbars.FluentMenuBar,
            fluentqt.FluentMenuBar,
        )
        self.assertIs(
            menus_toolbars.FluentMenuItem,
            fluentqt.FluentMenuItem,
        )
        self.assertIs(layout.Card, fluentqt.Card)
        self.assertIs(layout.Divider, fluentqt.Divider)
        self.assertIs(layout.Expander, fluentqt.Expander)
        self.assertIs(layout.Field, fluentqt.Field)
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
        self.assertIs(
            scrolling.ScrollViewZoomAwareWidget,
            fluentqt.ScrollViewZoomAwareWidget,
        )
        self.assertIs(collections.FlowView, fluentqt.FlowView)
        self.assertIs(collections.DataGrid, fluentqt.DataGrid)
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
        self.assertIs(status_info.Toast, fluentqt.Toast)
        self.assertIs(status_info.ToolTip, fluentqt.ToolTip)
        self.assertIs(
            textfields.AutoSuggestBox,
            fluentqt.AutoSuggestBox,
        )
        self.assertIs(
            textfields.EditingCommandRouter,
            fluentqt.EditingCommandRouter,
        )
        self.assertIs(textfields.NumberBox, fluentqt.NumberBox)
        self.assertIs(textfields.TextEdit, fluentqt.TextEdit)
        self.assertIs(dialogs_flyouts.Flyout, fluentqt.Flyout)
        self.assertIs(dialogs_flyouts.Popup, fluentqt.Popup)
        self.assertIs(dialogs_flyouts.Dialog, fluentqt.Dialog)
        self.assertIs(
            dialogs_flyouts.ContentDialog,
            fluentqt.ContentDialog,
        )
        self.assertIs(
            dialogs_flyouts.ContentDialogButton,
            fluentqt.ContentDialogButton,
        )
        self.assertIs(windowing.TitleBar, fluentqt.TitleBar)
        self.assertIs(windowing.Window, fluentqt.Window)
        self.assertIs(foundation.FontIcon, fluentqt.FontIcon)
        self.assertIs(foundation.Theme, fluentqt.Theme)
        self.assertIs(native.fluent.Avatar, fluentqt.Avatar)
        self.assertIs(native.fluent.Button, fluentqt.Button)
        self.assertIs(
            native.fluent.CalendarDatePicker,
            fluentqt.CalendarDatePicker,
        )
        self.assertIs(native.fluent.CalendarView, fluentqt.CalendarView)
        self.assertIs(native.fluent.DatePicker, fluentqt.DatePicker)
        self.assertIs(native.fluent.TimePicker, fluentqt.TimePicker)
        self.assertIs(
            native.fluent.AutoSuggestBox,
            fluentqt.AutoSuggestBox,
        )
        self.assertNotIn("SuggestionListPopup", dir(native.fluent))
        self.assertNotIn("AutoSuggestItemDelegate", dir(native.fluent))
        self.assertIs(native.fluent.ColorPicker, fluentqt.ColorPicker)
        self.assertTrue(
            issubclass(fluentqt.ComboBox, native.fluent.ComboBox)
        )
        self.assertIsNot(native.fluent.ComboBox, fluentqt.ComboBox)
        self.assertNotIn("ComboBoxItemDelegate", dir(native.fluent))
        self.assertIs(native.fluent.CompoundButton, fluentqt.CompoundButton)
        self.assertIs(native.fluent.FontIcon, fluentqt.FontIcon)
        self.assertIs(
            native.fluent.RatingControl,
            fluentqt.RatingControl,
        )
        self.assertIs(native.fluent.PipsPager, fluentqt.PipsPager)
        self.assertTrue(
            issubclass(
                fluentqt.AnnotatedScrollBar,
                native.fluent.AnnotatedScrollBar,
            )
        )
        self.assertIsNot(
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
            issubclass(fluentqt.DrawerView, native.fluent.DrawerView)
        )
        self.assertIsNot(native.fluent.DrawerView, fluentqt.DrawerView)
        self.assertNotIn(
            "setContentWidget",
            native.fluent.DrawerView.__dict__,
        )
        self.assertTrue(
            hasattr(
                native.fluent.DrawerView,
                "_setContentWidgetWithOwnership",
            )
        )
        self.assertTrue(issubclass(fluentqt.Popup, native.fluent.Popup))
        self.assertIsNot(native.fluent.Popup, fluentqt.Popup)
        for method_name in (
            "setPosition",
            "setThemeSource",
            "addLightDismissPassthrough",
            "clearLightDismissPassthrough",
        ):
            self.assertNotIn(method_name, native.fluent.Popup.__dict__)
            self.assertTrue(hasattr(fluentqt.Popup, method_name))
        for method_name in (
            "_setPositionWithAnchor",
            "_setThemeSource",
            "_addLightDismissPassthrough",
            "_clearLightDismissPassthrough",
        ):
            self.assertTrue(hasattr(native.fluent.Popup, method_name))
        for method_name in (
            "computePosition",
            "automaticPositionAnchor",
            "setFocusOnOpenEnabled",
        ):
            self.assertFalse(hasattr(fluentqt.Popup, method_name))
        self.assertTrue(issubclass(fluentqt.Flyout, native.fluent.Flyout))
        self.assertIsNot(native.fluent.Flyout, fluentqt.Flyout)
        for method_name in ("setAnchor", "showAt"):
            self.assertNotIn(method_name, native.fluent.Flyout.__dict__)
            self.assertIn(method_name, fluentqt.Flyout.__dict__)
        for method_name in ("_setAnchor", "_showAt"):
            self.assertTrue(hasattr(native.fluent.Flyout, method_name))
        for method_name in ("computePosition", "automaticPositionAnchor"):
            self.assertFalse(hasattr(fluentqt.Flyout, method_name))
        self.assertTrue(issubclass(fluentqt.Dialog, native.fluent.Dialog))
        self.assertIsNot(native.fluent.Dialog, fluentqt.Dialog)
        self.assertNotIn("setThemeSource", native.fluent.Dialog.__dict__)
        self.assertTrue(hasattr(native.fluent.Dialog, "_setThemeSource"))
        self.assertTrue(hasattr(fluentqt.Dialog, "setThemeSource"))
        for method_name in (
            "onThemeUpdated",
            "isAnimating",
            "ownerWidget",
            "drawShadow",
        ):
            self.assertFalse(hasattr(fluentqt.Dialog, method_name))
        self.assertTrue(
            issubclass(
                fluentqt.ContentDialog,
                native.fluent.ContentDialog,
            )
        )
        self.assertIsNot(
            native.fluent.ContentDialog,
            fluentqt.ContentDialog,
        )
        self.assertNotIn(
            "setContent",
            native.fluent.ContentDialog.__dict__,
        )
        self.assertTrue(
            hasattr(native.fluent.ContentDialog, "_setContent")
        )
        self.assertIn("setContent", fluentqt.ContentDialog.__dict__)
        self.assertIn("takeContent", fluentqt.ContentDialog.__dict__)
        self.assertFalse(
            hasattr(native.fluent.ContentDialog, "ResultPrimary")
        )
        self.assertEqual(fluentqt.ContentDialog.ResultNone, 0)
        self.assertEqual(fluentqt.ContentDialog.ResultPrimary, 1)
        self.assertEqual(fluentqt.ContentDialog.ResultSecondary, 2)
        self.assertTrue(
            issubclass(fluentqt.FlowView, native.fluent.FlowView)
        )
        self.assertTrue(
            issubclass(fluentqt.DataGrid, native.fluent.DataGrid)
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
        self.assertTrue(issubclass(fluentqt.Toast, native.fluent.Toast))
        self.assertIsNot(native.fluent.Toast, fluentqt.Toast)
        self.assertNotIn("present", native.fluent.Toast.__dict__)
        self.assertTrue(hasattr(native.fluent.Toast, "_present"))
        self.assertNotIn("showToast", native.fluent.Toast.__dict__)
        self.assertNotIn("showOrUpdateToast", native.fluent.Toast.__dict__)
        self.assertTrue(hasattr(native, "showToastForBinding"))
        self.assertTrue(hasattr(native, "showOrUpdateToastForBinding"))
        self.assertIs(native.fluent.ToolTip, fluentqt.ToolTip)
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
        self.assertTrue(issubclass(fluentqt.Field, native.fluent.Field))
        self.assertIsNot(native.fluent.Field, fluentqt.Field)
        self.assertTrue(
            hasattr(native.fluent.Field, "_setEditorWithOwnership")
        )
        self.assertFalse(hasattr(native.fluent.Field, "onThemeUpdated"))
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
        self.assertIs(
            native.fluent.windowing.TitleBar,
            fluentqt.TitleBar,
        )
        self.assertIs(native.fluent.windowing.Window, fluentqt.Window)

        info = fluentqt.binding_build_info()
        self.assertEqual(
            fluentqt.__version__,
            os.environ["FLUENTQT_EXPECTED_VERSION"],
        )
        self.assertEqual(
            fluentqt.__api_version__,
            ".".join(fluentqt.__version__.split(".")[:2]),
        )
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
        for picker_type in (
            fluentqt.CalendarDatePicker,
            fluentqt.DatePicker,
            fluentqt.TimePicker,
        ):
            self.assertFalse(hasattr(picker_type, "anchors"))
            self.assertFalse(hasattr(picker_type, "bind"))
            self.assertFalse(hasattr(picker_type, "setState"))
        self.assertFalse(hasattr(fluentqt.ColorPicker, "anchors"))
        self.assertFalse(hasattr(fluentqt.ColorPicker, "bind"))
        self.assertFalse(hasattr(fluentqt.ColorPicker, "setState"))
        self.assertFalse(hasattr(fluentqt.ComboBox, "anchors"))
        self.assertFalse(hasattr(fluentqt.ComboBox, "bind"))
        self.assertFalse(hasattr(fluentqt.ComboBox, "setState"))
        self.assertTrue(hasattr(fluentqt.ComboBox, "pressProgress"))
        self.assertTrue(hasattr(fluentqt.ComboBox, "setPressProgress"))
        self.assertFalse(hasattr(fluentqt.ComboBox, "onThemeUpdated"))
        for command_surface_type in (
            fluentqt.CommandBar,
            fluentqt.CommandBarFlyout,
            fluentqt.FluentMenuBar,
        ):
            self.assertFalse(hasattr(command_surface_type, "anchors"))
            self.assertFalse(hasattr(command_surface_type, "bind"))
            self.assertFalse(hasattr(command_surface_type, "setState"))
            self.assertFalse(
                hasattr(command_surface_type, "onThemeUpdated")
            )
        self.assertFalse(hasattr(fluentqt.LineEdit, "onThemeUpdated"))
        self.assertFalse(hasattr(fluentqt.AutoSuggestBox, "anchors"))
        self.assertFalse(hasattr(fluentqt.AutoSuggestBox, "bind"))
        self.assertFalse(hasattr(fluentqt.AutoSuggestBox, "setState"))
        self.assertFalse(
            hasattr(fluentqt.AutoSuggestBox, "onThemeUpdated")
        )
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
        self.assertFalse(hasattr(fluentqt.DrawerView, "anchors"))
        self.assertFalse(hasattr(fluentqt.DrawerView, "bind"))
        self.assertFalse(hasattr(fluentqt.DrawerView, "setState"))
        self.assertFalse(hasattr(fluentqt.Popup, "anchors"))
        self.assertFalse(hasattr(fluentqt.Popup, "bind"))
        self.assertFalse(hasattr(fluentqt.Popup, "setState"))
        self.assertFalse(hasattr(fluentqt.Flyout, "anchors"))
        self.assertFalse(hasattr(fluentqt.Flyout, "bind"))
        self.assertFalse(hasattr(fluentqt.Flyout, "setState"))
        for dialog_type in (fluentqt.Dialog, fluentqt.ContentDialog):
            self.assertFalse(hasattr(dialog_type, "anchors"))
            self.assertFalse(hasattr(dialog_type, "bind"))
            self.assertFalse(hasattr(dialog_type, "setState"))
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
        self.assertTrue(hasattr(fluentqt.Shimmer, "elements"))
        self.assertTrue(hasattr(fluentqt.Shimmer, "setElements"))
        self.assertTrue(hasattr(fluentqt.Shimmer, "clearElements"))
        self.assertFalse(hasattr(fluentqt.InfoBar, "anchors"))
        self.assertFalse(hasattr(fluentqt.InfoBar, "bind"))
        self.assertFalse(hasattr(fluentqt.InfoBar, "setState"))
        for status_overlay_type in (fluentqt.Toast, fluentqt.ToolTip):
            self.assertFalse(hasattr(status_overlay_type, "anchors"))
            self.assertFalse(hasattr(status_overlay_type, "bind"))
            self.assertFalse(hasattr(status_overlay_type, "setState"))
            self.assertFalse(
                hasattr(status_overlay_type, "onThemeUpdated")
            )
        self.assertFalse(hasattr(fluentqt.Toast, "toastProgress"))
        self.assertFalse(hasattr(fluentqt.Toast, "setToastProgress"))
        self.assertFalse(hasattr(fluentqt.TextEdit, "anchors"))
        self.assertFalse(hasattr(fluentqt.TextEdit, "bind"))
        self.assertFalse(hasattr(fluentqt.TextEdit, "setState"))
        self.assertFalse(hasattr(fluentqt.ScrollView, "anchors"))
        self.assertFalse(hasattr(fluentqt.ScrollView, "bind"))
        self.assertFalse(hasattr(fluentqt.ScrollView, "setState"))
        for window_type in (fluentqt.TitleBar, fluentqt.Window):
            self.assertFalse(hasattr(window_type, "anchors"))
            self.assertFalse(hasattr(window_type, "bind"))
            self.assertFalse(hasattr(window_type, "setState"))
            self.assertFalse(hasattr(window_type, "onThemeUpdated"))
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

        for section in ("classes", "enums", "functions", "variables"):
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

    def test_calendar_date_picker_values_popup_and_internal_calendar(self):
        host = QWidget()
        host.resize(720, 520)
        picker = fluentqt.CalendarDatePicker(host)
        picker.move(32, 32)
        picker.show()
        host.show()
        QCoreApplication.processEvents()

        minimum = QDate(2026, 5, 10)
        maximum = QDate(2026, 5, 20)
        date_changes = []
        open_changes = []
        picker.dateChanged.connect(date_changes.append)
        picker.calendarOpenChanged.connect(open_changes.append)
        picker.setPlaceholderText("Choose a date")
        self.assertEqual(picker.displayText(), "Choose a date")
        picker.setDateRange(minimum, maximum)
        picker.setDate(QDate(2026, 5, 1))
        picker.setDisplayFormat("yyyy-MM-dd")

        self.assertEqual(picker.date(), minimum)
        self.assertEqual(picker.displayText(), "2026-05-10")
        self.assertTrue(picker.isDateSelectable(maximum))
        self.assertFalse(picker.isDateSelectable(maximum.addDays(1)))

        picker.openCalendar()
        QCoreApplication.processEvents()
        self.assertTrue(picker.isCalendarOpen())
        self.assertTrue(picker.isOpen())
        calendar = picker.calendarView()
        self.assertIsInstance(calendar, fluentqt.CalendarView)
        self.assertIs(calendar, picker.calendarView())
        self.assertTrue(Shiboken.isValid(calendar))
        self.assertEqual(calendar.minDate(), minimum)
        self.assertEqual(calendar.maxDate(), maximum)
        self.assertEqual(calendar.selectedDate(), minimum)

        picker.closeCalendar()
        QCoreApplication.processEvents()
        self.assertFalse(picker.isCalendarOpen())
        picker.clearDate()
        self.assertFalse(picker.date().isValid())
        self.assertEqual(picker.displayText(), "Choose a date")
        self.assertEqual(date_changes, [minimum, QDate()])
        self.assertEqual(open_changes, [True, False])

        picker.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QCoreApplication.processEvents()
        self.assertFalse(Shiboken.isValid(picker))
        self.assertFalse(Shiboken.isValid(calendar))
        host.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QCoreApplication.processEvents()

    def test_date_picker_properties_enums_signals_and_popup(self):
        class PythonDatePicker(fluentqt.DatePicker):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.key_press_count = 0

            def keyPressEvent(self, event):
                self.key_press_count += 1
                super().keyPressEvent(event)

        host = QWidget()
        host.resize(720, 520)
        picker = PythonDatePicker(host)
        picker.move(32, 32)
        picker.show()
        host.show()
        QCoreApplication.processEvents()

        minimum = QDate(2026, 7, 10)
        maximum = QDate(2026, 7, 25)
        selected_changes = []
        open_changes = []
        picker.selectedDateChanged.connect(selected_changes.append)
        picker.dropDownOpenChanged.connect(open_changes.append)
        picker.setDateRange(minimum, maximum)
        picker.setSelectedDate(QDate(2026, 7, 1))
        picker.setMonthFormat(fluentqt.DatePicker.MonthFormat.TwoDigitMonth)
        picker.setDayFormat(fluentqt.DatePicker.DayFormat.TwoDigitDay)
        picker.setYearFormat(fluentqt.DatePicker.YearFormat.TwoDigitYear)
        picker.setPlaceholderText(
            fluentqt.DatePicker.DateField.Month,
            "month",
        )
        picker.setFieldTextAlignment(
            fluentqt.DatePicker.DateField.Month,
            Qt.AlignRight,
        )

        self.assertEqual(picker.selectedDate(), minimum)
        self.assertEqual(
            picker.fieldDisplayText(fluentqt.DatePicker.DateField.Month),
            "07",
        )
        self.assertEqual(
            picker.fieldDisplayText(fluentqt.DatePicker.DateField.Day),
            "10",
        )
        self.assertEqual(
            picker.fieldDisplayText(fluentqt.DatePicker.DateField.Year),
            "26",
        )
        self.assertEqual(
            picker.fieldTextAlignment(fluentqt.DatePicker.DateField.Month),
            Qt.AlignRight,
        )

        picker.setYearVisible(False)
        self.assertFalse(picker.yearVisible())
        picker.openPicker()
        QCoreApplication.processEvents()
        self.assertTrue(picker.isDropDownOpen())
        picker.closePicker()
        QCoreApplication.processEvents()
        self.assertFalse(picker.isOpen())
        QTest.keyClick(picker, Qt.Key_Space)
        QCoreApplication.processEvents()
        self.assertGreaterEqual(picker.key_press_count, 1)
        self.assertTrue(picker.isDropDownOpen())
        picker.closePicker()
        QCoreApplication.processEvents()
        picker.clearSelectedDate()

        self.assertFalse(picker.selectedDate().isValid())
        self.assertEqual(selected_changes, [minimum, QDate()])
        self.assertEqual(open_changes, [True, False, True, False])
        host.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QCoreApplication.processEvents()

    def test_time_picker_properties_enums_signals_and_popup(self):
        host = QWidget()
        host.resize(720, 520)
        picker = fluentqt.TimePicker(host)
        picker.move(32, 32)
        picker.show()
        host.show()
        QCoreApplication.processEvents()

        selected_changes = []
        open_changes = []
        picker.selectedTimeChanged.connect(selected_changes.append)
        picker.dropDownOpenChanged.connect(open_changes.append)
        picker.setMinuteIncrement(15)
        picker.setSelectedTime(QTime(9, 58))
        picker.setClockIdentifier(
            fluentqt.TimePicker.ClockIdentifier.TwentyFourHourClock
        )
        picker.setPlaceholderText(
            fluentqt.TimePicker.TimeField.Minute,
            "minute",
        )
        picker.setFieldTextAlignment(
            fluentqt.TimePicker.TimeField.Hour,
            Qt.AlignRight,
        )

        self.assertEqual(picker.minuteIncrement(), 15)
        self.assertEqual(picker.selectedTime(), QTime(9, 45))
        self.assertEqual(
            picker.fieldDisplayText(fluentqt.TimePicker.TimeField.Hour),
            "09",
        )
        self.assertEqual(
            picker.fieldDisplayText(fluentqt.TimePicker.TimeField.Minute),
            "45",
        )
        self.assertEqual(
            picker.fieldDisplayText(fluentqt.TimePicker.TimeField.Period),
            "",
        )
        self.assertEqual(
            picker.fieldTextAlignment(fluentqt.TimePicker.TimeField.Hour),
            Qt.AlignRight,
        )

        picker.openPicker()
        QCoreApplication.processEvents()
        self.assertTrue(picker.isDropDownOpen())
        picker.closePicker()
        QCoreApplication.processEvents()
        self.assertFalse(picker.isOpen())
        picker.clearSelectedTime()

        self.assertFalse(picker.selectedTime().isValid())
        self.assertEqual(selected_changes, [QTime(9, 45), QTime()])
        self.assertEqual(open_changes, [True, False])
        host.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QCoreApplication.processEvents()

    def test_date_time_picker_popup_lifecycle_stress(self):
        for picker_type, open_method in (
            (fluentqt.CalendarDatePicker, "openCalendar"),
            (fluentqt.DatePicker, "openPicker"),
            (fluentqt.TimePicker, "openPicker"),
        ):
            for _ in range(25):
                host = QWidget()
                host.resize(640, 480)
                picker = picker_type(host)
                picker.show()
                host.show()
                QCoreApplication.processEvents()
                getattr(picker, open_method)()
                QCoreApplication.processEvents()
                internal_calendar = (
                    picker.calendarView()
                    if isinstance(picker, fluentqt.CalendarDatePicker)
                    else None
                )
                picker_ref = weakref.ref(picker)
                picker.deleteLater()
                QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
                QCoreApplication.processEvents()
                self.assertFalse(Shiboken.isValid(picker))
                if internal_calendar is not None:
                    self.assertFalse(Shiboken.isValid(internal_calendar))
                del internal_calendar
                del picker
                host.deleteLater()
                QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
                QCoreApplication.processEvents()
                del host
                gc.collect()
                self.assertIsNone(picker_ref())

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
        ring.setAnimationEnabled(False)
        self.assertFalse(ring.isAnimationEnabled())
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

        element_changes = []
        shimmer.elementsChanged.connect(lambda: element_changes.append(True))
        elements = [
            fluentqt.Shimmer.Element(
                fluentqt.Shimmer.Shape.Circle,
                QRectF(8.0, 8.0, 40.0, 40.0),
            ),
            fluentqt.Shimmer.Element(
                fluentqt.Shimmer.Shape.RoundedRect,
                QRectF(60.0, 10.0, 150.0, 16.0),
                4.0,
            ),
        ]
        shimmer.setElements(elements)
        self.assertEqual(shimmer.elements(), elements)
        self.assertEqual(
            shimmer.shimmerTemplate(),
            fluentqt.Shimmer.ShimmerTemplate.Custom,
        )
        shimmer.clearElements()
        self.assertEqual(shimmer.elements(), [])
        self.assertEqual(element_changes, [True, True])

    def test_tool_tip_attachment_properties_and_theme_source_lifetime(self):
        host = QWidget()
        host.resize(480, 260)
        target = fluentqt.Button("Hover for details", host)
        target.move(120, 90)
        host.show()
        self.app.processEvents()

        tip = fluentqt.ToolTip.attach(
            target,
            "Native Fluent tooltip",
            fluentqt.ToolTip.Placement.Below,
        )
        self.assertIsInstance(tip, fluentqt.ToolTip)
        self.assertIs(tip.parent(), target)
        self.assertFalse(Shiboken.ownedByPython(tip))
        self.assertEqual(tip.text(), "Native Fluent tooltip")

        margin_changes = []
        animation_changes = []
        tip.marginsChanged.connect(lambda: margin_changes.append(True))
        tip.animationEnabledChanged.connect(animation_changes.append)
        tip.setMargins(QMargins(10, 7, 10, 7))
        tip.setAnimationEnabled(False)
        self.assertEqual(tip.margins(), QMargins(10, 7, 10, 7))
        self.assertEqual(margin_changes, [True])
        self.assertEqual(animation_changes, [False])
        self.assertGreater(tip.shadowMargin(), 0)

        reused = fluentqt.ToolTip.attach(target, "Updated tooltip")
        self.assertIs(reused, tip)
        self.assertEqual(tip.text(), "Updated tooltip")

        theme_source = QWidget()
        theme_source.marker = "retained-theme-source"
        source_ref = weakref.ref(theme_source)
        tip.setThemeSource(theme_source)
        del theme_source
        gc.collect()
        self.assertIsNotNone(source_ref())
        self.assertEqual(source_ref().marker, "retained-theme-source")

        source_ref().deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(source_ref()))
        tip.setThemeSource(None)
        gc.collect()
        self.assertIsNone(source_ref())

        target.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(tip))
        host.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)

    def test_status_overlay_python_virtual_dispatch(self):
        class PythonToast(fluentqt.Toast):
            def __init__(self):
                super().__init__()
                self.size_hint_calls = 0

            def sizeHint(self):
                self.size_hint_calls += 1
                return QSize(333, 111)

        class PythonToolTip(fluentqt.ToolTip):
            def __init__(self):
                super().__init__()
                self.filtered_events = 0

            def eventFilter(self, watched, event):
                if event.type() == QEvent.User:
                    self.filtered_events += 1
                return super().eventFilter(watched, event)

        toast = PythonToast()
        host = QWidget()
        anchor = QWidget(host)
        host.resize(500, 300)
        toast.setAnimationEnabled(False)
        self.assertTrue(toast.present(anchor))
        self.assertGreater(toast.size_hint_calls, 0)

        watched = QWidget()
        tip = PythonToolTip()
        watched.installEventFilter(tip)
        QCoreApplication.sendEvent(watched, QEvent(QEvent.User))
        self.assertEqual(tip.filtered_events, 1)
        host.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)

    def test_toast_properties_action_and_direct_hosting(self):
        host = QWidget()
        host.resize(560, 320)
        anchor = fluentqt.Button("Save", host)
        anchor.move(24, 24)
        host.show()
        self.app.processEvents()

        toast = fluentqt.Toast()
        title_changes = []
        message_changes = []
        severity_changes = []
        open_changes = []
        presented = []
        dismissed = []
        dismiss_reasons = []
        action_changes = []
        toast.titleChanged.connect(title_changes.append)
        toast.messageChanged.connect(message_changes.append)
        toast.severityChanged.connect(severity_changes.append)
        toast.isOpenChanged.connect(open_changes.append)
        toast.presented.connect(lambda: presented.append(True))
        toast.dismissed.connect(lambda: dismissed.append(True))
        toast.dismissedWithReason.connect(dismiss_reasons.append)
        toast.actionChanged.connect(
            lambda value: action_changes.append(value is None)
        )

        toast.setTitle("Saved")
        toast.setMessage("The document is ready.")
        toast.setSeverity(fluentqt.Toast.Severity.Success)
        toast.setPlacement(fluentqt.Toast.Placement.BottomEnd)
        toast.setPlacementMargins(QMargins(20, 18, 20, 18))
        toast.setDuration(-1)
        toast.setAnimationEnabled(False)
        toast.setPauseOnHoverEnabled(True)
        toast.setUpdateKey("save-result")
        self.assertEqual(title_changes, ["Saved"])
        self.assertEqual(message_changes, ["The document is ready."])
        self.assertEqual(severity_changes, [fluentqt.Toast.Severity.Success])
        self.assertEqual(toast.duration(), 0)
        self.assertTrue(toast.isPauseOnHoverEnabled())
        self.assertEqual(toast.updateKey(), "save-result")

        action = QAction("Open folder")
        action.marker = "python-action"
        action_ref = weakref.ref(action)
        toast.setAction(action)
        del action
        gc.collect()
        self.assertIs(toast.action(), action_ref())
        self.assertEqual(toast.action().marker, "python-action")
        self.assertEqual(action_changes, [False])

        self.assertTrue(toast.present(anchor))
        self.assertIs(toast.parentWidget(), host)
        self.assertFalse(Shiboken.ownedByPython(toast))
        self.assertTrue(toast.isOpen())
        self.assertEqual(open_changes, [True])
        self.assertEqual(presented, [True])

        # Toast is hosted by anchor.window(), not by the transient child
        # anchor. Destroying that child must not invalidate the toast wrapper.
        anchor.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertTrue(Shiboken.isValid(toast))
        self.assertTrue(toast.isOpen())
        self.assertIs(toast.parentWidget(), host)

        toast.dismiss()
        self.assertFalse(toast.isOpen())
        self.assertEqual(open_changes, [True, False])
        self.assertEqual(dismissed, [True])
        self.assertEqual(
            dismiss_reasons,
            [fluentqt.Toast.DismissReason.Programmatic],
        )
        self.assertTrue(Shiboken.isValid(toast))

        host.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(toast))
        gc.collect()
        self.assertIsNone(action_ref())

    def test_managed_toast_update_eviction_and_self_deletion(self):
        host = QWidget()
        host.resize(560, 320)
        anchor = QWidget(host)
        host.show()
        self.app.processEvents()

        previous_maximum = fluentqt.Toast.maximumVisible()
        try:
            fluentqt.Toast.setMaximumVisible(1)
            first = fluentqt.Toast.showOrUpdateToast(
                anchor,
                "sync-job",
                "Preparing",
                durationMs=0,
                placement=fluentqt.Toast.Placement.TopEnd,
            )
            self.assertIsInstance(first, fluentqt.Toast)
            self.assertIs(first.parentWidget(), host)
            self.assertFalse(Shiboken.ownedByPython(first))
            updated = []
            evicted = []
            first.updated.connect(lambda: updated.append(True))
            first.dismissedWithReason.connect(evicted.append)

            second_reference = fluentqt.Toast.showOrUpdateToast(
                anchor,
                "sync-job",
                "Complete",
                severity=fluentqt.Toast.Severity.Success,
                durationMs=0,
                placement=fluentqt.Toast.Placement.TopEnd,
            )
            self.assertIs(second_reference, first)
            self.assertEqual(first.message(), "Complete")
            self.assertEqual(first.severity(), fluentqt.Toast.Severity.Success)
            self.assertEqual(updated, [True])

            replacement_anchor = QWidget(host)
            anchor.deleteLater()
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
            self.app.processEvents()
            self.assertTrue(Shiboken.isValid(first))
            self.assertIs(first.parentWidget(), host)

            replacement = fluentqt.Toast.showToast(
                replacement_anchor,
                "New notification",
                durationMs=0,
                placement=fluentqt.Toast.Placement.TopEnd,
            )
            self.assertIsInstance(replacement, fluentqt.Toast)
            self.assertEqual(
                evicted,
                [fluentqt.Toast.DismissReason.Evicted],
            )
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
            self.app.processEvents()
            self.assertFalse(Shiboken.isValid(first))

            replacement.setAnimationEnabled(False)
            replacement.dismiss()
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
            self.app.processEvents()
            self.assertFalse(Shiboken.isValid(replacement))
        finally:
            fluentqt.Toast.setMaximumVisible(previous_maximum)
            host.deleteLater()
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)

    def test_status_overlay_lifetime_gc_stress(self):
        for cycle in range(25):
            host = QWidget()
            host.resize(420, 240)
            anchor = QWidget(host)
            theme_source = QWidget()
            action = QAction("Action {0}".format(cycle))
            tip = fluentqt.ToolTip.attach(anchor, "Tip {0}".format(cycle))
            tip.setThemeSource(theme_source)
            managed = fluentqt.Toast.showToast(
                anchor,
                "Toast {0}".format(cycle),
                durationMs=0,
            )
            managed.setAction(action)
            managed.setAnimationEnabled(False)

            tip_ref = weakref.ref(tip)
            toast_ref = weakref.ref(managed)
            source_ref = weakref.ref(theme_source)
            action_ref = weakref.ref(action)
            del tip
            del managed
            del theme_source
            del action
            gc.collect()
            self.assertIsNotNone(tip_ref())
            self.assertIsNotNone(toast_ref())
            self.assertIsNotNone(source_ref())
            self.assertIsNotNone(action_ref())

            toast_ref().dismiss()
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
            anchor.deleteLater()
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
            host.deleteLater()
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
            self.app.processEvents()
            gc.collect()
            self.assertIsNone(tip_ref())
            self.assertIsNone(toast_ref())
            self.assertIsNone(source_ref())
            self.assertIsNone(action_ref())

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
        self.assertFalse(bar.hasDetailLabelProvider())
        provider_offsets = []
        bar.setDetailLabelProvider(
            lambda offset: provider_offsets.append(offset)
            or "Offset {0}".format(offset)
        )
        self.assertTrue(bar.hasDetailLabelProvider())

        bar.show()
        self.app.processEvents()
        QTest.mouseMove(bar, QPoint(12, 19))
        self.app.processEvents()
        self.assertTrue(bar.isDetailLabelVisible())
        self.assertEqual(bar.detailLabelText(), "Offset 0")
        self.assertIn(0, provider_offsets)
        self.assertTrue(detail_requests)
        bar.hide()

        bar.clearDetailLabelProvider()
        self.assertFalse(bar.hasDetailLabelProvider())

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
        self.assertGreaterEqual(
            meta_object.indexOfProperty("selectedVisualOffset"),
            0,
        )
        self.assertGreaterEqual(
            meta_object.indexOfProperty("visibleWindowOffset"),
            0,
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

    def test_inspector_report_and_options(self):
        root = QWidget()
        root.setObjectName("bindingInspectorRoot")
        root.resize(320, 180)
        button = QToolButton(root)
        button.setObjectName("smallUnnamedAction")
        button.setGeometry(12, 12, 18, 18)
        root.show()
        QApplication.processEvents()

        report = fluentqt.inspect_widget(root)
        self.assertEqual(report["schema_version"], 1)
        self.assertEqual(report["tool"], "FluentQt Inspector")
        codes = {finding["code"] for finding in report["findings"]}
        self.assertIn("accessibility.missing-name", codes)
        self.assertIn("input.small-hit-area", codes)

        quiet_report = fluentqt.inspect_widget(
            root,
            check_clipped_text=False,
            check_accessibility_names=False,
            check_hit_areas=False,
            check_focus_order=False,
            check_duplicate_actions=False,
            check_nested_scrolling=False,
        )
        self.assertEqual(quiet_report["summary"]["findings"], 0)
        with self.assertRaises(TypeError):
            fluentqt.inspect_widget(object())
        with self.assertRaises(TypeError):
            fluentqt.inspect_widget(root, minimum_hit_area=(24.5, 24))
        with self.assertRaises(TypeError):
            fluentqt.inspect_widget(root, spacing_grid="4")
        with self.assertRaises(ValueError):
            fluentqt.inspect_widget(root, spacing_grid=0)
        root.close()

    def test_theme_api(self):
        previous_theme = fluentqt.current_theme()
        try:
            fluentqt.reset_theme_tokens()

            fluentqt.set_theme(fluentqt.Theme.Dark)
            self.assertEqual(fluentqt.current_theme(), fluentqt.Theme.Dark)

            fluentqt.apply_user_theme()
            applied_scale = fluentqt.font_scale()
            temporary_scale = 1.25 if abs(applied_scale - 1.25) > 0.01 else 1.5
            fluentqt.set_font_scale(temporary_scale)
            initial_revision = fluentqt.theme_revision()
            fluentqt.apply_user_theme()
            self.assertGreater(fluentqt.theme_revision(), initial_revision)
            self.assertAlmostEqual(fluentqt.font_scale(), applied_scale)

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
        # PySide may omit generated method docstrings for limited-API Python
        # builds. The generated-code verifier checks pointer privacy; this
        # runtime probe still proves the public two-argument tuple contract.
        native_event_doc = fluentqt.Window.nativeEvent.__doc__ or ""
        self.assertNotIn("result", native_event_doc)

        class NativeEventWindow(fluentqt.Window):
            def nativeEvent(self, event_type, message):
                return False, 0

        window = NativeEventWindow()
        self.assertEqual(
            window.nativeEvent(QByteArray(b"fluentqt-test"), 0),
            (False, 0),
        )

    def test_backdrop_value_types_and_window_state(self):
        capabilities = fluentqt.BackdropCapabilities()
        self.assertTrue(
            capabilities.supportsNative(fluentqt.BackdropEffect.Solid)
        )
        self.assertFalse(
            capabilities.supportsTransparentMaterial(
                fluentqt.BackdropEffect.Mica
            )
        )
        capabilities.alphaSurfaceSupported = True
        capabilities.nativeMica = True
        capabilities.provider = "python-test"
        self.assertTrue(
            capabilities.supportsNative(fluentqt.BackdropEffect.Mica)
        )
        self.assertTrue(
            capabilities.supportsTransparentMaterial(
                fluentqt.BackdropEffect.Mica
            )
        )

        copied_capabilities = fluentqt.BackdropCapabilities(capabilities)
        self.assertTrue(copied_capabilities.alphaSurfaceSupported)
        self.assertTrue(copied_capabilities.nativeMica)
        self.assertEqual(copied_capabilities.provider, "python-test")

        state = fluentqt.BackdropState()
        state.requestedEffect = fluentqt.BackdropEffect.Acrylic
        state.effectiveEffect = fluentqt.BackdropEffect.Acrylic
        state.backend = fluentqt.BackdropBackend.PaintedMaterial
        state.fidelity = fluentqt.BackdropFidelity.Emulated
        state.surfaceMode = fluentqt.BackdropSurfaceMode.PaintedOpaque
        state.platformApplied = False
        state.reason = "python-test"
        self.assertEqual(fluentqt.BackdropState(state), state)

        window = fluentqt.Window()
        effect_changes = []
        state_changes = []
        window.backdropEffectChanged.connect(effect_changes.append)
        window.backdropStateChanged.connect(state_changes.append)

        window.setBackdropEffect(fluentqt.BackdropEffect.Solid)
        solid = window.backdropState()
        self.assertEqual(solid.requestedEffect, fluentqt.BackdropEffect.Solid)
        self.assertEqual(solid.effectiveEffect, fluentqt.BackdropEffect.Solid)
        self.assertEqual(solid.backend, fluentqt.BackdropBackend.Solid)
        self.assertEqual(solid.fidelity, fluentqt.BackdropFidelity.Solid)
        self.assertEqual(
            solid.surfaceMode,
            fluentqt.BackdropSurfaceMode.SolidOpaque,
        )
        self.assertFalse(solid.platformApplied)

        window.setBackdropEffect(fluentqt.BackdropEffect.Mica)
        mica = window.backdropState()
        self.assertEqual(mica.requestedEffect, fluentqt.BackdropEffect.Mica)
        self.assertEqual(mica.effectiveEffect, fluentqt.BackdropEffect.Mica)
        self.assertEqual(
            mica.backend,
            fluentqt.BackdropBackend.PaintedMaterial,
        )
        self.assertEqual(mica.fidelity, fluentqt.BackdropFidelity.Emulated)
        self.assertEqual(
            mica.surfaceMode,
            fluentqt.BackdropSurfaceMode.PaintedOpaque,
        )
        self.assertFalse(mica.platformApplied)
        self.assertTrue(mica.reason)
        self.assertEqual(
            effect_changes,
            [fluentqt.BackdropEffect.Solid, fluentqt.BackdropEffect.Mica],
        )
        self.assertGreaterEqual(len(state_changes), 2)

    def test_title_bar_properties_signals_and_virtual_dispatch(self):
        class EventTitleBar(fluentqt.TitleBar):
            def __init__(self):
                super().__init__()
                self.user_events = 0

            def event(self, event):
                if event.type() == QEvent.User:
                    self.user_events += 1
                return super().event(event)

        title_bar = EventTitleBar()
        self.assertIs(title_bar.contentHost(), title_bar)
        self.assertEqual(
            title_bar.titleBarHeight(),
            fluentqt.TitleBar.defaultTitleBarHeight(),
        )

        leading_changes = []
        trailing_changes = []
        height_changes = []
        active_changes = []
        title_bar.systemReservedLeadingWidthChanged.connect(
            leading_changes.append
        )
        title_bar.systemReservedTrailingWidthChanged.connect(
            trailing_changes.append
        )
        title_bar.titleBarHeightChanged.connect(height_changes.append)
        title_bar.windowActiveChanged.connect(active_changes.append)

        title_bar.setSystemReservedLeadingWidth(24)
        title_bar.setSystemReservedLeadingWidth(24)
        title_bar.setSystemReservedTrailingWidth(48)
        title_bar.setTitleBarHeight(44)
        self.assertEqual(title_bar.systemReservedLeadingWidth(), 24)
        self.assertEqual(title_bar.systemReservedTrailingWidth(), 48)
        self.assertEqual(title_bar.titleBarHeight(), 44)
        self.assertEqual(leading_changes, [24])
        self.assertEqual(trailing_changes, [48])
        self.assertEqual(height_changes, [44])

        QCoreApplication.sendEvent(title_bar, QEvent(QEvent.WindowActivate))
        QCoreApplication.sendEvent(title_bar, QEvent(QEvent.WindowDeactivate))
        self.assertEqual(active_changes, [True, False])
        QCoreApplication.sendEvent(title_bar, QEvent(QEvent.User))
        self.assertEqual(title_bar.user_events, 1)
        self.assertIsInstance(title_bar.dragExclusionRects(), list)

    def test_title_bar_content_and_window_lifetime(self):
        title_bar = fluentqt.TitleBar()
        first = QWidget()
        second = QWidget()
        content_changes = []
        title_bar.contentWidgetChanged.connect(content_changes.append)

        title_bar.setContentWidget(first)
        self.assertIs(title_bar.contentWidget(), first)
        self.assertIs(first.parent(), title_bar)

        title_bar.setContentWidget(second)
        self.assertIsNone(first.parent())
        self.assertTrue(Shiboken.isValid(first))
        self.assertIs(title_bar.contentWidget(), second)
        self.assertIs(second.parent(), title_bar)

        title_bar.setContentWidget(None)
        self.assertIsNone(second.parent())
        self.assertTrue(Shiboken.isValid(second))
        self.assertEqual(content_changes, [first, second, None])

        owned_title_bar = fluentqt.TitleBar()
        owned_content = QWidget()
        owned_title_bar.setContentWidget(owned_content)
        owned_title_bar_ref = weakref.ref(owned_title_bar)
        del owned_title_bar
        gc.collect()
        self.assertIsNone(owned_title_bar_ref())
        self.assertFalse(Shiboken.isValid(owned_content))

        window = fluentqt.Window()
        window_title_bar = window.titleBar()
        self.assertIs(window.titleBar(), window_title_bar)
        self.assertIs(window_title_bar.window(), window)
        self.assertFalse(Shiboken.ownedByPython(window_title_bar))
        self.assertIsNotNone(window.contentHost())
        window.setChromeInteractive(False)
        self.assertFalse(window.isChromeInteractive())
        window.setChromeInteractive(True)
        self.assertTrue(window.isChromeInteractive())
        window.resize(640, 420)
        self.app.processEvents()
        self.assertTrue(window.chromeFrameRect().isValid())

        window_ref = weakref.ref(window)
        del window
        gc.collect()
        self.assertIsNone(window_ref())
        self.assertFalse(Shiboken.isValid(window_title_bar))

    def test_title_bar_content_gc_stress(self):
        for _ in range(25):
            title_bar = fluentqt.TitleBar()
            content = QWidget()
            title_bar.setContentWidget(content)
            content_ref = weakref.ref(content)
            del content
            gc.collect()
            self.assertIs(title_bar.contentWidget(), content_ref())
            self.assertTrue(Shiboken.isValid(content_ref()))

            title_bar_ref = weakref.ref(title_bar)
            del title_bar
            self.app.processEvents()
            gc.collect()
            self.assertIsNone(title_bar_ref())
            self.assertIsNone(content_ref())

    def test_window_title_bar_gc_stress(self):
        for _ in range(25):
            window = fluentqt.Window()
            title_bar = window.titleBar()
            title_bar_ref = weakref.ref(title_bar)
            window_ref = weakref.ref(window)
            del window
            self.app.processEvents()
            gc.collect()
            self.assertIsNone(window_ref())
            self.assertFalse(Shiboken.isValid(title_bar))
            del title_bar
            gc.collect()
            self.assertIsNone(title_bar_ref())

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

    def test_field_constructor_and_validation_preserve_editor_value(self):
        editor = fluentqt.LineEdit()
        editor.setText("keep-me")
        field = fluentqt.Field(editor=editor)
        field.setLabelText("Email")
        field.setRequired(True)
        field.setHelperText("Used for recovery")
        field.setValidationState(fluentqt.Field.ValidationState.Error)
        field.setValidationMessage("Enter a valid address")

        self.assertIs(field.editor(), editor)
        self.assertIs(field._fluentqt_hosted_editor, editor)
        self.assertEqual(
            field.editorOwnership(),
            fluentqt.WidgetOwnership.Borrowed,
        )
        self.assertEqual(editor.text(), "keep-me")
        self.assertEqual(field.labelText(), "Email")
        self.assertTrue(field.isRequired())
        self.assertEqual(field.helperText(), "Used for recovery")
        self.assertEqual(field.validationMessage(), "Enter a valid address")

    def test_field_owned_editor_lifecycle(self):
        field = fluentqt.Field()
        first = fluentqt.LineEdit()
        second = fluentqt.LineEdit()

        field.setOwnedEditor(first)
        self.assertIs(field.editor(), first)
        self.assertEqual(
            field.editorOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

        field.setOwnedEditor(second)
        self.assertFalse(Shiboken.isValid(first))
        self.assertIs(field.editor(), second)

        field.releaseEditor()
        self.assertFalse(Shiboken.isValid(second))
        self.assertIsNone(field.editor())
        self.assertIsNone(field._fluentqt_hosted_editor)

    def test_field_borrowed_editor_lifecycle(self):
        class PythonEditor(fluentqt.LineEdit):
            marker = "field-borrowed-subclass"

        field = fluentqt.Field()
        editor = PythonEditor()
        editor_ref = weakref.ref(editor)
        field.setEditor(editor)
        del editor
        gc.collect()

        hosted = field.editor()
        self.assertIs(hosted, editor_ref())
        self.assertEqual(hosted.marker, "field-borrowed-subclass")
        field.releaseEditor()
        self.assertTrue(Shiboken.isValid(hosted))
        self.assertIsNone(hosted.parent())
        self.assertIsNone(field.editor())

    def test_field_reparented_editor_lifecycle(self):
        first_parent = QWidget()
        first = fluentqt.LineEdit(first_parent)
        second_parent = QWidget()
        second = fluentqt.LineEdit(second_parent)
        field = fluentqt.Field()

        field.setReparentedEditor(first)
        self.assertIsNot(first.parent(), first_parent)
        self.assertIs(field._fluentqt_original_parent, first_parent)
        field.setReparentedEditor(second)
        self.assertIs(first.parent(), first_parent)
        self.assertIsNot(second.parent(), second_parent)
        field.releaseEditor()
        self.assertIs(second.parent(), second_parent)

    def test_field_take_and_invalid_editor_contracts(self):
        parent = QWidget()
        field = fluentqt.Field(parent)
        editor = fluentqt.LineEdit()
        field.setBorrowedEditor(editor)

        with self.assertRaisesRegex(ValueError, "takeEditor"):
            field.setOwnedEditor(editor)
        with self.assertRaisesRegex(ValueError, "host or its ancestor"):
            field.setBorrowedEditor(field)
        with self.assertRaisesRegex(ValueError, "host or its ancestor"):
            field.setBorrowedEditor(parent)

        taken = field.takeEditor()
        self.assertIs(taken, editor)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))
        self.assertIsNone(field._fluentqt_hosted_editor)
        self.assertIsNone(field._fluentqt_original_parent)

    def test_field_owned_gc_stress(self):
        for _ in range(25):
            field = fluentqt.Field()
            editor = fluentqt.LineEdit()
            field.setOwnedEditor(editor)
            field_ref = weakref.ref(field)
            del field
            gc.collect()
            self.assertIsNone(field_ref())
            self.assertFalse(Shiboken.isValid(editor))
            del editor

    def test_field_borrowed_gc_stress(self):
        for _ in range(25):
            field = fluentqt.Field()
            editor = fluentqt.LineEdit()
            field.setBorrowedEditor(editor)
            field_ref = weakref.ref(field)
            del field
            gc.collect()
            self.assertIsNone(field_ref())
            self.assertTrue(Shiboken.isValid(editor))
            self.assertIsNone(editor.parent())
            del editor
            gc.collect()

    def test_field_reparented_gc_stress(self):
        for _ in range(25):
            original_parent = QWidget()
            editor = fluentqt.LineEdit(original_parent)
            field = fluentqt.Field()
            field.setReparentedEditor(editor)
            field_ref = weakref.ref(field)
            del field
            gc.collect()
            self.assertIsNone(field_ref())
            self.assertTrue(Shiboken.isValid(editor))
            self.assertIs(editor.parent(), original_parent)
            del original_parent
            gc.collect()
            self.assertFalse(Shiboken.isValid(editor))
            del editor

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

    def test_auto_suggest_box_properties_enums_and_repeat_safe_signals(self):
        box = fluentqt.AutoSuggestBox()
        self.assertEqual(box.suggestions(), [])
        self.assertEqual(
            box.queryButtonPlacement(),
            fluentqt.AutoSuggestBox.QueryButtonPlacement.Right,
        )
        self.assertFalse(box.isSuggestionListOpen())

        changes = {
            "suggestions": [],
            "header": [],
            "icon": [],
            "visible": [],
            "placement": [],
            "input_height": [],
            "query_size": [],
            "clear_size": [],
            "font": [],
            "item_height": [],
        }
        box.suggestionsChanged.connect(
            lambda: changes["suggestions"].append(True)
        )
        box.headerChanged.connect(lambda: changes["header"].append(True))
        box.queryIconGlyphChanged.connect(
            lambda: changes["icon"].append(True)
        )
        box.queryIconVisibleChanged.connect(
            lambda: changes["visible"].append(True)
        )
        box.queryButtonPlacementChanged.connect(
            lambda: changes["placement"].append(True)
        )
        box.inputHeightChanged.connect(
            lambda: changes["input_height"].append(True)
        )
        box.queryButtonSizeChanged.connect(
            lambda: changes["query_size"].append(True)
        )
        box.clearButtonSizeChanged.connect(
            lambda: changes["clear_size"].append(True)
        )
        box.suggestionFontRoleChanged.connect(
            lambda: changes["font"].append(True)
        )
        box.suggestionItemHeightChanged.connect(
            lambda: changes["item_height"].append(True)
        )

        suggestions = ["Alpha", "Alpine", "Azure"]
        setters = (
            (box.setSuggestions, suggestions),
            (box.setHeader, "Search files"),
            (box.setQueryIconGlyph, "?"),
            (box.setQueryIconVisible, False),
            (
                box.setQueryButtonPlacement,
                fluentqt.AutoSuggestBox.QueryButtonPlacement.Left,
            ),
            (box.setInputHeight, 28),
            (box.setQueryButtonSize, 20),
            (box.setClearButtonSize, 18),
            (box.setSuggestionFontRole, fluentqt.FontRole.Caption),
            (box.setSuggestionItemHeight, 30),
        )
        for setter, value in setters:
            setter(value)
            setter(value)

        self.assertEqual(box.suggestions(), suggestions)
        self.assertEqual(box.header(), "Search files")
        self.assertEqual(box.queryIconGlyph(), "?")
        self.assertFalse(box.isQueryIconVisible())
        self.assertEqual(
            box.queryButtonPlacement(),
            fluentqt.AutoSuggestBox.QueryButtonPlacement.Left,
        )
        self.assertEqual(box.inputHeight(), 28)
        self.assertEqual(box.queryButtonSize(), 20)
        self.assertEqual(box.clearButtonSize(), 18)
        self.assertEqual(box.suggestionFontRole(), fluentqt.FontRole.Caption)
        self.assertEqual(box.suggestionItemHeight(), 30)
        self.assertGreaterEqual(box.sizeHint().height(), 28)
        self.assertGreaterEqual(box.minimumSizeHint().height(), 28)
        for signal_events in changes.values():
            self.assertEqual(signal_events, [True])

        self.assertNotEqual(
            fluentqt.AutoSuggestBox.TextChangeReason.UserInput,
            fluentqt.AutoSuggestBox.TextChangeReason.ProgrammaticChange,
        )
        box.clearSuggestions()
        self.assertEqual(box.suggestions(), [])

    def test_auto_suggest_box_keyboard_signals_and_subclassing(self):
        class PythonAutoSuggestBox(fluentqt.AutoSuggestBox):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.key_press_count = 0

            def keyPressEvent(self, event):
                self.key_press_count += 1
                super().keyPressEvent(event)

        host = QWidget()
        host.resize(520, 360)
        box = PythonAutoSuggestBox(host)
        box.setGeometry(48, 48, 240, box.sizeHint().height())
        box.setSuggestions(["Alpha", "Alpine", "Azure"])
        reasons = []
        chosen = []
        submitted = []
        open_states = []
        box.textChangedWithReason.connect(
            lambda text, reason: reasons.append((text, reason))
        )
        box.suggestionChosen.connect(chosen.append)
        box.querySubmitted.connect(
            lambda text, item: submitted.append((text, item))
        )
        box.suggestionListOpenChanged.connect(open_states.append)

        host.show()
        box.show()
        box.setFocus()
        QCoreApplication.processEvents()
        QTest.keyClicks(box, "a")
        QCoreApplication.processEvents()
        self.assertTrue(box.isSuggestionListOpen())
        self.assertIs(QApplication.focusWidget(), box)
        self.assertEqual(
            reasons[-1],
            (
                "a",
                fluentqt.AutoSuggestBox.TextChangeReason.UserInput,
            ),
        )

        popup = host.findChild(
            native.fluent.Flyout,
            "AutoSuggestBoxSuggestionPopup",
        )
        self.assertIsNotNone(popup)
        self.assertTrue(popup.isVisible())
        self.assertIs(popup.window(), host)
        self.assertFalse(popup.isWindow())

        QTest.keyClick(box, Qt.Key_Down)
        QCoreApplication.processEvents()
        self.assertEqual(box.text(), "Alpha")
        self.assertEqual(chosen, ["Alpha"])
        self.assertEqual(
            reasons[-1][1],
            fluentqt.AutoSuggestBox.TextChangeReason.ProgrammaticChange,
        )

        QTest.keyClick(box, Qt.Key_Return)
        QCoreApplication.processEvents()
        self.assertFalse(box.isSuggestionListOpen())
        self.assertEqual(chosen, ["Alpha", "Alpha"])
        self.assertEqual(submitted, [("Alpha", "Alpha")])
        self.assertEqual(
            reasons[-1],
            (
                "Alpha",
                fluentqt.AutoSuggestBox.TextChangeReason.SuggestionChosen,
            ),
        )
        self.assertEqual(open_states, [True, False])
        self.assertGreaterEqual(box.key_press_count, 3)
        host.close()

    def test_auto_suggest_box_popup_lifecycle_stress(self):
        for _ in range(25):
            host = QWidget()
            host.resize(520, 360)
            box = fluentqt.AutoSuggestBox(host)
            box.setGeometry(48, 48, 240, box.sizeHint().height())
            box.setSuggestions(["Alpha", "Alpine", "Azure"])
            host.show()
            box.show()
            box.setFocus()
            QCoreApplication.processEvents()
            QTest.keyClicks(box, "a")
            QCoreApplication.processEvents()
            popup = host.findChild(
                native.fluent.Flyout,
                "AutoSuggestBoxSuggestionPopup",
            )
            self.assertIsNotNone(popup)
            self.assertTrue(box.isSuggestionListOpen())
            self.assertTrue(Shiboken.isValid(popup))

            box_ref = weakref.ref(box)
            popup_ref = weakref.ref(popup)
            box.deleteLater()
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
            QCoreApplication.processEvents()
            self.assertFalse(Shiboken.isValid(box))
            self.assertFalse(Shiboken.isValid(popup))
            del popup
            del box
            host.deleteLater()
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
            QCoreApplication.processEvents()
            del host
            gc.collect()
            self.assertIsNone(box_ref())
            self.assertIsNone(popup_ref())

    def test_combo_box_properties_items_and_signals(self):
        combo = fluentqt.ComboBox()
        self.assertEqual(combo.fontRole(), fluentqt.FontRole.Body)
        self.assertGreater(combo.contentPaddingH(), 0)
        self.assertGreater(combo.contentPaddingV(), 0)
        self.assertTrue(combo.chevronGlyph())
        self.assertGreater(combo.chevronSize(), 0)
        self.assertGreaterEqual(combo.popupOffset(), 0)
        self.assertEqual(combo.pressProgress(), 0.0)

        font_changes = []
        layout_changes = []
        chevron_changes = []
        index_changes = []
        text_changes = []
        combo.fontRoleChanged.connect(lambda: font_changes.append(True))
        combo.layoutChanged.connect(lambda: layout_changes.append(True))
        combo.chevronChanged.connect(lambda: chevron_changes.append(True))
        combo.currentIndexChanged.connect(index_changes.append)
        combo.currentTextChanged.connect(text_changes.append)

        combo.setFontRole(fluentqt.FontRole.Caption)
        combo.setContentPaddingH(18)
        combo.setContentPaddingV(7)
        combo.setChevronGlyph("v")
        combo.setChevronSize(15)
        combo.setChevronOffset(QPoint(12, 1))
        combo.setPopupOffset(9)
        combo.setPressProgress(0.5)
        combo.addItems(["Alpha", "Beta", "Gamma"])
        combo.setItemData(1, {"route": "beta"}, Qt.UserRole)
        combo.setCurrentIndex(1)

        self.assertEqual(combo.fontRole(), fluentqt.FontRole.Caption)
        self.assertEqual(combo.contentPaddingH(), 18)
        self.assertEqual(combo.contentPaddingV(), 7)
        self.assertEqual(combo.chevronGlyph(), "v")
        self.assertEqual(combo.chevronSize(), 15)
        self.assertEqual(combo.chevronOffset(), QPoint(12, 1))
        self.assertEqual(combo.popupOffset(), 9)
        self.assertEqual(combo.pressProgress(), 0.5)
        self.assertEqual(combo.count(), 3)
        self.assertEqual(combo.currentIndex(), 1)
        self.assertEqual(combo.currentText(), "Beta")
        self.assertEqual(combo.itemData(1, Qt.UserRole), {"route": "beta"})
        self.assertEqual(font_changes, [True])
        self.assertEqual(layout_changes, [True, True, True])
        self.assertEqual(chevron_changes, [True, True, True])
        self.assertEqual(index_changes[-1:], [1])
        self.assertEqual(text_changes[-1:], ["Beta"])

    def test_combo_box_dropdown_lifecycle_selection_and_subclassing(self):
        class PythonComboBox(fluentqt.ComboBox):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.show_popup_count = 0
                self.key_press_count = 0

            def showPopup(self):
                self.show_popup_count += 1
                super().showPopup()

            def keyPressEvent(self, event):
                self.key_press_count += 1
                super().keyPressEvent(event)

        host = QWidget()
        host.resize(520, 360)
        combo = PythonComboBox(host)
        combo.setGeometry(48, 48, 190, 32)
        combo.addItems(["Alpha", "Beta", "Gamma"])
        combo.setCurrentIndex(0)
        host.show()
        QCoreApplication.processEvents()

        QTest.mouseClick(
            combo,
            Qt.LeftButton,
            Qt.NoModifier,
            QPoint(8, 8),
        )
        QCoreApplication.processEvents()
        popup = host.findChild(QWidget, "ComboBoxPopup")
        self.assertIsNotNone(popup)
        self.assertTrue(popup.isVisible())
        self.assertIs(popup.window(), host)
        self.assertFalse(popup.isWindow())
        self.assertEqual(combo.show_popup_count, 1)

        popup_view = popup.findChild(QListView, "ComboBoxPopupListView")
        self.assertIsNotNone(popup_view)
        self.assertIs(popup_view.model(), combo.model())
        target = popup_view.model().index(2, 0)
        target_rect = popup_view.visualRect(target)
        self.assertFalse(target_rect.isEmpty())
        QTest.mouseClick(
            popup_view.viewport(),
            Qt.LeftButton,
            Qt.NoModifier,
            target_rect.center(),
        )
        QCoreApplication.processEvents()
        self.assertEqual(combo.currentIndex(), 2)
        self.assertEqual(combo.currentText(), "Gamma")
        self.assertFalse(popup.isVisible())

        combo.showPopup()
        QCoreApplication.processEvents()
        self.assertEqual(combo.show_popup_count, 2)
        self.assertTrue(popup.isVisible())
        QTest.keyClick(popup, Qt.Key_Escape)
        QCoreApplication.processEvents()
        self.assertFalse(popup.isVisible())

        combo.setCurrentIndex(0)
        combo.setFocus()
        QTest.keyClick(combo, Qt.Key_Down)
        self.assertEqual(combo.key_press_count, 1)
        self.assertEqual(combo.currentIndex(), 1)
        host.close()

    def test_combo_box_model_editor_and_customization_boundaries(self):
        class PythonTextModel(QAbstractListModel):
            def __init__(self, values):
                super().__init__()
                self.values = list(values)
                self.data_calls = 0

            def rowCount(self, parent=QModelIndex()):
                return 0 if parent.isValid() else len(self.values)

            def data(self, index, role=Qt.DisplayRole):
                self.data_calls += 1
                if index.isValid() and role in (
                    Qt.DisplayRole,
                    Qt.EditRole,
                ):
                    return self.values[index.row()]
                return None

        combo = fluentqt.ComboBox()
        first_model = QStringListModel(["One", "Two", "Three"])
        first_model_ref = weakref.ref(first_model)
        combo.setModel(first_model)
        combo.setCurrentIndex(1)
        del first_model
        gc.collect()
        self.assertIs(combo.model(), first_model_ref())
        self.assertEqual(combo.currentText(), "Two")

        replacement = QStringListModel(["Red", "Green"])
        combo.setModel(replacement)
        QCoreApplication.processEvents()
        gc.collect()
        self.assertIsNone(first_model_ref())
        self.assertIs(combo.model(), replacement)

        python_model = PythonTextModel(["North", "South", "West"])
        combo.setModel(python_model)
        combo.setCurrentIndex(1)
        self.assertEqual(combo.currentText(), "South")
        self.assertGreater(python_model.data_calls, 0)

        with self.assertRaisesRegex(NotImplementedError, "dropdown view"):
            combo.view()
        with self.assertRaisesRegex(NotImplementedError, "custom QComboBox"):
            combo.setView(QListView())
        with self.assertRaisesRegex(NotImplementedError, "dropdown delegate"):
            combo.itemDelegate()
        with self.assertRaisesRegex(NotImplementedError, "dropdown delegate"):
            combo.setItemDelegate(QStyledItemDelegate())

        class PythonEditor(QLineEdit):
            pass

        first_editor = PythonEditor()
        combo.setLineEdit(first_editor)
        self.assertTrue(combo.isEditable())
        self.assertIs(combo.lineEdit(), first_editor)
        self.assertIs(first_editor.parent(), combo)
        self.assertFalse(Shiboken.ownedByPython(first_editor))

        second_editor = PythonEditor()
        combo.setLineEdit(second_editor)
        self.assertFalse(Shiboken.isValid(first_editor))
        self.assertIs(combo.lineEdit(), second_editor)
        combo.setEditable(False)
        self.assertFalse(Shiboken.isValid(second_editor))
        self.assertIsNone(combo.lineEdit())

        combo.setEditable(True)
        fluent_editor = combo.fluentLineEdit()
        self.assertIsInstance(fluent_editor, fluentqt.LineEdit)
        self.assertIs(combo.lineEdit(), fluent_editor)
        self.assertIs(fluent_editor.parent(), combo)

    def test_binding_theme_adapter_refreshes_native_widget_subtree(self):
        previous_theme = fluentqt.current_theme()
        fluentqt.set_theme(fluentqt.Theme.Light)
        root = fluentqt.Card()
        combo = fluentqt.ComboBox(root)
        combo.addItems(["10", "11", "12"])
        combo.setEditable(True)
        editor = combo.fluentLineEdit()
        self.assertIsNotNone(editor)

        root.setProperty("fluentThemeOverride", int(fluentqt.Theme.Dark))
        snapshot = native.themeTokensForWidgetForBinding(editor)
        self.assertEqual(snapshot["theme"], int(fluentqt.Theme.Dark))
        native.refreshWidgetThemeForBinding(root)
        self.assertEqual(
            editor.palette().color(
                QPalette.ColorGroup.Active,
                QPalette.ColorRole.Text,
            ),
            QColor(snapshot["colors"]["textPrimary"]),
        )
        self.assertFalse(hasattr(fluentqt, "refreshWidgetThemeForBinding"))
        self.assertFalse(hasattr(fluentqt, "themeTokensForWidgetForBinding"))

        root.deleteLater()
        fluentqt.set_theme(previous_theme)

    def test_combo_box_dependency_gc_stress(self):
        class PythonEditor(QLineEdit):
            pass

        for _ in range(25):
            combo = fluentqt.ComboBox()
            model = QStringListModel(["Alpha", "Beta"])
            editor = PythonEditor()
            combo.setModel(model)
            combo.setLineEdit(editor)
            combo_ref = weakref.ref(combo)
            model_ref = weakref.ref(model)
            editor_ref = weakref.ref(editor)
            del model
            del editor
            gc.collect()
            self.assertIsNotNone(model_ref())
            self.assertIsNotNone(editor_ref())

            del combo
            gc.collect()
            self.assertIsNone(combo_ref())
            self.assertIsNone(model_ref())
            self.assertIsNone(editor_ref())

    def test_menu_buttons_properties_interaction_and_subclassing(self):
        class PythonSplitButton(fluentqt.SplitButton):
            def __init__(self):
                super().__init__("Python split")
                self.release_count = 0

            def mouseReleaseEvent(self, event):
                self.release_count += 1
                super().mouseReleaseEvent(event)

        drop_down = fluentqt.DropDownButton("Options")
        split = PythonSplitButton()
        toggle = fluentqt.ToggleSplitButton("Pin")
        drop_menu = fluentqt.FluentMenu("Options")
        split_menu = fluentqt.FluentMenu("Split")
        toggle_menu = fluentqt.FluentMenu("Toggle")
        for menu in (drop_menu, split_menu, toggle_menu):
            menu.addAction("First")
            menu.aboutToShow.connect(
                lambda current=menu: QTimer.singleShot(
                    0,
                    current.close,
                )
            )

        drop_down.setMenu(drop_menu)
        split.setMenu(split_menu)
        toggle.setMenu(toggle_menu)
        self.assertIs(drop_down.menu(), drop_menu)
        self.assertIs(split.menu(), split_menu)
        self.assertIs(toggle.menu(), toggle_menu)
        self.assertGreater(split.secondaryWidth(), 0)
        split.setSecondaryWidth(40)
        self.assertEqual(split.secondaryWidth(), 40)

        drop_open = []
        split_open = []
        toggle_open = []
        split_clicks = []
        toggle_changes = []
        drop_down.openChanged.connect(
            lambda: drop_open.append(drop_down.isOpen())
        )
        split.openChanged.connect(
            lambda: split_open.append(split.isOpen())
        )
        toggle.openChanged.connect(
            lambda: toggle_open.append(toggle.isOpen())
        )
        split.clicked.connect(lambda: split_clicks.append(True))
        toggle.toggled.connect(toggle_changes.append)

        for button in (drop_down, split, toggle):
            button.resize(170, 36)
            button.show()
        QCoreApplication.processEvents()

        QTest.mouseClick(
            drop_down,
            Qt.LeftButton,
            Qt.NoModifier,
            drop_down.rect().center(),
        )
        wait_for_events(1)
        QTest.mouseClick(
            split,
            Qt.LeftButton,
            Qt.NoModifier,
            QPoint(split.width() - 8, split.height() // 2),
        )
        wait_for_events(1)
        QTest.mouseClick(
            toggle,
            Qt.LeftButton,
            Qt.NoModifier,
            QPoint(toggle.width() - 8, toggle.height() // 2),
        )
        wait_for_events(1)

        self.assertEqual(drop_open, [True, False])
        self.assertEqual(split_open, [True, False])
        self.assertEqual(toggle_open, [True, False])
        self.assertEqual(split_clicks, [])
        self.assertEqual(toggle_changes, [])
        self.assertFalse(toggle.isChecked())

        QTest.mouseClick(
            split,
            Qt.LeftButton,
            Qt.NoModifier,
            QPoint(split.width() // 4, split.height() // 2),
        )
        QTest.mouseClick(
            toggle,
            Qt.LeftButton,
            Qt.NoModifier,
            QPoint(toggle.width() // 4, toggle.height() // 2),
        )
        self.assertEqual(split.release_count, 2)
        self.assertEqual(split_clicks, [True])
        self.assertEqual(toggle_changes, [True])
        self.assertTrue(toggle.isChecked())

    def test_fluent_menu_item_properties_and_action_semantics(self):
        menu = fluentqt.FluentMenu("Actions")
        item = fluentqt.FluentMenuItem("Open", menu)
        menu.addAction(item)
        menu_changes = []
        item_changes = []
        triggered = []
        menu.fontStyleChanged.connect(lambda: menu_changes.append(True))
        item.fontStyleChanged.connect(lambda: item_changes.append(True))
        item.triggered.connect(lambda: triggered.append(True))

        menu.setFontStyle(fluentqt.FontRole.BodyStrong)
        item.setFontStyle(fluentqt.FontRole.Caption)
        self.assertEqual(
            menu.fontStyle(),
            fluentqt.FontRole.BodyStrong,
        )
        self.assertEqual(item.fontStyle(), fluentqt.FontRole.Caption)
        self.assertEqual(menu_changes, [True])
        self.assertEqual(item_changes, [True])
        self.assertEqual(menu.actions(), [item])

        item.trigger()
        self.assertEqual(triggered, [True])

    def test_command_surfaces_properties_actions_and_subclassing(self):
        class PythonCommandBar(fluentqt.CommandBar):
            def sizeHint(self):
                return QSize(321, 45)

        command_bar = PythonCommandBar()
        label_changes = []
        overflow_changes = []
        background_changes = []
        command_bar.labelPositionChanged.connect(label_changes.append)
        command_bar.dynamicOverflowEnabledChanged.connect(
            overflow_changes.append
        )
        command_bar.backgroundVisibleChanged.connect(
            background_changes.append
        )

        command_bar.setLabelPosition(
            fluentqt.CommandBar.LabelPosition.Collapsed
        )
        command_bar.setDynamicOverflowEnabled(False)
        command_bar.setBackgroundVisible(False)
        self.assertEqual(
            command_bar.labelPosition(),
            fluentqt.CommandBar.LabelPosition.Collapsed,
        )
        self.assertFalse(command_bar.isDynamicOverflowEnabled())
        self.assertFalse(command_bar.isBackgroundVisible())
        self.assertEqual(
            label_changes,
            [fluentqt.CommandBar.LabelPosition.Collapsed],
        )
        self.assertEqual(overflow_changes, [False])
        self.assertEqual(background_changes, [False])
        self.assertEqual(command_bar.sizeHint(), QSize(321, 45))

        first = QAction("First")
        inserted = QAction("Inserted")
        secondary = QAction("Secondary")
        self.assertTrue(command_bar.addPrimaryAction(first))
        self.assertTrue(
            command_bar.insertPrimaryAction(first, inserted)
        )
        self.assertTrue(command_bar.addSecondaryAction(secondary))
        self.assertEqual(command_bar.primaryActions(), [inserted, first])
        self.assertEqual(command_bar.secondaryActions(), [secondary])
        self.assertTrue(command_bar.addSecondaryAction(first))
        self.assertEqual(command_bar.primaryActions(), [inserted])
        self.assertEqual(
            command_bar.secondaryActions(), [secondary, first]
        )

        callback_results = []
        generated_action = command_bar.addAction(
            "Generated",
            lambda: callback_results.append(True),
        )
        self.assertIs(generated_action.parent(), command_bar)
        self.assertIn(generated_action, command_bar.primaryActions())
        generated_action.trigger()
        self.assertEqual(callback_results, [True])

        flyout = fluentqt.CommandBarFlyout()
        flyout_mode_changes = []
        always_expanded_changes = []
        flyout.showModeChanged.connect(flyout_mode_changes.append)
        flyout.alwaysExpandedChanged.connect(
            always_expanded_changes.append
        )
        flyout.setShowMode(
            fluentqt.CommandBarFlyout.ShowMode.Transient
        )
        flyout.setAlwaysExpanded(True)
        self.assertEqual(
            flyout.showMode(),
            fluentqt.CommandBarFlyout.ShowMode.Transient,
        )
        self.assertTrue(flyout.isAlwaysExpanded())
        self.assertEqual(
            flyout_mode_changes,
            [fluentqt.CommandBarFlyout.ShowMode.Transient],
        )
        self.assertEqual(always_expanded_changes, [True])
        with self.assertRaises(TypeError):
            type("PythonCommandBarFlyout", (fluentqt.CommandBarFlyout,), {})

        menu_bar = fluentqt.FluentMenuBar()
        font_changes = []
        menu_background_changes = []
        menu_bar.fontStyleChanged.connect(lambda: font_changes.append(True))
        menu_bar.backgroundVisibleChanged.connect(
            menu_background_changes.append
        )
        menu_bar.setFontStyle(fluentqt.FontRole.BodyStrong)
        menu_bar.setBackgroundVisible(False)
        self.assertEqual(
            menu_bar.fontStyle(), fluentqt.FontRole.BodyStrong
        )
        self.assertFalse(menu_bar.isBackgroundVisible())
        self.assertEqual(font_changes, [True])
        self.assertEqual(menu_background_changes, [False])
        menu = menu_bar.addMenu("File")
        self.assertIs(menu.parent(), menu_bar)
        self.assertIn(menu.menuAction(), menu_bar.actions())

    def test_editing_command_router_tracks_focus_and_executes_commands(self):
        scope = QWidget()
        editor = fluentqt.LineEdit(scope)
        editor.setText("hello")
        editor.setGeometry(8, 8, 220, 36)
        router = fluentqt.EditingCommandRouter(scope, scope)
        commands = (
            fluentqt.EditingCommandRouter.Command.Undo,
            fluentqt.EditingCommandRouter.Command.Redo,
            fluentqt.EditingCommandRouter.Command.Cut,
            fluentqt.EditingCommandRouter.Command.Copy,
            fluentqt.EditingCommandRouter.Command.Paste,
            fluentqt.EditingCommandRouter.Command.Delete,
            fluentqt.EditingCommandRouter.Command.SelectAll,
        )

        self.assertIs(router.scopeWindow(), scope)
        self.assertEqual(len(commands), 7)
        self.assertEqual(len(router.actions()), 7)
        for command in commands:
            self.assertIs(router.action(command), router.action(command))

        scope.show()
        editor.setFocus(Qt.FocusReason.OtherFocusReason)
        self.app.processEvents()
        router.refresh()
        self.assertTrue(router.hasActiveTarget())
        self.assertTrue(
            router.execute(fluentqt.EditingCommandRouter.Command.SelectAll)
        )
        router.refresh()
        self.assertTrue(
            router.canExecute(fluentqt.EditingCommandRouter.Command.Copy)
        )
        QApplication.clipboard().clear()
        self.assertTrue(
            router.execute(fluentqt.EditingCommandRouter.Command.Copy)
        )
        self.assertEqual(QApplication.clipboard().text(), "hello")
        scope.close()
        scope.deleteLater()
        self.app.processEvents()

    def test_final_type_fallback_for_old_shiboken(self):
        from fluentqt.menus_toolbars import _enforce_final_type

        class LegacyGeneratedFinalType:
            pass

        self.assertIs(
            _enforce_final_type(LegacyGeneratedFinalType),
            LegacyGeneratedFinalType,
        )
        with self.assertRaisesRegex(TypeError, "final"):
            type(
                "InvalidLegacyGeneratedSubclass",
                (LegacyGeneratedFinalType,),
                {},
            )

    def test_callable_add_action_fallback_for_old_shiboken(self):
        from fluentqt.menus_toolbars import (
            _install_legacy_callable_add_action,
        )

        class LegacyCommandSurface(QWidget):
            pass

        _install_legacy_callable_add_action(
            LegacyCommandSurface,
            (6, 2, 4),
        )

        class PythonLegacyCommandSurface(LegacyCommandSurface):
            pass

        surface = PythonLegacyCommandSurface()
        calls = []
        shortcut = QKeySequence("Ctrl+K")
        cases = (
            ("Text", lambda: calls.append(0)),
            (QIcon(), "Icon", lambda: calls.append(1)),
            ("Shortcut", shortcut, lambda: calls.append(2)),
            (QIcon(), "Both", shortcut, lambda: calls.append(3)),
        )
        for index, arguments in enumerate(cases):
            action = surface.addAction(*arguments)
            self.assertIs(action.parent(), surface)
            self.assertIn(action, surface.actions())
            if index >= 2:
                self.assertEqual(action.shortcut(), shortcut)
            action.trigger()
        self.assertEqual(calls, [0, 1, 2, 3])

    def test_command_surface_borrowed_dependencies_and_external_delete(self):
        command_bar = fluentqt.CommandBar()
        flyout = fluentqt.CommandBarFlyout()
        shared_action = QAction("Shared")
        shared_ref = weakref.ref(shared_action)

        self.assertTrue(command_bar.addPrimaryAction(shared_action))
        self.assertTrue(flyout.addSecondaryAction(shared_action))
        self.assertIsNone(shared_action.parent())
        self.assertTrue(Shiboken.ownedByPython(shared_action))
        del shared_action
        gc.collect()
        self.assertIsNotNone(shared_ref())
        self.assertIs(command_bar.primaryActions()[0], shared_ref())
        self.assertIs(flyout.secondaryActions()[0], shared_ref())

        command_bar.clearPrimaryActions()
        gc.collect()
        self.assertIsNotNone(shared_ref())
        self.assertEqual(command_bar.primaryActions(), [])
        flyout.clearSecondaryActions()
        gc.collect()
        self.assertIsNone(shared_ref())

        anchor = QWidget()
        anchor_ref = weakref.ref(anchor)
        flyout.setAnchor(anchor)
        del anchor
        gc.collect()
        self.assertIs(flyout.anchor(), anchor_ref())
        flyout.setAnchor(None)
        gc.collect()
        self.assertIsNone(anchor_ref())

        externally_deleted = QAction("Deleted externally")
        deleted_ref = weakref.ref(externally_deleted)
        self.assertTrue(command_bar.addPrimaryAction(externally_deleted))
        # Exercise Qt's supported external-destruction path. Direct
        # Shiboken.delete() can fast-fail inside PySide6 6.2.4 on Windows
        # before QAction finishes notifying every borrowing widget, so that
        # low-level wrapper operation is not part of the public contract.
        externally_deleted.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(externally_deleted))
        self.assertEqual(command_bar.primaryActions(), [])
        command_bar.clearPrimaryActions()
        del externally_deleted
        gc.collect()
        self.assertIsNone(deleted_ref())

    def test_command_surface_dependency_gc_stress(self):
        for _ in range(25):
            command_bar = fluentqt.CommandBar()
            flyout = fluentqt.CommandBarFlyout()
            action = QAction("Shared")
            anchor = QWidget()
            command_bar.addPrimaryAction(action)
            flyout.addSecondaryAction(action)
            flyout.setAnchor(anchor)

            command_bar_ref = weakref.ref(command_bar)
            flyout_ref = weakref.ref(flyout)
            action_ref = weakref.ref(action)
            anchor_ref = weakref.ref(anchor)
            del action
            del anchor
            gc.collect()
            self.assertIsNotNone(action_ref())
            self.assertIsNotNone(anchor_ref())

            del command_bar
            gc.collect()
            self.assertIsNone(command_bar_ref())
            self.assertIsNotNone(action_ref())

            del flyout
            gc.collect()
            self.assertIsNone(flyout_ref())
            self.assertIsNone(action_ref())
            self.assertIsNone(anchor_ref())

    def test_menu_button_external_delete_clears_native_pointer(self):
        for button_type in (
            fluentqt.DropDownButton,
            fluentqt.SplitButton,
            fluentqt.ToggleSplitButton,
        ):
            with self.subTest(button_type=button_type.__name__):
                button = button_type("Menu")
                menu = fluentqt.FluentMenu("Actions")
                changes = []
                button.menuChanged.connect(lambda: changes.append(True))
                button.setMenu(menu)
                self.assertIs(button.menu(), menu)

                menu.deleteLater()
                QCoreApplication.sendPostedEvents(
                    None,
                    QEvent.DeferredDelete,
                )
                QCoreApplication.processEvents()
                self.assertFalse(Shiboken.isValid(menu))
                self.assertIsNone(button.menu())
                self.assertEqual(changes, [True, True])

    def test_menu_button_dependency_gc_stress(self):
        for button_type in (
            fluentqt.DropDownButton,
            fluentqt.SplitButton,
            fluentqt.ToggleSplitButton,
        ):
            for _ in range(25):
                button = button_type("Menu")
                first_menu = fluentqt.FluentMenu("First")
                button.setMenu(first_menu)
                first_ref = weakref.ref(first_menu)
                del first_menu
                gc.collect()
                self.assertIsNotNone(first_ref())
                self.assertTrue(Shiboken.isValid(first_ref()))

                replacement = fluentqt.FluentMenu("Replacement")
                button.setMenu(replacement)
                gc.collect()
                self.assertIsNone(first_ref())
                self.assertIs(button.menu(), replacement)

                replacement_ref = weakref.ref(replacement)
                button.setMenu(None)
                del replacement
                gc.collect()
                self.assertIsNone(replacement_ref())
                self.assertIsNone(button.menu())

                final_menu = fluentqt.FluentMenu("Final")
                button.setMenu(final_menu)

                button_ref = weakref.ref(button)
                final_ref = weakref.ref(final_menu)
                del final_menu
                del button
                gc.collect()
                self.assertIsNone(button_ref())
                self.assertIsNone(final_ref())

    def test_dialog_same_window_lifecycle_exec_and_subclassing(self):
        class PythonDialog(fluentqt.Dialog):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.show_count = 0

            def showEvent(self, event):
                self.show_count += 1
                super().showEvent(event)

        host = QWidget()
        host.resize(640, 480)
        host.show()
        dialog = PythonDialog(host)
        dialog.setFixedSize(320, 200)
        dialog.setAnimationEnabled(False)
        dialog.setDragEnabled(False)
        dialog.setSmokeEnabled(True)
        dialog.setWindowModality(Qt.ApplicationModal)

        self.assertEqual(dialog.shadowSize(), 16)
        self.assertFalse(dialog.isDragEnabled())
        self.assertFalse(dialog.isAnimationEnabled())
        self.assertTrue(dialog.isSmokeEnabled())
        dialog.setAnimationProgress(0.75)
        self.assertAlmostEqual(dialog.animationProgress(), 0.75)

        dialog.open()
        QCoreApplication.processEvents()
        self.assertTrue(dialog.isVisible())
        self.assertIs(dialog.parentWidget(), host)
        self.assertIs(dialog.window(), host)
        self.assertEqual(dialog.windowType(), Qt.Widget)
        self.assertGreaterEqual(dialog.show_count, 1)
        scrim = host.findChild(QWidget, "DialogSmokeScrim")
        self.assertIsNotNone(scrim)
        self.assertTrue(scrim.isVisible())

        dialog.done(QDialog.Rejected)
        QCoreApplication.processEvents()
        self.assertFalse(dialog.isVisible())
        self.assertIsNone(host.findChild(QWidget, "DialogSmokeScrim"))

        QTimer.singleShot(0, lambda: dialog.done(QDialog.Accepted))
        self.assertEqual(dialog.exec(), QDialog.Accepted)
        host.close()

    def test_dialog_theme_source_facade_retains_and_releases_widget(self):
        dialog = fluentqt.Dialog()
        with self.assertRaisesRegex(TypeError, "theme source"):
            dialog.setThemeSource(object())
        with self.assertRaisesRegex(ValueError, "theme source"):
            dialog.setThemeSource(dialog)

        source = QWidget()
        dialog.setThemeSource(source)
        source_ref = weakref.ref(source)
        del source
        gc.collect()
        self.assertIsNotNone(source_ref())

        dialog.setThemeSource(None)
        gc.collect()
        self.assertIsNone(source_ref())
        self.assertIsNone(dialog._fluentqt_dialog_theme_source_record)

        external = QWidget()
        dialog.setThemeSource(external)
        external.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QCoreApplication.processEvents()
        self.assertIsNone(dialog._fluentqt_dialog_theme_source_record)

    def test_dialog_theme_source_gc_stress(self):
        for _ in range(25):
            dialog = fluentqt.Dialog()
            source = QWidget()
            dialog.setThemeSource(source)
            source_ref = weakref.ref(source)
            del source
            gc.collect()
            self.assertIsNotNone(source_ref())

            dialog_ref = weakref.ref(dialog)
            del dialog
            gc.collect()
            self.assertIsNone(dialog_ref())
            self.assertIsNone(source_ref())

    def test_content_dialog_properties_results_signals_and_subclassing(self):
        class PythonContentDialog(fluentqt.ContentDialog):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.show_count = 0

            def showEvent(self, event):
                self.show_count += 1
                super().showEvent(event)

        host = QWidget()
        host.resize(640, 480)
        host.show()
        dialog = PythonContentDialog(host)
        dialog.setAnimationEnabled(False)
        dialog.setTitle("Delete this item?")
        dialog.setPrimaryButtonText("Delete")
        dialog.setSecondaryButtonText("Keep")
        dialog.setCloseButtonText("Cancel")
        dialog.setDefaultButton(fluentqt.ContentDialogButton.Primary)

        self.assertEqual(dialog.title(), "Delete this item?")
        self.assertEqual(dialog.primaryButtonText(), "Delete")
        self.assertEqual(dialog.secondaryButtonText(), "Keep")
        self.assertEqual(dialog.closeButtonText(), "Cancel")
        self.assertEqual(
            dialog.defaultButton(),
            int(fluentqt.ContentDialogButton.Primary),
        )

        results = []
        primary_clicks = []
        dialog.finished.connect(results.append)
        dialog.primaryButtonClicked.connect(
            lambda: primary_clicks.append(True)
        )
        dialog.open()
        QCoreApplication.processEvents()
        self.assertGreaterEqual(dialog.show_count, 1)
        self.assertIs(dialog.window(), host)

        primary = next(
            button
            for button in dialog.findChildren(fluentqt.Button)
            if button.text() == "Delete"
        )
        primary.click()
        QCoreApplication.processEvents()
        self.assertEqual(primary_clicks, [True])
        self.assertEqual(results, [fluentqt.ContentDialog.ResultPrimary])
        self.assertEqual(
            dialog.result(),
            fluentqt.ContentDialog.ResultPrimary,
        )
        host.close()

    def test_content_dialog_content_ownership_and_external_delete(self):
        class PythonContent(QWidget):
            pass

        original_parent = QWidget()
        dialog = fluentqt.ContentDialog()
        with self.assertRaisesRegex(TypeError, "content"):
            dialog.setContent(object())
        with self.assertRaisesRegex(ValueError, "content"):
            dialog.setContent(dialog)

        ancestor = QWidget()
        nested_dialog = fluentqt.ContentDialog(ancestor)
        with self.assertRaisesRegex(ValueError, "ancestor"):
            nested_dialog.setContent(ancestor)
        # A parented Python subclass must leave through Qt's deferred-delete
        # path on PySide6 6.2.4/Windows. Direct Shiboken.delete() can corrupt
        # the parent's child teardown and fast-fail the next dialog test.
        nested_dialog.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QCoreApplication.processEvents()
        self.assertFalse(Shiboken.isValid(nested_dialog))

        first = PythonContent(original_parent)
        dialog.setContent(first)
        self.assertIs(dialog.content(), first)
        self.assertIs(first.parent(), dialog)

        first_ref = weakref.ref(first)
        del first
        gc.collect()
        self.assertIs(dialog.content(), first_ref())

        second = PythonContent(original_parent)
        retained_first = first_ref()
        dialog.setContent(second)
        self.assertIsNone(retained_first.parent())
        self.assertIs(dialog.content(), second)
        self.assertIs(second.parent(), dialog)
        del retained_first
        gc.collect()
        self.assertIsNone(first_ref())

        taken = dialog.takeContent()
        self.assertIs(taken, second)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))
        self.assertIsNone(dialog.content())
        self.assertIsNone(dialog.takeContent())

        external = PythonContent()
        dialog.setContent(external)
        # Exercise Qt's supported external-destruction path. Direct
        # Shiboken.delete() on a still-parented Python subclass can fast-fail
        # inside PySide6 6.2.4 on Windows before Qt finishes its destroyed
        # signal chain, so that low-level wrapper operation is not part of the
        # public ContentDialog lifecycle contract.
        external.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QCoreApplication.processEvents()
        self.assertFalse(Shiboken.isValid(external))
        self.assertIsNone(dialog.content())
        self.assertIsNone(dialog._fluentqt_content_record)

        owned = PythonContent()
        dialog.setContent(owned)
        dialog_ref = weakref.ref(dialog)
        del dialog
        gc.collect()
        self.assertIsNone(dialog_ref())
        self.assertFalse(Shiboken.isValid(owned))

    def test_content_dialog_constructor_routes_content_through_facade(self):
        content = QWidget()
        dialog = fluentqt.ContentDialog(content=content)
        self.assertIs(dialog.content(), content)
        self.assertIs(content.parent(), dialog)
        self.assertIs(dialog.takeContent(), content)
        self.assertIsNone(content.parent())

    def test_content_dialog_content_gc_stress(self):
        for _ in range(25):
            dialog = fluentqt.ContentDialog()
            content = QWidget()
            dialog.setContent(content)
            content_ref = weakref.ref(content)
            del content
            gc.collect()
            self.assertIs(dialog.content(), content_ref())

            dialog_ref = weakref.ref(dialog)
            del dialog
            gc.collect()
            self.assertIsNone(dialog_ref())
            self.assertIsNone(content_ref())

    def test_popup_properties_overlay_lifecycle_focus_and_subclassing(self):
        class PythonPopup(fluentqt.Popup):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.show_count = 0

            def showEvent(self, event):
                self.show_count += 1
                super().showEvent(event)

        host = QWidget()
        host.resize(640, 480)
        trigger = fluentqt.Button("Open", host)
        trigger.setGeometry(80, 72, 120, 36)
        trigger.show()
        passthrough = fluentqt.Button("Toolbar", host)
        passthrough.setGeometry(440, 24, 120, 36)
        passthrough.show()
        host.show()
        host.activateWindow()
        trigger.setFocus(Qt.OtherFocusReason)
        QCoreApplication.processEvents()

        popup = PythonPopup(host)
        popup.resize(320, 180)
        popup.setAnimationEnabled(False)
        popup.setExitAnimationEnabled(False)
        popup.setModal(True)
        popup.setDim(True)
        popup.setLightDismissConsumesPress(True)
        popup.setPosition(trigger, QPoint(0, trigger.height() + 8))
        popup.setThemeSource(trigger)
        popup.addLightDismissPassthrough(passthrough)
        close_policy = (
            fluentqt.Popup.CloseFlag.CloseOnPressOutside
            | fluentqt.Popup.CloseFlag.CloseOnEscape
        )
        popup.setClosePolicy(close_policy)

        self.assertIs(dialogs_flyouts.Popup, fluentqt.Popup)
        self.assertFalse(popup.isOpen())
        self.assertTrue(popup.isModal())
        self.assertTrue(popup.isDim())
        self.assertFalse(popup.isAnimationEnabled())
        self.assertFalse(popup.isExitAnimationEnabled())
        self.assertTrue(popup.lightDismissConsumesPress())
        self.assertEqual(popup.closePolicy(), close_policy)

        lifecycle = []
        popup.aboutToShow.connect(lambda: lifecycle.append("aboutToShow"))
        popup.opened.connect(lambda: lifecycle.append("opened"))
        popup.aboutToHide.connect(lambda: lifecycle.append("aboutToHide"))
        popup.closed.connect(lambda: lifecycle.append("closed"))

        popup.open()
        QCoreApplication.processEvents()
        self.assertTrue(popup.isOpen())
        self.assertAlmostEqual(popup.popupProgress(), 1.0)
        self.assertTrue(popup.isVisible())
        self.assertIs(popup.window(), host)
        self.assertIs(QApplication.focusWidget(), popup)
        self.assertGreaterEqual(popup.show_count, 1)
        self.assertEqual(lifecycle, ["aboutToShow", "opened"])
        self.assertIsNotNone(host.findChild(QWidget, "PopupScrim"))

        QTest.keyClick(host, Qt.Key_Escape)
        QCoreApplication.processEvents()
        self.assertFalse(popup.isOpen())
        self.assertAlmostEqual(popup.popupProgress(), 0.0)
        self.assertIs(QApplication.focusWidget(), trigger)
        self.assertEqual(
            lifecycle,
            ["aboutToShow", "opened", "aboutToHide", "closed"],
        )

        popup.setClosePolicy(fluentqt.Popup.CloseFlag.NoAutoClose)
        popup.open()
        QCoreApplication.processEvents()
        QTest.keyClick(host, Qt.Key_Escape)
        QCoreApplication.processEvents()
        self.assertTrue(popup.isOpen())
        popup.close()
        QCoreApplication.processEvents()
        host.close()

    def test_popup_dependency_facade_retains_and_releases_widgets(self):
        popup = fluentqt.Popup()
        with self.assertRaisesRegex(TypeError, "position anchor"):
            popup.setPosition(None, QPoint())
        with self.assertRaisesRegex(ValueError, "position anchor"):
            popup.setPosition(popup, QPoint())
        with self.assertRaisesRegex(ValueError, "theme source"):
            popup.setThemeSource(popup)
        with self.assertRaisesRegex(ValueError, "passthrough"):
            popup.addLightDismissPassthrough(popup)

        anchor = QWidget()
        theme_source = QWidget()
        passthrough = QWidget()
        popup.setPosition(anchor, QPoint(12, 18))
        popup.setThemeSource(theme_source)
        popup.addLightDismissPassthrough(passthrough)
        popup.addLightDismissPassthrough(passthrough)

        anchor_ref = weakref.ref(anchor)
        theme_ref = weakref.ref(theme_source)
        passthrough_ref = weakref.ref(passthrough)
        del anchor
        del theme_source
        del passthrough
        gc.collect()
        self.assertIsNotNone(anchor_ref())
        self.assertIsNotNone(theme_ref())
        self.assertIsNotNone(passthrough_ref())
        self.assertEqual(len(popup._fluentqt_passthrough_records), 1)

        popup.setThemeSource(None)
        popup.clearLightDismissPassthrough()
        gc.collect()
        self.assertIsNone(theme_ref())
        self.assertIsNone(passthrough_ref())
        self.assertIsNone(popup._fluentqt_theme_source_record)
        self.assertEqual(popup._fluentqt_passthrough_records, {})

        retained_anchor = anchor_ref()
        retained_anchor.deleteLater()
        del retained_anchor
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QCoreApplication.processEvents()
        gc.collect()
        self.assertIsNone(popup._fluentqt_position_anchor_record)
        self.assertIsNone(anchor_ref())

    def test_popup_dependency_gc_stress(self):
        for _ in range(25):
            popup = fluentqt.Popup()
            anchor = QWidget()
            theme_source = QWidget()
            passthrough = QWidget()
            popup.setPosition(anchor, QPoint(4, 8))
            popup.setThemeSource(theme_source)
            popup.addLightDismissPassthrough(passthrough)

            dependency_refs = tuple(
                weakref.ref(widget)
                for widget in (anchor, theme_source, passthrough)
            )
            del anchor
            del theme_source
            del passthrough
            gc.collect()
            self.assertTrue(all(ref() is not None for ref in dependency_refs))

            popup_ref = weakref.ref(popup)
            del popup
            gc.collect()
            self.assertIsNone(popup_ref())
            self.assertTrue(all(ref() is None for ref in dependency_refs))

    def test_flyout_placement_overlay_lifecycle_and_subclassing(self):
        class PythonFlyout(fluentqt.Flyout):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.show_count = 0

            def showEvent(self, event):
                self.show_count += 1
                super().showEvent(event)

        host = QWidget()
        host.resize(640, 480)
        anchor = fluentqt.Button("Open", host)
        anchor.setGeometry(260, 180, 120, 36)
        anchor.show()
        host.show()
        host.activateWindow()
        anchor.setFocus(Qt.OtherFocusReason)
        QCoreApplication.processEvents()

        flyout = PythonFlyout(host)
        flyout.setFixedSize(320, 180)
        flyout.setAnimationEnabled(False)
        flyout.setExitAnimationEnabled(False)
        flyout.setAnchorOffset(12)
        flyout.setClampToWindow(True)

        self.assertIs(dialogs_flyouts.Flyout, fluentqt.Flyout)
        self.assertEqual(flyout.placement(), fluentqt.Flyout.Placement.Bottom)
        self.assertEqual(flyout.anchorOffset(), 12)
        self.assertTrue(flyout.clampToWindow())
        self.assertFalse(flyout.isModal())
        self.assertFalse(flyout.isDim())
        self.assertEqual(
            flyout.closePolicy(),
            fluentqt.Flyout.CloseFlag.CloseOnPressOutside
            | fluentqt.Flyout.CloseFlag.CloseOnEscape,
        )

        placements = []
        flyout.placementChanged.connect(placements.append)
        flyout.setPlacement(fluentqt.Flyout.Placement.Right)
        flyout.setPlacement(fluentqt.Flyout.Placement.Right)
        flyout.setPlacement(fluentqt.Flyout.Placement.Bottom)
        self.assertEqual(
            placements,
            [
                fluentqt.Flyout.Placement.Right,
                fluentqt.Flyout.Placement.Bottom,
            ],
        )

        lifecycle = []
        flyout.aboutToShow.connect(lambda: lifecycle.append("aboutToShow"))
        flyout.opened.connect(lambda: lifecycle.append("opened"))
        flyout.aboutToHide.connect(lambda: lifecycle.append("aboutToHide"))
        flyout.closed.connect(lambda: lifecycle.append("closed"))
        flyout.showAt(anchor)
        QCoreApplication.processEvents()

        self.assertTrue(flyout.isOpen())
        self.assertIs(flyout.anchor(), anchor)
        self.assertIs(flyout.window(), host)
        self.assertGreaterEqual(flyout.show_count, 1)
        self.assertEqual(lifecycle, ["aboutToShow", "opened"])
        visible_card = flyout.geometry().adjusted(16, 16, -16, -16)
        self.assertEqual(
            visible_card.top(),
            anchor.geometry().bottom() + flyout.anchorOffset(),
        )
        scrim = host.findChild(QWidget, "PopupScrim")
        self.assertTrue(scrim is None or not scrim.isVisible())

        QTest.keyClick(host, Qt.Key_Escape)
        QCoreApplication.processEvents()
        self.assertFalse(flyout.isOpen())
        self.assertIs(QApplication.focusWidget(), anchor)
        self.assertEqual(
            lifecycle,
            ["aboutToShow", "opened", "aboutToHide", "closed"],
        )

        anchor.move(260, 430)
        flyout.setPlacement(fluentqt.Flyout.Placement.Auto)
        flyout.showAt(anchor)
        QCoreApplication.processEvents()
        visible_card = flyout.geometry().adjusted(16, 16, -16, -16)
        self.assertLess(visible_card.bottom(), anchor.geometry().top())

        anchor.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QCoreApplication.processEvents()
        self.assertFalse(flyout.isOpen())
        self.assertIsNone(flyout.anchor())
        self.assertIsNone(flyout._fluentqt_flyout_anchor_record)
        host.close()

    def test_flyout_dependency_facade_retains_and_releases_widgets(self):
        flyout = fluentqt.Flyout()
        with self.assertRaisesRegex(TypeError, "Flyout anchor"):
            flyout.showAt(None)
        with self.assertRaisesRegex(TypeError, "Flyout anchor"):
            flyout.setAnchor(object())
        with self.assertRaisesRegex(TypeError, "Flyout anchor"):
            flyout.showAt(object())
        with self.assertRaisesRegex(ValueError, "Flyout.*anchor"):
            flyout.setAnchor(flyout)
        with self.assertRaisesRegex(ValueError, "Flyout.*anchor"):
            flyout.showAt(flyout)

        anchor = QWidget()
        replacement = QWidget()
        theme_source = QWidget()
        passthrough = QWidget()
        flyout.setAnchor(anchor)
        flyout.setThemeSource(theme_source)
        flyout.addLightDismissPassthrough(passthrough)

        anchor_ref = weakref.ref(anchor)
        replacement_ref = weakref.ref(replacement)
        theme_ref = weakref.ref(theme_source)
        passthrough_ref = weakref.ref(passthrough)
        del anchor
        del theme_source
        del passthrough
        gc.collect()
        self.assertIs(flyout.anchor(), anchor_ref())
        self.assertIsNotNone(theme_ref())
        self.assertIsNotNone(passthrough_ref())

        flyout.setAnchor(replacement)
        del replacement
        gc.collect()
        self.assertIsNone(anchor_ref())
        self.assertIs(flyout.anchor(), replacement_ref())

        flyout.setAnchor(None)
        flyout.setThemeSource(None)
        flyout.clearLightDismissPassthrough()
        gc.collect()
        self.assertIsNone(replacement_ref())
        self.assertIsNone(theme_ref())
        self.assertIsNone(passthrough_ref())
        self.assertIsNone(flyout._fluentqt_flyout_anchor_record)

    def test_flyout_show_at_preserves_reentrant_anchor_change(self):
        host = QWidget()
        host.resize(640, 480)
        original = fluentqt.Button("Original", host)
        replacement = fluentqt.Button("Replacement", host)
        original.setGeometry(80, 80, 120, 36)
        replacement.setGeometry(420, 80, 120, 36)
        original.show()
        replacement.show()
        host.show()

        flyout = fluentqt.Flyout(host)
        flyout.setAnimationEnabled(False)
        flyout.setExitAnimationEnabled(False)
        flyout.aboutToShow.connect(lambda: flyout.setAnchor(replacement))
        flyout.showAt(original)
        QCoreApplication.processEvents()

        self.assertIs(flyout.anchor(), replacement)
        self.assertIs(
            flyout._fluentqt_flyout_anchor_record[0],
            replacement,
        )
        flyout.close()
        host.close()

    def test_flyout_dependency_gc_stress(self):
        for _ in range(25):
            flyout = fluentqt.Flyout()
            anchor = QWidget()
            theme_source = QWidget()
            passthrough = QWidget()
            flyout.setAnchor(anchor)
            flyout.setThemeSource(theme_source)
            flyout.addLightDismissPassthrough(passthrough)

            dependency_refs = tuple(
                weakref.ref(widget)
                for widget in (anchor, theme_source, passthrough)
            )
            del anchor
            del theme_source
            del passthrough
            gc.collect()
            self.assertTrue(all(ref() is not None for ref in dependency_refs))

            flyout_ref = weakref.ref(flyout)
            del flyout
            gc.collect()
            self.assertIsNone(flyout_ref())
            self.assertTrue(all(ref() is None for ref in dependency_refs))

    def test_coach_mark_same_window_lifecycle_and_subclassing(self):
        class PythonCoachMark(fluentqt.CoachMark):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.show_count = 0

            def showEvent(self, event):
                self.show_count += 1
                super().showEvent(event)

        host = QWidget()
        host.resize(720, 520)
        target = fluentqt.Button("Target", host)
        target.setGeometry(260, 120, 120, 36)
        target.show()
        host.show()

        coach = PythonCoachMark(host)
        coach.setCardSize(QSize(300, 150))
        coach.setPlacement(fluentqt.CoachMark.Placement.Bottom)
        content_host = coach.contentHost()
        content = fluentqt.Label("Python content", content_host)
        content.show()

        self.assertIs(dialogs_flyouts.CoachMark, fluentqt.CoachMark)
        self.assertFalse(coach.isOpen())
        self.assertEqual(coach.cardSize(), QSize(300, 150))
        self.assertEqual(
            coach.surfaceMode(),
            fluentqt.CoachMark.SurfaceMode.SameWindowSurface,
        )
        self.assertIs(content_host.parent(), coach)
        self.assertFalse(hasattr(fluentqt.CoachMark, "onThemeUpdated"))

        lifecycle = []
        coach.openChanged.connect(
            lambda opened: lifecycle.append(("changed", opened))
        )
        coach.opened.connect(lambda: lifecycle.append(("opened", True)))
        coach.closed.connect(lambda: lifecycle.append(("closed", False)))
        opened_spy = QSignalSpy(coach.opened)
        closed_spy = QSignalSpy(coach.closed)
        coach.setTarget(target)
        coach.open()
        QCoreApplication.processEvents()

        self.assertTrue(coach.isOpen())
        self.assertIs(coach.target(), target)
        self.assertIs(coach.window(), host)
        self.assertFalse(coach.isWindow())
        self.assertGreaterEqual(coach.show_count, 1)
        self.assertEqual(lifecycle, [("changed", True)])
        if opened_spy.count() == 0:
            self.assertTrue(opened_spy.wait(1000))
        self.assertEqual(
            lifecycle[:2],
            [("changed", True), ("opened", True)],
        )

        coach.close()
        self.assertFalse(coach.isOpen())
        self.assertEqual(lifecycle[-1], ("changed", False))
        if closed_spy.count() == 0:
            self.assertTrue(closed_spy.wait(1000))
        self.assertEqual(
            lifecycle[-2:],
            [("changed", False), ("closed", False)],
        )

        coach.setTarget(target)
        target.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QCoreApplication.processEvents()
        gc.collect()
        self.assertIsNone(coach.target())
        self.assertIsNone(coach._fluentqt_coach_mark_target_record)

        host.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QCoreApplication.processEvents()
        self.assertFalse(Shiboken.isValid(coach))
        self.assertFalse(Shiboken.isValid(content_host))
        self.assertFalse(Shiboken.isValid(content))

    def test_teaching_tip_lifecycle_close_reason_and_subclassing(self):
        class PythonTeachingTip(fluentqt.TeachingTip):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.show_count = 0

            def showEvent(self, event):
                self.show_count += 1
                super().showEvent(event)

        host = QWidget()
        host.resize(760, 560)
        target = fluentqt.Button("Learn more", host)
        target.setGeometry(300, 120, 140, 36)
        target.show()
        host.show()

        tip = PythonTeachingTip(host)
        tip.setAnimationEnabled(False)
        tip.setExitAnimationEnabled(False)
        tip.setCardSize(QSize(340, 180))
        tip.setPreferredPlacement(
            fluentqt.TeachingTip.PreferredPlacement.Bottom
        )
        tip.setPlacementMargin(8)
        tip.setTailVisible(True)
        tip.setLightDismissEnabled(True)
        content_host = tip.contentHost()
        content = fluentqt.Label("Teaching content", content_host)
        content.show()

        self.assertIs(dialogs_flyouts.TeachingTip, fluentqt.TeachingTip)
        self.assertIsInstance(tip, fluentqt.Popup)
        self.assertEqual(tip.cardSize(), QSize(340, 180))
        self.assertEqual(tip.placementMargin(), 8)
        self.assertTrue(tip.isTailVisible())
        self.assertTrue(tip.isLightDismissEnabled())
        self.assertIs(content_host.parent(), tip)
        for internal_name in (
            "onThemeUpdated",
            "computePosition",
            "automaticPositionAnchor",
        ):
            self.assertFalse(hasattr(fluentqt.TeachingTip, internal_name))

        close_reasons = []
        tip.closing.connect(close_reasons.append)
        tip.showAt(target)
        QCoreApplication.processEvents()

        self.assertTrue(tip.isOpen())
        self.assertIs(tip.target(), target)
        self.assertIs(tip.window(), host)
        self.assertFalse(tip.isWindow())
        self.assertGreaterEqual(tip.show_count, 1)

        tip.closeWithReason(fluentqt.TeachingTip.CloseReason.ActionButton)
        QCoreApplication.processEvents()
        self.assertFalse(tip.isOpen())
        self.assertEqual(
            close_reasons[-1],
            fluentqt.TeachingTip.CloseReason.ActionButton,
        )

        replacement = fluentqt.Button("Replacement", host)
        replacement.setGeometry(300, 360, 140, 36)
        replacement.show()
        tip.showAt(replacement)
        self.assertTrue(tip.isOpen())
        replacement.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QCoreApplication.processEvents()
        gc.collect()
        self.assertFalse(tip.isOpen())
        self.assertIsNone(tip.target())
        self.assertIsNone(tip._fluentqt_teaching_tip_target_record)
        self.assertEqual(
            close_reasons[-1],
            fluentqt.TeachingTip.CloseReason.TargetDestroyed,
        )

        with self.assertRaisesRegex(TypeError, "TeachingTip target"):
            tip.showAt(None)
        with self.assertRaisesRegex(ValueError, "TeachingTip.*target"):
            tip.setTarget(tip)

        host.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QCoreApplication.processEvents()
        self.assertFalse(Shiboken.isValid(tip))
        self.assertFalse(Shiboken.isValid(content_host))
        self.assertFalse(Shiboken.isValid(content))

    def test_guidance_overlay_target_dependency_gc_stress(self):
        for _ in range(25):
            coach = fluentqt.CoachMark()
            tip = fluentqt.TeachingTip()
            coach_target = QWidget()
            tip_target = QWidget()
            coach.setTarget(coach_target)
            tip.setTarget(tip_target)

            coach_target_ref = weakref.ref(coach_target)
            tip_target_ref = weakref.ref(tip_target)
            del coach_target
            del tip_target
            gc.collect()
            self.assertIs(coach.target(), coach_target_ref())
            self.assertIs(tip.target(), tip_target_ref())

            coach_ref = weakref.ref(coach)
            tip_ref = weakref.ref(tip)
            del coach
            del tip
            gc.collect()
            self.assertIsNone(coach_ref())
            self.assertIsNone(tip_ref())
            self.assertIsNone(coach_target_ref())
            self.assertIsNone(tip_target_ref())

    def test_drawer_view_properties_overlay_lifecycle_and_subclassing(self):
        class PythonDrawer(fluentqt.DrawerView):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.show_count = 0

            def showEvent(self, event):
                self.show_count += 1
                super().showEvent(event)

        host = QWidget()
        host.resize(640, 480)
        host.show()
        drawer = PythonDrawer(host)
        drawer.setAnimationEnabled(False)
        drawer.setDrawerLength(220)
        drawer.setAvailableMargins(QMargins(10, 20, 30, 40))
        drawer.setEdge(fluentqt.DrawerView.DrawerEdge.Right)

        self.assertIs(collections.DrawerView, fluentqt.DrawerView)
        self.assertFalse(drawer.isOpen())
        self.assertAlmostEqual(drawer.position(), 0.0)
        self.assertTrue(drawer.isModal())
        self.assertTrue(drawer.isDim())
        self.assertTrue(drawer.isInteractive())
        self.assertEqual(drawer.dragMargin(), 24)
        self.assertEqual(drawer.outerCornerRadius(), 8)
        self.assertFalse(drawer.isAnimationEnabled())
        self.assertEqual(
            drawer.edge(),
            fluentqt.DrawerView.DrawerEdge.Right,
        )
        default_policy = (
            fluentqt.DrawerView.CloseFlag.CloseOnPressOutside
            | fluentqt.DrawerView.CloseFlag.CloseOnEscape
        )
        self.assertEqual(drawer.closePolicy(), default_policy)

        lifecycle = []
        drawer.aboutToShow.connect(lambda: lifecycle.append("aboutToShow"))
        drawer.opened.connect(lambda: lifecycle.append("opened"))
        drawer.aboutToHide.connect(lambda: lifecycle.append("aboutToHide"))
        drawer.closed.connect(lambda: lifecycle.append("closed"))

        drawer.open()
        QCoreApplication.processEvents()
        self.assertTrue(drawer.isOpen())
        self.assertAlmostEqual(drawer.position(), 1.0)
        self.assertTrue(drawer.isVisible())
        self.assertIs(drawer.window(), host)
        self.assertTrue(drawer.panelGeometry().isValid())
        self.assertTrue(drawer.contentGeometry().isValid())
        self.assertEqual(drawer.scrimGeometry(), host.rect())
        self.assertGreaterEqual(drawer.show_count, 1)
        self.assertEqual(lifecycle, ["aboutToShow", "opened"])

        scrim = host.findChild(QWidget, "DrawerViewScrim")
        self.assertIsNotNone(scrim)
        QTest.mouseClick(scrim, Qt.LeftButton, pos=scrim.rect().center())
        QCoreApplication.processEvents()
        self.assertFalse(drawer.isOpen())
        self.assertEqual(
            lifecycle,
            ["aboutToShow", "opened", "aboutToHide", "closed"],
        )

        drawer.setClosePolicy(
            fluentqt.DrawerView.CloseFlag.CloseOnEscape
        )
        drawer.open()
        QCoreApplication.processEvents()
        QTest.keyClick(host, Qt.Key_Escape)
        QCoreApplication.processEvents()
        self.assertFalse(drawer.isOpen())

        drawer.setClosePolicy(fluentqt.DrawerView.CloseFlag.NoAutoClose)
        drawer.open()
        QCoreApplication.processEvents()
        QTest.keyClick(host, Qt.Key_Escape)
        QCoreApplication.processEvents()
        self.assertTrue(drawer.isOpen())
        drawer.close()
        QCoreApplication.processEvents()

    def test_drawer_view_constructor_and_content_contract_validation(self):
        content = QWidget()
        drawer = fluentqt.DrawerView(contentWidget=content)
        self.assertIs(drawer.contentWidget(), content)
        self.assertEqual(
            drawer.contentOwnership(),
            fluentqt.WidgetOwnership.Borrowed,
        )
        self.assertIs(drawer._fluentqt_content_record[0], content)

        with self.assertRaisesRegex(TypeError, "contentOwnership"):
            fluentqt.DrawerView(
                contentOwnership=fluentqt.WidgetOwnership.Owned
            )
        with self.assertRaisesRegex(ValueError, "host or its ancestor"):
            drawer.setOwnedContentWidget(drawer)

        owner = QWidget()
        nested = fluentqt.DrawerView(owner)
        nested_content = QWidget()
        nested.setBorrowedContentWidget(nested_content)
        with self.assertRaisesRegex(ValueError, "host or its ancestor"):
            nested.setReparentedContentWidget(owner)
        self.assertIs(nested.contentWidget(), nested_content)

        with self.assertRaisesRegex(ValueError, "takeContentWidget"):
            drawer.setOwnedContentWidget(content)
        self.assertIs(drawer.contentWidget(), content)
        taken = drawer.takeContentWidget()
        self.assertIs(taken, content)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))
        self.assertIsNone(drawer._fluentqt_content_record)

    def test_drawer_view_owned_content_lifecycle(self):
        drawer = fluentqt.DrawerView()
        first = QWidget()
        second = QWidget()

        self.assertTrue(drawer.setOwnedContentWidget(first))
        self.assertIs(first.parent(), drawer)
        self.assertEqual(
            drawer.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )
        self.assertTrue(drawer.setOwnedContentWidget(second))
        self.assertFalse(Shiboken.isValid(first))
        self.assertIs(drawer.contentWidget(), second)

        drawer_ref = weakref.ref(drawer)
        del drawer
        gc.collect()
        self.assertIsNone(drawer_ref())
        self.assertFalse(Shiboken.isValid(second))

    def test_drawer_view_borrowed_content_lifecycle_and_external_delete(self):
        class PythonContent(QWidget):
            def __init__(self):
                super().__init__()
                self.marker = "drawer-borrowed"

        drawer = fluentqt.DrawerView()
        first = PythonContent()
        first_ref = weakref.ref(first)
        self.assertTrue(drawer.setContentWidget(first))
        del first
        gc.collect()

        hosted = drawer.contentWidget()
        self.assertIs(hosted, first_ref())
        self.assertEqual(hosted.marker, "drawer-borrowed")
        self.assertIs(drawer._fluentqt_content_record[0], hosted)

        second = QWidget()
        self.assertTrue(drawer.setBorrowedContentWidget(second))
        self.assertTrue(Shiboken.isValid(hosted))
        self.assertIsNone(hosted.parent())

        second.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QCoreApplication.processEvents()
        self.assertIsNone(drawer.contentWidget())
        self.assertIsNone(drawer._fluentqt_content_record)
        self.assertEqual(
            drawer.contentOwnership(),
            fluentqt.WidgetOwnership.Borrowed,
        )

        replacement = QWidget()
        drawer.setBorrowedContentWidget(replacement)
        drawer_ref = weakref.ref(drawer)
        del drawer
        gc.collect()
        self.assertIsNone(drawer_ref())
        self.assertTrue(Shiboken.isValid(replacement))
        self.assertIsNone(replacement.parent())

    def test_drawer_view_reparented_content_lifecycle(self):
        first_parent = QWidget()
        first = QWidget(first_parent)
        second_parent = QWidget()
        second = QWidget(second_parent)
        drawer = fluentqt.DrawerView()

        self.assertTrue(drawer.setReparentedContentWidget(first))
        self.assertIs(first.parent(), drawer)
        self.assertIs(drawer._fluentqt_content_record[2], first_parent)
        self.assertTrue(drawer.setReparentedContentWidget(second))
        self.assertIs(first.parent(), first_parent)
        self.assertIs(second.parent(), drawer)

        self.assertTrue(drawer.setReparentedContentWidget(None))
        self.assertIs(second.parent(), second_parent)
        self.assertIsNone(drawer.contentWidget())
        self.assertEqual(
            drawer.contentOwnership(),
            fluentqt.WidgetOwnership.Borrowed,
        )

        restore_parent = QWidget()
        restored = QWidget(restore_parent)
        restoring_drawer = fluentqt.DrawerView()
        restoring_drawer.setReparentedContentWidget(restored)
        restore_parent_ref = weakref.ref(restore_parent)
        del restore_parent
        gc.collect()
        self.assertIsNotNone(restore_parent_ref())

        restoring_drawer_ref = weakref.ref(restoring_drawer)
        del restoring_drawer
        gc.collect()
        self.assertIsNone(restoring_drawer_ref())
        self.assertTrue(Shiboken.isValid(restored))
        self.assertIs(restored.parent(), restore_parent_ref())

    def test_drawer_view_owned_gc_stress(self):
        for _ in range(25):
            drawer = fluentqt.DrawerView()
            content = QWidget()
            drawer.setOwnedContentWidget(content)
            drawer_ref = weakref.ref(drawer)
            del drawer
            gc.collect()
            self.assertIsNone(drawer_ref())
            self.assertFalse(Shiboken.isValid(content))
            del content

    def test_drawer_view_borrowed_gc_stress(self):
        for _ in range(25):
            drawer = fluentqt.DrawerView()
            content = QWidget()
            drawer.setBorrowedContentWidget(content)
            drawer_ref = weakref.ref(drawer)
            del drawer
            gc.collect()
            self.assertIsNone(drawer_ref())
            self.assertTrue(Shiboken.isValid(content))
            self.assertIsNone(content.parent())
            del content
            gc.collect()

    def test_drawer_view_reparented_gc_stress(self):
        for _ in range(25):
            original_parent = QWidget()
            content = QWidget(original_parent)
            drawer = fluentqt.DrawerView()
            drawer.setReparentedContentWidget(content)
            drawer_ref = weakref.ref(drawer)
            del drawer
            gc.collect()
            self.assertIsNone(drawer_ref())
            self.assertTrue(Shiboken.isValid(content))
            self.assertIs(content.parent(), original_parent)
            del original_parent
            gc.collect()
            self.assertFalse(Shiboken.isValid(content))
            del content

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

        keyword_options = fluentqt.SplitViewPaneOptions(
            minimumPaneSize=30,
            preferredPaneSize=90,
            maximumPaneSize=240,
            fillPane=True,
        )
        self.assertEqual(keyword_options.minimumSize, 30)
        self.assertEqual(keyword_options.preferredSize, 90)
        self.assertEqual(keyword_options.maximumSize, 240)
        self.assertTrue(keyword_options.fill)

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

        for unsupported in ("header", "setHeader", "footer", "setFooter"):
            self.assertFalse(hasattr(view, unsupported), unsupported)
        with self.assertRaisesRegex(TypeError, "header QWidget hosting"):
            fluentqt.ListView(header=QWidget())

        grouped = fluentqt.ListView(
            sectionEnabled=True,
            sectionKeyFunction=lambda row: "A" if row < 2 else "G",
        )
        grouped_model = QStringListModel(["Alpha", "Beta", "Gamma"])
        grouped.setModel(grouped_model)
        self.assertTrue(grouped.sectionEnabled())
        self.assertTrue(grouped.isSectionEnabled())
        grouped.setSectionEnabled(False)
        self.assertFalse(grouped.sectionEnabled())
        grouped.setSectionKeyFunction(lambda row: grouped_model.data(
            grouped_model.index(row, 0)
        )[0])
        grouped_model.setStringList(["Alpha", "Beta", "Gamma", "Golf"])
        self.assertEqual(grouped_model.rowCount(), 4)
        with self.assertRaisesRegex(TypeError, "must return str"):
            grouped.setSectionKeyFunction(lambda _row: 7)
        self.assertEqual(
            grouped._fluentqt_section_key_function(3),
            "G",
        )
        grouped.setSectionKeyFunction(None)

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

    def test_data_grid_public_surface_and_caller_owned_dependencies(self):
        self.assertTrue(issubclass(fluentqt.DataGrid, QTableView))
        self.assertIs(collections.DataGrid, fluentqt.DataGrid)
        self.assertIs(
            fluentqt.DataGrid.SelectionMode,
            fluentqt.SelectionMode,
        )

        view = fluentqt.DataGrid(
            selectionMode=fluentqt.SelectionMode.Extended,
            placeholderText="No rows",
            borderVisible=False,
            backgroundVisible=False,
            scrollChainingEnabled=True,
        )
        self.assertEqual(
            view.selectionMode(),
            fluentqt.SelectionMode.Extended,
        )
        self.assertEqual(view.placeholderText(), "No rows")
        self.assertFalse(view.isBorderVisible())
        self.assertFalse(view.isBackgroundVisible())
        self.assertTrue(view.isScrollChainingEnabled())
        self.assertTrue(view.isShowingPlaceholder())

        class PythonTableModel(QAbstractTableModel):
            def rowCount(self, parent=QModelIndex()):
                return 0 if parent.isValid() else 2

            def columnCount(self, parent=QModelIndex()):
                return 0 if parent.isValid() else 2

            def data(self, index, role=Qt.DisplayRole):
                if index.isValid() and role == Qt.DisplayRole:
                    return f"R{index.row()} C{index.column()}"
                return None

        model = PythonTableModel()
        model_ref = weakref.ref(model)
        view.setModel(model)
        del model
        gc.collect()
        self.assertIs(view.model(), model_ref())
        self.assertFalse(view.isShowingPlaceholder())

        delegate = QStyledItemDelegate()
        delegate_ref = weakref.ref(delegate)
        view.setItemDelegate(delegate)
        del delegate
        gc.collect()
        self.assertIs(view.itemDelegate(), delegate_ref())

        selection = QItemSelectionModel(model_ref())
        selection_ref = weakref.ref(selection)
        view.setSelectionModel(selection)
        del selection
        gc.collect()
        self.assertIs(view.selectionModel(), selection_ref())

        changes = []
        view.selectionModeChanged.connect(lambda: changes.append(True))
        view.setSelectionMode(fluentqt.SelectionMode.Single)
        view.setSelectionMode(fluentqt.SelectionMode.Single)
        self.assertEqual(changes, [True])

        for method_name in (
            "selectionMode",
            "setSelectionMode",
            "verticalFluentScrollBar",
            "horizontalFluentScrollBar",
        ):
            self.assertNotIn(
                method_name,
                native.fluent.DataGrid.__dict__,
            )
            self.assertIn(method_name, fluentqt.DataGrid.__dict__)
        for bar in (
            view.verticalFluentScrollBar(),
            view.horizontalFluentScrollBar(),
        ):
            self.assertTrue(Shiboken.isValid(bar))
            self.assertFalse(Shiboken.ownedByPython(bar))

        view_ref = weakref.ref(view)
        del view
        self.app.processEvents()
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertIsNone(model_ref())
        self.assertIsNone(delegate_ref())
        self.assertIsNone(selection_ref())

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
            backgroundVisible=False,
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
        self.assertFalse(view.backgroundVisible())

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
        view.setSelectionIndicatorHeight(14.0)
        self.assertEqual(view.selectionIndicatorHeight(), 14.0)

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

    def test_scroll_view_zoom_aware_widget_uses_native_interface_dispatch(self):
        class ZoomAwareCanvas(fluentqt.ScrollViewZoomAwareWidget):
            def __init__(self):
                super().__init__()
                self.factors = []

            def scrollViewUnscaledSize(self):
                return QSizeF(560, 360)

            def setScrollViewZoomFactor(self, factor):
                self.factors.append(float(factor))
                self.resize(round(560 * factor), round(360 * factor))

        view = fluentqt.ScrollView()
        view.setZoomMode(fluentqt.ScrollView.ZoomMode.Enabled)
        canvas = ZoomAwareCanvas()
        view.setOwnedContentWidget(canvas)
        self.assertEqual(canvas.factors, [1.0])
        view.zoomTo(1.5, False)
        self.assertEqual(canvas.factors, [1.0, 1.5])
        self.assertEqual(canvas.size(), QSize(840, 540))

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
