"""Optional Gallery integration, fallback and native compositor regression checks.

CTest covers the CPU-safe path. Run with a desktop Qt platform and
FLUENTQT_SPATIAL_EVIDENCE_DIR to exercise GPU input, overlays and screenshots.
"""
import json
import os
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

import fluentqt
from PySide6.QtCore import QAbstractAnimation, QEvent, QObject, QPoint, QPointF, QRect, QSettings, QSizeF, Qt
from PySide6.QtGui import QPainter, QPolygonF, QTransform
from PySide6.QtTest import QSignalSpy, QTest
from PySide6.QtWidgets import QApplication, QGraphicsView, QWidget
from shiboken6 import delete, isValid

from fluentqt_gallery.catalog import ENTRIES
from fluentqt_gallery.native_samples import build_native_sample
from fluentqt_gallery.settings import NavigationStyle, ThemeMode, gallery_settings
from fluentqt_gallery.spatial_support import SPATIAL_AVAILABLE
from fluentqt_gallery.window import GalleryWindow
import fluentqt_gallery.settings as settings_module


class GallerySpatialTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        cls.app.setProperty("fluentqtGalleryAutomated", True)
        fluentqt.initialize_resources()
        cls.app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    def setUp(self):
        self.settings = gallery_settings()
        self.settings.set_theme_mode(ThemeMode.Light)
        self.settings.set_motion_mode(fluentqt.MotionMode.Full)
        self.settings.set_spatial_mode_enabled(False)
        self.settings.set_navigation_style(NavigationStyle.Left)
        self.settings.set_intro_completed(True)
        self.settings.set_home_particles_enabled(False)
        self.errors = []
        self.old_hook = sys.excepthook
        sys.excepthook = lambda *error: self.errors.append(error)

    def tearDown(self):
        self.settings.set_spatial_mode_enabled(False)
        QApplication.processEvents()
        sys.excepthook = self.old_hook
        self.assertEqual(self.errors, [], "Unhandled Qt callback error")

    def test_spatial_defaults_respect_module_saved_choice_and_accessibility(self):
        cases = ((None, 0, 1), (False, 0, 1), (True, 0, 1),
                 (True, 1, 1), (True, 2, 1), (True, 0, 3))
        with TemporaryDirectory() as directory:
            path = Path(directory) / "config.ini"
            with (patch.object(settings_module, "persistence_available", return_value=True),
                  patch.object(settings_module, "config_file_path", return_value=path)):
                for available in (False, True):
                    for saved, motion, theme in cases:
                        with self.subTest(available=available, saved=saved, motion=motion, theme=theme):
                            storage = QSettings(str(path), QSettings.IniFormat)
                            storage.clear()
                            storage.setValue("settings/motionMode", motion)
                            storage.setValue("settings/themeMode", theme)
                            if saved is not None:
                                storage.setValue("settings/spatialModeEnabled", saved)
                            storage.sync()
                            with patch.object(settings_module, "SPATIAL_AVAILABLE", available):
                                settings = settings_module.GallerySettings()
                            try:
                                expected = (available if saved is None else saved) and motion == 0 and theme != 3
                                self.assertEqual(settings.spatial_mode_enabled, expected)
                                self.assertEqual(storage.contains("settings/spatialModeEnabled"), saved is not None)
                                if expected:
                                    settings.set_spatial_mode_enabled(False)
                                    storage.sync()
                                    self.assertFalse(storage.value("settings/spatialModeEnabled", type=bool))
                            finally:
                                delete(settings)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_cache_budget_preserves_native_density(self):
        from fluentqt_gallery.spatial_controller import _cache_plan, _CACHE_BUDGET_BYTES, _PAINT_SAMPLES
        normal = _cache_plan([QSizeF(240, 700), QSizeF(960, 700)], 2, 16384)
        self.assertEqual(normal[0], 4)
        panels = [QSizeF(240, 900), QSizeF(1360, 900)]
        large = _cache_plan(panels, 2, 16384)
        self.assertGreaterEqual(large[0], 2)
        self.assertEqual(large[0], 4)
        paint_width, paint_height = large[2].width(), large[2].height()
        self.assertLess(paint_height, large[1][1].height())
        self.assertLessEqual(sum(s.width() * s.height() for s in large[1]) * 4
                             + paint_width * paint_height * (12 * _PAINT_SAMPLES + 4),
                             _CACHE_BUDGET_BYTES)
        limited = _cache_plan(panels, 2, 4096)
        self.assertTrue(all(s.width() <= 4096 and s.height() <= 4096 for s in limited[1]))
        self.assertIsNone(_cache_plan(panels, 2, 2048))
        self.assertIsNone(_cache_plan(panels, 2, 16384, budget=1024))
        self.assertIsNone(_cache_plan(panels, 0, 16384))
        self.assertIsNone(_cache_plan([QSizeF(1e20, 900), QSizeF()], 2, 16384))
        self.assertEqual(_cache_plan(panels, 1.25, 4096, max_extra=1.5)[0], 1.875)

    def test_gallery_without_spatial_does_not_load_opengl(self):
        # Model an installed 2D binding: the optional module is absent.
        code = '''
import importlib.util, json, sys
original = importlib.util.find_spec
importlib.util.find_spec = lambda name, *a, **k: None if name == "fluentqt.spatial" else original(name, *a, **k)
from PySide6.QtWidgets import QApplication
import fluentqt
app = QApplication([])
fluentqt.initialize_resources()
from fluentqt_gallery.window import GalleryWindow
from fluentqt_gallery.catalog import ENTRIES, ROUTES
window = GalleryWindow(startup_visuals=False)
window.navigate("settings", animated=False)
assert window._spatial_controller is None
assert not window._settings.spatial_available
assert not window._settings.spatial_mode_enabled
assert (len(ENTRIES), len(ROUTES)) == (83, 105)
assert "PySide6.QtOpenGLWidgets" not in sys.modules
assert "PySide6.QtOpenGL" not in sys.modules
assert "fluentqt.spatial" not in sys.modules
print("2D-only Gallery: no OpenGL imports")
'''
        result = subprocess.run([sys.executable, "-c", code], env={**os.environ, "QT_QPA_PLATFORM": "offscreen"}, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_explicit_fallback_keeps_native_ui_and_badges(self):
        with patch.dict(os.environ, {"FLUENT_QT_GALLERY_DISABLE_3D": "1"}):
            window = GalleryWindow(startup_visuals=False)
        try:
            window.navigate("settings", animated=False)
            self.assertIsNone(window._spatial_controller.canvas)
            toggle = window.findChild(fluentqt.ToggleSwitch, "gallerySettingsSpatialModeToggle")
            self.assertFalse(toggle.isEnabled())
            self.settings.set_spatial_mode_enabled(True)
            self.assertFalse(toggle.isOn())
            for name in ("gallerySettingsSpatialSupportBadge", "galleryNavigationSpatialSupportBadge"):
                badge = window.findChild(QWidget, name)
                self.assertIn("3D is unavailable", badge.accessibleDescription())
                self.assertEqual(badge.badge.status(), fluentqt.InfoBadge.InfoBadgeStatus.Critical)
        finally:
            delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_all_samples_follow_one_setting_and_keep_widget_state(self):
        host = QWidget()
        self.settings.set_spatial_availability(True)
        results = []
        try:
            for entry in ENTRIES:
                if entry.category_id != "spatial":
                    continue
                for sample in entry.samples:
                    result = build_native_sample(entry.route_id, sample.id, host)
                    results.append(result)
                    self.assertFalse(result.widget._spatial_binding.view.isSpatialEnabled())
            self.assertEqual(len(results), 11)
            level = host.findChild(fluentqt.Slider, "spatialLevelSlider")
            level.setValue(73)
            self.settings.set_spatial_mode_enabled(True)
            self.assertTrue(all(r.widget._spatial_binding.view.isSpatialEnabled() for r in results))
            self.settings.set_spatial_mode_enabled(False)
            self.assertTrue(all(not r.widget._spatial_binding.view.isSpatialEnabled() for r in results))
            self.assertEqual(level.value(), 73)
            self.settings.set_spatial_mode_enabled(True)
            results[0].widget._spatial_binding.view.setSpatialEnabled(False)
            self.assertFalse(self.settings.spatial_mode_enabled)
            self.assertTrue(all(not r.widget._spatial_binding.view.isSpatialEnabled() for r in results))
            self.settings.set_spatial_mode_enabled(True)
            fluentqt.set_motion_mode(fluentqt.MotionMode.Reduced)
            self.assertFalse(self.settings.spatial_mode_enabled)
            self.assertTrue(all(not r.widget._spatial_binding.view.isSpatialEnabled() for r in results))
        finally:
            delete(host)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_chart_is_outside_more_examples_and_updates_its_model(self):
        window = GalleryWindow(startup_visuals=False)
        try:
            window.navigate("spatial-view", animated=False)
            page = window._pages["spatial-view"][1]
            more = page.findChild(fluentqt.Expander, "galleryMoreSpatialExamples")
            self.assertFalse(more.isExpanded())
            chart = page.findChild(fluentqt.DonutChart, "spatialAllocationChart")
            self.assertIsNotNone(chart)
            self.assertFalse(more.isAncestorOf(chart))
            slider = page.findChild(fluentqt.Slider, "spatialAllocationSlider")
            slider.setValue(72)
            self.assertEqual(chart.centerText(), "72%")
        finally:
            delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_2d_startup_defers_gpu_probe_until_requested(self):
        with patch("fluentqt_gallery.spatial_controller._session_unavailable_reason", return_value=""), \
             patch("fluentqt_gallery.spatial_controller._unavailable_reason", return_value="Simulated unavailable GPU") as probe:
            window = GalleryWindow(startup_visuals=False)
            try:
                window.show()
                window.resize(960, 720)
                window.navigate("settings", animated=False)
                self.settings.set_theme_mode(ThemeMode.Dark)
                QTest.qWait(100)
                controller = window._spatial_controller
                probe.assert_not_called()
                self.assertIsNone(controller.canvas)
                self.assertIsNone(controller.capture)
                self.assertFalse(controller.filtering)
                toggle = window.findChild(fluentqt.ToggleSwitch, "gallerySettingsSpatialModeToggle")
                self.assertTrue(toggle.isEnabled())
                QTest.mouseClick(toggle, Qt.LeftButton, pos=QPoint(20, 16))
                probe.assert_called_once()
                self.assertFalse(toggle.isEnabled())
                self.assertFalse(toggle.isOn())
                self.assertIsNone(controller.canvas)
                self.assertTrue(self.settings.spatial_mode_enabled)
            finally:
                delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_deferred_surface_waits_for_layout_and_can_be_cancelled(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires native deferred OpenGL initialization")
        from fluentqt_gallery.spatial_controller import GallerySpatialController

        for cancel_while_pending in (False, True):
            with self.subTest(cancel_while_pending=cancel_while_pending):
                window = fluentqt.Window()
                window._splash = window._dismissal_splash = None
                window.resize(800, 600)
                navigation = fluentqt.NavigationView(window)
                navigation.setMinimumSize(0, 0)
                navigation.resize(0, 0)
                controller = GallerySpatialController(window, navigation)
                try:
                    window.show()
                    self.assertTrue(QTest.qWaitForWindowExposed(window))
                    self.settings.set_spatial_mode_enabled(True)
                    surface = controller.canvas
                    self.assertIsNotNone(surface)
                    self.assertFalse(surface.isValid())
                    QTest.qWait(100)
                    self.assertFalse(controller.renderer_failed)
                    self.assertTrue(self.settings.spatial_availability_pending)
                    self.assertTrue(self.settings.spatial_mode_enabled)
                    self.assertIsNone(navigation.graphicsEffect())
                    if cancel_while_pending:
                        self.settings.set_spatial_mode_enabled(False)
                        self.assertTrue(surface.isHidden())
                    navigation.resize(window.size())
                    if cancel_while_pending:
                        QTest.qWait(100)
                        self.assertFalse(surface.isValid())
                        self.assertIsNone(navigation.graphicsEffect())
                        self.settings.set_spatial_mode_enabled(True)
                    for _ in range(60):
                        if surface.property("presenting"):
                            break
                        QTest.qWait(50)
                    self.assertTrue(self.settings.spatial_available)
                    self.assertTrue(surface.isValid())
                    self.assertTrue(surface.property("presenting"))
                    self.assertFalse(controller.renderer_timeout.isActive())
                    self.settings.set_spatial_mode_enabled(False)
                    QTest.qWait(500)
                finally:
                    delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_hidden_surface_revalidates_after_reparenting(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires native OpenGL context recreation")
        from fluentqt_gallery.spatial_controller import GallerySpatialController

        desktop = QWidget()
        window = fluentqt.Window()
        window._splash = window._dismissal_splash = None
        window.resize(800, 600)
        navigation = fluentqt.NavigationView(window)
        navigation.resize(window.size())
        controller = GallerySpatialController(window, navigation)
        try:
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(window))
            self.settings.set_spatial_mode_enabled(True)
            QTest.qWait(600)
            surface = controller.canvas
            self.assertTrue(surface.property("presenting"))
            self.settings.set_spatial_mode_enabled(False)
            QTest.qWait(600)
            self.assertTrue(surface.isHidden())
            window.setParent(desktop, Qt.Widget)
            desktop.resize(800, 600)
            desktop.show()
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(desktop))
            self.settings.set_spatial_mode_enabled(True)
            for _ in range(60):
                if surface.property("presenting"):
                    break
                QTest.qWait(50)
            self.assertTrue(surface.property("presenting"))
            self.assertTrue(surface.isValid())
            self.assertTrue(self.settings.spatial_available)
            self.assertIs(controller.canvas, surface)
        finally:
            delete(window)
            delete(desktop)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_missing_initialization_callback_falls_back_after_waiting(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires an exposed native window")
        from fluentqt_gallery.spatial_controller import GallerySpatialController

        class BlockSurfaceInitialization(QObject):
            def eventFilter(self, obj, event):
                return (obj.objectName() == "gallerySpatialSurface"
                        and event.type() in (QEvent.Show, QEvent.Resize, QEvent.Paint))

        window = fluentqt.Window()
        window._splash = window._dismissal_splash = None
        window.resize(800, 600)
        navigation = fluentqt.NavigationView(window)
        navigation.resize(window.size())
        controller = GallerySpatialController(window, navigation)
        blocker = BlockSurfaceInitialization()
        try:
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(window))
            self.app.installEventFilter(blocker)
            self.settings.set_spatial_mode_enabled(True)
            self.assertIsNotNone(controller.canvas)
            self.assertFalse(controller.canvas.isValid())
            QTest.qWait(100)
            self.assertTrue(self.settings.spatial_availability_pending)
            self.assertIsNone(navigation.graphicsEffect())
            for _ in range(130):
                if not self.settings.spatial_availability_pending:
                    break
                QTest.qWait(50)
            self.assertFalse(self.settings.spatial_availability_pending)
            self.assertFalse(self.settings.spatial_available)
            self.assertTrue(controller.renderer_failed)
            self.assertIsNone(controller.canvas)
            self.assertIsNone(navigation.graphicsEffect())
            self.assertIsNone(navigation.contentHost().graphicsEffect())
        finally:
            self.app.removeEventFilter(blocker)
            delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_cache_resource_failure_can_retry(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires an exposed native window")
        window = GalleryWindow(startup_visuals=False)
        try:
            window.resize(1000, 700)
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(window))
            controller = window._spatial_controller
            with patch("fluentqt_gallery.spatial_controller._cache_plan", return_value=None):
                self.settings.set_spatial_mode_enabled(True)
                for _ in range(60):
                    if not self.settings.spatial_mode_enabled:
                        break
                    QTest.qWait(50)
                self.assertFalse(self.settings.spatial_mode_enabled)
                self.assertTrue(self.settings.spatial_available)
                self.assertFalse(controller.renderer_failed)
            surface = controller.canvas
            self.settings.set_spatial_mode_enabled(True)
            for _ in range(60):
                if surface.property("presenting"):
                    break
                QTest.qWait(50)
            controller.settle()
            QTest.qWait(100)
            self.assertTrue(surface.property("presenting"))
            self.assertIs(controller.canvas, surface)
            self.assertTrue(all(cache for cache in surface.caches))
        finally:
            delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_gpu_filters_projected_high_dpi_detail(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires native GPU texture sampling")
        from fluentqt_gallery.spatial_controller import GallerySpatialController

        class DetailContent(QWidget):
            def paintEvent(self, event):
                painter = QPainter(self)
                painter.fillRect(self.rect(), Qt.white)
                for x in range(40, 180, 4):
                    painter.fillRect(QRect(x, 180, 1, 40), Qt.black)
                painter.end()

        window = fluentqt.Window()
        window._splash = window._dismissal_splash = None
        window.resize(900, 500)
        navigation = fluentqt.NavigationView(window)
        navigation.resize(window.size())
        content = DetailContent()
        navigation.contentHost().insertPage(0, content)
        navigation.contentHost().setCurrentIndex(0, 0, False)
        controller = GallerySpatialController(window, navigation)
        try:
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(window))
            self.settings.set_spatial_mode_enabled(True)
            for _ in range(60):
                if controller.canvas.property("presenting"):
                    break
                QTest.qWait(50)
            self.assertTrue(controller.canvas.property("presenting"))
            controller.settle()
            QTest.qWait(120)
            surface = controller.canvas
            self.assertGreater(surface.paint_target.format().samples(), 1)
            image = surface.grabFramebuffer()
            dpr = surface.devicePixelRatioF()
            a = surface.mapFrom(window, controller.projected_position(content, QPoint(50, 190)))
            b = surface.mapFrom(window, controller.projected_position(content, QPoint(168, 208)))
            pixels = QRect(a * dpr, b * dpr)
            filtered = sum(20 < image.pixelColor(x, y).red() < 235
                           for y in range(pixels.top(), pixels.bottom())
                           for x in range(pixels.left(), pixels.right()))
            self.assertGreater(filtered, pixels.width() * pixels.height() * .08,
                               "Projected thin strokes must retain filtered coverage")
        finally:
            delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_overlays_block_projected_home_links(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires native window hit testing and GPU composition")
        from fluentqt_gallery.intro_tour import GalleryIntroTour, TourStep
        from fluentqt_gallery.visual import GalleryHeroLinkCard

        for spatial in (False, True):
            with self.subTest(spatial=spatial), patch(
                "fluentqt_gallery.visual.QDesktopServices.openUrl", return_value=True
            ) as open_url:
                self.settings.set_spatial_mode_enabled(spatial)
                window = GalleryWindow(startup_visuals=False)
                tour = None
                try:
                    window.resize(1200, 850)
                    window.show()
                    QTest.qWait(600)
                    controller = window._spatial_controller
                    if spatial:
                        self.assertTrue(controller.canvas.property("presenting"))
                        controller.settle()
                    link = window.findChild(GalleryHeroLinkCard)
                    self.assertIsNotNone(link)
                    point = controller.projected_position(link, QPoint(24, 36))

                    def click(native=True, position=point):
                        QTest.mouseClick(window.windowHandle() if native else window,
                                         Qt.LeftButton, pos=position)

                    click()
                    self.assertEqual(open_url.call_count, 1)
                    open_url.reset_mock()
                    dialog = fluentqt.ContentDialog(window)
                    dialog.setAnimationEnabled(False)
                    dialog.setTitle("Close behavior")
                    dialog.setContent(fluentqt.Label("Choose how to close Gallery."))
                    dialog.setCloseButtonText("Cancel")
                    dialog.open()
                    scrim = window.findChild(QWidget, "DialogSmokeScrim")
                    self.assertTrue(scrim.isVisible())
                    self.assertFalse(scrim.testAttribute(Qt.WA_TransparentForMouseEvents))
                    self.assertFalse(dialog.geometry().contains(point))
                    click()
                    click(native=False)
                    open_url.assert_not_called()
                    dialog.setModal(False)
                    click()
                    self.assertEqual(open_url.call_count, 1, "Dim-only scrims retain modeless input")
                    dialog.done(fluentqt.ContentDialog.ResultNone)
                    open_url.reset_mock()

                    target = window.findChild(QWidget, "galleryMainNavigationPane")
                    self.assertIsNotNone(target)
                    tour = GalleryIntroTour(window)
                    tour.set_steps([TourStep(target, "", "Browse by category", "Explore the controls.",
                                             fluentqt.CoachMark.Placement.Right)])
                    tour.start()
                    QTest.qWait(350)
                    card = window.findChild(fluentqt.CoachMark)
                    title = next(label for label in card.findChildren(fluentqt.Label)
                                 if label.text() == "Browse by category")
                    card.move(card.pos() + point - title.mapTo(window, title.rect().center()))
                    click()
                    click(native=False)
                    open_url.assert_not_called()
                    next_button = next(button for button in card.findChildren(fluentqt.Button)
                                       if button.text() == "Finish")
                    finished = QSignalSpy(tour.finished)
                    click(position=next_button.mapTo(window, next_button.rect().center()))
                    self.assertEqual(finished.count(), 1)
                    QTest.qWait(350)
                    click()
                    self.assertEqual(open_url.call_count, 1, "Closing overlays restores links")
                finally:
                    if tour is not None:
                        delete(tour)
                    delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_gpu_shell_input_overlay_and_scroll(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires a native desktop GPU; offscreen is not visual approval")
        window = GalleryWindow(startup_visuals=False)
        window.resize(1248, 820)
        window.show()
        window.raise_()
        window.activateWindow()
        QTest.qWait(500)
        controller = window._spatial_controller
        self.assertIsNone(controller.canvas)
        self.assertTrue(self.settings.spatial_availability_pending)
        evidence = Path(os.environ.get("FLUENTQT_SPATIAL_EVIDENCE_DIR", "build/spatial-validation/python-gallery"))
        evidence.mkdir(parents=True, exist_ok=True)

        def screenshot(name):
            QApplication.processEvents()
            QTest.qWait(120)
            self.assertTrue(window.grab().save(str(evidence / (name + ".png"))))

        def click(widget, point=None):
            point = point or widget.rect().center()
            presented = controller.projected_position(widget, point)
            QTest.mouseClick(window, Qt.LeftButton, pos=presented)
            QTest.qWait(160)

        try:
            window.navigate("settings", animated=False)
            # Grab the real client surface with an opaque Window backdrop. Native
            # Mica is composed outside QWidget::grab and would leave transparent gaps.
            effect = window.findChild(fluentqt.ComboBox, "gallerySettingsEffectChoice")
            effect.setCurrentIndex(0)
            theme = window.findChild(fluentqt.ComboBox, "gallerySettingsThemeChoice")
            navigation = window.findChild(fluentqt.ComboBox, "gallerySettingsNavigationChoice")
            toggle = window.findChild(fluentqt.ToggleSwitch, "gallerySettingsSpatialModeToggle")
            first_handle, first_id = window.windowHandle(), window.winId()
            first_geometry = window.geometry()
            visibility = QSignalSpy(first_handle.visibleChanged)
            QTest.mouseClick(toggle, Qt.LeftButton, pos=QPoint(20, 16))
            QTest.qWait(600)
            self.assertTrue(self.settings.spatial_available, self.settings.spatial_unavailable_reason)
            self.assertTrue(self.settings.spatial_mode_enabled)
            self.assertEqual(controller.progress, 1.)
            if QApplication.platformName() == "cocoa":
                self.assertIs(window.windowHandle(), first_handle)
                self.assertEqual(window.winId(), first_id)
                self.assertEqual(window.geometry(), first_geometry)
                self.assertEqual(visibility.count(), 0)
            self.assertTrue(all(cache.get("texture") and cache["texture"].isValid()
                                for cache in controller.canvas.caches))
            screenshot("settings-light")
            click(toggle, QPoint(20, 16))
            QTest.qWait(500)
            self.assertFalse(self.settings.spatial_mode_enabled)
            self.assertEqual(controller.progress, 0.)
            self.assertFalse(controller.filtering)
            self.assertTrue(controller.canvas.isHidden())
            self.assertEqual(controller.canvas.caches, [{}, {}])
            self.assertTrue(controller.backdrop.isNull())
            window.resize(1220, 820)
            self.settings.set_theme_mode(ThemeMode.Dark)
            QTest.qWait(150)
            self.assertTrue(controller.backdrop.isNull())
            self.settings.set_theme_mode(ThemeMode.Light)
            self.settings.set_spatial_mode_enabled(True)
            QTest.qWait(500)
            click(theme)
            popup = window.findChild(QWidget, "ComboBoxPopup")
            self.assertIsNotNone(popup, str([(w.objectName(), type(w).__name__, w.isVisible()) for w in QApplication.allWidgets() if "Popup" in w.objectName()]))
            self.assertTrue(popup.isVisible())
            screenshot("settings-dropdown")
            QTest.keyClick(popup, Qt.Key_Escape)
            QTest.qWait(250)
            self.assertFalse(popup.isVisible())
            navigation.setCurrentIndex(1)
            QTest.qWait(400)
            self.assertTrue(window._menu_button.isHidden())
            self.assertEqual(controller.canvas.property("galleryRotationAxis"), "X")
            theme.setCurrentIndex(2)
            QTest.qWait(500)
            screenshot("settings-top-dark")
            host = controller.navigation.contentHost()
            position = controller.projected_position(host, QPoint(20, 20))
            pixel = controller.canvas.mapFrom(window, position) * window.devicePixelRatioF()
            background = controller.canvas.grabFramebuffer().pixelColor(pixel)
            self.assertLess(max(background.red(), background.green(), background.blue()), 96,
                            "GPU capture must preserve the transparent dark-theme host")
            theme.setCurrentIndex(1)
            navigation.setCurrentIndex(0)
            window.navigate("spatial-view", animated=False)
            QTest.qWait(600)
            page = window._pages["spatial-view"][1]
            view = page._gallery_sample_results[1].widget._spatial_binding.view
            page.ensureWidgetVisible(view, 0, 24)
            QTest.qWait(300)
            screenshot("spatial-cards")
            item = view.items()[0]
            switch = item.widget().findChild(fluentqt.ToggleSwitch)
            canvas = view.findChild(QGraphicsView)
            local = switch.mapTo(item.widget(), QPoint(20, 16))
            quad = QPolygonF([item.projectedPolygon()[i] for i in range(4)])
            rect = item.widget().rect()
            source = QPolygonF([QPointF(0,0), QPointF(rect.width(),0), QPointF(rect.width(),rect.height()), QPointF(0,rect.height())])
            transform = QTransform()
            self.assertTrue(QTransform.quadToQuad(source, quad, transform))
            before = switch.isOn()
            click(canvas.viewport(), transform.map(QPointF(local)).toPoint())
            self.assertNotEqual(switch.isOn(), before)
            scroll = page.verticalScrollBar()
            saved = scroll.value()
            scroll.setValue(0)
            QTest.qWait(100)
            scroll.setValue(saved)
            QTest.qWait(300)
            screenshot("spatial-cards-return")
            window.navigate("spatial-item", animated=False)
            QTest.qWait(350)
            page = window._pages["spatial-item"][1]
            view = page._gallery_sample_results[0].widget._spatial_binding.view
            page.ensureWidgetVisible(view, 0, 16)
            QTest.qWait(300)
            screenshot("spatial-item")
            window.navigate("home", animated=False)
            QTest.qWait(250)
            window._maybe_start_intro_tour()
            QTest.qWait(400)
            tour = window._intro_tour
            self.assertLess(window.children().index(controller.canvas), window.children().index(tour._scrim))
            tour.go_to_step(2)
            QTest.qWait(400)
            screenshot("intro-mask")
            tour.finish_tour()
            QTest.qWait(350)
            window.resize(640, 720)
            window.navigate("settings", animated=False)
            QTest.qWait(500)
            screenshot("settings-narrow")
            self.assertEqual(controller.pointer_motion.state(), QAbstractAnimation.Stopped)
            effect.setCurrentIndex(1)
            QTest.qWait(250)
            self.assertEqual(self.settings.window_effect, 1)
            self.assertTrue(self.settings.spatial_mode_enabled)
            self.settings.set_motion_mode(fluentqt.MotionMode.Reduced)
            self.assertFalse(self.settings.spatial_mode_enabled)
            (evidence / "native.json").write_text(json.dumps({"renderer": controller.renderer_name, "platform": QApplication.platformName(), "sample_count": 11, "snapshot_backdrop": "Normal (opaque QWidget client capture)", "checks": ["toggle through projection", "native dropdown", "nested switch input", "scroll return", "top navigation", "dark theme", "intro stacking", "narrow layout", "Mica with 3D", "reduced motion"]}, indent=2) + "\n")
        finally:
            delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_startup_keeps_splash_before_3d(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires a native desktop GPU")
        old_effect = self.settings.window_effect
        self.settings.set_window_effect(0)
        self.settings.set_theme_mode(ThemeMode.Dark)
        self.settings.set_spatial_mode_enabled(True)
        window = GalleryWindow(startup_visuals=True)
        try:
            window.resize(1248, 820)
            window.show()
            self.assertIsNotNone(window._splash)
            controller = window._spatial_controller
            self.assertIsNone(controller.capture)
            self.assertTrue(QTest.qWaitForWindowExposed(window))
            self.assertFalse(controller.canvas.property("presenting"))
            frame = controller.canvas.grabFramebuffer()
            self.assertFalse(frame.isNull())
            background = frame.pixelColor(frame.rect().center())
            self.assertEqual(background.alpha(), 255)
            self.assertLess(background.lightness(), 80)
            for _ in range(100):
                QTest.qWait(100)
                if controller.capture is not None and controller.progress == 1.:
                    break
            self.assertTrue(self.settings.spatial_available)
            self.assertIsNone(window._splash)
            self.assertIsNone(window._dismissal_splash)
            self.assertIsNotNone(controller.capture)
            self.assertEqual(controller.progress, 1.)
        finally:
            delete(window)
            self.settings.set_window_effect(old_effect)


if __name__ == "__main__":
    unittest.main(verbosity=2)
