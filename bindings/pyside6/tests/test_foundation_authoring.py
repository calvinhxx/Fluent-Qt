"""Behavior tests for the Python-safe Fluent foundation authoring API."""

from __future__ import annotations

import json
import gc
import os
import subprocess
import sys
import weakref
from pathlib import Path
import unittest

import fluentqt
import shiboken6

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QCoreApplication, QEvent, QObject, QThread
from PySide6.QtWidgets import QApplication, QLabel, QSlider, QWidget
from PySide6.QtTest import QTest


def delete_qobject_immediately(obj):
    # Shiboken 6.2.4 delete() has an uninitialized isQAppSingleton flag.
    # Deliver Qt's deletion event synchronously to retain reentrancy coverage.
    obj.deleteLater()
    QCoreApplication.sendPostedEvents(obj, QEvent.DeferredDelete)
    if shiboken6.isValid(obj):
        raise AssertionError("Qt did not synchronously delete the test object")


class FoundationAuthoringTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        if not fluentqt.initialize_resources():
            raise RuntimeError("FluentQt resources could not be initialized")

    def tearDown(self):
        self.assertTrue(shiboken6.isValid(self.app), "Object teardown deleted QApplication")
        self.assertIs(QApplication.instance(), self.app)
        fluentqt.reset_theme_tokens()
        fluentqt.set_theme(fluentqt.Theme.Light)
        fluentqt.set_motion_mode(fluentqt.MotionMode.Full)
        QApplication.processEvents()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QApplication.processEvents()

    def test_design_namespaces_match_the_cpp_token_sources(self):
        aliases_path = (
            Path(__file__).resolve().parents[3]
            / "tools"
            / "fonts"
            / "fluent_icon_aliases.json"
        )
        expected_aliases = json.loads(aliases_path.read_text(encoding="utf-8"))
        actual_aliases = {
            name: value
            for name, value in vars(fluentqt.Icons).items()
            if not name.startswith("_") and isinstance(value, str)
        }

        self.assertEqual(actual_aliases, expected_aliases)
        self.assertIs(fluentqt.Typography.Icons, fluentqt.Icons)
        self.assertIs(fluentqt.Typography.IconSize, fluentqt.IconSize)
        self.assertIs(fluentqt.Typography.FontRole, fluentqt.FontRole)
        self.assertEqual(
            fluentqt.Typography.Icons.Add,
            "ic_fluent_add_20_regular",
        )
        self.assertEqual(fluentqt.IconSize.Compact, 12)
        self.assertEqual(fluentqt.IconSize.XLarge, 24)

        self.assertEqual(fluentqt.Spacing.BaseUnit, 4)
        self.assertEqual(fluentqt.Spacing.Padding.ComboBoxHorizontal, 11)
        self.assertEqual(fluentqt.Spacing.Border.Focused, 2)
        self.assertEqual(fluentqt.Spacing.Gap.Section, 24)
        self.assertEqual(fluentqt.Spacing.ControlHeight.Standard, 32)
        self.assertEqual(fluentqt.CornerRadius.None_, 0)
        self.assertEqual(fluentqt.CornerRadius.Control, 4)
        self.assertEqual(fluentqt.CornerRadius.Indicator, 1.5)

        with self.assertRaises(AttributeError):
            fluentqt.Icons.Add = "replacement"
        with self.assertRaises(AttributeError):
            fluentqt.Spacing.Border.Focused = 3
        with self.assertRaises(TypeError):
            fluentqt.Spacing()

    def test_property_binding_reuses_native_one_way_and_two_way_engine(self):
        source = QSlider()
        target = QSlider()

        fluentqt.bind(source, "value", target, "value")
        source.setValue(37)
        self.assertEqual(target.value(), 37)

        first = QSlider()
        second = QSlider()
        fluentqt.bind(
            first,
            "value",
            second,
            "value",
            fluentqt.BindingMode.TwoWay,
        )
        first.setValue(41)
        self.assertEqual(second.value(), 41)
        second.setValue(73)
        self.assertEqual(first.value(), 73)

        with self.assertRaises(ValueError):
            fluentqt.bind(source, "missing", target, "value")

        lifetime_source = QSlider()
        lifetime_target = QSlider()
        fluentqt.bind(
            lifetime_source,
            "value",
            lifetime_target,
            "value",
            fluentqt.BindingMode.TwoWay,
        )
        lifetime_target.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.assertFalse(shiboken6.isValid(lifetime_target))
        lifetime_source.setValue(61)
        self.assertEqual(lifetime_source.value(), 61)

    def test_legacy_bind_rejects_foreign_thread_without_replacing_existing_binding(self):
        original, source, target = QObject(), QObject(), QObject()
        original.setObjectName("original")
        source.setObjectName("foreign")
        binding = fluentqt.bind_managed(original, "objectName", target, "objectName")
        self.assertTrue(binding.isValid())

        thread = QThread()
        source.moveToThread(thread)
        thread.finished.connect(source.deleteLater)
        thread.start()
        try:
            with self.assertRaises(ValueError):
                fluentqt.bind(source, "objectName", target, "objectName")
            self.assertEqual(target.objectName(), "original")
            self.assertTrue(binding.isValid())
            original.setObjectName("updated")
            self.assertEqual(target.objectName(), "updated")
        finally:
            thread.quit()
            thread.wait()

    def test_computed_and_state_authoring_reject_foreign_threads_before_evaluation(self):
        source, foreign_target, target = QObject(), QObject(), QObject()
        controller = fluentqt.StateGroup()
        thread = QThread()
        for obj in (source, foreign_target, controller):
            obj.moveToThread(thread)
            thread.finished.connect(obj.deleteLater)
        thread.start()
        calls = []
        try:
            with self.assertRaisesRegex(ValueError, "target's Qt thread"):
                fluentqt.bind_computed(
                    foreign_target, "objectName", [], lambda: calls.append(True))
            with self.assertRaisesRegex(ValueError, "binding's Qt thread"):
                fluentqt.bind_computed(
                    target, "objectName", [(source, "objectName")],
                    lambda: calls.append(True))
            with self.assertRaisesRegex(ValueError, "controller's Qt thread"):
                controller.add("active", {}, when=lambda: calls.append(True))
            self.assertFalse(calls)
            self.assertEqual(target.objectName(), "")
            self.assertFalse(controller.has("active"))
        finally:
            thread.quit()
            thread.wait()

    def test_authoring_thread_checks_allow_clean_process_shutdown(self):
        probe = """
import fluentqt
from PySide6.QtWidgets import QApplication, QLabel
app = QApplication([])
target = QLabel()
binding = fluentqt.bind_computed(target, "text", [], lambda: "ready")
assert binding.isValid() and target.text() == "ready"
states = fluentqt.StateGroup()
states.add("busy", {target: {"enabled": False}})
states.set("busy")
assert not target.isEnabled()
states.clear()
assert target.isEnabled()
"""
        environment = dict(os.environ, QT_QPA_PLATFORM="offscreen",
                           PYTHONMALLOC="malloc", MALLOC_PERTURB_="165")
        process = subprocess.run(
            [sys.executable, "-X", "faulthandler", "-c", probe],
            env=environment, capture_output=True, text=True, timeout=30)
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)

    def test_legacy_bind_reports_target_destruction_during_initial_sync(self):
        source, target = QSlider(), QSlider()
        source.setValue(20)
        target.valueChanged.connect(lambda value: delete_qobject_immediately(target))
        with self.assertRaises(ValueError):
            fluentqt.bind(source, "value", target, "value")
        self.assertFalse(shiboken6.isValid(target))

    def test_legacy_bind_replaces_target_binding_and_keeps_notify_requirement(self):
        first, second, target = QSlider(), QSlider(), QSlider()
        first.setValue(10)
        second.setValue(20)
        fluentqt.bind(first, "value", target, "value")
        fluentqt.bind(second, "value", target, "value")
        self.assertEqual(target.value(), 20)
        first.setValue(30)
        self.assertEqual(target.value(), 20)

        unobserved = QWidget()
        unobserved.setMinimumWidth(40)
        with self.assertRaises(ValueError):
            fluentqt.bind(unobserved, "minimumWidth", target, "value")
        second.setValue(50)
        self.assertEqual(target.value(), 50)

    def test_managed_binding_pauses_resumes_and_survives_a_dropped_handle(self):
        source, target = QSlider(), QSlider()
        source.setValue(20)
        handle = fluentqt.bind_managed(source, "value", target, "value")
        self.assertEqual(target.value(), 20)
        handle.pause()
        source.setValue(70)
        self.assertEqual(target.value(), 20)
        self.assertTrue(handle.isSuspended())
        handle.resume()
        self.assertEqual(target.value(), 70)
        handle.setEnabled(False)
        source.setValue(40)
        self.assertEqual(target.value(), 70)
        handle.setEnabled(True)
        self.assertEqual(target.value(), 40)
        del handle
        gc.collect()
        source.setValue(55)
        self.assertEqual(target.value(), 55)

    def test_computed_binding_coalesces_and_stops_after_dependency_destruction(self):
        first, second, label = QSlider(), QSlider(), QLabel()
        calls = []

        def compute():
            calls.append(True)
            return str(first.value() + second.value())

        handle = fluentqt.bind_computed(
            label, "text", [(first, "value"), (second, "value")], compute, deferred=True)
        self.assertEqual(label.text(), "0")
        self.assertEqual(len(calls), 1)
        first.setValue(20)
        second.setValue(30)
        self.assertEqual(label.text(), "0")
        QApplication.processEvents()
        self.assertEqual(label.text(), "50")
        self.assertEqual(len(calls), 2)
        handle.pause()
        first.setValue(40)
        QApplication.processEvents()
        self.assertEqual(len(calls), 2)
        handle.resume()
        self.assertEqual(label.text(), "70")
        first.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        count = len(calls)
        second.setValue(60)
        QApplication.processEvents()
        self.assertFalse(handle.isValid())
        self.assertEqual(len(calls), count)

    def test_computed_failure_holds_value_and_invalid_replacement_preserves_binding(self):
        source, label = QSlider(), QLabel()

        def compute():
            if source.value() == 60:
                raise RuntimeError("expression failed")
            return str(source.value())

        handle = fluentqt.bind_computed(label, "text", [(source, "value")], compute)
        errors = []
        expression = label._fluentqt_binding_expressions["text"]
        expression.error_occurred.connect(
            lambda error: errors.append((error, sys.exc_info()[0])))
        source.setValue(25)
        with self.assertRaises(ValueError):
            fluentqt.bind_computed(label, "text", [(source, "missing")], lambda: "bad")
        self.assertTrue(handle.isValid())
        source.setValue(30)
        self.assertEqual(label.text(), "30")
        source.setValue(60)
        self.assertEqual(label.text(), "30")
        self.assertTrue(handle.isSuspended())
        self.assertIsInstance(handle.last_error, RuntimeError)
        self.assertEqual(len(errors), 1)
        self.assertIs(errors[0][0], handle.last_error)
        self.assertIsNone(errors[0][1])
        source.setValue(35)
        handle.resume()
        self.assertEqual(label.text(), "35")
        self.assertIsNone(handle.last_error)
        replacement = fluentqt.bind_managed(source, "value", label, "text")
        self.assertTrue(replacement.isValid())
        self.assertFalse(handle.isValid())
        QApplication.processEvents()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.assertFalse(getattr(label, "_fluentqt_binding_expressions", {}))

    def test_computed_target_teardown_releases_the_retained_callable(self):
        class Expression:
            def __call__(self):
                return "alive"

        label = QLabel()
        expression = Expression()
        expression_ref = weakref.ref(expression)
        handle = fluentqt.bind_computed(label, "text", [], expression)
        del expression
        gc.collect()
        self.assertIsNotNone(expression_ref())
        label.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QApplication.processEvents()
        gc.collect()
        self.assertFalse(handle.isValid())
        self.assertIsNone(expression_ref())

    def test_computed_evaluator_can_destroy_its_target_without_stale_writes(self):
        source, target = QSlider(), QLabel()
        expression_survived = []
        evaluator_ownership = []

        def compute():
            if source.value() == 40:
                expression = target._fluentqt_binding_expressions["text"]
                evaluator_ownership.append(shiboken6.ownedByPython(expression))
                delete_qobject_immediately(target)
                expression_survived.append(shiboken6.isValid(expression))
            return "value"

        handle = fluentqt.bind_computed(target, "text", [(source, "value")], compute)
        expression = target._fluentqt_binding_expressions["text"]
        self.assertIs(expression.parent(), target)
        self.assertFalse(shiboken6.ownedByPython(expression))
        source.setValue(40)
        self.assertEqual(evaluator_ownership, [True])
        self.assertEqual(expression_survived, [True])
        self.assertFalse(shiboken6.isValid(target))
        self.assertFalse(handle.isValid())
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.assertFalse(shiboken6.isValid(expression))

    def test_computed_error_listener_can_destroy_its_target(self):
        source, target = QSlider(), QLabel()
        expression_survived = []

        def compute():
            if source.value():
                raise RuntimeError("evaluation failed")
            return "value"

        handle = fluentqt.bind_computed(target, "text", [(source, "value")], compute)
        expression = target._fluentqt_binding_expressions["text"]

        def failed(error):
            delete_qobject_immediately(target)
            expression_survived.append(shiboken6.isValid(expression))

        expression.error_occurred.connect(failed)
        source.setValue(1)
        self.assertEqual(expression_survived, [True])
        self.assertFalse(handle.isValid())
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.assertFalse(shiboken6.isValid(expression))

    def test_initial_computed_evaluator_can_destroy_its_target(self):
        target = QLabel()

        def compute():
            delete_qobject_immediately(target)
            return "unused"

        with self.assertRaisesRegex(ValueError, "destroyed its target"):
            fluentqt.bind_computed(target, "text", [], compute)
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.assertFalse(shiboken6.isValid(target))

    def test_conditional_evaluator_can_destroy_its_controller(self):
        source, target = QSlider(), QLabel("Default")
        states = fluentqt.StateGroup()
        expression_survived = []
        evaluator_ownership = []

        def condition():
            if source.value() == 40:
                expression = states._fluentqt_conditions["active"]
                evaluator_ownership.append(shiboken6.ownedByPython(expression))
                delete_qobject_immediately(states)
                expression_survived.append(shiboken6.isValid(expression))
            return False

        states.add("active", {target: {"text": "Active"}}, when=condition,
                   dependencies=[(source, "value")])
        expression = states._fluentqt_conditions["active"]
        source.setValue(40)
        self.assertEqual(evaluator_ownership, [True])
        self.assertEqual(expression_survived, [True])
        self.assertFalse(shiboken6.isValid(states))
        self.assertEqual(target.text(), "Default")
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.assertFalse(shiboken6.isValid(expression))

    def test_conditional_signal_can_destroy_the_controller(self):
        source, label = QSlider(), QLabel("Default")
        states = fluentqt.StateGroup()
        states.state_changed.connect(lambda name: delete_qobject_immediately(states) if name else None)
        states.add("active", {label: {"text": "Active"}},
                   when=lambda: source.value() > 50, dependencies=[(source, "value")])
        source.setValue(70)
        self.assertFalse(shiboken6.isValid(states))
        source.setValue(30)
        QApplication.processEvents()

    def test_conditional_inherited_states_and_two_way_binding_restoration(self):
        source, target, label = QSlider(), QSlider(), QLabel("Ready")
        source.setValue(20)
        fluentqt.bind(source, "value", target, "value", fluentqt.BindingMode.TwoWay)
        states = fluentqt.StateGroup()
        changed = []
        states.state_changed.connect(changed.append)
        states.add("busy", {label: {"enabled": False}})
        states.add("saving", {label: {"text": "Saving..."}, target: {"value": 99}},
                   extend="busy", when=lambda: source.value() > 50,
                   dependencies=[(source, "value")])
        source.setValue(70)
        self.assertEqual(states.state(), "saving")
        self.assertFalse(label.isEnabled())
        self.assertEqual(label.text(), "Saving...")
        self.assertEqual(target.value(), 99)
        self.assertEqual(source.value(), 70)
        source.setValue(30)
        self.assertEqual(states.state(), "")
        self.assertTrue(label.isEnabled())
        self.assertEqual(label.text(), "Ready")
        self.assertEqual(target.value(), 30)
        self.assertEqual(changed, ["saving", ""])

    def test_behavior_and_transition_use_the_native_motion_policy(self):
        fluentqt.set_motion_mode(fluentqt.MotionMode.Disabled)
        source = QSlider()
        bar = fluentqt.ProgressBar()
        fluentqt.set_behavior(bar, "value", duration=300)
        fluentqt.bind(source, "value", bar, "value")
        source.setValue(60)
        self.assertEqual(bar.value(), 60)
        states = fluentqt.StateGroup()
        finished = []
        states.transition_finished.connect(finished.append)
        states.add("high", {bar: {"value": 80.0}})
        states.add_transition(["value"], duration=300)
        states.set("high")
        self.assertEqual(bar.value(), 80)
        self.assertEqual(finished, ["high"])
        source.setValue(40)
        self.assertEqual(bar.value(), 80)
        states.clear()
        self.assertEqual(bar.value(), 40)

    def test_state_anchor_changes_restore_the_original_native_layout(self):
        surface = fluentqt.Card()
        surface.resize(360, 120)
        button = fluentqt.Button("Move me", surface)
        anchors = fluentqt.AnchorLayout(surface)
        anchors.addWidget(button, fluentqt.anchors(left=(surface, 12), vertical_center=surface))
        states = fluentqt.StateGroup(surface)
        states.add("right", {}, anchor_changes={button: fluentqt.anchors(
            right=(surface, -12), vertical_center=surface)})
        surface.show()
        QApplication.processEvents()
        self.assertEqual(button.x(), 12)
        states.set("right")
        QApplication.processEvents()
        self.assertEqual(button.x() + button.width(), surface.width() - 12)
        states.clear()
        QApplication.processEvents()
        self.assertEqual(button.x(), 12)
        surface.deleteLater()

    def test_state_group_applies_and_restores_properties(self):
        label = QLabel("Ready")
        states = fluentqt.StateGroup()
        changed = []
        states.state_changed.connect(changed.append)
        states.add("busy", {label: {"text": "Working...", "enabled": False}})

        states.set("busy")
        self.assertEqual(label.text(), "Working...")
        self.assertFalse(label.isEnabled())
        self.assertEqual(states.state(), "busy")

        states.clear()
        self.assertEqual(label.text(), "Ready")
        self.assertTrue(label.isEnabled())
        self.assertEqual(changed, ["busy", ""])

        with self.assertRaises(KeyError):
            states.set("missing")
        with self.assertRaises(ValueError):
            states.add("busy", {label: {"missingProperty": True}})
        self.assertTrue(states.has("busy"))
        states.set("busy")
        self.assertEqual(label.text(), "Working...")
        states.clear()

        owner = QWidget()
        transient = QLabel("Transient", owner)
        states.add("transient", {transient: {"text": "Changed"}})
        owner.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.assertFalse(shiboken6.isValid(transient))
        states.set("transient")
        self.assertEqual(states.state(), "transient")

    def test_anchor_layout_centers_and_pins_widgets(self):
        host = QWidget()
        host.resize(240, 140)
        centered = QLabel("Center", host)
        pinned = QLabel("Pinned", host)
        filled = QWidget(host)
        layout = fluentqt.AnchorLayout(host)
        layout.addWidget(filled, fluentqt.anchors(fill=(5, 6, 7, 8)))
        layout.addWidget(centered, fluentqt.anchors(center_in=host))
        layout.addWidget(pinned, fluentqt.anchors(top_right=(host, 12)))

        host.show()
        QApplication.processEvents()

        self.assertLessEqual(
            abs(centered.geometry().center().x() - host.rect().center().x()),
            1,
        )
        self.assertLessEqual(
            abs(centered.geometry().center().y() - host.rect().center().y()),
            1,
        )
        self.assertEqual(pinned.geometry().top(), 12)
        self.assertEqual(host.width() - pinned.geometry().right() - 1, 12)
        self.assertEqual(filled.geometry().left(), 5)
        self.assertEqual(filled.geometry().top(), 6)
        self.assertEqual(host.width() - filled.geometry().right() - 1, 7)
        self.assertEqual(host.height() - filled.geometry().bottom() - 1, 8)

    def test_fluent_widget_is_subclassable_and_receives_theme_updates(self):
        updates = []
        self.assertFalse(hasattr(fluentqt.FluentWidget, "onThemeUpdated"))
        self.assertTrue(hasattr(fluentqt.FluentWidget, "on_theme_updated"))

        class PythonCard(fluentqt.FluentWidget):
            def __init__(self):
                super().__init__()
                self.ready = True

            def on_theme_updated(self):
                updates.append((self.ready, self.effective_theme()))
                super().on_theme_updated()

        card = PythonCard()
        initial_updates = len(updates)
        tokens = card.theme_tokens()
        self.assertIsInstance(tokens, fluentqt.ThemeTokens)
        self.assertIsInstance(tokens, dict)
        self.assertIn("colors", tokens)
        self.assertIn("radius", tokens)
        self.assertIn("spacing", tokens)
        self.assertEqual(
            tokens["spacing"]["border"],
            {"focused": 2, "normal": 1},
        )
        self.assertEqual(tokens.spacing.border.focused, 2)
        self.assertEqual(
            tokens.colors.strokeFocusOuter,
            tokens["colors"]["strokeFocusOuter"],
        )
        self.assertEqual(
            tokens.colors.bgLayerOverlay,
            tokens["colors"]["bgLayerOverlay"],
        )
        self.assertIn("animation", tokens)
        self.assertIn("material", tokens)
        self.assertIn("elevation", tokens)
        self.assertIn("breakpoints", tokens)
        self.assertIn("backdrop", tokens)
        self.assertEqual(tokens["breakpoints"]["small"], 640)
        self.assertGreater(tokens["animation"]["duration"]["normal"], 0)
        self.assertFalse(card.theme_font().family() == "")

        next_theme = (
            fluentqt.Theme.Dark
            if fluentqt.current_theme() == fluentqt.Theme.Light
            else fluentqt.Theme.Light
        )
        fluentqt.set_theme(next_theme)
        QApplication.processEvents()
        self.assertGreater(len(updates), initial_updates)
        self.assertEqual(card.effective_theme(), next_theme)
        self.assertTrue(all(ready for ready, _theme in updates))

    def test_high_contrast_theme_round_trips_with_distinct_tokens(self):
        card = fluentqt.FluentWidget()

        self.assertFalse(
            fluentqt.theme_uses_dark_appearance(fluentqt.Theme.Light)
        )
        self.assertTrue(
            fluentqt.theme_uses_dark_appearance(fluentqt.Theme.Dark)
        )
        self.assertTrue(
            fluentqt.theme_uses_dark_appearance(
                fluentqt.Theme.HighContrast
            )
        )

        fluentqt.set_theme(fluentqt.Theme.HighContrast)
        QApplication.processEvents()

        self.assertEqual(
            fluentqt.current_theme(),
            fluentqt.Theme.HighContrast,
        )
        self.assertEqual(
            card.effective_theme(),
            fluentqt.Theme.HighContrast,
        )
        self.assertEqual(card.theme_tokens().colors.bgCanvas.name(), "#000000")
        self.assertEqual(card.theme_tokens().colors.textPrimary.name(), "#ffffff")
        self.assertEqual(fluentqt.accent_color().name(), "#1aebff")

    def test_motion_policy_facade_uses_native_rules_and_emits_once(self):
        policy = fluentqt.motion_policy()
        self.assertIs(policy, fluentqt.MotionPolicy())
        self.assertIs(fluentqt.MotionPolicy.Mode, fluentqt.MotionMode)
        self.assertIs(fluentqt.MotionPolicy.Kind, fluentqt.MotionKind)
        self.assertEqual(
            policy.mode(),
            fluentqt.MotionMode.Full,
        )
        self.assertTrue(policy.shouldAnimate())
        self.assertTrue(
            policy.shouldAnimate(True, fluentqt.MotionKind.Continuous)
        )
        self.assertEqual(policy.resolvedDuration(250), 250)

        emitted = []
        handler = emitted.append
        policy.modeChanged.connect(handler)
        try:
            fluentqt.set_motion_mode(fluentqt.MotionMode.Reduced)
            self.assertEqual(emitted, [fluentqt.MotionMode.Reduced])
            self.assertEqual(policy.mode(), fluentqt.MotionMode.Reduced)
            self.assertTrue(policy.shouldAnimate())
            self.assertFalse(
                policy.shouldAnimate(True, fluentqt.MotionKind.Continuous)
            )
            self.assertEqual(policy.resolvedDuration(250), 50)
            self.assertEqual(policy.resolvedDuration(30), 30)
            self.assertEqual(policy.resolvedDuration(250, False), 0)

            fluentqt.setMotionMode(fluentqt.MotionMode.Reduced)
            self.assertEqual(emitted, [fluentqt.MotionMode.Reduced])

            policy.setMode(fluentqt.MotionMode.Disabled)
            self.assertEqual(
                emitted,
                [
                    fluentqt.MotionMode.Reduced,
                    fluentqt.MotionMode.Disabled,
                ],
            )
            self.assertFalse(policy.shouldAnimate())
            self.assertEqual(policy.resolvedDuration(250), 0)
        finally:
            policy.modeChanged.disconnect(handler)

        with self.assertRaises(ValueError):
            policy.setMode(99)
        with self.assertRaises(ValueError):
            policy.shouldAnimate(True, 99)
        with self.assertRaises(TypeError):
            policy.resolvedDuration(True)


if __name__ == "__main__":
    # Stop at the first lifetime failure before later Qt calls can hide it.
    unittest.main(failfast=True)
