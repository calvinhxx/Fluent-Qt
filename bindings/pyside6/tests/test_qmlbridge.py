"""Optional QML scenes share native plugin lifecycle and borrow Python models."""

from __future__ import annotations

import gc
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
import weakref

import fluentqt
import shiboken6
from fluentqt.foundation import (
    accent_color, current_motion_mode, current_theme,
    set_accent_color, set_motion_mode, set_theme,
)
from fluentqt.plugins import Plugin, PluginManager, PluginMetadata
from fluentqt.qmlbridge import QmlPlugin, QmlSession, QmlTheme, QmlView
from PySide6.QtCore import QCoreApplication, QEvent, QMetaObject, QObject, Property, QUrl, Signal, Slot, Qt
from PySide6.QtGui import QColor
from PySide6.QtQml import QQmlEngine
from PySide6.QtQuick import QQuickItem, QSGRendererInterface
from PySide6.QtQuickWidgets import QQuickWidget
from PySide6.QtTest import QSignalSpy, QTest
from PySide6.QtWidgets import QApplication, QWidget


def _delete(obj):
    if shiboken6.isValid(obj):
        obj.deleteLater()
    _flush_deletions()


def _flush_deletions():
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    QApplication.processEvents()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)


class PreviewModel(QObject):
    titleChanged = Signal()
    progressChanged = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._title = "A shared canvas"
        self._progress = 40

    @Property(str, notify=titleChanged)
    def title(self):
        return self._title

    @title.setter
    def title(self, value):
        if self._title != value:
            self._title = value
            self.titleChanged.emit()

    @Property(int, notify=progressChanged)
    def progress(self):
        return self._progress

    @progress.setter
    def progress(self, value):
        value = max(0, min(100, value))
        if self._progress != value:
            self._progress = value
            self.progressChanged.emit()


class QmlGraphicsPolicyTest(unittest.TestCase):
    def test_backend_selection_and_unloaded_texture_checks_use_fresh_processes(self):
        native = "Metal" if sys.platform == "darwin" else (
            "Direct3D11" if sys.platform == "win32" else "OpenGL")
        probe = r'''
import os
import sys
from PySide6.QtWidgets import QApplication, QWidget
from PySide6.QtQuickWidgets import QQuickWidget
from fluentqt.qmlbridge import QmlSession
app = QApplication([])
api_names = ["Automatic", "OpenGL", "Direct3D11", "Metal"]
meta = QmlSession.staticMetaObject
enum = meta.enumerator(meta.indexOfEnumerator("GraphicsApi"))
assert [enum.key(index) for index in range(enum.keyCount())] == api_names
for index, name in enumerate(api_names):
    member = getattr(QmlSession.GraphicsApi, name)
    assert int(getattr(member, "value", member)) == enum.value(index)
scenario, expected = sys.argv[1:]
environment = {name: os.environ.get(name) for name in (
    "FLUENT_QT_SPATIAL_BACKEND", "QSG_RHI_BACKEND", "QT_QUICK_BACKEND", "QMLSCENE_DEVICE")}
api = getattr(QmlSession.GraphicsApi, expected)
if scenario == "legacy":
    QmlSession.prepareApplication()
elif scenario in ("opengl", "native"):
    QmlSession.prepareApplication(api)
elif scenario == "environment":
    QmlSession.prepareApplication(QmlSession.GraphicsApi.Automatic)
software = scenario == "software"
assert QmlSession.isGraphicsApiCompatible(api)
assert QmlSession.isGraphicsApiCompatible(QmlSession.GraphicsApi.Automatic)
assert QmlSession.isGraphicsApiCompatible(QmlSession.GraphicsApi.OpenGL) == (
    software or expected == "OpenGL")
if software:
    assert QmlSession.isGraphicsApiCompatible(QmlSession.GraphicsApi.Metal)
    assert QmlSession.isGraphicsApiCompatible(QmlSession.GraphicsApi.Direct3D11)
owner = QWidget()
session = QmlSession(owner, owner)
assert session.activate()
view = session.createView(owner)
supported = not software and expected == "OpenGL"
assert view.setTextureRenderingEnabled(True) == supported
assert view.isTextureRenderingEnabled() == supported
assert view.findChild(QQuickWidget) is None
assert view.rootObject() is None
assert environment == {name: os.environ.get(name) for name in environment}
'''
        for scenario in ("legacy", "opengl", "native", "environment", "software"):
            with self.subTest(scenario=scenario):
                environment = dict(os.environ)
                for name in ("QSG_RHI_BACKEND", "QT_QUICK_BACKEND", "QMLSCENE_DEVICE"):
                    environment.pop(name, None)
                if scenario == "environment":
                    environment["QSG_RHI_BACKEND"] = {
                        "Metal": "metal", "Direct3D11": "d3d11", "OpenGL": "opengl",
                    }[native]
                elif scenario == "software":
                    environment["QT_QUICK_BACKEND"] = "software"
                expected = native if scenario in ("native", "environment") else "OpenGL"
                process = subprocess.run(
                    [sys.executable, "-c", probe, scenario, expected],
                    env=environment, capture_output=True, text=True, timeout=30,
                )
                self.assertEqual(process.returncode, 0, process.stdout + process.stderr)


class QmlBindingTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        QmlSession.prepareApplication()
        cls.app = QApplication.instance() or QApplication([])
        if not fluentqt.initialize_resources():
            raise RuntimeError("FluentQt resources could not be initialized")
        cls.directory = tempfile.TemporaryDirectory(prefix="fluentqt-qml-binding-")
        source = Path(cls.directory.name) / "Preview.qml"
        source.write_text("""import QtQuick 2.15
Rectangle {
    objectName: "bindingPreviewRoot"
    width: 240; height: 120
    color: fluentTheme.surface
    property string currentTitle: previewModel.title
    property int currentProgress: previewModel.progress
    property color currentAccent: fluentTheme.accent
    property color currentSurfaceAlt: fluentTheme.surfaceAlt
    property bool currentDark: fluentTheme.dark
    property bool currentAnimations: fluentTheme.animationsEnabled
    function advance() {
        previewModel.title = "Updated from QML"
        previewModel.progress = Math.min(100, previewModel.progress + 10)
    }
    function stopAndContinue() {
        previewModel.requestStop()
        previewModel.reportContinuation()
    }
}
""", encoding="utf-8")
        cls.source = QUrl.fromLocalFile(str(source))
        broken = Path(cls.directory.name) / "Broken.qml"
        broken.write_text("import QtQuick 2.15\nRectangle { this is invalid QML }\n", encoding="utf-8")
        cls.broken_source = QUrl.fromLocalFile(str(broken))

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def setUp(self):
        self.objects = []
        self.old_theme = current_theme()
        self.old_motion = current_motion_mode()
        self.old_accent = accent_color()
        set_theme(fluentqt.Theme.Light)
        set_motion_mode(fluentqt.MotionMode.Full)

    def own(self, obj):
        self.objects.append(obj)
        return obj

    def session_view(self, parent=None):
        session = self.own(QmlSession())
        self.assertTrue(session.activate())
        view = self.own(session.createView(parent))
        self.assertIsInstance(view, QmlView)
        return session, view

    def ready(self, view, model=None):
        if model is not None:
            self.assertTrue(view.setContextObject("previewModel", model))
        self.assertTrue(view.load(self.source), view.lastError())
        deadline = time.monotonic() + 5
        while view.status() == QmlView.Status.Loading and time.monotonic() < deadline:
            QTest.qWait(10)
        self.assertEqual(view.status(), QmlView.Status.Ready, view.errors())
        self.assertIsInstance(view.rootObject(), QQuickItem)
        return view.rootObject()

    def tearDown(self):
        for obj in reversed(self.objects):
            if obj is not None and shiboken6.isValid(obj):
                _delete(obj)
        self.objects.clear()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        gc.collect()
        set_accent_color(self.old_accent)
        set_theme(self.old_theme)
        set_motion_mode(self.old_motion)

    def test_base_import_keeps_quick_python_modules_optional(self):
        result = subprocess.run([sys.executable, "-c", """
import sys
import fluentqt
from fluentqt.plugins import PluginManager
assert not any(name in sys.modules for name in (
    'PySide6.QtQml', 'PySide6.QtQuick', 'PySide6.QtQuickWidgets'))
from fluentqt.qmlbridge import QmlView
assert 'PySide6.QtQuick' in sys.modules
assert 'PySide6.QtQml' in sys.modules
"""], env=os.environ.copy(), capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_inactive_session_and_unattached_view_reject_loading(self):
        session = self.own(QmlSession())
        view = self.own(QmlView())
        self.assertIsNone(session.createView())
        self.assertFalse(session.attachView(view))
        self.assertFalse(view.load(self.source))
        self.assertTrue(view.lastError())
        self.assertIsNone(view.rootObject())
        self.assertTrue(session.activate())
        self.assertTrue(session.attachView(view))
        self.assertTrue(view.isActive())

    def test_qml_and_python_share_the_same_model_and_notify_signals(self):
        _, view = self.session_view()
        model = self.own(PreviewModel())
        root = self.ready(view, model)
        self.assertEqual(root.property("currentTitle"), "A shared canvas")
        self.assertEqual(root.property("currentProgress"), 40)
        model.progress = 65
        self.assertEqual(root.property("currentProgress"), 65)
        changes = QSignalSpy(model.progressChanged)
        self.assertTrue(QMetaObject.invokeMethod(root, "advance", Qt.DirectConnection))
        self.assertEqual(model.title, "Updated from QML")
        self.assertEqual(model.progress, 75)
        self.assertEqual(changes.count(), 1)
        self.assertEqual(root.property("currentProgress"), 75)
        self.assertIsNone(model.parent())
        self.assertFalse(view.setContextObject("replacement", model))

    def test_texture_request_is_lazy_and_software_rejection_retains_the_live_scene(self):
        session, view = self.session_view()
        model = self.own(PreviewModel())
        root = self.ready(view, model)
        quick = view.findChild(QQuickWidget, "qmlQuickWidget")
        self.assertIsNotNone(quick)
        engine = quick.engine()
        source = view.source()
        changed = QSignalSpy(view.textureRenderingEnabledChanged)
        software = (quick.quickWindow().rendererInterface().graphicsApi()
                    == QSGRendererInterface.GraphicsApi.Software)
        accepted = view.setTextureRenderingEnabled(True)
        self.assertEqual(accepted, not software)
        self.assertEqual(view.isTextureRenderingEnabled(), accepted)
        self.assertEqual(changed.count(), int(accepted))
        self.assertEqual(view.textureId(), 0, "Requesting texture mode must not allocate a GPU target")
        self.assertEqual(view.textureStatistics()["allocatedBytes"], 0)
        self.assertIs(view.rootObject(), root)
        self.assertIs(quick.engine(), engine)
        model.progress = 65
        self.assertEqual(root.property("currentProgress"), 65)
        self.assertFalse(view.renderTexture(None, 2., 67108864))
        self.assertIs(view.rootObject(), root)
        self.assertEqual(view.source(), source)
        self.assertEqual(view.status(), QmlView.Status.Ready)
        self.assertTrue(view.setTextureRenderingEnabled(False))
        self.assertIs(view.rootObject(), root)
        self.assertIs(quick.engine(), engine)
        self.assertTrue(session.isActive())
        self.assertTrue(shiboken6.isValid(model))

    def test_unload_destroys_the_engine_but_retains_context_bindings_for_reload(self):
        _, view = self.session_view()
        model = self.own(PreviewModel())
        root = self.ready(view, model)
        engine = QQmlEngine.contextForObject(root).engine()
        destroyed = QSignalSpy(engine.destroyed)
        theme = view.themeObject()
        self.assertIsInstance(theme, QmlTheme)
        self.assertFalse(shiboken6.ownedByPython(root))
        self.assertFalse(shiboken6.ownedByPython(theme))
        view.unload()
        self.assertIsNone(view.rootObject())
        _flush_deletions()
        self.assertFalse(shiboken6.isValid(root))
        self.assertFalse(shiboken6.isValid(engine))
        self.assertEqual(destroyed.count(), 1)
        self.assertEqual(view.status(), QmlView.Status.Unloaded)
        self.assertTrue(view.source().isEmpty())
        self.assertTrue(shiboken6.isValid(model))
        self.assertTrue(shiboken6.isValid(theme))
        model.title = "After unloading"
        root = self.ready(view)
        self.assertEqual(root.property("currentTitle"), "After unloading")

    def test_deactivation_preserves_view_and_model_and_never_implicitly_reloads(self):
        parent = self.own(QWidget())
        session, view = self.session_view(parent)
        model = self.own(PreviewModel(parent))
        root = self.ready(view, model)
        states = QSignalSpy(session.activeChanged)
        self.assertTrue(session.deactivate())
        self.assertTrue(session.deactivate())
        self.assertEqual(states.count(), 1)
        self.assertIsNone(view.rootObject())
        _flush_deletions()
        self.assertFalse(shiboken6.isValid(root))
        self.assertTrue(shiboken6.isValid(view))
        self.assertTrue(shiboken6.isValid(model))
        self.assertIs(view.parentWidget(), parent)
        self.assertIs(model.parent(), parent)
        self.assertTrue(view.source().isEmpty())
        self.assertFalse(view.load(self.source))
        self.assertTrue(session.activate())
        self.assertTrue(view.isActive())
        self.assertIsNone(view.rootObject())
        self.assertEqual(view.status(), QmlView.Status.Unloaded)
        model.progress = 90
        self.assertEqual(self.ready(view).property("currentProgress"), 90)

    def test_session_destruction_only_unloads_caller_owned_outputs(self):
        session, view = self.session_view()
        model = self.own(PreviewModel())
        root = self.ready(view, model)
        _delete(session)
        self.assertFalse(shiboken6.isValid(root))
        self.assertTrue(shiboken6.isValid(view))
        self.assertTrue(shiboken6.isValid(model))
        self.assertFalse(view.isActive())
        self.assertIsNone(view.parent())

    def test_unparented_factory_view_can_be_collected_without_stopping_the_session(self):
        session = self.own(QmlSession())
        self.assertTrue(session.activate())
        view = session.createView()
        reference = weakref.ref(view)
        # Merely resolving a signal on an old PySide native factory wrapper
        # retains it. Observe native destruction through an owned QObject child.
        witness = QObject(view)
        destroyed = QSignalSpy(witness.destroyed)
        del view
        gc.collect()
        self.assertIsNone(reference())
        self.assertFalse(shiboken6.isValid(witness))
        self.assertEqual(destroyed.count(), 1)
        self.assertTrue(session.isActive())
        self.assertTrue(session.deactivate())

    def test_factory_view_delete_later_releases_scene_without_deleting_the_model(self):
        session, view = self.session_view()
        model = self.own(PreviewModel())
        root = self.ready(view, model)
        destroyed = QSignalSpy(view.destroyed)
        view.deleteLater()
        # A native factory result may invalidate its wrapper immediately.
        # Flush the event type globally, without passing that wrapper again.
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertEqual(destroyed.count(), 1)
        self.assertFalse(shiboken6.isValid(view))
        self.assertFalse(shiboken6.isValid(root))
        self.assertTrue(shiboken6.isValid(model))
        self.assertTrue(session.deactivate())

    def test_borrowed_context_does_not_keep_an_unparented_python_model_alive(self):
        for loaded in (False, True):
            with self.subTest(loaded=loaded):
                _, view = self.session_view()
                model = PreviewModel()
                self.assertTrue(view.setContextObject("previewModel", model))
                if loaded:
                    self.ready(view)
                reference = weakref.ref(model)
                del model
                gc.collect()
                self.app.processEvents()
                self.assertIsNone(reference())
                self.assertIsNone(view.rootObject())
                self.assertEqual(view.status(), QmlView.Status.Unloaded)
                self.assertTrue(view.isActive())

    def test_session_does_not_keep_its_borrowed_host_alive(self):
        host = QObject()
        reference = weakref.ref(host)
        session = self.own(QmlSession(host))
        self.assertIsNone(session.parent())
        del host
        gc.collect()
        self.assertIsNone(reference())
        self.assertTrue(shiboken6.isValid(session))

    def test_destroying_one_model_unloads_the_scene_and_preserves_other_models(self):
        _, view = self.session_view()
        model = self.own(PreviewModel())
        other = self.own(QObject())
        self.assertTrue(view.setContextObject("otherModel", other))
        root = self.ready(view, model)
        _delete(model)
        self.app.processEvents()
        self.assertFalse(shiboken6.isValid(root))
        self.assertIsNone(view.rootObject())
        self.assertTrue(shiboken6.isValid(other))
        replacement = self.own(PreviewModel())
        replacement.title = "Replacement model"
        self.assertEqual(self.ready(view, replacement).property("currentTitle"), "Replacement model")

    def test_context_names_and_null_removal_preserve_the_view_owned_theme(self):
        _, view = self.session_view()
        model = PreviewModel()
        reference = weakref.ref(model)
        self.assertFalse(view.setContextObject("fluentTheme", model))
        self.assertFalse(view.setContextObject("invalid.name", model))
        self.assertTrue(view.setContextObject("previewModel", model))
        self.assertTrue(view.setContextObject("previewModel", None))
        del model
        gc.collect()
        self.assertIsNone(reference())
        self.assertIsInstance(view.themeObject(), QmlTheme)

    def test_error_is_reported_and_a_valid_page_can_load_afterward(self):
        _, view = self.session_view()
        model = self.own(PreviewModel())
        self.assertTrue(view.setContextObject("previewModel", model))
        errors = QSignalSpy(view.errorOccurred)
        self.assertFalse(view.load(self.broken_source))
        self.assertEqual(view.status(), QmlView.Status.Error)
        self.assertTrue(view.errors())
        self.assertTrue(view.lastError())
        self.assertGreater(errors.count(), 0)
        self.assertIsNone(view.rootObject())
        self.ready(view)
        self.assertEqual(view.errors(), [])
        self.assertEqual(view.lastError(), "")

    def test_theme_and_motion_tokens_update_the_loaded_scene(self):
        _, view = self.session_view()
        model = self.own(PreviewModel())
        root = self.ready(view, model)
        theme = view.themeObject()
        initial_accent = theme.accent()
        initial_surface_alt = root.property("currentSurfaceAlt")
        self.assertEqual(initial_surface_alt, theme.surfaceAlt())
        changes = QSignalSpy(theme.changed)
        set_accent_color(QColor("#765432"))
        set_theme(fluentqt.Theme.Dark)
        set_motion_mode(fluentqt.MotionMode.Disabled)
        self.app.processEvents()
        self.assertEqual(root.property("currentAccent"), theme.accent())
        self.assertNotEqual(theme.accent(), initial_accent)
        self.assertEqual(root.property("currentSurfaceAlt"), theme.surfaceAlt())
        self.assertNotEqual(root.property("currentSurfaceAlt"), initial_surface_alt)
        self.assertTrue(root.property("currentDark"))
        self.assertFalse(root.property("currentAnimations"))
        self.assertGreater(changes.count(), 0)
        for name in ("accent", "surfaceAlt", "dark", "fontFamily", "fontPixelSize",
                     "cornerRadius", "transitionDuration"):
            index = theme.metaObject().indexOfProperty(name)
            self.assertGreaterEqual(index, 0)
            self.assertFalse(theme.metaObject().property(index).isWritable())

    def test_native_qml_plugin_participates_in_python_dependencies_and_widget_factory(self):
        events = []

        class Dependent(Plugin):
            def metadata(self):
                return PluginMetadata("example.qml-user", dependencies=["fluentqt.qmlbridge"])

            def onActivate(self, context):
                events.append(context.activePlugin("fluentqt.qmlbridge"))
                return True

            def onDeactivate(self):
                events.append("stopped")

        parent = self.own(QWidget())
        plugin = self.own(QmlPlugin())
        dependent = self.own(Dependent())
        manager = self.own(PluginManager())
        self.assertTrue(manager.registerPlugin(dependent))
        self.assertTrue(manager.registerPlugin(plugin))
        self.assertEqual(QmlPlugin.pluginId(), "fluentqt.qmlbridge")
        self.assertIsNone(manager.createWidget("fluentqt.qmlbridge", "qml-view", parent))
        self.assertTrue(manager.activate("example.qml-user"), manager.lastError())
        self.assertEqual(events, [plugin])
        view = self.own(manager.createWidget("fluentqt.qmlbridge", "qml-view", parent))
        self.assertIsInstance(view, QmlView)
        self.assertIs(view.parentWidget(), parent)
        model = self.own(PreviewModel())
        root = self.ready(view, model)
        self.assertTrue(manager.deactivate("fluentqt.qmlbridge"))
        self.assertEqual(events[-1], "stopped")
        self.assertIsNone(view.rootObject())
        _flush_deletions()
        self.assertFalse(shiboken6.isValid(root))
        self.assertTrue(shiboken6.isValid(view))
        self.assertTrue(shiboken6.isValid(model))
        self.assertFalse(view.isActive())

    def test_qml_model_signal_can_stop_the_session_while_the_script_finishes(self):
        class StopModel(PreviewModel):
            stopRequested = Signal()

            def __init__(self):
                super().__init__()
                self.continued = False

            @Slot()
            def requestStop(self):
                self.stopRequested.emit()

            @Slot()
            def reportContinuation(self):
                self.continued = True

        session, view = self.session_view()
        model = self.own(StopModel())
        model.stopRequested.connect(session.deactivate)
        root = self.ready(view, model)
        self.assertTrue(QMetaObject.invokeMethod(root, "stopAndContinue", Qt.DirectConnection))
        self.assertTrue(model.continued)
        self.assertFalse(session.isActive())
        self.assertIsNone(view.rootObject())
        self.assertTrue(view.source().isEmpty())
        self.assertEqual(view.status(), QmlView.Status.Unloaded)
        _flush_deletions()
        self.assertFalse(shiboken6.isValid(root))
        self.assertTrue(shiboken6.isValid(model))
        self.assertTrue(session.activate())
        self.assertEqual(self.ready(view).property("currentProgress"), 40)


if __name__ == "__main__":
    unittest.main()
