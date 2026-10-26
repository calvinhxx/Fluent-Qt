"""Exact native-parity tests for the standalone Python Gallery package."""

from __future__ import annotations

import ast
from collections.abc import Callable
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from tempfile import TemporaryDirectory
import traceback
import unittest
from unittest.mock import patch
import uuid

import fluentqt
from fluentqt._qt_compat import delete_qobject
from _test_font_environment import initialize_headless_test_fixed_font
import fluentqt_gallery.application_controller as application_controller_module
import fluentqt_gallery.settings as gallery_settings_module
import fluentqt_gallery.single_instance as single_instance_module
import shiboken6

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import (
    QAbstractAnimation,
    QCoreApplication,
    QElapsedTimer,
    QEvent,
    QEventLoop,
    QMargins,
    QPoint,
    QPointF,
    QPropertyAnimation,
    QRect,
    QRectF,
    QSettings,
    QSize,
    Qt,
    QTimer,
    QUrl,
)
from PySide6.QtGui import (
    QAction,
    QColor,
    QCloseEvent,
    QFont,
    QFontDatabase,
    QHelpEvent,
    QIcon,
    QImage,
    QMouseEvent,
    QPalette,
    QPainter,
    QPixmap,
)
from PySide6.QtTest import QSignalSpy, QTest
from PySide6.QtSvg import QSvgRenderer
from PySide6.QtWidgets import (
    QAbstractItemView,
    QApplication,
    QFrame,
    QGraphicsOpacityEffect,
    QHBoxLayout,
    QLabel,
    QSizePolicy,
    QVBoxLayout,
    QWidget,
)

from fluentqt_gallery.app import (
    _prepare_qml_bridge_application,
    _normalize_snapshot_capture,
    _pending_gallery_network_requests,
    _reset_current_page_scroll,
    _snapshot_canvas_color,
    normalize_route,
    parse_args,
    runtime_catalog_errors,
    save_snapshot,
)
from fluentqt_gallery.application_controller import (
    CloseBehaviorPromptContent,
    GalleryApplicationController,
    keep_running_choice,
)
from fluentqt_gallery.spatial_support import SPATIAL_AVAILABLE
from fluentqt_gallery.qmlbridge_support import QML_BRIDGE_AVAILABLE

from fluentqt_gallery.catalog import (
    CATEGORIES,
    CATEGORY_BY_ID,
    CONTRACT,
    ENTRIES,
    ENTRY_BY_ROUTE_ID,
    ROUTES,
    ROUTE_BY_ID,
    SAMPLE_BY_KEY,
    SUPPORT_TYPES,
    catalog_coverage_errors,
    entries_for_category,
)
from fluentqt_gallery.foundation_pages import (
    GalleryIconBrowser,
    TypographyRampCard,
    _catalog_glyphs_for_size,
    _icon_font,
    _load_icon_catalog,
    _theme_snapshot,
    _theme_tokens,
)
from fluentqt_gallery.identity import (
    APPLICATION_ID,
    APPLICATION_NAME,
    ORGANIZATION_NAME,
)
from fluentqt_gallery.metrics import (
    TITLE_BAR_HEIGHT,
    drawer_title_bar_avoidance_margins,
)
from fluentqt_gallery.native_samples import (
    _CPP_ALIGNMENT_MIN_CALL_LENGTH,
    _CPP_DISPLAY_MEMBER_ALIASES,
    _DISPLAY_HARD_LINE_LENGTH,
    _DISPLAY_PREFERRED_LINE_LENGTH,
    _cpp_wrapped_call_names,
    _snake_case,
    ported_sample_keys,
)
from fluentqt_gallery.samples import build_sample
from fluentqt_gallery.visual import (
    GalleryCodeBlock,
    GalleryEntryCard,
    GalleryEntryGrid,
    GalleryHomeHero,
    GalleryPageSkeleton,
    GallerySplashScreen,
    _acrylic_noise_tile,
    _direct_icon_font,
    _direct_icon_glyph,
    _draw_pixmap_in_logical_rect,
    _gallery_artwork_pixmap,
    _gallery_artwork_renderer,
    _hero_link_pixmap,
    _image_alpha_bounds,
    _macos_dock_icon_pixmap,
    _qt_seeded_bytes,
    _single_shot,
    _tint_github_mark,
    app_icon,
    app_icon_pixmap,
    css_color,
    control_image_path,
    gallery_font_icon_pixmap,
    gallery_colors,
)
from fluentqt_gallery.update_checker import (
    UpdateResult,
    UpdateStatus,
    compare_versions,
)
from fluentqt_gallery.window import (
    GalleryWindow,
    _build_sample_card,
    _refresh_fluent_subtree,
    build_component_page,
    build_settings_page,
    gallery_window_editing_command_router,
)
from fluentqt_gallery.window_placement import (
    constrain_geometry,
    effective_minimum_size,
    recommended_initial_size,
    restored_geometry,
)
from fluentqt_gallery.settings import CloseBehavior, persistence_available
from fluentqt_gallery.single_instance import (
    GallerySingleInstance,
    StartResult,
    _scoped_instance_name,
)


MANIFEST_PATH = Path(__file__).resolve().parents[2] / "api-manifest.json"
SOURCE_PACKAGE_DIR = Path(__file__).resolve().parents[1] / "src/fluentqt_gallery"
RUNTIME_PACKAGE_DIR = Path(application_controller_module.__file__).resolve().parent
EXPECTED_SUPPORT_TYPES = frozenset(
    {
        "AnchorLayout",
        "AnchorSpec",
        "BindingHandle",
        "AnnotatedScrollBarLabel",
        "BreadcrumbItem",
        "ChartData",
        "ChartModel",
        "CornerRadius",
        "EditingCommandRouter",
        "FluentMenuItem",
        "FluentWidget",
        "FormController",
        "Icons",
        "IconSize",
        "MotionPolicy",
        "PivotItem",
        "PropertyDefinition",
        "ScrollViewZoomAwareWidget",
        "SelectorBarItem",
        "SplitViewPaneOptions",
        "Spacing",
        "StackContentHost",
        "StateGroup",
        "StepperItem",
        "TabViewItem",
        "ThemeTokens",
        "Typography",
    }
)
SEMANTIC_ICON_CATALOG_PATTERN = re.compile(
    r"(?P<quote>['\"])(?:{0})(?P=quote)".format(
        "|".join(
            re.escape(value)
            for name, value in vars(fluentqt.Icons).items()
            if not name.startswith("_") and isinstance(value, str)
        )
    )
)
DISPLAY_PREVIEW_LAYOUT_METHODS = frozenset(
    {
        "addLayout",
        "addSpacing",
        "addStretch",
        "addWidget",
        "setAlignment",
        "setContentsMargins",
        "setSpacing",
        "setStretch",
    }
)
DISPLAY_PREVIEW_LAYOUT_TYPES = frozenset(
    {
        "QFormLayout",
        "QGridLayout",
        "QHBoxLayout",
        "QStackedLayout",
        "QVBoxLayout",
    }
)
CPP_PYTHON_MEMBER_EQUIVALENTS = frozenset(
    {
        "arg",
        "at",
        "backgroundVisible",
        "canExecute",
        "checkState",
        "contentHost",
        "count",
        "currentIndex",
        "dragExclusionRects",
        "height",
        "isAlwaysExpanded",
        "isChecked",
        "isEmpty",
        "isHidden",
        "isReadOnly",
        "isVisible",
        "itemAt",
        "itemCount",
        "labelPosition",
        "layout",
        "minimumWidth",
        "name",
        "pageCount",
        "progressText",
        "property",
        "scrollableHeight",
        "scrollableWidth",
        "size",
        "sizeHint",
        "tabAt",
        "tabCount",
        "text",
        "titleBar",
        "titleBarHeight",
        "toInt",
        "toMap",  # QVariantMap values arrive as native Python dictionaries.
        "toReal",  # QVariant qreal maps to a Python float.
        "toString",
        "toStringList",  # QVariant QStringList values are native Python lists.
        "value",
        "valueToKey",
        "width",
        "window",
    }
)


def _python_language_member_equivalents(module: ast.AST) -> set[str]:
    """Recognize real Python operations corresponding to Qt value methods.

    QString uses Python string operations, and QMap insertion uses dictionary
    assignment. Require the operation in the AST instead of exempting these
    C++ methods from parity checks or matching words in comments and strings.
    """
    equivalents = set()
    dictionaries = {
        ast.unparse(target)
        for node in ast.walk(module)
        if isinstance(node, ast.Assign) and isinstance(node.value, ast.Dict)
        for target in node.targets
    }
    for node in ast.walk(module):
        if (isinstance(node, ast.Compare) and len(node.ops) == 1 and isinstance(node.ops[0], ast.Eq)
                and all(isinstance(value, ast.Call) and isinstance(value.func, ast.Attribute)
                        and value.func.attr == "casefold" and not value.args and not value.keywords
                        for value in (node.left, node.comparators[0]))):
            equivalents.add("compare")
        if (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                and node.func.attr == "items" and not node.args):
            equivalents.update({"cbegin", "cend", "key"})
        if (isinstance(node, ast.Assign) and isinstance(node.value, ast.Constant)
                and node.value.value == ""):
            equivalents.add("clear")
        if isinstance(node, ast.Subscript):
            if (isinstance(node.value, ast.Call) and isinstance(node.value.func, ast.Attribute)
                    and node.value.func.attr == "split" and node.value.args):
                equivalents.add("section")
            if (isinstance(node.ctx, ast.Store)
                    and ast.unparse(node.value) in dictionaries
                    and not isinstance(node.slice, ast.Slice)):
                equivalents.add("insert")
            if isinstance(node.slice, ast.Constant) and node.slice.value == 0:
                equivalents.add("first")
            if (isinstance(node.slice, ast.UnaryOp) and isinstance(node.slice.op, ast.USub)
                    and isinstance(node.slice.operand, ast.Constant) and node.slice.operand.value == 1):
                equivalents.add("last")
        if (
            isinstance(node, ast.Call)
            and isinstance(node.func, ast.Attribute)
            and node.func.attr in {"lower", "strip"}
            and not node.args
            and not node.keywords
        ):
            equivalents.add({"lower": "toLower", "strip": "trimmed"}[node.func.attr])
        elif (isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
              and node.func.id == "float" and len(node.args) == 1):
            equivalents.add("toDouble")
        elif (isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
              and node.func.id == "bool" and len(node.args) == 1):
            equivalents.add("toBool")
        elif (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
              and node.func.attr == "endswith" and node.args):
            equivalents.add("endsWith")
        elif (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
              and node.func.attr == "replace" and len(node.args) == 2
              and isinstance(node.args[1], ast.Constant) and node.args[1].value == ""):
            equivalents.add("remove")
        elif (isinstance(node, ast.BinOp) and isinstance(node.op, ast.Mult)
              and any(isinstance(value, ast.Constant) and isinstance(value.value, str)
                      for value in (node.left, node.right))):
            equivalents.add("repeated")
        elif isinstance(node, ast.Compare) and any(
            isinstance(operator, (ast.In, ast.NotIn)) for operator in node.ops
        ):
            equivalents.add("contains")
    return equivalents


FORBIDDEN_DISPLAY_HELPERS = frozenset(
    {
        "AnnotatedColorSectionsContent",
        "GALLERY_ACCENT_PALETTE",
        "PagerPicture",
        "add_labeled_widget",
        "color_section_labels",
        "gallery_glyph_pixmap",
        "gallery_initials_avatar",
        "make_body_label",
        "make_gradient_pane",
        "make_title_bar_content",
        "make_title_label",
        "make_window_content",
        "set_action_glyph",
    }
)
SELF_CONTAINED_DISPLAY_HELPERS = {
    ("list-view", "list-view-basic"): frozenset(
        {"make_initials_avatar"}
    ),
    ("list-view", "list-view-multi-select"): frozenset(
        {"make_glyph_pixmap"}
    ),
}

# PySide 6.2 can release the Python QApplication wrapper during long-running
# test methods unless the module keeps an explicit strong reference.
_TEST_APPLICATION: QApplication | None = None


def _contract_sample_keys() -> frozenset[tuple[str, str]]:
    return frozenset(
        (component["id"], sample["id"])
        for component in CONTRACT["components"]
        if (SPATIAL_AVAILABLE or component["category_id"] != "spatial")
        and (QML_BRIDGE_AVAILABLE or component["category_id"] != "qml-bridge")
        for sample in component["samples"]
    )


def _take_top_level_widgets(
    namespace: dict[str, object],
) -> tuple[QWidget, ...]:
    top_level_widgets = {
        id(value): value
        for value in namespace.values()
        if (
            isinstance(value, QWidget)
            and shiboken6.isValid(value)
            and value.parent() is None
        )
    }
    for name, value in tuple(namespace.items()):
        if id(value) in top_level_widgets:
            namespace.pop(name, None)
    return tuple(top_level_widgets.values())


def _detach_sample_namespace(widget: QWidget) -> dict[str, object]:
    namespace = getattr(widget, "_fluentqt_gallery_source_namespace", None)
    if hasattr(widget, "_fluentqt_gallery_source_namespace"):
        del widget._fluentqt_gallery_source_namespace
    if not isinstance(namespace, dict):
        return {}
    for name, value in tuple(namespace.items()):
        if value is widget:
            namespace.pop(name, None)
    return namespace


def _qwait(delay_ms: int) -> None:
    loop = QEventLoop()
    QTimer.singleShot(max(0, int(delay_ms)), loop.quit)
    loop.exec()


def _wait_until(
    predicate: Callable[[], bool],
    timeout_ms: int = 2000,
) -> bool:
    timer = QElapsedTimer()
    timer.start()
    while timer.elapsed() < timeout_ms:
        if predicate():
            return True
        _qwait(min(20, timeout_ms - timer.elapsed()))
    return predicate()


