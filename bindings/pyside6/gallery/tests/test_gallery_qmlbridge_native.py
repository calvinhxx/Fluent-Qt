"""One strict native GPU roundtrip; readback images are not OS screenshots."""

import json
import os
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import time
import traceback
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import fluentqt
import shiboken6
from fluentqt.qmlbridge import QmlSession, QmlView
from fluentqt.spatial import SpatialRuntime
from PySide6.QtCore import (
    QAbstractAnimation, QCoreApplication, QEvent, QPoint, QPointF, QRect, QSize, Qt, qVersion,
)
from PySide6.QtGui import QColor, QKeyEvent, QMouseEvent, QWheelEvent
from PySide6.QtQuick import QQuickItem, QSGRendererInterface
from PySide6.QtQuickWidgets import QQuickWidget
from PySide6.QtTest import QSignalSpy, QTest
from PySide6.QtWidgets import QApplication

import fluentqt_gallery.settings as settings_module
from fluentqt_gallery.settings import NavigationStyle, ThemeMode, gallery_settings
from fluentqt_gallery.window import GalleryWindow


def _delete(obj):
    if shiboken6.isValid(obj):
        obj.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    QApplication.processEvents()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)


class GalleryQmlBridgeNativeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fluentqt.prepare_high_dpi_application()
        QmlSession.prepareApplication()
        cls.app = QApplication.instance() or QApplication(["qml-bridge-native-roundtrip"])
        cls.app.setProperty("fluentqtGalleryAutomated", True)
        if not fluentqt.initialize_resources():
            raise RuntimeError("FluentQt resources could not be initialized")
        cls.app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    def _wait(self, predicate, description, timeout=10.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if predicate():
                return
            QTest.qWait(25)
        self.fail(f"{description}: {self._state()}")

    def _state(self):
        controller = self.controller
        layer = getattr(controller.canvas, "qml_layer", None)
        return {
            "route": self.window.current_route,
            "requested3d": self.settings.spatial_mode_enabled,
            "rendererReady": controller.renderer_ready,
            "rendererFailed": controller.renderer_failed,
            "renderer": controller.renderer_name,
            "directCompositionRequired": controller._direct_composition_required,
            "presenting": bool(controller.canvas and controller.canvas.property("presenting")),
            "progress": controller.progress,
            "reason": self.settings.spatial_unavailable_reason,
            "qmlTextures": layer.statistics() if layer is not None and shiboken6.isValid(layer) else {},
        }

    def _presenting(self):
        controller = self.controller
        return (controller.renderer_ready and controller.canvas is not None
                and bool(controller.canvas.property("presenting"))
                and controller.progress >= .999
                and controller.motion.state() != QAbstractAnimation.Running)

    def _assert_projected(self, canvas, panel):
        controller = self.controller
        self._wait(lambda: self._presenting() and panel.view.textureId() != 0,
                   "QML and Widgets must remain in the actual GPU presentation")
        self.assertTrue(self.settings.spatial_mode_enabled)
        self.assertTrue(self.settings.spatial_available)
        self.assertFalse(controller.renderer_failed)
        self.assertIs(controller.canvas, canvas)
        self.assertFalse(controller._direct_composition_required)
        self.assertTrue(panel.view.isTextureRenderingEnabled())
        self.assertFalse(panel.view.textureSize().isEmpty())
        self.assertGreater(panel.view.textureRevision(), 0)
        self.assertGreater(panel.view.textureStatistics()["renderedFrames"], 0)
        self.assertGreater(panel.view.textureStatistics()["allocatedBytes"], 0)
        root = panel.view.rootObject()
        self.assertEqual(root.window().rendererInterface().graphicsApi(),
                         QSGRendererInterface.GraphicsApi.OpenGL)
        self.assertIsNotNone(self.window._content_host.graphicsEffect())
        self.assertIsNotNone(self.window._navigation_view.graphicsEffect())
        self.assertTrue(self._quick(panel).isHidden(), "Native Quick must not duplicate GPU content")

    def _quick(self, panel):
        self._wait(lambda: panel.view.status() == QmlView.Status.Ready, "QML must become ready")
        widgets = panel.view.findChildren(QQuickWidget, "qmlQuickWidget")
        self.assertEqual(len(widgets), 1, "Mode handoff must retain exactly one Quick owner")
        return widgets[0]

    def _click(self, widget, point=None):
        point = widget.rect().center() if point is None else point
        target = self.controller.projected_position(widget, point)
        QTest.mouseClick(self.window.windowHandle(), Qt.LeftButton, pos=target)

    def _settle_geometry(self, page, view, code):
        previous = None
        since = time.monotonic()

        def stable():
            nonlocal previous, since
            current = (view.mapTo(page.widget(), QPoint()).y(), view.width(), view.height(),
                       page.widget().width(), page.widget().height(),
                       page.viewport().width(), page.viewport().height(),
                       page.verticalScrollBar().maximum(), page.verticalScrollBar().value(),
                       code.height(), self.controller.projected_position(view, view.rect().center()))
            if current != previous:
                previous, since = current, time.monotonic()
                return False
            return time.monotonic() - since >= .04

        self._wait(stable, "Source and scroll geometry must settle before clipping", timeout=3.)

    def _clip_action(self, page, view, code, action):
        center = action.mapToScene(QPointF(action.width() / 2, action.height() / 2)).toPoint()
        sample = action.mapToScene(QPointF(8, action.height() / 2)).toPoint()
        self._settle_geometry(page, view, code)
        bar = page.verticalScrollBar()
        action_y = view.mapTo(page.widget(), center).y()
        canvas = self.controller.canvas
        attempts = []
        for edge in ("top", "bottom"):
            for inset in (12, 4, 1):
                target = (action_y + inset if edge == "top"
                          else action_y - page.viewport().height() - inset)
                if not bar.minimum() <= target <= bar.maximum():
                    continue
                page.scrollTo(page.horizontalScrollBar().value(), target, False)
                self._settle_geometry(page, view, code)
                placed = QRect(view.mapTo(page.viewport(), QPoint()), view.size())
                visible = placed.intersected(page.viewport().rect())
                frame = canvas.grabFramebuffer()
                projected = [self.controller.projected_position(view, point)
                             for point in (center, sample)]
                pixels = [canvas.mapFrom(self.window, point) * canvas.devicePixelRatioF()
                          for point in projected]
                clipped = all(not page.viewport().rect().contains(view.mapTo(page.viewport(), point))
                              for point in (center, sample))
                inside = all(self.window.rect().contains(point) for point in projected)
                in_frame = not frame.isNull() and all(frame.rect().contains(pixel) for pixel in pixels)
                record = {"edge": edge, "inset": inset, "target": target, "actual": bar.value(),
                          "visibleHeight": visible.height(), "viewHeight": view.height(),
                          "projected": [[p.x(), p.y()] for p in projected],
                          "pixels": [[p.x(), p.y()] for p in pixels],
                          "frameSize": [frame.width(), frame.height()],
                          "clipped": clipped, "insideWindow": inside, "insideFrame": in_frame}
                attempts.append(record)
                if (clipped and inside and in_frame and bar.value() == target
                        and 0 < visible.height() < view.height() and view.textureId() > 0):
                    self.records.append({"label": "clipped-action-placement", **record})
                    return center, sample
        self.fail(f"No partially clipped QML action fits the actual window/GPU frame: {attempts}")

    def _capture(self, label, panel):
        page = self.window._pages["qml-view"][1]
        view = panel.view
        QTest.qWait(400)  # The page's bounded first-show scroll resets precede reveal.
        page.ensureWidgetVisible(view, 16, 16)
        root = view.rootObject()  # Retain the borrowed parent wrapper during findChild use.
        action = root.findChild(QQuickItem, "qmlBridgeAdvanceButton")
        self.assertIsNotNone(action)
        local = action.mapToScene(QPointF(8, action.height() / 2)).toPoint()
        samples = ((local, view.themeObject().property("accent")),
                   (QPoint(view.width() - 24, local.y()), view.themeObject().property("surface")))
        rail = next(item for item in root.findChildren(QQuickItem)
                    if abs(item.height() - 6) < .01 and item.width() > view.width() * .7)
        progress_point = rail.mapToScene(QPointF(rail.width() * .45, 3)).toPoint()
        progress_color = (view.themeObject().property("accent") if panel.progress.value() >= 45
                          else view.themeObject().property("border"))
        background = view.themeObject().property("surface")
        alpha = progress_color.alphaF()
        progress_color = QColor(round(progress_color.red() * alpha + background.red() * (1 - alpha)),
                                round(progress_color.green() * alpha + background.green() * (1 - alpha)),
                                round(progress_color.blue() * alpha + background.blue() * (1 - alpha)))
        samples += ((progress_point, progress_color),)
        canvas = self.controller.canvas
        frame = None
        measured = []

        def matches():
            nonlocal frame, measured
            frame = canvas.grabFramebuffer()
            if frame.isNull():
                return False
            measured = []
            for point, expected in samples:
                projected = self.controller.projected_position(view, point)
                pixel = canvas.mapFrom(self.window, projected) * canvas.devicePixelRatioF()
                if not frame.rect().contains(pixel):
                    return False
                actual = frame.pixelColor(pixel)
                delta = max(abs(actual.red() - expected.red()),
                            abs(actual.green() - expected.green()),
                            abs(actual.blue() - expected.blue()))
                measured.append({"projected": [projected.x(), projected.y()],
                                 "pixel": [pixel.x(), pixel.y()],
                                 "actual": [actual.red(), actual.green(), actual.blue()],
                                 "expected": [expected.red(), expected.green(), expected.blue()],
                                 "maximumRgbDifference": delta})
            return all(sample["maximumRgbDifference"] <= 12 for sample in measured)

        self._wait(matches, "The final Spatial framebuffer must contain both QML color regions")
        self.assertTrue(frame.save(str(self.evidence / (label + "-composite.png"))))
        painted = self.window.grab()
        self.assertTrue(painted.save(str(self.evidence / (label + "-window.png"))))
        self.records.append({
            "label": label, **self._state(), "qtVersion": qVersion(),
            "platform": self.app.platformName(), "samples": measured,
            "textureId": view.textureId(), "textureRevision": view.textureRevision(),
            "textureSize": [view.textureSize().width(), view.textureSize().height()],
            "textureStatistics": view.textureStatistics(),
            "widgetProgress": panel.progress.value(),
            "qmlProgress": root.property("previewProgress"),
            "evidenceKind": "Spatial GPU framebuffer readback and QWidget.grab; not an OS screenshot",
        })

    def _assert_failed_qml_frames(self, canvas, panel):
        root = panel.view.rootObject()
        for failure in ("budget", "composition"):
            self._assert_projected(canvas, panel)
            panel.progress.setValue(40)
            action = root.findChild(QQuickItem, "qmlBridgeAdvanceButton")
            self.assertIsNotNone(action)
            panel.view.setFocus()
            action.forceActiveFocus()
            action_point = action.mapToScene(QPointF(action.width() / 2, action.height() / 2)).toPoint()
            projected = self.controller.projected_position(panel.view, action_point)
            layer = canvas.qml_layer
            errors = QSignalSpy(layer.renderingFailed)
            before = canvas.grabFramebuffer()
            self.assertFalse(before.isNull())
            requested = QSignalSpy(self.settings.spatialModeEnabledChanged)
            canvas.makeCurrent()
            if failure == "budget":
                self.assertFalse(layer.prepareFrame(canvas.context(), canvas.qml_source(),
                                                    canvas.plan[0], 0))
            else:
                self.assertEqual(layer.compose(panel.view.textureId(), QSize(1, 1), 0), 0)
            canvas.doneCurrent()
            failed = canvas.grabFramebuffer()
            self.assertEqual(errors.count(), 1)
            self.assertEqual(failed, before,
                             "The queued fallback must retain the completed GPU frame")
            self.assertTrue(canvas.property("presenting"))
            self.assertTrue(panel.view.isTextureRenderingEnabled())
            self.assertTrue(canvas.property("galleryQmlFailurePending"))
            # Keep the fallback queued while exercising actual input dispatch.
            for kind, buttons in ((QEvent.MouseButtonPress, Qt.LeftButton),
                                  (QEvent.MouseButtonRelease, Qt.NoButton)):
                event = QMouseEvent(kind, QPointF(projected),
                                    QPointF(self.window.mapToGlobal(projected)),
                                    Qt.LeftButton, buttons, Qt.NoModifier)
                QCoreApplication.sendEvent(self.window, event)
            action.forceActiveFocus()
            for kind in (QEvent.KeyPress, QEvent.KeyRelease):
                QCoreApplication.sendEvent(panel.view, QKeyEvent(kind, Qt.Key_Return, Qt.NoModifier))
            self.assertEqual(panel.progress.value(), 40,
                             "The retained frame must reject input for the changed scene")
            canvas.resize(canvas.size() + QSize(24, 16))
            resized = canvas.grabFramebuffer()
            self.assertFalse(resized.isNull())
            self.assertNotEqual(resized.size(), before.size())
            visible = sum(resized.pixelColor(x, y).alpha() > 0
                          for y in range(0, resized.height(), 16)
                          for x in range(0, resized.width(), 16))
            self.assertGreater(visible, resized.width() * resized.height() // (16 * 16 * 4))
            native = self._quick(panel)
            self._wait(lambda: canvas.isHidden() and not panel.view.isTextureRenderingEnabled()
                       and native.isVisible() and native.geometry() == panel.view.contentsRect(),
                       "The queued failure must restore native QML after rendering returns")
            self.assertIs(panel.view.rootObject(), root)
            self.assertTrue(self._quick(panel).isVisible())
            self.assertTrue(self.settings.spatial_mode_enabled)
            self.assertEqual(requested.count(), 0)
            self.assertFalse(canvas.property("galleryQmlFailurePending"))
            quick = self._quick(panel)
            QTest.mouseClick(quick, Qt.LeftButton, pos=action_point)
            self.assertEqual(panel.progress.value(), 50)
            QTest.keyClick(quick, Qt.Key_Return)
            self.assertEqual(panel.progress.value(), 60)
            self.records.append({"label": f"{failure}-failure-retained-frame", **self._state()})
            self.settings.set_spatial_mode_enabled(False)
            self.settings.set_spatial_mode_enabled(True)
            self._assert_projected(canvas, panel)

    def test_real_gpu_projects_qml_and_retains_the_scene_across_mode_handoffs(self):
        self.assertEqual(self.app.platformName(), "cocoa", "This local native gate requires Cocoa")
        self.assertNotEqual(os.environ.get("QT_QUICK_BACKEND"), "software")
        self.assertEqual(os.environ.get("FLUENT_QT_GALLERY_DISABLE_3D", "0"), "0")
        self.evidence = Path(os.environ.get(
            "FLUENTQT_QMLBRIDGE_NATIVE_EVIDENCE_DIR",
            str(Path(__file__).resolve().parents[4] / "build/qmlbridge-regression/python-native-roundtrip"),
        ))
        self.evidence.mkdir(parents=True, exist_ok=True)
        self.records = []
        old_name, old_organization = self.app.applicationName(), self.app.organizationName()
        old_theme, old_motion = fluentqt.current_theme(), fluentqt.current_motion_mode()
        old_settings = settings_module._SETTINGS
        old_hook = sys.excepthook
        callback_errors = []
        sys.excepthook = lambda *error: callback_errors.append("".join(traceback.format_exception(*error)))
        self.app.setApplicationName("FluentQtQmlBridgeNativeRegression")
        self.app.setOrganizationName("FluentQtNativeTests")
        self.window = None
        settings_module._SETTINGS = None
        try:
            with TemporaryDirectory(prefix="fluentqt-qml-native-") as temporary, \
                    patch.object(settings_module, "persistence_available", return_value=True), \
                    patch.object(settings_module, "config_file_path", return_value=Path(temporary) / "config.ini"):
                self.settings = gallery_settings()
                saved = {name: getattr(self.settings, name) for name in (
                    "theme_mode", "motion_mode", "navigation_style", "window_effect",
                    "spatial_mode_enabled", "intro_completed", "home_particles_enabled")}
                try:
                    self.settings.set_motion_mode(fluentqt.MotionMode.Full)
                    self.settings.set_theme_mode(ThemeMode.Dark)
                    self.settings.set_navigation_style(NavigationStyle.Left)
                    self.settings.set_window_effect(1)
                    self.settings.set_intro_completed(True)
                    self.settings.set_home_particles_enabled(False)
                    self.settings.set_spatial_mode_enabled(False)
                    self.window = GalleryWindow(startup_visuals=False)
                    self.window.resize(1180, 760)
                    self.window.navigate("button", animated=False)
                    self.window.show()
                    self.window.raise_()
                    self.window.activateWindow()
                    self.assertTrue(QTest.qWaitForWindowExposed(self.window, 5000))
                    self.controller = self.window._spatial_controller
                    self.settings.set_spatial_mode_enabled(True)
                    self._wait(self._presenting, "The real GPU must present the Button page")
                    self.assertTrue(SpatialRuntime.isHardwareRenderer(self.controller.renderer_name))
                    canvas = self.controller.canvas
                    self.records.append({"label": "initial-button", **self._state(),
                                         "canvasClass": canvas.metaObject().className(),
                                         "surfaceType": str(self.window.windowHandle().surfaceType())})
                    requested = QSignalSpy(self.settings.spatialModeEnabledChanged)

                    self.window.navigate("qml-view", animated=False)
                    page = self.window._pages["qml-view"][1]
                    self._wait(lambda: len(page._gallery_sample_results) == 6,
                               "All six atomic examples must be built")
                    widgets = {result.sample_id: result.widget for result in page._gallery_sample_results}
                    panel = SimpleNamespace(
                        widget=widgets["qml-view-action"],
                        view=page.findChild(QmlView, "qmlBridgeActionView"),
                        progress=page.findChild(fluentqt.ProgressBar, "qmlBridgeProgressIndicator"),
                    )
                    view = panel.view
                    context_view = page.findChild(QmlView, "qmlBridgeContextView")
                    title_edit = page.findChild(fluentqt.LineEdit, "qmlBridgeTitleEdit")
                    lifecycle_view = page.findChild(QmlView, "qmlBridgeLifecycleView")
                    self.assertEqual(len(page.findChildren(QmlView)), 6)
                    self._wait(lambda: all(v.status() == QmlView.Status.Ready
                                           for v in page.findChildren(QmlView)),
                               "Each atomic scene must initially be ready")
                    roots = {v.objectName(): v.rootObject() for v in page.findChildren(QmlView)}
                    self._wait(lambda: self._presenting() and view.status() == QmlView.Status.Ready
                               and view.isTextureRenderingEnabled(), "The projected scene must be ready")
                    QTest.qWait(400)
                    page.ensureWidgetVisible(view, 16, 16)
                    self._assert_projected(canvas, panel)
                    quick = self._quick(panel)
                    root, engine = view.rootObject(), quick.engine()
                    self._capture("initial-qml", panel)
                    self.assertEqual(panel.progress.value(), 40)

                    bar = page.verticalScrollBar()
                    self.assertGreater(bar.maximum(), 0)
                    viewport = page.viewport()
                    for delta in (-120, 120):
                        page.ensureWidgetVisible(view, 16, 16)
                        background = QPoint(view.width() - 24, view.height() // 2)
                        self._wait(lambda: viewport.rect().contains(view.mapTo(viewport, background)),
                                   "The QML wheel point must be inside the clipping viewport")
                        native_hit = self.controller.navigation.childAt(
                            view.mapTo(self.controller.navigation, background))
                        self.assertIs(native_hit, view, "Wheel must target QML blank space, not a Slider")
                        target = self.controller.projected_position(view, background)
                        prior = bar.value()
                        progress_before_wheel = panel.progress.value()
                        event = QWheelEvent(QPointF(target),
                                            QPointF(self.window.mapToGlobal(target)),
                                            QPoint(0, delta), QPoint(0, delta),
                                            Qt.NoButton, Qt.NoModifier, Qt.ScrollUpdate, False)
                        QApplication.sendEvent(self.window, event)
                        self._wait(lambda: bar.value() != prior, "Projected wheel must scroll")
                        self.assertEqual(panel.progress.value(), progress_before_wheel)
                        self.records.append({"label": "qml-background-wheel", "before": prior,
                                             "after": bar.value(), "maximum": bar.maximum(),
                                             "delta": delta, "projected": [target.x(), target.y()],
                                             "hit": native_hit.objectName(), **self._state()})
                        self._assert_projected(canvas, panel)

                    page.ensureWidgetVisible(title_edit, 16, 16)
                    QTest.qWait(50)
                    self._click(title_edit)
                    title_edit.selectAll()
                    QTest.keyClicks(title_edit, "Edited in the native Gallery")
                    panel.progress.setValue(65)
                    self._wait(lambda: context_view.rootObject().property("previewTitle") == title_edit.text(),
                               "Widgets edits must reach the same live QML scene")
                    self.assertEqual(root.property("previewProgress"), 65)
                    page.ensureWidgetVisible(view, 16, 16)
                    QTest.qWait(50)
                    action = root.findChild(QQuickItem, "qmlBridgeAdvanceButton")
                    self.assertTrue(shiboken6.isValid(action))
                    center = action.mapToScene(QPointF(action.width() / 2, action.height() / 2)).toPoint()
                    self._click(view, center)
                    self._wait(lambda: panel.progress.value() == 75,
                               "The projected physical click must advance exactly once")
                    self.assertEqual(title_edit.text(), "Edited in the native Gallery")
                    code = page._gallery_sample_cards[list(widgets).index("qml-view-action")]._gallery_source_expander
                    code.setAnimationEnabled(False)
                    focused = QApplication.focusWidget()
                    if focused is not None:
                        focused.clearFocus()
                    code.setExpanded(True)
                    center, clipped_sample = self._clip_action(page, view, code, action)
                    self._click(view, center)
                    QTest.qWait(80)
                    self.assertEqual(panel.progress.value(), 75, "Clipped QML must not receive a physical click")
                    self.assertGreater(view.textureId(), 0)
                    projected = self.controller.projected_position(view, clipped_sample)
                    frame = canvas.grabFramebuffer()
                    pixel = canvas.mapFrom(self.window, projected) * canvas.devicePixelRatioF()
                    self.assertTrue(frame.rect().contains(pixel))
                    actual = frame.pixelColor(pixel)
                    accent = view.themeObject().property("accent")
                    self.assertGreater(max(abs(actual.red() - accent.red()),
                                           abs(actual.green() - accent.green()),
                                           abs(actual.blue() - accent.blue())), 12,
                                       "A projected QML texture must not escape its scroll clip")
                    self.assertTrue(frame.save(str(self.evidence / "clipped-action-composite.png")))
                    code.setExpanded(False)
                    self._settle_geometry(page, view, code)
                    page.ensureWidgetVisible(view, 16, 16)

                    for _ in range(2):
                        self.settings.set_spatial_mode_enabled(False)
                        self._wait(lambda: not canvas.property("presenting")
                                   and not view.isTextureRenderingEnabled() and quick.isVisible(),
                                   "Explicit 2D must restore native Quick")
                        self.assertIs(view.rootObject(), root)
                        self.assertIs(quick.engine(), engine)
                        self.assertIs(root.findChild(QQuickItem, "qmlBridgeAdvanceButton"), action)
                        self.assertEqual(panel.progress.value(), 75)
                        self.settings.set_spatial_mode_enabled(True)
                        self._assert_projected(canvas, panel)
                        self.assertIs(view.rootObject(), root)
                        self.assertIs(quick.engine(), engine)
                        for v in page.findChildren(QmlView):
                            self.assertIs(v.rootObject(), roots[v.objectName()])
                    self.assertEqual(requested.count(), 4)
                    QTest.qWait(400)
                    revision, frames = view.textureRevision(), view.textureStatistics()["renderedFrames"]
                    QTest.mouseMove(self.window.windowHandle(), QPoint(self.window.width() // 2, 24))
                    QTest.qWait(150)
                    self.assertEqual(view.textureRevision(), revision)
                    self.assertEqual(view.textureStatistics()["renderedFrames"], frames)

                    page.ensureWidgetVisible(page._gallery_theme_button, 16, 16)
                    QTest.qWait(50)
                    self._click(page._gallery_theme_button)
                    self._wait(lambda: not view.themeObject().property("dark"),
                               "The local preview must switch to Light while staying projected")
                    self.assertEqual(self.settings.theme_mode, ThemeMode.Dark)
                    deactivate = page.findChild(fluentqt.Button, "qmlBridgeDeactivate")
                    reload_button = page.findChild(fluentqt.Button, "qmlBridgeReload")
                    for _ in range(2):
                        page.ensureWidgetVisible(deactivate, 16, 16)
                        self._click(deactivate)
                        self._wait(lambda: lifecycle_view.status() == QmlView.Status.Unloaded,
                                   "Only the lifecycle scene must unload")
                        self.assertIsNone(lifecycle_view.rootObject())
                        self.assertEqual(lifecycle_view.textureId(), 0)
                        self.assertEqual(len(page.findChildren(QQuickWidget)), 5)
                        self.assertIs(view.rootObject(), root)
                        self.assertIs(quick.engine(), engine)
                        self.assertEqual(panel.progress.value(), 75)
                        self._wait(self._presenting, "The other atomic cards must stay in 3D")
                        self._click(reload_button)
                        self._wait(lambda: lifecycle_view.status() == QmlView.Status.Ready
                                   and lifecycle_view.isTextureRenderingEnabled(),
                                   "Reload must recreate only the lifecycle scene")
                        self.assertEqual(len(page.findChildren(QQuickWidget)), 6)
                        page.ensureWidgetVisible(view, 16, 16)
                        self._assert_projected(canvas, panel)
                        self.assertIs(view.rootObject(), root)
                        self.assertEqual(panel.progress.value(), 75)
                    error_view = page.findChild(QmlView, "qmlBridgeErrorView")
                    missing = page.findChild(fluentqt.Button, "qmlBridgeLoadMissing")
                    restore = page.findChild(fluentqt.Button, "qmlBridgeRestore")
                    page.ensureWidgetVisible(missing, 16, 16)
                    self._click(missing)
                    self._wait(lambda: error_view.status() == QmlView.Status.Error,
                               "The Error card must expose the actual loading failure")
                    self.assertTrue(error_view.lastError())
                    self.assertIsNone(error_view.rootObject())
                    self.assertIs(view.rootObject(), root)
                    self.assertTrue(self._presenting())
                    self._click(restore)
                    self._wait(lambda: error_view.status() == QmlView.Status.Ready,
                               "Restore must load the Error card's Basic scene")
                    self._capture("after-local-theme-reloads", panel)
                    retained_root = view.rootObject()
                    retained_roots = {v.objectName(): v.rootObject() for v in page.findChildren(QmlView)}
                    self.window.navigate("button", animated=False)
                    self._wait(self._presenting, "Other pages must remain in real GPU presentation")
                    self.assertIs(self.controller.canvas, canvas)
                    self.assertIs(view.rootObject(), retained_root)
                    cached_frames = view.textureStatistics()["renderedFrames"]
                    QTest.qWait(150)
                    self.assertEqual(view.textureStatistics()["renderedFrames"], cached_frames)
                    self.records.append({"label": "restored-button", **self._state()})
                    self.window.navigate("qml-view", animated=False)
                    self._wait(lambda: self._presenting() and view.status() == QmlView.Status.Ready
                               and view.isTextureRenderingEnabled(), "The cached scene must remain ready")
                    QTest.qWait(400)
                    page.ensureWidgetVisible(view, 16, 16)
                    self._assert_projected(canvas, panel)
                    self.assertIs(widgets["qml-view-action"], panel.widget)
                    self.assertIs(view.rootObject(), retained_root)
                    for v in page.findChildren(QmlView):
                        self.assertIs(v.rootObject(), retained_roots[v.objectName()])
                    self._capture("returned-qml", panel)
                    self.assertEqual(requested.count(), 4)
                    self._assert_failed_qml_frames(canvas, panel)
                    self.assertEqual(callback_errors, [])
                finally:
                    if self.window is not None:
                        self.window.hide()
                        _delete(self.window)
                        self.window = None
                    for name, value in saved.items():
                        getattr(self.settings, "set_" + name)(value)
                    _delete(self.settings)
        finally:
            settings_module._SETTINGS = old_settings
            fluentqt.set_theme(old_theme)
            fluentqt.set_motion_mode(old_motion)
            self.app.setApplicationName(old_name)
            self.app.setOrganizationName(old_organization)
            sys.excepthook = old_hook
            (self.evidence / "roundtrip.json").write_text(
                json.dumps({"records": self.records, "callbackErrors": callback_errors}, indent=2),
                encoding="utf-8",
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
