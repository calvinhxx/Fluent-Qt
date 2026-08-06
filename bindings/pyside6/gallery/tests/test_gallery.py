"""Exact native-parity tests for the standalone Python Gallery package."""

from __future__ import annotations

import ast
from collections.abc import Callable
import json
from pathlib import Path
import re
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest
import uuid

import fluentqt
import shiboken6

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import (
    QCoreApplication,
    QElapsedTimer,
    QEvent,
    QEventLoop,
    QPoint,
    QPointF,
    QRect,
    QRectF,
    QSize,
    Qt,
    QTimer,
    QUrl,
)
from PySide6.QtGui import (
    QColor,
    QFont,
    QHelpEvent,
    QIcon,
    QImage,
    QMouseEvent,
    QPalette,
    QPainter,
    QPixmap,
)
from PySide6.QtTest import QTest
from PySide6.QtWidgets import (
    QApplication,
    QFrame,
    QHBoxLayout,
    QLabel,
    QSizePolicy,
    QVBoxLayout,
    QWidget,
)

from fluentqt_gallery.app import (
    _normalize_snapshot_capture,
    _pending_gallery_network_requests,
    _reset_current_page_scroll,
    normalize_route,
    runtime_catalog_errors,
    save_snapshot,
)
from fluentqt_gallery.application_controller import (
    CloseBehaviorPromptContent,
    GalleryApplicationController,
    keep_running_choice,
)
from fluentqt_gallery.catalog import (
    CATEGORIES,
    ENTRIES,
    ENTRY_BY_ROUTE_ID,
    ROUTES,
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
    _acrylic_noise_tile,
    _direct_icon_font,
    _direct_icon_glyph,
    _draw_pixmap_in_logical_rect,
    _hero_link_pixmap,
    _macos_dock_icon_pixmap,
    _qt_seeded_bytes,
    _single_shot,
    _tint_github_mark,
    app_icon,
    app_icon_pixmap,
    css_color,
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
    _refresh_fluent_subtree,
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
EXPECTED_SUPPORT_TYPES = frozenset(
    {
        "AnchorLayout",
        "AnchorSpec",
        "AnnotatedScrollBarLabel",
        "BreadcrumbItem",
        "CornerRadius",
        "EditingCommandRouter",
        "FluentMenuItem",
        "FluentWidget",
        "Icons",
        "IconSize",
        "PivotItem",
        "ScrollViewZoomAwareWidget",
        "SelectorBarItem",
        "SplitViewPaneOptions",
        "Spacing",
        "StackContentHost",
        "StateGroup",
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
        "toString",
        "value",
        "valueToKey",
        "width",
        "window",
    }
)
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

# PySide 6.2 can release the Python QApplication wrapper during long-running
# test methods unless the module keeps an explicit strong reference.
_TEST_APPLICATION: QApplication | None = None


def _contract_sample_keys() -> frozenset[tuple[str, str]]:
    return frozenset(
        (entry.route_id, sample.id)
        for entry in ENTRIES
        for sample in entry.samples
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
    @classmethod
    def setUpClass(cls):
        global _TEST_APPLICATION
        _TEST_APPLICATION = QApplication.instance() or QApplication([])
        cls.app = _TEST_APPLICATION
        cls.app.setProperty("fluentqtGalleryAutomated", True)
        if not fluentqt.initialize_resources():
            raise RuntimeError("FluentQt resources could not be initialized")
        cls.app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    def tearDown(self):
        QApplication.processEvents()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QApplication.processEvents()

    def test_contract_exactly_matches_the_public_binding(self):
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        self.assertEqual(len(manifest["classes"]), 87)
        self.assertEqual(catalog_coverage_errors(manifest["classes"]), [])
        self.assertEqual(runtime_catalog_errors(), [])
        self.assertEqual(len(ROUTES), 88)
        self.assertEqual(len(ENTRIES), 67)
        self.assertEqual(len(CATEGORIES), 12)
        self.assertEqual(
            sum(len(entry.samples) for entry in ENTRIES),
            199,
        )
        self.assertEqual(len({route.id for route in ROUTES}), 88)
        self.assertEqual(len({entry.route_id for entry in ENTRIES}), 67)

    def test_support_types_are_explicit_and_embedded_in_real_samples(self):
        self.assertEqual(SUPPORT_TYPES, EXPECTED_SUPPORT_TYPES)
        routed_types = {entry.name for entry in ENTRIES}
        self.assertTrue(routed_types.isdisjoint(SUPPORT_TYPES))
        self.assertEqual(len(routed_types | set(SUPPORT_TYPES)), 87)
        for entry in ENTRIES:
            self.assertFalse(entry.support_type)

    def test_route_normalization_uses_native_route_ids(self):
        self.assertEqual(normalize_route("Button"), "button")
        self.assertEqual(normalize_route("button"), "button")
        self.assertEqual(normalize_route("basic-input"), "basic-input")
        self.assertEqual(normalize_route("home"), "home")
        self.assertEqual(normalize_route("component/Button"), "button")
        self.assertEqual(normalize_route("category/basic-input"), "basic-input")

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
                int(output[0]), int(StartResult.ExistingInstanceNotified)
            )
            self.assertEqual(output[1:] or [""], [""])
            self.assertEqual(activated, [True])
        finally:
            if worker.poll() is None:
                worker.kill()
                worker.wait(timeout=1.0)
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
        self.assertEqual(len(expected), 199)
        self.assertEqual(ported_sample_keys(), expected)

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
            tree.close()
            tree.deleteLater()
            QApplication.processEvents()

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
                        if root.effective_theme() == fluentqt.Theme.Dark
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
        self.assertEqual(len(grouped_samples), 72)
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
            self.assertIn("scrim.setAlpha(0x24)", result.preview_source)
            self.assertIn(
                "2.5 if material_grid else 2.0", result.preview_source
            )
            self.assertIn('QFont("FluentQt Icons")', result.preview_source)
            self.assertIn("\ue73e", result.preview_source)
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
            self.assertIn(
                "flip_view.currentIndexChanged.connect(update_status)",
                result.source,
            )
            self.assertIn(
                "def update_status(index, label=status):",
                result.preview_source,
            )
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
            self.assertIn("page = QLabel(flip_view)", result.preview_source)
            self.assertEqual(result.widget.pageCount(), 3)
            self.assertEqual(result.source.count("addOwnedPage("), 3)
            self.assertIn("addOwnedPage(sunrise_photo)", result.source)
            self.assertIn("addOwnedPage(ocean_photo)", result.source)
            self.assertIn("addOwnedPage(forest_photo)", result.source)
        finally:
            dispose(result)

        result = build_sample("flip-view", "flip-view-vertical")
        try:
            self.assertEqual(result.widget.pageCount(), 3)
            self.assertEqual(
                result.widget.orientation(), Qt.Orientation.Vertical
            )
            self.assertIn(
                "flip_view.setOrientation(Qt.Orientation.Vertical)",
                result.source,
            )
            self.assertEqual(result.source.count("addOwnedPage("), 3)
            self.assertIn("addOwnedPage(first_page)", result.source)
            self.assertIn("addOwnedPage(second_page)", result.source)
            self.assertIn("addOwnedPage(third_page)", result.source)
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
            self.assertEqual(
                code_label.sizePolicy().horizontalPolicy(),
                QSizePolicy.Policy.Ignored,
            )
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
                if fluentqt.current_theme() == fluentqt.Theme.Dark
                else "#795E26"
            )
            self.assertIn(expected_function, code_label.text())

            theme_button = page._gallery_theme_button
            initial_theme = page._gallery_sample_theme
            self.assertEqual(
                theme_button.property("gallerySampleTheme"),
                "Dark" if initial_theme == fluentqt.Theme.Dark else "Light",
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

    def test_code_block_does_not_render_terminal_newline_as_empty_row(self):
        source = (
            "button = fluentqt.RepeatButton('Click and hold')\n"
            "button.clicked.connect(increment)"
        )
        blocks = (
            GalleryCodeBlock(source),
            GalleryCodeBlock(source + "\n"),
        )
        try:
            heights = []
            for block in blocks:
                block.resize(760, block.height())
                block.setExpandedAnimated(True, False)
                block.show()
                QApplication.processEvents()
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
                heights.append((block.height(), label.height()))
                self.assertFalse(label.text().endswith("<br/></span>"))
                margins = content.layout().contentsMargins()
                self.assertEqual(
                    content.height() - label.geometry().bottom() - 1,
                    margins.bottom(),
                )
            self.assertEqual(heights[0], heights[1])
        finally:
            for block in blocks:
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
            expected_top = (
                TITLE_BAR_HEIGHT
                if sys.platform == "darwin"
                else 0
            )
            self.assertEqual(
                drawer.availableMargins(),
                drawer_title_bar_avoidance_margins(),
            )
            self.assertEqual(drawer.availableMargins().top(), expected_top)
            self.assertIn(
                "drawer_title_bar_avoidance_margins()",
                result.source,
            )
            self.assertNotIn("setAvailableMargins(QMargins())", result.source)

            drawer.setAnimationEnabled(False)
            drawer.open()
            QApplication.processEvents()
            self.assertEqual(drawer.geometry().top(), expected_top)
            if sys.platform == "darwin":
                self.assertGreater(drawer.geometry().top(), 0)
        finally:
            if drawer is not None:
                drawer.close()
            window.close()
            window.deleteLater()
            QApplication.processEvents()

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
        source = QPixmap(64, 64)
        source.fill(QColor("#0078D4"))
        padded = _macos_dock_icon_pixmap(source)
        self.assertEqual(padded.size(), QSize(256, 256))
        image = padded.toImage()
        self.assertEqual(image.pixelColor(14, 128).alpha(), 0)
        self.assertGreater(image.pixelColor(15, 128).alpha(), 0)
        self.assertGreater(image.pixelColor(239, 128).alpha(), 0)
        self.assertEqual(image.pixelColor(240, 128).alpha(), 0)

        icon = app_icon()
        self.assertFalse(icon.isNull())
        if sys.platform == "darwin":
            self.assertEqual(icon.actualSize(QSize(256, 256)), QSize(256, 256))

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

    def test_home_section_headers_follow_a_light_to_dark_theme_switch(self):
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
                if fluentqt.current_theme() == fluentqt.Theme.Dark
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
                if fluentqt.current_theme() == fluentqt.Theme.Dark
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
                                sample.cpp_snippet,
                            )
                        )
                        cpp_has_widget = bool(
                            re.search(r"\bQWidget\b", sample.cpp_snippet)
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
                                    sample.cpp_snippet,
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
                        displayed_semantic_statements = sum(
                            isinstance(node, ast.stmt)
                            and not isinstance(
                                node, (ast.Import, ast.ImportFrom)
                            )
                            for node in ast.walk(displayed_tree)
                        )
                        cpp_semantic_statements = max(
                            1, sample.cpp_snippet.count(";")
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
                        if not re.search(r"\bfor\s*\(", sample.cpp_snippet):
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
                            sample.cpp_snippet
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
                        cpp_line_count += len(sample.cpp_snippet.splitlines())
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
                        expected_design_tokens = {
                            "fluentqt.Typography.Icons.{0}".format(name)
                            for name in re.findall(
                                r"Typography::Icons::([A-Za-z0-9_]+)",
                                sample.cpp_snippet,
                            )
                        }
                        expected_design_tokens.update(
                            "fluentqt.Typography.IconSize.{0}".format(name)
                            for name in re.findall(
                                r"Typography::IconSize::([A-Za-z0-9_]+)",
                                sample.cpp_snippet,
                            )
                        )
                        expected_design_tokens.update(
                            "fluentqt.FontRole.{0}".format(name)
                            for name in re.findall(
                                r"Typography::FontRole::([A-Za-z0-9_]+)",
                                sample.cpp_snippet,
                            )
                        )
                        expected_design_tokens.update(
                            "fluentqt.Spacing.{0}".format(
                                name.replace("::", ".")
                            )
                            for name in re.findall(
                                r"Spacing::((?:[A-Za-z0-9_]+::)*"
                                r"[A-Za-z0-9_]+)",
                                sample.cpp_snippet,
                            )
                        )
                        expected_design_tokens.update(
                            "fluentqt.CornerRadius.{0}".format(
                                "None_" if name == "None" else name
                            )
                            for name in re.findall(
                                r"CornerRadius::([A-Za-z0-9_]+)",
                                sample.cpp_snippet,
                            )
                        )
                        for design_token in expected_design_tokens:
                            self.assertIn(
                                design_token,
                                result.source,
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
                                sample.cpp_snippet,
                            )
                        )
                        python_members = {
                            node.attr
                            for node in ast.walk(displayed_tree)
                            if isinstance(node, ast.Attribute)
                        }
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
                                sample.cpp_snippet,
                            )
                        )
                        python_connection_count = sum(
                            isinstance(node, ast.Call)
                            and isinstance(node.func, ast.Attribute)
                            and node.func.attr == "connect"
                            for node in ast.walk(displayed_tree)
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
                            sample.cpp_snippet,
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
                                for node in ast.walk(displayed_tree)
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
                            for node in ast.walk(displayed_tree)
                            if isinstance(node, ast.Constant)
                            and isinstance(node.value, str)
                            and node.value
                        }
                        for raw_literal in re.findall(
                            r'"((?:\\.|[^"\\])*)"',
                            sample.cpp_snippet,
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
                                or cpp_literal.startswith(":/")
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

    def test_window_builds_all_88_routes_and_199_sample_cards(self):
        window = GalleryWindow()
        window.show()
        QApplication.processEvents()
        try:
            self.assertEqual(window.all_route_ids(), tuple(route.id for route in ROUTES))
            self.assertEqual(window.visit_all_routes(), [])
            self.assertEqual(len(window._pages), 88)
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
            self.assertEqual(built_sample_count, 199)
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_all_built_routes_survive_theme_and_style_refresh_cycles(self):
        window = GalleryWindow(startup_visuals=False)
        original_theme = int(window._settings.theme_mode)
        original_style = int(window._settings.style_theme)
        window.show()
        QApplication.processEvents()
        try:
            self.assertEqual(window.visit_all_routes(), [])
            window.navigate("settings", record_history=False)
            QApplication.processEvents()
            button_generation = window._page_visual_generations["button"]
            window._set_style((original_style + 1) % 3)
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
            for style in (0, 1, 2):
                window._set_style(style)
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
            window._set_style(original_style)
            window._set_theme_mode(original_theme)
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_window_shell_matches_native_gallery_geometry(self):
        window = GalleryWindow()
        window.show()
        QApplication.processEvents()
        try:
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
            self.assertEqual(home._gallery_hero.height(), 390)
            self.assertEqual(
                home._gallery_hero._tagline.text(),
                "Interactive documentation for FluentQt: browse components, "
                "run live examples, and inspect focused API usage.",
            )
            self.assertEqual(len(home._gallery_featured_grid.cards), 9)
            self.assertEqual(home._gallery_featured_grid.columns, 3)
            self.assertEqual(len(home._gallery_category_grid.cards), 13)
        finally:
            window.close()
            window.deleteLater()
            QApplication.processEvents()

    def test_opt_in_startup_splash_matches_native_chrome_handoff(self):
        window = GalleryWindow(startup_visuals=True)
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
                    fluentqt.ProgressRing, "gallerySplashSpinner"
                )
            )
            self.assertTrue(window._menu_button.isHidden())
            window._prewarm_paused = True
            window.show()
            QApplication.processEvents()
            self.assertTrue(splash.isVisible())
            self.assertEqual(splash.geometry(), window.contentHost().rect())
            window._prewarm_queue.clear()
            window._prewarm_paused = False
            window._prewarm_next_route()
            _qwait(500)
            self.assertIsNone(window._splash)
            self.assertTrue(window._menu_button.isVisible())
        finally:
            window.close()
            window.deleteLater()
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

            _qwait(120)
            self.assertIn("button", window._pages)
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
            self.assertFalse(window._menu_button.isEnabled())
            self.assertEqual(window._content_host.geometry().top(), 48)
            self.assertEqual(
                window._top_main_navigation_pane.sizeHint().height(), 48
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
            rows = window._top_main_navigation_pane._child_flyout_panel.findChildren(
                QWidget, options=Qt.FindDirectChildrenOnly
            )
            self.assertEqual(len(rows), 7)

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
            "foundation-qmlplus": ("Property binding", "States", "Anchors"),
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

            self.assertEqual(len(foundation_code_blocks), 5)
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

    def test_settings_page_matches_native_rows_and_choices(self):
        window = GalleryWindow()
        window.show()
        window.navigate("settings", animated=False)
        QApplication.processEvents()
        try:
            _index, page = window._pages["settings"]
            self.assertEqual(len(page._gallery_settings_rows), 6)
            labels = tuple(label.text() for label in page.findChildren(fluentqt.Label))
            for text in (
                "Appearance & behavior",
                "App theme",
                "Style & accent color",
                "Navigation style",
                "Window background effect",
                "App behavior",
                "Close button behavior",
                "Updates",
                "Gallery updates",
            ):
                self.assertIn(text, labels)
            expected_choices = (
                ("Use system setting", "Light", "Dark"),
                ("Fluent (Windows)", "Material 3 (Google)", "macOS"),
                ("Left", "Top"),
                ("Normal", "Mica", "Acrylic"),
                ("Minimize window", "Keep in system tray", "Quit app"),
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


if __name__ == "__main__":
    unittest.main()
