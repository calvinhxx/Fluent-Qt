"""Python extension dispatch, dependency transactions, and borrowed ownership."""

from __future__ import annotations

import gc
import importlib.util
import unittest
import weakref

import fluentqt
import shiboken6
from fluentqt._qt_compat import delete_qobject
from fluentqt.plugins import (
    Plugin,
    PluginContext,
    PluginManager,
    PluginMetadata,
    WidgetPlugin,
)
from PySide6.QtCore import QObject, QVersionNumber
from PySide6.QtTest import QSignalSpy
from PySide6.QtWidgets import QApplication, QWidget


def _metadata(identifier, dependencies=()):
    result = PluginMetadata()
    result.id = identifier
    result.name = identifier.title()
    result.version = QVersionNumber(1, 2, 3)
    result.dependencies = list(dependencies)
    return result


class RecordingPlugin(Plugin):
    def __init__(self, identifier, events, dependencies=(), parent=None, succeeds=True):
        super().__init__(parent)
        self.descriptor = _metadata(identifier, dependencies)
        self.events = events
        self.succeeds = succeeds
        self.context_copy = None
        self.activation_probe = None
        self.deactivation_probe = None

    def metadata(self):
        return self.descriptor

    def onActivate(self, context):
        self.events.append(("activate", self.descriptor.id))
        self.context_copy = PluginContext(context)
        if self.activation_probe is not None:
            self.activation_probe(context)
        return self.succeeds

    def onDeactivate(self):
        self.events.append(("deactivate", self.descriptor.id))
        if self.deactivation_probe is not None:
            self.deactivation_probe()


class ButtonsPlugin(WidgetPlugin):
    def __init__(self, events):
        super().__init__()
        self.events = events
        self.factory_calls = 0
        self.wrong_parent = False
        self.failure_message = None
        self.last_widget = None
        self.destroyed_outputs = []

    def metadata(self):
        return _metadata("example.buttons")

    def onActivate(self, context):
        self.events.append(("activate", "example.buttons"))
        return True

    def onDeactivate(self):
        self.events.append(("deactivate", "example.buttons"))

    def widgetKeys(self):
        return ["button"]

    def createWidget(self, key, parent):
        self.factory_calls += 1
        if self.failure_message is not None:
            raise RuntimeError(self.failure_message)
        widget = fluentqt.Button("Extension command", None if self.wrong_parent else parent)
        destroyed_outputs = self.destroyed_outputs
        widget.destroyed.connect(lambda *_: destroyed_outputs.append(key))
        self.last_widget = weakref.ref(widget)
        return widget


class PluginBindingTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        if not fluentqt.initialize_resources():
            raise RuntimeError("FluentQt resources could not be initialized")

    def setUp(self):
        self.events = []
        self.objects = []
        self.managers = []

    def own(self, obj):
        self.objects.append(obj)
        return obj

    def manager(self, host=None):
        manager = self.own(PluginManager(host))
        self.managers.append(manager)
        return manager

    def extension(self, identifier, dependencies=(), **kwargs):
        return self.own(RecordingPlugin(identifier, self.events, dependencies, **kwargs))

    def tearDown(self):
        for manager in self.managers:
            if shiboken6.isValid(manager):
                manager.deactivateAll()
        for obj in reversed(self.objects):
            if shiboken6.isValid(obj):
                delete_qobject(obj)
        self.objects.clear()
        self.managers.clear()
        self.app.processEvents()
        gc.collect()

    def test_metadata_is_snapshotted_without_reparenting_the_plugin(self):
        owner = self.own(QObject())
        host = self.own(QObject())
        manager = self.manager(host)
        plugin = self.extension("example.data", parent=owner)
        registered = QSignalSpy(manager.pluginRegistered)
        self.assertTrue(manager.registerPlugin(plugin))
        self.assertIs(plugin.parent(), owner)
        self.assertIs(manager.plugin("example.data"), plugin)
        self.assertIs(manager.context().host(), host)
        plugin.descriptor.name = "Changed after registration"
        plugin.descriptor.dependencies = ["unregistered"]
        snapshot = manager.metadata("example.data")
        self.assertEqual(snapshot.id, "example.data")
        self.assertEqual(snapshot.name, "Example.Data")
        self.assertEqual(snapshot.version.toString(), "1.2.3")
        self.assertEqual(snapshot.dependencies, [])
        self.assertTrue(manager.activate("example.data"))
        self.assertEqual(registered.count(), 1)

    def test_dependencies_activate_first_and_deactivation_cascades_in_reverse(self):
        manager = self.manager()
        dependent = self.extension("preview", ["theme", "data"])
        theme = self.extension("theme")
        data = self.extension("data", ["theme"])
        for plugin in (dependent, data, theme):
            self.assertTrue(manager.registerPlugin(plugin))
        self.assertTrue(manager.activate("preview"))
        self.assertEqual(manager.activePluginIds(), ["theme", "data", "preview"])
        self.assertIs(dependent.context_copy.activePlugin("data"), data)
        self.assertTrue(manager.activate("preview"))
        self.assertEqual(self.events, [("activate", name) for name in ("theme", "data", "preview")])
        self.assertTrue(manager.deactivate("theme"))
        self.assertEqual(self.events[-3:], [("deactivate", name) for name in ("preview", "data", "theme")])
        self.assertEqual(manager.activePluginIds(), [])
        self.assertEqual(manager.pluginIds(), ["preview", "data", "theme"])

    def test_failure_rolls_back_new_dependencies_and_preserves_previous_activation(self):
        manager = self.manager()
        stable = self.extension("stable")
        temporary = self.extension("temporary", ["stable"])
        failing = self.extension("failing", ["temporary"], succeeds=False)
        for plugin in (stable, temporary, failing):
            self.assertTrue(manager.registerPlugin(plugin))
        self.assertTrue(manager.activate("stable"))
        self.events.clear()
        self.assertFalse(manager.activate("failing"))
        self.assertEqual(self.events, [
            ("activate", "temporary"), ("activate", "failing"),
            ("deactivate", "failing"), ("deactivate", "temporary"),
        ])
        self.assertEqual(manager.activePluginIds(), ["stable"])
        self.assertIs(manager.activePlugin("stable"), stable)
        self.assertIsNone(manager.activePlugin("failing"))

    def test_cycles_and_missing_dependencies_fail_before_python_hooks_run(self):
        manager = self.manager()
        for plugin in (self.extension("first", ["second"]),
                       self.extension("second", ["first"]),
                       self.extension("missing", ["absent"])):
            self.assertTrue(manager.registerPlugin(plugin))
        self.assertFalse(manager.activate("first"))
        self.assertFalse(manager.activate("missing"))
        self.assertEqual(self.events, [])
        self.assertEqual(manager.activePluginIds(), [])

    def test_registration_rejects_invalid_metadata_and_duplicate_manager_identity(self):
        manager = self.manager()
        for identifier, dependencies in (("", []), (" spaced ", []),
                                         ("duplicate-dependencies", ["base", "base"])):
            with self.subTest(identifier=identifier):
                self.assertFalse(manager.registerPlugin(self.extension(identifier, dependencies)))
        plugin = self.extension("valid")
        self.assertTrue(manager.registerPlugin(plugin))
        self.assertFalse(manager.registerPlugin(self.extension("valid")))
        self.assertFalse(self.manager().registerPlugin(plugin))
        self.assertEqual(manager.pluginIds(), ["valid"])

    def test_nested_mutations_from_python_hooks_and_signals_are_rejected(self):
        manager = self.manager()
        plugin = self.extension("outer")
        nested = self.extension("nested")
        results = []
        plugin.activation_probe = lambda context: results.append(manager.registerPlugin(nested))
        plugin.deactivation_probe = lambda: results.append(manager.activate("outer"))
        manager.pluginActivated.connect(lambda identifier: results.append(manager.deactivate(identifier)))
        self.assertTrue(manager.registerPlugin(plugin))
        self.assertTrue(manager.activate("outer"))
        self.assertTrue(manager.isActive("outer"))
        self.assertEqual(manager.pluginIds(), ["outer"])
        self.assertTrue(manager.deactivate("outer"))
        self.assertEqual(results, [False, False, False])

    def test_unparented_python_plugin_collection_unregisters_it_and_stops_dependents(self):
        manager = self.manager()
        plugin = RecordingPlugin("retained", self.events)
        dependent = self.extension("dependent", ["retained"])
        reference = weakref.ref(plugin)
        self.assertTrue(manager.registerPlugin(plugin))
        self.assertTrue(manager.registerPlugin(dependent))
        self.assertTrue(manager.activate("dependent"))
        self.events.clear()
        del plugin
        gc.collect()
        self.assertIsNone(reference())
        self.assertEqual(manager.pluginIds(), ["dependent"])
        self.assertEqual(manager.activePluginIds(), [])
        self.assertEqual(self.events, [("deactivate", "dependent")])

    def test_qt_parent_retains_python_plugin_callbacks_after_gc(self):
        owner = self.own(QObject())
        manager = self.manager()
        plugin = RecordingPlugin("parented", self.events, parent=owner)
        reference = weakref.ref(plugin)
        self.assertTrue(manager.registerPlugin(plugin))
        del plugin
        gc.collect()
        self.assertIsNotNone(reference())
        self.assertIs(reference().parent(), owner)
        self.assertTrue(manager.activate("parented"))
        self.assertTrue(manager.deactivate("parented"))
        self.assertEqual(self.events, [("activate", "parented"), ("deactivate", "parented")])

    def test_manager_does_not_keep_a_borrowed_host_alive_after_gc(self):
        host = QObject()
        reference = weakref.ref(host)
        manager = self.manager(host)
        del host
        gc.collect()
        self.assertIsNone(reference())
        self.assertIsNone(manager.context().host())

    def test_python_activation_exception_uses_native_failure_cleanup(self):
        manager = self.manager()
        dependency = self.extension("dependency")
        failing = self.extension("failure", ["dependency"])

        def fail(context):
            raise RuntimeError("Python activation rejected")

        failing.activation_probe = fail
        self.assertTrue(manager.registerPlugin(dependency))
        self.assertTrue(manager.registerPlugin(failing))
        self.assertFalse(manager.activate("failure"))
        self.assertIn("Python activation rejected", manager.lastError())
        self.assertEqual(manager.activePluginIds(), [])
        self.assertEqual(self.events, [
            ("activate", "dependency"), ("activate", "failure"),
            ("deactivate", "failure"), ("deactivate", "dependency"),
        ])

    def test_python_metadata_exception_rejects_registration_with_a_diagnostic(self):
        class InvalidMetadata(RecordingPlugin):
            def metadata(self):
                raise RuntimeError("Python metadata rejected")

        manager = self.manager()
        plugin = self.own(InvalidMetadata("invalid", self.events))
        self.assertFalse(manager.registerPlugin(plugin))
        self.assertIn("Python metadata rejected", manager.lastError())
        self.assertEqual(manager.pluginIds(), [])
        self.assertEqual(self.events, [])

    def test_python_cleanup_exception_is_contained_and_does_not_block_reactivation(self):
        manager = self.manager()
        plugin = self.extension("cleanup")

        def fail_cleanup():
            raise RuntimeError("Python cleanup rejected")

        plugin.deactivation_probe = fail_cleanup
        self.assertTrue(manager.registerPlugin(plugin))
        self.assertTrue(manager.activate("cleanup"))
        self.assertTrue(manager.deactivate("cleanup"))
        self.assertFalse(manager.isActive("cleanup"))
        self.assertIn("Python cleanup rejected", plugin.lastError())
        plugin.deactivation_probe = None
        self.assertTrue(manager.activate("cleanup"))
        self.assertTrue(manager.isActive("cleanup"))
        self.assertEqual(plugin.lastError(), "")

    def test_explicitly_destroyed_borrowed_plugin_stops_surviving_dependents(self):
        owner = self.own(QObject())
        manager = self.manager()
        dependency = self.extension("dependency", parent=owner)
        dependent = self.extension("dependent", ["dependency"])
        self.assertTrue(manager.registerPlugin(dependency))
        self.assertTrue(manager.registerPlugin(dependent))
        self.assertTrue(manager.activate("dependent"))
        self.events.clear()
        delete_qobject(owner)
        self.assertFalse(shiboken6.isValid(dependency))
        self.assertTrue(shiboken6.isValid(dependent))
        self.assertEqual(manager.pluginIds(), ["dependent"])
        self.assertEqual(manager.activePluginIds(), [])
        self.assertEqual(self.events, [("deactivate", "dependent")])

    def test_context_value_copy_is_guarded_after_manager_and_host_destruction(self):
        host = self.own(QObject())
        manager = self.manager(host)
        plugin = self.extension("context")
        self.assertTrue(manager.registerPlugin(plugin))
        self.assertTrue(manager.activate("context"))
        context = plugin.context_copy
        self.assertIs(context.host(), host)
        self.assertIs(context.activePlugin("context"), plugin)
        delete_qobject(manager)
        self.assertTrue(shiboken6.isValid(plugin))
        self.assertIsNone(context.activePlugin("context"))
        self.assertIs(context.host(), host)
        delete_qobject(host)
        gc.collect()
        self.assertIsNone(context.host())
        self.assertIsNone(context.activePlugin("context"))

    def test_manager_destruction_stops_borrowed_plugins_without_destroying_them(self):
        host = self.own(QObject())
        manager = self.manager(host)
        plugin = self.extension("borrowed")
        self.assertTrue(manager.registerPlugin(plugin))
        self.assertTrue(manager.activate("borrowed"))
        delete_qobject(manager)
        self.assertTrue(shiboken6.isValid(plugin))
        self.assertTrue(shiboken6.isValid(host))
        self.assertIsNone(plugin.parent())
        self.assertEqual(self.events[-1], ("deactivate", "borrowed"))

    def test_widget_factory_requires_activation_and_rejects_unknown_keys(self):
        manager = self.manager()
        plugin = self.own(ButtonsPlugin(self.events))
        self.assertTrue(manager.registerPlugin(plugin))
        self.assertIsNone(manager.createWidget("example.buttons", "button"))
        self.assertEqual(plugin.factory_calls, 0)
        self.assertTrue(manager.activate("example.buttons"))
        self.assertIsNone(manager.createWidget("example.buttons", "unknown"))
        self.assertEqual(plugin.factory_calls, 0)

    def test_parentless_factory_widget_survives_gc_and_manager_and_plugin_destruction(self):
        manager = self.manager()
        plugin = self.own(ButtonsPlugin(self.events))
        self.assertTrue(manager.registerPlugin(plugin))
        self.assertTrue(manager.activate("example.buttons"))
        widget = self.own(manager.createWidget("example.buttons", "button"))
        self.assertIsInstance(widget, fluentqt.Button)
        self.assertIsNone(widget.parentWidget())
        gc.collect()
        self.assertTrue(shiboken6.isValid(widget))
        self.assertEqual(widget.text(), "Extension command")
        widget.clicked.connect(lambda: widget.setText("Executed"))
        delete_qobject(manager)
        delete_qobject(plugin)
        gc.collect()
        self.assertTrue(shiboken6.isValid(widget))
        widget.click()
        self.assertEqual(widget.text(), "Executed")

    def test_parentless_pure_factory_output_is_collected_when_the_caller_releases_it(self):
        manager = self.manager()
        plugin = self.own(ButtonsPlugin(self.events))
        self.assertTrue(manager.registerPlugin(plugin))
        self.assertTrue(manager.activate("example.buttons"))
        widget = manager.createWidget("example.buttons", "button")
        self.assertIsInstance(widget, fluentqt.Button)
        self.assertTrue(shiboken6.isValid(widget))
        reference = weakref.ref(widget)
        del widget
        gc.collect()
        self.assertIsNone(reference())
        self.assertEqual(plugin.destroyed_outputs, ["button"])
        self.assertTrue(manager.isActive("example.buttons"))

    def test_parented_factory_widget_keeps_requested_parent_and_survives_plugin_cleanup(self):
        parent = self.own(QWidget())
        manager = self.manager()
        plugin = self.own(ButtonsPlugin(self.events))
        self.assertTrue(manager.registerPlugin(plugin))
        self.assertTrue(manager.activate("example.buttons"))
        widget = self.own(manager.createWidget("example.buttons", "button", parent))
        self.assertIsInstance(widget, fluentqt.Button)
        self.assertIs(widget.parentWidget(), parent)
        self.assertTrue(manager.unregisterPlugin("example.buttons"))
        delete_qobject(plugin)
        gc.collect()
        self.assertTrue(shiboken6.isValid(widget))
        self.assertIs(widget.parentWidget(), parent)
        self.assertEqual(parent.findChildren(fluentqt.Button), [widget])

    def test_factory_output_with_wrong_parent_is_rejected_and_destroyed(self):
        parent = self.own(QWidget())
        manager = self.manager()
        plugin = self.own(ButtonsPlugin(self.events))
        plugin.wrong_parent = True
        self.assertTrue(manager.registerPlugin(plugin))
        self.assertTrue(manager.activate("example.buttons"))
        self.assertIsNone(manager.createWidget("example.buttons", "button", parent))
        self.assertTrue(manager.lastError())
        self.assertEqual(plugin.factory_calls, 1)
        self.assertEqual(plugin.destroyed_outputs, ["button"])
        rejected = plugin.last_widget()
        self.assertTrue(rejected is None or not shiboken6.isValid(rejected))
        self.assertEqual(parent.findChildren(fluentqt.Button), [])

    def test_python_factory_exception_reports_failure_without_stopping_the_plugin(self):
        manager = self.manager()
        plugin = self.own(ButtonsPlugin(self.events))
        plugin.failure_message = "Python widget factory rejected"
        self.assertTrue(manager.registerPlugin(plugin))
        self.assertTrue(manager.activate("example.buttons"))
        self.assertIsNone(manager.createWidget("example.buttons", "button"))
        self.assertIn("Python widget factory rejected", manager.lastError())
        self.assertTrue(manager.isActive("example.buttons"))
        self.assertEqual(plugin.factory_calls, 1)
        self.assertIsNone(plugin.last_widget)
        self.assertEqual(self.events, [("activate", "example.buttons")])

    def test_spatial_plugin_joins_python_dependencies_and_preserves_view_ownership(self):
        if importlib.util.find_spec("fluentqt.spatial") is None:
            self.skipTest("This binding build does not include the optional Spatial module")
        from fluentqt._fluentqt import fluent as native
        from fluentqt.spatial import SpatialPlugin

        parent = self.own(QWidget())
        manager = self.manager(parent)
        spatial = self.own(SpatialPlugin())
        dependent = self.extension("example.overlay", ["fluentqt.spatial"])
        self.assertTrue(manager.registerPlugin(dependent))
        self.assertTrue(manager.registerPlugin(spatial))
        self.assertTrue(manager.activate("example.overlay"))
        self.assertEqual(manager.activePluginIds(), ["fluentqt.spatial", "example.overlay"])
        self.assertIs(dependent.context_copy.activePlugin("fluentqt.spatial"), spatial)

        view = self.own(manager.createWidget("fluentqt.spatial", "spatial-view", parent))
        self.assertIsInstance(view, native.SpatialView)
        self.assertIs(view.parentWidget(), parent)
        model = self.own(fluentqt.ChartModel())
        chart = fluentqt.DonutChart()
        chart.setModel(model)
        reference = weakref.ref(chart)
        item = view.addOwnedWidget(chart)
        del chart
        gc.collect()
        self.assertIsNotNone(reference())
        self.assertIs(item.widget().model(), model)
        self.assertTrue(manager.deactivate("fluentqt.spatial"))
        self.assertFalse(view.isSpatialEnabled())
        self.assertEqual(view.itemCount(), 1)
        self.assertEqual(self.events[-1], ("deactivate", "example.overlay"))
        delete_qobject(manager)
        delete_qobject(spatial)
        gc.collect()
        self.assertTrue(shiboken6.isValid(view))
        self.assertIs(view.parentWidget(), parent)
        self.assertIs(item.widget().model(), model)
        view.releaseItem(item)
        self.assertTrue(shiboken6.isValid(model))


if __name__ == "__main__":
    unittest.main()
