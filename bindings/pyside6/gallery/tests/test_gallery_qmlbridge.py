"""Atomic QmlView Gallery cards keep their source, contexts and sessions independent."""

import ast
from contextlib import ExitStack, contextmanager
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
import traceback
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

import fluentqt
import shiboken6
from fluentqt.qmlbridge import QmlSession, QmlView
from fluentqt_gallery.app import _prepare_qml_bridge_application
from fluentqt_gallery.catalog import CATEGORY_BY_ID, ENTRY_BY_ROUTE_ID, ROUTE_BY_ID
from fluentqt_gallery.samples import build_sample
from fluentqt_gallery.settings import gallery_settings
from fluentqt_gallery.spatial_support import SPATIAL_AVAILABLE
from fluentqt_gallery.window import GalleryWindow
from PySide6.QtCore import QCoreApplication, QEvent, QObject, QPoint, QSize
from PySide6.QtQuickWidgets import QQuickWidget
from PySide6.QtTest import QSignalSpy, QTest
from PySide6.QtWidgets import QApplication, QWidget


SAMPLES = (
    ("qml-view-load", "qmlBridgeLoadView", "Basic"),
    ("qml-view-theme", "qmlBridgeThemeView", "Theme"),
    ("qml-view-context", "qmlBridgeContextView", "Context"),
    ("qml-view-action", "qmlBridgeActionView", "Action"),
    ("qml-view-lifecycle", "qmlBridgeLifecycleView", "Basic"),
    ("qml-view-error", "qmlBridgeErrorView", "Basic"),
)


def _view(panel, name):
    return panel if isinstance(panel, QmlView) and panel.objectName() == name else panel.findChild(QmlView, name)


def _delete(obj):
    if shiboken6.isValid(obj):
        obj.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    QApplication.processEvents()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)


@contextmanager
def _copied_example(sample_id):
    from fluentqt_gallery.native_samples_qmlbridge import example_source_files

    with TemporaryDirectory(prefix="fluentqt-gallery-copied-qml-") as temporary:
        directory = Path(temporary)
        for source in example_source_files(sample_id):
            filename = source["name"]
            if Path(filename).name != filename:
                raise AssertionError("Copied example filenames must be local: " + filename)
            (directory / filename).write_bytes(source["code"].encode("utf-8"))
        main = directory / "main.py"
        namespace = {"__name__": "copied_atomic_example", "__file__": str(main)}
        try:
            exec(compile(main.read_bytes(), str(main), "exec"), namespace)
            yield namespace
        finally:
            namespace.clear()


