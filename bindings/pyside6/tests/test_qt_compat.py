"""Shared lifetime, event and paint adapter runtime contracts."""
from types import SimpleNamespace
from functools import partial
import gc
import threading
import unittest
from unittest.mock import Mock, patch

from PySide6.QtCore import QEvent, QObject, QPoint, QPointF, QRect, QSize, QTimer, Qt
from PySide6.QtGui import QColor, QFont, QImage, QPaintDevice, QPainter, QPixmap, QRegion, QTransform
from PySide6.QtTest import QSignalSpy, QTest
from PySide6.QtWidgets import QApplication, QMenu, QWidget
from shiboken6 import isValid

from fluentqt import CommandPalette
from fluentqt._qt_compat import delete_qobject
from fluentqt._qt_events import is_display_scale_change_event
from fluentqt._qt_painting import (
    device_pixel_ratio_metric, font_style_strategy, forward_paint_array, region_rects,
    image_device_pixel_ratio,
    apply_render_target_scale, set_render_target_device_pixel_ratio,
    draw_image_at, draw_pixmap_at,
)
from fluentqt._qt_widgets import application_is_saving_session
from fluentqt import _qt_widgets
from fluentqt._fluentqt import objectHasCurrentThreadAffinityForBinding


create_owned_widget = partial(_qt_widgets.create_owned_widget,
                              affinity=objectHasCurrentThreadAffinityForBinding)


class QtPopupGeometryCompatTest(unittest.TestCase):
    def test_fractional_scale_keeps_all_frame_pixels_inside_screen(self):
        for dpr in (0.5, 0.75, 1.0, 1.25, 1.5, 2.0):
            for origin in (QPoint(), QPoint(-3840, -120)):
                with self.subTest(dpr=dpr, origin=origin):
                    available = QRect(origin + QPoint(0, 40), QRect(0, 0, 3840, 2024).size())
                    screen = SimpleNamespace(availableGeometry=lambda: available,
                                             geometry=lambda: QRect(origin.x(), origin.y(), 3840, 2160),
                                             devicePixelRatio=lambda: dpr)
                    popup = SimpleNamespace(frameGeometry=lambda: QRect(100, 100, 129, 57),
                                            pos=lambda: QPoint(104, 106))
                    transform = QTransform.fromScale(dpr, dpr)
                    frame_offset = QPoint(-4, -6)
                    for requested in (origin - QPoint(100, 100), origin + QPoint(4000, 2200)):
                        position = _qt_widgets.clamp_popup_position(popup, requested, screen)
                        native_top_left = transform.map(position + frame_offset - origin)
                        native_size = transform.map(QPoint(129, 57))
                        native_frame = QRect(native_top_left.x(), native_top_left.y(),
                                             native_size.x(), native_size.y())
                        native_available = transform.mapRect(available.translated(-origin))
                        self.assertTrue(native_available.contains(native_frame),
                                        (native_available, native_frame))
                    interior = origin + QPoint(200, 250)
                    self.assertEqual(_qt_widgets.clamp_popup_position(popup, interior, screen), interior)

    def test_half_scale_rounding_does_not_push_right_edge_outside(self):
        screen = SimpleNamespace(availableGeometry=lambda: QRect(0, 0, 3840, 2064),
                                 geometry=lambda: QRect(0, 0, 3840, 2160),
                                 devicePixelRatio=lambda: 0.5)
        popup = SimpleNamespace(frameGeometry=lambda: QRect(0, 0, 129, 56),
                                pos=lambda: QPoint())
        self.assertEqual(_qt_widgets.clamp_popup_position(popup, QPoint(3900, 1942), screen),
                         QPoint(3710, 1942))


class QtWindowGeometryCompatTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def test_prepared_window_maps_maximized_and_restores_its_normal_size(self):
        window = QWidget()
        self.addCleanup(delete_qobject, window)
        self.addCleanup(window.close)
        available = window.screen().availableGeometry()
        geometry = QRect(available.topLeft() + QPoint(40, 30), QSize(420, 300))
        first_show_states = []

        class FirstShow(QObject):
            def eventFilter(self, watched, event):
                if event.type() == QEvent.Type.Show and not first_show_states:
                    first_show_states.append(watched.windowState())
                return False

        probe = FirstShow(window)
        window.installEventFilter(probe)
        _qt_widgets.prepare_window_normal_geometry(window, geometry)
        self.assertIsNotNone(window.windowHandle())
        self.assertEqual(window.geometry(), geometry)
        window.setWindowState(window.windowState() | Qt.WindowState.WindowMaximized)
        window.show()
        # The first frame is maximized; the prepared geometry is the restore target.
        self.assertTrue(first_show_states and first_show_states[0] & Qt.WindowState.WindowMaximized)
        window.showNormal()
        for _ in range(100):
            if not window.isMaximized() and window.size() == geometry.size():
                break
            QTest.qWait(20)
        self.assertEqual(window.size(), geometry.size())


class QtLifetimeCompatTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def test_pool_deletion_never_destroys_application(self):
        class Child(QWidget):
            pass

        # Pool allocation plus CTest's malloc perturbation exposes the old
        # uninitialized Shiboken singleton flag; no private layout assumptions.
        objects = [kind() for _ in range(100) for kind in (QObject, QWidget, Child)]
        for obj in objects:
            delete_qobject(obj)
            self.assertFalse(isValid(obj))
            self.assertTrue(isValid(self.app))
            self.assertIs(QApplication.instance(), self.app)

    def test_parent_child_and_repeated_deletion(self):
        parent = QWidget()
        child = QObject(parent)
        spy = QSignalSpy(child.destroyed)
        delete_qobject(parent)
        self.assertFalse(isValid(parent))
        self.assertFalse(isValid(child))
        self.assertEqual(spy.count(), 1)
        delete_qobject(parent)
        delete_qobject(child)
        delete_qobject(None)
        self.assertEqual(spy.count(), 1)

    def test_native_created_object_is_destroyed_before_return(self):
        menu = QMenu()
        action = menu.addAction("Native action")
        action.setParent(None)
        destroyed = QSignalSpy(action.destroyed)
        delete_qobject(action)
        self.assertFalse(isValid(action))
        self.assertEqual(destroyed.count(), 1)
        self.assertEqual(menu.actions(), [])
        self.assertTrue(isValid(self.app))

    def test_final_native_wrapper_is_invalidated_after_destruction(self):
        target = CommandPalette()
        destroyed = QSignalSpy(target.destroyed)
        delete_qobject(target)
        self.assertFalse(isValid(target))
        self.assertEqual(destroyed.count(), 1)
        self.assertTrue(isValid(self.app))
        delete_qobject(target)
        self.assertEqual(destroyed.count(), 1)

    def test_delivery_does_not_flush_other_deletions_or_timers(self):
        target, unrelated = QObject(), QObject()
        timer = QTimer()
        timer.setSingleShot(True)
        fired = QSignalSpy(timer.timeout)
        timer.start(0)
        unrelated.deleteLater()
        try:
            delete_qobject(target)
            self.assertFalse(isValid(target))
            self.assertTrue(isValid(unrelated))
            self.assertEqual(fired.count(), 0)
        finally:
            timer.stop()
            delete_qobject(unrelated)

    def test_application_and_non_qobjects_are_rejected(self):
        with self.assertRaises(ValueError):
            delete_qobject(self.app)
        with self.assertRaises(TypeError):
            delete_qobject(object())
        self.assertTrue(isValid(self.app))

    def test_destruction_from_timer_callback_completes_before_return(self):
        target, timer = QObject(), QTimer()
        timer.setSingleShot(True)
        result = []

        def destroy_target():
            delete_qobject(target)
            result.append(isValid(target))

        timer.timeout.connect(destroy_target)
        timer.start(0)
        self.app.processEvents()
        self.assertEqual(result, [False])
        self.assertTrue(isValid(self.app))


class QtWidgetFactoryCompatTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def test_preserves_parent_and_unparented_collection(self):
        owner, parent = QObject(), QWidget()
        destroyed = []
        try:
            widget = create_owned_widget(owner, QWidget, lambda: True, lambda _: True)
            widget.destroyed.connect(lambda: destroyed.append(True))
            del widget
            gc.collect()
            self.assertEqual(destroyed, [True])
            child = create_owned_widget(owner, QWidget, lambda: True, lambda _: True, parent)
            self.assertIs(child.parentWidget(), parent)
            delete_qobject(parent)
            self.assertFalse(isValid(child))
        finally:
            delete_qobject(owner)
            delete_qobject(parent)

    def test_rejects_worker_before_reading_policy_or_constructing(self):
        owner = QObject()
        active, factory, attach = Mock(return_value=True), Mock(return_value=None), Mock()
        results = []
        worker = threading.Thread(target=lambda: results.append(
            create_owned_widget(owner, factory, active, attach)))
        try:
            worker.start()
            worker.join(timeout=5)
            self.assertFalse(worker.is_alive())
            self.assertEqual(results, [None])
            active.assert_not_called()
            factory.assert_not_called()
            attach.assert_not_called()
            active.return_value = False
            self.assertIsNone(create_owned_widget(owner, factory, active, attach))
            factory.assert_not_called()
            attach.assert_not_called()
        finally:
            delete_qobject(owner)

    def test_rechecks_parent_after_policy_callback(self):
        owner, parent = QObject(), QWidget()
        factory = Mock(return_value=None)
        def active():
            delete_qobject(parent)
            return True
        try:
            self.assertIsNone(create_owned_widget(owner, factory, active, lambda _: True, parent))
            factory.assert_not_called()
        finally:
            delete_qobject(owner)
            delete_qobject(parent)

    def test_rejects_parent_changes_during_factory_or_attach(self):
        for phase in ("factory", "attach"):
            for relationship in ("parent-to-other", "parent-to-none", "none-to-other"):
                with self.subTest(phase=phase, relationship=relationship):
                    owner, parent, other = QObject(), QWidget(), QWidget()
                    requested = None if relationship == "none-to-other" else parent
                    replacement = None if relationship == "parent-to-none" else other
                    created = []

                    def factory(requested_parent):
                        widget = QWidget(requested_parent)
                        created.append(widget)
                        if phase == "factory":
                            widget.setParent(replacement)
                        return widget

                    def attach_widget(widget):
                        if phase == "attach":
                            widget.setParent(replacement)
                        return True

                    attach = Mock(side_effect=attach_widget)
                    try:
                        self.assertIsNone(create_owned_widget(
                            owner, factory, lambda: True, attach, requested))
                        self.assertFalse(isValid(created[0]))
                        self.assertEqual(attach.call_count, 0 if phase == "factory" else 1)
                        self.assertTrue(all(isValid(obj) for obj in (owner, parent, other)))
                        self.assertEqual(parent.children(), [])
                        self.assertEqual(other.children(), [])
                    finally:
                        for obj in created + [owner, parent, other]:
                            delete_qobject(obj)

    def test_rejects_destroyed_parent_when_factory_output_survives(self):
        for phase in ("factory", "attach"):
            with self.subTest(phase=phase):
                owner, parent, other = QObject(), QWidget(), QWidget()
                created = []

                def replace_parent(widget):
                    widget.setParent(other)
                    delete_qobject(parent)

                def factory(requested_parent):
                    widget = QWidget(requested_parent)
                    created.append(widget)
                    if phase == "factory":
                        replace_parent(widget)
                    return widget

                def attach_widget(widget):
                    if phase == "attach":
                        replace_parent(widget)
                    return True

                attach = Mock(side_effect=attach_widget)
                try:
                    self.assertIsNone(create_owned_widget(
                        owner, factory, lambda: True, attach, parent))
                    self.assertFalse(isValid(created[0]))
                    self.assertFalse(isValid(parent))
                    self.assertTrue(isValid(owner))
                    self.assertTrue(isValid(other))
                    self.assertEqual(other.children(), [])
                    self.assertEqual(attach.call_count, 0 if phase == "factory" else 1)
                finally:
                    for obj in created + [owner, parent, other]:
                        delete_qobject(obj)

    def test_cleans_failed_and_reentrant_attachments(self):
        for action in ("reject", "destroy-owner", "destroy-widget", "raise"):
            with self.subTest(action=action):
                owner = QObject()
                created = []
                def factory(parent):
                    created.append(QWidget(parent))
                    return created[-1]
                def attach(widget):
                    if action == "destroy-owner":
                        delete_qobject(owner)
                    elif action == "destroy-widget":
                        delete_qobject(widget)
                    elif action == "raise":
                        raise RuntimeError("Attachment failed")
                    return action != "reject"
                try:
                    if action == "raise":
                        with self.assertRaisesRegex(RuntimeError, "Attachment failed"):
                            create_owned_widget(owner, factory, lambda: True, attach)
                    else:
                        self.assertIsNone(create_owned_widget(owner, factory, lambda: True, attach))
                    self.assertEqual(len(created), 1)
                    self.assertFalse(isValid(created[0]))
                finally:
                    delete_qobject(owner)


class QtAdapterCompatTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def test_system_color_scheme_handles_missing_and_unknown_apis(self):
        scheme = SimpleNamespace(Dark=1, Light=2, Unknown=0)
        for qt, hints, expected in (
            (SimpleNamespace(), SimpleNamespace(), None),
            (SimpleNamespace(ColorScheme=scheme), SimpleNamespace(), None),
            (SimpleNamespace(ColorScheme=scheme), SimpleNamespace(colorScheme=lambda: 0), None),
            (SimpleNamespace(ColorScheme=scheme), SimpleNamespace(colorScheme=lambda: 1), "dark"),
            (SimpleNamespace(ColorScheme=scheme), SimpleNamespace(colorScheme=lambda: 2), "light"),
        ):
            with self.subTest(expected=expected), patch.object(_qt_widgets, "Qt", qt), patch.object(
                _qt_widgets, "QGuiApplication", SimpleNamespace(styleHints=lambda: hints)
            ):
                self.assertEqual(_qt_widgets.system_color_scheme(), expected)

    def test_accessible_announcement_uses_available_event_shape(self):
        self.app = QApplication.instance() or QApplication([])
        target = QWidget()
        sent = []
        try:
            with patch.object(_qt_widgets.QAccessible, "updateAccessibility", sent.append):
                _qt_widgets.send_accessible_announcement(None, "Zoom 110%")
                _qt_widgets.send_accessible_announcement(target, "")
                self.assertEqual(sent, [])
                with patch.object(_qt_widgets, "_QtGui", SimpleNamespace()):
                    _qt_widgets.send_accessible_announcement(target, "Zoom 110%")
                self.assertEqual(sent[-1].type(), _qt_widgets.QAccessible.Event.Alert)
                announcement = getattr(_qt_widgets._QtGui, "QAccessibleAnnouncementEvent", None)
                if announcement is not None:
                    politeness = _qt_widgets.QAccessible.AnnouncementPoliteness
                    _qt_widgets.send_accessible_announcement(target, "Zoom 125%")
                    self.assertIsInstance(sent[-1], announcement)
                    self.assertEqual(sent[-1].message(), "Zoom 125%")
                    self.assertEqual(sent[-1].politeness(), politeness.Polite)
                    _qt_widgets.send_accessible_announcement(target, "Zoom 150%", assertive=True)
                    self.assertEqual(sent[-1].politeness(), politeness.Assertive)
        finally:
            delete_qobject(target)

    def test_paint_array_forwarding_keeps_legacy_pointer_and_count(self):
        calls = []
        pointer = object()
        engine = SimpleNamespace(
            syncState=lambda: calls.append("sync"),
            drawRects=lambda *args: calls.append(args),
        )
        painter = SimpleNamespace(
            paintEngine=lambda: engine,
            drawRects=lambda *args: calls.append(args),
        )
        forward_paint_array(painter, "drawRects", pointer, (2,))
        self.assertEqual(calls, ["sync", (pointer, 2)])
        calls.clear()
        rects = [QRect(1, 2, 3, 4), QRect(10, 20, 30, 40)]
        forward_paint_array(painter, "drawRects", rects, ())
        self.assertEqual(calls, [(rects,)])

    def test_region_font_and_session_adapters_accept_available_api_shapes(self):
        region = QRegion(QRect(0, 0, 3, 4)) | QRegion(QRect(8, 9, 3, 4))
        restored = QRegion()
        for rect in region_rects(region):
            restored |= QRegion(rect)
        self.assertEqual(restored, region)
        self.assertEqual(list(region_rects(SimpleNamespace(rects=lambda: [QRect(1, 2, 3, 4)]))),
                         [QRect(1, 2, 3, 4)])
        for strategy in (QFont.PreferAntialias, QFont.PreferNoShaping):
            self.assertEqual(font_style_strategy(strategy), strategy)
            legacy = int(getattr(strategy, "value", strategy))
            self.assertEqual(font_style_strategy(legacy), strategy)
        for app in (None, SimpleNamespace()):
            self.assertFalse(application_is_saving_session(app))
        for saving in (False, True):
            self.assertEqual(application_is_saving_session(
                SimpleNamespace(isSavingSession=lambda: saving)), saving)

    def test_display_scale_events_match_available_runtime_api(self):
        expected = {
            event_type for event_type in (
                getattr(QEvent.Type, "ScreenChangeInternal", None),
                getattr(QEvent.Type, "DevicePixelRatioChange", None),
            ) if event_type is not None
        }
        self.assertFalse(is_display_scale_change_event(None))
        for event_type in expected | {QEvent.Resize, QEvent.Move, QEvent.User}:
            self.assertEqual(is_display_scale_change_event(QEvent(event_type)), event_type in expected)

    def test_half_dpr_bitmaps_fill_their_logical_extent(self):
        source = QImage(40, 30, QImage.Format_ARGB32_Premultiplied)
        source.fill(Qt.blue)
        source.setDevicePixelRatio(0.5)
        for bitmap, draw in ((source, draw_image_at), (QPixmap.fromImage(source), draw_pixmap_at)):
            result = QImage(90, 70, QImage.Format_ARGB32_Premultiplied)
            result.fill(Qt.white)
            painter = QPainter(result)
            draw(painter, QPointF(3, 5), bitmap)
            painter.end()
            self.assertEqual(result.pixelColor(81, 63), QColor(Qt.blue))
            self.assertEqual(result.pixelColor(84, 66), QColor(Qt.white))

    def test_explicit_render_target_preserves_half_dpr_geometry(self):
        for dpr in (0.5, 0.67, 1.0, 1.25):
            with self.subTest(dpr=dpr):
                image = QImage(round(80 * dpr), round(60 * dpr), QImage.Format_ARGB32_Premultiplied)
                image.fill(Qt.white)
                set_render_target_device_pixel_ratio(image, dpr)
                painter = QPainter(image)
                apply_render_target_scale(painter, dpr)
                painter.fillRect(QRect(20, 10, 20, 20), Qt.red)
                painter.end()
                self.assertEqual(image.pixelColor(round(30 * dpr), round(20 * dpr)), QColor(Qt.red))
                self.assertEqual(image.pixelColor(round(50 * dpr), round(20 * dpr)), QColor(Qt.white))

    def test_dpr_rounding_matches_qt_including_half_values(self):
        scale = QPaintDevice.devicePixelRatioFScale()
        image = QImage(2, 2, QImage.Format_ARGB32_Premultiplied)
        for dpr in (1, 1.25, 1.5, 2.5, 3.5, 2.625, 1.25 + 0.5 / scale, 1.25 + 2.5 / scale):
            with self.subTest(dpr=dpr):
                image.setDevicePixelRatio(dpr)
                self.assertEqual(image_device_pixel_ratio(dpr), image.devicePixelRatioF())
                self.assertEqual(device_pixel_ratio_metric(QPaintDevice.PdmDevicePixelRatio, dpr),
                                 QPointF(dpr, 0).toPoint().x())
                self.assertEqual(device_pixel_ratio_metric(QPaintDevice.PdmDevicePixelRatioScaled, dpr),
                                 QPointF(dpr * scale, 0).toPoint().x())
                for name in ("PdmDevicePixelRatioF_EncodedA", "PdmDevicePixelRatioF_EncodedB"):
                    metric = getattr(QPaintDevice, name, None)
                    if metric is not None:
                        self.assertEqual(device_pixel_ratio_metric(metric, dpr),
                                         QPaintDevice.encodeMetricF(metric, dpr))
        self.assertEqual(device_pixel_ratio_metric(QPaintDevice.PdmWidth, 2.5), 0)

        class Device(QPaintDevice):
            def metric(self, metric):
                return device_pixel_ratio_metric(metric, self.dpr)

        device = Device()
        for base in (1, 2):
            for offset in (-1.5, -0.5, 0.5, 1.5):
                dpr = base + offset / scale
                with self.subTest(dpr=dpr, boundary=True):
                    image.setDevicePixelRatio(dpr)
                    device.dpr = image.devicePixelRatioF()
                    self.assertEqual(device.devicePixelRatioF(), image.devicePixelRatioF())
                    self.assertEqual(image_device_pixel_ratio(dpr), image.devicePixelRatioF())


if __name__ == "__main__":
    unittest.main()