class PythonGalleryTest(unittest.TestCase):
    def _action_toasts(self, panel):
        # Managed factories return the native base of the public Toast facade.
        return [widget for widget in panel.window().findChildren(QWidget)
                if isinstance(widget, fluentqt.Toast)
                and widget.updateKey() == "gallery.action-feedback"]

    def test_action_toasts_preserve_inline_state_and_do_not_stack(self):
        for route, sample_id in (("chart-view", "chart-view-export"),
                                 ("property-panel", "property-panel-chart-workbench")):
            with self.subTest(sample=sample_id):
                result = build_sample(route, sample_id)
                panel = result.widget
                panel.resize(1000, 650)
                panel.show()
                QApplication.processEvents()
                chart = panel.findChild(fluentqt.ChartView)
                summary = panel.findChild(fluentqt.Label, "chartWorkbenchStatus")
                before = summary.text() if summary else None
                rows = panel.layout().count()
                chart.imageCopied.emit(True)
                toast = next(iter(self._action_toasts(panel)), None)
                self.assertIsNotNone(toast)
                self.assertEqual(toast.message(), "Image copied")
                self.assertEqual(toast.severity(), fluentqt.Toast.Success)
                self.assertEqual(toast.duration(), 2200)
                chart.dataCopied.emit(False)
                self.assertIs(next(iter(self._action_toasts(panel)), None), toast)
                self.assertEqual(len(self._action_toasts(panel)), 1)
                self.assertEqual(toast.severity(), fluentqt.Toast.Warning)
                self.assertEqual(toast.duration(), 4500)
                self.assertEqual(panel.layout().count(), rows)
                if summary:
                    self.assertEqual(summary.text(), before)
                self.assertIn("showOrUpdateToast", result.preview_source or result.source)
                delete_qobject(panel)

    def test_save_view_toast_preserves_project_counts(self):
        panel = build_sample("data-grid-tool-bar", "data-grid-tool-bar-basic").widget
        summary = panel.findChild(fluentqt.Label, "dataGridSummary")
        before = summary.text()
        panel.findChild(fluentqt.Button, "dataGridSaveView").click()
        toast = next(iter(self._action_toasts(panel)), None)
        self.assertIsNotNone(toast)
        self.assertEqual(toast.message(), "View saved for this demo")
        self.assertEqual(summary.text(), before)
        panel.findChild(fluentqt.Button, "dataGridRestoreView").click()
        self.assertEqual(toast.message(), "View restored")
        self.assertIn("projects", summary.text())
        self.assertEqual(len(self._action_toasts(panel)), 1)
        delete_qobject(panel)

    def test_command_results_use_toasts_and_keep_teaching_sources_aligned(self):
        cases = (
            ("menu", "menu-command-shortcuts", "New"),
            ("menu", "menu-cascading-selection", "Name"),
            ("menu-bar", "menu-bar-hosted-surface", "New"),
            ("menu-bar", "menu-bar-access-keys", "Run"),
            ("command-bar", "command-bar-responsive-overflow", "Add"),
            ("command-bar-flyout", "command-bar-flyout-show-modes", "Share"),
            ("command-bar-flyout", "command-bar-flyout-always-expanded", "Copy link"),
        )
        for route, sample_id, command in cases:
            with self.subTest(sample=sample_id):
                result = build_sample(route, sample_id)
                panel = result.widget
                panel.resize(800, 500)
                panel.show()
                QApplication.processEvents()
                action = next(a for a in panel.findChildren(QAction) if a.text() == command)
                action.trigger()
                toast = next(iter(self._action_toasts(panel)), None)
                self.assertIsNotNone(toast)
                self.assertEqual(toast.message(), command + " selected")
                self.assertEqual(toast.severity(), fluentqt.Toast.Informational)
                self.assertIn("showOrUpdateToast", result.source)
                delete_qobject(panel)

    def test_property_title_preview_updates_clears_and_resets(self):
        result = build_sample("property-panel", "property-panel-basic")
        document = result.widget
        properties = document.findChild(fluentqt.PropertyPanel, "documentProperties")
        title = document.findChild(fluentqt.Label, "propertyDocumentTitle")
        self.assertIsNotNone(properties)
        self.assertIsNotNone(title)
        editor = properties.editor("title")
        document.resize(640, document.sizeHint().height())
        document.show()
        QApplication.processEvents()
        self.assertLess(document.sizeHint().height(), 160)
        editor.setFocus()
        editor.selectAll()
        QTest.keyClicks(editor, "Planning brief <draft>")
        self.assertEqual(title.text(), "Planning brief <draft>")
        self.assertEqual(title.textFormat(), Qt.PlainText)
        editor.selectAll()
        QTest.keyClick(editor, Qt.Key_Backspace)
        self.assertEqual(title.text(), "Untitled document")
        self.assertEqual(properties.value("title"), "")
        self.assertTrue(properties.resetProperty("title"))
        self.assertEqual(title.text(), "Quarterly report")
        QTest.keyClicks(editor, " - project planning and review notes for the next quarter")
        document.resize(320, document.sizeHint().height())
        QApplication.processEvents()
        self.assertLessEqual(document.width(), 320)
        self.assertTrue(title.isTextElided())
        self.assertTrue(document.rect().contains(QRect(editor.mapTo(document, QPoint()), editor.size())))
        document.close()
        delete_qobject(document)

    def test_chart_workbench_edits_reset_and_raw_export(self):
        result = build_sample("property-panel", "property-panel-chart-workbench")
        panel = result.widget
        properties = panel.findChild(fluentqt.PropertyPanel, "workbenchProperties")
        chart = panel.findChild(fluentqt.ChartView, "workbenchChart")
        scope = panel.findChild(fluentqt.ComboBox, "chartExportScope")
        self.assertIsNotNone(properties)
        self.assertIsNotNone(chart)
        number = properties.editor("maximum")
        panel.resize(1000, 650)
        panel.show()
        QApplication.processEvents()
        number.setValue(120)
        self.assertEqual(chart.maximumY(), 120)
        properties.resetProperty("maximum")
        self.assertEqual(chart.maximumY(), 80)
        color = properties.findChild(fluentqt.ColorPicker, "propertyColor_accent")
        color.setColor(QColor("#c24531"))
        self.assertEqual(properties.value("accent"), QColor("#c24531"))
        self.assertEqual(fluentqt.widget_theme_overrides(chart)["light"]["accentDefault"], "#c24531")
        scope.setCurrentIndex(1)
        self.assertEqual(bytes(chart.exportCsv(True)).count(b"\n"), 5)
        self.assertEqual(bytes(chart.exportCsv()).count(b"\n"), 8)
        self.assertFalse(chart.exportImage(2).isNull())
        for width in (1000, 420, 1000):
            panel.resize(width, 1000 if width < 760 else 650)
            QApplication.processEvents()
            self.assertIs(properties.editor("maximum"), number)
            chart_rect = QRect(chart.mapTo(panel, QPoint()), chart.size())
            properties_rect = QRect(properties.mapTo(panel, QPoint()), properties.size())
            self.assertTrue(panel.rect().contains(chart_rect))
            self.assertTrue(panel.rect().contains(properties_rect))
            self.assertFalse(chart_rect.intersects(properties_rect))
        self.assertIn("exportCsv", result.source)
        panel.close()
        delete_qobject(panel)

    def test_atomic_examples_keep_table_and_basic_controls_independent(self):
        from fluentqt_gallery.native_samples import build_native_sample

        for sample_id in ("data-grid-pinned-columns", "data-grid-large-read-only",
                          "data-grid-column-selection", "data-grid-edit-validation"):
            result = build_native_sample("data-grid", sample_id, None)
            try:
                self.assertIsInstance(result.widget, fluentqt.DataGrid)
                self.assertFalse(result.widget.isScrollChainingEnabled())
                self.assertFalse(result.widget.findChildren(fluentqt.DataGridToolBar))
                self.assertNotIn("DataGridToolBar", result.source)
                if sample_id == "data-grid-pinned-columns":
                    self.assertTrue(result.widget.isColumnPinned(0))
                    self.assertEqual(result.widget.model().rowCount(), 2)
                if sample_id == "data-grid-large-read-only":
                    from PySide6.QtGui import QFontMetricsF
                    model = result.widget.model()
                    metrics = QFontMetricsF(model.index(77, 4).data(Qt.ItemDataRole.FontRole))
                    values = [model.index(row, 4).data() for row in range(77, 80)]
                    self.assertEqual(values, ["119 MB", "120 MB", "121 MB"])
                    self.assertAlmostEqual(metrics.horizontalAdvance(values[0]), metrics.horizontalAdvance(values[1]))
                    self.assertAlmostEqual(metrics.horizontalAdvance(values[1]), metrics.horizontalAdvance(values[2]))
                if sample_id == "data-grid-column-selection":
                    self.assertEqual(result.widget.model().index(0, 0).data(), "Aurora")
                    self.assertEqual(result.widget.horizontalHeader().sortIndicatorOrder(), Qt.SortOrder.AscendingOrder)
                if sample_id == "data-grid-edit-validation":
                    grid = result.widget
                    grid.show()
                    index = grid.model().index(2, 2)
                    grid.setCurrentIndex(index)
                    grid.edit(index)
                    self.app.processEvents()
                    editor = grid.findChild(fluentqt.LineEdit)
                    self.assertIsNotNone(editor)
                    self.assertFalse(editor.isClearButtonEnabled())
                    self.assertFalse(editor.isFrameVisible())
                    self.assertEqual(editor.geometry(), grid.visualRect(index).adjusted(4, 2, -4, -2))
                    editor.setText("Project scope")
                    QTest.keyClick(editor, Qt.Key.Key_Return)
                    self.app.processEvents()
                    self.assertEqual(index.data(), "Project scope")
            finally:
                delete_qobject(result.widget)

        result = build_native_sample("tree-select", "tree-select-basic", None)
        try:
            self.assertIsInstance(result.widget, fluentqt.TreeSelect)
            self.assertEqual(result.widget.selectedKeys(), ["client"])
            result.widget.setSelectedKeys(["platform"])
            self.assertEqual(result.widget.selectedKeys(), ["platform"])
            self.assertLessEqual(len(result.source.splitlines()), 25)
        finally:
            delete_qobject(result.widget)

        result = build_native_sample("tag-input", "tag-input-basic", None)
        try:
            self.assertIsInstance(result.widget, fluentqt.TagInput)
            self.assertEqual(result.widget.selectedTexts(), ["Qt"])
            self.assertLessEqual(len(result.source.splitlines()), 10)
        finally:
            delete_qobject(result.widget)

        result = build_native_sample("list-detail-view", "list-detail-basic", None)
        try:
            panel = result.widget
            view = panel.findChild(fluentqt.ListDetailView)
            width = panel.findChild(fluentqt.ComboBox)
            editor = view.findChild(fluentqt.LineEdit)
            editor.setText("Retained draft")
            panel.resize(700, 220)
            panel.show()
            QApplication.processEvents()
            self.assertFalse(view.isCompact())
            width.setCurrentIndex(1)
            QApplication.processEvents()
            self.assertTrue(view.isCompact())
            self.assertTrue(view.listPane().isVisible())
            self.assertFalse(view.detailPane().isVisible())
            buttons = {button.text(): button for button in view.findChildren(fluentqt.Button)}
            buttons["Open meeting note"].click()
            self.assertEqual(view.currentPane(), fluentqt.ListDetailView.Detail)
            self.assertTrue(buttons["Back to notes"].isVisible())
            buttons["Back to notes"].click()
            self.assertEqual(view.currentPane(), fluentqt.ListDetailView.List)
            buttons["Open meeting note"].click()
            self.assertEqual(editor.text(), "Retained draft")
            width.setCurrentIndex(0)
            QApplication.processEvents()
            self.assertFalse(view.isCompact())
            self.assertTrue(view.listPane().isVisible())
            self.assertTrue(view.detailPane().isVisible())
            self.assertFalse(buttons["Back to notes"].isVisible())
            self.assertEqual(editor.text(), "Retained draft")
        finally:
            result.widget.close()
            delete_qobject(result.widget)

    def test_tree_select_project_teams_share_directory_and_paths(self):
        from fluentqt_gallery.native_samples import build_native_sample
        from fluentqt_gallery.native_samples_basic import team_selection_source_files
        result = build_native_sample("tree-select", "tree-select-project-teams", None)
        panel = result.widget
        owner = panel.findChild(fluentqt.TreeSelect, "projectOwningTeam")
        teams = panel.findChild(fluentqt.TreeSelect, "projectCollaboratingTeams")
        try:
            self.assertIs(owner.model(), teams.model())
            self.assertEqual(owner.selectedKeys(), ["client"])
            teams.setSelectedKeys(["product-design", "brand-design"])
            self.assertEqual(teams.selectedCount(), 2)
            self.assertNotEqual(teams.selectedItems()[0]["path"], teams.selectedItems()[1]["path"])
            panel.findChild(fluentqt.CheckBox, "teamReadOnly").setChecked(True)
            self.assertTrue(owner.isReadOnly())
            self.assertTrue(teams.isReadOnly())
            files = team_selection_source_files(result)
            self.assertEqual(len(files), 1)
            self.assertEqual(files[0]["usage_snippet"], result.source)
            self.assertLess(len(result.source.splitlines()), 25)
            namespace = {"__name__": "tree_select_source_test"}
            exec(compile(files[0]["code"], files[0]["name"], "exec"), namespace)
            copied = namespace["root"]
            try:
                copied_owner = copied.findChild(fluentqt.TreeSelect, "projectOwningTeam")
                self.assertEqual(copied_owner.selectedKeys(), ["client"])
            finally:
                with patch("sys.excepthook") as exception_hook:
                    namespace.clear()
                    delete_qobject(copied)
                    QApplication.processEvents()
                    exception_hook.assert_not_called()
        finally:
            panel.close()
            delete_qobject(panel)

    def test_tag_input_project_members_search_and_resolution(self):
        from fluentqt_gallery.native_samples import build_native_sample
        from PySide6.QtTest import QTest
        result = build_native_sample("tag-input", "tag-input-project-members", None)
        panel = result.widget
        panel.resize(440, 400)
        panel.show()
        panel.activateWindow()
        box = panel.findChild(fluentqt.TagInput, "projectMemberInput")
        try:
            box.setFocus()
            QApplication.processEvents()
            self.assertEqual(box.selectedKeys(), ["chen-design", "lin"])
            self.assertEqual(box.selectedTexts(), ["Chen Chen · Design team", "Lin Xiao"])
            box.setText("Chen Chen")
            box.requestSuggestions()
            self.assertTrue(_wait_until(lambda: box.loadState() == fluentqt.AutoSuggestBox.LoadState.Ready))
            self.assertEqual(box.suggestionCount(), 2)
            QTest.keyClick(box, Qt.Key_Down)
            self.assertEqual(box.selectedCount(), 2)
            QTest.keyClick(box, Qt.Key_Return)
            self.assertEqual(box.selectedKeys(), ["chen-design", "lin", "chen-dev"])
            self.assertEqual(box.selectedTexts(), ["Chen Chen · Design team", "Lin Xiao", "Chen Chen · Engineering team"])
            self.assertEqual(box.text(), "")
            box.tokensSubmitted.emit(["alex@example.com", "alex@example.com", "unknown@example.com"])
            self.assertEqual(box.selectedCount(), 4)
            self.assertEqual(box.text(), "unknown@example.com")
            box.removeItem("chen-dev")
            box.tokensSubmitted.emit(["chen.dev@example.com"])
            self.assertEqual(box.selectedTexts()[-1], "Chen Chen · Engineering team")
            box.cancelSuggestions()
            panel.findChild(fluentqt.CheckBox, "tagFailNext").setChecked(True)
            box.setText("wang")
            box.requestSuggestions()
            self.assertTrue(_wait_until(lambda: box.loadState() == fluentqt.AutoSuggestBox.LoadState.Failed))
            self.assertEqual(box.selectedCount(), 4)
            box.retrySuggestions()
            self.assertTrue(_wait_until(lambda: box.loadState() == fluentqt.AutoSuggestBox.LoadState.Ready))
            self.assertEqual(box.suggestionCount(), 1)
            box.setMaximumWidth(280)
            panel.resize(320, 400)
            QApplication.processEvents()
            self.assertEqual(box.selectedCount(), 4)
            self.assertLessEqual(box.width(), 280)
        finally:
            panel.close()
            delete_qobject(panel)

    def test_async_search_pages_retry_and_stale_results(self):
        from fluentqt_gallery.native_samples import build_native_sample
        from PySide6.QtTest import QTest
        result = build_native_sample("auto-suggest-box", "auto-suggest-box-async", None)
        panel = result.widget
        panel.resize(480, 420)
        panel.show()
        panel.activateWindow()
        QTest.qWaitForWindowActive(panel)
        box = panel.findChild(fluentqt.AutoSuggestBox, "asyncProjectSearch")
        try:
            box.setFocus()
            box.setText("A")
            box.requestSuggestions()
            box.setText("Canvas")
            box.requestSuggestions()
            _qwait(1100)
            self.assertEqual(panel.property("staleResponsesIgnored"), 1)
            self.assertEqual(box.suggestionCount(), 2)
            self.assertEqual(box.suggestionModel().index(0, 0).data(), "Canvas")
            box.setText("Aurora")
            box.requestSuggestions()
            _qwait(400)
            self.assertEqual(box.suggestionCount(), 4)
            self.assertTrue(box.hasMoreSuggestions())
            box.loadMoreSuggestions()
            self.assertEqual(box.suggestionCount(), 4)
            _qwait(400)
            self.assertEqual(box.suggestionCount(), 8)
            self.assertFalse(box.hasMoreSuggestions())
            box.cancelSuggestions()
            panel.findChild(fluentqt.CheckBox, "asyncFailNext").setChecked(True)
            box.setText("Canvas")
            box.requestSuggestions()
            _qwait(400)
            self.assertEqual(box.loadState(), fluentqt.AutoSuggestBox.LoadState.Failed)
            box.retrySuggestions()
            _qwait(400)
            self.assertEqual(box.loadState(), fluentqt.AutoSuggestBox.LoadState.Ready)
            self.assertEqual(box.suggestionCount(), 2)
            box.setText("zzz")
            box.requestSuggestions()
            _qwait(400)
            self.assertEqual(box.suggestionCount(), 0)
            box.setText("A")
            box.requestSuggestions()
            box.cancelSuggestions()
            _qwait(950)
            self.assertFalse(box.isSuggestionListOpen())
            self.assertEqual(box.suggestionCount(), 0)
        finally:
            panel.close()
            delete_qobject(panel)

    def test_list_detail_file_browser_preserves_live_state(self):
        from fluentqt_gallery.native_samples import build_native_sample
        from fluentqt_gallery.native_samples_layout import file_browser_source_files
        result = build_native_sample("list-detail-view", "list-detail-file-browser", None)
        panel = result.widget
        panel.resize(840, 440)
        panel.show()
        panel.activateWindow()
        QApplication.processEvents()
        view = panel.findChild(fluentqt.ListDetailView, "fileBrowserLayout")
        files = panel.findChild(fluentqt.ListView, "fileBrowserList")
        editor = panel.findChild(fluentqt.TextEdit, "fileBrowserEditor")
        narrow = panel.findChild(fluentqt.Button, "fileBrowserNarrow")
        back = panel.findChild(fluentqt.Button, "fileBrowserBack")
        files.setCurrentIndex(files.model().index(20, 0))
        files.verticalScrollBar().setValue(300)
        offset = files.verticalScrollBar().value()
        editor.setPlainText("A draft across layouts")
        narrow.click()
        QApplication.processEvents()
        self.assertTrue(view.isCompact())
        self.assertTrue(back.isVisible())
        back.click()
        QApplication.processEvents()
        self.assertEqual(files.currentIndex().row(), 20)
        self.assertEqual(files.verticalScrollBar().value(), offset)
        from PySide6.QtTest import QTest
        files.setFocus()
        QTest.keyClick(files, Qt.Key_Return)
        QApplication.processEvents()
        self.assertEqual(view.currentPane(), fluentqt.ListDetailView.Pane.Detail)
        narrow.click()
        QApplication.processEvents()
        self.assertFalse(view.isCompact())
        self.assertEqual(editor.toPlainText(), "A draft across layouts")
        self.assertEqual(files.model().index(20, 0).data(Qt.UserRole), "A draft across layouts")
        source = file_browser_source_files(result)[0]
        self.assertIn("class FileBrowserSample", source["code"])
        self.assertIn("setCompactThreshold(640)", source["usage_snippet"])
        panel.close()
        delete_qobject(panel)

    def test_file_browser_search_create_and_save_keep_the_draft(self):
        from fluentqt_gallery.native_samples import build_native_sample
        from PySide6.QtGui import QTextCursor
        result = build_native_sample("list-detail-view", "list-detail-file-browser", None)
        panel = result.widget
        panel.resize(960, 644)
        panel.show()
        QApplication.processEvents()
        files = panel.findChild(fluentqt.ListView, "fileBrowserList")
        editor = panel.findChild(fluentqt.TextEdit, "fileBrowserEditor")
        search = panel.findChild(fluentqt.LineEdit, "fileBrowserSearch")
        save = panel.findChild(fluentqt.Button, "fileBrowserSave")
        create = panel.findChild(fluentqt.Button, "fileBrowserNew")
        self.assertEqual(files.currentIndex().data(), "layout-notes.md")
        self.assertFalse(editor.isAutoResizeEnabled())
        self.assertFalse(editor.isFrameVisible())
        editor.setPlainText("A searchable draft")
        cursor = editor.textCursor()
        cursor.setPosition(2)
        cursor.setPosition(12, QTextCursor.KeepAnchor)
        editor.setTextCursor(cursor)
        search.setText("LAYOUT")
        self.assertEqual(files.model().rowCount(), 1)
        search.setText("missing file")
        self.assertEqual(files.model().rowCount(), 0)
        self.assertEqual(editor.toPlainText(), "A searchable draft")
        search.clear()
        self.assertEqual(files.currentIndex().data(), "layout-notes.md")
        self.assertEqual(editor.textCursor().selectedText(), "searchable")
        save.click()
        self.assertFalse(save.isEnabled())
        self.assertEqual(files.currentIndex().data(Qt.UserRole + 1), "Just now")
        count = files.model().rowCount()
        create.click()
        self.assertEqual(files.model().rowCount(), count + 1)
        self.assertEqual(files.currentIndex().data(), "untitled-1.md")
        self.assertTrue(save.isEnabled())
        files.setCurrentIndex(files.model().index(2, 0))
        self.assertEqual(editor.toPlainText(), "A searchable draft")
        files.setCurrentIndex(files.model().index(0, 0))
        self.assertTrue(editor.isReadOnly())
        self.assertFalse(save.isEnabled())
        panel.close()
        delete_qobject(panel)

    def test_runtime_package_matches_gallery_sources(self):
        stale_files = [
            source.name
            for source in sorted(SOURCE_PACKAGE_DIR.glob("*.py"))
            if not (RUNTIME_PACKAGE_DIR / source.name).is_file()
            or (RUNTIME_PACKAGE_DIR / source.name).read_bytes()
            != source.read_bytes()
        ]
        self.assertEqual(
            stale_files,
            [],
            "PySide6 Gallery build package is stale; rebuild the configured "
            "PySide6 tree before running Gallery tests",
        )

    @classmethod
    def setUpClass(cls):
        global _TEST_APPLICATION
        _TEST_APPLICATION = QApplication.instance() or QApplication([])
        _prepare_qml_bridge_application()
        cls.app = _TEST_APPLICATION
        cls.app.setProperty("fluentqtGalleryAutomated", True)
        if not fluentqt.initialize_resources():
            raise RuntimeError("FluentQt resources could not be initialized")
        cls.app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
        initialize_headless_test_fixed_font(cls.app)

    def setUp(self):
        # These are native 2D shell checks; the Spatial suite owns GPU presentation.
        gallery_settings_module.gallery_settings().set_spatial_mode_enabled(False)

    def tearDown(self):
        QApplication.processEvents()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QApplication.processEvents()

    def test_contract_exactly_matches_the_public_binding(self):
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        self.assertEqual(catalog_coverage_errors(manifest["classes"]), [])
        self.assertEqual(runtime_catalog_errors(), [])
        components = [c for c in CONTRACT["components"]
                      if (SPATIAL_AVAILABLE or c["category_id"] != "spatial")
                      and (QML_BRIDGE_AVAILABLE or c["category_id"] != "qml-bridge")]
        categories = [c for c in CONTRACT["categories"]
                      if (SPATIAL_AVAILABLE or c["id"] != "spatial")
                      and (QML_BRIDGE_AVAILABLE or c["id"] != "qml-bridge")]
        hidden = {c["id"] for c in CONTRACT["categories"] if c not in categories}
        if not any(c["plugin"] for c in categories):
            hidden.add("plugins")
        for route in CONTRACT["routes"]:
            if route["parent_id"] in hidden:
                hidden.add(route["id"])
        routes = [r for r in CONTRACT["routes"] if r["id"] not in hidden]
        self.assertEqual([(entry.route_id, entry.name) for entry in ENTRIES],
                         [(c["id"], c["api_type"]) for c in components])
        self.assertEqual([route.id for route in ROUTES], [r["id"] for r in routes])
        self.assertEqual([category.id for category in CATEGORIES], [c["id"] for c in categories])
        self.assertEqual(len({route.id for route in ROUTES}), len(ROUTES))
        self.assertEqual(len({entry.route_id for entry in ENTRIES}), len(ENTRIES))
        self.assertEqual(
            [(entry.route_id, sample.id) for entry in ENTRIES for sample in entry.samples],
            [(c["id"], sample["id"]) for c in components for sample in c["samples"]])

    def test_support_types_are_explicit_and_embedded_in_real_samples(self):
        self.assertEqual(SUPPORT_TYPES, EXPECTED_SUPPORT_TYPES)
        routed_types = {entry.name for entry in ENTRIES}
        self.assertTrue(routed_types.isdisjoint(SUPPORT_TYPES))
        for entry in ENTRIES:
            self.assertFalse(entry.support_type)

    def test_route_normalization_uses_native_route_ids(self):
        self.assertEqual(normalize_route("Button"), "button")
        self.assertEqual(normalize_route("button"), "button")
        self.assertEqual(normalize_route("basic-input"), "basic-input")
        self.assertEqual(normalize_route("home"), "home")
        self.assertEqual(normalize_route("component/Button"), "button")
        self.assertEqual(normalize_route("category/basic-input"), "basic-input")

    def test_high_contrast_cli_theme_uses_the_effective_canvas_token(self):
        self.assertEqual(
            parse_args(["--theme", "high-contrast"]).theme,
            "high-contrast",
        )
        original_theme = fluentqt.current_theme()
        self.addCleanup(fluentqt.set_theme, original_theme)
        window = GalleryWindow(startup_visuals=False)
        original_mode = int(window._settings.theme_mode)
        try:
            window._set_theme_mode(3)
            self.assertEqual(
                _snapshot_canvas_color(window),
                QColor(_theme_tokens(window)["bgCanvas"]),
            )
            self.assertEqual(_snapshot_canvas_color(window).name(), "#000000")
        finally:
            window._set_theme_mode(original_mode)
            window.close()
            window.deleteLater()

    def test_navigation_icons_use_native_optical_alias_resolution(self):
        native_catalog_path = (
            Path(__file__).resolve().parents[4]
            / "res"
            / "icons"
            / "FluentQtIcons.json"
        )
        native_catalog = json.loads(
            native_catalog_path.read_text(encoding="utf-8")
        )
        expected = {
            0xE80F: "ic_fluent_home_16_regular",
            0xE80A: "ic_fluent_grid_16_regular",
            0xE972: "ic_fluent_chevron_down_16_regular",
            0xE713: "ic_fluent_settings_16_regular",
        }
        for semantic_codepoint, native_name in expected.items():
            with self.subTest(codepoint=hex(semantic_codepoint)):
                self.assertEqual(
                    ord(_direct_icon_glyph(chr(semantic_codepoint), 16)),
                    native_catalog[native_name],
                )

    def test_qt62_font_flags_and_context_timer_compatibility(self):
        font = _direct_icon_font(16)
        self.assertIsInstance(font.styleStrategy(), QFont.StyleStrategy)
        self.assertTrue(
            font.styleStrategy()
            & QFont.StyleStrategy.NoSubpixelAntialias
        )

        context = QWidget()
        fired = []
        timer = _single_shot(0, context, lambda: fired.append(True))
        self.assertIs(timer.parent(), context)
        _qwait(1)
        QApplication.processEvents()
        self.assertEqual(fired, [True])
        context.deleteLater()
        QApplication.processEvents()

    def test_window_placement_matches_native_logical_screen_contract(self):
        self.assertEqual(
            effective_minimum_size(QSize(1920, 1080)), QSize(460, 500)
        )
        self.assertEqual(
            effective_minimum_size(QSize(640, 360)), QSize(460, 360)
        )
        self.assertEqual(
            recommended_initial_size(QSize(3840, 2160)), QSize(1440, 900)
        )
        self.assertEqual(
            recommended_initial_size(QSize(1920, 1080)), QSize(1382, 842)
        )
        self.assertEqual(
            recommended_initial_size(QSize(1280, 720)), QSize(922, 600)
        )
        self.assertEqual(
            recommended_initial_size(QSize(640, 360)), QSize(640, 360)
        )
        available = QRect(0, 0, 1280, 720)
        self.assertEqual(
            constrain_geometry(
                QRect(-200, -100, 1600, 900),
                available,
                QSize(460, 500),
            ),
            available,
        )
        self.assertEqual(
            constrain_geometry(
                QRect(1800, 1000, 900, 600),
                available,
                QSize(460, 500),
            ),
            QRect(380, 120, 900, 600),
        )
        saved = QRect(100, 80, 900, 600)
        self.assertEqual(
            restored_geometry(saved, available, QSize(460, 500)), saved
        )
        self.assertEqual(
            restored_geometry(
                QRect(0, 0, 640, 480),
                QRect(0, 0, 1920, 1080),
                QSize(460, 500),
            ),
            QRect(349, 119, 1382, 842),
        )

    def test_first_close_prompt_matches_native_content_dialog(self):
        content = CloseBehaviorPromptContent(CloseBehavior.Tray)
        try:
            self.assertEqual(content.size(), QSize(300, 134))
            self.assertEqual(len(content.rows), 3)
            self.assertEqual(
                [row.accessibleName() for row in content.rows],
                ["Minimize window", keep_running_choice(), "Quit the app"],
            )
            self.assertEqual(
                [row.behavior for row in content.rows],
                [
                    CloseBehavior.Minimize,
                    CloseBehavior.Tray,
                    CloseBehavior.Quit,
                ],
            )
            self.assertTrue(content.rows[1]._selected)
        finally:
            content.deleteLater()

        window = GalleryWindow(startup_visuals=False)
        controller = GalleryApplicationController(
            window,
            window._settings,
            window,
            setup_status_item=False,
        )
        window.show()
        QApplication.processEvents()
        try:
            self.assertTrue(window.isChromeInteractive())
            controller._show_close_behavior_dialog()
            dialog = controller._close_dialog
            self.assertIsNotNone(dialog)
            self.assertEqual(dialog.objectName(), "galleryCloseBehaviorDialog")
            self.assertEqual(dialog.size(), QSize(380, 288))
            self.assertEqual(dialog.title(), "Close behavior")
            self.assertEqual(dialog.primaryButtonText(), "Save")
            self.assertEqual(dialog.closeButtonText(), "Cancel")
            self.assertEqual(
                dialog.defaultButton(),
                int(fluentqt.ContentDialogButton.Primary),
            )
            self.assertFalse(window.isChromeInteractive())
            dialog.done(fluentqt.ContentDialog.ResultNone)
            self.assertTrue(
                _wait_until(window.isChromeInteractive, timeout_ms=2000)
            )
        finally:
            controller.arm_application_quit()
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_session_shutdown_bypasses_interactive_close_behavior(self):
        window = GalleryWindow(startup_visuals=False)
        controller = GalleryApplicationController(
            window,
            window._settings,
            window,
            setup_status_item=False,
        )
        event = QCloseEvent()
        try:
            with patch.object(
                application_controller_module,
                "_application_is_saving_session",
                return_value=True,
            ):
                self.assertFalse(controller.eventFilter(window, event))
            self.assertTrue(event.isAccepted())
            self.assertFalse(controller._close_request_scheduled)
        finally:
            controller.arm_application_quit()
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_wayland_inactive_window_forces_remap_and_attention(self):
        class RestoreWindow(QWidget):
            def __init__(self):
                super().__init__()
                self.native_restore_prepared = False

            def prepareForNativeRestore(self):
                self.native_restore_prepared = True

            def reapplySystemBackdrop(self):
                pass

            def requestForegroundActivation(self):
                pass

        window = RestoreWindow()
        controller = GalleryApplicationController(
            window,
            object(),
            window,
            setup_status_item=False,
        )
        scheduled: list[tuple[int, Callable[[], None]]] = []
        window.show()
        QApplication.processEvents()
        try:
            with (
                patch.object(
                    application_controller_module,
                    "_is_wayland_platform",
                    return_value=True,
                ),
                patch.object(
                    application_controller_module,
                    "_window_is_active",
                    return_value=False,
                ),
                patch.object(
                    application_controller_module.sys,
                    "platform",
                    "linux",
                ),
                patch.object(
                    application_controller_module,
                    "_single_shot",
                    side_effect=lambda delay, _context, callback: (
                        scheduled.append((delay, callback))
                    ),
                ),
                patch.object(
                    application_controller_module,
                    "_request_application_attention",
                ) as request_attention,
            ):
                controller.restore_window()
                self.assertTrue(window.native_restore_prepared)
                self.assertFalse(window.isVisible())
                self.assertEqual(
                    [delay for delay, _callback in scheduled], [100, 250]
                )
                next(
                    callback
                    for delay, callback in scheduled
                    if delay == 250
                )()
                request_attention.assert_called_once_with(window)
        finally:
            controller.arm_application_quit()
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_windows_secondary_launch_grants_foreground_permission(self):
        class LockFile:
            @staticmethod
            def getLockInfo():
                return 4242, "host", "Fluent-Qt Gallery"

        with (
            patch.object(single_instance_module.sys, "platform", "win32"),
            patch.object(
                single_instance_module, "_allow_set_foreground_window"
            ) as allow_foreground,
        ):
            single_instance_module._allow_owner_foreground_activation(
                LockFile()
            )
        allow_foreground.assert_called_once_with(4242)

    def test_windows_system_theme_falls_back_to_registry(self):
        with (
            patch.object(
                gallery_settings_module,
                "system_color_scheme",
                return_value=None,
            ),
            patch.object(
                gallery_settings_module,
                "_windows_apps_use_light_theme",
                return_value=False,
            ),
        ):
            self.assertEqual(
                gallery_settings_module._system_theme(),
                fluentqt.Theme.Dark,
            )

    def test_gallery_app_is_single_instance_and_reactivates_primary(self):
        instance_id = "com.fluentqt.gallery.test.{0}".format(uuid.uuid4().hex)
        runtime = TemporaryDirectory()
        self.addCleanup(runtime.cleanup)
        primary = GallerySingleInstance(
            instance_id, runtime_directory=runtime.name
        )
        activated: list[bool] = []
        primary.activationRequested.connect(lambda: activated.append(True))
        self.assertEqual(primary.start(), StartResult.Primary)

        probe = """
import sys
from PySide6.QtCore import QCoreApplication
from fluentqt_gallery.single_instance import GallerySingleInstance
QCoreApplication.setApplicationName(sys.argv[3])
QCoreApplication.setOrganizationName(sys.argv[4])
app = QCoreApplication([])
instance = GallerySingleInstance(sys.argv[1], runtime_directory=sys.argv[2])
result = instance.start()
print(int(result), flush=True)
print(instance.error_string, flush=True)
instance.close()
"""
        callback_errors: list[tuple[object, ...]] = []
        previous_excepthook = sys.excepthook
        sys.excepthook = lambda *error: callback_errors.append(error)
        try:
            for expected_activation_count in range(1, 4):
                worker = subprocess.Popen(
                    (
                        sys.executable,
                        "-c",
                        probe,
                        instance_id,
                        runtime.name,
                        QCoreApplication.applicationName(),
                        QCoreApplication.organizationName(),
                    ),
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                )
                try:
                    for _attempt in range(250):
                        if worker.poll() is not None:
                            break
                        _qwait(10)
                    stdout, stderr = worker.communicate(timeout=1.0)
                    _qwait(200)
                    output = stdout.splitlines()
                    self.assertEqual(worker.returncode, 0, stderr)
                    self.assertEqual(
                        int(output[0]),
                        int(StartResult.ExistingInstanceNotified),
                    )
                    self.assertEqual(output[1:] or [""], [""])
                    self.assertEqual(
                        activated,
                        [True] * expected_activation_count,
                    )
                    self.assertEqual(primary._accepted_sockets, {})
                finally:
                    if worker.poll() is None:
                        worker.kill()
                        worker.wait(timeout=1.0)
            self.assertEqual(callback_errors, [])
        finally:
            sys.excepthook = previous_excepthook
            primary.close()

    def test_python_and_native_galleries_have_independent_runtime_identity(self):
        self.assertEqual(APPLICATION_ID, "com.fluentqt.gallery.pyside6")
        self.assertEqual(APPLICATION_NAME, "Fluent-Qt Gallery (Python)")
        self.assertEqual(ORGANIZATION_NAME, "Fluent-Qt")
        self.assertNotEqual(
            _scoped_instance_name(APPLICATION_ID),
            _scoped_instance_name("com.fluentqt.gallery"),
        )

        previous_name = QCoreApplication.applicationName()
        previous_organization = QCoreApplication.organizationName()
        try:
            QCoreApplication.setApplicationName(APPLICATION_NAME)
            QCoreApplication.setOrganizationName(ORGANIZATION_NAME)
            self.assertTrue(persistence_available())

            QCoreApplication.setApplicationName("Fluent-Qt Gallery")
            self.assertFalse(persistence_available())
        finally:
            QCoreApplication.setApplicationName(previous_name)
            QCoreApplication.setOrganizationName(previous_organization)

    def test_gallery_app_defers_heavy_ui_imports_until_primary_lock(self):
        probe = """
import json
import sys
import fluentqt_gallery.app
heavy_modules = (
    "fluentqt_gallery.catalog",
    "fluentqt_gallery.native_samples",
    "fluentqt_gallery.window",
)
print(json.dumps([name for name in heavy_modules if name in sys.modules]))
"""
        completed = subprocess.run(
            (sys.executable, "-c", probe),
            check=False,
            capture_output=True,
            text=True,
            timeout=10.0,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(json.loads(completed.stdout), [])

    def test_every_native_sample_has_an_exact_python_port(self):
        expected = _contract_sample_keys()
        self.assertTrue(expected)
        self.assertEqual(ported_sample_keys(), expected)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding is not installed")
    def test_spatial_calendar_preview_keeps_the_selected_month_independent_of_today(self):
        result = build_sample("spatial-view", "spatial-view-calendar")
        try:
            calendar = result.widget.findChild(fluentqt.CalendarView, "spatialCalendar")
            selected = result.widget.findChild(fluentqt.Label, "spatialSelectedDate")
            self.assertIsNotNone(calendar)
            self.assertEqual(calendar.visibleMonth().toString(Qt.ISODate), "2026-09-01")
            self.assertEqual(calendar.selectedDate().toString(Qt.ISODate), "2026-09-15")
            self.assertTrue(calendar.dateCellRect(calendar.selectedDate()).isValid())
            self.assertEqual(selected.text(), "Selected: 2026-09-15")
        finally:
            result.widget.close()
            result.widget.deleteLater()

    def test_splash_preview_handles_host_teardown_after_namespace_cleanup(self):
        host = QWidget()
        result = build_sample("splash-screen", "splash-screen-startup", host)
        root = result.widget
        namespace = _detach_sample_namespace(root)
        retained = dict(namespace)
        try:
            # Sample cleanup can release globals before Qt destroys its host layout.
            namespace.clear()
            self.assertFalse(root.eventFilter(host, QEvent(QEvent.Type.ChildRemoved)))
            host.deleteLater()
            QCoreApplication.sendPostedEvents(host, QEvent.Type.DeferredDelete)
            self.assertFalse(shiboken6.isValid(root))
        finally:
            namespace.update(retained)
            if shiboken6.isValid(host):
                host.deleteLater()
                QCoreApplication.sendPostedEvents(host, QEvent.Type.DeferredDelete)

    def test_splash_preview_defaults_to_branded_and_selects_simple(self):
        host = QWidget()
        result = build_sample("splash-screen", "splash-screen-startup", host)
        root = result.widget
        try:
            splash = root.findChild(fluentqt.SplashScreen, "sampleSplash")
            presentation = root.findChild(fluentqt.ComboBox, "splashPresentation")
            target = root.findChild(QWidget, "splashDestinationIcon")
            self.assertEqual(splash.presentation(), fluentqt.SplashScreen.Presentation.Branded)
            self.assertIs(splash.transitionTarget(), target)
            self.assertEqual(splash.title(), "FluentQt")
            presentation.setCurrentIndex(1)
            self.assertEqual(splash.presentation(), fluentqt.SplashScreen.Presentation.Simple)
            presentation.setCurrentIndex(0)
            self.assertEqual(splash.presentation(), fluentqt.SplashScreen.Presentation.Branded)
        finally:
            host.deleteLater()
            QCoreApplication.sendPostedEvents(host, QEvent.Type.DeferredDelete)

    def test_startup_replacement_releases_python_owner(self):
        self.addCleanup(fluentqt.set_motion_mode, fluentqt.current_motion_mode())
        window = GalleryWindow(startup_visuals=True)
        window.show()
        fluentqt.set_motion_mode(fluentqt.MotionMode.Disabled)
        original = window._splash
        self.assertIsNotNone(original)
        replacement = fluentqt.SplashScreen(window.contentHost())
        try:
            replacement.show()
            QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
            self.assertFalse(shiboken6.isValid(original))
            self.assertIsNone(window._splash)
            self.assertTrue(window._startup_finished)
            self.assertTrue(replacement.isVisible())
            self.assertIsNone(window._intro_tour)
        finally:
            window.close()
            window.deleteLater()
            QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)

    def test_splash_connected_transition_survives_window_close(self):
        self.addCleanup(fluentqt.set_motion_mode, fluentqt.current_motion_mode())
        fluentqt.set_motion_mode(fluentqt.MotionMode.Full)
        for _ in range(8):
            window = fluentqt.Window()
            window.resize(640, 480)
            target = fluentqt.Label(window)
            target.setGeometry(20, 12, 24, 24)
            splash = fluentqt.SplashScreen(window.contentHost())
            splash.setIcon(app_icon())
            splash.setTransitionTarget(target)
            try:
                window.show()
                splash.show()
                splash.dismiss()
                moving = window.findChild(QWidget, "splashLogoTransition")
                self.assertIsNotNone(moving)
                later_sibling = QWidget(window)
                later_sibling.show()
                window.close()
                self.assertFalse(window.isVisible())
                if shiboken6.isValid(moving):
                    self.assertFalse(moving.isVisible())
                QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
                self.assertFalse(shiboken6.isValid(moving))
            finally:
                window.deleteLater()
                QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)

    def test_text_edit_log_sample_appends_plain_text_and_clears(self):
        result = build_sample("text-edit", "text-edit-log-append")
        try:
            log = result.widget.findChild(fluentqt.TextEdit, "appendLog")
            append = result.widget.findChild(fluentqt.Button, "appendLogEntry")
            clear = result.widget.findChild(fluentqt.Button, "clearLog")
            self.assertIsNotNone(log)
            self.assertIsNotNone(append)
            self.assertIsNotNone(clear)
            self.assertTrue(log.isReadOnly())
            self.assertEqual(log.accessibleName(), "Activity log")
            self.assertIn("log.appendPlainText", result.source)
            initial = log.toPlainText()
            append.click()
            append.click()
            self.assertEqual(
                log.toPlainText(),
                initial + "\nINFO Received: <ready>\nINFO Received: <ready>",
            )
            clear.click()
            self.assertEqual(log.toPlainText(), "")
            append.click()
            self.assertEqual(log.toPlainText(), "INFO Received: <ready>")
        finally:
            result.widget.deleteLater()
            QApplication.processEvents()

    def test_toggle_switch_state_sample_has_an_accessible_name(self):
        result = build_sample("toggle-switch", "toggle-switch-state")
        try:
            toggle = result.widget.findChild(fluentqt.ToggleSwitch)
            self.assertIsNotNone(toggle)
            self.assertEqual(toggle.accessibleName(), "Feature toggle")
            self.assertIn(
                'toggle.setAccessibleName("Feature toggle")', result.source
            )
        finally:
            result.widget.deleteLater()
            QApplication.processEvents()

    def test_connection_form_shared_validation_and_save(self):
        result = build_sample("field", "field-connection-form")
        root = result.widget
        try:
            root.resize(560, root.sizeHint().height())
            root.show()
            QApplication.processEvents()
            form = root.findChild(fluentqt.FormController, "connectionFormController")
            self.assertIn("FormController", result.covered_types)
            self.assertIsNotNone(form)
            self.assertEqual(form.fieldKeys(), ["name", "address", "token"])
            token = root.findChild(fluentqt.LineEdit, "connectionInput_token")
            address = root.findChild(fluentqt.LineEdit, "connectionInput_address")
            self.assertIn("completeValidation", result.preview_source)
            token.setText("demo-token")
            address.setText("https://offline.example")
            form.validate()
            self.assertTrue(form.isValidating())
            address.setText("https://api.example.com/v1")
            _qwait(750)
            self.assertEqual(form.errorMessages(), [])
            self.assertTrue(form.submit())
            self.assertFalse(form.submit())
            elapsed = QElapsedTimer()
            elapsed.start()
            while form.isSubmitting() and elapsed.elapsed() < 3000:
                _qwait(20)
            self.assertFalse(form.isSubmitting())
            self.assertFalse(form.isDirty())
            self.assertTrue(form.isValid())
            root.resize(340, root.sizeHint().height())
            QApplication.processEvents()
            for field in root.findChildren(fluentqt.Field):
                self.assertLessEqual(field.geometry().right(), root.width())
        finally:
            root.deleteLater()
            QApplication.processEvents()

    def test_connection_form_exposes_concise_usage_and_complete_source(self):
        from fluentqt_gallery.native_samples_layout import connection_form_source_files

        entry = ENTRY_BY_ROUTE_ID["field"]
        sample = SAMPLE_BY_KEY[("field", "field-connection-form")]
        card, result = _build_sample_card(entry, sample, None)
        self.addCleanup(card.deleteLater)
        self.assertLessEqual(len(result.source.splitlines()), 80)
        self.assertLessEqual(max(map(len, result.source.splitlines())), 88)
        self.assertIn("Full example", result.source)
        self.assertNotIn("class ConnectionForm", result.source)
        full = connection_form_source_files(result)[0]["code"]
        self.assertIn(result.preview_source.split("class ConnectionForm", 1)[1], full)
        self.assertNotIn("from fluentqt_gallery", full)

        block = card._gallery_source_expander
        self.assertEqual(block.code(), result.source)
        block._source_selector.setSelectedIndex(1)
        self.assertEqual(block.code(), full)
        block._source_selector.setSelectedIndex(0)
        self.assertEqual(block.code(), result.source)

        namespace = {"__name__": "connection_form_source_check"}
        exec(compile(full, "connection_form.py", "exec"), namespace)
        copied = namespace["panel"]
        self.addCleanup(copied.deleteLater)
        form = copied.findChild(fluentqt.FormController, "connectionFormController")
        self.assertEqual(form.fieldKeys(), ["name", "address", "token"])
        copied.findChild(fluentqt.LineEdit, "connectionInput_token").setText("demo-token")
        self.assertTrue(form.submit())
        elapsed = QElapsedTimer()
        elapsed.start()
        while form.isSubmitting() and elapsed.elapsed() < 3000:
            _qwait(20)
        self.assertFalse(form.isSubmitting())
        self.assertTrue(form.isValid())
        self.assertFalse(form.isDirty())

    def test_command_palette_exposes_complete_workspace_source(self):
        result = build_sample("command-palette", "command-palette-shared-actions")
        try:
            self.assertLessEqual(len(result.source.splitlines()), 80)
            self.assertIn("Full example", result.source)
            for token in ("setBackdropBlurRadius", "setDimOpacity", "update_count()",
                          "fluentqt.Typography.Icons.Info", "navigate_to"):
                self.assertIn(token, result.preview_source)
            from fluentqt_gallery.native_samples_navigation import command_palette_source_files
            full = command_palette_source_files(result)[0]["code"]
            self.assertNotIn("from fluentqt_gallery", full)
            namespace = {"__name__": "workspace_source_check"}
            exec(compile(full, "workspace.py", "exec"), namespace)
            copied = namespace["root"]
            self.assertEqual(len(copied.findChild(fluentqt.CommandPalette).commands()), 8)
            copied.close()
            copied.deleteLater()
        finally:
            result.widget.deleteLater()

    def test_overlay_samples_share_backdrop_controls_and_modal_input(self):
        from PySide6.QtWidgets import QAbstractButton

        cases = (
            ("popup", "popup-modal-dim", fluentqt.Popup, "Gallery.Popup"),
            ("dialog", "dialog-backdrop-effects", fluentqt.Dialog, "Gallery.Dialog"),
            ("drawer-view", "drawer-view-backdrop-effects", fluentqt.DrawerView,
             "Gallery.Drawer"),
        )
        for route, sample, widget_type, prefix in cases:
            with self.subTest(route=route):
                host = QWidget()
                host.resize(900, 700)
                result = build_sample(route, sample, host)
                root = result.widget
                try:
                    root.setGeometry(20, 20, 700, 180)
                    host.show()
                    host.activateWindow()
                    QApplication.processEvents()
                    root.findChild(QAbstractButton, prefix + ".OpenEffects").click()
                    popup = host.findChild(widget_type, prefix + ".Effects")
                    self.assertTrue(popup.isModal())
                    popup.setAnimationEnabled(False)
                    popup.findChild(fluentqt.Slider, prefix + ".Blur").setValue(12)
                    popup.findChild(fluentqt.Slider, prefix + ".Dim").setValue(25)
                    self.assertEqual(popup.backdropBlurRadius(), 12.0)
                    self.assertAlmostEqual(popup.dimOpacity(), 0.25)
                    dim = popup.findChild(fluentqt.CheckBox, prefix + ".DimEnabled")
                    dim.setChecked(False)
                    self.assertFalse(popup.isDim())
                    self.assertTrue(popup.isModal())
                    self.assertEqual(popup.backdropBlurRadius(), 12.0)
                    counter = root.findChild(QAbstractButton, prefix + ".Background")
                    status = root.findChild(fluentqt.Label, prefix + ".Status")
                    point = counter.mapTo(host, counter.rect().center())
                    QTest.mouseClick(host.windowHandle(), Qt.MouseButton.LeftButton,
                                     Qt.KeyboardModifier.NoModifier, point)
                    self.assertEqual(status.text(), "Background clicks: 0")
                    self.assertTrue(popup.isOpen())
                    modal = popup.findChild(fluentqt.CheckBox, prefix + ".Modal")
                    modal.setChecked(False)
                    QTest.mouseClick(host.windowHandle(), Qt.MouseButton.LeftButton,
                                     Qt.KeyboardModifier.NoModifier, point)
                    self.assertEqual(status.text(), "Background clicks: 1")
                    self.assertTrue(popup.isOpen())
                    modal.setChecked(True)
                    popup.findChild(fluentqt.CheckBox, prefix + ".CloseOutside").setChecked(True)
                    closed = []
                    popup.closed.connect(lambda: closed.append(True))
                    QTest.mouseClick(host.windowHandle(), Qt.MouseButton.LeftButton,
                                     Qt.KeyboardModifier.NoModifier, point)
                    # The example deletes itself on closed; the native mouse
                    # delivery may process that deferred deletion before returning.
                    self.assertEqual(closed, [True])
                    self.assertEqual(status.text(), "Background clicks: 1")
                finally:
                    host.close()
                    host.deleteLater()

    def test_command_palette_samples_share_actions_and_editing_context(self):
        from PySide6.QtWidgets import QAbstractButton

        for sample_id in ("command-palette-shared-actions", "command-palette-editing-context"):
            with self.subTest(sample=sample_id):
                host = QWidget()
                host.resize(900, 700)
                result = build_sample("command-palette", sample_id, host)
                self.assertIn("fluentqt.CommandPalette(window)", result.source)
                self.assertNotIn("gallery_window_editing_command_router", result.source)
                root = result.widget
                try:
                    root.setGeometry(20, 20, 700, 400)
                    host.show()
                    self.assertTrue(QTest.qWaitForWindowExposed(host))
                    host.activateWindow()
                    self.assertTrue(QTest.qWaitForWindowActive(host))
                    QApplication.processEvents()
                    palette = root.findChild(fluentqt.CommandPalette)
                    self.assertIsNotNone(palette)
                    palette.setAnimationEnabled(False)
                    if sample_id.endswith("shared-actions"):
                        modal = root.findChild(fluentqt.CheckBox)
                        modal.setChecked(False)
                        self.assertFalse(palette.isModal())
                        modal.setChecked(True)
                        self.assertTrue(palette.isModal())
                        self.assertEqual(len(palette.commands()), 8)
                        self.assertEqual(palette.backdropBlurRadius(), 8.0)
                        self.assertAlmostEqual(palette.dimOpacity(), 0.12)
                        self.assertIsNone(root.findChild(fluentqt.Slider))
                        bar = root.findChild(fluentqt.CommandBar)
                        menu = root.findChild(fluentqt.FluentMenu)
                        details = bar.primaryActions()[-1]
                        self.assertIn(details, menu.actions())
                        self.assertIn(details, palette.commands())
                        root.findChild(QAbstractButton, "Gallery.CommandPalette.Open").click()
                        palette.setQuery("properties")
                        self.assertIs(palette.currentAction(), details)
                        self.assertTrue(palette.triggerCurrent())
                        self.assertTrue(details.isChecked())
                        status = root.findChild(fluentqt.Label, "Gallery.CommandPalette.Status")
                        self.assertFalse(status.isHidden())
                        notes = root.findChild(fluentqt.ListView, "Gallery.CommandPalette.Notes")
                        self.assertEqual(notes.model().rowCount(), 0)
                        palette.open()
                        palette.setQuery("new")
                        self.assertTrue(palette.triggerCurrent())
                        self.assertEqual(notes.model().rowCount(), 1)
                        self.assertEqual(status.text(), "1 note")
                        palette.open()
                        palette.setQuery("clear")
                        self.assertTrue(palette.triggerCurrent())
                        self.assertEqual(notes.model().rowCount(), 0)
                        self.assertIsNone(palette.currentAction())
                        palette.open()
                        palette.setQuery("inbox")
                        self.assertTrue(palette.triggerCurrent())
                        self.assertEqual(root.findChild(fluentqt.Label, "Gallery.CommandPalette.Location").text(), "Inbox")
                        self.assertEqual(notes.model().rowCount(), 1)
                        palette.open()
                        palette.setQuery("home")
                        self.assertTrue(palette.triggerCurrent())
                        self.assertEqual(notes.model().rowCount(), 0)
                    else:
                        root.findChild(QAbstractButton, "Gallery.CommandPalette.OpenEditing").click()
                        editor = root.findChild(fluentqt.LineEdit, "Gallery.CommandPalette.Editor")
                        search = palette.findChild(fluentqt.LineEdit, "FluentCommandPalette.Search")
                        QTest.keyClicks(search, "copy")
                        QTest.keyClick(search, Qt.Key.Key_Return)
                        self.assertFalse(palette.isOpen())
                        self.assertEqual(QApplication.clipboard().text(), "Review")
                        self.assertEqual(editor.selectedText(), "Review")
                        self.assertTrue(editor.hasFocus())
                        editor.clear()
                        open_button = root.findChild(QAbstractButton, "Gallery.CommandPalette.OpenEditing")
                        self.assertFalse(open_button.isEnabled())
                        root.findChild(QAbstractButton, "Gallery.CommandPalette.ResetEditing").click()
                        self.assertEqual(editor.text(), "Review the release notes")
                        self.assertTrue(open_button.isEnabled())
                finally:
                    host.close()
                    host.deleteLater()
                    QApplication.processEvents()

    def test_command_submenu_samples_execute_shared_choices(self):
        from PySide6.QtWidgets import QAbstractButton

        for route in ("command-bar", "command-bar-flyout"):
            with self.subTest(route=route):
                samples = ENTRY_BY_ROUTE_ID[route].samples
                self.assertEqual(
                    samples[0].id,
                    "command-bar-responsive-overflow" if route == "command-bar"
                    else "command-bar-flyout-show-modes",
                )
                self.assertEqual(samples[-1].id, route + "-submenus")
                result = build_sample(route, route + "-submenus")
                root = result.widget
                try:
                    root.resize(800, 400)
                    root.show()
                    QApplication.processEvents()
                    flyout = root.findChild(fluentqt.CommandBarFlyout)
                    if flyout is not None:
                        flyout.setAnimationEnabled(False)
                        root.findChild(
                            QAbstractButton, "Gallery.CommandSubmenus.Open"
                        ).click()
                    view = root.findChild(
                        fluentqt.FluentMenu, "Gallery.CommandSubmenus.View"
                    )
                    presenter = next(
                        button for button in root.findChildren(QAbstractButton)
                        if button.property("commandAction") is view.menuAction()
                        and button.isVisible()
                    )
                    presenter.click()
                    self.assertTrue(view.isVisible())
                    # QAction.menu is not exposed by PySide6 6.2.
                    sort = next(
                        child for child in view.findChildren(fluentqt.FluentMenu)
                        if child.menuAction() is view.actions()[0]
                    )
                    view.setActiveAction(sort.menuAction())
                    QTest.keyClick(view, Qt.Key_Right)
                    order = next(
                        child for child in sort.findChildren(fluentqt.FluentMenu)
                        if child.menuAction() is sort.actions()[0]
                    )
                    sort.setActiveAction(order.menuAction())
                    QTest.keyClick(sort, Qt.Key_Right)
                    self.assertTrue(order.isVisible())
                    descending = order.actions()[-1]
                    order.setActiveAction(descending)
                    QTest.keyClick(order, Qt.Key_Return)
                    QApplication.processEvents()
                    self.assertTrue(descending.isChecked())
                    self.assertEqual(
                        root.findChild(
                            fluentqt.Label, "Gallery.CommandSubmenus.Status"
                        ).text(),
                        "Order: Descending",
                    )
                    self.assertFalse(view.isVisible())
                    if flyout is not None:
                        self.assertFalse(flyout.isOpen())
                    else:
                        bar = root.findChild(fluentqt.CommandBar)
                        compact = root.findChild(
                            QAbstractButton, "Gallery.CommandSubmenus.Compact"
                        )
                        status = root.findChild(
                            fluentqt.Label, "Gallery.CommandSubmenus.Status"
                        )
                        button_size, status_size = compact.size(), status.size()
                        for checked, width in ((True, 80), (False, 480)):
                            compact.setChecked(checked)
                            QApplication.processEvents()
                            self.assertEqual(bar.width(), width)
                            self.assertEqual(compact.size(), button_size)
                            self.assertGreaterEqual(
                                compact.width(), compact.sizeHint().width()
                            )
                            self.assertEqual(status.size(), status_size)
                            self.assertGreaterEqual(
                                status.width(),
                                status.fontMetrics().horizontalAdvance(status.text()),
                            )
                finally:
                    root.close()
                    root.deleteLater()
                    QApplication.processEvents()

    def test_command_bar_action_icons_match_native_size_variants(self):
        result = build_sample(
            "command-bar", "command-bar-responsive-overflow"
        )
        try:
            command_bar = result.widget.findChild(
                fluentqt.CommandBar, "Gallery.CommandBar.Responsive"
            )
            self.assertIsNotNone(command_bar)
            for action in command_bar.primaryActions():
                if action.isSeparator():
                    continue
                with self.subTest(action=action.text()):
                    self.assertEqual(
                        action.icon().availableSizes(),
                        [QSize(16, 16), QSize(20, 20), QSize(24, 24)],
                    )
                    self.assertFalse(
                        action.icon().pixmap(
                            QSize(20, 20),
                            QIcon.Mode.Active,
                            QIcon.State.On,
                        ).isNull()
                    )
                    self.assertFalse(
                        action.icon().pixmap(
                            QSize(20, 20),
                            QIcon.Mode.Disabled,
                            QIcon.State.On,
                        ).isNull()
                    )
        finally:
            result.widget.deleteLater()
            QApplication.processEvents()

    def test_tree_view_delegate_uses_native_indicator_motion_contract(self):
        result = build_sample("tree-view", "tree-view-basic")
        tree = result.widget
        hierarchy_changes = QSignalSpy(tree.indicatorHierarchyTransitionChanged)
        try:
            delegate = tree.itemDelegate()
            paint_names = set(delegate.paint.__func__.__code__.co_names)
            self.assertIn("selectedIndicatorProgress", paint_names)
            self.assertIn("isIndicatorMotionActiveForIndex", paint_names)
            self.assertIn("indicatorMotionDirection", paint_names)
            self.assertIn("indicatorHierarchyTransition", paint_names)
            self.assertTrue(tree.isIndicatorMotionAnimationEnabled())

            tree.resize(520, 252)
            tree.show()
            _qwait(20)
            parent_index = tree.model().index(0, 0)
            child_index = tree.model().index(0, 0, parent_index)
            tree.setSelectedItem(child_index)
            QApplication.processEvents()
            self.assertEqual(tree.indicatorMotionCurrentIndex(), child_index)
            self.assertEqual(
                tree.indicatorHierarchyTransition(),
                fluentqt.TreeView.IndicatorHierarchyTransition.Inward,
            )
        finally:
            changes_before_teardown = hierarchy_changes.count()
            tree.close()
            delete_qobject(tree)
            self.assertEqual(hierarchy_changes.count(), changes_before_teardown,
                             "Teardown must not notify slots using the destroyed TreeView")

    def test_command_bar_reuses_the_gallery_window_editing_router(self):
        window = GalleryWindow(startup_visuals=False)
        window.show()
        try:
            window.navigate(
                "command-bar", record_history=False, animated=False
            )
            _qwait(20)
            routers = window.findChildren(fluentqt.EditingCommandRouter)
            self.assertEqual(len(routers), 1)
            router = routers[0]
            self.assertIs(router.scopeWindow(), window)

            editor = window.findChild(
                fluentqt.LineEdit,
                "Gallery.CommandBar.EditingTarget",
            )
            self.assertIsNotNone(editor)
            page = window._pages["command-bar"][1]
            result = next(
                item
                for item in page._gallery_sample_results
                if item.sample_id == "command-bar-editing-router"
            )
            namespace = result.widget._fluentqt_gallery_source_namespace
            self.assertIs(namespace["router"], router)
            editor = namespace["editor"]
            page.ensureWidgetVisible(editor, 40, 180)
            _qwait(20)
            QApplication.clipboard().clear()
            editor.setFocus(Qt.FocusReason.OtherFocusReason)
            editor.selectAll()
            _qwait(20)
            command = fluentqt.EditingCommandRouter.Command.Copy
            self.assertTrue(router.hasActiveTarget())
            self.assertTrue(router.canExecute(command))
            self.assertTrue(router.action(command).isEnabled())
            self.assertIn("Copy: on", namespace["status"].text())

            bar = namespace["bar"]
            copy_button = next(
                button
                for button in bar.findChildren(fluentqt.Button)
                if button.text() == "Copy" and button.isVisible()
            )
            self.assertTrue(copy_button.isEnabled())
            copy_button.setFocus(Qt.FocusReason.MouseFocusReason)
            editor.deselect()
            _qwait(20)
            self.assertTrue(router.canExecute(command))
            self.assertTrue(copy_button.isEnabled())
            QTest.mouseClick(copy_button, Qt.MouseButton.LeftButton)
            _qwait(20)
            self.assertEqual(
                QApplication.clipboard().text(),
                "Review the release notes before Friday",
            )

            editor.setText("Cut this text")
            editor.setFocus(Qt.FocusReason.OtherFocusReason)
            editor.selectAll()
            _qwait(20)
            cut_command = fluentqt.EditingCommandRouter.Command.Cut
            cut_button = next(
                button
                for button in bar.findChildren(fluentqt.Button)
                if button.text() == "Cut" and button.isVisible()
            )
            cut_button.setFocus(Qt.FocusReason.MouseFocusReason)
            editor.deselect()
            _qwait(20)
            self.assertTrue(router.canExecute(cut_command))
            self.assertTrue(cut_button.isEnabled())
            QTest.mouseClick(cut_button, Qt.MouseButton.LeftButton)
            _qwait(20)
            self.assertEqual(editor.text(), "")
            self.assertEqual(
                QApplication.clipboard().text(), "Cut this text"
            )
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_command_surfaces_and_icons_follow_the_effective_theme(self):
        cases = (
            (
                "command-bar",
                "command-bar-responsive-overflow",
                "panel",
                "bar",
                "primaryActions",
            ),
            (
                "command-bar-flyout",
                "command-bar-flyout-show-modes",
                "photo",
                "flyout",
                "primaryActions",
            ),
        )
        for route_id, sample_id, child_name, owner_name, actions_name in cases:
            with self.subTest(route=route_id, sample=sample_id):
                result = build_sample(route_id, sample_id)
                try:
                    namespace = (
                        result.widget._fluentqt_gallery_source_namespace
                    )
                    root = namespace["root"]
                    child = namespace[child_name]
                    owner = namespace[owner_name]
                    self.assertIsInstance(root, fluentqt.FluentWidget)
                    self.assertIsInstance(child, fluentqt.FluentWidget)
                    actions = getattr(owner, actions_name)()
                    action = next(
                        item for item in actions if not item.isSeparator()
                    )
                    previous_icon_key = action.icon().cacheKey()
                    target = (
                        fluentqt.Theme.Light
                        if fluentqt.theme_uses_dark_appearance(
                            root.effective_theme()
                        )
                        else fluentqt.Theme.Dark
                    )
                    root.setProperty("fluentThemeOverride", int(target))
                    _refresh_fluent_subtree(root)
                    QApplication.processEvents()
                    self.assertEqual(root.effective_theme(), target)
                    self.assertEqual(child.effective_theme(), target)
                    self.assertNotEqual(
                        action.icon().cacheKey(), previous_icon_key
                    )
                finally:
                    result.widget.close()
                    result.widget.deleteLater()
                    QApplication.processEvents()

    def test_theme_refresh_handles_item_view_update_overload(self):
        root = QWidget()
        view = fluentqt.ListView(root)
        _refresh_fluent_subtree(root)
        self.assertTrue(shiboken6.isValid(view))
        root.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)

    def test_sample_cards_preserve_backgroundless_collection_surfaces(self):
        parent = QWidget()
        checked_views = 0
        try:
            for route_id in ("list-view", "tree-view"):
                entry = ENTRY_BY_ROUTE_ID[route_id]
                for sample in entry.samples:
                    with self.subTest(route=route_id, sample=sample.id):
                        card, result = _build_sample_card(
                            entry, sample, parent
                        )
                        views = []
                        if isinstance(
                            result.widget, QAbstractItemView
                        ):
                            views.append(result.widget)
                        views.extend(
                            result.widget.findChildren(QAbstractItemView)
                        )
                        backgroundless_views = [
                            view
                            for view in views
                            if callable(
                                getattr(view, "backgroundVisible", None)
                            )
                            and not view.backgroundVisible()
                        ]
                        self.assertTrue(backgroundless_views)
                        for view in backgroundless_views:
                            self.assertTrue(
                                view.property(
                                    "fluentPreserveParentSurface"
                                )
                            )
                            self.assertIsNotNone(view.viewport())
                            self.assertTrue(
                                view.viewport().property(
                                    "fluentPreserveParentSurface"
                                )
                            )
                            checked_views += 1
                        card.deleteLater()
                        QCoreApplication.sendPostedEvents(
                            None, QEvent.DeferredDelete
                        )
            self.assertGreaterEqual(checked_views, 12)
        finally:
            parent.deleteLater()
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)

    def test_custom_samples_follow_the_local_preview_theme(self):
        original_theme = fluentqt.current_theme()
        fluentqt.set_theme(fluentqt.Theme.Light)
        preview = fluentqt.Card()
        preview.setProperty(
            "fluentThemeOverride", int(fluentqt.Theme.Dark)
        )
        cases = (
            (
                "tree-view",
                "tree-view-basic",
                "row_delegate",
                "_theme_snapshot(self)",
            ),
            (
                "navigation-view",
                "navigation-view-chrome-slots",
                "surface",
                "_theme_tokens(self)",
            ),
        )
        try:
            for route_id, sample_id, context_name, source_marker in cases:
                with self.subTest(route=route_id, sample=sample_id):
                    result = build_sample(route_id, sample_id, preview)
                    try:
                        namespace = (
                            result.widget._fluentqt_gallery_source_namespace
                        )
                        snapshot = _theme_snapshot(namespace[context_name])
                        self.assertEqual(
                            snapshot["theme"], int(fluentqt.Theme.Dark)
                        )
                        self.assertEqual(
                            QColor(snapshot["colors"]["textPrimary"]),
                            QColor("#ffffff"),
                        )
                        self.assertIn(source_marker, result.preview_source)
                        self.assertNotIn(
                            "_theme_tokens()", result.preview_source
                        )
                    finally:
                        result.widget.close()
                        result.widget.deleteLater()
        finally:
            preview.deleteLater()
            fluentqt.set_theme(original_theme)
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)

    def test_automated_gallery_keeps_native_photo_loading_contract(self):
        previous = QApplication.instance().property(
            "fluentqtGalleryAutomated"
        )
        QApplication.instance().setProperty(
            "fluentqtGalleryAutomated", True
        )
        result = build_sample("flow-view", "flow-view-basic")
        try:
            self.assertIsNotNone(
                getattr(
                    result.widget,
                    "_gallery_photo_network_manager",
                    None,
                )
            )
            self.assertGreaterEqual(
                getattr(
                    result.widget,
                    "_gallery_photo_network_pending",
                    -1,
                ),
                0,
            )
            self.assertEqual(
                len(result.widget._gallery_photo_network_replies),
                result.widget._gallery_photo_network_pending,
            )
        finally:
            QApplication.instance().setProperty(
                "fluentqtGalleryAutomated", previous
            )
            result.widget.deleteLater()
            QApplication.processEvents()

    def test_snapshot_network_pending_count_uses_visible_route(self):
        window = GalleryWindow(startup_visuals=False)
        window.navigate("flow-view", animated=False)
        QApplication.processEvents()
        try:
            self.assertGreaterEqual(
                _pending_gallery_network_requests(window), 0
            )
            window.navigate("button", animated=False)
            QApplication.processEvents()
            self.assertEqual(_pending_gallery_network_requests(window), 0)
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_native_root_group_layout_contract_is_applied(self):
        grouped_samples = {
            key: sample
            for key, sample in SAMPLE_BY_KEY.items()
            if sample.preview_orientation is not None
        }
        self.assertEqual(len(grouped_samples), 75)
        expected_types = {
            "horizontal": QHBoxLayout,
            "vertical": QVBoxLayout,
        }
        expected_alignments = {
            "horizontal": Qt.AlignLeft | Qt.AlignVCenter,
            "vertical": Qt.AlignTop | Qt.AlignLeft,
        }
        for (route_id, sample_id), sample in grouped_samples.items():
            with self.subTest(route=route_id, sample=sample_id):
                result = build_sample(route_id, sample_id)
                try:
                    layout = result.widget.layout()
                    self.assertIsInstance(
                        layout,
                        expected_types[sample.preview_orientation],
                    )
                    margins = layout.contentsMargins()
                    self.assertEqual(
                        (
                            margins.left(),
                            margins.top(),
                            margins.right(),
                            margins.bottom(),
                        ),
                        (0, 0, 0, 0),
                    )
                    self.assertEqual(layout.spacing(), sample.preview_spacing)
                    self.assertEqual(
                        layout.alignment(),
                        expected_alignments[sample.preview_orientation],
                    )
                finally:
                    result.widget.close()
                    result.widget.deleteLater()

    def test_high_risk_samples_match_native_gallery_geometry_and_models(self):
        def dispose(result):
            if shiboken6.isValid(result.widget):
                result.widget.close()

        result = build_sample(
            "annotated-scrollbar", "annotated-scrollbar-basic"
        )
        try:
            self.assertEqual(result.widget.size(), QSize(390, 300))
            bar = result.widget.findChild(fluentqt.AnnotatedScrollBar)
            self.assertIsNotNone(bar)
            self.assertEqual(bar.size(), QSize(148, 300))
            self.assertEqual((bar.minimum(), bar.maximum()), (0, 960))
            self.assertEqual(bar.pageStep(), 120)
            self.assertEqual(bar.labelColumnWidth(), 56)
            self.assertEqual(bar.indicatorWidth(), 32)
            self.assertEqual(
                tuple((label.text, label.offset, label.detailText) for label in bar.labels()),
                tuple(
                    (
                        str(year),
                        (2023 - year) * 120,
                        "October {0}".format(year),
                    )
                    for year in range(2023, 2014, -1)
                ),
            )
            texts = {label.text() for label in result.widget.findChildren(fluentqt.Label)}
            self.assertTrue(
                {"Offset: 0", "Current label: 2023", "Detail: October 2023"}
                <= texts
            )
        finally:
            dispose(result)

        result = build_sample(
            "annotated-scrollbar", "annotated-scrollbar-scrollview"
        )
        try:
            self.assertEqual(result.widget.size(), QSize(542, 354))
            scroll = result.widget.findChild(fluentqt.ScrollView)
            bar = result.widget.findChild(fluentqt.AnnotatedScrollBar)
            self.assertEqual(scroll.size(), QSize(380, 320))
            self.assertEqual(bar.size(), QSize(150, 320))
            self.assertEqual(bar.preferredSize(), QSize(150, 320))
            self.assertEqual(bar.minimumBarSize(), QSize(120, 220))
            self.assertEqual(bar.labelColumnWidth(), 86)
            self.assertEqual(bar.minimumLabelSpacing(), 56)
            self.assertEqual(bar.indicatorWidth(), 34)
            self.assertEqual(bar.caretSize(), QSize(16, 18))
            self.assertEqual(
                tuple((label.text, label.offset) for label in bar.labels()),
                (
                    ("Azure", 0),
                    ("Crimson", 900),
                    ("Cyan", 2430),
                    ("Fuchsia", 2700),
                    ("Gold", 4770),
                ),
            )
            self.assertEqual(scroll.contentWidget().size(), QSize(380, 7560))
            texts = {label.text() for label in result.widget.findChildren(fluentqt.Label)}
            self.assertIn("Section: Azure - offset 0", texts)
        finally:
            dispose(result)

        result = build_sample(
            "annotated-scrollbar", "annotated-scrollbar-label-density"
        )
        try:
            self.assertEqual(result.widget.size(), QSize(382, 360))
            bar = result.widget.findChild(fluentqt.AnnotatedScrollBar)
            slider = result.widget.findChild(fluentqt.Slider)
            self.assertEqual(bar.size(), QSize(144, 360))
            self.assertEqual(len(bar.labels()), 12)
            self.assertEqual(
                (slider.minimum(), slider.maximum(), slider.value()),
                (180, 360, 360),
            )
            self.assertEqual((slider.singleStep(), slider.pageStep()), (20, 40))
            self.assertEqual(slider.size(), QSize(220, 36))
            texts = {label.text() for label in result.widget.findChildren(fluentqt.Label)}
            self.assertTrue(
                {"Height: 360 px", "Visible labels: 12 of 12"} <= texts
            )
            result.widget.show()
            QApplication.processEvents()
            initial_size = result.widget.size()
            initial_bar_top = bar.y()
            slider.setValue(220)
            QApplication.processEvents()
            self.assertEqual(result.widget.size(), initial_size)
            self.assertEqual(result.widget.size(), QSize(382, 360))
            self.assertEqual(bar.height(), 220)
            self.assertEqual(bar.y(), initial_bar_top)
            self.assertEqual(bar.y(), 0)
        finally:
            dispose(result)

        result = build_sample("flow-view", "flow-view-basic")
        try:
            view = result.widget
            self.assertEqual(view.size(), QSize(540, 282))
            self.assertEqual(view.defaultItemSize(), QSize(160, 118))
            self.assertEqual(view.minimumItemSize(), QSize(140, 100))
            self.assertEqual(view.maximumItemSize(), QSize(180, 128))
            self.assertEqual((view.horizontalSpacing(), view.verticalSpacing()), (10, 10))
            self.assertEqual(view.model().rowCount(), 9)
            self.assertEqual(view.selectedIndex(), 0)
            self.assertIn(
                "self._grid_view is None and hovered and not selected",
                result.preview_source,
            )
        finally:
            dispose(result)

        result = build_sample("grid-view", "grid-view-basic")
        try:
            view = result.widget
            self.assertEqual(view.size(), QSize(508, 256))
            self.assertEqual(view.cellSize(), QSize(150, 112))
            self.assertEqual(view.maxColumns(), 3)
            self.assertEqual((view.horizontalSpacing(), view.verticalSpacing()), (10, 10))
            self.assertEqual(view.model().rowCount(), 8)
            self.assertEqual(view.selectedIndex(), 0)
            self.assertEqual(
                view.itemDelegate().sizeHint(None, view.model().index(0, 0)),
                view.gridSize(),
            )
            self.assertIn(
                "scrim.setColorAt(0.0, QColor(0, 0, 0, 20))",
                result.preview_source,
            )
            self.assertIn(
                "scrim.setColorAt(1.0, QColor(0, 0, 0, 150))",
                result.preview_source,
            )
            self.assertIn('QFont("FluentQt Icons")', result.preview_source)
            self.assertIn("\ue73e", result.preview_source)
        finally:
            dispose(result)

        for sample_id in (
            "data-grid-large-read-only",
            "data-grid-column-selection",
            "data-grid-edit-validation",
        ):
            result = build_sample("data-grid", sample_id)
            try:
                self.assertIsInstance(result.widget, fluentqt.DataGrid)
                self.assertFalse(result.widget.isScrollChainingEnabled())
                self.assertNotIn(
                    "grid.setScrollChainingEnabled(True)",
                    result.source,
                )
            finally:
                dispose(result)

        result = build_sample("scroll-view", "scroll-view-content-zoom")
        try:
            buttons = result.widget.findChildren(fluentqt.Button)
            self.assertEqual(
                tuple(button.text() for button in buttons),
                ("Zoom out", "Reset", "Zoom in"),
            )
            controls = buttons[0].parentWidget()
            self.assertIsNotNone(controls)
            self.assertIsInstance(controls.layout(), QHBoxLayout)
            self.assertTrue(
                controls.layout().alignment() & Qt.AlignLeft
            )
            self.assertEqual(controls.layout().spacing(), 8)
            self.assertTrue(
                all(button.minimumWidth() == 74 for button in buttons)
            )
            scroll_view = result.widget.findChild(fluentqt.ScrollView)
            self.assertEqual(scroll_view.size(), QSize(420, 240))
            self.assertEqual(
                scroll_view.contentWidget().size(), QSize(760, 520)
            )
        finally:
            dispose(result)

        result = build_sample(
            "flip-view", "flip-view-external-navigation"
        )
        try:
            self.assertIsInstance(result.widget.layout(), QVBoxLayout)
            margins = result.widget.layout().contentsMargins()
            self.assertEqual(
                (
                    margins.left(),
                    margins.top(),
                    margins.right(),
                    margins.bottom(),
                ),
                (0, 0, 0, 0),
            )
            self.assertEqual(result.widget.layout().spacing(), 10)
            flip_view = result.widget.findChild(fluentqt.FlipView)
            self.assertEqual(flip_view.size(), QSize(360, 168))
            buttons = result.widget.findChildren(fluentqt.Button)
            self.assertEqual(
                tuple(button.text() for button in buttons),
                ("Previous", "Next"),
            )
            controls = buttons[0].parentWidget()
            self.assertIsInstance(controls.layout(), QHBoxLayout)
            self.assertEqual(controls.layout().spacing(), 8)
            self.assertTrue(controls.layout().alignment() & Qt.AlignLeft)
            self.assertTrue(controls.layout().alignment() & Qt.AlignVCenter)
            status = next(
                label
                for label in result.widget.findChildren(fluentqt.Label)
                if label.text().startswith("Current page:")
            )
            buttons[1].click()
            self.assertEqual(flip_view.currentIndex(), 1)
            self.assertEqual(status.text(), "Current page: 2")
        finally:
            dispose(result)

        result = build_sample("flip-view", "flip-view-basic")
        try:
            pages = result.widget.findChildren(QLabel)
            self.assertEqual(len(pages), 3)
            self.assertTrue(all(page.text() == "" for page in pages))
            self.assertTrue(
                all(page.alignment() == Qt.AlignCenter for page in pages)
            )
            self.assertIn(
                "Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignBottom",
                result.preview_source,
            )
            self.assertEqual(result.widget.pageCount(), 3)
            self.assertEqual(result.source.count("addOwnedPage("), 3)
        finally:
            dispose(result)

        result = build_sample("flip-view", "flip-view-vertical")
        try:
            self.assertEqual(result.widget.pageCount(), 3)
            self.assertEqual(
                result.widget.orientation(), Qt.Orientation.Vertical
            )
            self.assertEqual(result.source.count("addOwnedPage("), 3)
        finally:
            dispose(result)

        result = build_sample("color-picker", "color-picker-rgba")
        try:
            swatch = next(
                widget
                for widget in result.widget.findChildren(QWidget)
                if widget.size() == QSize(64, 40)
            )
            status_row = swatch.parentWidget()
            self.assertIsInstance(status_row.layout(), QHBoxLayout)
            self.assertEqual(
                status_row.layout().alignment(),
                Qt.AlignLeft | Qt.AlignVCenter,
            )
        finally:
            dispose(result)

        result = build_sample("checkbox", "checkbox-select-all")
        try:
            child_boxes = tuple(
                box
                for box in result.widget.findChildren(fluentqt.CheckBox)
                if box.text() in {"Mail", "Calendar", "People"}
            )
            self.assertEqual(len(child_boxes), 3)
            children = child_boxes[0].parentWidget()
            self.assertIsInstance(children.layout(), QVBoxLayout)
            self.assertEqual(
                children.layout().alignment(),
                Qt.AlignTop | Qt.AlignLeft,
            )
        finally:
            dispose(result)

        result = build_sample("scrollbar", "scrollbar-basic")
        try:
            bars = result.widget.findChildren(fluentqt.ScrollBar)
            horizontal = next(
                bar for bar in bars if bar.orientation() == Qt.Horizontal
            )
            vertical = next(
                bar for bar in bars if bar.orientation() == Qt.Vertical
            )
            horizontal_column = horizontal.parentWidget()
            row = vertical.parentWidget()
            self.assertEqual(
                horizontal_column.layout().alignment(),
                Qt.AlignTop | Qt.AlignLeft,
            )
            self.assertEqual(
                row.layout().alignment(),
                Qt.AlignLeft | Qt.AlignVCenter,
            )
        finally:
            dispose(result)

        result = build_sample("font-icon", "font-icon-optical-sizes")
        try:
            self.assertEqual(result.widget.size(), QSize(420, 104))
            self.assertEqual(
                tuple(icon.iconSize() for icon in result.widget.findChildren(fluentqt.FontIcon)),
                (12, 16, 24, 32),
            )
            captions = tuple(
                label.text()
                for label in result.widget.findChildren(fluentqt.Label)
            )
            self.assertEqual(captions, ("12 px", "16 px", "24 px", "32 px"))
            self.assertNotIn("for ", result.source)
            self.assertNotIn("values =", result.source)
            self.assertNotIn("make_icon_cell", result.source)
            for name in ("compact", "standard", "medium", "large"):
                self.assertIn(
                    "{0} = fluentqt.FontIcon("
                    "fluentqt.Typography.Icons.Search)".format(name),
                    result.source,
                )
            self.assertIn(
                "compact.setIconSize(fluentqt.Typography.IconSize.Compact)",
                result.source,
            )
            self.assertIn(
                "standard.setIconSize(fluentqt.Typography.IconSize.Standard)",
                result.source,
            )
        finally:
            dispose(result)

        result = build_sample("button", "button-icon-layouts")
        try:
            self.assertIn(
                "leading.setIconGlyph(fluentqt.Typography.Icons.Add)",
                result.source,
            )
            self.assertIn(
                'icon_only.setIconGlyph('
                "fluentqt.Typography.Icons.More)",
                result.source,
            )
            self.assertIn(
                'trailing.setIconGlyph('
                "fluentqt.Typography.Icons.ChevronRight)",
                result.source,
            )
        finally:
            dispose(result)

        result = build_sample("shimmer", "shimmer-static-phase")
        try:
            self.assertEqual(type(result.widget).__name__, "StatusInfoSampleSurface")
            shimmer = result.widget.findChild(fluentqt.Shimmer)
            self.assertEqual(shimmer.size(), QSize(260, 72))
            self.assertFalse(shimmer.isAnimationEnabled())
            self.assertAlmostEqual(shimmer.shimmerProgress(), 0.42)
            self.assertIn(
                "Progress phase: 0.42",
                {label.text() for label in result.widget.findChildren(fluentqt.Label)},
            )
        finally:
            dispose(result)

    def test_gallery_pixmap_draw_matches_cpp_logical_dpr_contract(self):
        image = QImage(
            80,
            80,
            QImage.Format.Format_ARGB32_Premultiplied,
        )
        image.setDevicePixelRatio(2.0)
        image.fill(Qt.GlobalColor.transparent)
        source = QPixmap(10, 5)
        source.fill(QColor(220, 30, 40))

        painter = QPainter(image)
        _draw_pixmap_in_logical_rect(
            painter,
            QRectF(0.0, 0.0, 40.0, 40.0),
            source,
        )
        painter.end()

        self.assertEqual(source.devicePixelRatioF(), 1.0)
        self.assertEqual(image.pixelColor(40, 19).alpha(), 0)
        self.assertEqual(image.pixelColor(40, 20), QColor(220, 30, 40))
        self.assertEqual(image.pixelColor(40, 59), QColor(220, 30, 40))
        self.assertEqual(image.pixelColor(40, 60).alpha(), 0)

    def test_gallery_svg_artwork_uses_display_resolution_and_cached_pixels(self):
        source = control_image_path("collections", "Timeline")
        self.assertEqual(source.suffix, ".svg")
        self.assertEqual(control_image_path("foundation", "QML+").name, "QMLPlus.svg")
        self.assertEqual(control_image_path("missing", "Unknown").name, "Placeholder.svg")
        with TemporaryDirectory() as temporary:
            path = Path(temporary) / "Timeline.svg"
            path.write_bytes(source.read_bytes())
            misses = _gallery_artwork_renderer.cache_info().misses
            renderer = QSvgRenderer(str(path))
            renders = []
            for dpr, physical_size in ((1.0, 36), (1.25, 45), (2.0, 72), (3.0, 108)):
                with self.subTest(dpr=dpr):
                    pixmap = _gallery_artwork_pixmap(path, QSize(36, 36), dpr)
                    self.assertEqual(pixmap.size(), QSize(physical_size, physical_size))
                    self.assertAlmostEqual(pixmap.devicePixelRatioF(), dpr)
                    self.assertEqual(
                        _gallery_artwork_pixmap(path, QSize(36, 36), dpr).cacheKey(),
                        pixmap.cacheKey(),
                    )
                    reference = QImage(pixmap.size(), QImage.Format_ARGB32_Premultiplied)
                    reference.fill(Qt.GlobalColor.transparent)
                    painter = QPainter(reference)
                    painter.setRenderHint(QPainter.Antialiasing)
                    renderer.render(painter, QRectF(QPointF(), reference.size()))
                    painter.end()
                    actual = pixmap.toImage()
                    actual.setDevicePixelRatio(1.0)
                    self.assertEqual(actual, reference)
                    renders.append(pixmap)
            self.assertEqual(_gallery_artwork_renderer.cache_info().misses - misses, 1)
            for pixmap in renders:
                self.assertEqual(
                    _gallery_artwork_pixmap(path, QSize(36, 36), pixmap.devicePixelRatioF()).cacheKey(),
                    pixmap.cacheKey(),
                )
            larger = _gallery_artwork_pixmap(path, QSize(40, 40), 3.0)
            self.assertEqual(larger.size(), QSize(120, 120))
            self.assertNotEqual(larger.cacheKey(), renders[-1].cacheKey())

    def test_gallery_svg_artwork_paints_tiles_and_virtualized_cards_at_three_times_scale(self):
        path = control_image_path("collections", "Timeline")
        card = GalleryEntryCard("timeline", "Timeline", "", image_path=path)
        grid = GalleryEntryGrid()
        grid.resize(320, 86)
        grid.set_cards([card])
        expected = _gallery_artwork_pixmap(path, QSize(36, 36), 3.0).toImage()
        try:
            for widget, origin in ((card._icon, QPoint(6, 6)), (grid, QPoint(54, 54))):
                image = QImage(widget.size() * 3, QImage.Format_ARGB32_Premultiplied)
                image.setDevicePixelRatio(3.0)
                image.fill(Qt.GlobalColor.transparent)
                widget.render(image)
                for y in range(expected.height()):
                    for x in range(expected.width()):
                        if expected.pixelColor(x, y).alpha() == 255:
                            self.assertEqual(image.pixelColor(origin + QPoint(x, y)), expected.pixelColor(x, y))
        finally:
            delete_qobject(grid)

    def test_gallery_raster_assets_are_rendered_at_physical_dpr(self):
        for dpr, expected_app_size, expected_link_size in (
            (1.0, 18, 32),
            (1.25, 23, 40),
            (2.0, 36, 64),
            (3.0, 54, 96),
        ):
            with self.subTest(dpr=dpr):
                app_pixmap = app_icon_pixmap(18, dpr)
                self.assertEqual(
                    app_pixmap.size(),
                    QSize(expected_app_size, expected_app_size),
                )
                self.assertAlmostEqual(app_pixmap.devicePixelRatioF(), dpr)

                link_pixmap = _hero_link_pixmap(
                    "GitHub-Mark.png",
                    32,
                    QColor("#202020"),
                    dpr,
                )
                self.assertEqual(
                    max(
                        link_pixmap.width(),
                        link_pixmap.height(),
                    ),
                    expected_link_size,
                )
                self.assertGreaterEqual(
                    min(
                        link_pixmap.width(),
                        link_pixmap.height(),
                    ),
                    int(expected_link_size * 0.9),
                )
                self.assertAlmostEqual(link_pixmap.devicePixelRatioF(), dpr)

                font_pixmap = gallery_font_icon_pixmap(
                    "ic_fluent_open_16_regular",
                    16,
                    QColor("#202020"),
                    dpr,
                )
                self.assertEqual(
                    font_pixmap.size(),
                    QSize(
                        int(16 * dpr + 0.5),
                        int(16 * dpr + 0.5),
                    ),
                )
                self.assertAlmostEqual(font_pixmap.devicePixelRatioF(), dpr)

    def test_gallery_display_scale_event_refreshes_visible_raster_assets(self):
        window = GalleryWindow(startup_visuals=False)
        window.show()
        QApplication.processEvents()
        try:
            title_icon = window.findChild(
                QLabel,
                "GalleryTitleBar.AppIcon",
            )
            hero_icon = window.findChild(QLabel, "galleryHomeHeroIcon")
            self.assertIsNotNone(title_icon)
            self.assertIsNotNone(hero_icon)
            title_before = title_icon.pixmap()
            hero_before = hero_icon.pixmap()
            self.assertFalse(title_before.isNull())
            self.assertFalse(hero_before.isNull())

            screen_change = getattr(
                QEvent.Type,
                "ScreenChangeInternal",
                None,
            )
            if screen_change is None:
                self.skipTest("Qt does not expose ScreenChangeInternal")
            QApplication.sendEvent(window, QEvent(screen_change))
            _qwait(20)

            title_after = title_icon.pixmap()
            hero_after = hero_icon.pixmap()
            self.assertNotEqual(title_after.cacheKey(), title_before.cacheKey())
            self.assertNotEqual(hero_after.cacheKey(), hero_before.cacheKey())
            for label, logical_size in ((title_icon, 18), (hero_icon, 56)):
                pixmap = label.pixmap()
                dpr = max(1.0, label.devicePixelRatioF())
                self.assertAlmostEqual(pixmap.devicePixelRatioF(), dpr)
                self.assertEqual(
                    pixmap.size(),
                    QSize(
                        int(logical_size * dpr + 0.5),
                        int(logical_size * dpr + 0.5),
                    ),
                )
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_multi_select_status_wraps_complete_selection(self):
        result = build_sample(
            "multi-select-combobox",
            "multi-select-combobox-selection",
        )
        try:
            box = result.widget.findChild(fluentqt.MultiSelectComboBox)
            self.assertIsNotNone(box)
            status = next(
                label
                for label in result.widget.findChildren(fluentqt.Label)
                if label.text().startswith("Selected:")
            )

            box.selectAll()
            QApplication.processEvents()
            self.assertEqual(
                status.text(),
                "Selected: Design, Engineering, Research, Support",
            )
            self.assertTrue(status.wordWrap())
            self.assertEqual(status.width(), box.width())
            self.assertGreater(
                status.heightForWidth(status.width()),
                status.fontMetrics().height(),
            )
            self.assertIn("status.setWordWrap(True)", result.preview_source)
        finally:
            if shiboken6.isValid(result.widget):
                result.widget.close()

    def test_gallery_generated_pixels_match_cpp_argb_contract(self):
        tile = _acrylic_noise_tile()
        expected_noise = _qt_seeded_bytes(0xACE71C5E, 96 * 96)
        self.assertEqual(tile.size(), QSize(96, 96))
        for index in (0, 1, 95, 96, 4096, 9215):
            value = expected_noise[index]
            self.assertEqual(
                int(tile.pixel(index % 96, index // 96)),
                0xFF000000 | value * 0x010101,
            )

        mark = QImage(4, 1, QImage.Format_ARGB32)
        source_pixels = (
            0xFF000000,
            0xFFFFFFFF,
            0xFFFF0000,
            0x80000000,
        )
        mark_pixels = mark.bits().cast("I")
        for x, pixel in enumerate(source_pixels):
            mark_pixels[x] = pixel
        _tint_github_mark(mark, QColor(0x12, 0x34, 0x56))
        self.assertEqual(
            tuple(int(mark.pixel(x, 0)) for x in range(4)),
            (
                0xFF123456,
                0x00123456,
                0xA8123456,
                0x80123456,
            ),
        )

    def test_hero_alpha_bounds_preserve_threshold_and_pixel_extent(self):
        for image_format in (
            QImage.Format_ARGB32,
            QImage.Format_RGBA8888,
            QImage.Format_ARGB32_Premultiplied,
        ):
            with self.subTest(image_format=image_format):
                image = QImage(11, 7, image_format)
                image.fill(Qt.transparent)
                self.assertTrue(_image_alpha_bounds(image).isNull())
                image.setPixelColor(0, 0, QColor(255, 255, 255, 8))
                self.assertTrue(_image_alpha_bounds(image).isNull())
                image.setPixelColor(7, 2, QColor(20, 30, 40, 9))
                image.setPixelColor(3, 4, QColor(40, 60, 80, 255))
                image.setPixelColor(10, 6, QColor(1, 2, 3, 8))
                self.assertEqual(_image_alpha_bounds(image), QRect(3, 2, 5, 3))
                image.setPixelColor(10, 6, QColor(1, 2, 3, 9))
                self.assertEqual(_image_alpha_bounds(image), QRect(3, 2, 8, 5))
                image.fill(QColor(5, 10, 15, 255))
                self.assertEqual(_image_alpha_bounds(image), image.rect())

    def test_hero_tint_palette_cache_preserves_alpha_for_repeats_and_overflow(self):
        image = QImage(40, 40, QImage.Format_ARGB32)
        pixels = image.bits().cast("I")
        expected = []
        for index in range(1600):
            seed = index if index < 1400 else index - 1400
            source = ((seed * 17 % 256) << 24) | (seed * 41 & 0xFFFFFF)
            pixels[index] = source
            luminance = (
                ((source >> 16) & 255) * 11
                + ((source >> 8) & 255) * 16 + (source & 255) * 5
            ) // 32
            alpha = (255 - luminance) * (source >> 24) // 255
            expected.append((alpha << 24) | 0x123456)
        _tint_github_mark(image, QColor(0x12, 0x34, 0x56))
        self.assertEqual(list(image.constBits().cast("I")), expected)

    def test_particle_sample_cards_fill_width_and_controls_change_motion(self):
        entry = ENTRY_BY_ROUTE_ID["particle-backdrop"]
        for sample in entry.samples:
            self.assertTrue(sample.fill_available_width)
            card, result = _build_sample_card(entry, sample, None)
            try:
                for width in (880, 400):
                    card.resize(width, 650)
                    card.show()
                    _qwait(30)
                    preview = result.widget
                    self.assertEqual(
                        preview.width(), preview.parentWidget().width() - 40
                    )
                    self.assertLessEqual(preview.parentWidget().width(), card.width())
                if sample.id == "particle-backdrop-interaction":
                    backdrop = preview.findChild(fluentqt.ParticleBackdrop)
                    effect = preview.findChild(fluentqt.ComboBox, "particleEffect")
                    pause = preview.findChild(fluentqt.Button, "particlePause")
                    speed = preview.findChild(fluentqt.Slider, "particleSpeed")
                    QTest.mouseClick(pause, Qt.LeftButton)
                    self.assertFalse(backdrop.isAnimationEnabled())
                    self.assertEqual(pause.text(), "Resume motion")
                    self.assertEqual(backdrop.effect(), fluentqt.ParticleBackdrop.Effect.Starfield)
                    presets = (
                        ("FlowingRibbons", fluentqt.ParticleBackdrop.Effect.FlowingRibbons),
                        ("FloatingDots", fluentqt.ParticleBackdrop.Effect.FloatingDots),
                        ("Starfield", fluentqt.ParticleBackdrop.Effect.Starfield),
                    )
                    for index, (identifier, preset) in enumerate(presets):
                        effect.setCurrentIndex(index)
                        self.assertEqual(backdrop.effect(), preset)
                        self.assertFalse(backdrop.isAnimationEnabled())
                        self.assertIn(identifier, sample.cpp_snippet)
                    speed.setValue(150)
                    self.assertEqual(backdrop.speed(), 1.5)
                    QTest.mouseClick(pause, Qt.LeftButton)
                    self.assertTrue(backdrop.isAnimationEnabled())
            finally:
                card.close()

    def test_existing_samples_keep_preview_row_spacing(self):
        for route in ("button", "slider", "tab-view", "navigation-view"):
            entry = ENTRY_BY_ROUTE_ID[route]
            for sample in entry.samples:
                self.assertFalse(sample.fill_available_width)
                card, result = _build_sample_card(entry, sample, None)
                try:
                    row = result.widget.parentWidget().layout()
                    self.assertEqual(row.stretch(0), 0)
                    self.assertIsNotNone(row.itemAt(1).spacerItem())
                finally:
                    card.close()

    def test_sequence_samples_fill_width_and_keep_interactions_live(self):
        for route in ("timeline", "stepper"):
            entry = ENTRY_BY_ROUTE_ID[route]
            for sample in entry.samples:
                with self.subTest(route=route, sample=sample.id):
                    self.assertTrue(sample.fill_available_width)
                    card, result = _build_sample_card(entry, sample, None)
                    try:
                        for width in (880, 400):
                            card.resize(width, 650)
                            card.show()
                            _qwait(30)
                            root = result.widget
                            self.assertEqual(root.width(), root.parentWidget().width() - 40)
                            control = root.findChild(
                                fluentqt.Timeline if route == "timeline" else fluentqt.Stepper
                            )
                            self.assertEqual(control.width(), root.width())
                        if route == "timeline":
                            change = root.findChild(fluentqt.Button, "timelineChangeState")
                            QTest.mouseClick(change, Qt.LeftButton)
                            self.assertEqual(
                                control.model().index(1, 0).data(fluentqt.Timeline.DataRole.StatusRole),
                                fluentqt.Timeline.Status.Success,
                            )
                        else:
                            toggle = root.findChild(fluentqt.Button, "stepperToggleError")
                            QTest.mouseClick(toggle, Qt.LeftButton)
                            self.assertEqual(control.itemAt(1).state, fluentqt.Stepper.State.Error)
                            control.findChildren(fluentqt.Button, "stepperStep")[0].click()
                            self.assertEqual(control.currentIndex(), 0)
                            self.assertEqual(control.itemAt(1).state, fluentqt.Stepper.State.Error)
                    finally:
                        card.close()
                        card.deleteLater()

    def test_sample_card_preview_growth_settles_scroll_layout(self):
        for width in (720, 420):
            with self.subTest(width=width):
                page = build_component_page(ENTRY_BY_ROUTE_ID["button"], lambda _route: None)
                page.resize(width, 280)
                first, second = page._gallery_sample_cards[:2]
                content = page._gallery_content
                spacing = first.parentWidget().layout().spacing()
                try:
                    page.show()
                    self.assertGreaterEqual(second.y(), first.geometry().bottom() + 1 + spacing)
                    QApplication.processEvents()
                    before_height = first.height()
                    before_second_top = second.y()
                    before_content_height = content.height()

                    preview = first._gallery_preview_widget
                    preview.setMinimumHeight(preview.height() + 240)
                    first.resize(first.width() + 1, first.height())
                    # Check before posted layout requests can repair stale siblings.
                    self.assertGreater(first.height(), before_height)
                    self.assertGreater(second.y(), before_second_top)
                    self.assertGreaterEqual(second.y(), first.geometry().bottom() + 1 + spacing)
                    self.assertGreater(content.height(), before_content_height)
                    self.assertGreaterEqual(content.height(), content.layout().minimumSize().height())
                finally:
                    page.close()
                    page.deleteLater()
                    QApplication.processEvents()

    def test_sample_card_source_transition_settles_scroll_layout(self):
        page = build_component_page(ENTRY_BY_ROUTE_ID["button"], lambda _route: None)
        page.resize(420, 320)
        first, second = page._gallery_sample_cards[:2]
        spacing = first.parentWidget().layout().spacing()
        try:
            page.show()
            QApplication.processEvents()
            collapsed_height = first.height()
            collapsed_second_top = second.y()
            code = first._gallery_source_expander
            code.setExpandedAnimated(True, False)
            self.assertGreater(first.height(), collapsed_height)
            self.assertGreaterEqual(second.y(), first.geometry().bottom() + 1 + spacing)
            self.assertGreaterEqual(page._gallery_content.height(), page._gallery_content.layout().minimumSize().height())

            code.setExpandedAnimated(False, False)
            self.assertEqual(first.height(), collapsed_height)
            self.assertEqual(second.y(), collapsed_second_top)
            self.assertGreaterEqual(second.y(), first.geometry().bottom() + 1 + spacing)
        finally:
            page.close()
            page.deleteLater()
            QApplication.processEvents()

    def test_sample_card_and_code_block_match_native_shell_behavior(self):
        window = GalleryWindow()
        window.show()
        window.navigate("button", animated=False)
        QApplication.processEvents()
        try:
            _index, page = window._pages["button"]
            card = page._gallery_sample_cards[0]
            margins = card.layout().contentsMargins()
            self.assertEqual(
                (margins.left(), margins.top(), margins.right(), margins.bottom()),
                (20, 18, 20, 18),
            )
            self.assertEqual(card.layout().spacing(), 12)
            self.assertEqual(
                card.sizePolicy().horizontalPolicy(),
                QSizePolicy.Policy.Preferred,
            )
            self.assertGreaterEqual(card.minimumSizeHint().width(), 280)

            preview = card._gallery_preview_surface
            preview_margins = preview.layout().contentsMargins()
            self.assertEqual(
                (
                    preview_margins.left(),
                    preview_margins.top(),
                    preview_margins.right(),
                    preview_margins.bottom(),
                ),
                (20, 20, 20, 20),
            )
            self.assertEqual(preview.layout().spacing(), 16)

            card._update_anchored_layout()
            QApplication.processEvents()
            anchored_widgets = tuple(
                widget
                for widget in (
                    card._gallery_title,
                    card._gallery_description,
                    card._gallery_preview_surface,
                    card._gallery_source_expander,
                )
                if widget is not None
            )
            self.assertEqual(
                [
                    current.y() - (previous.y() + previous.height())
                    for previous, current in zip(
                        anchored_widgets, anchored_widgets[1:]
                    )
                ],
                [12] * (len(anchored_widgets) - 1),
            )

            code = card._gallery_source_expander
            self.assertTrue(code.isAnimationEnabled())
            self.assertEqual(code.headerText(), "Source code")
            self.assertIsNotNone(code.findChild(QWidget, "galleryCodeBlockHeader"))
            self.assertIsNotNone(code.findChild(QWidget, "galleryCodeBlockContent"))
            code_label = code.findChild(fluentqt.Label, "galleryCodeBlockText")
            self.assertIsNotNone(code_label)
            self.assertEqual(code_label.text(), "")
            self.assertTrue(code_label.wordWrap())
            self.assertIsInstance(code, fluentqt.CodeBlock)
            # Retain the viewport wrapper while inspecting its native child.
            code_viewport = code_label.parentWidget()
            self.assertEqual(code_viewport.objectName(), "fluentCodeViewport")
            source = card._gallery_result.source
            self.assertNotIn("for text, style in", source)
            self.assertRegex(
                source,
                r"accent = fluentqt\.Button\(['\"]Accent['\"]\)",
            )
            self.assertLessEqual(
                max(len(line) for line in source.splitlines()),
                88,
            )
            collapsed_height = card.height()
            code.setExpandedAnimated(True, False)
            QApplication.processEvents()
            card._update_anchored_layout()
            self.assertGreater(card.height(), collapsed_height)
            self.assertIn('<span style="color:', code_label.text())
            self.assertNotIn("<pre", code_label.text())
            expected_function = (
                "#DCDCAA"
                if fluentqt.theme_uses_dark_appearance(
                    fluentqt.current_theme()
                )
                else "#795E26"
            )
            self.assertIn(expected_function, code_label.text())

            theme_button = page._gallery_theme_button
            initial_theme = page._gallery_sample_theme
            self.assertEqual(
                theme_button.property("gallerySampleTheme"),
                {
                    fluentqt.Theme.Light: "Light",
                    fluentqt.Theme.Dark: "Dark",
                    fluentqt.Theme.HighContrast: "High contrast",
                }[initial_theme],
            )
            theme_button.click()
            QApplication.processEvents()
            self.assertNotEqual(page._gallery_sample_theme, initial_theme)
            self.assertEqual(
                preview.property("fluentThemeOverride"),
                int(page._gallery_sample_theme),
            )
            self.assertIn("Preview theme:", theme_button.accessibleName())
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_public_code_block_sample_wrap_control(self):
        result = build_sample("code-block", "code-block-scrolling")
        try:
            code = result.widget.findChild(fluentqt.CodeBlock, "sampleCodeBlock")
            wrap = result.widget.findChild(fluentqt.ToggleSwitch, "codeBlockWrap")
            self.assertIsNotNone(code)
            self.assertIsNotNone(wrap)
            self.assertEqual(code.maxVisibleLines(), 6)
            self.assertEqual(code.language(), "python")
            self.assertTrue(code.isWordWrapEnabled())
            source = code.code()
            wrap.setIsOn(False)
            self.assertFalse(code.isWordWrapEnabled())
            self.assertEqual(code.code(), source)
            wrap.setIsOn(True)
            self.assertTrue(code.isWordWrapEnabled())
        finally:
            result.widget.close()
            result.widget.deleteLater()
            QApplication.processEvents()

    def test_code_block_wraps_long_python_at_narrow_width(self):
        block = GalleryCodeBlock(
            "result = fluentqt.Button.ButtonStyle.Accent "
            "+ fluentqt.Button.ButtonLayout.IconBefore"
        )
        block.resize(300, block.height())
        block.setExpandedAnimated(True, False)
        block.show()
        QApplication.processEvents()
        try:
            label = block.findChild(
                fluentqt.Label,
                "galleryCodeBlockText",
            )
            content = block.findChild(
                QWidget,
                "galleryCodeBlockContentInner",
            )
            self.assertIsNotNone(label)
            self.assertIsNotNone(content)
            self.assertTrue(label.hasHeightForWidth())
            self.assertGreater(
                label.heightForWidth(170),
                label.heightForWidth(700),
            )
            self.assertIn("&#8203;", label.text())
            self.assertNotIn("fluentqt.&#8203;", label.text())
            self.assertLessEqual(label.width(), content.width())
            self.assertLessEqual(content.width(), block.width())
        finally:
            block.close()
            block.deleteLater()
            QApplication.processEvents()

    def test_code_block_reuses_native_renderer_and_preserves_terminal_newline(self):
        source = 'button = fluentqt.Button("Continue")\r\n'
        block = GalleryCodeBlock(source)
        self.assertIsInstance(block, fluentqt.CodeBlock)
        try:
            block.resize(760, block.height())
            block.setExpandedAnimated(True, False)
            block.show()
            QApplication.processEvents()
            label = block.findChild(fluentqt.Label, "galleryCodeBlockText")
            self.assertIsNotNone(label)
            self.assertEqual(block.code(), source)
            self.assertEqual(block.language(), "python")
            self.assertEqual(block.maxVisibleLines(), 0)
            block.copy()
            self.assertEqual(QApplication.clipboard().text(), source)
            self.assertGreaterEqual(label.height(), label.heightForWidth(label.width()))
            copy = block.findChild(fluentqt.Button, "galleryCodeBlockCopyButton")
            self.assertEqual(copy.parentWidget().objectName(), "galleryCodeBlockHeader")
        finally:
            block.close()
            block.deleteLater()
            QApplication.processEvents()

    def test_drawer_close_policy_avoids_macos_title_bar(self):
        window = GalleryWindow(startup_visuals=False)
        window.show()
        window.navigate("drawer-view", animated=False)
        QApplication.processEvents()
        drawer = None
        try:
            _index, page = window._pages["drawer-view"]
            card = next(
                card
                for card in page._gallery_sample_cards
                if card.property("gallerySampleId")
                == "drawer-view-close-policy"
            )
            result = card._gallery_result
            drawer = result.widget._fluentqt_gallery_source_namespace[
                "drawer"
            ]
            expected_available_top = (
                TITLE_BAR_HEIGHT
                if sys.platform == "darwin"
                else 0
            )
            self.assertEqual(
                drawer.availableMargins(),
                drawer_title_bar_avoidance_margins(),
            )
            self.assertEqual(
                drawer.availableMargins().top(),
                expected_available_top,
            )
            self.assertIn(
                "drawer_title_bar_avoidance_margins()",
                result.source,
            )
            self.assertNotIn("setAvailableMargins(QMargins())", result.source)

            drawer.setAnimationEnabled(False)
            drawer.open()
            QApplication.processEvents()
            overlay_surface = window.property("fluentOverlaySurfaceRect")
            self.assertIsInstance(overlay_surface, QRect)
            self.assertEqual(
                drawer.geometry().top(),
                overlay_surface.top() + expected_available_top,
            )
            if sys.platform == "darwin":
                self.assertGreater(drawer.geometry().top(), 0)
        finally:
            if drawer is not None:
                drawer.close()
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_coach_mark_sample_reuses_overlay_until_preview_is_destroyed(self):
        for close_before_destroy in (False, True):
            with self.subTest(close_before_destroy=close_before_destroy):
                window = QWidget()
                layout = QVBoxLayout(window)
                result = build_sample(
                    "coach-mark", "coach-mark-targeted-glide", window
                )
                layout.addWidget(result.widget)
                window.resize(800, 500)
                window.show()
                QApplication.processEvents()
                try:
                    bottom = result.widget.findChild(
                        fluentqt.Button, "galleryCoachMarkBottom"
                    )
                    self.assertIsNotNone(bottom)
                    QTest.mouseClick(bottom, Qt.MouseButton.LeftButton)
                    coach = window.findChild(fluentqt.CoachMark)
                    self.assertIsNotNone(coach)
                    self.assertTrue(coach.isOpen())
                    self.assertIs(coach.parentWidget(), window)
                    dismiss = coach.findChild(
                        fluentqt.Button, "galleryCoachMarkDismiss"
                    )
                    self.assertIsNotNone(dismiss)
                    self.assertEqual(dismiss.accessibleName(), "Close")
                    got_it = coach.findChild(
                        fluentqt.Button, "galleryCoachMarkClose"
                    )
                    self.assertIsNotNone(got_it)

                    QTest.mouseClick(got_it, Qt.MouseButton.LeftButton)
                    self.assertFalse(coach.isOpen())
                    self.assertTrue(_wait_until(lambda: not coach.isVisible()))
                    self.assertTrue(shiboken6.isValid(coach))
                    QTest.mouseClick(bottom, Qt.MouseButton.LeftButton)
                    self.assertTrue(coach.isOpen())
                    self.assertEqual(window.findChildren(fluentqt.CoachMark), [coach])

                    if close_before_destroy:
                        QTest.mouseClick(got_it, Qt.MouseButton.LeftButton)
                        self.assertFalse(coach.isOpen())
                        self.assertTrue(_wait_until(lambda: not coach.isVisible()))

                    result.widget.deleteLater()
                    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
                    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
                    self.assertFalse(shiboken6.isValid(coach))
                    self.assertEqual(window.findChildren(fluentqt.CoachMark), [])
                finally:
                    window.close()
                    window.deleteLater()
                    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)

    def test_display_unparse_is_stable_across_python_versions(self):
        from fluentqt_gallery.native_samples import _stable_unparse

        expected = (
            "action = lambda: 'lambda :'\n"
            "points = [('中文', x) for x, y in pairs]\n"
            "nested = [x for (x, y), z in triples]"
        )
        node = ast.parse(expected)
        self.assertEqual(_stable_unparse(node), expected)
        legacy = (
            "action = lambda : 'lambda :'\n"
            "points = [('中文', x) for (x, y) in pairs]\n"
            "nested = [x for ((x, y), z) in triples]"
        )
        with patch.object(ast, "unparse", return_value=legacy):
            normalized = _stable_unparse(node)
        self.assertEqual(normalized, expected)
        self.assertEqual(ast.dump(ast.parse(normalized)), ast.dump(node))

    def test_python_snippets_follow_cpp_semantic_wrap_boundaries(self):
        cases = (
            (
                "compound-button",
                "compound-button-content",
                "standard = fluentqt.CompoundButton(\n"
                "    'Install update',\n"
                "    'Download and restart the app'\n"
                ")",
            ),
            (
                "scroll-view",
                "scroll-view-content-zoom",
                "scroll_view.setVerticalScrollBarVisibility(\n"
                "    fluentqt.ScrollView.ScrollBarVisibility.Auto\n"
                ")",
            ),
            (
                "tab-view",
                "tab-view-add-close",
                "tabs.addTab(\n"
                "    fluentqt.TabViewItem(\"Disabled\", "
                "fluentqt.Typography.Icons.Lock, True, False)\n"
                ")",
            ),
            (
                "toast",
                "toast-title-placement",
                "bottom_start.clicked.connect(\n"
                "    lambda: present(bottom_start, "
                "fluentqt.Toast.Placement.BottomStart)\n"
                ")",
            ),
        )
        for route_id, sample_id, expected in cases:
            with self.subTest(route=route_id, sample=sample_id):
                result = build_sample(route_id, sample_id)
                try:
                    self.assertIn(expected, result.source)
                    self.assertLessEqual(
                        max(map(len, result.source.splitlines()), default=0),
                        _DISPLAY_HARD_LINE_LENGTH,
                    )
                finally:
                    result.widget.close()
                    result.widget.deleteLater()

        command_bar = build_sample(
            "command-bar", "command-bar-responsive-overflow"
        )
        try:
            self.assertNotIn(
                "bar.addPrimaryAction(add_action)\n\n"
                "bar.addPrimaryAction(edit_action)",
                command_bar.source,
            )
        finally:
            command_bar.widget.close()
            command_bar.widget.deleteLater()
            QApplication.processEvents()

    def test_macos_dock_icon_matches_native_gallery_visual_padding(self):
        source = QPixmap(1024, 1024)
        source.fill(QColor("#0078D4"))
        padded = _macos_dock_icon_pixmap(source)
        self.assertEqual(padded.size(), QSize(1024, 1024))
        image = padded.toImage()
        self.assertEqual(image.pixelColor(60, 512).alpha(), 0)
        self.assertGreater(image.pixelColor(61, 512).alpha(), 0)
        self.assertGreater(image.pixelColor(961, 512).alpha(), 0)
        self.assertEqual(image.pixelColor(962, 512).alpha(), 0)

        icon = app_icon()
        self.assertFalse(icon.isNull())
        for size in (256, 512, 1024):
            with self.subTest(size=size):
                self.assertEqual(icon.actualSize(QSize(size, size)), QSize(size, size))

    def test_content_labels_match_native_tracked_style_sheet_contract(self):
        window = GalleryWindow(startup_visuals=False)
        window.show()
        window.navigate("button", animated=False)
        _qwait(50)
        try:
            _index, page = window._pages["button"]
            colors = gallery_colors()
            expected_primary = css_color(colors.text_primary)
            expected_secondary = css_color(colors.text_secondary)

            title = page.findChild(fluentqt.Label, "galleryContentTitle")
            section = page.findChild(
                fluentqt.Label, "galleryContentSectionHeader"
            )
            body = page.findChild(fluentqt.Label, "galleryContentBody")
            self.assertIsNotNone(title)
            self.assertIsNotNone(section)
            self.assertIsNotNone(body)
            self.assertIn(expected_primary, title.styleSheet())
            self.assertIn(expected_primary, section.styleSheet())
            self.assertIn(expected_secondary, body.styleSheet())
            self.assertEqual(
                title.textColorRole(),
                fluentqt.Label.TextColorRole.Primary,
            )
            self.assertEqual(
                section.textColorRole(),
                fluentqt.Label.TextColorRole.Primary,
            )
            self.assertEqual(
                body.textColorRole(),
                fluentqt.Label.TextColorRole.Secondary,
            )

            page.onThemeUpdated()
            self.assertIn(expected_secondary, body.styleSheet())

            window.navigate("basic-input", animated=False)
            _qwait(50)
            _index, category_page = window._pages["basic-input"]
            subtitle = category_page.findChild(
                fluentqt.Label, "galleryContentSubtitle"
            )
            self.assertIsNotNone(subtitle)
            self.assertIn(expected_secondary, subtitle.styleSheet())
            self.assertEqual(
                subtitle.textColorRole(),
                fluentqt.Label.TextColorRole.Secondary,
            )
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_home_section_headers_follow_light_dark_and_high_contrast(self):
        window = GalleryWindow(startup_visuals=False)
        original_mode = int(window._settings.theme_mode)
        window.show()
        QApplication.processEvents()
        try:
            window._set_theme_mode(1)
            QApplication.processEvents()
            headers = (
                window.findChild(
                    fluentqt.Label, "galleryHomeFeaturedHeader"
                ),
                window.findChild(
                    fluentqt.Label, "galleryHomeCategoriesHeader"
                ),
            )
            self.assertTrue(all(header is not None for header in headers))
            light_color = css_color(gallery_colors().text_primary)
            for header in headers:
                self.assertEqual(
                    header.textColorRole(),
                    fluentqt.Label.TextColorRole.Primary,
                )
                self.assertIn(light_color, header.styleSheet())

            window._set_theme_mode(2)
            QApplication.processEvents()
            dark_color = css_color(gallery_colors().text_primary)
            self.assertNotEqual(light_color, dark_color)
            for header in headers:
                self.assertIn(dark_color, header.styleSheet())
                self.assertNotIn(light_color, header.styleSheet())

            window._set_theme_mode(3)
            QApplication.processEvents()
            self.assertEqual(
                fluentqt.current_theme(),
                fluentqt.Theme.HighContrast,
            )
            self.assertEqual(
                QColor(_theme_tokens()["bgCanvas"]),
                QColor("#000000"),
            )
            self.assertEqual(
                QColor(_theme_tokens()["strokeFocusOuter"]),
                QColor("#1aebff"),
            )
        finally:
            window._set_theme_mode(original_mode)
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_global_theme_switch_refreshes_scroll_view_viewport_palette(self):
        window = GalleryWindow(startup_visuals=False)
        original_mode = int(window._settings.theme_mode)
        window.show()
        window.navigate("annotated-scrollbar", animated=False)
        QApplication.processEvents()
        try:
            _index, page = window._pages["annotated-scrollbar"]
            card = next(
                card
                for card in page._gallery_sample_cards
                if card.property("gallerySampleId")
                == "annotated-scrollbar-scrollview"
            )
            scroll = card._gallery_preview_widget.findChild(
                fluentqt.ScrollView
            )
            self.assertIsNotNone(scroll)
            self.assertEqual(
                scroll.contentWidget().width(),
                scroll.viewport().width(),
            )

            target_mode = (
                1
                if fluentqt.theme_uses_dark_appearance(
                    fluentqt.current_theme()
                )
                else 2
            )
            window._set_theme_mode(target_mode)
            QApplication.processEvents()

            expected = QColor(_theme_tokens()["bgCanvas"])
            palette = scroll.viewport().palette()
            self.assertEqual(
                palette.color(QPalette.ColorRole.Window), expected
            )
            self.assertEqual(
                palette.color(QPalette.ColorRole.Base), expected
            )
        finally:
            window._set_theme_mode(original_mode)
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_component_preview_theme_follows_cpp_override_contract(self):
        original_theme = fluentqt.current_theme()
        window = GalleryWindow()
        window.show()
        window.navigate("button", animated=False)
        QApplication.processEvents()
        try:
            _index, page = window._pages["button"]
            self.assertFalse(page._gallery_sample_theme_explicit)
            target = (
                fluentqt.Theme.Light
                if fluentqt.theme_uses_dark_appearance(
                    fluentqt.current_theme()
                )
                else fluentqt.Theme.Dark
            )
            window._set_theme_mode(2 if target == fluentqt.Theme.Dark else 1)
            QApplication.processEvents()
            self.assertFalse(page._gallery_sample_theme_explicit)
            self.assertEqual(page._gallery_sample_theme, target)
            self.assertEqual(
                page._gallery_theme_button.property("gallerySampleTheme"),
                "Dark" if target == fluentqt.Theme.Dark else "Light",
            )

            page._gallery_theme_button.click()
            QApplication.processEvents()
            explicit_theme = page._gallery_sample_theme
            self.assertTrue(page._gallery_sample_theme_explicit)
            window._set_theme_mode(2 if target == fluentqt.Theme.Dark else 1)
            QApplication.processEvents()
            self.assertEqual(page._gallery_sample_theme, explicit_theme)
            for card in page._gallery_sample_cards:
                self.assertEqual(
                    card._gallery_preview_surface.property(
                        "fluentThemeOverride"
                    ),
                    int(explicit_theme),
                )
        finally:
            fluentqt.set_theme(original_theme)
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_component_preview_labels_high_contrast_before_local_toggle(self):
        window = GalleryWindow()
        original_mode = int(window._settings.theme_mode)
        window.show()
        window.navigate("button", animated=False)
        QApplication.processEvents()
        try:
            _index, page = window._pages["button"]
            window._set_theme_mode(3)
            QApplication.processEvents()

            self.assertFalse(page._gallery_sample_theme_explicit)
            self.assertEqual(
                page._gallery_sample_theme,
                fluentqt.Theme.HighContrast,
            )
            self.assertEqual(
                page._gallery_theme_button.property("gallerySampleTheme"),
                "High contrast",
            )

            page._gallery_theme_button.click()
            QApplication.processEvents()
            self.assertTrue(page._gallery_sample_theme_explicit)
            self.assertEqual(page._gallery_sample_theme, fluentqt.Theme.Light)
        finally:
            window._set_theme_mode(original_mode)
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_source_widget_roots_are_constructed_with_preview_parent(self):
        preview = QWidget()
        result = build_sample(
            "tab-view",
            "tab-view-hosted-pages",
            preview,
        )
        try:
            self.assertIs(result.widget.parentWidget(), preview)
            self.assertIn(
                "root = QWidget(globals().get('gallery_parent'))",
                result.preview_source,
            )
            self.assertNotIn("gallery_parent", result.source)
        finally:
            result.widget.close()
            result.widget.deleteLater()
            preview.close()
            preview.deleteLater()

    def test_window_samples_use_compositor_safe_activation(self):
        expected_fragments = {
            "window-content-host": (
                "window.setCustomWindowChromeEnabled(True)",
                "window.setBackdropEffect(fluentqt.BackdropEffect.Mica)",
                "content.setAutoFillBackground(False)",
            ),
            "window-custom-titlebar": (
                "window.setCustomWindowChromeEnabled(True)",
                "window.setBackdropEffect(fluentqt.BackdropEffect.Mica)",
            ),
        }
        for sample_id, fragments in expected_fragments.items():
            with self.subTest(sample=sample_id):
                result = build_sample("window", sample_id)
                try:
                    self.assertIn(
                        "window.requestForegroundActivation()",
                        result.preview_source,
                    )
                    self.assertNotIn(
                        "window.activateWindow()",
                        result.preview_source,
                    )
                    for source in (result.preview_source, result.source):
                        for fragment in fragments:
                            self.assertIn(fragment, source)
                finally:
                    result.widget.close()
                    result.widget.deleteLater()

    def test_value_member_parity_requires_python_operations(self):
        self.assertEqual(
            _python_language_member_equivalents(ast.parse("match = token.casefold() == email.casefold()")),
            {"compare"},
        )
        self.assertEqual(
            _python_language_member_equivalents(ast.parse("operation = token.casefold\nnote = 'compare()'")),
            set(),
        )
        self.assertEqual(
            _python_language_member_equivalents(ast.parse(
                'self.editors = {}\nself.editors[key] = editor\n'
                'value = editor.text().strip()\n'
            )),
            {"insert", "trimmed"},
        )
        self.assertEqual(
            _python_language_member_equivalents(ast.parse(
                'self.editors = {}\nvalue = self.editors[key]\n'
                'items = []\nitems[key] = editor\n'
                'operation = value.strip\nvalue = value.strip("x")\n'
                'note = "insert() trimmed() strip()"\n'
            )),
            set(),
        )
        self.assertEqual(
            _python_language_member_equivalents(
                ast.parse("accepted = suffix.lower() in extensions")
            ),
            {"toLower", "contains"},
        )
        self.assertEqual(
            _python_language_member_equivalents(
                ast.parse("rejected = suffix not in extensions")
            ),
            {"contains"},
        )
        self.assertEqual(
            _python_language_member_equivalents(ast.parse(
                "operation = suffix.lower\n"
                "note = 'toLower() contains() lower() in extensions'\n"
                "for extension in extensions:\n"
                "    pass\n"
            )),
            set(),
        )
        self.assertEqual(
            _python_language_member_equivalents(ast.parse(
                'self.error = ""\ndark = bool(theme.property("dark"))\n'
            )),
            {"clear", "toBool"},
        )
        self.assertEqual(
            _python_language_member_equivalents(ast.parse(
                'saved = text.split("Next:")[0]\n'
                'markdown = name.endswith(".md")\nbody = "notes" * 40\n'
            )),
            {"section", "first", "endsWith", "repeated"},
        )
        self.assertEqual(
            _python_language_member_equivalents(ast.parse(
                'parts = text.split("Next:")\noperation = name.endswith\n'
                'count = 3 * 40\nnote = "section endsWith repeated"\n'
            )),
            set(),
        )

    def test_preview_source_executes_and_displayed_source_stays_concise(self):
        app = QApplication.instance()
        self.assertIsNotNone(app)
        displayed_statement_count = 0
        cpp_line_count = 0
        maximum_displayed_lines = 0
        maximum_displayed_width = 0
        for entry in ENTRIES:
            for sample in entry.samples:
                with self.subTest(route=entry.route_id, sample=sample.id):
                    # Bridge host code and its QML page are separate source files.
                    # Compare the host API here; the focused bridge gate also
                    # executes the complete file bundle and checks its QML bytes.
                    cpp_source = sample.cpp_snippet
                    cpp_api_source = cpp_source
                    if entry.route_id == "code-block":
                        # Code shown inside the block is data, not an executed
                        # API call. Compare the outer CodeBlock setup only.
                        cpp_api_source = re.sub(r'"(?:\\.|[^"\\])*"', '""', cpp_api_source)
                    if sample.id == "command-palette-shared-actions":
                        # Compare the workspace below the icon painter helper.
                        # The copyable Python bundle renders icons with public FontIcon.
                        cpp_api_source = cpp_source.split("auto* layout =", 1)[1]
                    if entry.route_id == "qml-view":
                        cpp_api_source = cpp_source.split("QWidget* makePanel(", 1)[1]
                        cpp_api_source = cpp_api_source.split("\nint main(", 1)[0]
                        # source.resolved(QUrl("Missing.qml")) and Python's
                        # qml_source("Missing") both select the sibling file.
                        # The bridge source gate executes both error paths.
                        cpp_api_source = cpp_api_source.replace(
                            'source.resolved(QUrl("Missing.qml"))', 'qml_source("Missing")'
                        )
                    result = None
                    namespace: dict[str, object] = {
                        "__name__": "fluentqt_gallery_snippet"
                    }
                    try:
                        result = build_sample(entry.route_id, sample.id)
                        self.assertEqual(result.route_id, entry.route_id)
                        self.assertEqual(result.sample_id, sample.id)
                        self.assertEqual(result.parity_level, "native-equivalent")
                        self.assertIn(entry.name, result.covered_types)
                        self.assertTrue(result.source.strip())
                        self.assertTrue(result.preview_source.strip())
                        displayed_lines = len(result.source.splitlines())
                        displayed_tree = ast.parse(result.source)
                        cpp_has_layout = bool(
                            re.search(
                                r"\bQ(?:Form|Grid|HBox|Stacked|VBox)Layout\b",
                                cpp_source,
                            )
                        )
                        cpp_has_widget = bool(
                            re.search(r"\bQWidget\b", cpp_source)
                        )
                        leaked_preview_scaffolding: set[str] = set()
                        for call in (
                            node
                            for node in ast.walk(displayed_tree)
                            if isinstance(node, ast.Call)
                        ):
                            if isinstance(call.func, ast.Attribute):
                                call_name = call.func.attr
                            elif isinstance(call.func, ast.Name):
                                call_name = call.func.id
                            else:
                                continue
                            if (
                                call_name in DISPLAY_PREVIEW_LAYOUT_METHODS
                                and not re.search(
                                    r"(?:->|\.){0}\s*\(".format(
                                        re.escape(call_name)
                                    ),
                                    cpp_source,
                                )
                            ):
                                leaked_preview_scaffolding.add(call_name)
                            elif (
                                call_name in DISPLAY_PREVIEW_LAYOUT_TYPES
                                and not cpp_has_layout
                            ):
                                leaked_preview_scaffolding.add(call_name)
                            elif call_name == "QWidget" and not cpp_has_widget:
                                leaked_preview_scaffolding.add(call_name)
                        self.assertEqual(
                            leaked_preview_scaffolding,
                            set(),
                            "{0}/{1} exposes Gallery preview layout that the "
                            "canonical C++ teaching snippet omits".format(
                                entry.route_id,
                                sample.id,
                            ),
                        )
                        allowed_helpers = SELF_CONTAINED_DISPLAY_HELPERS.get(
                            (entry.route_id, sample.id), frozenset()
                        )
                        helper_definitions = {
                            node.name: node
                            for node in displayed_tree.body
                            if isinstance(node, ast.FunctionDef)
                            and node.name in allowed_helpers
                        }
                        self.assertEqual(
                            set(helper_definitions),
                            set(allowed_helpers),
                            "{0}/{1} lost its self-contained rendering helper"
                            .format(entry.route_id, sample.id),
                        )
                        helper_statement_ids = {
                            id(node)
                            for helper in helper_definitions.values()
                            for node in ast.walk(helper)
                            if isinstance(node, ast.stmt)
                        }
                        displayed_semantic_statements = sum(
                            isinstance(node, ast.stmt)
                            and not isinstance(
                                node, (ast.Import, ast.ImportFrom)
                            )
                            and id(node) not in helper_statement_ids
                            for node in ast.walk(displayed_tree)
                        )
                        cpp_semantic_statements = max(
                            1, cpp_source.count(";")
                        )
                        self.assertLessEqual(
                            displayed_semantic_statements,
                            max(
                                cpp_semantic_statements + 4,
                                int(cpp_semantic_statements * 1.6),
                            ),
                            "{0}/{1} teaches substantially more Python "
                            "operations than its canonical C++ card".format(
                                entry.route_id,
                                sample.id,
                            ),
                        )
                        top_level_loops = [
                            node
                            for node in displayed_tree.body
                            if isinstance(node, (ast.For, ast.AsyncFor))
                        ]
                        if not re.search(r"\bfor\s*\(", cpp_source):
                            self.assertEqual(
                                top_level_loops,
                                [],
                                "{0}/{1} hides fixed component construction "
                                "behind a Python loop although the canonical "
                                "C++ example is explicit".format(
                                    entry.route_id,
                                    sample.id,
                                ),
                            )
                            for loop in (
                                node
                                for node in ast.walk(displayed_tree)
                                if isinstance(node, (ast.For, ast.AsyncFor))
                            ):
                                fluent_constructors = [
                                    call
                                    for call in ast.walk(loop)
                                    if isinstance(call, ast.Call)
                                    and isinstance(call.func, ast.Attribute)
                                    and isinstance(call.func.value, ast.Name)
                                    and call.func.value.id == "fluentqt"
                                    and call.func.attr[:1].isupper()
                                ]
                                self.assertEqual(
                                    fluent_constructors,
                                    [],
                                    "{0}/{1} constructs fixed FluentQt "
                                    "controls inside a hidden loop".format(
                                        entry.route_id,
                                        sample.id,
                                    ),
                                )
                        displayed_imports = [
                            node
                            for node in displayed_tree.body
                            if isinstance(node, (ast.Import, ast.ImportFrom))
                        ]
                        if displayed_imports:
                            self.assertIs(
                                displayed_tree.body[0],
                                displayed_imports[0],
                                "{0}/{1} must start with its required "
                                "PySide6 imports".format(
                                    entry.route_id, sample.id
                                ),
                            )
                        loaded_names = {
                            node.id
                            for node in ast.walk(displayed_tree)
                            if isinstance(node, ast.Name)
                            and isinstance(node.ctx, ast.Load)
                        }
                        for imported in displayed_imports:
                            for alias in imported.names:
                                self.assertNotEqual(
                                    alias.name,
                                    "fluentqt",
                                    "{0}/{1} repeats the page-level "
                                    "import fluentqt declaration".format(
                                        entry.route_id, sample.id
                                    ),
                                )
                                if alias.name == "*":
                                    continue
                                local_name = alias.asname or (
                                    alias.name
                                    if isinstance(imported, ast.ImportFrom)
                                    else alias.name.split(".", 1)[0]
                                )
                                self.assertIn(
                                    local_name,
                                    loaded_names,
                                    "{0}/{1} keeps unused import {2}".format(
                                        entry.route_id,
                                        sample.id,
                                        local_name,
                                    ),
                                )
                        cpp_wrapped_calls = _cpp_wrapped_call_names(
                            cpp_source
                        )
                        for statement in ast.walk(displayed_tree):
                            if not isinstance(
                                statement,
                                (ast.AnnAssign, ast.Assign, ast.Expr, ast.Return),
                            ):
                                continue
                            if statement.end_lineno is None or (
                                statement.lineno == statement.end_lineno
                            ):
                                continue
                            if isinstance(statement, ast.Expr):
                                value = statement.value
                            elif isinstance(
                                statement, (ast.Assign, ast.AnnAssign)
                            ):
                                value = statement.value
                            else:
                                value = statement.value
                            if not isinstance(value, ast.Call):
                                continue
                            rendered = ast.unparse(statement)
                            if isinstance(value.func, ast.Attribute):
                                call_name = value.func.attr
                            elif isinstance(value.func, ast.Name):
                                call_name = value.func.id
                            else:
                                call_name = ""
                            rendered_length = (
                                statement.col_offset + len(rendered)
                            )
                            aligns_with_cpp = (
                                call_name in cpp_wrapped_calls
                                and rendered_length
                                >= _CPP_ALIGNMENT_MIN_CALL_LENGTH
                            )
                            self.assertTrue(
                                "\n" in rendered
                                or rendered_length
                                > _DISPLAY_PREFERRED_LINE_LENGTH
                                or aligns_with_cpp,
                                "{0}/{1} needlessly wraps a short call: {2}"
                                .format(
                                    entry.route_id,
                                    sample.id,
                                    rendered,
                                ),
                            )
                        displayed_statement_count += sum(
                            isinstance(node, ast.stmt)
                            for node in ast.walk(displayed_tree)
                        )
                        cpp_line_count += len(cpp_source.splitlines())
                        maximum_displayed_lines = max(
                            maximum_displayed_lines, displayed_lines
                        )
                        displayed_width = max(
                            map(len, result.source.splitlines()),
                            default=0,
                        )
                        maximum_displayed_width = max(
                            maximum_displayed_width,
                            displayed_width,
                        )
                        self.assertLessEqual(
                            displayed_width,
                            _DISPLAY_HARD_LINE_LENGTH,
                            "{0}/{1} displayed Python contains an "
                            "overlong line".format(
                                entry.route_id,
                                sample.id,
                            ),
                        )
                        for line in result.source.splitlines():
                            if len(line) <= _DISPLAY_PREFERRED_LINE_LENGTH:
                                continue
                            expression = line.strip().removesuffix(",")
                            try:
                                parsed_expression = ast.parse(
                                    expression,
                                    mode="eval",
                                ).body
                            except SyntaxError:
                                self.fail(
                                    "{0}/{1} exceeds the preferred line "
                                    "length at a breakable boundary: {2}"
                                    .format(
                                        entry.route_id,
                                        sample.id,
                                        line,
                                    )
                                )
                            self.assertIsInstance(
                                parsed_expression,
                                (ast.Constant, ast.JoinedStr),
                                "{0}/{1} uses the hard line ceiling for a "
                                "breakable expression: {2}".format(
                                    entry.route_id,
                                    sample.id,
                                    line,
                                ),
                            )
                        self.assertIsNone(
                            re.search(
                                r"['\"](?:\\u[eEfF][0-9A-Fa-f]{3}|"
                                "[\uE000-\uF8FF])['\"]",
                                result.source,
                            ),
                            "{0}/{1} exposes a private-use icon codepoint "
                            "instead of a semantic Fluent icon constant"
                            .format(entry.route_id, sample.id),
                        )
                        self.assertIsNone(
                            SEMANTIC_ICON_CATALOG_PATTERN.search(result.source),
                            "{0}/{1} uses a raw catalog name that has a public "
                            "semantic Fluent icon constant".format(
                                entry.route_id, sample.id
                            ),
                        )
                        # Extension samples display a short integration excerpt and
                        # a separately selectable Full example, like the C++ Gallery.
                        # Check the full example for API and design-token parity.
                        contract_source = (result.preview_source
                                           if entry.category_id in {"spatial", "qml-bridge"}
                                           else result.source)
                        if sample.id in {"tree-select-project-teams", "tag-input-project-members", "auto-suggest-box-async", "field-connection-form", "list-detail-file-browser", "data-grid-tool-bar-basic", "command-palette-shared-actions", "popup-modal-dim",
                                         "dialog-backdrop-effects", "drawer-view-backdrop-effects",
                                         "property-panel-chart-workbench"}:
                            contract_source = result.preview_source
                        if entry.route_id == "qml-view":
                            from fluentqt.qmlbridge import QmlView
                            from fluentqt_gallery.native_samples_qmlbridge import (
                                example_source_files, full_example_source,
                            )
                            contract_source = full_example_source(sample.id)
                        contract_tree = ast.parse(contract_source)
                        if entry.route_id == "qml-view":
                            files = example_source_files(sample.id)
                            host = next(file for file in files if file["name"] == "main.py")
                            qml = next(file for file in files if file["language"] == "qml")
                            self.assertEqual(host["code"], contract_source)
                            self.assertNotIn("import QtQuick", host["code"])
                            self.assertNotIn("_QML", host["code"])
                            self.assertFalse(any(
                                isinstance(node, ast.Call)
                                and isinstance(node.func, ast.Attribute)
                                and node.func.attr == "write_text"
                                for node in ast.walk(contract_tree)
                            ), "The Python host must load its separate QML page")
                            views = result.widget.findChildren(QmlView)
                            if isinstance(result.widget, QmlView):
                                views.insert(0, result.widget)
                            self.assertEqual(len(views), 1)
                            loaded_file = Path(views[0].source().toLocalFile())
                            self.assertEqual(
                                qml["code"].encode("utf-8"), loaded_file.read_bytes(),
                                "The copyable QML must be the asset used by this preview",
                            )
                        expected_design_tokens = {
                            "fluentqt.Typography.Icons.{0}".format(name)
                            for name in re.findall(
                                r"Typography::Icons::([A-Za-z0-9_]+)\b(?!\s*\()",
                                cpp_source,
                            )
                        }
                        expected_design_tokens.update(
                            "fluentqt.Typography.IconSize.{0}".format(name)
                            for name in re.findall(
                                r"Typography::IconSize::([A-Za-z0-9_]+)",
                                cpp_source,
                            )
                        )
                        expected_design_tokens.update(
                            "fluentqt.FontRole.{0}".format(name)
                            for name in re.findall(
                                r"Typography::FontRole::([A-Za-z0-9_]+)",
                                cpp_source,
                            )
                        )
                        expected_design_tokens.update(
                            "fluentqt.Spacing.{0}".format(
                                name.replace("::", ".")
                            )
                            for name in re.findall(
                                r"Spacing::((?:[A-Za-z0-9_]+::)*"
                                r"[A-Za-z0-9_]+)",
                                cpp_source,
                            )
                        )
                        expected_design_tokens.update(
                            "fluentqt.CornerRadius.{0}".format(
                                "None_" if name == "None" else name
                            )
                            for name in re.findall(
                                r"CornerRadius::([A-Za-z0-9_]+)",
                                cpp_source,
                            )
                        )
                        for design_token in expected_design_tokens:
                            self.assertIn(
                                design_token,
                                contract_source,
                                "{0}/{1} replaces canonical C++ design "
                                "token {2} with a hard-coded value or a "
                                "different alias".format(
                                    entry.route_id,
                                    sample.id,
                                    design_token,
                                ),
                            )
                        for cxx_token in (
                            "#include",
                            "QStringLiteral",
                            "fluent::",
                        ):
                            self.assertNotIn(cxx_token, result.source)
                        gallery_imports = tuple(
                            line
                            for line in result.source.splitlines()
                            if line.startswith("from fluentqt_gallery")
                        )
                        self.assertEqual(
                            gallery_imports,
                            (
                                "from fluentqt_gallery.metrics import "
                                "drawer_title_bar_avoidance_margins",
                            )
                            if (
                                entry.route_id,
                                sample.id,
                            )
                            == (
                                "drawer-view",
                                "drawer-view-close-policy",
                            )
                            else (),
                        )
                        self.assertNotIn("gallery_parent", result.source)
                        self.assertTrue(
                            result.source_driven,
                            "{0}/{1} preview was not built from its exact "
                            "preview source".format(entry.route_id, sample.id),
                        )
                        preview_namespace = getattr(
                            result.widget,
                            "_fluentqt_gallery_source_namespace",
                            None,
                        )
                        self.assertIsInstance(preview_namespace, dict)
                        self.assertTrue(
                            any(
                                value is result.widget
                                for value in preview_namespace.values()
                            )
                        )
                        compile(
                            result.source,
                            "<fluentqt-gallery-display-{0}-{1}>".format(
                                entry.route_id, sample.id
                            ),
                            "exec",
                        )
                        cpp_methods = set(
                            re.findall(
                                r"(?:->|\.)([A-Za-z_][A-Za-z0-9_]*)\s*\(",
                                cpp_api_source,
                            )
                        )
                        python_members = {
                            node.attr
                            for node in ast.walk(contract_tree)
                            if isinstance(node, ast.Attribute)
                        }
                        python_members.update(
                            _python_language_member_equivalents(contract_tree)
                        )
                        for member, aliases in (
                            _CPP_DISPLAY_MEMBER_ALIASES.items()
                        ):
                            if any(
                                alias in python_members
                                for alias in aliases
                            ):
                                python_members.add(member)
                        missing_cpp_methods = {
                            member
                            for member in cpp_methods
                            if member not in CPP_PYTHON_MEMBER_EQUIVALENTS
                            and member not in python_members
                            and _snake_case(member) not in python_members
                        }
                        if entry.route_id == "tooltip":
                            missing_cpp_methods.discard("setText")
                            self.assertIn(
                                "fluentqt.ToolTip.attach(",
                                result.source,
                            )
                        self.assertEqual(
                            missing_cpp_methods,
                            set(),
                            "{0}/{1} drops canonical C++ member calls: {2}"
                            .format(
                                entry.route_id,
                                sample.id,
                                sorted(missing_cpp_methods),
                            ),
                        )

                        cpp_connection_count = len(
                            re.findall(
                                r"(?<![A-Za-z_:])(?:QObject::)?"
                                r"connect\s*\(",
                                cpp_source,
                            )
                        )
                        python_connection_count = sum(
                            isinstance(node, ast.Call)
                            and isinstance(node.func, ast.Attribute)
                            and node.func.attr == "connect"
                            for node in ast.walk(contract_tree)
                        )
                        self.assertGreaterEqual(
                            python_connection_count,
                            cpp_connection_count,
                            "{0}/{1} drops a canonical C++ signal "
                            "connection".format(
                                entry.route_id, sample.id
                            ),
                        )

                        cpp_constructor_types = re.findall(
                            r"\bnew\s+([A-Z][A-Za-z0-9_]*)\b",
                            cpp_api_source,
                        )
                        for type_name in set(cpp_constructor_types):
                            if not hasattr(fluentqt, type_name):
                                continue
                            if (
                                entry.route_id == "tooltip"
                                and type_name == "ToolTip"
                            ):
                                continue
                            cpp_constructor_count = (
                                cpp_constructor_types.count(type_name)
                            )
                            python_constructor_count = sum(
                                isinstance(node, ast.Call)
                                and isinstance(node.func, ast.Attribute)
                                and isinstance(node.func.value, ast.Name)
                                and node.func.value.id == "fluentqt"
                                and node.func.attr == type_name
                                for node in ast.walk(contract_tree)
                            )
                            self.assertGreaterEqual(
                                python_constructor_count,
                                cpp_constructor_count,
                                "{0}/{1} omits {2} construction shown by "
                                "the canonical C++ card".format(
                                    entry.route_id,
                                    sample.id,
                                    type_name,
                                ),
                            )

                        python_text_literals = {
                            node.value
                            for node in ast.walk(contract_tree)
                            if isinstance(node, ast.Constant)
                            and isinstance(node.value, str)
                            and node.value
                        }
                        for raw_literal in re.findall(
                            r'"((?:\\.|[^"\\])*)"',
                            cpp_api_source,
                        ):
                            try:
                                cpp_literal = ast.literal_eval(
                                    '"{0}"'.format(raw_literal)
                                )
                            except (SyntaxError, ValueError):
                                cpp_literal = raw_literal
                            if (
                                not cpp_literal
                                or len(cpp_literal) <= 1
                                or cpp_literal.startswith((":/", "qrc:/"))
                                or cpp_literal.endswith(".h")
                                or "%" in cpp_literal
                            ):
                                continue
                            self.assertTrue(
                                any(
                                    cpp_literal in python_literal
                                    or python_literal in cpp_literal
                                    for python_literal in python_text_literals
                                ),
                                "{0}/{1} omits canonical visible text {2!r}"
                                .format(
                                    entry.route_id,
                                    sample.id,
                                    cpp_literal,
                                ),
                            )

                        for helper_name in FORBIDDEN_DISPLAY_HELPERS:
                            self.assertNotIn(
                                helper_name,
                                result.source,
                                "{0}/{1} exposes private Gallery helper {2}"
                                .format(
                                    entry.route_id,
                                    sample.id,
                                    helper_name,
                                ),
                            )
                        preview_code = compile(
                            result.preview_source,
                            "<fluentqt-gallery-preview-{0}-{1}>".format(
                                entry.route_id, sample.id
                            ),
                            "exec",
                        )
                        exec(preview_code, namespace)
                        source_roots = {
                            id(value): value
                            for value in namespace.values()
                            if (
                                isinstance(value, QWidget)
                                and shiboken6.isValid(value)
                                and value.parent() is None
                                and value.graphicsProxyWidget() is None
                            )
                        }
                        self.assertEqual(
                            len(source_roots),
                            1,
                            "{0}/{1} exact preview source must create one "
                            "root".format(entry.route_id, sample.id),
                        )
                        self.assertEqual(
                            type(next(iter(source_roots.values()))).__name__,
                            type(result.widget).__name__,
                        )
                    finally:
                        source_widgets = _take_top_level_widgets(namespace)
                        result_namespace: dict[str, object] = {}
                        if result is not None:
                            if shiboken6.isValid(result.widget):
                                result.widget.close()
                        source_widget = None
                        for source_widget in source_widgets:
                            if shiboken6.isValid(source_widget):
                                source_widget.close()
                        app.processEvents()
                        if result is not None and shiboken6.isValid(
                            result.widget
                        ):
                            result_namespace = _detach_sample_namespace(
                                result.widget
                            )
                        result = None
                        source_widgets = ()
                        source_widget = None
                        result_namespace.clear()
                        namespace.clear()
                        app.processEvents()
                        self.assertIs(
                            QApplication.instance(),
                            app,
                            "QApplication lifetime changed after {0}/{1}".format(
                                entry.route_id, sample.id
                            ),
                        )
        self.assertLessEqual(
            displayed_statement_count,
            int(cpp_line_count * 1.05),
            "Python teaching snippets contain more semantic operations than "
            "the canonical C++ examples",
        )
        self.assertLessEqual(maximum_displayed_lines, 80)
        self.assertLessEqual(maximum_displayed_width, 88)

    def test_list_view_display_sources_execute_with_preview_surface_semantics(self):
        for sample_id, row_count, selected_rows in (
            ("list-view-basic", 12, 1),
            ("list-view-multi-select", 9, 2),
        ):
            with self.subTest(sample=sample_id):
                result = build_sample("list-view", sample_id)
                namespace: dict[str, object] = {
                    "__name__": "fluentqt_gallery_display_snippet",
                    "fluentqt": fluentqt,
                }
                source_widgets: tuple[QWidget, ...] = ()
                try:
                    exec(
                        compile(
                            result.source,
                            "<fluentqt-gallery-display-list-view>",
                            "exec",
                        ),
                        namespace,
                    )
                    list_view = namespace.get("list_view")
                    self.assertIsInstance(list_view, fluentqt.ListView)
                    self.assertEqual(list_view.size(), QSize(320, 234))
                    self.assertFalse(list_view.isBackgroundVisible())
                    self.assertFalse(list_view.isBorderVisible())
                    self.assertTrue(
                        list_view.property(
                            "fluentPreserveParentSurface"
                        )
                    )
                    self.assertTrue(
                        list_view.viewport().property(
                            "fluentPreserveParentSurface"
                        )
                    )
                    self.assertEqual(list_view.model().rowCount(), row_count)
                    self.assertEqual(
                        len(list_view.selectionModel().selectedRows()),
                        selected_rows,
                    )
                    decoration = list_view.model().index(0, 0).data(
                        Qt.ItemDataRole.DecorationRole
                    )
                    self.assertIsInstance(decoration, QPixmap)
                    self.assertFalse(decoration.isNull())
                finally:
                    source_widgets = _take_top_level_widgets(namespace)
                    if shiboken6.isValid(result.widget):
                        result.widget.close()
                    for source_widget in source_widgets:
                        if shiboken6.isValid(source_widget):
                            source_widget.close()
                    namespace.clear()
                    QApplication.processEvents()

    def test_window_builds_all_available_routes_and_sample_cards(self):
        window = GalleryWindow()
        window.show()
        QApplication.processEvents()
        try:
            self.assertEqual(window.all_route_ids(), tuple(route.id for route in ROUTES))
            self.assertEqual(window.visit_all_routes(), [])
            self.assertEqual(len(window._pages), len(ROUTES))
            built_sample_count = 0
            for entry in ENTRIES:
                _index, page = window._pages[entry.route_id]
                results = page._gallery_sample_results
                self.assertEqual(
                    tuple(result.sample_id for result in results),
                    tuple(sample.id for sample in entry.samples),
                )
                self.assertTrue(
                    all(result.parity_level == "native-equivalent" for result in results)
                )
                self.assertTrue(all(result.source_driven for result in results))
                unsafe_labels = []
                for result in results:
                    labels = list(
                        result.widget.findChildren(fluentqt.Label)
                    )
                    if isinstance(result.widget, fluentqt.Label):
                        labels.insert(0, result.widget)
                    for label in labels:
                        has_own_color = re.search(
                            r"(?:^|[;{\n])\s*color\s*:",
                            label.styleSheet(),
                            flags=re.IGNORECASE,
                        )
                        if (
                            label.textColorRole()
                            == fluentqt.Label.TextColorRole.Default
                            and has_own_color is None
                        ):
                            unsafe_labels.append(label.text())
                self.assertEqual(
                    unsafe_labels,
                    [],
                    "{0} keeps palette-only labels below the styled Gallery "
                    "sample surface".format(entry.route_id),
                )
                built_sample_count += len(results)
            self.assertEqual(built_sample_count, len(SAMPLE_BY_KEY))
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_all_built_routes_survive_theme_refresh_cycles(self):
        window = GalleryWindow(startup_visuals=False)
        original_theme = int(window._settings.theme_mode)
        window.show()
        QApplication.processEvents()
        try:
            self.assertEqual(window.visit_all_routes(), [])
            window.navigate("settings", record_history=False)
            QApplication.processEvents()
            button_generation = window._page_visual_generations["button"]
            window._set_theme_mode(2 if original_theme != 2 else 1)
            self.assertGreater(window._visual_generation, button_generation)
            self.assertEqual(
                window._page_visual_generations["button"],
                button_generation,
                "hidden pages must not be synchronously walked on a style switch",
            )
            window.navigate("button", record_history=False)
            QApplication.processEvents()
            self.assertEqual(
                window._page_visual_generations["button"],
                window._visual_generation,
            )
            window.navigate("settings", record_history=False)
            for theme in (1, 2, 0):
                window._set_theme_mode(theme)
                QApplication.processEvents()

            _index, iconography = window._pages["foundation-iconography"]
            browser = iconography.findChild(
                GalleryIconBrowser, "galleryIconBrowser"
            )
            self.assertIsNotNone(browser)
            self.assertTrue(shiboken6.isValid(browser.grid._hover_tip))
            self.assertFalse(
                hasattr(browser.grid._hover_tip, "onThemeUpdated")
            )
        finally:
            window._set_theme_mode(original_theme)
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_window_shell_matches_native_gallery_geometry(self):
        window = GalleryWindow()
        window.show()
        QApplication.processEvents()
        try:
            launch_effect = window._pages["home"][1]._gallery_hero._particles.effect()
            self.assertIn(launch_effect, (
                fluentqt.ParticleBackdrop.Effect.FlowingRibbons,
                fluentqt.ParticleBackdrop.Effect.FloatingDots,
                fluentqt.ParticleBackdrop.Effect.Starfield,
            ))
            self.assertEqual(window.windowTitle(), "Fluent-Qt Gallery")
            self.assertEqual(window.titleBar().titleBarHeight(), 42)
            self.assertEqual(window._search.objectName(), "GalleryTitleBar.SearchBox")
            self.assertEqual(window._search.height(), 28)
            self.assertEqual(window._search.maximumWidth(), 360)
            self.assertEqual(
                window._search.x(),
                (window.titleBar().width() - window._search.width()) // 2,
            )
            self.assertEqual(
                window._search.placeholderText(),
                "Search components and examples...",
            )
            self.assertEqual(window._navigation_view.expandedPaneWidth(), 260)
            self.assertEqual(window._navigation_view.compactPaneWidth(), 48)
            self.assertEqual(
                window._navigation_view.compactModeThresholdWidth(), 640
            )
            self.assertEqual(
                window._navigation_view.expandedModeThresholdWidth(), 1008
            )
            trees = window.findChildren(fluentqt.TreeView)
            self.assertEqual(len(trees), 2)
            self.assertEqual(
                {tree.objectName() for tree in trees},
                {
                    "galleryMainNavigationTreeView",
                    "galleryFooterNavigationTreeView",
                },
            )
            main_tree = window.findChild(
                fluentqt.TreeView, "galleryMainNavigationTreeView"
            )
            window.navigate("foundation-typography", animated=False)
            _qwait(250)
            self.assertEqual(main_tree.selectionIndicatorInset(), 36.0)
            self.assertEqual(main_tree.selectedIndicatorRect().x(), 36.0)
            window.navigate("home", animated=False)
            _qwait(250)
            self.assertEqual(main_tree.selectionIndicatorInset(), 7.0)
            self.assertEqual(main_tree.selectedIndicatorRect().x(), 7.0)

            _index, home = window._pages["home"]
            self.assertEqual(home._gallery_hero._particles.effect(), launch_effect)
            home._gallery_hero.refresh_theme()
            self.assertEqual(home._gallery_hero._particles.effect(), launch_effect)
            recreated_hero = GalleryHomeHero(home)
            self.assertEqual(recreated_hero._particles.effect(), launch_effect)
            self.assertEqual(home._gallery_hero.height(), 390)
            self.assertEqual(
                home._gallery_hero._tagline.text(),
                "Interactive documentation for FluentQt: browse components, "
                "run live examples, and inspect focused API usage.",
            )
            self.assertEqual(len(home._gallery_featured_grid.cards), 9)
            self.assertEqual(home._gallery_featured_grid.columns, 3)
            root_categories = sum(not category.plugin for category in CATEGORIES)
            plugin_root = int(any(category.plugin for category in CATEGORIES))
            self.assertEqual(
                len(home._gallery_category_grid.cards), root_categories + plugin_root + 1
            )
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_home_particle_effect_settings_normalize_legacy_enum_names(self):
        self.addCleanup(fluentqt.set_motion_mode, fluentqt.current_motion_mode())
        self.addCleanup(fluentqt.set_theme, fluentqt.current_theme())
        identifiers = ("FlowingRibbons", "FloatingDots", "Starfield")
        cases = tuple(
            (value, identifier)
            for identifier in identifiers
            for value in (
                identifier,
                identifier.encode("ascii"),
                repr(identifier.encode("ascii")),
                f'b"{identifier}"',
            )
        ) + (("retired-effect", ""), (b"\xff", ""), ("", ""))
        with TemporaryDirectory() as temporary_dir:
            path = Path(temporary_dir) / "config.ini"
            with (
                patch.object(
                    gallery_settings_module, "persistence_available", return_value=True
                ),
                patch.object(gallery_settings_module, "config_file_path", return_value=path),
            ):
                persisted = QSettings(str(path), QSettings.IniFormat)
                key = "home/lastParticleEffect"
                for saved, expected in cases:
                    with self.subTest(saved=saved):
                        persisted.setValue(key, saved)
                        persisted.sync()
                        settings = gallery_settings_module.GallerySettings()
                        try:
                            self.assertEqual(settings.last_home_particle_effect, expected)
                            persisted.sync()
                            if expected:
                                self.assertIsInstance(persisted.value(key), str)
                                self.assertEqual(persisted.value(key), expected)
                            # Also normalize direct writes of old enum names.
                            settings.set_last_home_particle_effect("FlowingRibbons")
                            settings.set_last_home_particle_effect("FloatingDots")
                            settings.set_last_home_particle_effect(saved)
                            self.assertEqual(settings.last_home_particle_effect, expected)
                            persisted.sync()
                            self.assertIsInstance(persisted.value(key), str)
                            self.assertEqual(persisted.value(key), expected)
                        finally:
                            settings._system_theme_poll.stop()
                            settings.deleteLater()
                            QApplication.processEvents()
        for invalid in (None, 42, "ParticleBackdrop.Effect.FlowingRibbons"):
            with self.subTest(invalid=invalid):
                self.assertEqual(gallery_settings_module._home_particle_effect_id(invalid), "")

    def test_home_particle_effect_does_not_repeat_across_launches(self):
        probe = r'''
import json
import sys
from pathlib import Path
from unittest.mock import patch
import fluentqt
fluentqt.prepare_high_dpi_application()
from PySide6.QtCore import QStandardPaths
from PySide6.QtWidgets import QApplication
QStandardPaths.setTestModeEnabled(True)
app = QApplication([])
fluentqt.initialize_resources()
import fluentqt_gallery.settings as settings_module
import fluentqt_gallery.visual as visual_module
identifiers = ("FlowingRibbons", "FloatingDots", "Starfield")
with (
    patch.object(settings_module, "persistence_available", return_value=True),
    patch.object(settings_module, "config_file_path", return_value=Path(sys.argv[1])),
    patch.object(visual_module, "QRandomGenerator") as random,
):
    random.global_.return_value.bounded.return_value = 0
    settings = settings_module.gallery_settings()
    previous = settings.last_home_particle_effect
    first = visual_module.GalleryHomeHero()
    second = visual_module.GalleryHomeHero()
    enabled = settings.home_particles_enabled
    assert first._particles.effect() == second._particles.effect()
    assert first._particles.isAnimationEnabled() == enabled
    assert first._particles.isHidden() != enabled
    chosen = settings.last_home_particle_effect
    if enabled:
        candidates = tuple(name for name in identifiers if name != previous)
        assert chosen == candidates[0]
        assert first._particles.effect() == getattr(fluentqt.ParticleBackdrop.Effect, chosen)
        random.global_.return_value.bounded.assert_called_once_with(len(candidates))
        settings.set_home_particles_enabled(False)
        assert first._particles.isHidden() and second._particles.isHidden()
        assert not settings_module._config_settings().value("home/particlesEnabled", True, type=bool)
        settings.set_home_particles_enabled(True)
        assert first._particles.effect() == getattr(fluentqt.ParticleBackdrop.Effect, chosen)
        assert settings.last_home_particle_effect == chosen
        random.global_.return_value.bounded.assert_called_once()
    else:
        assert chosen == previous
        random.global_.assert_not_called()
    print(json.dumps({"previous": previous, "chosen": chosen}))
'''
        effects = ("FlowingRibbons", "FloatingDots", "Starfield")
        launches = (
            ("fresh", "", "", True),
            *((name, name, name, True) for name in effects),
            *(("bytes-" + name, name.encode("ascii"), name, True) for name in effects),
            *(("repr-" + name, repr(name.encode("ascii")), name, True) for name in effects),
            ("double-quoted-bytes", 'b"FlowingRibbons"', "FlowingRibbons", True),
            ("unknown", "retired-effect", "", True),
            ("saved-by-previous-process", None, None, True),
            ("disabled-legacy-bytes", b"Starfield", "Starfield", False),
            ("re-enabled-next-process", None, None, True),
        )
        with TemporaryDirectory() as temporary_dir:
            path = Path(temporary_dir) / "config.ini"
            persisted = QSettings(str(path), QSettings.IniFormat)
            key = "home/lastParticleEffect"
            previous_chosen = ""
            for launch, saved, previous, enabled in launches:
                with self.subTest(launch=launch):
                    if saved is not None:
                        persisted.setValue(key, saved)
                    else:
                        previous = previous_chosen
                    persisted.setValue("home/particlesEnabled", enabled)
                    persisted.sync()
                    result = subprocess.run(
                        [sys.executable, "-c", probe, str(path)],
                        capture_output=True, text=True, timeout=30,
                    )
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    report = json.loads(result.stdout.strip().splitlines()[-1])
                    self.assertEqual(report["previous"], previous)
                    chosen = report["chosen"]
                    self.assertIn(chosen, effects)
                    if enabled:
                        expected = next(name for name in effects if name != previous)
                        self.assertEqual(chosen, expected)
                        self.assertNotEqual(chosen, previous)
                    else:
                        self.assertEqual(chosen, previous)
                    persisted.sync()
                    self.assertIsInstance(persisted.value(key), str)
                    self.assertEqual(persisted.value(key), chosen)
                    previous_chosen = chosen

    def test_startup_prewarm_completes_hidden_first_layout(self):
        window = GalleryWindow(startup_visuals=True)
        try:
            window.resize(960, 680)
            window._prewarm_queue = ["button", "slider", "tab-view"]
            window._prewarm_total = 3
            window._prewarm_paused = True
            window.show()
            QApplication.processEvents()
            window._prewarm_paused = False
            for _ in range(40):
                window._prewarm_next_route()
                if window._startup_ready_timer.isValid():
                    break
            self.assertTrue(window._startup_ready_timer.isValid())
            self.assertEqual(window._prewarm_done, 3)
            self.assertEqual(window._prewarm_failures, {})
            for route in ("button", "slider", "tab-view"):
                page = window._pages[route][1]
                self.assertTrue(page.isHidden())
                self.assertFalse(page.testAttribute(Qt.WA_PendingResizeEvent))
                self.assertEqual(page.size(), window._content_host.contentsRect().size())
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_startup_waits_for_splash_paint_and_builds_one_sample_per_turn(self):
        window = GalleryWindow(startup_visuals=True)
        try:
            self.assertEqual(window.current_route, "home")
            self.assertEqual(window._pages, {})
            QApplication.processEvents()
            self.assertEqual(window._pages, {})
            self.assertFalse(window._prewarm_started)
            window._prewarm_queue = ["button"]
            window._prewarm_total = 1
            window._prewarm_paused = True
            window.show()
            QApplication.processEvents()
            self.assertTrue(window._prewarm_started)
            self.assertEqual(window._pages, {})
            window._prewarm_paused = False
            window._prewarm_next_route()
            route, staged = window._prewarm_page
            self.assertEqual(route, "button")
            self.assertEqual(staged._gallery_sample_cards, ())
            self.assertTrue(staged.isHidden())
            for count in range(1, len(ENTRY_BY_ROUTE_ID[route].samples) + 1):
                window._prewarm_next_route()
                self.assertEqual(len(staged._gallery_sample_cards), count)
                self.assertNotIn(route, window._pages)
                self.assertEqual(window._prewarm_done, 0)
                self.assertEqual(window._splash.progress(), 0)
                # Navigation must not finish or reveal a partial page.
                window.navigate(route)
                self.assertNotIn(route, window._pages)
                self.assertTrue(staged.isHidden())
                with self.assertRaisesRegex(RuntimeError, "still being prepared"):
                    window._ensure_page(route)
            window._prewarm_next_route()
            self.assertIs(window._pages[route][1], staged)
            self.assertEqual(window._prewarm_done, 1)
            self.assertEqual(window._splash.progress(), 100)
        finally:
            delete_qobject(window)

    def test_startup_is_bounded_and_cold_pages_remain_cancellable(self):
        self.addCleanup(fluentqt.set_motion_mode, fluentqt.current_motion_mode())
        fluentqt.set_motion_mode(fluentqt.MotionMode.Disabled)
        window = GalleryWindow(startup_visuals=True)
        try:
            with patch("fluentqt_gallery.update_checker.GalleryUpdateChecker.check_for_updates") as network:
                window.show()
                self.assertTrue(_wait_until(lambda: window._startup_finished, 10000))
                self.assertEqual(set(window._pages), {"home", "settings"})
                self.assertEqual(window._prewarm_failures, {})
                network.assert_not_called()
            window.navigate("button")
            self.assertTrue(_wait_until(lambda: window._cold_page is not None, 1000))
            staged = window._cold_page[1]
            self.assertTrue(staged.isHidden())
            self.assertTrue(staged._gallery_pending_samples)
            window.navigate("settings")
            QApplication.sendPostedEvents(None, QEvent.DeferredDelete)
            self.assertFalse(shiboken6.isValid(staged))
            self.assertNotIn("button", window._pages)
            window.navigate("button")
            self.assertTrue(_wait_until(lambda: "button" in window._pages, 5000))
            page = window._pages["button"][1]
            self.assertFalse(page._gallery_pending_samples)
            window.navigate("settings")
            window.navigate("button")
            self.assertIs(window._pages["button"][1], page)
        finally:
            delete_qobject(window)

    def test_startup_catalog_grid_builds_one_entry_per_turn(self):
        window = GalleryWindow(startup_visuals=True)
        try:
            window._prewarm_queue = ["all-controls"]
            window._prewarm_total = 1
            window._prewarm_paused = True
            window.show()
            QApplication.processEvents()
            window._prewarm_paused = False
            window._prewarm_next_route()
            route, page = window._prewarm_page
            self.assertEqual(route, "all-controls")
            total = len(page._gallery_pending_entries)
            self.assertGreater(total, 80)
            self.assertEqual(page._gallery_buttons, ())
            for count in range(1, total + 1):
                window._prewarm_next_route()
                self.assertEqual(len(page._gallery_buttons), count)
                self.assertNotIn(route, window._pages)
                self.assertEqual(window._prewarm_done, 0)
                self.assertTrue(page.isHidden())
                self.assertEqual(
                    len(page._gallery_entry_grid.cards), total if count == total else 0
                )
            window._prewarm_next_route()
            self.assertIs(window._pages[route][1], page)
            self.assertEqual(window._prewarm_done, 1)
        finally:
            delete_qobject(window)

    def test_startup_hidden_page_activity_and_close_resume(self):
        window = GalleryWindow(startup_visuals=True)
        try:
            window._prewarm_queue = ["home", "button"]
            window._prewarm_total = 2
            window._prewarm_paused = True
            window.show()
            QApplication.processEvents()
            window._prewarm_paused = False
            window._prewarm_next_route()
            window._prewarm_next_route()
            home = window._pages["home"][1]
            self.assertTrue(home.isHidden())
            for backdrop in home.findChildren(fluentqt.ParticleBackdrop):
                self.assertFalse(backdrop.isAnimating())
            window._prewarm_next_route()
            window._prewarm_next_route()
            staged = window._prewarm_page[1]
            count = len(staged._gallery_sample_cards)
            window.close()
            _qwait(80)
            self.assertFalse(window._prewarm_step_timer.isActive())
            self.assertEqual(len(staged._gallery_sample_cards), count)
            window.show()
            self.assertTrue(_wait_until(lambda: window._startup_ready_timer.isValid(), 3000))
            self.assertIs(window._pages["button"][1], staged)
            self.assertEqual(window._prewarm_done, 2)
        finally:
            delete_qobject(window)

    def test_startup_failure_is_explicit_and_does_not_strand_splash(self):
        window = GalleryWindow(startup_visuals=True)
        failures = []
        window.prewarmFailed.connect(lambda route, detail: failures.append((route, detail)))
        original_create = window._create_page

        def fail_button(route_id, **kwargs):
            if route_id == "button":
                raise RuntimeError("sample initialization failed")
            return original_create(route_id, **kwargs)

        try:
            window._prewarm_queue = ["button", "slider"]
            window._prewarm_total = 2
            with patch.object(window, "_create_page", side_effect=fail_button):
                window.show()
                self.assertTrue(_wait_until(lambda: window._startup_ready_timer.isValid(), 5000))
            self.assertEqual(len(failures), 1)
            self.assertEqual(failures[0][0], "button")
            self.assertIn("sample initialization failed", failures[0][1])
            self.assertEqual(window.property("galleryPrewarmFailedRoutes"), ["button"])
            self.assertEqual(window._pages["button"][1].objectName(), "galleryPageLoadError")
            self.assertIn("slider", window._pages)
            self.assertEqual(window._prewarm_done, 1)
            self.assertEqual(window._splash.progress(), 50)
            self.assertIn("could not be prepared", window._splash.text())
            self.assertTrue(_wait_until(lambda: window._startup_finished, 3000))
        finally:
            delete_qobject(window)

    def test_splash_handoff_caches_native_pixels_and_invalidates(self):
        class PaintCounter(QLabel):
            paints = 0

            def paintEvent(self, event):
                self.paints += 1
                super().paintEvent(event)

        self.addCleanup(fluentqt.set_motion_mode, fluentqt.current_motion_mode())
        self.addCleanup(fluentqt.set_theme, fluentqt.current_theme())
        fluentqt.set_motion_mode(fluentqt.MotionMode.Full)
        host = QWidget()
        host.resize(640, 420)
        content = PaintCounter("Live Gallery content", host)
        content.setGeometry(10, 10, 300, 180)
        cover_host = QWidget(host)
        cover_host.setGeometry(320, 10, 300, 180)
        splash = GallerySplashScreen(cover_host)
        host.show()
        splash.show()
        QApplication.processEvents()
        try:
            before = host.grab(content.geometry()).toImage()
            splash.cache_dismissal_content(content)
            self.assertIsNotNone(content.graphicsEffect())
            self.assertEqual(host.grab(content.geometry()).toImage(), before)
            paints = content.paints
            for _ in range(3):
                content.update()
                host.grab()
            self.assertEqual(content.paints, paints)
            content.resize(330, 185)
            host.grab()
            self.assertGreater(content.paints, paints)
            paints = content.paints
            next_theme = (
                fluentqt.Theme.Dark
                if fluentqt.current_theme() != fluentqt.Theme.Dark
                else fluentqt.Theme.Light
            )
            fluentqt.set_theme(next_theme)
            host.grab()
            self.assertGreater(content.paints, paints)
            splash.hide()
            self.assertIsNone(content.graphicsEffect())
            paints = content.paints
            host.grab()
            self.assertGreater(content.paints, paints)
        finally:
            host.close()
            host.deleteLater()
            QApplication.processEvents()

    def test_splash_cache_preserves_effects_motion_and_particle_lifetime(self):
        self.addCleanup(fluentqt.set_motion_mode, fluentqt.current_motion_mode())
        fluentqt.set_motion_mode(fluentqt.MotionMode.Full)
        host = QWidget()
        host.resize(640, 420)
        content = QWidget(host)
        content.setGeometry(0, 0, 300, 220)
        visible = fluentqt.ParticleBackdrop(content)
        disabled = fluentqt.ParticleBackdrop(content)
        disabled.setAnimationEnabled(False)
        hidden = fluentqt.ParticleBackdrop(content)
        hidden.hide()
        cover_host = QWidget(host)
        cover_host.setGeometry(310, 0, 300, 220)
        splash = GallerySplashScreen(cover_host)
        host.show()
        splash.show()
        QApplication.processEvents()
        try:
            effect = QGraphicsOpacityEffect(content)
            content.setGraphicsEffect(effect)
            splash.cache_dismissal_content(content)
            self.assertIs(content.graphicsEffect(), effect)
            content.setGraphicsEffect(None)
            splash.cache_dismissal_content(host)
            self.assertIsNone(host.graphicsEffect())
            splash.cache_dismissal_content(content)
            self.assertFalse(visible.isAnimationEnabled())
            self.assertFalse(disabled.isAnimationEnabled())
            self.assertTrue(hidden.isAnimationEnabled())
            splash.hide()
            self.assertTrue(visible.isAnimationEnabled())
            self.assertFalse(disabled.isAnimationEnabled())
            self.assertIsNone(content.graphicsEffect())
            splash.show()
            fluentqt.set_motion_mode(fluentqt.MotionMode.Reduced)
            splash.cache_dismissal_content(content)
            self.assertIsNone(content.graphicsEffect())
            fluentqt.set_motion_mode(fluentqt.MotionMode.Full)
            splash.cache_dismissal_content(content)
            self.assertTrue(shiboken6.isValid(host))
            self.assertTrue(shiboken6.isValid(content))
            # Shiboken 6.2 leaves isQAppSingleton uninitialized: its delete()
            # can destroy QApplication. Flush Qt's native deferred deletion.
            splash.deleteLater()
            QCoreApplication.sendPostedEvents(splash, QEvent.DeferredDelete)
            self.assertFalse(shiboken6.isValid(splash))
            self.assertIs(QApplication.instance(), self.app)
            self.assertTrue(shiboken6.isValid(host))
            self.assertTrue(shiboken6.isValid(content))
            self.assertIsNone(content.graphicsEffect())
            self.assertTrue(visible.isAnimationEnabled())
        finally:
            host.close()
            host.deleteLater()
            QApplication.processEvents()

    def test_opt_in_startup_splash_matches_native_chrome_handoff(self):
        self.addCleanup(fluentqt.set_motion_mode, fluentqt.current_motion_mode())
        window = GalleryWindow(startup_visuals=True)
        fluentqt.set_motion_mode(fluentqt.MotionMode.Full)
        try:
            splash = window._splash
            self.assertIsNotNone(splash)
            self.assertEqual(splash.objectName(), "gallerySplashScreen")
            self.assertIs(splash.parentWidget(), window.contentHost())
            self.assertIs(
                window._navigation_view.parentWidget(),
                window.contentHost(),
            )
            self.assertIsNotNone(
                splash.findChild(
                    fluentqt.ProgressRing, "splashProgressRing"
                )
            )
            self.assertTrue(window._menu_button.isHidden())
            # Empty readiness still waits for the first visible splash paint;
            # hidden time must not pay for the branded presentation.
            window._prewarm_queue.clear()
            window._prewarm_paused = False
            window._prewarm_next_route()
            _qwait(300)
            visible = QElapsedTimer()
            visible.start()
            window.show()
            QApplication.processEvents()
            self.assertTrue(splash.isVisible())
            self.assertEqual(splash.geometry(), window.contentHost().rect())
            _qwait(350)
            self.assertIs(window._splash, splash)
            self.assertIsNone(window.findChild(QWidget, "splashLogoTransition"))
            self.assertTrue(_wait_until(lambda: window._splash is None))
            self.assertGreaterEqual(visible.elapsed(), 1400)
            self.assertIsNone(window._splash)
            self.assertIsNotNone(window._navigation_view.graphicsEffect())
            window.navigate("button")
            self.assertIsNone(window._navigation_view.graphicsEffect())
            self.assertTrue(window._menu_button.isVisible())
            _qwait(500)
            self.assertIsNone(window._intro_tour)
            self.assertTrue(_wait_until(lambda: not shiboken6.isValid(splash)))
            self.assertIsNone(window._navigation_view.graphicsEffect())
            self.assertGreaterEqual(visible.elapsed(), 2000)
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_pending_startup_presentation_respects_reduced_motion(self):
        self.addCleanup(fluentqt.set_motion_mode, fluentqt.current_motion_mode())
        for mode in (fluentqt.MotionMode.Reduced, fluentqt.MotionMode.Disabled):
            with self.subTest(mode=mode):
                window = GalleryWindow(startup_visuals=True)
                fluentqt.set_motion_mode(fluentqt.MotionMode.Full)
                try:
                    splash = window._splash
                    window._prewarm_queue.clear()
                    window._prewarm_paused = False
                    window._prewarm_next_route()
                    window.show()
                    _qwait(200)
                    self.assertIs(window._splash, splash)
                    fluentqt.set_motion_mode(mode)
                    self.assertTrue(_wait_until(
                        lambda: not shiboken6.isValid(splash), 400
                    ))
                    self.assertIsNone(window.findChild(QWidget, "splashLogoTransition"))
                finally:
                    window.close()
                    window.deleteLater()
                    QApplication.processEvents()

    def test_gallery_owned_splash_and_title_motion_follow_policy(self):
        original_motion = fluentqt.current_motion_mode()
        self.addCleanup(fluentqt.set_motion_mode, original_motion)
        window = GalleryWindow(startup_visuals=False)
        settings = window._settings
        original_setting = settings.motion_mode
        self.addCleanup(settings.set_motion_mode, original_setting)
        window.show()
        QApplication.processEvents()
        try:
            title = window._title_content
            settings.set_motion_mode(fluentqt.MotionMode.Reduced)

            title.set_back_available(True)
            self.assertEqual(title._back_animation.duration(), 50)
            title._back_animation.stop()
            title.set_chrome_visible(False)
            title.set_chrome_visible(True, animated=True)
            self.assertEqual(title._chrome_animation.duration(), 50)
            title._chrome_animation.stop()
            title._start_button_press(window._back_button)
            press = window._back_button._gallery_title_press_animation
            self.assertEqual(press.duration(), 50)
            press.stop()

            splash = GallerySplashScreen(window.contentHost())
            splash.show()
            splash.dismiss()
            self.assertEqual(splash.findChild(QPropertyAnimation, "splashDismissAnimation").duration(), 50)
            splash.findChild(QPropertyAnimation, "splashDismissAnimation").stop()
            splash.deleteLater()

            settings.set_motion_mode(fluentqt.MotionMode.Full)
            title.set_back_available(False)
            self.assertEqual(title._back_animation.duration(), 250)
            title._back_animation.setCurrentTime(100)
            fluentqt.set_motion_mode(fluentqt.MotionMode.Reduced)
            self.assertLessEqual(
                title._back_animation.duration()
                - title._back_animation.currentTime(),
                50,
            )
            fluentqt.set_motion_mode(fluentqt.MotionMode.Full)
            self.assertEqual(title._back_animation.duration(), 250)

            settings.set_motion_mode(fluentqt.MotionMode.Reduced)

            settings.set_motion_mode(fluentqt.MotionMode.Full)
            title.set_back_available(True)
            dynamic_splash = GallerySplashScreen(window.contentHost())
            dynamic_splash.show()
            dynamic_splash.dismiss()
            settings.set_motion_mode(fluentqt.MotionMode.Disabled)
            self.assertAlmostEqual(title._back_reveal, 1.0, places=4)
            self.assertAlmostEqual(
                dynamic_splash.graphicsEffect().opacity(), 0.0, places=4
            )
            self.assertEqual(
                dynamic_splash.findChild(QPropertyAnimation, "splashDismissAnimation").state(),
                QAbstractAnimation.State.Stopped,
            )

            title.set_back_available(False)
            self.assertAlmostEqual(title._back_reveal, 0.0, places=4)
            self.assertEqual(
                title._back_animation.state(),
                QAbstractAnimation.State.Stopped,
            )
            title.set_chrome_visible(False)
            title.set_chrome_visible(True, animated=True)
            self.assertAlmostEqual(
                title._chrome_reveal_opacity, 1.0, places=4
            )
            self.assertEqual(
                title._chrome_animation.state(),
                QAbstractAnimation.State.Stopped,
            )
            title._start_button_press(window._back_button)
            self.assertAlmostEqual(
                window._back_button.iconScale(), 1.0, places=4
            )
            self.assertEqual(
                press.state(), QAbstractAnimation.State.Stopped
            )

            disabled_splash = GallerySplashScreen(window.contentHost())
            disabled_splash.show()
            disabled_splash.dismiss()
            self.assertAlmostEqual(
                disabled_splash.graphicsEffect().opacity(), 0.0, places=4
            )
            self.assertEqual(
                disabled_splash.findChild(QPropertyAnimation, "splashDismissAnimation").state(),
                QAbstractAnimation.State.Stopped,
            )
        finally:
            window.close()
            window.deleteLater()
            QApplication.sendPostedEvents(None, QEvent.DeferredDelete)
            QApplication.processEvents()

    def test_first_launch_intro_tour_matches_native_steps(self):
        window = GalleryWindow(startup_visuals=False)
        original_intro_completed = window._settings.intro_completed
        window._settings.set_intro_completed(False)
        window.show()
        QApplication.processEvents()
        try:
            window._maybe_start_intro_tour()
            tour = window._intro_tour
            self.assertIsNotNone(tour)
            self.assertTrue(tour._card.isOpen())
            self.assertEqual(tour._card.cardSize(), QSize(330, 168))
            self.assertEqual(tour._counter.text(), "1 / 4")
            self.assertEqual(
                tour._title.text(), "Welcome to Fluent Gallery"
            )
            self.assertEqual(
                tour._body.text(),
                "A live catalog of Fluent controls for Qt, with runnable "
                "samples. Here's a 15-second tour of the essentials.",
            )

            tour.go_to_step(3)
            self.assertEqual(tour._counter.text(), "4 / 4")
            self.assertEqual(tour._next.text(), "Finish")
            self.assertEqual(tour._title.text(), "Make it yours")

            tour.finish_tour()
            QApplication.processEvents()
            self.assertTrue(window._settings.intro_completed)
        finally:
            if window._intro_tour is not None:
                window._intro_tour.finish_tour()
            window._settings.set_intro_completed(original_intro_completed)
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_destroying_window_with_active_intro_tour_releases_overlays(self):
        window = GalleryWindow(startup_visuals=False)
        settings = window._settings
        original_intro_completed = settings.intro_completed
        settings.set_intro_completed(False)
        try:
            window.show()
            QApplication.processEvents()
            window._maybe_start_intro_tour()
            tour = window._intro_tour
            self.assertIsNotNone(tour)
            card, scrim = tour._card, tour._scrim
            self.assertTrue(card.isOpen())

            # Exercise QObject child-removal events while the tour still filters
            # its host; finishing the tour first would hide this lifetime defect.
            delete_qobject(window)
            for obj in (window, tour, card, scrim):
                self.assertFalse(shiboken6.isValid(obj))
            self.assertFalse(settings.intro_completed)
        finally:
            settings.set_intro_completed(original_intro_completed)
            if shiboken6.isValid(window):
                delete_qobject(window)

    def test_gallery_owned_intro_and_navigation_motion_follow_policy(self):
        original_motion = fluentqt.current_motion_mode()
        self.addCleanup(fluentqt.set_motion_mode, original_motion)
        window = GalleryWindow(startup_visuals=False)
        settings = window._settings
        original_setting = settings.motion_mode
        self.addCleanup(settings.set_motion_mode, original_setting)
        original_navigation = settings.navigation_style
        self.addCleanup(
            settings.set_navigation_style, original_navigation
        )
        original_intro_completed = window._settings.intro_completed
        window._settings.set_intro_completed(False)
        window.setFixedSize(1440, 900)
        window.show()
        window._navigation_view.setAnimationEnabled(False)
        QApplication.processEvents()
        try:
            settings.set_motion_mode(fluentqt.MotionMode.Reduced)
            window._set_navigation_style(1)
            QApplication.processEvents()
            foundation = window._top_main_navigation_pane._buttons[
                "foundation"
            ]
            foundation.click()
            QApplication.processEvents()
            popup = window._top_main_navigation_pane._child_flyout
            entrance = popup.findChild(
                QPropertyAnimation,
                "galleryTopNavigationFlyoutEntranceAnimation",
                Qt.FindDirectChildrenOnly,
            )
            self.assertIsNotNone(entrance)
            self.assertLessEqual(entrance.duration(), 50)
            entrance.stop()

            settings_button = window._top_footer_navigation_pane._buttons[
                "settings"
            ]
            window._top_footer_navigation_pane._start_settings_icon_rotation(
                settings_button
            )
            rotation = settings_button._gallery_settings_rotation
            self.assertLessEqual(rotation.duration(), 50)
            rotation.stop()

            settings.set_motion_mode(fluentqt.MotionMode.Disabled)
            window._top_main_navigation_pane._close_child_flyout(False)
            foundation.click()
            disabled_popup = window._top_main_navigation_pane._child_flyout
            disabled_entrance = disabled_popup.findChild(
                QPropertyAnimation,
                "galleryTopNavigationFlyoutEntranceAnimation",
                Qt.FindDirectChildrenOnly,
            )
            self.assertIsNotNone(disabled_entrance)
            self.assertEqual(disabled_popup.pos(), disabled_entrance.endValue())
            self.assertEqual(
                disabled_entrance.state(),
                QAbstractAnimation.State.Stopped,
            )
            window._top_footer_navigation_pane._start_settings_icon_rotation(
                settings_button
            )
            self.assertAlmostEqual(
                settings_button.iconRotation(), 0.0, places=4
            )
            self.assertEqual(
                rotation.state(), QAbstractAnimation.State.Stopped
            )

            window._maybe_start_intro_tour()
            tour = window._intro_tour
            self.assertIsNotNone(tour)
            self.assertAlmostEqual(tour._scrim.progress, 1.0, places=4)
            self.assertEqual(
                tour._dim_animation.state(),
                QAbstractAnimation.State.Stopped,
            )
            tour.go_to_step(1)
            tour.go_to_step(2)
            expected_spot = tour._spotlight_rect(
                window._navigation_view.mainChromeWidget()
            )
            self.assertEqual(tour._scrim.spotlightRect, expected_spot)
            self.assertEqual(
                tour._spot_animation.state(),
                QAbstractAnimation.State.Stopped,
            )
            tour.finish_tour()
            self.assertAlmostEqual(tour._scrim.progress, 0.0, places=4)
        finally:
            if window._intro_tour is not None:
                window._intro_tour.finish_tour()
            window._settings.set_intro_completed(original_intro_completed)
            window.close()
            window.deleteLater()
            QApplication.sendPostedEvents(None, QEvent.DeferredDelete)
            QApplication.processEvents()

    def test_cold_route_uses_native_shimmer_handoff_without_page_motion(self):
        window = GalleryWindow(startup_visuals=False)
        window._startup_visuals = True
        window._startup_finished = True
        window.show()
        QApplication.processEvents()
        try:
            self.assertFalse(
                window._content_host.transitionAnimationEnabled()
            )
            self.assertNotIn("button", window._pages)
            window.navigate("button")
            skeleton_index, skeleton = window._skeleton
            self.assertEqual(skeleton.objectName(), "galleryPageSkeleton")
            self.assertEqual(
                window._content_host.currentIndex(), skeleton_index
            )
            self.assertEqual(window.current_route, "button")

            self.assertTrue(_wait_until(lambda: "button" in window._pages, 5000))
            button_index, button_page = window._pages["button"]
            self.assertEqual(
                window._content_host.currentIndex(), button_index
            )
            self.assertIs(
                window._content_host.pageWidget(button_index), button_page
            )
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_page_skeleton_reuses_native_shimmer_geometry_and_lifecycle(self):
        skeleton = GalleryPageSkeleton()
        skeleton.resize(1000, 700)
        skeleton.show()
        QApplication.processEvents()
        try:
            shimmers = skeleton.findChildren(fluentqt.Shimmer)
            self.assertEqual(len(shimmers), 1)
            shimmer = shimmers[0]
            self.assertEqual(
                shimmer.objectName(), "galleryPageSkeletonShimmer"
            )
            self.assertEqual(shimmer.geometry(), skeleton.rect())
            self.assertEqual(
                shimmer.shimmerTemplate(),
                fluentqt.Shimmer.ShimmerTemplate.Custom,
            )
            self.assertTrue(shimmer.isAnimationRunning())

            content = QRectF(skeleton.rect()).adjusted(24, 34, -24, -48)
            expected_rects = [
                QRectF(content.left(), 34.0, 320.0, 40.0),
                QRectF(content.left(), 90.0, 460.0, 24.0),
                QRectF(content.left(), 142.0, content.width(), 132.0),
                QRectF(content.left(), 290.0, content.width(), 132.0),
                QRectF(content.left(), 438.0, content.width(), 132.0),
            ]
            elements = shimmer.elements()
            self.assertEqual(len(elements), len(expected_rects))
            self.assertTrue(
                all(
                    element.shape
                    == fluentqt.Shimmer.Shape.RoundedRect
                    for element in elements
                )
            )
            self.assertEqual(
                [element.rect for element in elements], expected_rects
            )
            self.assertTrue(
                all(
                    element.radius == float(fluentqt.CornerRadius.Control)
                    for element in elements
                )
            )

            skeleton.hide()
            QApplication.processEvents()
            self.assertFalse(shimmer.isAnimationRunning())
        finally:
            skeleton.close()
            skeleton.deleteLater()
            QApplication.processEvents()

    def test_title_bar_search_and_back_reveal_match_native_interaction(self):
        window = GalleryWindow()
        window.show()
        QApplication.processEvents()
        try:
            window._filter_search_suggestions("split button", None)
            self.assertEqual(
                window._search.suggestions(),
                ["SplitButton", "ToggleSplitButton"],
            )
            window._submit_search("split button", None)
            self.assertEqual(window.current_route, "split-button")

            self.assertTrue(
                _wait_until(
                    lambda: (
                        window._back_button.width() == 24
                        and abs(window._back_button.contentOpacity() - 1.0)
                        <= 0.005
                    ),
                    timeout_ms=1000,
                )
            )
            self.assertEqual(window._back_button.width(), 24)
            self.assertAlmostEqual(
                window._back_button.contentOpacity(), 1.0, places=2
            )
            self.assertEqual(
                window._menu_button.x() - window._back_button.x(), 32
            )

            window.navigate_back()
            self.assertTrue(
                _wait_until(
                    lambda: (
                        window._back_button.width() == 0
                        and abs(window._back_button.contentOpacity()) <= 0.005
                    ),
                    timeout_ms=1000,
                )
            )
            self.assertEqual(window.current_route, "home")
            self.assertEqual(window._back_button.width(), 0)
            self.assertAlmostEqual(
                window._back_button.contentOpacity(), 0.0, places=2
            )
            self.assertEqual(
                window._menu_button.x(), window._back_button.x()
            )
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_plugins_navigation_has_three_levels_and_root_shortcuts(self):
        window = GalleryWindow(startup_visuals=False)
        try:
            pane = window._main_navigation_pane
            if not SPATIAL_AVAILABLE:
                self.assertEqual("plugins" in pane._route_items, QML_BRIDGE_AVAILABLE)
                self.assertEqual("plugins" in window._top_main_navigation_pane._buttons, QML_BRIDGE_AVAILABLE)
                return
            # Complete route/count parity is covered against the generated
            # contract; this test owns the plugin navigation hierarchy.
            self.assertTrue(CATEGORY_BY_ID["spatial"].plugin)
            plugins = pane._route_items["plugins"]
            spatial = pane._route_items["spatial"]
            leaf = pane._route_items["spatial-view"]
            self.assertIsNone(plugins.parent())
            self.assertIs(spatial.parent(), plugins)
            self.assertIs(leaf.parent(), spatial)
            self.assertEqual([plugins.data(Qt.UserRole + 7), spatial.data(Qt.UserRole + 7),
                              leaf.data(Qt.UserRole + 7)], [0, 1, 2])
            self.assertEqual(spatial.data(Qt.UserRole + 5), 23)
            self.assertEqual(leaf.data(Qt.UserRole + 5), 4 + 43 - 3 - 8 + 32)
            pane.set_compact(False)
            pane.sync_selected("spatial-view")
            self.assertTrue(pane._tree.isExpanded(plugins.index()))
            self.assertTrue(pane._tree.isExpanded(spatial.index()))
            self.assertEqual(pane._tree.currentIndex().data(Qt.UserRole + 1), "spatial-view")
            self.assertEqual(pane._tree.selectionIndicatorInset(), 68)
            pane.sync_selected("spatial")
            self.assertEqual(pane._tree.selectionIndicatorInset(), 23)
            pane.sync_selected("spatial-view")
            pane.set_compact(True)
            self.assertEqual(pane._tree.currentIndex().data(Qt.UserRole + 1), "plugins")
            self.assertEqual(pane._tree.selectionIndicatorInset(), 7)
            top = window._top_main_navigation_pane
            self.assertNotIn("spatial", top._buttons)
            top.sync_selected("spatial-item")
            self.assertTrue(top._buttons["plugins"].isChecked())
            self.assertEqual(top._buttons["plugins"].text(), "Plugins")

            plugins_page = window._ensure_page("plugins")[1]
            self.assertEqual([card._route_id for card in plugins_page._gallery_buttons],
                             [category.id for category in CATEGORIES if category.plugin])
            headings = [label.text() for label in plugins_page.findChildren(fluentqt.Label)
                        if label.objectName() == "galleryContentSectionHeader"]
            self.assertEqual(headings, ["Plugin modules"])
            all_page = window._ensure_page("all-controls")[1]
            controls = {card._route_id for card in all_page._gallery_buttons}
            self.assertNotIn("spatial-view", controls)
            self.assertNotIn("spatial-item", controls)
            self.assertIn("button", controls)
            self.assertIn("font-icon", controls)
            home = window._ensure_page("home")[1]
            shortcuts = {card._route_id for card in home._gallery_category_grid.cards}
            self.assertIn("plugins", shortcuts)
            self.assertNotIn("spatial", shortcuts)
            self.assertEqual(shortcuts, {"all-controls", "foundation"} | {
                route.id for route in ROUTES if route.kind == "category" and route.depth == 0
            })
            self.assertEqual(ROUTE_BY_ID["spatial"].parent_id, "plugins")
        finally:
            delete_qobject(window)

    def test_plugins_top_and_compact_flyouts_drill_down_from_same_anchor(self):
        if not SPATIAL_AVAILABLE:
            self.assertNotIn("plugins", ROUTE_BY_ID)
            return
        window = GalleryWindow(startup_visuals=False)
        window.setFixedSize(1180, 900)
        window.show()
        window._navigation_view.setAnimationEnabled(False)
        QApplication.processEvents()
        try:
            window._apply_navigation_style(gallery_settings_module.NavigationStyle.Left)
            window.navigate("spatial", animated=False)
            pane = window._main_navigation_pane
            tree = pane._tree
            tree.expand(pane._route_items["spatial"].index())
            # Select the child immediately while the subtree reveal is running.
            tree.setFocus()
            QTest.keyClick(tree, Qt.Key_Down)
            self.assertTrue(_wait_until(lambda: window.current_route == "spatial-view"),
                            f"Keyboard route={window.current_route}, current={tree.currentIndex().data(Qt.UserRole + 1)}, compact={pane._compact}")
            self.assertTrue(_wait_until(lambda: not tree.selectedIndicatorRect().isEmpty()),
                            f"Keyboard selection={tree.selectionModel().isSelected(tree.currentIndex())}, current={tree.currentIndex().data(Qt.UserRole + 1)}, rect={tree.visualRect(tree.currentIndex())}")
            self.assertTrue(tree.selectionModel().isSelected(pane._route_items["spatial-view"].index()))
            window._set_navigation_style(1)
            QApplication.processEvents()
            top = window._top_main_navigation_pane
            anchor = top._buttons["plugins"]
            QTest.mouseClick(anchor, Qt.LeftButton)
            self.assertTrue(_wait_until(lambda: top._child_flyout is not None))
            rows = top._child_flyout_panel.findChildren(QWidget, options=Qt.FindDirectChildrenOnly)
            self.assertEqual([row._route_id for row in rows], [c.id for c in CATEGORIES if c.plugin])
            QTest.mouseClick(rows[0], Qt.LeftButton, pos=rows[0].rect().center())
            self.assertTrue(_wait_until(lambda: top._child_flyout_panel is not None
                                       and [row._route_id for row in top._child_flyout_panel.findChildren(
                                           QWidget, options=Qt.FindDirectChildrenOnly)] == ["spatial-view", "spatial-item"]))
            self.assertIs(top._child_flyout_anchor, anchor)
            rows = top._child_flyout_panel.findChildren(QWidget, options=Qt.FindDirectChildrenOnly)
            self.assertEqual([row._route_id for row in rows], ["spatial-view", "spatial-item"])
            QTest.mouseClick(rows[0], Qt.LeftButton, pos=rows[0].rect().center())
            self.assertTrue(_wait_until(lambda: window.current_route == "spatial-view"))
            self.assertIsNone(top._child_flyout)
            self.assertTrue(anchor.isChecked())
            self.assertEqual(anchor.text(), "Plugins")
            top._show_child_flyout("plugins", anchor)
            rows = top._child_flyout_panel.findChildren(QWidget, options=Qt.FindDirectChildrenOnly)
            self.assertTrue(rows[0]._selected)
            top._close_child_flyout(False)
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)

            window.setFixedSize(800, 900)
            window._set_navigation_style(0)
            QApplication.processEvents()
            pane = window._main_navigation_pane
            self.assertTrue(pane._compact)
            pane._activate_index(pane._route_items["plugins"].index())
            self.assertTrue(_wait_until(lambda: pane._compact_flyout_panel is not None))
            rows = pane._compact_flyout_panel.findChildren(QWidget, options=Qt.FindDirectChildrenOnly)
            self.assertEqual([row._route_id for row in rows], [c.id for c in CATEGORIES if c.plugin])
            anchor_geometry = pane._compact_flyout_anchor.geometry()
            QTest.mouseClick(rows[0], Qt.LeftButton, pos=rows[0].rect().center())
            self.assertTrue(_wait_until(lambda: pane._compact_flyout_panel is not None
                                       and [row._route_id for row in pane._compact_flyout_panel.findChildren(
                                           QWidget, options=Qt.FindDirectChildrenOnly)] == ["spatial-view", "spatial-item"]))
            self.assertEqual(pane._compact_flyout_anchor.geometry(), anchor_geometry)
            rows = pane._compact_flyout_panel.findChildren(QWidget, options=Qt.FindDirectChildrenOnly)
            self.assertEqual([row._route_id for row in rows], ["spatial-view", "spatial-item"])
            QTest.mouseClick(rows[1], Qt.LeftButton, pos=rows[1].rect().center())
            self.assertTrue(_wait_until(lambda: window.current_route == "spatial-item"
                                       and pane._compact_flyout is None))
            self.assertEqual(pane._tree.currentIndex().data(Qt.UserRole + 1), "plugins")
            pane._show_compact_flyout(pane._route_items["plugins"].index())
            rows = pane._compact_flyout_panel.findChildren(QWidget, options=Qt.FindDirectChildrenOnly)
            self.assertTrue(rows[0]._selected)
        finally:
            delete_qobject(window)

    def test_top_navigation_labels_and_overflow_keep_active_plugins_visible(self):
        window = GalleryWindow(startup_visuals=False)
        window.setFixedSize(760, 640)
        window.show()
        window._navigation_view.setAnimationEnabled(False)
        QApplication.processEvents()
        try:
            window._set_navigation_style(1)
            QApplication.processEvents()
            top = window._top_main_navigation_pane
            selected = "spatial-view" if SPATIAL_AVAILABLE else "button"
            root = "plugins" if SPATIAL_AVAILABLE else "basic-input"
            stable_size = top.sizeHint()
            top.sync_selected(selected)
            self.assertEqual(top.sizeHint(), stable_size)
            self.assertEqual(top.minimumSizeHint(), QSize(52, 48))
            self.assertEqual(top.layout().contentsMargins(), QMargins(8, 6, 8, 6))
            self.assertEqual(top._buttons["home"].text(), "Home")
            self.assertEqual(window._top_footer_navigation_pane._buttons["settings"].text(), "Settings")
            self.assertEqual(top._buttons[root].text(), ROUTE_BY_ID[root].title)
            self.assertTrue(top._buttons[root].isChecked())
            self.assertFalse(top._buttons[root].isHidden())
            self.assertFalse(top._buttons["home"].isHidden())
            self.assertTrue(top._more_button.isVisible())
            for route_id, button in top._buttons.items():
                self.assertEqual(button.height(), 36)
                if route_id not in (root, "home"):
                    self.assertEqual(button.text(), "")
                if not button.isHidden():
                    self.assertLessEqual(button.geometry().right(), top.rect().right())
            hidden = [route_id for route_id, button in top._buttons.items() if button.isHidden()]
            self.assertEqual([route_id for route_id, _title in top._children["overflow"]], hidden)
            target = next(route_id for route_id in hidden if top._children.get(route_id))
            QTest.mouseClick(top._more_button, Qt.LeftButton)
            self.assertTrue(_wait_until(lambda: top._child_flyout_panel is not None))
            row = top._child_flyout_panel.findChild(QWidget, "galleryCompactNavigationFlyoutRow_" + target)
            self.assertIsNotNone(row)
            QTest.mouseClick(row, Qt.LeftButton, pos=row.rect().center())
            self.assertTrue(_wait_until(lambda: window.current_route == target
                                       and top._child_flyout_panel is not None))
            self.assertTrue(top._buttons[target].isChecked())
            self.assertFalse(top._buttons[target].isHidden())
            self.assertEqual(top._buttons[target].text(), ROUTE_BY_ID[target].title)
            rows = top._child_flyout_panel.findChildren(QWidget, options=Qt.FindDirectChildrenOnly)
            self.assertEqual([row._route_id for row in rows],
                             [route_id for route_id, _title in top._children[target]])
        finally:
            delete_qobject(window)

    def test_top_navigation_uses_native_icon_bar_and_child_flyout(self):
        window = GalleryWindow()
        window.setFixedSize(1440, 900)
        window.show()
        window._navigation_view.setAnimationEnabled(False)
        QApplication.processEvents()
        try:
            window._set_navigation_style(1)
            QApplication.processEvents()
            self.assertEqual(
                window._navigation_view.effectiveDisplayMode(),
                fluentqt.NavigationView.DisplayMode.Top,
            )
            self.assertIs(
                window._navigation_view.mainChromeWidget(),
                window._top_main_navigation_pane,
            )
            self.assertIs(
                window._navigation_view.footerChromeWidget(),
                window._top_footer_navigation_pane,
            )
            self.assertTrue(window._menu_button.isHidden())
            self.assertEqual(window._content_host.geometry().top(), 48)
            self.assertEqual(
                window._top_main_navigation_pane.sizeHint().height(), 48
            )
            self.assertIsInstance(
                window._top_main_navigation_pane, fluentqt.FluentWidget
            )

            foundation = window._top_main_navigation_pane._buttons[
                "foundation"
            ]
            foundation.click()
            QApplication.processEvents()
            self.assertEqual(window.current_route, "foundation")
            popup = window._top_main_navigation_pane._child_flyout
            self.assertIsNotNone(popup)
            self.assertTrue(popup.isVisible())
            panel = window._top_main_navigation_pane._child_flyout_panel
            self.assertIsInstance(panel, fluentqt.FluentWidget)
            rows = panel.findChildren(
                QWidget, options=Qt.FindDirectChildrenOnly
            )
            self.assertEqual(len(rows), 7)
            self.assertTrue(
                all(isinstance(row, fluentqt.FluentWidget) for row in rows)
            )
            self.assertTrue(all(row.height() == 36 for row in rows))

            visible_card = popup.rect().marginsRemoved(
                popup.contentsMargins()
            )
            self.assertEqual(
                panel.geometry(),
                visible_card.marginsRemoved(QMargins(3, 4, 3, 4)),
            )
            entrance = popup.findChild(
                QPropertyAnimation,
                "galleryTopNavigationFlyoutEntranceAnimation",
                Qt.FindDirectChildrenOnly,
            )
            self.assertIsNotNone(entrance)
            motion = window._top_main_navigation_pane.theme_tokens()[
                "animation"
            ]
            self.assertEqual(
                entrance.duration(), int(motion["duration"]["fast"])
            )
            self.assertEqual(
                entrance.easingCurve(), motion["easing"]["decelerate"]
            )

            _qwait(int(motion["duration"]["fast"]) + 30)
            anchor_top_left = foundation.mapTo(window, QPoint(0, 0))
            visible_geometry = popup.geometry().marginsRemoved(
                popup.contentsMargins()
            )
            self.assertEqual(visible_geometry.left(), anchor_top_left.x())
            self.assertEqual(
                visible_geometry.top(),
                anchor_top_left.y() + foundation.height() + 8,
            )

            window._set_navigation_style(0)
            QApplication.processEvents()
            self.assertIs(
                window._navigation_view.mainChromeWidget(),
                window._main_navigation_pane,
            )
            self.assertIs(
                window._navigation_view.footerChromeWidget(),
                window._footer_navigation_pane,
            )
            self.assertTrue(window._menu_button.isEnabled())
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_top_to_left_navigation_releases_compact_density_after_animation(self):
        window = GalleryWindow(startup_visuals=False)
        window.setFixedSize(1440, 900)
        window.show()
        window._navigation_view.setAnimationEnabled(True)
        QApplication.processEvents()
        try:
            window._set_navigation_style(1)
            _qwait(300)
            window._set_navigation_style(0)
            self.assertTrue(window._main_navigation_pane._compact)
            _qwait(500)
            self.assertEqual(
                window._navigation_view.effectiveDisplayMode(),
                fluentqt.NavigationView.DisplayMode.Left,
            )
            self.assertTrue(window._navigation_view.isPaneOpen())
            self.assertEqual(
                window._main_navigation_pane.geometry().x(), 0
            )
            self.assertFalse(window._main_navigation_pane._compact)
            self.assertFalse(window._footer_navigation_pane._compact)
            self.assertTrue(window._main_navigation_pane._tree.isVisible())
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_compact_navigation_maps_children_and_opens_native_flyout(self):
        window = GalleryWindow()
        window.setFixedSize(800, 900)
        window.show()
        window._navigation_view.setAnimationEnabled(False)
        QApplication.processEvents()
        try:
            self.assertEqual(
                window._navigation_view.effectiveDisplayMode(),
                fluentqt.NavigationView.DisplayMode.LeftCompact,
            )
            window.navigate("button", animated=False)
            QApplication.processEvents()
            pane = window._main_navigation_pane
            self.assertTrue(pane._compact)
            _qwait(300)
            self.assertAlmostEqual(
                float(
                    pane._tree.property(
                        "galleryCompactVisualProgress"
                    )
                ),
                1.0,
                places=2,
            )
            self.assertAlmostEqual(
                float(
                    window._footer_navigation_pane._tree.property(
                        "galleryCompactVisualProgress"
                    )
                ),
                1.0,
                places=2,
            )
            category_index = pane._route_items["basic-input"].index()
            category_rect = pane._tree.visualRect(category_index)
            help_position = category_rect.center()
            help_event = QHelpEvent(
                QEvent.Type.ToolTip,
                help_position,
                pane._tree.viewport().mapToGlobal(help_position),
            )
            QApplication.sendEvent(pane._tree.viewport(), help_event)
            self.assertIsNotNone(pane._compact_tooltip)
            self.assertEqual(pane._compact_tooltip.text(), "Basic input")
            self.assertTrue(pane._compact_tooltip.isVisible())
            QApplication.sendEvent(
                pane._tree.viewport(), QEvent(QEvent.Type.Leave)
            )
            _qwait(180)
            self.assertFalse(pane._compact_tooltip.isVisible())

            activated_routes: list[str] = []
            pane.routeActivated.connect(activated_routes.append)
            pane._tree.setCurrentIndex(
                pane._route_items["home"].index()
            )
            pane._tree.setFocus()
            QTest.keyClick(pane._tree, Qt.Key.Key_Down)
            _qwait(20)
            self.assertEqual(activated_routes[-1], "foundation")

            window.navigate("button", animated=False)
            QApplication.processEvents()
            self.assertEqual(
                pane._tree.currentIndex().data(Qt.UserRole + 1),
                "basic-input",
            )
            pane.setProperty("fluentNavPaneFloating", True)
            QApplication.processEvents()
            self.assertTrue(pane._surface_visible)
            pane._activate_index(category_index)
            QApplication.processEvents()
            self.assertIsNone(pane._compact_flyout)
            self.assertTrue(pane._tree.isExpanded(category_index))
            pane.setProperty("fluentNavPaneFloating", False)
            pane._tree.collapse(category_index)
            QApplication.processEvents()
            self.assertFalse(pane._surface_visible)

            footer = window._footer_navigation_pane
            footer.setProperty("fluentNavPaneFloating", True)
            QApplication.processEvents()
            self.assertTrue(footer._surface_visible)
            self.assertTrue(
                footer._tree.property("fluentPreserveParentSurface")
            )
            self.assertTrue(
                footer._tree.viewport().property(
                    "fluentPreserveParentSurface"
                )
            )
            footer.setProperty("fluentNavPaneFloating", False)
            QApplication.processEvents()
            self.assertFalse(footer._surface_visible)

            pane._activate_index(category_index)
            QApplication.processEvents()
            popup = pane._compact_flyout
            self.assertIsNotNone(popup)
            self.assertTrue(popup.isVisible())
            rows = pane._compact_flyout_panel.findChildren(
                QWidget, options=Qt.FindDirectChildrenOnly
            )
            self.assertEqual(
                len(rows), len(entries_for_category("basic-input"))
            )
            rows[0].activated.emit("button")
            QApplication.processEvents()
            self.assertEqual(window.current_route, "button")
            self.assertIsNone(pane._compact_flyout)
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_minimal_navigation_keeps_categories_open_and_dismisses_leaves(self):
        window = GalleryWindow(startup_visuals=False)
        window.setFixedSize(520, 760)
        window.show()
        window._navigation_view.setAnimationEnabled(False)
        QApplication.processEvents()
        try:
            self.assertEqual(
                window._navigation_view.effectiveDisplayMode(),
                fluentqt.NavigationView.DisplayMode.LeftMinimal,
            )
            window._toggle_navigation_pane()
            QApplication.processEvents()
            self.assertTrue(window._navigation_view.isPaneOpen())

            pane = window._main_navigation_pane
            category_index = pane._route_items["layout"].index()
            pane._activate_index(category_index)
            QApplication.processEvents()

            self.assertEqual(window.current_route, "layout")
            self.assertTrue(window._navigation_view.isPaneOpen())
            self.assertTrue(pane._tree.isExpanded(category_index))

            pane._activate_index(pane._route_items["card"].index())
            QApplication.processEvents()

            self.assertEqual(window.current_route, "card")
            self.assertFalse(window._navigation_view.isPaneOpen())
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_settings_navigation_icon_uses_native_rotation_feedback(self):
        window = GalleryWindow()
        window.show()
        QApplication.processEvents()
        try:
            footer = window._footer_navigation_pane
            rotations: list[float] = []
            footer._settings_rotation_animation.valueChanged.connect(
                lambda value: rotations.append(float(value))
            )
            footer._activate_index(footer._item.index())
            self.assertEqual(window.current_route, "settings")
            self.assertTrue(
                _wait_until(
                    lambda: any(value > 0.0 for value in rotations),
                    timeout_ms=1000,
                )
            )
            self.assertTrue(
                _wait_until(
                    lambda: abs(footer._settings_icon_rotation) <= 0.005,
                    timeout_ms=1500,
                )
            )
            self.assertAlmostEqual(
                footer._settings_icon_rotation, 0.0, places=2
            )
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_component_page_matches_native_content_sections(self):
        window = GalleryWindow()
        window.show()
        window.navigate("button", animated=False)
        QApplication.processEvents()
        try:
            _index, page = window._pages["button"]
            labels = tuple(
                label.text() for label in page.findChildren(fluentqt.Label)
            )
            for heading in ("Button", "Overview", "Use", "Live examples", "Category"):
                with self.subTest(heading=heading):
                    self.assertIn(heading, labels)
            self.assertNotIn("C++ Gallery is the contract", labels)
            self.assertEqual(page.findChildren(fluentqt.Breadcrumb), [])
            self.assertEqual(
                len(page._gallery_sample_cards),
                len(ENTRY_BY_ROUTE_ID["button"].samples),
            )
            for card in page._gallery_sample_cards:
                self.assertEqual(
                    card._gallery_source_expander.headerText(),
                    "Source code",
                )
                self.assertEqual(
                    card._gallery_preview_surface.appearance(),
                    fluentqt.Card.Appearance.LayerAlt,
                )
            reference = page._gallery_reference_card
            self.assertEqual(
                reference.findChild(
                    fluentqt.Label, "galleryComponentReferenceImport"
                ).text(),
                "import fluentqt",
            )
            self.assertEqual(
                reference.findChild(
                    fluentqt.Label, "galleryComponentReferenceType"
                ).text(),
                "fluentqt.Button",
            )
            self.assertEqual(
                reference.findChild(
                    fluentqt.Label, "galleryComponentReferenceModule"
                ).text(),
                "fluentqt.basicinput",
            )
            reference_text = "\n".join(
                label.text()
                for label in reference.findChildren(fluentqt.Label)
            )
            self.assertNotIn(".h", reference_text)
            self.assertNotIn("fluent::", reference_text)
            self.assertNotIn("CMake", reference_text)
            category_card = page._gallery_category_card
            self.assertIsInstance(category_card, QFrame)
            self.assertNotIsInstance(category_card, fluentqt.Card)
            self.assertEqual(category_card.objectName(), "galleryEntryCard")
            self.assertEqual(
                category_card.property("galleryTargetRouteId"),
                "basic-input",
            )
            icon_tile = category_card.findChild(QWidget, "galleryIconTile")
            self.assertIsNotNone(icon_tile)
            self.assertEqual(icon_tile.size(), QSize(40, 40))
            self.assertTrue(
                icon_tile.testAttribute(Qt.WA_TransparentForMouseEvents)
            )
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_foundation_pages_match_native_sections_and_full_catalog(self):
        window = GalleryWindow()
        window.show()
        QApplication.processEvents()
        expected_sections = {
            "foundation-qmlplus": ("Bindings", "States", "Animation", "Anchors"),
            "foundation-typography": ("Type ramp", "Use semantic roles"),
            "foundation-color": (
                "Text",
                "Fill & accent",
                "Background & layers",
                "Stroke",
                "System",
                "Charts",
            ),
            "foundation-iconography": ("Complete Regular catalog",),
            "foundation-geometry": (
                "Corner radius",
                "Stroke widths",
                "Use geometry tokens",
            ),
            "foundation-spacing": (
                "Spacing scale",
                "Component padding",
                "Gaps",
                "Control heights",
            ),
        }
        foundation_code_blocks = []
        try:
            for route_id, expected in expected_sections.items():
                with self.subTest(route=route_id):
                    window.navigate(route_id, record_history=False, animated=False)
                    QApplication.processEvents()
                    _index, page = window._pages[route_id]
                    headings = tuple(
                        label.text()
                        for label in page.findChildren(fluentqt.Label)
                        if label.objectName() == "galleryContentSectionHeader"
                    )
                    self.assertEqual(headings, expected)
                    self.assertNotIn("Overview", headings)
                    foundation_code_blocks.extend(
                        page.findChildren(GalleryCodeBlock)
                    )

            # Twelve atomic QMLPlus examples, plus Typography and Geometry.
            self.assertEqual(len(foundation_code_blocks), 14)
            for block in foundation_code_blocks:
                with self.subTest(code_block=block.objectName()):
                    source = block._code
                    tree = ast.parse(source)
                    imports = [
                        node
                        for node in tree.body
                        if isinstance(node, (ast.Import, ast.ImportFrom))
                    ]
                    if imports:
                        self.assertIs(tree.body[0], imports[0])
                    loaded_names = {
                        node.id
                        for node in ast.walk(tree)
                        if isinstance(node, ast.Name)
                        and isinstance(node.ctx, ast.Load)
                    }
                    for imported in imports:
                        for alias in imported.names:
                            self.assertNotEqual(alias.name, "fluentqt")
                            if alias.name == "*":
                                continue
                            local_name = alias.asname or (
                                alias.name
                                if isinstance(imported, ast.ImportFrom)
                                else alias.name.split(".", 1)[0]
                            )
                            self.assertIn(local_name, loaded_names)
                    for statement in ast.walk(tree):
                        if not isinstance(
                            statement,
                            (ast.AnnAssign, ast.Assign, ast.Expr, ast.Return),
                        ):
                            continue
                        if statement.end_lineno is None or (
                            statement.lineno == statement.end_lineno
                        ):
                            continue
                        value = statement.value
                        if not isinstance(value, ast.Call):
                            continue
                        rendered = ast.unparse(statement)
                        self.assertTrue(
                            "\n" in rendered
                            or statement.col_offset + len(rendered)
                            > _DISPLAY_PREFERRED_LINE_LENGTH,
                            "{0} needlessly wraps a short call: {1}".format(
                                block.objectName(), rendered
                            ),
                        )
                    compiled = compile(
                        source,
                        "<{0}>".format(block.objectName()),
                        "exec",
                    )
                    self.assertLessEqual(
                        max(map(len, source.splitlines()), default=0),
                        _DISPLAY_HARD_LINE_LENGTH,
                    )
                    if block.objectName() != "galleryFoundationGeometryCodeBlock":
                        namespace = {
                            "__name__": "fluentqt_foundation_snippet",
                            "fluentqt": fluentqt,
                        }
                        if block.property("galleryRouteId") == "foundation-qmlplus":
                            namespace["panel"] = QWidget()
                        exec(compiled, namespace)
                        for widget in _take_top_level_widgets(namespace):
                            if shiboken6.isValid(widget):
                                widget.close()
                                widget.deleteLater()
                        namespace.clear()
                        QApplication.processEvents()

            geometry_code = next(
                block._code
                for block in foundation_code_blocks
                if block.objectName() == "galleryFoundationGeometryCodeBlock"
            )
            self.assertIn(
                "from PySide6.QtGui import QPen",
                geometry_code,
            )
            self.assertIn("tokens = self.theme_tokens()", geometry_code)
            self.assertIn(
                "fluentqt.Spacing.Border.Focused",
                geometry_code,
            )
            self.assertIn("tokens.colors.strokeFocusOuter", geometry_code)
            self.assertIn("fluentqt.CornerRadius.Control", geometry_code)
            self.assertIn(
                "painter.drawRoundedRect(control_rect, radius, radius)",
                geometry_code,
            )
            self.assertNotIn("class GeometryPreview", geometry_code)
            self.assertNotIn("preview.resize", geometry_code)
            self.assertLessEqual(len(geometry_code.splitlines()), 12)
            self.assertNotIn("colors.stroke_focus_outer", geometry_code)

            _index, typography = window._pages["foundation-typography"]
            ramp = typography.findChild(
                TypographyRampCard, "galleryTypographyTypeRamp"
            )
            self.assertIsNotNone(ramp)
            self.assertEqual(
                tuple(row[1] for row in ramp.ROWS),
                (
                    "Display",
                    "Title Large",
                    "Title",
                    "Subtitle",
                    "Body Large Strong",
                    "Body Large",
                    "Body Strong",
                    "Body",
                    "Caption",
                ),
            )

            _index, iconography = window._pages["foundation-iconography"]
            browser = iconography.findChild(GalleryIconBrowser, "galleryIconBrowser")
            self.assertIsNotNone(browser)
            self.assertEqual(browser.iconCount(), 9558)
            self.assertEqual(browser.visibleIconCount(), browser.iconCount())
            self.assertEqual(
                browser.search.placeholderText(),
                "Search name, 20, U+F109, or paste a lookup...",
            )
            self.assertEqual(browser.pager.numberOfPages(), 45)
            self.assertEqual(browser.pager.selectedPageIndex(), 0)
            self.assertIn("1-216", browser.page_label.text())
            self.assertIn("Page 1 of 45", browser.page_label.text())
            self.assertFalse(browser.page_label.wordWrap())

            records = _load_icon_catalog()
            native_catalog_path = (
                Path(__file__).resolve().parents[4]
                / "res"
                / "icons"
                / "FluentQtIcons.json"
            )
            native_catalog = json.loads(
                native_catalog_path.read_text(encoding="utf-8")
            )
            self.assertEqual(
                {record.name: record.codepoint for record in records},
                native_catalog,
            )
            records_by_name = {record.name: record for record in records}
            access_time_24 = records_by_name["ic_fluent_access_time_24_regular"]
            access_time_20 = records_by_name["ic_fluent_access_time_20_regular"]
            glyphs = _catalog_glyphs_for_size(records, 20)
            self.assertEqual(
                glyphs[access_time_24.codepoint],
                chr(access_time_20.codepoint),
            )
            icon_font = _icon_font(20)
            self.assertEqual(icon_font.family(), "FluentQt Icons")
            self.assertEqual(icon_font.pixelSize(), 20)
            self.assertEqual(
                icon_font.hintingPreference(),
                QFont.HintingPreference.PreferNoHinting
                if sys.platform == "win32"
                else QFont.HintingPreference.PreferVerticalHinting,
            )
            self.assertTrue(
                icon_font.styleStrategy()
                & QFont.StyleStrategy.NoSubpixelAntialias
            )
            self.assertEqual(browser.grid.page_item_count(), 216)
            self.assertEqual(browser.grid._large_glyphs, glyphs)

            browser.search.setText("ruler 20")
            QApplication.processEvents()
            self.assertGreater(browser.visibleIconCount(), 0)
            self.assertLess(browser.visibleIconCount(), browser.iconCount())
            self.assertFalse(browser.showingClosestMatches())

            browser.search.setText("calendar 20")
            QApplication.processEvents()
            exact_calendar_count = browser.visibleIconCount()
            self.assertGreater(exact_calendar_count, 0)
            self.assertFalse(browser.showingClosestMatches())
            browser.search.setText("calender 20")
            QApplication.processEvents()
            self.assertEqual(browser.visibleIconCount(), exact_calendar_count)
            self.assertTrue(browser.showingClosestMatches())

            browser.search.setText("delete 20")
            QApplication.processEvents()
            exact_delete_count = browser.visibleIconCount()
            self.assertGreater(exact_delete_count, 0)
            browser.search.setText("trash 20")
            QApplication.processEvents()
            self.assertEqual(browser.visibleIconCount(), exact_delete_count)
            self.assertTrue(browser.showingClosestMatches())

            browser.search.setText("qz")
            QApplication.processEvents()
            self.assertEqual(browser.visibleIconCount(), 0)
            self.assertFalse(browser.showingClosestMatches())
            browser.search.setText("U+F109")
            QApplication.processEvents()
            self.assertEqual(browser.visibleIconCount(), 1)
            self.assertFalse(browser.showingClosestMatches())
            browser.search.setText("ic_fluent_add_20_regular")
            QApplication.processEvents()
            self.assertEqual(browser.visibleIconCount(), 1)
            self.assertTrue(browser.pagination.isHidden())
            self.assertEqual(browser.pager.numberOfPages(), 1)

            browser.grid.resize(600, browser.grid.heightForWidth(600))
            window.resize(1180, 760)
            QApplication.processEvents()
            hover_position = QPoint(22, 22)
            hover_event = QMouseEvent(
                QEvent.MouseMove,
                QPointF(hover_position),
                QPointF(browser.grid.mapToGlobal(hover_position)),
                Qt.NoButton,
                Qt.NoButton,
                Qt.NoModifier,
            )
            QApplication.sendEvent(browser.grid, hover_event)
            _qwait(360)
            QApplication.processEvents()
            hover_tip = browser.findChild(
                fluentqt.ToolTip, "galleryIconHoverTip"
            )
            self.assertIsNotNone(hover_tip)
            self.assertIn("ic_fluent_add_20_regular", hover_tip.text())
            self.assertIn("U+", hover_tip.text())
            self.assertIn("20 px", hover_tip.text())
            self.assertIn("Click to copy lookup", hover_tip.text())

            QApplication.clipboard().clear()
            QTest.mouseClick(
                browser.grid,
                Qt.LeftButton,
                Qt.NoModifier,
                QPoint(22, 22),
            )
            self.assertEqual(
                QApplication.clipboard().text(),
                'fluentqt.FontIcon("ic_fluent_add_20_regular")',
            )
            self.assertIsNotNone(
                window.findChild(QWidget, "galleryToast")
            )
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_motion_setting_defaults_noops_persists_and_reloads(self):
        self.assertEqual(int(fluentqt.MotionMode.Full), 0)
        self.assertEqual(int(fluentqt.MotionMode.Reduced), 1)
        self.assertEqual(int(fluentqt.MotionMode.Disabled), 2)
        original_motion = fluentqt.current_motion_mode()
        original_theme = fluentqt.current_theme()
        self.addCleanup(fluentqt.set_motion_mode, original_motion)
        self.addCleanup(fluentqt.set_theme, original_theme)
        with TemporaryDirectory() as temporary_dir:
            path = Path(temporary_dir) / "config.ini"
            with (
                patch.object(
                    gallery_settings_module,
                    "persistence_available",
                    return_value=True,
                ),
                patch.object(
                    gallery_settings_module,
                    "config_file_path",
                    return_value=path,
                ),
            ):
                persisted = QSettings(str(path), QSettings.IniFormat)
                persisted.clear()
                persisted.sync()

                first = gallery_settings_module.GallerySettings()
                emitted: list[int] = []
                first.motionModeChanged.connect(emitted.append)
                try:
                    self.assertEqual(
                        first.motion_mode, fluentqt.MotionMode.Full
                    )
                    self.assertEqual(
                        fluentqt.current_motion_mode(),
                        fluentqt.MotionMode.Full,
                    )
                    first.set_motion_mode(fluentqt.MotionMode.Full)
                    self.assertEqual(emitted, [])

                    first.set_motion_mode(fluentqt.MotionMode.Reduced)
                    self.assertEqual(emitted, [1])
                    self.assertEqual(
                        fluentqt.current_motion_mode(),
                        fluentqt.MotionMode.Reduced,
                    )
                    persisted.sync()
                    self.assertEqual(
                        int(persisted.value("settings/motionMode")), 1
                    )
                finally:
                    first._system_theme_poll.stop()
                    first.deleteLater()
                    QApplication.processEvents()

                fluentqt.set_motion_mode(fluentqt.MotionMode.Full)
                reloaded = gallery_settings_module.GallerySettings()
                try:
                    self.assertEqual(
                        reloaded.motion_mode, fluentqt.MotionMode.Reduced
                    )
                    self.assertEqual(
                        fluentqt.current_motion_mode(),
                        fluentqt.MotionMode.Reduced,
                    )
                finally:
                    reloaded._system_theme_poll.stop()
                    reloaded.deleteLater()
                    QApplication.processEvents()

    def test_particle_gpu_request_is_component_owned_not_persisted(self):
        settings = gallery_settings_module.gallery_settings()
        self.assertFalse(hasattr(settings, "particle_acceleration_enabled"))
        backdrop = fluentqt.ParticleBackdrop()
        try:
            self.assertFalse(backdrop.isGpuAccelerationEnabled())
            changes = []
            backdrop.gpuAccelerationEnabledChanged.connect(changes.append)
            backdrop.setGpuAccelerationEnabled(False)
            backdrop.setGpuAccelerationEnabled(True)
            backdrop.setGpuAccelerationEnabled(True)
            self.assertEqual(changes, [True])
            backdrop.setGpuAccelerationEnabled(False)
            self.assertEqual(changes, [True, False])
        finally:
            delete_qobject(backdrop)

    def test_settings_page_matches_native_rows_and_choices(self):
        window = GalleryWindow()
        window.show()
        window.navigate("settings", animated=False)
        QApplication.processEvents()
        try:
            _index, page = window._pages["settings"]
            self.assertEqual(len(page._gallery_settings_rows), 9)
            particles = page.findChild(
                fluentqt.ToggleSwitch, "gallerySettingsHomeParticlesToggle"
            )
            self.assertIsNotNone(particles)
            self.assertEqual(particles.accessibleName(), "Home particle effects")
            settings = window._settings
            original_particles = settings.home_particles_enabled
            self.addCleanup(settings.set_home_particles_enabled, original_particles)
            settings.set_home_particles_enabled(True)
            self.assertTrue(particles.isOn())
            self.assertIsNone(page.findChild(
                fluentqt.ComboBox, "gallerySettingsParticleRenderingChoice"
            ))
            particles.setFocus(Qt.OtherFocusReason)
            QTest.keyClick(particles, Qt.Key_Space)
            self.assertFalse(settings.home_particles_enabled)
            _index, home = window._pages["home"]
            self.assertTrue(home._gallery_hero._particles.isHidden())
            self.assertFalse(home._gallery_hero._particles.isAnimating())
            settings.set_home_particles_enabled(True)
            self.assertTrue(particles.isOn())
            self.assertIsNone(
                page.findChild(
                    fluentqt.ComboBox, "gallerySettingsStyleChoice"
                )
            )
            labels = tuple(label.text() for label in page.findChildren(fluentqt.Label))
            for text in (
                "Appearance & behavior",
                "App theme",
                "Accent color",
                "Motion",
                "Home particle effects",
                "Navigation style",
                "Window background effect",
                "App behavior",
                "Close button behavior",
                "Updates",
                "Gallery updates",
            ):
                self.assertIn(text, labels)
            expected_choices = (
                ("Use system setting", "Light", "Dark", "High contrast"),
                ("Full", "Reduced", "Disabled"),
                ("Left", "Top"),
                ("Normal", "Mica", "Acrylic"),
                ("Minimize window", keep_running_choice(), "Quit app"),
            )
            for combo, expected in zip(
                page._gallery_settings_choices, expected_choices
            ):
                self.assertEqual(
                    tuple(combo.itemText(index) for index in range(combo.count())),
                    expected,
                )
            self.assertIsNotNone(
                page.findChild(
                    fluentqt.Button, "gallerySettingsCheckUpdatesButton"
                )
            )

            accent = page._gallery_settings_buttons[0]
            accent._open_flyout()
            QApplication.processEvents()
            accent_flyouts = [
                flyout
                for flyout in window.findChildren(fluentqt.Flyout)
                if flyout.isVisible()
            ]
            self.assertEqual(len(accent_flyouts), 1)
            accent_flyout = accent_flyouts[0]
            self.assertIn(
                "Accent color",
                {
                    label.text()
                    for label in accent_flyout.findChildren(fluentqt.Label)
                },
            )
            swatches = [
                widget
                for widget in accent_flyout.findChildren(QWidget)
                if widget.size() == QSize(30, 30)
                and widget.toolTip().startswith("#")
            ]
            self.assertEqual(len(swatches), 16)
            self.assertTrue(
                {"Custom…", "Reset"}.issubset(
                    {
                        button.text()
                        for button in accent_flyout.findChildren(
                            fluentqt.Button
                        )
                    }
                )
            )
            accent_flyout.close()
            QApplication.processEvents()

            handler = page._gallery_handle_update_result
            handler(
                UpdateResult(
                    status=UpdateStatus.UpdateAvailable,
                    current_version="1.0.0",
                    latest_version="2.0.0",
                    release_url=QUrl("https://example.com/release"),
                    asset_url=QUrl("https://example.com/gallery.exe"),
                )
            )
            self.assertEqual(page._gallery_update_button.text(), "Download")
            self.assertIn("Version 2.0.0 available", page._gallery_update_status.text())
            handler(
                UpdateResult(
                    status=UpdateStatus.UpToDate,
                    current_version="2.0.0",
                )
            )
            self.assertEqual(page._gallery_update_button.text(), "Check again")
            self.assertEqual(page._gallery_update_status.text(), "Latest version 2.0.0")
            handler(
                UpdateResult(
                    status=UpdateStatus.Error,
                    current_version="2.0.0",
                    message="offline",
                )
            )
            self.assertEqual(page._gallery_update_button.text(), "Try again")
            self.assertEqual(page._gallery_update_status.text(), "Update check failed")
            self.assertEqual(page._gallery_update_button.toolTip(), "offline")
            self.assertEqual(compare_versions("v2.0.0", "1.9.9"), 1)
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_snapshot_composites_the_platform_backdrop_fallback(self):
        window = GalleryWindow()
        window.show()
        QApplication.processEvents()
        try:
            window.navigate("expander", record_history=False, animated=False)
            QApplication.processEvents()
            _index, page = window._pages["expander"]
            scroll_bar = page.verticalScrollBar()
            self.assertIsInstance(scroll_bar, fluentqt.ScrollBar)
            overlay_bar = page.findChild(
                fluentqt.ScrollBar,
                "fluentScrollViewFloatingVerticalBar",
            )
            self.assertIsNotNone(overlay_bar)
            overlay_bar.setOpacity(1.0)
            _reset_current_page_scroll(window)
            self.assertEqual(scroll_bar.value(), scroll_bar.minimum())
            self.assertEqual(overlay_bar.opacity(), 0.0)
            _qwait(50)
            QApplication.processEvents()
            self.assertEqual(overlay_bar.opacity(), 0.0)
            with TemporaryDirectory() as temporary_dir:
                output = Path(temporary_dir) / "gallery.png"
                save_snapshot(window, output)
                image = QImage(str(output))
                self.assertFalse(image.isNull())
                self.assertEqual(image.size(), window.size())
                self.assertEqual(image.pixelColor(0, 0).alpha(), 255)
                self.assertEqual(
                    image.pixelColor(image.width() // 2, image.height() // 2).alpha(),
                    255,
                )
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_snapshot_fills_logical_canvas_at_retina_dpr(self):
        physical = QImage(160, 80, QImage.Format_ARGB32_Premultiplied)
        physical.setDevicePixelRatio(2.0)
        physical.fill(QColor(210, 35, 45))

        image = _normalize_snapshot_capture(physical, QSize(80, 40))

        self.assertEqual(image.size(), QSize(80, 40))
        self.assertEqual(image.devicePixelRatioF(), 1.0)
        expected = QColor(210, 35, 45)
        self.assertEqual(image.pixelColor(0, 0), expected)
        self.assertEqual(image.pixelColor(79, 39), expected)


    def test_review_file_browser_search_uses_the_effective_theme(self):
        from fluentqt_gallery.native_samples import build_native_sample
        from fluentqt import _fluentqt as native
        from fluentqt.foundation import ThemeTokens
        original = fluentqt.current_theme()
        result = build_native_sample("list-detail-view", "list-detail-file-browser", None)
        panel = result.widget
        search = panel.findChild(fluentqt.LineEdit, "fileBrowserSearch")
        try:
            for theme in (fluentqt.Theme.Light, fluentqt.Theme.Dark, fluentqt.Theme.HighContrast):
                fluentqt.set_theme(theme)
                QApplication.processEvents()
                expected = QColor(panel.theme_tokens().colors.textSecondary)
                actual = QColor(ThemeTokens(native.themeTokensForWidgetForBinding(search)).colors.textTertiary)
                self.assertEqual(actual, expected)
        finally:
            fluentqt.set_theme(original)
            delete_qobject(panel)

    def test_review_full_source_entry_point_does_not_rewrite_literals(self):
        from fluentqt_gallery.samples import PreviewResult
        from fluentqt_gallery.native_source_files import source_files_for_sample
        widget = QWidget()
        source = ("import fluentqt\nfrom PySide6.QtWidgets import QWidget\n"
                  "root = QWidget()\ncaption = '    panel.show()'\n")
        result = PreviewResult(widget, source, ())
        files = source_files_for_sample("tag-input-project-members", result)
        self.assertEqual(len(files), 1)
        code = files[0]["code"]
        self.assertIn("caption = '    panel.show()'", code)
        self.assertIn('if __name__ == "__main__":\n    root.show()', code)
        compile(code, files[0]["name"], "exec")
        self.assertIsNone(source_files_for_sample("a-short-example", result))
        delete_qobject(widget)



class _GalleryTestResult(unittest.TextTestResult):
    def addError(self, test, err):
        super().addError(test, err)
        # A native teardown failure must not swallow the preceding Python error.
        traceback.print_exception(*err, file=self.stream)
        self.stream.flush()


class _GalleryTestRunner(unittest.TextTestRunner):
    resultclass = _GalleryTestResult


if __name__ == "__main__":
    unittest.main(testRunner=_GalleryTestRunner, verbosity=2)