class GalleryGraphicsPolicyTest(unittest.TestCase):
    def test_startup_preserves_backend_choices_in_fresh_processes(self):
        native = "Metal" if sys.platform == "darwin" else (
            "Direct3D11" if sys.platform == "win32" else "OpenGL")
        probe = r'''
import os
import sys
from unittest.mock import patch
from PySide6.QtQuick import QQuickWindow, QSGRendererInterface
from PySide6.QtWidgets import QApplication
from fluentqt._fluentqt import fluent as native
from fluentqt.qmlbridge import QmlSession
from fluentqt_gallery import app as entry
from fluentqt_gallery import spatial_support
application = QApplication([])
scenario, native_api = sys.argv[1:]
environment = {name: os.environ.get(name) for name in (
    "FLUENT_QT_SPATIAL_BACKEND", "QSG_RHI_BACKEND", "QT_QUICK_BACKEND", "QMLSCENE_DEVICE")}
original = QQuickWindow.graphicsApi()
prefer_rhi = scenario in ("rhi", "explicit-match", "software")
with patch.object(native._SpatialTextureHost, "isPreferred", return_value=prefer_rhi), \
        patch.object(spatial_support, "SPATIAL_AVAILABLE", scenario != "no-spatial"), \
        patch.object(QmlSession, "prepareApplication", wraps=QmlSession.prepareApplication) as prepare:
    entry._prepare_qml_bridge_application()
    first_count = prepare.call_count
    entry._prepare_qml_bridge_application()
    assert prepare.call_count == first_count
    assert first_count == (1 if scenario in ("default", "rhi") else 0)
    assert bool(application.property("fluentqtGalleryGraphicsBackendFailure")) == (
        scenario == "explicit-conflict")
    if scenario == "default":
        assert QQuickWindow.graphicsApi() == QSGRendererInterface.GraphicsApi.OpenGL
    elif scenario == "rhi":
        assert QQuickWindow.graphicsApi() == getattr(QSGRendererInterface.GraphicsApi, native_api)
    else:
        assert QQuickWindow.graphicsApi() == original
assert environment == {name: os.environ.get(name) for name in environment}
'''
        scenarios = ["default", "rhi", "explicit-match", "software", "no-spatial"]
        if native != "OpenGL":
            scenarios.append("explicit-conflict")
        for scenario in scenarios:
            with self.subTest(scenario=scenario):
                environment = dict(os.environ)
                for name in ("FLUENT_QT_SPATIAL_BACKEND", "QSG_RHI_BACKEND",
                             "QT_QUICK_BACKEND", "QMLSCENE_DEVICE"):
                    environment.pop(name, None)
                if scenario in ("rhi", "explicit-match", "software"):
                    environment["FLUENT_QT_SPATIAL_BACKEND"] = "rhi"
                if scenario in ("explicit-match", "explicit-conflict", "no-spatial"):
                    environment["QSG_RHI_BACKEND"] = {
                        "Metal": "metal", "Direct3D11": "d3d11", "OpenGL": "opengl",
                    }[native]
                if scenario == "software":
                    environment["QT_QUICK_BACKEND"] = "software"
                result = subprocess.run(
                    [sys.executable, "-c", probe, scenario, native],
                    env=environment, capture_output=True, text=True, timeout=30,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


class GalleryQmlBridgeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        cls.app.setProperty("fluentqtGalleryAutomated", True)
        _prepare_qml_bridge_application()
        fluentqt.initialize_resources()
        cls.app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
        gallery_settings().set_spatial_mode_enabled(False)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "Optional Spatial binding is unavailable")
    def test_failed_rhi_preflight_only_uses_a_compatible_opengl_fallback(self):
        from fluentqt_gallery import spatial_controller as spatial

        previous = self.app.property("fluentqtGalleryGraphicsBackendFailure")
        self.app.setProperty("fluentqtGalleryGraphicsBackendFailure", None)
        try:
            for available, compatible in ((True, False), (True, True), (False, False)):
                with self.subTest(qml_bridge=available, quick_matches_opengl=compatible):
                    controller = SimpleNamespace(
                        _session_active=Mock(return_value=True), renderer_failed=False,
                        canvas=None, disable=Mock(), window=Mock(), navigation=Mock(),
                        set_filtering=Mock(), sync=Mock(), start_presentation=Mock(),
                    )
                    controller.navigation.mapTo.return_value = QPoint()
                    controller.navigation.size.return_value = QSize(100, 100)
                    surface = Mock()
                    with patch.object(spatial._RhiSurface, "isPreferred", return_value=True), \
                            patch.object(spatial._RhiSurface, "preflightFailure",
                                         return_value="A hardware graphics device is unavailable."), \
                            patch.object(spatial, "QML_BRIDGE_AVAILABLE", available), \
                            patch.object(QmlSession, "isGraphicsApiCompatible",
                                         return_value=compatible) as api_check, \
                            patch.object(spatial, "_unavailable_reason", return_value="") as gl_probe, \
                            patch.object(spatial, "_Surface", return_value=surface) as gl_surface:
                        spatial.GallerySpatialController.ensure_renderer(controller)
                        if available:
                            api_check.assert_called_once_with(QmlSession.GraphicsApi.OpenGL)
                        else:
                            api_check.assert_not_called()
                        if compatible or not available:
                            controller.disable.assert_not_called()
                            gl_probe.assert_called_once_with()
                            gl_surface.assert_called_once_with(controller, controller.window)
                            self.assertIs(controller.canvas, surface)
                            surface.show.assert_called_once_with()
                        else:
                            controller.disable.assert_called_once_with(
                                "A hardware graphics device is unavailable.")
                            gl_probe.assert_not_called()
                            gl_surface.assert_not_called()
                            self.assertIsNone(controller.canvas)
        finally:
            self.app.setProperty("fluentqtGalleryGraphicsBackendFailure", previous)

    def test_python_source_files_remeasure_without_a_host_language_selector(self):
        from fluentqt_gallery.native_samples_qmlbridge import (
            example_source_files, teaching_source,
        )
        from fluentqt_gallery.visual import GalleryCodeBlock

        for sample_id, _name, _qml_name in SAMPLES:
            with self.subTest(sample=sample_id):
                python = example_source_files(sample_id)
                block = GalleryCodeBlock(teaching_source(sample_id),
                                         source_files=python)
                try:
                    block.resize(520, block.height())
                    block.show()
                    block.setExpandedAnimated(True, False)
                    expanded = QSignalSpy(block.expandedChanged)
                    self.assertIsNone(block.findChild(QWidget, "galleryCodeBlockLanguageSelector"))
                    self.assertEqual([block._file_selector.itemText(i)
                                      for i in range(block._file_selector.count())],
                                     ["main.py", python[1]["name"]])
                    for _ in range(3):
                        block._file_selector.setCurrentIndex(0)
                        self.assertEqual(block._code_language, "python")
                        for mode, expected in ((0, python[0]["usage_snippet"]),
                                               (1, python[0]["code"])):
                            block._source_selector.setSelectedIndex(mode)
                            # The new text must fit before the next paint/event pass.
                            self.assertGreaterEqual(block._code_label.height(),
                                                    block._code_label.heightForWidth(
                                                        block._code_label.width()))
                            QApplication.processEvents()
                            self.assertEqual(block._code, expected)
                            self.assertGreaterEqual(block._code_label.height(),
                                                    block._code_label.heightForWidth(
                                                        block._code_label.width()))
                            block.copy()
                            self.assertEqual(QApplication.clipboard().text(), expected)
                        block._file_selector.setCurrentIndex(1)
                        QApplication.processEvents()
                        self.assertEqual(block._code, python[1]["code"])
                        self.assertEqual(block._code_language, "qml")
                        self.assertGreaterEqual(block._code_label.height(),
                                                block._code_label.heightForWidth(
                                                    block._code_label.width()))
                        block.copy()
                        self.assertEqual(QApplication.clipboard().text(), python[1]["code"])
                    self.assertTrue(block.isExpanded())
                    self.assertEqual(expanded.count(), 0)
                finally:
                    _delete(block)

    def _assert_key_usage(self, sample_id, source):
        required = {
            "qml-view-load": ("activate()", "createView(", 'load(qml_source("Basic"))'),
            "qml-view-theme": ('load(qml_source("Theme"))',),
            "qml-view-context": ("setContextObject", "widgetInput", "A shared title"),
            "qml-view-action": ("setContextObject", "widgetProgress", "setRange(0, 100)",
                                "setValue(40)"),
            "qml-view-lifecycle": ("session.deactivate()", "session.activate()",
                                   "view.unload()", "view.load("),
            "qml-view-error": ("QmlView.Status.Error", "lastError()", 'qml_source("Missing")',
                               "restore.clicked.connect", 'qml_source("Basic")'),
        }
        for token in required[sample_id]:
            self.assertIn(token, source)
        self.assertNotIn("def main(", source)
        self.assertNotIn("import QtQuick", source)
        self.assertNotIn("Rectangle {", source)
        self.assertNotIn("function advance()", source)
        if sample_id != "qml-view-load":
            self.assertNotIn("createView(", source,
                             "Key usage must focus on this card rather than repeat setup")
        tree = ast.parse(source)
        if sample_id == "qml-view-error":
            guarded = [node for node in ast.walk(tree)
                       if isinstance(node, ast.If) and isinstance(node.test, ast.Compare)
                       and any(isinstance(value, ast.Name) and value.id == "error"
                               for value in node.test.comparators)]
            self.assertTrue(any(
                isinstance(call, ast.Call) and isinstance(call.func, ast.Attribute)
                and call.func.attr == "lastError"
                for branch in guarded for statement in branch.body
                for call in ast.walk(statement)), "lastError must be read only in the Error branch")

    @contextmanager
    def _ready_presentation(self):
        from fluentqt_gallery.spatial_controller import GallerySpatialController

        class Canvas(QWidget):
            def clear_frame_caches(self):
                pass

        settings = gallery_settings()
        old_mode = settings.spatial_mode_enabled
        old_motion = settings.motion_mode
        old_availability = settings.spatial_available, settings.spatial_unavailable_reason
        settings.set_motion_mode(fluentqt.MotionMode.Full)
        settings.set_spatial_mode_enabled(False)
        with patch.object(GallerySpatialController, "ensure_renderer"):
            window = GalleryWindow(startup_visuals=False)
        controller = window._spatial_controller
        controller.canvas = Canvas(window)
        controller.renderer_ready = True
        controller.renderer_failed = False
        # Exercise real Qt effects, page selection and capability events without
        # requiring a GPU. Native compositor pixels have a separate desktop gate.
        controller.sync = Mock()
        settings.set_spatial_availability(True)
        settings.set_spatial_mode_enabled(True)
        try:
            self.assertTrue(controller.target)
            self.assertTrue(controller.capture.isEnabled())
            yield window, controller
        finally:
            _delete(window)
            settings.set_spatial_mode_enabled(old_mode)
            settings.set_motion_mode(old_motion)
            settings.set_spatial_availability(*old_availability)

    def test_atomic_samples_execute_their_displayed_source(self):
        from fluentqt_gallery.native_samples_qmlbridge import (
            example_source_files, full_example_source,
        )
        entries = ENTRY_BY_ROUTE_ID["qml-view"].samples
        self.assertEqual([sample.id for sample in entries], [item[0] for item in SAMPLES])
        for sample_id, name, qml_name in SAMPLES:
            with self.subTest(sample=sample_id):
                result = build_sample("qml-view", sample_id)
                try:
                    self.assertTrue(result.source_driven)
                    self.assertEqual(result.parity_level, "native-equivalent")
                    self.assertEqual(ast.dump(ast.parse(result.preview_source)),
                                     ast.dump(ast.parse(full_example_source(sample_id, gallery_parent=True))))
                    view = _view(result.widget, name)
                    self.assertIsInstance(view, QmlView)
                    self.assertEqual(view.status(), QmlView.Status.Ready)
                    self.assertTrue(view.source().toString().endswith(qml_name + ".qml"))
                    source = next(sample for sample in entries if sample.id == sample_id).cpp_snippet
                    self.assertNotIn("import QtQuick", source)
                    qml = next(file for file in example_source_files(sample_id)
                               if file["language"] == "qml")
                    self.assertEqual(qml["name"], qml_name + ".qml")
                    self.assertEqual(qml["code"].encode("utf-8"),
                                     Path(view.source().toLocalFile()).read_bytes())
                    self.assertLessEqual(len(result.source.splitlines()), 80)
                    self.assertLessEqual(max(map(len, result.source.splitlines())), 88)
                    self._assert_key_usage(sample_id, result.source)
                    self.assertNotIn("PreviewModel", result.preview_source)
                finally:
                    _delete(result.widget)

    def test_optional_page_keeps_six_plugin_cards_and_copyable_source(self):
        from fluentqt_gallery.native_samples_qmlbridge import (
            example_source_files, full_example_source, teaching_source,
        )

        self.assertTrue(CATEGORY_BY_ID["qml-bridge"].plugin)
        self.assertEqual(ROUTE_BY_ID["qml-bridge"].parent_id, "plugins")
        self.assertEqual(ROUTE_BY_ID["qml-view"].parent_id, "qml-bridge")
        self.assertEqual(ROUTE_BY_ID["qml-view"].depth, 2)
        window = GalleryWindow(startup_visuals=False)
        try:
            page = window._ensure_page("qml-view")[1]
            reference = page.findChild(fluentqt.Label, "galleryComponentReferenceImport")
            self.assertEqual(reference.text(), "from fluentqt.qmlbridge import QmlSession, QmlView")
            self.assertIsNone(page.findChild(fluentqt.Label, "galleryQmlBridgeModeStatus"))
            self.assertEqual(len(page._gallery_sample_cards), 6)
            for card in page._gallery_sample_cards:
                sample_id = card.property("gallerySampleId")
                key = teaching_source(sample_id)
                full = full_example_source(sample_id)
                self.assertNotEqual(key.strip(), full.strip())
                code = card._gallery_source_expander
                self.assertIsNone(code.findChild(QWidget, "galleryCodeBlockLanguageSelector"))
                self.assertIsNotNone(code._source_selector)
                files = example_source_files(sample_id)
                self.assertIsNotNone(code._file_selector)
                self.assertEqual([code._file_selector.itemText(index)
                                  for index in range(code._file_selector.count())],
                                 [file["name"] for file in files])
                code._source_selector.setSelectedIndex(0)
                self.assertEqual(code._code, key)
                code.copy()
                self.assertEqual(QApplication.clipboard().text(), key)
                code._source_selector.setSelectedIndex(1)
                self.assertEqual(code._code, full)
                code.copy()
                copied = QApplication.clipboard().text()
                self.assertEqual(copied, full)
                self.assertIn("QmlSession", copied)
                self.assertNotIn("gallery_parent", copied)
                self.assertNotIn("import QtQuick", copied)
                compile(copied, "<qml-bridge-atomic-example>", "exec")
                code._file_selector.setCurrentIndex(1)
                self.assertTrue(code._source_selector.isHidden())
                self.assertEqual(code._code, files[1]["code"])
                code.copy()
                self.assertEqual(QApplication.clipboard().text(), files[1]["code"])
                code._file_selector.setCurrentIndex(0)
                self.assertFalse(code._source_selector.isHidden())
                self.assertEqual(code._code, full)
                code._source_selector.setSelectedIndex(0)
                self.assertEqual(code._code, key)
                self.assertLessEqual(len(code._code.splitlines()), 80)
                self.assertLessEqual(max(map(len, code._code.splitlines())), 88)
            self.assertIn("qml-bridge", [card._route_id for card in window._ensure_page("plugins")[1]._gallery_buttons])
            self.assertNotIn("qml-view", [card._route_id for card in window._ensure_page("all-controls")[1]._gallery_buttons])
        finally:
            _delete(window)

    def test_full_example_files_load_canonical_assets_and_execute_each_capability(self):
        from fluentqt_gallery import catalog
        from fluentqt_gallery.native_samples_qmlbridge import (
            example_source_files, full_example_source, qml_source, teaching_source,
        )
        from PySide6.QtCore import QMetaObject, Qt

        contract = json.loads(Path(catalog.__file__).with_name("contract.json").read_text("utf-8"))
        cpp_samples = {
            sample["id"]: sample for component in contract["components"]
            if component["id"] == "qml-view" for sample in component["samples"]
        }
        for sample_id, name, qml_name in SAMPLES:
            with self.subTest(sample=sample_id):
                source = full_example_source(sample_id)
                tree = ast.parse(source)
                files = example_source_files(sample_id)
                self.assertEqual([file["name"] for file in files],
                                 ["main.py", qml_name + ".qml"])
                self.assertEqual(files[0]["language"], "python")
                self.assertEqual(files[0]["code"], source)
                self.assertEqual(files[0]["usage_snippet"], teaching_source(sample_id))
                self.assertEqual(files[1]["language"], "qml")
                self.assertEqual(files[1]["usage_snippet"], "")
                expected = Path(qml_source(qml_name).toLocalFile()).read_bytes()
                self.assertEqual(files[1]["code"].encode("utf-8"), expected)
                cpp_files = cpp_samples[sample_id]["source_files"]
                self.assertEqual(cpp_files[0]["name"], "main.cpp")
                self.assertEqual(cpp_files[0]["code"], cpp_samples[sample_id]["cpp_snippet"])
                self.assertNotIn("import QtQuick", cpp_files[0]["code"])
                self.assertEqual(cpp_files[1]["name"], files[1]["name"])
                self.assertEqual(cpp_files[1]["language"], "qml")
                self.assertEqual(cpp_files[1]["code"], files[1]["code"])
                self.assertNotIn("import QtQuick", source)
                self.assertNotIn("_QML", source)
                self.assertNotIn("TemporaryDirectory", source)
                self.assertFalse(any(
                    isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                    and node.func.attr in {"write_text", "write_bytes"}
                    for node in ast.walk(tree)))
                main = next(node for node in tree.body
                            if isinstance(node, ast.FunctionDef) and node.name == "main")
                main_calls = sorted((node.lineno, ast.unparse(node.func))
                                    for node in ast.walk(main) if isinstance(node, ast.Call))
                prepare_line = next(line for line, call in main_calls
                                    if call == "QmlSession.prepareApplication")
                app_line = next(line for line, call in main_calls if call == "QApplication")
                self.assertLess(prepare_line, app_line)
                self.assertIn("app.exec", [call for _, call in main_calls])
                panel = None
                errors = []
                with patch.object(sys, "excepthook", side_effect=lambda *error: errors.append(
                        "".join(traceback.format_exception(*error)))), \
                        _copied_example(sample_id) as namespace:
                    try:
                        panel = namespace["make_panel"]()
                        view = _view(panel, name)
                        self.assertEqual(view.status(), QmlView.Status.Ready)
                        self.assertEqual(Path(view.source().toLocalFile()).read_bytes(), expected)
                        root = view.rootObject()
                        if sample_id == "qml-view-context":
                            title = panel.findChild(fluentqt.LineEdit, "qmlBridgeTitleEdit")
                            title.setText("Copied code updates QML")
                            self.assertEqual(root.property("previewTitle"), title.text())
                        elif sample_id == "qml-view-action":
                            progress = panel.findChild(fluentqt.ProgressBar, "qmlBridgeProgressIndicator")
                            self.assertEqual(progress.value(), 40)
                            self.assertTrue(QMetaObject.invokeMethod(root, "advance", Qt.DirectConnection))
                            self.assertEqual(progress.value(), 50)
                            self.assertEqual(root.property("previewProgress"), 50)
                        elif sample_id == "qml-view-lifecycle":
                            panel.findChild(fluentqt.Button, "qmlBridgeDeactivate").click()
                            self.assertEqual(view.status(), QmlView.Status.Unloaded)
                            self.assertIsNone(view.rootObject())
                            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
                            panel.findChild(fluentqt.Button, "qmlBridgeReload").click()
                            self.assertEqual(view.status(), QmlView.Status.Ready)
                            ready_root = view.rootObject()
                            ready_engine = view.findChild(QQuickWidget).engine()
                            panel.findChild(fluentqt.Button, "qmlBridgeReload").click()
                            self.assertEqual(view.status(), QmlView.Status.Ready)
                            self.assertIsNot(view.rootObject(), ready_root)
                            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
                            self.assertFalse(shiboken6.isValid(ready_root))
                            self.assertFalse(shiboken6.isValid(ready_engine))
                        elif sample_id == "qml-view-error":
                            panel.findChild(fluentqt.Button, "qmlBridgeLoadMissing").click()
                            self.assertEqual(view.status(), QmlView.Status.Error)
                            self.assertTrue(view.lastError())
                            self.assertEqual(panel.findChild(fluentqt.Label, "qmlBridgeErrorStatus").text(),
                                             view.lastError())
                            panel.findChild(fluentqt.Button, "qmlBridgeRestore").click()
                            self.assertEqual(view.status(), QmlView.Status.Ready)
                    finally:
                        if panel is not None:
                            _delete(panel)
                        namespace.clear()
                self.assertEqual(errors, [], "\n".join(errors))

    def test_full_example_main_assembles_a_real_window_for_every_card(self):
        real_window = fluentqt.Window
        for sample_id, name, _ in SAMPLES:
            with self.subTest(sample=sample_id), _copied_example(sample_id) as namespace:
                windows = []

                def create_window(*args, **kwargs):
                    window = real_window(*args, **kwargs)
                    windows.append(window)
                    return window

                application = Mock(return_value=self.app)
                try:
                    with patch.dict(namespace, {"QApplication": application}), \
                            patch.object(fluentqt, "Window", side_effect=create_window) as factory, \
                            patch.object(fluentqt, "prepare_high_dpi_application"), \
                            patch.object(namespace["QmlSession"], "prepareApplication"), \
                            patch.object(self.app, "exec", return_value=0) as event_loop:
                        self.assertEqual(namespace["main"](), 0)
                        factory.assert_called_once_with()
                        application.assert_called_once_with(sys.argv)
                        event_loop.assert_called_once_with()
                    self.assertEqual(len(windows), 1)
                    window = windows[0]
                    self.assertIsInstance(window, real_window)
                    panel = window.contentWidget()
                    self.assertIsInstance(panel, QWidget)
                    self.assertTrue(window.isAncestorOf(panel))
                    self.assertTrue(window.isVisible())
                    view = _view(panel, name)
                    self.assertIsInstance(view, QmlView)
                    self.assertEqual(view.status(), QmlView.Status.Ready, view.errors())
                    self.assertIsNotNone(view.rootObject())
                    self.assertTrue(view.source().isLocalFile())
                finally:
                    for window in windows:
                        if shiboken6.isValid(window):
                            _delete(window)
                    namespace.clear()

    def test_context_and_action_bind_real_independent_widgets(self):
        context = build_sample("qml-view", "qml-view-context").widget
        action = build_sample("qml-view", "qml-view-action").widget
        try:
            context_view = _view(context, "qmlBridgeContextView")
            action_view = _view(action, "qmlBridgeActionView")
            title = context.findChild(fluentqt.LineEdit, "qmlBridgeTitleEdit")
            progress = action.findChild(fluentqt.ProgressBar, "qmlBridgeProgressIndicator")
            self.assertEqual(title.text(), "A shared title")
            self.assertEqual(progress.value(), 40)
            title.setText("Only the context card changes")
            self.assertEqual(context_view.rootObject().property("previewTitle"), title.text())
            self.assertEqual(action_view.rootObject().property("previewProgress"), 40)
            self.assertTrue(QCoreApplication.instance() is not None)
            from PySide6.QtCore import QMetaObject, Qt
            self.assertTrue(QMetaObject.invokeMethod(action_view.rootObject(), "advance", Qt.DirectConnection))
            self.assertEqual(progress.value(), 50)
            self.assertEqual(action_view.rootObject().property("previewProgress"), 50)
            self.assertEqual(title.text(), "Only the context card changes")
            self.assertIsNot(context_view.rootObject(), action_view.rootObject())
        finally:
            _delete(context)
            _delete(action)

    def test_lifecycle_and_error_only_replace_their_own_scene(self):
        window = GalleryWindow(startup_visuals=False)
        try:
            page = window._ensure_page("qml-view")[1]
            views = {name: page.findChild(QmlView, name) for _, name, _ in SAMPLES}
            self.assertTrue(all(view is not None for view in views.values()))
            roots = {name: view.rootObject() for name, view in views.items()}
            engines = {name: view.findChild(QQuickWidget).engine() for name, view in views.items()}
            lifecycle = views["qmlBridgeLifecycleView"]
            page.findChild(fluentqt.Button, "qmlBridgeDeactivate").click()
            self.assertEqual(lifecycle.status(), QmlView.Status.Unloaded)
            self.assertIsNone(lifecycle.rootObject())
            for name, view in views.items():
                if view is not lifecycle:
                    self.assertIs(view.rootObject(), roots[name])
            page.findChild(fluentqt.Button, "qmlBridgeReload").click()
            self.assertEqual(lifecycle.status(), QmlView.Status.Ready)
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
            ready_root = lifecycle.rootObject()
            ready_engine = lifecycle.findChild(QQuickWidget).engine()
            page.findChild(fluentqt.Button, "qmlBridgeReload").click()
            self.assertEqual(lifecycle.status(), QmlView.Status.Ready)
            self.assertIsNot(lifecycle.rootObject(), ready_root)
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
            QApplication.processEvents()
            QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
            self.assertFalse(shiboken6.isValid(ready_root))
            self.assertFalse(shiboken6.isValid(ready_engine))
            for name, view in views.items():
                if view is not lifecycle:
                    self.assertIs(view.rootObject(), roots[name])
                    self.assertIs(view.findChild(QQuickWidget).engine(), engines[name])
            error = views["qmlBridgeErrorView"]
            page.findChild(fluentqt.Button, "qmlBridgeLoadMissing").click()
            self.assertEqual(error.status(), QmlView.Status.Error)
            self.assertTrue(error.lastError())
            self.assertIsNone(error.rootObject())
            self.assertIn(error.lastError(), page.findChild(fluentqt.Label, "qmlBridgeErrorStatus").text())
            self.assertIs(views["qmlBridgeActionView"].rootObject(), roots["qmlBridgeActionView"])
            page.findChild(fluentqt.Button, "qmlBridgeRestore").click()
            self.assertEqual(error.status(), QmlView.Status.Ready)
            self.assertEqual(page.findChild(fluentqt.Label, "qmlBridgeErrorStatus").text(), "QML loaded")
        finally:
            _delete(window)

    def test_status_callbacks_survive_host_and_source_namespace_teardown(self):
        for sample_id, view_name, label_name in (
            ("qml-view-lifecycle", "qmlBridgeLifecycleView", "qmlBridgeLifecycleStatus"),
            ("qml-view-error", "qmlBridgeErrorView", "qmlBridgeErrorStatus"),
        ):
            for copied_source in (False, True):
                for clear_namespace in (False, True):
                    with self.subTest(sample=sample_id, copied=copied_source,
                                      cleared=clear_namespace):
                        errors, events, valid_at_unload = [], [], []
                        host = QWidget()
                        namespace = {}
                        with patch.object(
                            sys, "excepthook",
                            side_effect=lambda *error: errors.append(
                                "".join(traceback.format_exception(*error))),
                        ), ExitStack() as copied_examples:
                            try:
                                if copied_source:
                                    namespace = copied_examples.enter_context(
                                        _copied_example(sample_id))
                                    panel = namespace["make_panel"](host)
                                else:
                                    result = build_sample("qml-view", sample_id, host)
                                    panel = result.widget
                                    namespace = panel._fluentqt_gallery_source_namespace
                                view = _view(panel, view_name)
                                label = panel.findChild(fluentqt.Label, label_name)
                                self.assertEqual(view.status(), QmlView.Status.Ready)
                                self.assertEqual(label.text(), "QML loaded")
                                def status_changed(state):
                                    events.append(("status", state))
                                    if state == QmlView.Status.Unloaded:
                                        valid_at_unload.append(shiboken6.isValid(view))

                                view.statusChanged.connect(status_changed)
                                label.destroyed.connect(
                                    lambda *_: events.append(("label", None)))
                                if clear_namespace:
                                    namespace.clear()
                                # Parent destruction retires the native view before
                                # the status label; callbacks must use signal state
                                # even when its Python wrapper is already invalid.
                                _delete(host)
                                self.assertFalse(shiboken6.isValid(view))
                                self.assertFalse(shiboken6.isValid(label))
                                self.assertEqual(valid_at_unload, [False])
                                self.assertIn(("status", QmlView.Status.Unloaded), events)
                                self.assertLess(
                                    events.index(("status", QmlView.Status.Unloaded)),
                                    events.index(("label", None)))
                            finally:
                                _delete(host)
                                namespace.clear()
                        self.assertEqual(errors, [], "\n".join(errors))

    def test_missing_bridge_hides_its_subtree_without_importing_quick(self):
        code = """
import importlib.util, sys
original = importlib.util.find_spec
importlib.util.find_spec = lambda name, *a, **k: None if name == 'fluentqt.qmlbridge' else original(name, *a, **k)
from fluentqt_gallery.catalog import CATEGORIES, ENTRIES, ROUTES
from fluentqt_gallery.native_samples import ported_sample_keys
assert not any(c.id == 'qml-bridge' for c in CATEGORIES)
assert not any(e.route_id == 'qml-view' for e in ENTRIES)
assert not any(r.id in ('qml-bridge', 'qml-view') for r in ROUTES)
assert not any(route == 'qml-view' for route, _ in ported_sample_keys())
assert 'fluentqt.qmlbridge' not in sys.modules
assert 'PySide6.QtQuick' not in sys.modules
from fluentqt_gallery.native_samples_qmlbridge import full_example_source
assert 'QmlSession' in full_example_source("qml-view-load")
assert 'fluentqt.qmlbridge' not in sys.modules
assert 'PySide6.QtQuick' not in sys.modules
"""
        result = subprocess.run([sys.executable, "-c", code], env=os.environ.copy(),
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_software_scenes_fall_back_independently_without_changing_preferences(self):
        with self._ready_presentation() as (window, controller):
            settings = gallery_settings()
            changes = QSignalSpy(settings.spatialModeEnabledChanged)
            canvas = controller.canvas
            page = window._ensure_page("qml-view")[1]
            views = page.findChildren(QmlView)
            lifecycle = page.findChild(QmlView, "qmlBridgeLifecycleView")
            self.assertFalse(page.property("fluentqtDirectCompositionRequired"))
            self.assertEqual(len(views), 6)
            self.assertTrue(all(view.property("fluentqtDirectCompositionRequired") for view in views))
            self.assertTrue(controller.target, "A hidden cached page must not pause Home")

            window.navigate("qml-view", animated=False)
            self.assertTrue(controller._direct_composition_required)
            self.assertFalse(controller.target)
            self.assertIsNone(window._content_host.graphicsEffect())
            self.assertIsNone(window._navigation_view.graphicsEffect())
            self.assertTrue(canvas.isHidden())
            self.assertTrue(settings.spatial_mode_enabled)
            self.assertTrue(settings.spatial_available)
            self.assertTrue(window._spatial_session.isActive())

            page.findChild(fluentqt.Button, "qmlBridgeDeactivate").click()
            QApplication.processEvents()
            self.assertEqual(lifecycle.status(), QmlView.Status.Unloaded)
            self.assertTrue(controller._direct_composition_required)
            self.assertFalse(controller.target, "The other five software scenes still require 2D")
            self.assertIs(controller.canvas, canvas)
            page.findChild(fluentqt.Button, "qmlBridgeReload").click()
            self.assertEqual(lifecycle.status(), QmlView.Status.Ready)
            page._gallery_theme_button.click()
            self.assertTrue(controller._direct_composition_required)
            self.assertIsNone(window._content_host.graphicsEffect())
            window.navigate("button", animated=False)
            QApplication.processEvents()
            self.assertTrue(controller.target)
            self.assertIs(controller.canvas, canvas)
            self.assertEqual(changes.count(), 0)

            settings.set_spatial_mode_enabled(False)
            window.navigate("qml-view", animated=False)
            page.findChild(fluentqt.Button, "qmlBridgeDeactivate").click()
            QApplication.processEvents()
            self.assertFalse(controller.target)
            self.assertFalse(settings.spatial_mode_enabled)
            self.assertEqual(changes.count(), 1)
            window.navigate("home", animated=False)
            QApplication.processEvents()
            self.assertFalse(controller.target)

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding")
    def test_dynamic_requirement_removes_effects_before_quick_construction(self):
        from fluentqt.qmlbridge import QmlSession

        with self._ready_presentation() as (window, controller):
            page = window._pages["home"][1]
            session = QmlSession(page, page)
            self.assertTrue(session.activate())
            view = session.createView(page)
            observed = []

            class BeforeQuick(QObject):
                def eventFilter(self, watched, event):
                    if event.type() == QEvent.ChildAdded:
                        observed.append((window._content_host.graphicsEffect(),
                                         window._navigation_view.graphicsEffect(),
                                         controller.target))
                    return False

            observer = BeforeQuick(view)
            view.installEventFilter(observer)
            from fluentqt_gallery.native_samples_qmlbridge import qml_source
            self.assertTrue(view.load(qml_source("Basic")))
            self.assertTrue(observed)
            self.assertTrue(all(entry == (None, None, False) for entry in observed))
            self.assertTrue(gallery_settings().spatial_mode_enabled)
            self.assertTrue(session.deactivate())
            self.assertFalse(controller.target, "Resume waits for the next event-loop turn")
            QApplication.processEvents()
            self.assertTrue(controller.target)


if __name__ == "__main__":
    unittest.main()
