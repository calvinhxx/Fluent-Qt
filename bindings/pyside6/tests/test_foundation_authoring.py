"""Behavior tests for the Python-safe Fluent foundation authoring API."""

from __future__ import annotations

import json
from pathlib import Path
import unittest

import fluentqt
import shiboken6

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QCoreApplication, QEvent
from PySide6.QtWidgets import QApplication, QLabel, QSlider, QWidget


class FoundationAuthoringTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        if not fluentqt.initialize_resources():
            raise RuntimeError("FluentQt resources could not be initialized")

    def tearDown(self):
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
    unittest.main()
