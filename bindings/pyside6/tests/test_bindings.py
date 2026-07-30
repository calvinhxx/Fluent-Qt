import gc
import json
import os
from pathlib import Path
import unittest

import fluentqt
import fluentqt._fluentqt as native
import PySide6
import shiboken6
import shiboken6_generator

fluentqt.prepare_high_dpi_application()

from fluentqt import (
    basicinput,
    foundation,
    layout,
    status_info,
    textfields,
    windowing,
)
from PySide6.QtCore import (
    QByteArray,
    QCoreApplication,
    QEvent,
    QStandardPaths,
    Qt,
    QUrl,
    qVersion,
)
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
    QApplication,
    QCheckBox,
    QLabel,
    QLineEdit,
    QPushButton,
    QRadioButton,
    QSlider,
    QWidget,
)
from shiboken6 import Shiboken


class FluentQtBindingTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        QStandardPaths.setTestModeEnabled(True)
        cls.app = QApplication.instance() or QApplication([])
        if not fluentqt.initialize_resources():
            raise RuntimeError("FluentQt resources could not be initialized")

    def test_public_types_and_build_versions(self):
        self.assertTrue(issubclass(fluentqt.Button, QPushButton))
        self.assertTrue(issubclass(fluentqt.CheckBox, QCheckBox))
        self.assertTrue(issubclass(fluentqt.HyperlinkButton, QPushButton))
        self.assertTrue(issubclass(fluentqt.RadioButton, QRadioButton))
        self.assertTrue(issubclass(fluentqt.RepeatButton, QPushButton))
        self.assertTrue(issubclass(fluentqt.Slider, QSlider))
        self.assertTrue(issubclass(fluentqt.ToggleButton, QPushButton))
        self.assertTrue(issubclass(fluentqt.ToggleSwitch, QWidget))
        self.assertTrue(issubclass(fluentqt.Label, QLabel))
        self.assertTrue(issubclass(fluentqt.LineEdit, QLineEdit))
        self.assertTrue(issubclass(fluentqt.NumberBox, QLineEdit))
        self.assertTrue(issubclass(fluentqt.PasswordBox, QLineEdit))
        self.assertTrue(issubclass(fluentqt.InfoBadge, QWidget))
        self.assertTrue(issubclass(fluentqt.ProgressBar, QWidget))
        self.assertTrue(issubclass(fluentqt.ProgressRing, QWidget))
        self.assertTrue(issubclass(fluentqt.Shimmer, QWidget))
        self.assertTrue(issubclass(fluentqt.Divider, QWidget))
        self.assertTrue(issubclass(fluentqt.Window, QWidget))
        self.assertIs(basicinput.HyperlinkButton, fluentqt.HyperlinkButton)
        self.assertIs(basicinput.ToggleSwitch, fluentqt.ToggleSwitch)
        self.assertIs(layout.Divider, fluentqt.Divider)
        self.assertIs(status_info.InfoBadge, fluentqt.InfoBadge)
        self.assertIs(status_info.ProgressRing, fluentqt.ProgressRing)
        self.assertIs(status_info.Shimmer, fluentqt.Shimmer)
        self.assertIs(textfields.NumberBox, fluentqt.NumberBox)
        self.assertIs(windowing.Window, fluentqt.Window)
        self.assertIs(foundation.Theme, fluentqt.Theme)
        self.assertIs(native.fluent.Button, fluentqt.Button)
        self.assertIs(native.fluent.windowing.Window, fluentqt.Window)

        info = fluentqt.binding_build_info()
        self.assertEqual(
            info["fluentqt_version"],
            os.environ["FLUENTQT_EXPECTED_VERSION"],
        )
        self.assertEqual(info["pyside6_version"], PySide6.__version__)
        self.assertEqual(info["shiboken6_version"], shiboken6.__version__)
        self.assertEqual(
            info["shiboken6_generator_version"],
            shiboken6_generator.__version__,
        )
        self.assertEqual(info["qt_compile_version"], qVersion())
        self.assertEqual(info["qt_runtime_version"], qVersion())

    def test_cpp_mixins_are_not_published_as_python_api(self):
        self.assertNotIn("FluentElement", dir(native.fluent))
        self.assertNotIn("QMLPlus", dir(native.fluent))
        self.assertFalse(hasattr(fluentqt.Button, "anchors"))
        self.assertFalse(hasattr(fluentqt.Button, "bind"))
        self.assertFalse(hasattr(fluentqt.Button, "setState"))
        self.assertFalse(hasattr(fluentqt.Divider, "anchors"))
        self.assertFalse(hasattr(fluentqt.Divider, "bind"))
        self.assertFalse(hasattr(fluentqt.Divider, "setState"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "anchors"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "bind"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "setState"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "elements"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "setElements"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "clearElements"))

    def test_api_manifest(self):
        manifest_path = Path(__file__).resolve().parents[1] / "api-manifest.json"
        with manifest_path.open(encoding="utf-8") as manifest_file:
            manifest = json.load(manifest_file)

        for section in ("classes", "enums", "functions"):
            with self.subTest(section=section):
                missing = [
                    name
                    for name in manifest[section]
                    if not hasattr(fluentqt, name)
                ]
                self.assertEqual(missing, [])

    def test_properties_and_signal(self):
        body_font = fluentqt.font_for_role(fluentqt.FontRole.Body)
        self.assertEqual(body_font.pixelSize(), 14)
        self.assertTrue(body_font.family())

        button = fluentqt.Button("Save")
        button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
        self.assertEqual(button.text(), "Save")
        self.assertEqual(
            button.fluentStyle(), fluentqt.Button.ButtonStyle.Accent
        )

        clicks = []
        button.clicked.connect(lambda: clicks.append(True))
        button.click()
        self.assertEqual(clicks, [True])

        line_edit = fluentqt.LineEdit()
        line_edit.setFontRole(fluentqt.FontRole.BodyStrong)
        self.assertEqual(line_edit.fontRole(), fluentqt.FontRole.BodyStrong)

    def test_phase_one_component_properties_and_signals(self):
        radio = fluentqt.RadioButton("Option")
        radio.setChecked(True)
        self.assertEqual(radio.text(), "Option")
        self.assertTrue(radio.isChecked())

        slider = fluentqt.Slider(Qt.Horizontal)
        slider.setRange(0, 100)
        slider.setValue(40)
        self.assertEqual(slider.value(), 40)

        toggle_button = fluentqt.ToggleButton("Tri-state")
        toggle_button.setThreeState(True)
        toggle_button.setCheckState(Qt.PartiallyChecked)
        self.assertTrue(toggle_button.isThreeState())
        self.assertEqual(toggle_button.checkState(), Qt.PartiallyChecked)

        toggle_values = []
        toggle_switch = fluentqt.ToggleSwitch()
        toggle_switch.toggled.connect(toggle_values.append)
        toggle_switch.setOnContent("On")
        toggle_switch.setOffContent("Off")
        toggle_switch.setIsOn(True)
        self.assertTrue(toggle_switch.isOn())
        self.assertEqual(toggle_values, [True])

        label = fluentqt.Label("Status")
        label.setFluentTypography(fluentqt.FontRole.BodyStrong)
        self.assertEqual(label.text(), "Status")
        self.assertEqual(
            label.fluentTypography(),
            fluentqt.FontRole.BodyStrong,
        )

        password = fluentqt.PasswordBox()
        password.setHeader("Password")
        password.setPassword("secret")
        password.setPasswordRevealMode(
            fluentqt.PasswordBox.PasswordRevealMode.Hidden
        )
        self.assertEqual(password.header(), "Password")
        self.assertEqual(password.password(), "secret")

        number = fluentqt.NumberBox()
        number.setRange(0.0, 10.0)
        number.setValue(4.5)
        number.setSmallChange(0.5)
        self.assertEqual(number.value(), 4.5)
        self.assertEqual(number.smallChange(), 0.5)

    def test_status_info_properties_and_signals(self):
        badge = fluentqt.InfoBadge()
        badge_values = []
        badge.valueChanged.connect(badge_values.append)
        badge.setValue(12)
        badge.setDisplayMode(
            fluentqt.InfoBadge.InfoBadgeDisplayMode.Value
        )
        badge.setStatus(
            fluentqt.InfoBadge.InfoBadgeStatus.Success
        )
        badge.setBadgeOpacity(0.75)
        self.assertEqual(badge_values, [12])
        self.assertEqual(badge.value(), 12)
        self.assertEqual(
            badge.effectiveDisplayMode(),
            fluentqt.InfoBadge.InfoBadgeDisplayMode.Value,
        )
        self.assertEqual(
            badge.status(),
            fluentqt.InfoBadge.InfoBadgeStatus.Success,
        )
        self.assertAlmostEqual(badge.badgeOpacity(), 0.75)

        bar = fluentqt.ProgressBar()
        bar.setRange(10.0, 110.0)
        bar_values = []
        bar.valueChanged.connect(bar_values.append)
        bar.setValue(60.0)
        bar.setShowPaused(True)
        bar.setRailVisible(False)
        self.assertEqual(bar_values, [60.0])
        self.assertAlmostEqual(bar.progressRatio(), 0.5)
        self.assertEqual(bar.progressText(), "60")
        self.assertTrue(bar.showPaused())
        self.assertFalse(bar.railVisible())

        ring = fluentqt.ProgressRing()
        ring.setIsIndeterminate(False)
        ring.setRange(0, 200)
        ring_values = []
        ring.valueChanged.connect(ring_values.append)
        ring.setValue(50)
        ring.setRingSize(
            fluentqt.ProgressRing.ProgressRingSize.Large
        )
        ring.setStatus(
            fluentqt.ProgressRing.ProgressRingStatus.Paused
        )
        ring.setBackgroundVisible(True)
        self.assertEqual(ring_values, [50])
        self.assertAlmostEqual(ring.progressRatio(), 0.25)
        self.assertEqual(
            ring.ringSize(),
            fluentqt.ProgressRing.ProgressRingSize.Large,
        )
        self.assertEqual(
            ring.status(),
            fluentqt.ProgressRing.ProgressRingStatus.Paused,
        )
        self.assertTrue(ring.backgroundVisible())

        shimmer = fluentqt.Shimmer()
        active_values = []
        progress_values = []
        shimmer.activeChanged.connect(active_values.append)
        shimmer.shimmerProgressChanged.connect(progress_values.append)
        shimmer.setActive(False)
        shimmer.setAnimationEnabled(False)
        shimmer.setShimmerProgress(1.25)
        shimmer.setCycleDuration(100)
        shimmer.setShimmerTemplate(
            fluentqt.Shimmer.ShimmerTemplate.ImageCard
        )
        self.assertEqual(active_values, [False])
        self.assertEqual(progress_values, [0.25])
        self.assertFalse(shimmer.isActive())
        self.assertFalse(shimmer.isAnimationEnabled())
        self.assertAlmostEqual(shimmer.shimmerProgress(), 0.25)
        self.assertEqual(shimmer.cycleDuration(), 250)
        self.assertEqual(
            shimmer.shimmerTemplate(),
            fluentqt.Shimmer.ShimmerTemplate.ImageCard,
        )

    def test_m2_leaf_component_properties_and_signals(self):
        repeat = fluentqt.RepeatButton("Hold")
        delay_changes = []
        interval_changes = []
        repeat.delayChanged.connect(lambda: delay_changes.append(repeat.delay()))
        repeat.intervalChanged.connect(
            lambda: interval_changes.append(repeat.interval())
        )
        repeat.setDelay(250)
        repeat.setInterval(40)
        self.assertTrue(repeat.autoRepeat())
        self.assertEqual(repeat.delay(), 250)
        self.assertEqual(repeat.interval(), 40)
        self.assertEqual(delay_changes, [250])
        self.assertEqual(interval_changes, [40])

        link = fluentqt.HyperlinkButton("FluentQt")
        url_changes = []
        underline_changes = []
        link.urlChanged.connect(lambda: url_changes.append(link.url()))
        link.showUnderlineChanged.connect(
            lambda: underline_changes.append(link.showUnderline())
        )
        target = QUrl("https://github.com/calvinhxx/Fluent-Qt")
        link.setUrl(target)
        link.setShowUnderline(True)
        self.assertEqual(link.url(), target)
        self.assertTrue(link.showUnderline())
        self.assertEqual(url_changes, [target])
        self.assertEqual(underline_changes, [True])

        divider = fluentqt.Divider(Qt.Vertical)
        orientation_changes = []
        inset_changes = []
        thickness_changes = []
        color_changes = []
        divider.orientationChanged.connect(orientation_changes.append)
        divider.leadingInsetChanged.connect(inset_changes.append)
        divider.thicknessChanged.connect(thickness_changes.append)
        divider.colorChanged.connect(color_changes.append)
        divider.setOrientation(Qt.Horizontal)
        divider.setLeadingInset(12)
        divider.setTrailingInset(20)
        divider.setThickness(2.0)
        color = QColor(12, 34, 56, 78)
        divider.setColor(color)
        self.assertEqual(divider.orientation(), Qt.Horizontal)
        self.assertEqual(divider.leadingInset(), 12)
        self.assertEqual(divider.trailingInset(), 20)
        self.assertEqual(divider.thickness(), 2.0)
        self.assertEqual(divider.color(), color)
        self.assertEqual(orientation_changes, [Qt.Horizontal])
        self.assertEqual(inset_changes, [12])
        self.assertEqual(thickness_changes, [2.0])
        self.assertEqual(color_changes, [color])

    def test_theme_api(self):
        previous_theme = fluentqt.current_theme()
        try:
            fluentqt.reset_theme_tokens()
            initial_revision = fluentqt.theme_revision()

            fluentqt.set_theme(fluentqt.Theme.Dark)
            self.assertEqual(fluentqt.current_theme(), fluentqt.Theme.Dark)

            fluentqt.apply_style_theme(fluentqt.StyleTheme.Material)
            self.assertEqual(
                fluentqt.current_design_language(),
                fluentqt.DesignLanguage.DesignMaterial,
            )
            self.assertGreater(fluentqt.theme_revision(), initial_revision)

            accent_revision = fluentqt.theme_revision()
            accent = QColor("#7f52ff")
            fluentqt.set_accent_color(accent)
            self.assertGreater(fluentqt.theme_revision(), accent_revision)
            self.assertEqual(fluentqt.accent_color(), accent)

            fluentqt.set_font_scale(1.25)
            self.assertAlmostEqual(fluentqt.font_scale(), 1.25)
            resolved_font = fluentqt.font_for_role(
                fluentqt.FontRole.BodyStrong
            )
            self.assertEqual(resolved_font.pixelSize(), 18)

            label = fluentqt.Label()
            label.setFluentTypography(fluentqt.FontRole.BodyStrong)
            self.assertEqual(label.font(), resolved_font)
        finally:
            fluentqt.reset_theme_tokens()
            fluentqt.set_theme(previous_theme)

    def test_python_virtual_override(self):
        class EventButton(fluentqt.Button):
            def __init__(self):
                super().__init__("Event")
                self.user_events = 0

            def event(self, event):
                if event.type() == QEvent.User:
                    self.user_events += 1
                return super().event(event)

        button = EventButton()
        QCoreApplication.sendEvent(button, QEvent(QEvent.User))
        self.assertEqual(button.user_events, 1)

    def test_native_event_uses_safe_pyside_contract(self):
        self.assertNotIn("result", fluentqt.Window.nativeEvent.__doc__)

        class NativeEventWindow(fluentqt.Window):
            def nativeEvent(self, event_type, message):
                return False, 0

        window = NativeEventWindow()
        self.assertEqual(
            window.nativeEvent(QByteArray(b"fluentqt-test"), 0),
            (False, 0),
        )

    def test_window_content_parenting_and_release(self):
        window = fluentqt.Window()
        first = QWidget()
        second = QWidget()

        window.setContentWidget(first)
        self.assertIs(window.contentWidget(), first)
        self.assertIs(first.parent(), window.contentHost())

        window.setContentWidget(second)
        self.assertIsNone(first.parent())
        self.assertTrue(Shiboken.isValid(first))
        self.assertIs(window.contentWidget(), second)
        self.assertIs(second.parent(), window.contentHost())

        window.setContentWidget(None)
        gc.collect()
        self.assertIsNone(second.parent())
        self.assertTrue(Shiboken.isValid(second))

        owned_window = fluentqt.Window()
        owned_child = QWidget()
        owned_window.setContentWidget(owned_child)
        Shiboken.delete(owned_window)
        self.assertFalse(Shiboken.isValid(owned_child))


if __name__ == "__main__":
    unittest.main()
