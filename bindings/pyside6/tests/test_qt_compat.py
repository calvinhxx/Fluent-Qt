"""Shared lifetime, event, paint and compatibility-boundary contracts."""
import ast
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from PySide6.QtCore import QEvent, QObject, QPointF, QRect, QTimer
from PySide6.QtGui import QFont, QImage, QPaintDevice, QRegion
from PySide6.QtTest import QSignalSpy
from PySide6.QtWidgets import QApplication, QWidget
from shiboken6 import isValid

from fluentqt._qt_compat import delete_qobject
from fluentqt._qt_events import is_display_scale_change_event
from fluentqt._qt_painting import (
    device_pixel_ratio_metric, font_style_strategy, forward_paint_array, region_rects,
    image_device_pixel_ratio,
)
from fluentqt._qt_widgets import application_is_saving_session
from fluentqt import _qt_widgets


def compatibility_violations(source):
    tree = ast.parse(source)
    version_functions = {"qVersion"}
    version_types = {"QLibraryInfo"}
    version_values = set()
    qt_modules = {"PySide6", "QtCore", "QtGui", "QtWidgets"}
    qt_types = {"QEvent", "QPaintDevice", "QAccessible"}
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for name in node.names:
                if name.name.startswith("PySide6"):
                    qt_modules.add(name.asname or name.name.split(".")[0])
        if isinstance(node, ast.ImportFrom) and (node.module or "").startswith("PySide6"):
            for name in node.names:
                if name.name == "qVersion":
                    version_functions.add(name.asname or name.name)
                elif name.name == "QLibraryInfo":
                    version_types.add(name.asname or name.name)
                elif name.name in {"__version__", "__version_info__"}:
                    version_values.add(name.asname or name.name)
                elif name.name.startswith("Qt"):
                    qt_modules.add(name.asname or name.name)
                elif name.name.startswith("Q"):
                    qt_types.add(name.asname or name.name)

    def root_name(node):
        while isinstance(node, (ast.Attribute, ast.Call)):
            node = node.value if isinstance(node, ast.Attribute) else node.func
        return node.id if isinstance(node, ast.Name) else None

    def is_version_call(node):
        if not isinstance(node, ast.Call):
            return False
        return ((isinstance(node.func, ast.Name) and node.func.id in version_functions)
                or (isinstance(node.func, ast.Attribute) and node.func.attr == "qVersion"
                    and root_name(node.func) in qt_modules)
                or (isinstance(node.func, ast.Attribute) and node.func.attr == "version"
                    and (root_name(node.func) in version_types
                         or (isinstance(node.func.value, ast.Attribute)
                             and node.func.value.attr == "QLibraryInfo"
                             and root_name(node.func) in qt_modules))))

    def uses_version(node):
        return any(
            (isinstance(child, ast.Name) and child.id in version_values)
            or (isinstance(child, ast.Attribute) and child.attr in {"__version__", "__version_info__"}
                and root_name(child) in qt_modules)
            or is_version_call(child)
            for child in ast.walk(node)
        )

    # Follow named intermediate values so assigning qVersion() before comparing
    # it cannot bypass the boundary. Diagnostics that merely report it are allowed.
    assignments = [node for node in ast.walk(tree) if isinstance(node, (ast.Assign, ast.AnnAssign))]
    while True:
        previous = set(version_values)
        previous_types = set(qt_types)
        for node in assignments:
            if node.value is None:
                continue
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            for target in targets:
                names = {target.id} if isinstance(target, ast.Name) else set()
                if uses_version(node.value):
                    version_values.update(names)
                type_alias = isinstance(node.value, (ast.Name, ast.Attribute))
                style_hints = (isinstance(node.value, ast.Call)
                               and isinstance(node.value.func, ast.Attribute)
                               and node.value.func.attr == "styleHints")
                if (type_alias or style_hints) and root_name(node.value) in qt_types | qt_modules:
                    qt_types.update(names)
        if previous == version_values and previous_types == qt_types:
            break

    violations = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Compare) and uses_version(node):
            violations.append((node.lineno, "Qt/PySide version comparison"))
        if isinstance(node, (ast.If, ast.IfExp, ast.While)) and uses_version(node.test):
            violations.append((node.lineno, "Qt/PySide version condition"))
        if (isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
                and node.func.id in {"getattr", "hasattr"} and node.args):
            if root_name(node.args[0]) in qt_types | qt_modules:
                violations.append((node.lineno, "Qt API capability lookup"))
        if isinstance(node, ast.Try) and any(
            isinstance(child, ast.Name) and child.id == "AttributeError"
            for handler in node.handlers if handler.type is not None
            for child in ast.walk(handler.type)
        ):
            if any(isinstance(child, ast.Attribute) and root_name(child) in qt_types | qt_modules
                   for statement in node.body for child in ast.walk(statement)):
                violations.append((node.lineno, "Qt API availability fallback"))
    return violations


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


class QtAdapterCompatTest(unittest.TestCase):
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

    def test_boundary_detects_version_aliases_and_ignores_diagnostics(self):
        for source in (
            "from PySide6.QtCore import qVersion as version\nlegacy = version() < '6.8'",
            "v = tuple(map(int, qVersion().split('.')))\nlegacy = v < (6, 8)",
            "from PySide6 import __version_info__ as version\nlegacy = version[:2] == (6, 2)",
            "from PySide6 import QtCore as core\nlegacy = core.qVersion() < '6.8'",
            "import PySide6 as qt\nlegacy = qt.__version_info__[:2] < (6, 8)",
            "from PySide6.QtCore import QEvent as Event\nlegacy = hasattr(Event.Type, 'DevicePixelRatioChange')",
            "event = getattr(QEvent.Type, 'DevicePixelRatioChange', None)",
            "from PySide6.QtCore import QLibraryInfo as Info\nlegacy = Info.version().majorVersion() == 6",
            "from PySide6 import QtCore as core\nlegacy = core.QLibraryInfo.version().minorVersion() < 5",
            "if qVersion().startswith('6.2'):\n    fallback()",
            "from PySide6.QtCore import QEvent\nEvent = QEvent\nlegacy = hasattr(Event, 'DevicePixelRatioChange')",
            "from PySide6.QtGui import QGuiApplication\ntry:\n    QGuiApplication.styleHints().colorScheme()\nexcept AttributeError:\n    pass",
            "from PySide6.QtGui import QGuiApplication\nhints = QGuiApplication.styleHints()\ntry:\n    hints.colorScheme()\nexcept AttributeError:\n    pass",
        ):
            with self.subTest(source=source):
                self.assertTrue(compatibility_violations(source))
        self.assertFalse(compatibility_violations("info = {'qt_version': qVersion()}"))
        self.assertFalse(compatibility_violations("optional = getattr(widget, 'customProperty', None)"))
        self.assertFalse(compatibility_violations("sample = 'qVersion() < version'"))

    def test_version_adaptations_stay_in_private_qt_modules(self):
        root = Path(__file__).resolve().parents[3]
        offenders = []
        for directory in (root / "bindings/pyside6/src/fluentqt",
                          root / "bindings/pyside6/gallery/src/fluentqt_gallery"):
            for path in sorted(directory.rglob("*.py")):
                if path.parent.name == "fluentqt" and path.name.startswith("_qt_"):
                    continue
                for line, reason in compatibility_violations(path.read_text(encoding="utf-8")):
                    offenders.append(f"{path.relative_to(root)}:{line}: {reason}")
        self.assertEqual(offenders, [])


if __name__ == "__main__":
    unittest.main()
