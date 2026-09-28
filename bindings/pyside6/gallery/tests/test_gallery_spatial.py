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
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

import fluentqt
from fluentqt._qt_compat import delete_qobject as delete
from PySide6.QtCore import QAbstractAnimation, QEvent, QEventLoop, QObject, QPoint, QPointF, QRect, QRectF, QSettings, QSize, QSizeF, QTimer, Qt, qVersion
from PySide6.QtGui import QCursor, QMouseEvent, QPainter, QPolygonF, QTransform, QWindow, qGray
from PySide6.QtTest import QSignalSpy, QTest
from PySide6.QtWidgets import QApplication, QGraphicsOpacityEffect, QGraphicsView, QLineEdit, QMenu, QScrollArea, QWidget
from shiboken6 import isValid

from fluentqt_gallery.catalog import ENTRIES
from fluentqt_gallery.native_samples import build_native_sample
from fluentqt_gallery.settings import NavigationStyle, ThemeMode, gallery_settings
from fluentqt_gallery.spatial_support import SPATIAL_AVAILABLE
from fluentqt_gallery.window import GalleryWindow
import fluentqt_gallery.settings as settings_module


def _qwait(milliseconds):
    loop = QEventLoop()
    QTimer.singleShot(milliseconds, loop.quit)
    loop.exec()


class GallerySpatialTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        if os.environ.get("FLUENTQT_REQUIRE_NATIVE_TEST") == "1":
            if cls.app.platformName() in ("offscreen", "minimal", "vnc"):
                raise RuntimeError("Native Spatial tests require a real desktop Qt platform")
            if not SPATIAL_AVAILABLE:
                raise RuntimeError("Native Spatial tests require the optional Spatial binding")
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

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_destroyed_controller_disconnects_shared_settings_and_queued_failures(self):
        window = GalleryWindow(startup_visuals=False)
        controller = window._spatial_controller
        controller.cache_failure_timer.start(0)
        controller.render_failure_timer.start(0)
        delete(window)
        self.assertFalse(isValid(controller))
        self.settings.set_spatial_availability(True)
        self.settings.set_spatial_mode_enabled(True)
        self.settings.set_theme_mode(ThemeMode.Dark)
        self.settings.set_spatial_mode_enabled(False)
        _qwait(10)
        self.assertEqual(self.errors, [])

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_gl_limits_delegate_to_shared_native_runtime(self):
        from fluentqt_gallery import spatial_controller as compositor
        with patch.object(compositor, "SpatialRuntime") as runtime:
            for limit in (0, 2048, 16384):
                runtime.maximumTextureDimension.return_value = limit
                self.assertEqual(compositor._maximum_cache_dimension(None), limit)
            self.assertEqual(runtime.maximumTextureDimension.call_count, 3)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_sampler_uses_integer_uniform_without_overload_ambiguity(self):
        from fluentqt_gallery import panel_sampler as compositor
        from PySide6.QtGui import QMatrix4x4
        sampler = SimpleNamespace(program=Mock(), vertices=Mock(), vao=Mock())
        sampler.program.uniformLocation.return_value = 7
        gl = Mock()
        context = Mock()
        context.functions.return_value = gl
        with patch.object(compositor.QOpenGLContext, "currentContext", return_value=context), \
                patch.object(compositor.QOpenGLVertexArrayObject, "Binder", return_value=Mock()):
            compositor._PanelSampler.blit(sampler, 3, QSizeF(64, 64), QMatrix4x4())
        gl.glUniform1i.assert_called_once_with(7, 0)
        self.assertEqual([call.args[0] for call in sampler.program.setUniformValue.call_args_list],
                         ["target", "sourceSize", "panelSize"])

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_nested_opacity_keeps_color_across_paint_strips(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires actual GPU effect composition")
        from PySide6.QtGui import QColor, QPalette
        from fluentqt.spatial import SpatialRuntime
        from fluentqt_gallery.spatial_controller import GallerySpatialController
        window = fluentqt.Window()
        window._splash = window._dismissal_splash = None
        SpatialRuntime.prepareWindow(window)
        window.resize(1200, 900)
        navigation = fluentqt.NavigationView()
        navigation.setDisplayMode(fluentqt.NavigationView.DisplayMode.Left)
        window.setContentWidget(navigation)
        page = QWidget()
        page.setAutoFillBackground(True)
        palette = page.palette()
        palette.setColor(QPalette.Window, QColor(240, 240, 240))
        page.setPalette(palette)
        navigation.contentHost().insertPage(0, page)
        navigation.contentHost().setCurrentIndex(0, 0, False)
        tile = QWidget(page)
        tile.setGeometry(80, 180, 430, 160)
        tile.setAutoFillBackground(True)
        palette.setColor(QPalette.Window, QColor(220, 40, 70))
        tile.setPalette(palette)
        effect = QGraphicsOpacityEffect(tile)
        tile.setGraphicsEffect(effect)
        controller = GallerySpatialController(window, navigation)
        try:
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(window))
            self.settings.set_spatial_mode_enabled(True)
            for _ in range(100):
                if controller.canvas and controller.canvas.property("presenting"):
                    break
                _qwait(50)
            self.assertTrue(controller.canvas.property("presenting"))
            controller.settle()
            surface = controller.canvas
            dpr = surface.devicePixelRatioF()
            for y in (180, 520):
                tile.move(80, y)
                for alpha in (.25, .5, .75, 1.):
                    with self.subTest(y=y, alpha=alpha):
                        effect.setOpacity(alpha)
                        _qwait(100)
                        point = surface.mapFrom(window, controller.projected_position(
                            tile, tile.rect().center()))
                        image = surface.grabFramebuffer()
                        actual = image.pixelColor(round(point.x() * dpr), round(point.y() * dpr))
                        for channel, foreground in zip(actual.getRgb()[:3], (220, 40, 70)):
                            self.assertAlmostEqual(channel, 240 * (1 - alpha) + foreground * alpha,
                                                   delta=6)
        finally:
            delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_autosuggest_component_page_accepts_projected_focus_and_typing(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires a native focused editor and GPU composition")
        window = GalleryWindow(startup_visuals=False)
        window.resize(1200, 900)
        try:
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(window))
            window.activateWindow()
            self.assertTrue(QTest.qWaitForWindowActive(window))
            window.navigate("auto-suggest-box")
            _qwait(300)
            self.settings.set_spatial_mode_enabled(True)
            controller = window._spatial_controller
            for _ in range(100):
                if controller.canvas is not None and controller.canvas.property("presenting"):
                    break
                _qwait(50)
            self.assertTrue(controller.canvas.property("presenting"))
            controller.settle()
            boxes = [box for box in window.findChildren(fluentqt.AutoSuggestBox)
                     if box.isVisible() and box.objectName() != "GalleryTitleBar.SearchBox"]
            self.assertTrue(boxes)
            box = boxes[0]
            point = controller.projected_position(box, box.rect().center())
            QTest.mouseMove(window.windowHandle(), point)
            QTest.mouseClick(window.windowHandle(), Qt.LeftButton, Qt.NoModifier, point)
            _qwait(50)
            self.assertTrue(box.hasFocus())
            QTest.keyClicks(box, "a", Qt.NoModifier, 20)
            _qwait(650)
            self.assertTrue(box.isSuggestionListOpen())
            self.assertFalse(controller.canvas.grabFramebuffer().isNull())
            for theme in (ThemeMode.Light, ThemeMode.Dark):
                with self.subTest(theme=theme):
                    self.settings.set_theme_mode(theme)
                    box.setText("Visible 输入 42")
                    box.deselect()
                    _qwait(100)
                    canvas = controller.canvas
                    image = canvas.grabFramebuffer()
                    dpr = canvas.devicePixelRatioF()
                    bounds = QRect(12, box.height() - box.inputHeight() + 4,
                                   170, box.inputHeight() - 8)
                    polygon = QPolygonF()
                    for corner in (bounds.topLeft(), bounds.topRight(),
                                   bounds.bottomRight(), bounds.bottomLeft()):
                        point = canvas.mapFrom(window, controller.projected_position(box, corner))
                        polygon.append(QPointF(point.x() * dpr, point.y() * dpr))
                    pixels = polygon.boundingRect().toAlignedRect().intersected(image.rect())
                    ink = 0
                    for y in range(pixels.top(), pixels.bottom() + 1):
                        for x in range(pixels.left(), pixels.right() + 1):
                            if not polygon.containsPoint(QPointF(x, y), Qt.OddEvenFill):
                                continue
                            gray = qGray(image.pixel(x, y))
                            ink += gray < 100 if theme == ThemeMode.Light else gray > 180
                    self.assertGreater(ink, 40 * dpr * dpr,
                                       "Unselected input text must reach the GPU frame")
                    if evidence_dir := os.environ.get("FLUENTQT_SPATIAL_EVIDENCE_DIR"):
                        folder = Path(evidence_dir)
                        folder.mkdir(parents=True, exist_ok=True)
                        mode = "light" if theme == ThemeMode.Light else "dark"
                        image.save(str(folder / f"python-input-{mode}.png"))
            QTest.keyClick(box, Qt.Key_Down)
            QTest.keyClick(box, Qt.Key_Return)
            _qwait(80)
            self.assertFalse(box.isSuggestionListOpen())
            self.assertTrue(box.text())
            chosen = box.text()
            self.settings.set_spatial_mode_enabled(False)
            self.assertEqual(box.text(), chosen)
        finally:
            delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_windows_material_handoff_presents_opaque_gpu_background(self):
        if QApplication.platformName() != "windows":
            self.skipTest("requires the native Windows material compositor")
        from fluentqt.spatial import SpatialRuntime
        from fluentqt_gallery.spatial_controller import GallerySpatialController

        def wait_for(predicate):
            for _ in range(100):
                if predicate():
                    return True
                _qwait(50)
            return predicate()

        window = fluentqt.Window()
        window._splash = window._dismissal_splash = None
        window.resize(900, 650)
        SpatialRuntime.prepareWindow(window)
        navigation = fluentqt.NavigationView()
        window.setContentWidget(navigation)
        controller = GallerySpatialController(window, navigation)
        try:
            window.setBackdropEffect(fluentqt.BackdropEffect.Mica)
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(window))
            if window.backdropState().surfaceMode != fluentqt.BackdropSurfaceMode.CompositedTransparent:
                self.skipTest("native material unavailable; painted fallback is not DWM evidence")
            self.settings.set_spatial_mode_enabled(True)
            self.assertTrue(wait_for(lambda: controller.canvas is not None and
                                    controller.canvas.property("presenting")))
            controller.settle()
            surface = controller.canvas
            for effect in (fluentqt.BackdropEffect.Mica, fluentqt.BackdropEffect.Acrylic):
                window.setBackdropEffect(effect)
                self.assertTrue(wait_for(lambda: not surface.backdrop_cache))
                # "presenting" is a policy flag, not proof that Qt has swapped
                # the initial material frame. Test a visible handoff from one.
                material_frames = QSignalSpy(surface.frameSwapped)
                surface.update()
                self.assertTrue(wait_for(lambda: material_frames.count() > 0))
                opaque_frames = []

                def on_frame():
                    if window.backdropEffect() == fluentqt.BackdropEffect.Solid and surface.backdrop_cache:
                        opaque_frames.append(True)

                connection = surface.frameSwapped.connect(on_frame)
                try:
                    # No settings signal or later event-loop turn may repair the handoff.
                    window.setBackdropEffect(fluentqt.BackdropEffect.Solid)
                    self.assertTrue(opaque_frames)
                    self.assertTrue(surface.backdrop_cache)
                finally:
                    QObject.disconnect(connection)
        finally:
            delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_tilted_sampler_improves_contrast_without_coverage_flicker(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires a native desktop GPU")
        from PySide6.QtGui import QColor, QFont, QImage, QMatrix4x4
        from PySide6.QtOpenGL import QOpenGLFramebufferObject, QOpenGLTexture
        from PySide6.QtOpenGLWidgets import QOpenGLWidget
        from fluentqt_gallery.panel_sampler import _PanelSampler

        surface = QOpenGLWidget()
        surface.resize(640, 400)
        surface.show()
        self.assertTrue(QTest.qWaitForWindowExposed(surface))
        surface.makeCurrent()
        linear, monotone = _PanelSampler(), _PanelSampler()
        texture = target = None
        try:
            self.assertTrue(linear.create(linear_reference=True))
            self.assertTrue(monotone.create())
            source = QImage(600, 360, QImage.Format_RGBA8888)
            source.fill(QColor(224, 224, 224))
            painter = QPainter(source)
            painter.setPen(QColor(32, 32, 32))
            font = QFont(self.app.font())
            for row, size in enumerate((14, 18, 28)):
                font.setPixelSize(size)
                painter.setFont(font)
                painter.drawText(QPoint(30, 55 + row * 75), "Gallery 设置 · Popup 0123456789")
            for x in range(30, 450, 4):
                painter.fillRect(QRect(x, 260, 1, 50), QColor(32, 32, 32))
            painter.end()
            source = source.scaled(source.size() * 2, Qt.IgnoreAspectRatio, Qt.FastTransformation)
            texture = QOpenGLTexture(source)
            texture.setMinMagFilters(QOpenGLTexture.Linear, QOpenGLTexture.Linear)
            texture.setWrapMode(QOpenGLTexture.ClampToEdge)
            self.assertTrue(texture.isCreated())
            target = QOpenGLFramebufferObject(QSize(640, 400))
            self.assertTrue(target.isValid())
            gl = surface.context().functions()
            projection, quad = QMatrix4x4(), QMatrix4x4()
            projection.ortho(0., 640., 400., 0., -1., 1.)
            quad.translate(300., 180.)
            quad.scale(300., 180.)

            def draw(sampler, tilt):
                target.bind()
                gl.glViewport(0, 0, 640, 400)
                for state in (0x0BE2, 0x0B71, 0x0C11):
                    gl.glDisable(state)
                gl.glClearColor(224. / 255, 224. / 255, 224. / 255, 1.)
                gl.glClear(0x4000)
                sampler.blit(texture.textureId(), source.size(), projection * QMatrix4x4(tilt) * quad)
                return target.toImage()

            def measure(image, region):
                ink = energy = 0
                region = region.intersected(image.rect().adjusted(1, 1, -1, -1))
                for y in range(region.top(), region.bottom() + 1):
                    for x in range(region.left(), region.right() + 1):
                        value = qGray(image.pixel(x, y))
                        ink += 224 - value
                        energy += ((value - qGray(image.pixel(x - 1, y))) ** 2
                                   + (value - qGray(image.pixel(x, y - 1))) ** 2)
                return ink, energy / ink if ink else 0

            evidence, improvements = [], []
            ink_ranges = [[] for _ in range(3)]
            for phase in range(8):
                tilt = QTransform(.96, .006, .00006, -.012, .97, .00003,
                                  20. + phase / 8., 16. + phase / 20., 1.)
                before, after = draw(linear, tilt), draw(monotone, tilt)
                # Convex reconstruction must retain bounded colors, including images.
                values = [qGray(after.pixel(x, y)) for y in range(after.height())
                          for x in range(after.width())]
                self.assertGreaterEqual(min(values), 31)
                self.assertLessEqual(max(values), 225)
                ratios = []
                for row in range(3):
                    region = tilt.mapRect(QRectF(24, 20 + row * 75, 500, 43)).toAlignedRect()
                    old, current = measure(before, region), measure(after, region)
                    self.assertGreater(old[1], 10)
                    self.assertGreaterEqual(current[1], old[1] * .99)
                    self.assertLessEqual(abs(current[0] / old[0] - 1), .025)
                    ratios.append(current[1] / old[1])
                    ink_ranges[row].append(current[0])
                improvements.extend(ratios)
                evidence.append({"phase": phase / 8., "contrast_ratios": ratios})
                lines = tilt.mapRect(QRectF(35, 264, 400, 40)).toAlignedRect()
                filtered = sum(48 < qGray(after.pixel(x, y)) < 208
                               for y in range(lines.top(), lines.bottom() + 1)
                               for x in range(lines.left(), lines.right() + 1))
                self.assertGreater(filtered, lines.width() * lines.height() * .08)
                if phase == 0 and os.environ.get("FLUENTQT_SPATIAL_EVIDENCE_DIR"):
                    directory = Path(os.environ["FLUENTQT_SPATIAL_EVIDENCE_DIR"])
                    directory.mkdir(parents=True, exist_ok=True)
                    before.save(str(directory / "tilted-linear-reference.png"))
                    after.save(str(directory / "tilted-monotone.png"))
            self.assertGreater(sum(improvements) / len(improvements), 1.01)
            for values in ink_ranges:
                self.assertLess(max(values) / min(values), 1.03)
            print(json.dumps({"tilted_font_quality": evidence}), flush=True)
            self.assertEqual(gl.glGetError(), 0)
        finally:
            linear.destroy()
            monotone.destroy()
            if texture is not None:
                texture.destroy()
            # FBOs are not QObjects; drop the wrapper while its context is current.
            target = None
            surface.doneCurrent()
            delete(surface)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_sampler_blit_does_not_leave_gl_errors(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires a native desktop GPU")
        from PySide6.QtGui import QMatrix4x4
        window = GalleryWindow(startup_visuals=False)
        window.resize(900, 600)
        try:
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(window))
            self.settings.set_spatial_mode_enabled(True)
            controller = window._spatial_controller
            for _ in range(60):
                _qwait(50)
                if controller.canvas and controller.canvas.caches[1].get("texture"):
                    break
            self.assertTrue(self.settings.spatial_available, self.settings.spatial_unavailable_reason)
            surface = controller.canvas
            self.assertIsNotNone(surface)
            texture = surface.caches[1].get("texture")
            self.assertIsNotNone(texture)
            self.assertTrue(texture.isValid())
            surface.makeCurrent()
            try:
                gl = surface.context().functions()
                self.assertEqual(gl.glGetError(), 0, "normal Gallery frames must leave GL_NO_ERROR")
                surface.blitter.blit(texture.texture(), texture.size(), QMatrix4x4())
                self.assertEqual(gl.glGetError(), 0, "an actual sampler blit must leave GL_NO_ERROR")
            finally:
                surface.doneCurrent()
        finally:
            delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_support_badges_wake_tooltips_at_projected_positions(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires native mouse dispatch and GPU composition")
        window = GalleryWindow(startup_visuals=False)
        window.resize(1180, 820)
        try:
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(window))
            window.navigate("settings", animated=False)
            self.settings.set_spatial_mode_enabled(True)
            controller = window._spatial_controller
            for _ in range(60):
                if controller.canvas.property("presenting"):
                    break
                _qwait(50)
            self.assertTrue(controller.canvas.property("presenting"))
            controller.settle()
            window.raise_()
            window.activateWindow()
            QTest.mouseMove(window.windowHandle(), QPoint(window.width() // 2, 25))
            for theme in (ThemeMode.Light, ThemeMode.Dark):
                self.settings.set_theme_mode(theme)
                _qwait(150)
                for name in ("gallerySettingsSpatialSupportBadge", "galleryNavigationSpatialSupportBadge"):
                    with self.subTest(theme=theme, badge=name):
                        badge = window.findChild(QWidget, name)
                        self.assertIsNotNone(badge)
                        self.assertTrue(badge.isVisible())
                        tooltip = badge.findChild(fluentqt.ToolTip)
                        self.assertIsNotNone(tooltip)
                        tooltip.setAnimationEnabled(False)
                        previous_geometry, stable_turns = None, 0
                        geometry_samples = []
                        for _ in range(40):
                            _qwait(25)
                            point = controller.projected_position(badge, badge.rect().center())
                            geometry = (badge.mapTo(window, QPoint()), badge.size(), point)
                            geometry_samples.append([geometry[0].x(), geometry[0].y(),
                                                     point.x(), point.y()])
                            stable_turns = stable_turns + 1 if geometry == previous_geometry else 0
                            previous_geometry = geometry
                            if stable_turns >= 3:
                                break
                        self.assertGreaterEqual(stable_turns, 3, "Wait for page layout before targeting the badge")
                        # Move straight from the previous badge to this one.
                        # Detouring through the title bar would clear projected
                        # hover before dispatch and hide the transition defect.
                        # Use QPA input: QWidget's cursor-only warp can be
                        # overwritten by the host cursor when WSLg maps a tooltip.
                        point = controller.projected_position(badge, badge.rect().center())
                        QTest.mouseMove(window.windowHandle(), point)
                        # Let Qt's native wake-up timer dispatch ToolTip. Sending
                        # QHelpEvent here would hide hover/timer ordering bugs.
                        for _ in range(60):
                            if tooltip.isVisible():
                                break
                            _qwait(50)
                        current_point = controller.projected_position(badge, badge.rect().center())
                        print(json.dumps({"badge": name, "theme": str(theme),
                                          "stable_geometry": geometry_samples[-4:],
                                          "projected_after_wait": [current_point.x(), current_point.y()],
                                          "cursor_global": [QCursor.pos().x(), QCursor.pos().y()],
                                          "hover_target_matches": controller.hovered is badge}), flush=True)
                        self.assertIs(controller.hovered, badge)
                        self.assertTrue(tooltip.isVisible())
                        self.assertIn("GPU", tooltip.text())
                        global_point = window.mapToGlobal(
                            controller.projected_position(badge, badge.rect().center()))
                        self.assertLess(abs(tooltip.geometry().center().x() - global_point.x()), 40)
                        self.assertLess(abs(tooltip.geometry().bottom() - global_point.y()), 60)
                        if os.environ.get("FLUENTQT_SPATIAL_EVIDENCE_DIR"):
                            evidence = Path(os.environ["FLUENTQT_SPATIAL_EVIDENCE_DIR"])
                            evidence.mkdir(parents=True, exist_ok=True)
                            window.screen().grabWindow(window.winId()).save(
                                str(evidence / (name + "-" + str(theme) + ".png")))
        finally:
            delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_mouse_coordinates_keep_receiver_local_and_synthetic_global(self):
        from fluentqt_gallery.spatial_controller import _mouse_global_position
        receiver = QWidget()
        receiver.move(-100, -50)
        event = SimpleNamespace(position=lambda: QPointF(11, 22),
                                globalPosition=lambda: QPointF(-1, -1),
                                spontaneous=lambda: True)
        try:
            self.assertEqual(_mouse_global_position(receiver, event),
                             receiver.mapToGlobal(QPoint(11, 22)))
            event.spontaneous = lambda: False
            self.assertEqual(_mouse_global_position(receiver, event), QPoint(-1, -1))
            event.globalPosition = lambda: QPointF(-350, 321)
            self.assertEqual(_mouse_global_position(receiver, event), QPoint(-350, 321))
        finally:
            delete(receiver)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_projected_hover_leave_resets_forwarding_after_receiver_destruction(self):
        from fluentqt_gallery.spatial_controller import GallerySpatialController

        for destroyed in ("controller", "receiver", None):
            with self.subTest(destroyed=destroyed):
                receiver = QWindow()
                window = QWidget()
                # Use an independent receiver: QWidget owns its real native
                # handle, which cannot legally be deleted behind QWidget's back.
                window.windowHandle = lambda: receiver
                nav = QWidget(window)
                previous = QWidget(nav)
                controller = QObject()
                controller.window, controller.navigation = window, nav
                controller.canvas = QWidget(window)
                controller.capture = QGraphicsOpacityEffect(window)
                controller.hovered = previous
                controller.forwarding = False
                controller.renderer_ready = True
                controller.panels = []
                controller.native_overlay_at = lambda _point: False
                calls = []

                class LeaveHandler(QObject):
                    def eventFilter(self, watched, event):
                        if event.type() == QEvent.Leave:
                            calls.append((controller.hovered, controller.forwarding))
                            if destroyed == "controller":
                                delete(controller)
                            elif destroyed == "receiver":
                                delete(receiver)
                        return False

                handler = LeaveHandler()
                previous.installEventFilter(handler)
                global_pos = window.mapToGlobal(QPoint(200, 200))
                event = QMouseEvent(QEvent.MouseMove, QPointF(200, 200), QPointF(global_pos),
                                    Qt.NoButton, Qt.NoButton, Qt.NoModifier)
                try:
                    consumed = GallerySpatialController.eventFilter(controller, receiver, event)
                    self.assertEqual(calls, [(None, True)])
                    self.assertEqual(consumed, destroyed is not None)
                    if isValid(controller):
                        self.assertFalse(controller.forwarding)
                finally:
                    if isValid(controller):
                        delete(controller)
                    delete(window)
                    if isValid(receiver):
                        delete(receiver)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_menu_follow_preserves_nested_forwarding_guard(self):
        from fluentqt_gallery.spatial_controller import GallerySpatialController
        window = QWidget()
        navigation = fluentqt.NavigationView(window)
        source = QWidget(navigation.contentHost())
        menu = QMenu(source)
        menu.addAction("Item")
        menu.show()
        controller = SimpleNamespace(
            navigation=navigation, window=window, forwarding=True,
            native_popup_anchors={id(menu): (menu, source, QPoint(), QPoint())},
        )
        try:
            GallerySpatialController.publish_presentation_transforms(controller, False)
            self.assertTrue(controller.forwarding)
        finally:
            menu.hide()
            delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_empty_scene_does_not_request_resource_fallback(self):
        from fluentqt_gallery.spatial_controller import _Surface
        owner = SimpleNamespace(panels=[(QRectF(), QTransform()), (QRectF(), QTransform())])
        surface = Mock(owner=owner)
        surface.property.return_value = True
        _Surface.paintGL(surface)
        surface.prepare_caches.assert_not_called()

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_render_failures_use_context_owned_timers_on_supported_qt_versions(self):
        from fluentqt_gallery.spatial_controller import _Surface
        for resource_failure in (False, True):
            with self.subTest(resource_failure=resource_failure):
                owner = QObject()
                owner.panels = [(QRectF(0, 0, 10, 10), QTransform())]
                owner.navigation = Mock()
                owner.content_revision = 1
                owner.cache_failure_timer = QTimer(owner)
                owner.render_failure_timer = QTimer(owner)
                callbacks = []
                owner.cache_failure_timer.setSingleShot(True)
                owner.render_failure_timer.setSingleShot(True)
                owner.cache_failure_timer.timeout.connect(lambda: callbacks.append("cache"))
                owner.render_failure_timer.timeout.connect(lambda: callbacks.append("render"))
                surface = Mock(owner=owner, cache_failure_pending=False)
                surface.plan = (1., [QSize(10, 10), QSize(10, 10)])
                surface.devicePixelRatioF.return_value = 1.
                surface.static_cache_bytes.return_value = 0
                surface.particles.prepare.return_value = False
                surface.property.return_value = True
                surface.prepare_caches.return_value = not resource_failure
                surface.blitter.isCreated.return_value = False
                try:
                    _Surface.paintGL(surface)
                    _qwait(10)
                    self.assertEqual(callbacks, ["cache" if resource_failure else "render"])
                finally:
                    delete(owner)

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
        particles = 90 << 20
        joint = _cache_plan([QSizeF(240, 700), QSizeF(960, 700)], 2, 16384,
                            reserved_bytes=particles)
        self.assertEqual(joint[:2], normal[:2])
        self.assertLess(joint[2].height(), normal[2].height())
        self.assertLessEqual(sum(s.width() * s.height() for s in joint[1]) * 4
                             + joint[2].width() * joint[2].height() * (12 * _PAINT_SAMPLES + 4)
                             + particles, _CACHE_BUDGET_BYTES)
        self.assertIsNone(_cache_plan(panels, 2, 16384, reserved_bytes=_CACHE_BUDGET_BYTES))
        self.assertIsNone(_cache_plan(panels, 2, 16384, reserved_bytes=-1))

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
from fluentqt_gallery.catalog import CONTRACT, ENTRIES, ROUTES
window = GalleryWindow(startup_visuals=False)
window.navigate("settings", animated=False)
assert window._spatial_controller is None
assert not window._settings.spatial_available
assert not window._settings.spatial_mode_enabled
assert {entry.route_id for entry in ENTRIES} == {
    component["id"] for component in CONTRACT["components"] if component["category_id"] != "spatial"}
assert {route.id for route in ROUTES} == {
    route["id"] for route in CONTRACT["routes"]
    if route["id"] != "spatial" and route["parent_id"] != "spatial"}
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
    def test_unavailable_gpu_keeps_startup_and_requested_3d_in_usable_2d(self):
        from PySide6.QtOpenGLWidgets import QOpenGLWidget
        for initially_3d in (False, True):
            for theme in (ThemeMode.Light, ThemeMode.Dark):
                with self.subTest(initially_3d=initially_3d, theme=theme), \
                     patch("fluentqt_gallery.spatial_controller._session_unavailable_reason", return_value=""), \
                     patch("fluentqt_gallery.spatial_controller._RhiSurface.preflightFailure", return_value="Simulated unavailable GPU"), \
                     patch("fluentqt_gallery.spatial_controller.SpatialRuntime.preflightFailure", return_value="Simulated unavailable GPU") as probe:
                    self.settings.set_theme_mode(theme)
                    self.settings.set_spatial_mode_enabled(initially_3d)
                    window = GalleryWindow(startup_visuals=False)
                    try:
                        window.resize(960, 720)
                        window.show()
                        window.navigate("settings", animated=False)
                        _qwait(100)
                        controller = window._spatial_controller
                        toggle = window.findChild(fluentqt.ToggleSwitch, "gallerySettingsSpatialModeToggle")
                        if not initially_3d:
                            probe.assert_not_called()
                            self.assertTrue(toggle.isEnabled())
                            QTest.mouseClick(toggle, Qt.LeftButton, pos=QPoint(20, 16))
                        probe.assert_called_once()
                        self.assertFalse(toggle.isEnabled())
                        self.assertFalse(toggle.isOn())
                        self.assertIsNone(controller.canvas)
                        self.assertIsNone(controller.capture)
                        self.assertIsNone(controller.content_capture)
                        self.assertFalse(controller.filtering)
                        self.assertEqual(window.findChildren(QOpenGLWidget), [])
                        self.assertEqual(self.settings.spatial_unavailable_reason, "Simulated unavailable GPU")
                        self.assertTrue(self.settings.spatial_mode_enabled)
                        self.assertIsNone(window._navigation_view.graphicsEffect())
                        self.assertIsNone(window._content_host.graphicsEffect())
                        editor = window._search
                        self.assertIsInstance(editor, QLineEdit)
                        QTest.mouseClick(editor, Qt.LeftButton)
                        QTest.keyClicks(editor, "Button")
                        self.assertEqual(editor.text(), "Button")
                        QTest.keyClick(editor, Qt.Key_Return)
                        _qwait(50)
                        self.assertEqual(window._current_route, "button")
                        self.assertTrue(window._pages["button"][1].isVisible())
                    finally:
                        delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_first_toggle_preserves_window_identity_and_mica(self):
        platform = QApplication.platformName()
        if platform not in ("windows", "cocoa", "xcb") and not platform.startswith("wayland"):
            self.skipTest("requires a native desktop window")
        if tuple(map(int, qVersion().split(".")[:2])) < (6, 4):
            self.skipTest("Qt 6.4 introduced dynamic top-level surface replacement")
        old_effect = self.settings.window_effect
        self.settings.set_window_effect(1)
        try:
            for maximized in (False, True):
                with self.subTest(maximized=maximized):
                    self.settings.set_spatial_mode_enabled(False)
                    window = GalleryWindow(startup_visuals=False)
                    try:
                        window.resize(980, 720)
                        window.showMaximized() if maximized else window.show()
                        self.assertTrue(QTest.qWaitForWindowExposed(window))
                        window.raise_()
                        window.activateWindow()
                        window._search.setFocus()
                        _qwait(100)
                        if maximized and not window.isMaximized():
                            self.skipTest("Window manager did not enter maximized state before the 3D toggle")
                        controller = window._spatial_controller
                        self.assertIsNone(controller.canvas)
                        first_handle = window.windowHandle()
                        first_id = window.winId()
                        first_geometry = window.geometry()
                        first_focus = window.focusWidget()
                        self.assertIsNotNone(first_focus)
                        visibility = QSignalSpy(first_handle.visibleChanged)
                        self.settings.set_spatial_mode_enabled(True)
                        for _ in range(100):
                            if controller.canvas is not None and controller.canvas.property("presenting"):
                                break
                            _qwait(30)
                        self.assertTrue(self.settings.spatial_available, self.settings.spatial_unavailable_reason)
                        self.assertTrue(controller.canvas.property("presenting"))
                        self.assertIs(window.windowHandle(), first_handle)
                        self.assertEqual(window.winId(), first_id)
                        self.assertEqual(window.geometry(), first_geometry)
                        self.assertEqual(window.isMaximized(), maximized)
                        self.assertEqual(visibility.count(), 0)
                        self.assertIs(window.focusWidget(), first_focus)
                        self.assertEqual(window.backdropEffect(), fluentqt.BackdropEffect.Mica)
                    finally:
                        delete(window)
        finally:
            self.settings.set_window_effect(old_effect)

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
                    _qwait(100)
                    self.assertFalse(controller.renderer_failed)
                    self.assertTrue(self.settings.spatial_availability_pending)
                    self.assertTrue(self.settings.spatial_mode_enabled)
                    self.assertIsNone(navigation.graphicsEffect())
                    if cancel_while_pending:
                        self.settings.set_spatial_mode_enabled(False)
                        self.assertTrue(surface.isHidden())
                    navigation.resize(window.size())
                    if cancel_while_pending:
                        _qwait(100)
                        self.assertFalse(surface.isValid())
                        self.assertIsNone(navigation.graphicsEffect())
                        self.settings.set_spatial_mode_enabled(True)
                    for _ in range(60):
                        if surface.property("presenting"):
                            break
                        _qwait(50)
                    self.assertTrue(self.settings.spatial_available)
                    self.assertTrue(surface.isValid())
                    self.assertTrue(surface.property("presenting"))
                    self.assertFalse(controller.renderer_timeout.isActive())
                    self.settings.set_spatial_mode_enabled(False)
                    _qwait(500)
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
            _qwait(600)
            surface = controller.canvas
            self.assertTrue(surface.property("presenting"))
            old_context = surface.context()
            old_functions = old_context.functions()
            self.settings.set_spatial_mode_enabled(False)
            _qwait(600)
            self.assertTrue(surface.isHidden())
            self.assertTrue(isValid(old_context))
            self.assertTrue(isValid(old_functions), "2D mode must not invalidate a live context")
            window.setParent(desktop, Qt.Widget)
            desktop.resize(800, 600)
            desktop.show()
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(desktop))
            self.settings.set_spatial_mode_enabled(True)
            for _ in range(60):
                if surface.property("presenting"):
                    break
                _qwait(50)
            self.assertTrue(surface.property("presenting"))
            self.assertTrue(surface.isValid())
            self.assertTrue(self.settings.spatial_available)
            self.assertIs(controller.canvas, surface)
            self.assertFalse(isValid(old_context))
            self.assertFalse(isValid(old_functions), "Retired context functions must leave the wrapper registry")
            final_functions = surface.context().functions()
            self.assertTrue(isValid(final_functions))
        finally:
            delete(window)
            delete(desktop)
        self.assertFalse(isValid(final_functions), "Window destruction must invalidate its borrowed functions")

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
            _qwait(100)
            self.assertTrue(self.settings.spatial_availability_pending)
            self.assertIsNone(navigation.graphicsEffect())
            for _ in range(130):
                if not self.settings.spatial_availability_pending:
                    break
                _qwait(50)
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
    def test_native_home_particles_share_budget_without_recapturing_static_content(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires the real Home layout and native GPU composition")
        from fluentqt_gallery.spatial_controller import _CACHE_BUDGET_BYTES
        self.settings.set_home_particles_enabled(True)
        window = GalleryWindow(startup_visuals=False)
        try:
            window.resize(1200, 850)
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(window))
            if window.devicePixelRatioF() > 2:
                self.skipTest("this full-size acceleration budget covers native DPR up to 2")
            particles = window.findChild(fluentqt.ParticleBackdrop, "galleryHomeParticles")
            self.assertIsNotNone(particles)
            particles.setEffect(fluentqt.ParticleBackdrop.Starfield)
            particles.setPauseWhenInactive(False)
            self.settings.set_spatial_mode_enabled(True)
            controller = window._spatial_controller
            for _ in range(100):
                if (controller.canvas and controller.canvas.property("presenting")
                        and controller.canvas.particles.activeLayerCount()):
                    break
                _qwait(30)
            surface = controller.canvas
            self.assertIsNotNone(surface)
            self.assertTrue(surface.property("presenting"))
            self.assertGreater(surface.particles.activeLayerCount(), 0)
            controller.settle()
            QTest.mouseMove(window, QPoint(5, 5))
            QApplication.sendEvent(window, QEvent(QEvent.Leave))
            _qwait(800)

            def stable_state():
                return (tuple(cache["texture"].texture() for cache in surface.caches),
                        tuple(cache.get("key") for cache in surface.caches),
                        surface.plan, surface.particles.allocatedBytes(),
                        surface.particles.foregroundCaptureCount())

            before = stable_state()
            frames = surface.particles.particleFrameCount()
            for _ in range(3):
                _qwait(150)
                self.assertEqual(stable_state(), before)
                self.assertGreater(surface.particles.particleFrameCount(), frames)
                frames = surface.particles.particleFrameCount()
            self.assertGreaterEqual(surface.plan[0], window.devicePixelRatioF())
            self.assertLessEqual(surface.static_cache_bytes() + surface.particles.allocatedBytes(),
                                 _CACHE_BUDGET_BYTES)
            if os.environ.get("FLUENTQT_SPATIAL_EVIDENCE_DIR"):
                directory = Path(os.environ["FLUENTQT_SPATIAL_EVIDENCE_DIR"])
                directory.mkdir(parents=True, exist_ok=True)
                self.assertTrue(surface.grabFramebuffer().save(str(directory / "python-home-particles-joint.png")))
        finally:
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
                    _qwait(50)
                self.assertFalse(self.settings.spatial_mode_enabled)
                self.assertTrue(self.settings.spatial_available)
                self.assertFalse(controller.renderer_failed)
            surface = controller.canvas
            self.settings.set_spatial_mode_enabled(True)
            for _ in range(60):
                if surface.property("presenting"):
                    break
                _qwait(50)
            controller.settle()
            _qwait(100)
            self.assertTrue(surface.property("presenting"))
            self.assertIs(controller.canvas, surface)
            self.assertTrue(all(cache for cache in surface.caches))
        finally:
            delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_overlay_coordinates_follow_projection_scroll_and_2d_restore(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires native OpenGL composition and native menus")
        from fluentqt_gallery.spatial_controller import GallerySpatialController
        window = fluentqt.Window()
        window._splash = window._dismissal_splash = None
        window.resize(1100, 760)
        navigation = fluentqt.NavigationView(window)
        navigation.setAnimationEnabled(False)
        navigation.setDisplayMode(fluentqt.NavigationView.DisplayMode.Left)
        navigation.resize(window.size())
        page = QWidget()
        navigation.contentHost().insertPage(0, page)
        navigation.contentHost().setCurrentIndex(0, 0, False)
        scroll = QScrollArea(page)
        scroll.setGeometry(10, 30, 530, 520)
        content = QWidget()
        content.resize(500, 1100)
        scroll.setWidget(content)
        anchor = fluentqt.ComboBox(content)
        anchor.setGeometry(110, 210, 160, 32)
        anchor.addItems(["One", "Two", "Three"])
        controller = GallerySpatialController(window, navigation)
        overlays = []
        try:
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(window))
            self.settings.set_spatial_mode_enabled(True)
            for _ in range(100):
                if controller.canvas and controller.canvas.property("presenting"):
                    break
                _qwait(50)
            self.assertTrue(controller.canvas.property("presenting"))
            controller.settle()

            def bounds():
                host = navigation.contentHost()
                transform = host.property("_fluent_qt_overlay_presentation_transform")
                rect = QRect(anchor.mapTo(host, QPoint()), anchor.size())
                if transform is None:
                    return QRect(anchor.mapTo(window, QPoint()), anchor.size())
                return transform.mapRect(QRectF(rect)).translated(host.mapTo(window, QPoint())).toAlignedRect()

            point = QPoint(7, anchor.height() + 8)
            popup = fluentqt.Popup(anchor)
            popup.setModal(False)
            popup.setDim(False)
            popup.setAnimationEnabled(False)
            popup.setClosePolicy(fluentqt.Popup.CloseFlag.NoAutoClose)
            popup.resize(200, 112)
            popup.setPosition(anchor, point)
            popup.open()
            overlays.append(popup)
            flyout = fluentqt.Flyout(anchor)
            flyout.setAnimationEnabled(False)
            flyout.setClosePolicy(fluentqt.Popup.CloseFlag.NoAutoClose)
            flyout.setPlacement(fluentqt.Flyout.Placement.Bottom)
            flyout.resize(200, 112)
            flyout.showAt(anchor)
            overlays.append(flyout)
            tip = fluentqt.TeachingTip(anchor)
            tip.setAnimationEnabled(False)
            tip.setModal(False)
            tip.setDim(False)
            tip.setTailVisible(False)
            tip.setCardSize(QSize(168, 80))
            tip.setPreferredPlacement(fluentqt.TeachingTip.PreferredPlacement.BottomLeft)
            tip.showAt(anchor)
            overlays.append(tip)
            coach = fluentqt.CoachMark(anchor)
            coach.setCardSize(QSize(168, 80))
            coach.setTarget(anchor)
            coach.setPlacement(fluentqt.CoachMark.Placement.Bottom)
            coach.open()
            overlays.append(coach)

            def check():
                _qwait(30)
                rect = bounds()
                self.assertEqual(popup.geometry().adjusted(16, 16, -16, -16).topLeft(),
                                 controller.projected_position(anchor, point))
                self.assertEqual(flyout.geometry().adjusted(16, 16, -16, -16).topLeft(),
                                 QPoint(rect.center().x() - 84, rect.bottom() + flyout.anchorOffset()))
                self.assertEqual(tip.geometry().adjusted(16, 16, -16, -16).topLeft(),
                                 QPoint(rect.left(), rect.bottom() + tip.placementMargin()))
                self.assertEqual(coach.x(), rect.center().x() - coach.width() // 2)
                for overlay in overlays:
                    self.assertIs(overlay.parentWidget(), window)
                    self.assertFalse(overlay.isWindow())

            check()
            scroll.verticalScrollBar().setValue(60)
            check()
            controller.pointer = QPointF(.6, -.4)
            controller.sync()
            check()
            window.move(window.pos() + QPoint(20, 15))
            navigation.resize(navigation.width() - 35, navigation.height() - 25)
            check()
            self.settings.set_spatial_mode_enabled(False)
            controller.settle()
            check()
            self.assertIsNone(navigation.property("_fluent_qt_overlay_presentation_transform"))
            self.assertIsNone(navigation.contentHost().property("_fluent_qt_overlay_presentation_transform"))
            for overlay in overlays:
                overlay.close()
            _qwait(300)

            button = fluentqt.DropDownButton("Menu", page)
            button.setGeometry(90, 140, 140, 32)
            button.show()
            menu = QMenu(button)
            menu.addAction("First")
            submenu = menu.addMenu("Children")
            submenu.addAction("Child")
            button.setMenu(menu)
            self.settings.set_spatial_mode_enabled(True)
            controller.settle()
            QTest.mousePress(window, Qt.LeftButton,
                             pos=controller.projected_position(button, button.rect().center()))
            self.assertTrue(menu.isVisible())
            QTest.mouseRelease(menu, Qt.LeftButton, pos=QPoint(-2, -2))
            expected = window.mapToGlobal(controller.projected_position(button, button.rect().bottomLeft()))
            self.assertLessEqual((menu.pos() - expected).manhattanLength(), 1)
            # The Show callback's zero-timer must not retain the old projection.
            self.settings.set_spatial_mode_enabled(False)
            controller.settle()
            _qwait(20)
            self.assertEqual(menu.pos(), button.mapToGlobal(button.rect().bottomLeft()))
            self.settings.set_spatial_mode_enabled(True)
            controller.settle()
            _qwait(20)
            self.assertLessEqual((menu.pos() - expected).manhattanLength(), 1)
            child_position = menu.mapToGlobal(QPoint(menu.width(), 0))
            submenu.popup(child_position)
            _qwait(20)
            self.assertLessEqual((submenu.pos() - child_position).manhattanLength(), 1)
            submenu.hide()
            menu.hide()
            available = window.screen().availableGeometry()
            window.move(available.bottomRight() - QPoint(window.width() // 2, window.height() // 2))
            _qwait(20)
            QTest.keyClick(button, Qt.Key_F4)
            _qwait(20)
            self.assertTrue(menu.isVisible())
            self.assertTrue(available.contains(menu.frameGeometry()))
            window.move(window.pos() + QPoint(80, 80))
            _qwait(60)
            self.assertTrue(not menu.isVisible() or available.contains(menu.frameGeometry()))
            menu.hide()
        finally:
            delete(window)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_native_gpu_filters_projected_high_dpi_detail(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires native GPU texture sampling")
        from fluentqt_gallery.spatial_controller import GallerySpatialController

        class DetailContent(QWidget):
            glyph_dpr = 0
            def paintEvent(self, event):
                painter = QPainter(self)
                from fluentqt_gallery.glyph_paint_device import GlyphPaintDevice
                device = getattr(painter.paintEngine(), "device", None)
                if isinstance(device, GlyphPaintDevice):
                    self.glyph_dpr = device.native_dpr
                painter.fillRect(self.rect(), Qt.white)
                painter.setPen(Qt.black)
                font = self.font()
                for size, y, text in (
                    (14, 40, "High DPI text: Popup settings 0123456789"),
                    (28, 290, "Gallery 标题 · 清晰度"),
                    (18, 330, "正文：设置与组件，0123456789"),
                ):
                    font.setPixelSize(size)
                    painter.setFont(font)
                    painter.drawText(QPoint(40, y), text)
                for x in range(40, 180, 4):
                    painter.fillRect(QRect(x, 180, 1, 40), Qt.black)
                painter.end()

        window = fluentqt.Window()
        window._splash = window._dismissal_splash = None
        window.resize(900, 500)
        navigation = fluentqt.NavigationView(window)
        navigation.resize(window.size())
        content = DetailContent()
        button = fluentqt.Button("Show popup", content)
        button.setGeometry(40, 70, 140, 36)
        button.setFocusPolicy(Qt.NoFocus)
        toggle = fluentqt.ToggleSwitch(content)
        toggle.setOnContent("Light dismiss")
        toggle.setIsOn(True)
        toggle.setGeometry(205, 70, 190, 36)
        toggle.setFocusPolicy(Qt.NoFocus)
        navigation.contentHost().insertPage(0, content)
        navigation.contentHost().setCurrentIndex(0, 0, False)
        controller = GallerySpatialController(window, navigation)
        try:
            window.show()
            self.assertTrue(QTest.qWaitForWindowExposed(window))
            _qwait(200)
            toggle.setFocus(Qt.TabFocusReason)
            reference = content.grab().toImage()
            self.settings.set_spatial_mode_enabled(True)
            for _ in range(60):
                if controller.canvas.property("presenting"):
                    break
                _qwait(50)
            self.assertTrue(controller.canvas.property("presenting"))
            controller.settle()
            _qwait(120)
            surface = controller.canvas
            self.assertGreater(surface.paint_target.format().samples(), 1)
            dpr = surface.devicePixelRatioF()
            from fluentqt_gallery.glyph_paint_device import needs_native_glyph_coverage
            self.assertEqual(content.glyph_dpr, dpr if needs_native_glyph_coverage(dpr) else 0)
            controller.motion.setStartValue(0.)
            controller.motion.setEndValue(1.)
            controller.motion.setCurrentTime(controller.motion.duration() // 2)
            controller.motion.setCurrentTime(0)
            _qwait(200)
            frame = surface.grabFramebuffer()
            origin = surface.mapFrom(window, controller.projected_position(content, QPoint()))
            actual = frame.copy(QRect(origin * dpr, reference.size()))
            def contrast(image, rect):
                ink = energy = 0
                for y in range(rect.top() + 1, rect.bottom()):
                    for x in range(rect.left() + 1, rect.right()):
                        value = qGray(image.pixel(x, y))
                        ink += 255 - value
                        energy += ((value - qGray(image.pixel(x - 1, y))) ** 2
                                   + (value - qGray(image.pixel(x, y - 1))) ** 2)
                return energy / ink if ink else 0
            ratios, chromatic_pixels = [], []
            for region in (QRect(35, 20, 370, 35), QRect(35, 260, 370, 35), QRect(35, 306, 370, 30)):
                area = QRect(region.topLeft() * dpr, region.size() * dpr)
                before, after = contrast(reference, area), contrast(actual, area)
                self.assertGreater(before, 20)
                ratios.append(after / before)
                chromatic_pixels.append(sum(
                    max(reference.pixelColor(x, y).getRgb()[:3]) -
                    min(reference.pixelColor(x, y).getRgb()[:3]) > 5
                    for y in range(area.top(), area.bottom()) for x in range(area.left(), area.right())))
            # Keep the pixel-difference contract on controls/geometry. Native LCD
            # text has colored edge coverage unlike transparent GPU glyphs; all
            # three text sizes have their own unchanged 90% contrast contract.
            detail = QRect(QPoint(30, 65) * dpr, QSizeF(380, 170).toSize() * dpr)
            different = sum(
                sum(abs(a - b) for a, b in zip(actual.pixelColor(x, y).getRgb()[:3],
                                              reference.pixelColor(x, y).getRgb()[:3])) > 60
                for y in range(detail.top(), detail.bottom())
                for x in range(detail.left(), detail.right()))
            print(json.dumps({"font_dpr": dpr, "contrast_ratios": ratios,
                              "reference_chromatic_pixels": chromatic_pixels,
                              "detail_different_pixels": different}), flush=True)
            if os.environ.get("FLUENTQT_SPATIAL_EVIDENCE_DIR"):
                evidence = Path(os.environ["FLUENTQT_SPATIAL_EVIDENCE_DIR"])
                evidence.mkdir(parents=True, exist_ok=True)
                reference.save(str(evidence / "font-detail-2d.png"))
                actual.save(str(evidence / "font-detail-gpu.png"))
            for ratio in ratios:
                self.assertGreaterEqual(ratio, .9,
                                        "Native-DPR text must retain 90% of 2D edge contrast")
            # Half-pixel MSAA edges differ from Qt's aliased native fillRect even
            # before the adapter. Exact non-text forwarding at fractional DPR is
            # covered by the CPU paint-device contract, not this integer-grid oracle.
            if dpr == round(dpr):
                self.assertLess(different, detail.width() * detail.height() * .02)
            controller.settle()
            _qwait(100)
            image = surface.grabFramebuffer()
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
                    _qwait(600)
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
                    _qwait(350)
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
                    _qwait(350)
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
        _qwait(500)
        controller = window._spatial_controller
        self.assertIsNone(controller.canvas)
        self.assertTrue(self.settings.spatial_availability_pending)
        evidence = Path(os.environ.get("FLUENTQT_SPATIAL_EVIDENCE_DIR", "build/spatial-validation/python-gallery"))
        evidence.mkdir(parents=True, exist_ok=True)

        def screenshot(name):
            QApplication.processEvents()
            _qwait(120)
            self.assertTrue(window.grab().save(str(evidence / (name + ".png"))))

        def click(widget, point=None):
            point = point or widget.rect().center()
            presented = controller.projected_position(widget, point)
            QTest.mouseClick(window, Qt.LeftButton, pos=presented)
            _qwait(160)

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
            # The first device/pipeline initialization can exceed the animation
            # duration on a VM. Wait for its observable result with a deadline.
            for _ in range(100):
                if controller.progress == 1. or controller.renderer_failed:
                    break
                _qwait(50)
            self.assertTrue(self.settings.spatial_available, self.settings.spatial_unavailable_reason)
            self.assertTrue(self.settings.spatial_mode_enabled)
            self.assertEqual(controller.progress, 1.)
            from fluentqt_gallery.spatial_controller import _RhiSurface
            rhi = isinstance(controller.canvas, _RhiSurface)
            # Metal is an opt-in validation path here. Its CAMetalLayer cannot
            # be prepared while the macOS splash still uses raster flushing.
            metal_probe = rhi and QApplication.platformName() == "cocoa"
            if tuple(map(int, qVersion().split(".")[:2])) >= (6, 4) and not metal_probe:
                self.assertIs(window.windowHandle(), first_handle)
                self.assertEqual(window.winId(), first_id)
                self.assertEqual(window.geometry(), first_geometry)
                self.assertEqual(visibility.count(), 0)
            if rhi:
                self.assertTrue(controller.canvas.isReady())
                self.assertGreaterEqual(controller.canvas.statistics()["uploads"], 2)
            else:
                self.assertTrue(all(cache.get("texture") and cache["texture"].isValid()
                                    for cache in controller.canvas.caches))
            screenshot("settings-light")
            click(toggle, QPoint(20, 16))
            _qwait(500)
            self.assertFalse(self.settings.spatial_mode_enabled)
            self.assertEqual(controller.progress, 0.)
            self.assertFalse(controller.filtering)
            self.assertTrue(controller.canvas.isHidden())
            if rhi:
                self.assertEqual(controller.canvas.statistics()["cacheEstimatedBytes"], 0)
            else:
                self.assertEqual(controller.canvas.caches, [{}, {}])
            self.assertTrue(controller.backdrop.isNull())
            window.resize(1220, 820)
            self.settings.set_theme_mode(ThemeMode.Dark)
            _qwait(150)
            self.assertTrue(controller.backdrop.isNull())
            self.settings.set_theme_mode(ThemeMode.Light)
            self.settings.set_spatial_mode_enabled(True)
            _qwait(500)
            click(theme)
            popup = window.findChild(QWidget, "ComboBoxPopup")
            self.assertIsNotNone(popup, str([(w.objectName(), type(w).__name__, w.isVisible()) for w in QApplication.allWidgets() if "Popup" in w.objectName()]))
            self.assertTrue(popup.isVisible())
            screenshot("settings-dropdown")
            QTest.keyClick(popup, Qt.Key_Escape)
            _qwait(250)
            self.assertFalse(popup.isVisible())
            navigation.setCurrentIndex(1)
            _qwait(400)
            self.assertTrue(window._menu_button.isHidden())
            self.assertEqual(controller.canvas.property("galleryRotationAxis"), "X")
            theme.setCurrentIndex(2)
            _qwait(500)
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
            _qwait(600)
            page = window._pages["spatial-view"][1]
            view = page._gallery_sample_results[1].widget._spatial_binding.view
            page.ensureWidgetVisible(view, 0, 24)
            _qwait(300)
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
            _qwait(100)
            scroll.setValue(saved)
            _qwait(300)
            screenshot("spatial-cards-return")
            window.navigate("spatial-item", animated=False)
            _qwait(350)
            page = window._pages["spatial-item"][1]
            view = page._gallery_sample_results[0].widget._spatial_binding.view
            page.ensureWidgetVisible(view, 0, 16)
            _qwait(300)
            screenshot("spatial-item")
            window.navigate("home", animated=False)
            _qwait(250)
            window._maybe_start_intro_tour()
            _qwait(400)
            tour = window._intro_tour
            self.assertLess(window.children().index(controller.canvas), window.children().index(tour._scrim))
            tour.go_to_step(2)
            _qwait(400)
            screenshot("intro-mask")
            tour.finish_tour()
            _qwait(350)
            window.resize(640, 720)
            window.navigate("settings", animated=False)
            _qwait(500)
            screenshot("settings-narrow")
            self.assertEqual(controller.pointer_motion.state(), QAbstractAnimation.Stopped)
            effect.setCurrentIndex(1)
            _qwait(250)
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
            for _ in range(300):
                _qwait(100)
                if controller.capture is not None and controller.progress == 1.:
                    break
            self.assertTrue(self.settings.spatial_available)
            self.assertIsNone(window._splash)
            self.assertIsNone(window._dismissal_splash)
            self.assertEqual(set(window._pages), {"home", "settings"})
            self.assertEqual(window._prewarm_failures, {})
            self.assertIsNone(window._skeleton)
            from PySide6.QtOpenGLWidgets import QOpenGLWidget
            from fluentqt_gallery.spatial_controller import _RhiSurface
            if isinstance(controller.canvas, _RhiSurface):
                self.assertEqual(window.findChildren(QOpenGLWidget), [])
                self.assertEqual(window.findChildren(_RhiSurface), [controller.canvas])
                self.assertTrue(controller.canvas.isReady())
            else:
                self.assertEqual(window.findChildren(QOpenGLWidget), [controller.canvas])
            self.assertIsNotNone(controller.capture)
            self.assertEqual(controller.progress, 1.)
        finally:
            delete(window)
            self.settings.set_window_effect(old_effect)


if __name__ == "__main__":
    unittest.main(verbosity=2)
