"""Exact native-parity tests for the wheel-installed Python Gallery."""

from __future__ import annotations

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

from fluentqt.gallery.app import (
    _pending_gallery_network_requests,
    _reset_current_page_scroll,
    normalize_route,
    runtime_catalog_errors,
    save_snapshot,
)
from fluentqt.gallery.application_controller import (
    CloseBehaviorPromptContent,
    GalleryApplicationController,
    keep_running_choice,
)
from fluentqt.gallery.catalog import (
    CATEGORIES,
    ENTRIES,
    ENTRY_BY_ROUTE_ID,
    ROUTES,
    SAMPLE_BY_KEY,
    SUPPORT_TYPES,
    catalog_coverage_errors,
    entries_for_category,
)
from fluentqt.gallery.foundation_pages import (
    GalleryIconBrowser,
    TypographyRampCard,
    _catalog_glyphs_for_size,
    _icon_font,
    _load_icon_catalog,
)
from fluentqt.gallery.native_samples import ported_sample_keys
from fluentqt.gallery.samples import build_sample
from fluentqt.gallery.visual import (
    _direct_icon_font,
    _direct_icon_glyph,
    _draw_pixmap_in_logical_rect,
    _single_shot,
    css_color,
    gallery_colors,
)
from fluentqt.gallery.update_checker import (
    UpdateResult,
    UpdateStatus,
    compare_versions,
)
from fluentqt.gallery.window import GalleryWindow
from fluentqt.gallery.window_placement import (
    constrain_geometry,
    effective_minimum_size,
    recommended_initial_size,
    restored_geometry,
)
from fluentqt.gallery.settings import CloseBehavior
from fluentqt.gallery.single_instance import (
    GallerySingleInstance,
    StartResult,
)


MANIFEST_PATH = Path(__file__).resolve().parents[1] / "api-manifest.json"
EXPECTED_SUPPORT_TYPES = frozenset(
    {
        "AnnotatedScrollBarLabel",
        "BreadcrumbItem",
        "EditingCommandRouter",
        "FluentMenuItem",
        "PivotItem",
        "ScrollViewZoomAwareWidget",
        "SelectorBarItem",
        "SplitViewPaneOptions",
        "StackContentHost",
        "TabViewItem",
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
        self.assertEqual(len(manifest["classes"]), 77)
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
        self.assertEqual(len(routed_types | set(SUPPORT_TYPES)), 77)
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
            Path(__file__).resolve().parents[3]
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
            _qwait(300)
            self.assertTrue(window.isChromeInteractive())
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
from fluentqt.gallery.single_instance import GallerySingleInstance
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

    def test_gallery_app_defers_heavy_ui_imports_until_primary_lock(self):
        probe = """
import json
import sys
import fluentqt.gallery.app
heavy_modules = (
    "fluentqt.gallery.catalog",
    "fluentqt.gallery.native_samples",
    "fluentqt.gallery.window",
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
            self.assertEqual(scroll.contentWidget().size(), QSize(360, 7560))
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
            self.assertEqual(captions, ("16 px", "20 px", "24 px", "32 px"))
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
            collapsed_height = card.height()
            code.setExpandedAnimated(True, False)
            QApplication.processEvents()
            card._update_anchored_layout()
            self.assertGreater(card.height(), collapsed_height)
            self.assertIn('<span style="color:', code_label.text())
            self.assertNotIn("<pre", code_label.text())
            expected_keyword = (
                "#569CD6"
                if fluentqt.current_theme() == fluentqt.Theme.Dark
                else "#0000FF"
            )
            self.assertIn(expected_keyword, code_label.text())

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

    def test_content_labels_match_native_tracked_style_sheet_contract(self):
        window = GalleryWindow(startup_visuals=False)
        window.show()
        window.navigate("button", animated=False)
        _qwait(50)
        try:
            _index, page = window._pages["button"]
            colors = gallery_colors()
            expected_primary = "color: {0}; background: transparent;".format(
                css_color(colors.text_primary)
            )
            expected_secondary = "color: {0}; background: transparent;".format(
                css_color(colors.text_secondary)
            )

            title = page.findChild(fluentqt.Label, "galleryContentTitle")
            section = page.findChild(
                fluentqt.Label, "galleryContentSectionHeader"
            )
            body = page.findChild(fluentqt.Label, "galleryContentBody")
            self.assertIsNotNone(title)
            self.assertIsNotNone(section)
            self.assertIsNotNone(body)
            self.assertEqual(title.styleSheet(), expected_primary)
            self.assertEqual(section.styleSheet(), expected_primary)
            self.assertEqual(body.styleSheet(), "")
            self.assertEqual(
                body.palette().color(QPalette.ColorRole.WindowText),
                colors.text_primary,
            )
            self.assertEqual(
                body.textColorRole(), fluentqt.Label.TextColorRole.Default
            )

            page.onThemeUpdated()
            self.assertEqual(body.styleSheet(), expected_secondary)

            window.navigate("basic-input", animated=False)
            _qwait(50)
            _index, category_page = window._pages["basic-input"]
            subtitle = category_page.findChild(
                fluentqt.Label, "galleryContentSubtitle"
            )
            self.assertIsNotNone(subtitle)
            self.assertEqual(subtitle.styleSheet(), expected_secondary)
            self.assertEqual(
                subtitle.textColorRole(), fluentqt.Label.TextColorRole.Default
            )
        finally:
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
        displayed_line_count = 0
        cpp_line_count = 0
        maximum_displayed_lines = 0
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
                        displayed_line_count += displayed_lines
                        cpp_line_count += len(sample.cpp_snippet.splitlines())
                        maximum_displayed_lines = max(
                            maximum_displayed_lines, displayed_lines
                        )
                        for cxx_token in (
                            "#include",
                            "QStringLiteral",
                            "fluent::",
                        ):
                            self.assertNotIn(cxx_token, result.source)
                        self.assertNotIn(
                            "from fluentqt.gallery", result.source
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
                        python_methods = set(
                            re.findall(
                                r"\.([A-Za-z_][A-Za-z0-9_]*)\s*\(",
                                result.source,
                            )
                        )
                        self.assertTrue(
                            cpp_methods & python_methods
                            or any(
                                "fluentqt.{0}".format(covered)
                                in result.source
                                for covered in result.covered_types
                            ),
                            "{0}/{1} displayed Python no longer demonstrates "
                            "the canonical component API".format(
                                entry.route_id, sample.id
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
            displayed_line_count,
            int(cpp_line_count * 1.2),
            "Python teaching snippets have regressed toward app-internal "
            "preview implementations",
        )
        self.assertLessEqual(maximum_displayed_lines, 70)

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

            _qwait(300)
            self.assertEqual(window._back_button.width(), 24)
            self.assertAlmostEqual(
                window._back_button.contentOpacity(), 1.0, places=2
            )
            self.assertEqual(
                window._menu_button.x() - window._back_button.x(), 32
            )

            window.navigate_back()
            _qwait(300)
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

    def test_settings_navigation_icon_uses_native_rotation_feedback(self):
        window = GalleryWindow()
        window.show()
        QApplication.processEvents()
        try:
            footer = window._footer_navigation_pane
            footer._activate_index(footer._item.index())
            _qwait(40)
            self.assertEqual(window.current_route, "settings")
            self.assertGreater(footer._settings_icon_rotation, 0.0)
            _qwait(420)
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
                Path(__file__).resolve().parents[3]
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


if __name__ == "__main__":
    unittest.main()
