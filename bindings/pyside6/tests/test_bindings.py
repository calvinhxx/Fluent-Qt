import gc
import json
import os
from pathlib import Path
import unittest
import weakref

import fluentqt
import fluentqt._fluentqt as native
import PySide6
import shiboken6
import shiboken6_generator

fluentqt.prepare_high_dpi_application()

from fluentqt import (
    basicinput,
    collections,
    date_time,
    foundation,
    layout,
    scrolling,
    status_info,
    textfields,
    windowing,
)
from PySide6.QtCore import (
    QByteArray,
    QCoreApplication,
    QDate,
    QEvent,
    QEventLoop,
    QLocale,
    QMargins,
    QPoint,
    QSize,
    QStandardPaths,
    QTimer,
    Qt,
    QUrl,
    qVersion,
)
from PySide6.QtGui import QColor
from PySide6.QtTest import QTest
from PySide6.QtWidgets import (
    QApplication,
    QCheckBox,
    QFrame,
    QLabel,
    QLineEdit,
    QPushButton,
    QRadioButton,
    QScrollArea,
    QScrollBar,
    QSlider,
    QStackedWidget,
    QWidget,
)
from shiboken6 import Shiboken


def wait_for_events(duration_ms):
    loop = QEventLoop()
    QTimer.singleShot(duration_ms, loop.quit)
    loop.exec()


class FluentQtBindingTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        QStandardPaths.setTestModeEnabled(True)
        cls.app = QApplication.instance() or QApplication([])
        if not fluentqt.initialize_resources():
            raise RuntimeError("FluentQt resources could not be initialized")

    def test_public_types_and_build_versions(self):
        self.assertTrue(issubclass(fluentqt.Accordion, QWidget))
        self.assertTrue(issubclass(fluentqt.Avatar, QWidget))
        self.assertTrue(issubclass(fluentqt.Button, QPushButton))
        self.assertTrue(issubclass(fluentqt.CalendarView, QWidget))
        self.assertTrue(issubclass(fluentqt.CheckBox, QCheckBox))
        self.assertTrue(issubclass(fluentqt.ColorPicker, QWidget))
        self.assertTrue(
            issubclass(fluentqt.CompoundButton, fluentqt.Button)
        )
        self.assertTrue(issubclass(fluentqt.HyperlinkButton, QPushButton))
        self.assertTrue(issubclass(fluentqt.RadioButton, QRadioButton))
        self.assertTrue(issubclass(fluentqt.RatingControl, QWidget))
        self.assertTrue(issubclass(fluentqt.RepeatButton, QPushButton))
        self.assertTrue(issubclass(fluentqt.Slider, QSlider))
        self.assertTrue(issubclass(fluentqt.ToggleButton, QPushButton))
        self.assertTrue(issubclass(fluentqt.ToggleSwitch, QWidget))
        self.assertTrue(issubclass(fluentqt.Label, QLabel))
        self.assertTrue(issubclass(fluentqt.LineEdit, QLineEdit))
        self.assertTrue(issubclass(fluentqt.NumberBox, QLineEdit))
        self.assertTrue(issubclass(fluentqt.PasswordBox, QLineEdit))
        self.assertTrue(issubclass(fluentqt.TextEdit, QWidget))
        self.assertTrue(issubclass(fluentqt.InfoBadge, QWidget))
        self.assertTrue(issubclass(fluentqt.InfoBar, QWidget))
        self.assertTrue(issubclass(fluentqt.ProgressBar, QWidget))
        self.assertTrue(issubclass(fluentqt.ProgressRing, QWidget))
        self.assertTrue(issubclass(fluentqt.Shimmer, QWidget))
        self.assertTrue(issubclass(fluentqt.Card, QFrame))
        self.assertTrue(issubclass(fluentqt.Divider, QWidget))
        self.assertTrue(
            issubclass(fluentqt.Expander, fluentqt.Card)
        )
        self.assertTrue(issubclass(fluentqt.FontIcon, QWidget))
        self.assertTrue(
            issubclass(fluentqt.AnnotatedScrollBar, QWidget)
        )
        self.assertTrue(issubclass(fluentqt.PipsPager, QWidget))
        self.assertTrue(issubclass(fluentqt.ScrollBar, QScrollBar))
        self.assertTrue(issubclass(fluentqt.ScrollView, QScrollArea))
        self.assertTrue(issubclass(fluentqt.StackView, QStackedWidget))
        self.assertTrue(issubclass(fluentqt.Window, QWidget))
        self.assertIs(date_time.CalendarView, fluentqt.CalendarView)
        self.assertIs(basicinput.ColorPicker, fluentqt.ColorPicker)
        self.assertIs(basicinput.CompoundButton, fluentqt.CompoundButton)
        self.assertIs(basicinput.HyperlinkButton, fluentqt.HyperlinkButton)
        self.assertIs(basicinput.RatingControl, fluentqt.RatingControl)
        self.assertIs(basicinput.ToggleSwitch, fluentqt.ToggleSwitch)
        self.assertIs(layout.Card, fluentqt.Card)
        self.assertIs(layout.Divider, fluentqt.Divider)
        self.assertIs(layout.Expander, fluentqt.Expander)
        self.assertIs(layout.Accordion, fluentqt.Accordion)
        self.assertIs(
            scrolling.AnnotatedScrollBar,
            fluentqt.AnnotatedScrollBar,
        )
        self.assertIs(
            scrolling.AnnotatedScrollBarLabel,
            fluentqt.AnnotatedScrollBarLabel,
        )
        self.assertIs(scrolling.PipsPager, fluentqt.PipsPager)
        self.assertIs(scrolling.ScrollBar, fluentqt.ScrollBar)
        self.assertIs(scrolling.ScrollView, fluentqt.ScrollView)
        self.assertIs(collections.StackView, fluentqt.StackView)
        self.assertIs(
            collections.WidgetOwnership,
            fluentqt.WidgetOwnership,
        )
        self.assertIs(
            scrolling.WidgetOwnership,
            fluentqt.WidgetOwnership,
        )
        self.assertIs(status_info.Avatar, fluentqt.Avatar)
        self.assertIs(status_info.InfoBadge, fluentqt.InfoBadge)
        self.assertIs(status_info.InfoBar, fluentqt.InfoBar)
        self.assertIs(status_info.ProgressRing, fluentqt.ProgressRing)
        self.assertIs(status_info.Shimmer, fluentqt.Shimmer)
        self.assertIs(textfields.NumberBox, fluentqt.NumberBox)
        self.assertIs(textfields.TextEdit, fluentqt.TextEdit)
        self.assertIs(windowing.Window, fluentqt.Window)
        self.assertIs(foundation.FontIcon, fluentqt.FontIcon)
        self.assertIs(foundation.Theme, fluentqt.Theme)
        self.assertIs(native.fluent.Avatar, fluentqt.Avatar)
        self.assertIs(native.fluent.Button, fluentqt.Button)
        self.assertIs(native.fluent.CalendarView, fluentqt.CalendarView)
        self.assertIs(native.fluent.ColorPicker, fluentqt.ColorPicker)
        self.assertIs(native.fluent.CompoundButton, fluentqt.CompoundButton)
        self.assertIs(native.fluent.FontIcon, fluentqt.FontIcon)
        self.assertIs(
            native.fluent.RatingControl,
            fluentqt.RatingControl,
        )
        self.assertIs(native.fluent.PipsPager, fluentqt.PipsPager)
        self.assertIs(
            native.fluent.AnnotatedScrollBar,
            fluentqt.AnnotatedScrollBar,
        )
        self.assertIs(
            native.fluent.AnnotatedScrollBarLabel,
            fluentqt.AnnotatedScrollBarLabel,
        )
        self.assertIs(native.fluent.ScrollBar, fluentqt.ScrollBar)
        self.assertIs(native.fluent.TextEdit, fluentqt.TextEdit)
        self.assertTrue(
            issubclass(fluentqt.InfoBar, native.fluent.InfoBar)
        )
        self.assertIsNot(native.fluent.InfoBar, fluentqt.InfoBar)
        self.assertNotIn("setActionWidget", native.fluent.InfoBar.__dict__)
        self.assertTrue(
            hasattr(native.fluent.InfoBar, "_setActionWidget")
        )
        self.assertTrue(
            issubclass(fluentqt.ScrollView, native.fluent.ScrollView)
        )
        self.assertIsNot(native.fluent.ScrollView, fluentqt.ScrollView)
        self.assertNotIn("setWidget", native.fluent.ScrollView.__dict__)
        self.assertNotIn("takeWidget", native.fluent.ScrollView.__dict__)
        self.assertTrue(
            hasattr(
                native.fluent.ScrollView,
                "_setContentWidgetWithOwnership",
            )
        )
        self.assertTrue(
            hasattr(fluentqt.ScrollView, "setOwnedContentWidget")
        )
        self.assertTrue(
            hasattr(fluentqt.ScrollView, "setBorrowedContentWidget")
        )
        self.assertTrue(
            hasattr(fluentqt.ScrollView, "setReparentedContentWidget")
        )
        self.assertTrue(
            issubclass(fluentqt.Expander, native.fluent.Expander)
        )
        self.assertIsNot(native.fluent.Expander, fluentqt.Expander)
        self.assertTrue(
            hasattr(
                native.fluent.Expander,
                "_setContentWidgetWithOwnership",
            )
        )
        self.assertFalse(hasattr(native.fluent.Expander, "headerButton"))
        self.assertTrue(
            issubclass(fluentqt.Accordion, native.fluent.Accordion)
        )
        self.assertIsNot(native.fluent.Accordion, fluentqt.Accordion)
        self.assertNotIn("addItem", native.fluent.Accordion.__dict__)
        self.assertNotIn("insertItem", native.fluent.Accordion.__dict__)
        self.assertTrue(
            hasattr(native.fluent.Accordion, "_addItemWithOwnership")
        )
        self.assertTrue(
            hasattr(native.fluent.Accordion, "_insertItemWithOwnership")
        )
        self.assertTrue(hasattr(fluentqt.Accordion, "addOwnedItem"))
        self.assertTrue(hasattr(fluentqt.Accordion, "addBorrowedItem"))
        self.assertTrue(hasattr(fluentqt.Accordion, "addReparentedItem"))
        self.assertTrue(
            issubclass(fluentqt.StackView, native.fluent.StackView)
        )
        self.assertIsNot(native.fluent.StackView, fluentqt.StackView)
        self.assertNotIn("push", native.fluent.StackView.__dict__)
        self.assertNotIn("replace", native.fluent.StackView.__dict__)
        self.assertNotIn("setInitialItem", native.fluent.StackView.__dict__)
        self.assertNotIn(
            "setCurrentWidget",
            native.fluent.StackView.__dict__,
        )
        self.assertNotIn(
            "defaultItemOwnership",
            native.fluent.StackView.__dict__,
        )
        self.assertTrue(
            hasattr(native.fluent.StackView, "_pushItemWithOwnership")
        )
        self.assertTrue(
            hasattr(native.fluent.StackView, "_pushItemsWithOwnership")
        )
        self.assertTrue(
            hasattr(
                native.fluent.StackView,
                "_replaceCurrentWithOwnership",
            )
        )
        self.assertTrue(
            hasattr(
                native.fluent.StackView,
                "_replaceAtWithOwnership",
            )
        )
        self.assertTrue(
            hasattr(
                native.fluent.StackView,
                "_setInitialItemWithOwnership",
            )
        )
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
        self.assertFalse(hasattr(fluentqt.CompoundButton, "anchors"))
        self.assertFalse(hasattr(fluentqt.CompoundButton, "bind"))
        self.assertFalse(hasattr(fluentqt.CompoundButton, "setState"))
        self.assertFalse(hasattr(fluentqt.CalendarView, "anchors"))
        self.assertFalse(hasattr(fluentqt.CalendarView, "bind"))
        self.assertFalse(hasattr(fluentqt.CalendarView, "setState"))
        self.assertFalse(hasattr(fluentqt.ColorPicker, "anchors"))
        self.assertFalse(hasattr(fluentqt.ColorPicker, "bind"))
        self.assertFalse(hasattr(fluentqt.ColorPicker, "setState"))
        self.assertFalse(hasattr(fluentqt.Divider, "anchors"))
        self.assertFalse(hasattr(fluentqt.Divider, "bind"))
        self.assertFalse(hasattr(fluentqt.Divider, "setState"))
        self.assertFalse(hasattr(fluentqt.Card, "anchors"))
        self.assertFalse(hasattr(fluentqt.Card, "bind"))
        self.assertFalse(hasattr(fluentqt.Card, "setState"))
        self.assertFalse(hasattr(fluentqt.Expander, "anchors"))
        self.assertFalse(hasattr(fluentqt.Expander, "bind"))
        self.assertFalse(hasattr(fluentqt.Expander, "setState"))
        self.assertFalse(hasattr(fluentqt.Accordion, "anchors"))
        self.assertFalse(hasattr(fluentqt.Accordion, "bind"))
        self.assertFalse(hasattr(fluentqt.Accordion, "setState"))
        self.assertFalse(hasattr(fluentqt.StackView, "anchors"))
        self.assertFalse(hasattr(fluentqt.StackView, "bind"))
        self.assertFalse(hasattr(fluentqt.StackView, "setState"))
        self.assertFalse(hasattr(fluentqt.FontIcon, "anchors"))
        self.assertFalse(hasattr(fluentqt.FontIcon, "bind"))
        self.assertFalse(hasattr(fluentqt.FontIcon, "setState"))
        self.assertFalse(hasattr(fluentqt.Avatar, "anchors"))
        self.assertFalse(hasattr(fluentqt.Avatar, "bind"))
        self.assertFalse(hasattr(fluentqt.Avatar, "setState"))
        self.assertFalse(hasattr(fluentqt.RatingControl, "anchors"))
        self.assertFalse(hasattr(fluentqt.RatingControl, "bind"))
        self.assertFalse(hasattr(fluentqt.RatingControl, "setState"))
        self.assertFalse(hasattr(fluentqt.ScrollBar, "anchors"))
        self.assertFalse(hasattr(fluentqt.ScrollBar, "bind"))
        self.assertFalse(hasattr(fluentqt.ScrollBar, "setState"))
        self.assertFalse(hasattr(fluentqt.PipsPager, "anchors"))
        self.assertFalse(hasattr(fluentqt.PipsPager, "bind"))
        self.assertFalse(hasattr(fluentqt.PipsPager, "setState"))
        self.assertFalse(hasattr(fluentqt.PipsPager, "HitKind"))
        self.assertFalse(
            hasattr(fluentqt.PipsPager, "selectedVisualOffset")
        )
        self.assertFalse(
            hasattr(fluentqt.PipsPager, "visibleWindowOffset")
        )
        self.assertFalse(hasattr(fluentqt.Shimmer, "anchors"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "bind"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "setState"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "elements"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "setElements"))
        self.assertFalse(hasattr(fluentqt.Shimmer, "clearElements"))
        self.assertFalse(hasattr(fluentqt.InfoBar, "anchors"))
        self.assertFalse(hasattr(fluentqt.InfoBar, "bind"))
        self.assertFalse(hasattr(fluentqt.InfoBar, "setState"))
        self.assertFalse(hasattr(fluentqt.TextEdit, "anchors"))
        self.assertFalse(hasattr(fluentqt.TextEdit, "bind"))
        self.assertFalse(hasattr(fluentqt.TextEdit, "setState"))
        self.assertFalse(hasattr(fluentqt.ScrollView, "anchors"))
        self.assertFalse(hasattr(fluentqt.ScrollView, "bind"))
        self.assertFalse(hasattr(fluentqt.ScrollView, "setState"))
        self.assertFalse(
            hasattr(fluentqt.AnnotatedScrollBar, "anchors")
        )
        self.assertFalse(
            hasattr(fluentqt.AnnotatedScrollBar, "bind")
        )
        self.assertFalse(
            hasattr(fluentqt.AnnotatedScrollBar, "setState")
        )

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

        for class_name, method_names in manifest["methods"].items():
            with self.subTest(class_name=class_name):
                bound_class = getattr(fluentqt, class_name)
                missing = [
                    name
                    for name in method_names
                    if not hasattr(bound_class, name)
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

    def test_font_icon_properties_and_signals(self):
        empty_icon = fluentqt.FontIcon()
        self.assertEqual(empty_icon.glyph(), "")

        icon = fluentqt.FontIcon("ic_fluent_settings_20_regular")
        self.assertEqual(
            icon.glyph(),
            "ic_fluent_settings_20_regular",
        )
        self.assertGreater(icon.iconSize(), 0)
        self.assertFalse(icon.color().isValid())
        self.assertAlmostEqual(icon.rotation(), 0.0)
        self.assertEqual(icon.sizeHint().width(), icon.iconSize())
        self.assertEqual(icon.sizeHint().height(), icon.iconSize())
        self.assertTrue(
            icon.testAttribute(Qt.WA_TransparentForMouseEvents)
        )

        glyph_changes = []
        size_changes = []
        color_changes = []
        rotation_changes = []
        icon.glyphChanged.connect(glyph_changes.append)
        icon.iconSizeChanged.connect(size_changes.append)
        icon.colorChanged.connect(color_changes.append)
        icon.rotationChanged.connect(rotation_changes.append)

        icon.setGlyph("ic_fluent_save_20_regular")
        icon.setGlyph("ic_fluent_save_20_regular")
        icon.setIconSize(0)
        icon.setIconSize(1)
        accent = QColor("#7f52ff")
        icon.setColor(accent)
        icon.setColor(accent)
        icon.setRotation(90.0)
        icon.setRotation(90.0)

        self.assertEqual(
            glyph_changes,
            ["ic_fluent_save_20_regular"],
        )
        self.assertEqual(size_changes, [1])
        self.assertEqual(color_changes, [accent])
        self.assertEqual(rotation_changes, [90.0])
        self.assertEqual(icon.glyph(), "ic_fluent_save_20_regular")
        self.assertEqual(icon.iconSize(), 1)
        self.assertEqual(icon.color(), accent)
        self.assertAlmostEqual(icon.rotation(), 90.0)
        self.assertEqual(icon.minimumSizeHint(), icon.sizeHint())

    def test_color_picker_public_properties_and_signals(self):
        picker = fluentqt.ColorPicker()
        self.assertEqual(picker.color(), QColor(255, 255, 255, 255))
        self.assertTrue(picker.alphaEnabled())

        for internal_name in (
            "hue",
            "saturation",
            "value",
            "setHueFromBar",
            "setSVFromSpectrum",
            "setValueFromSlider",
            "setAlphaFromSlider",
        ):
            with self.subTest(internal_name=internal_name):
                self.assertFalse(
                    hasattr(fluentqt.ColorPicker, internal_name)
                )

        color_changes = []
        alpha_changes = []
        picker.colorChanged.connect(color_changes.append)
        picker.alphaEnabledChanged.connect(alpha_changes.append)
        selected = QColor(0, 120, 212, 180)
        picker.setColor(selected)
        picker.setColor(selected)
        picker.setAlphaEnabled(False)
        picker.setAlphaEnabled(False)

        self.assertEqual(picker.color(), selected)
        self.assertFalse(picker.alphaEnabled())
        self.assertEqual(color_changes, [selected])
        self.assertEqual(alpha_changes, [False])

    def test_calendar_view_date_properties_signals_and_queries(self):
        calendar = fluentqt.CalendarView()
        self.assertFalse(calendar.selectedDate().isValid())
        self.assertTrue(calendar.visibleMonth().isValid())
        self.assertFalse(calendar.minDate().isValid())
        self.assertFalse(calendar.maxDate().isValid())
        self.assertTrue(calendar.isFrameVisible())
        self.assertEqual(
            calendar.contentLevel(),
            fluentqt.CalendarView.CalendarContentLevel.Day,
        )

        minimum = QDate(2026, 5, 10)
        maximum = QDate(2026, 5, 20)
        min_changes = []
        max_changes = []
        selected_changes = []
        frame_changes = []
        level_changes = []
        locale_changes = []
        first_day_changes = []
        calendar.minDateChanged.connect(min_changes.append)
        calendar.maxDateChanged.connect(max_changes.append)
        calendar.selectedDateChanged.connect(selected_changes.append)
        calendar.frameVisibleChanged.connect(frame_changes.append)
        calendar.contentLevelChanged.connect(level_changes.append)
        calendar.localeChanged.connect(locale_changes.append)

        calendar.setDateRange(minimum, maximum)
        calendar.setDateRange(minimum, maximum)
        calendar.setSelectedDate(QDate(2026, 5, 1))
        calendar.setSelectedDate(QDate(2026, 5, 1))
        calendar.setFrameVisible(False)
        calendar.setFrameVisible(False)
        month_level = fluentqt.CalendarView.CalendarContentLevel.Month
        calendar.setContentLevel(month_level)
        calendar.setContentLevel(month_level)
        us_locale = QLocale(QLocale.English, QLocale.UnitedStates)
        german_locale = QLocale(QLocale.German, QLocale.Germany)
        next_locale = (
            german_locale if calendar.locale() == us_locale else us_locale
        )
        calendar.setLocale(next_locale)
        calendar.setLocale(next_locale)
        calendar.firstDayOfWeekChanged.connect(first_day_changes.append)
        next_first_day = (
            Qt.Thursday
            if calendar.firstDayOfWeek() != Qt.Thursday
            else Qt.Sunday
        )
        calendar.setFirstDayOfWeek(next_first_day)
        calendar.setFirstDayOfWeek(next_first_day)

        self.assertEqual(calendar.minDate(), minimum)
        self.assertEqual(calendar.maxDate(), maximum)
        self.assertEqual(calendar.selectedDate(), minimum)
        self.assertEqual(min_changes, [minimum])
        self.assertEqual(max_changes, [maximum])
        self.assertEqual(selected_changes, [minimum])
        self.assertEqual(frame_changes, [False])
        self.assertEqual(level_changes, [month_level])
        self.assertEqual(locale_changes, [next_locale])
        self.assertEqual(first_day_changes, [next_first_day])

        calendar.setContentLevel(
            fluentqt.CalendarView.CalendarContentLevel.Day
        )
        calendar.resize(calendar.sizeHint())
        QCoreApplication.processEvents()
        selected_rect = calendar.dateCellRect(minimum)
        self.assertTrue(selected_rect.isValid())
        self.assertEqual(calendar.dateAt(selected_rect.center()), minimum)
        self.assertTrue(calendar.isDateSelectable(minimum))
        self.assertFalse(calendar.isDateSelectable(minimum.addDays(-1)))

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
        compound = fluentqt.CompoundButton(
            "Install update",
            "Download and restart the app",
        )
        secondary_changes = []
        clicks = []
        compound.secondaryTextChanged.connect(secondary_changes.append)
        compound.clicked.connect(lambda: clicks.append(True))
        self.assertEqual(compound.text(), "Install update")
        self.assertEqual(
            compound.secondaryText(),
            "Download and restart the app",
        )
        self.assertEqual(
            compound.accessibleDescription(),
            "Download and restart the app",
        )
        self.assertEqual(
            compound.fluentSize(),
            fluentqt.Button.ButtonSize.Large,
        )
        two_line_size = compound.sizeHint()
        compound.setSecondaryText("Restart after downloading")
        compound.setSecondaryText("Restart after downloading")
        compound.click()
        self.assertEqual(
            secondary_changes,
            ["Restart after downloading"],
        )
        self.assertEqual(clicks, [True])
        self.assertEqual(
            compound.accessibleDescription(),
            "Restart after downloading",
        )
        compound.setSecondaryText("")
        self.assertLessEqual(
            compound.sizeHint().height(),
            two_line_size.height(),
        )

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

    def test_text_edit_properties_signals_and_multiline_contract(self):
        edit = fluentqt.TextEdit()
        text_changes = []
        margin_changes = []
        font_changes = []
        layout_changes = []
        chaining_changes = []
        edit.textChanged.connect(lambda: text_changes.append(edit.toPlainText()))
        edit.contentMarginsChanged.connect(
            lambda: margin_changes.append(edit.contentMargins())
        )
        edit.fontRoleChanged.connect(
            lambda: font_changes.append(edit.fontRole())
        )
        edit.layoutMetricsChanged.connect(
            lambda: layout_changes.append(
                (
                    edit.lineHeight(),
                    edit.minVisibleLines(),
                    edit.maxVisibleLines(),
                )
            )
        )
        edit.scrollChainingEnabledChanged.connect(
            lambda: chaining_changes.append(edit.isScrollChainingEnabled())
        )

        margins = QMargins(14, 6, 14, 6)
        edit.setContentMargins(margins)
        edit.setFontRole(fluentqt.FontRole.Subtitle)
        edit.setLineHeight(28)
        edit.setMinVisibleLines(2)
        edit.setMaxVisibleLines(3)
        edit.setScrollChainingEnabled(True)
        edit.setPlaceholderText("Write a note")
        edit.setPlainText("First line\nSecond line")
        edit.setReadOnly(True)

        self.assertEqual(edit.contentMargins(), margins)
        self.assertEqual(edit.fontRole(), fluentqt.FontRole.Subtitle)
        self.assertEqual(edit.lineHeight(), 28)
        self.assertEqual(edit.minVisibleLines(), 2)
        self.assertEqual(edit.maxVisibleLines(), 3)
        self.assertTrue(edit.isScrollChainingEnabled())
        self.assertEqual(edit.placeholderText(), "Write a note")
        self.assertEqual(edit.toPlainText(), "First line\nSecond line")
        self.assertTrue(edit.isReadOnly())
        self.assertIsInstance(edit.verticalScrollBar(), fluentqt.ScrollBar)
        self.assertIs(edit.verticalScrollBar().parent(), edit)
        self.assertEqual(margin_changes, [margins])
        self.assertEqual(font_changes, [fluentqt.FontRole.Subtitle])
        self.assertEqual(len(layout_changes), 3)
        self.assertEqual(chaining_changes, [True])
        self.assertTrue(text_changes)
        self.assertEqual(text_changes[-1], "First line\nSecond line")

        notification_count = len(text_changes)
        edit.clear()
        self.assertEqual(edit.toPlainText(), "")
        self.assertGreater(len(text_changes), notification_count)
        self.assertEqual(text_changes[-1], "")

    def test_card_and_expander_properties_and_signals(self):
        card = fluentqt.Card()
        appearance_changes = []
        border_changes = []
        card.appearanceChanged.connect(appearance_changes.append)
        card.borderVisibleChanged.connect(border_changes.append)

        self.assertEqual(
            card.appearance(),
            fluentqt.Card.Appearance.Layer,
        )
        self.assertTrue(card.isBorderVisible())
        card.setAppearance(fluentqt.Card.Appearance.Canvas)
        card.setBorderVisible(False)
        self.assertEqual(
            appearance_changes,
            [fluentqt.Card.Appearance.Canvas],
        )
        self.assertEqual(border_changes, [False])

        expander = fluentqt.Expander()
        header_changes = []
        expanded_changes = []
        animation_changes = []
        expander.headerTextChanged.connect(header_changes.append)
        expander.expandedChanged.connect(expanded_changes.append)
        expander.animationEnabledChanged.connect(animation_changes.append)

        self.assertEqual(expander.headerText(), "")
        self.assertFalse(expander.isExpanded())
        self.assertTrue(expander.isAnimationEnabled())
        self.assertEqual(
            expander.contentOwnership(),
            fluentqt.WidgetOwnership.Borrowed,
        )

        expander.setHeaderText("Details")
        expander.setAnimationEnabled(False)
        expander.setExpanded(True)
        self.assertEqual(header_changes, ["Details"])
        self.assertEqual(animation_changes, [False])
        self.assertEqual(expanded_changes, [True])
        self.assertTrue(expander.isExpanded())

    def test_avatar_properties_signals_and_composed_badge(self):
        avatar = fluentqt.Avatar()
        name_changes = []
        initials_changes = []
        shape_changes = []
        size_changes = []
        presence_changes = []
        background_changes = []
        foreground_changes = []
        avatar.nameChanged.connect(name_changes.append)
        avatar.initialsChanged.connect(initials_changes.append)
        avatar.shapeChanged.connect(shape_changes.append)
        avatar.avatarSizeChanged.connect(size_changes.append)
        avatar.presenceChanged.connect(presence_changes.append)
        avatar.backgroundColorChanged.connect(background_changes.append)
        avatar.foregroundColorChanged.connect(foreground_changes.append)

        avatar.setName("Ada Lovelace")
        avatar.setInitials(" AL ")
        avatar.setShape(fluentqt.Avatar.AvatarShape.Square)
        avatar.setAvatarSize(fluentqt.Avatar.AvatarSize.ExtraLarge)
        avatar.setPresence(fluentqt.Avatar.PresenceStatus.Available)
        background = QColor("#123456")
        foreground = QColor("#f0f1f2")
        avatar.setBackgroundColor(background)
        avatar.setForegroundColor(foreground)

        self.assertEqual(name_changes, ["Ada Lovelace"])
        self.assertEqual(initials_changes, ["AL"])
        self.assertEqual(
            shape_changes,
            [fluentqt.Avatar.AvatarShape.Square],
        )
        self.assertEqual(
            size_changes,
            [fluentqt.Avatar.AvatarSize.ExtraLarge],
        )
        self.assertEqual(
            presence_changes,
            [fluentqt.Avatar.PresenceStatus.Available],
        )
        self.assertEqual(background_changes, [background])
        self.assertEqual(foreground_changes, [foreground])
        self.assertEqual(avatar.effectiveInitials(), "AL")
        self.assertEqual(avatar.size(), avatar.sizeHint())
        self.assertEqual(avatar.size().width(), 56)

        badge = avatar.presenceBadge()
        self.assertIsInstance(badge, fluentqt.InfoBadge)
        self.assertIs(badge.parent(), avatar)
        self.assertTrue(Shiboken.isValid(badge))

    def test_rating_control_properties_and_signals(self):
        rating = fluentqt.RatingControl()
        value_changes = []
        placeholder_changes = []
        caption_changes = []
        clear_changes = []
        read_only_changes = []
        max_changes = []
        size_changes = []
        font_changes = []
        caption_font_changes = []
        rating.valueChanged.connect(value_changes.append)
        rating.placeholderValueChanged.connect(placeholder_changes.append)
        rating.captionChanged.connect(caption_changes.append)
        rating.isClearEnabledChanged.connect(clear_changes.append)
        rating.isReadOnlyChanged.connect(read_only_changes.append)
        rating.maxRatingChanged.connect(max_changes.append)
        rating.starSizeChanged.connect(size_changes.append)
        rating.fontRoleChanged.connect(
            lambda: font_changes.append(rating.fontRole())
        )
        rating.captionFontRoleChanged.connect(
            lambda: caption_font_changes.append(
                rating.captionFontRole()
            )
        )

        rating.setValue(3.5)
        rating.setPlaceholderValue(2.5)
        rating.setCaption("312 ratings")
        rating.setIsClearEnabled(False)
        rating.setIsReadOnly(True)
        rating.setMaxRating(7)
        rating.setStarSize(24)
        rating.setFontRole(fluentqt.FontRole.BodyStrong)
        rating.setCaptionFontRole(fluentqt.FontRole.BodyLarge)

        self.assertEqual(value_changes, [3.5])
        self.assertEqual(placeholder_changes, [2.5])
        self.assertEqual(caption_changes, ["312 ratings"])
        self.assertEqual(clear_changes, [False])
        self.assertEqual(read_only_changes, [True])
        self.assertEqual(max_changes, [7])
        self.assertEqual(size_changes, [24])
        self.assertEqual(font_changes, [fluentqt.FontRole.BodyStrong])
        self.assertEqual(
            caption_font_changes,
            [fluentqt.FontRole.BodyLarge],
        )

        rating.setValue(99.0)
        self.assertEqual(rating.value(), 7.0)

    def test_scroll_bar_properties_and_inherited_range_signal(self):
        scroll_bar = fluentqt.ScrollBar(Qt.Horizontal)
        thickness_changes = []
        value_changes = []
        scroll_bar.thicknessChanged.connect(
            lambda: thickness_changes.append(scroll_bar.thickness())
        )
        scroll_bar.valueChanged.connect(value_changes.append)

        self.assertEqual(scroll_bar.orientation(), Qt.Horizontal)
        self.assertEqual(scroll_bar.thickness(), 7)
        self.assertEqual(scroll_bar.opacity(), 0.0)

        scroll_bar.setThickness(11)
        scroll_bar.setOpacity(2.0)
        scroll_bar.setRange(0, 100)
        scroll_bar.setValue(42)

        self.assertEqual(thickness_changes, [11])
        self.assertEqual(scroll_bar.opacity(), 1.0)
        self.assertEqual(value_changes, [42])
        self.assertEqual(scroll_bar.sizeHint().height(), 11)

    def test_annotated_scroll_bar_labels_properties_and_static_detail(self):
        label_type = fluentqt.AnnotatedScrollBarLabel
        first = label_type("Start", 0, "Start detail")
        copied = label_type(first)
        self.assertEqual(copied, first)
        self.assertFalse(copied != first)
        with self.assertRaises(TypeError):
            hash(first)
        self.assertEqual(first.text, "Start")
        self.assertEqual(first.offset, 0)
        self.assertEqual(first.detailText, "Start detail")

        copied.text = "Beginning"
        copied.offset = 10
        copied.detailText = "Beginning detail"
        self.assertNotEqual(copied, first)

        bar = fluentqt.AnnotatedScrollBar()
        range_changes = []
        value_changes = []
        page_step_changes = []
        label_changes = []
        metric_changes = []
        detail_requests = []
        bar.rangeChanged.connect(
            lambda minimum, maximum: range_changes.append(
                (minimum, maximum)
            )
        )
        bar.valueChanged.connect(value_changes.append)
        bar.pageStepChanged.connect(page_step_changes.append)
        bar.labelsChanged.connect(lambda: label_changes.append(True))
        bar.layoutMetricsChanged.connect(
            lambda: metric_changes.append(True)
        )
        bar.detailLabelRequested.connect(detail_requests.append)

        bar.setRange(0, 1000)
        bar.setValue(250)
        bar.setPageStep(100)
        bar.setPreferredSize(QSize(144, 420))
        bar.setMinimumBarSize(QSize(72, 160))
        bar.setMinimumLabelSpacing(12)
        bar.setLabels(
            [
                label_type("Middle", 500, "Middle detail"),
                first,
                label_type("End", 1000, "End detail"),
            ]
        )

        self.assertEqual(range_changes, [(0, 1000)])
        self.assertEqual(value_changes, [250])
        self.assertEqual(page_step_changes, [100])
        self.assertEqual(label_changes, [True])
        self.assertEqual(len(metric_changes), 3)
        self.assertEqual(bar.preferredSize(), QSize(144, 420))
        self.assertEqual(bar.minimumBarSize(), QSize(72, 160))
        self.assertEqual(bar.minimumLabelSpacing(), 12)
        self.assertEqual(
            [label.text for label in bar.labels()],
            ["Middle", "Start", "End"],
        )

        bar.resize(144, 240)
        visible = bar.visibleLabels()
        self.assertEqual(
            [label.text for label in visible],
            ["Start", "Middle", "End"],
        )
        self.assertEqual(bar.visibleLabelCount(), 3)
        self.assertFalse(
            hasattr(bar, "setDetailLabelProvider")
        )
        self.assertFalse(
            hasattr(bar, "clearDetailLabelProvider")
        )
        self.assertFalse(
            hasattr(bar, "hasDetailLabelProvider")
        )

        bar.show()
        self.app.processEvents()
        QTest.mouseMove(bar, QPoint(12, 19))
        self.app.processEvents()
        self.assertTrue(bar.isDetailLabelVisible())
        self.assertEqual(bar.detailLabelText(), "Start detail")
        self.assertTrue(detail_requests)
        bar.hide()

        bar.clearLabels()
        self.assertEqual(label_changes, [True, True])
        self.assertEqual(bar.labels(), [])

    def test_annotated_scroll_bar_borrowed_scroll_view_link(self):
        class PythonScrollView(fluentqt.ScrollView):
            def __init__(self):
                super().__init__()
                self.marker = "python-scroll-view"

        view = PythonScrollView()
        content = QWidget()
        content.setFixedSize(420, 480)
        view.resize(180, 120)
        view.setContentWidget(content)
        view.show()
        self.app.processEvents()

        bar = fluentqt.AnnotatedScrollBar()
        bar.connectToScrollView(view)
        self.assertIs(bar.connectedScrollView(), view)
        self.assertEqual(
            bar.connectedScrollView().marker,
            "python-scroll-view",
        )
        self.assertIsNone(view.parent())
        self.assertEqual(
            (bar.minimum(), bar.maximum(), bar.pageStep()),
            (
                view.verticalScrollBar().minimum(),
                view.verticalScrollBar().maximum(),
                view.verticalScrollBar().pageStep(),
            ),
        )

        view.scrollTo(40, 80, False)
        self.app.processEvents()
        self.assertEqual(bar.value(), view.verticalOffset())
        bar.scrollRequested.emit(160)
        self.app.processEvents()
        self.assertEqual(view.verticalOffset(), 160)
        self.assertEqual(bar.value(), 160)

        previous = bar.value()
        bar.disconnectScrollView()
        self.assertIsNone(bar.connectedScrollView())
        view.scrollTo(40, 0, False)
        self.app.processEvents()
        self.assertEqual(bar.value(), previous)

        bar.connectToScrollView(view)
        view_ref = weakref.ref(view)
        del view
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertIsNone(bar.connectedScrollView())

    def test_pips_pager_properties_signals_and_navigation(self):
        pager = fluentqt.PipsPager()
        visibility = fluentqt.PipsPager.PipsPagerButtonVisibility
        page_count_changes = []
        selected_page_changes = []
        selected_index_changes = []
        visible_count_changes = []
        orientation_changes = []
        previous_visibility_changes = []
        next_visibility_changes = []
        pager.numberOfPagesChanged.connect(page_count_changes.append)
        pager.selectedPageIndexChanged.connect(
            selected_page_changes.append
        )
        pager.selectedIndexChanged.connect(
            lambda old, new: selected_index_changes.append((old, new))
        )
        pager.maxVisiblePipsChanged.connect(visible_count_changes.append)
        pager.orientationChanged.connect(orientation_changes.append)
        pager.previousButtonVisibilityChanged.connect(
            previous_visibility_changes.append
        )
        pager.nextButtonVisibilityChanged.connect(
            next_visibility_changes.append
        )

        pager.setSelectionAnimationEnabled(False)
        pager.setNumberOfPages(7)
        pager.setMaxVisiblePips(3)
        pager.setPreviousButtonVisibility(
            visibility.Visible
        )
        pager.setNextButtonVisibility(
            visibility.VisibleOnPointerOver
        )
        pager.setSelectedPageIndex(4)
        pager.setOrientation(Qt.Vertical)

        self.assertEqual(page_count_changes, [7])
        self.assertEqual(selected_page_changes, [4])
        self.assertEqual(selected_index_changes, [(0, 4)])
        self.assertEqual(visible_count_changes, [3])
        self.assertEqual(orientation_changes, [Qt.Vertical])
        self.assertEqual(
            previous_visibility_changes,
            [visibility.Visible],
        )
        self.assertEqual(
            next_visibility_changes,
            [visibility.VisibleOnPointerOver],
        )
        self.assertEqual(pager.visiblePipCount(), 3)
        self.assertEqual(pager.firstVisiblePage(), 3)
        self.assertFalse(pager.pipHitRect(0).isValid())
        self.assertTrue(pager.pipHitRect(4).isValid())
        self.assertTrue(pager.hasPreviousPage())
        self.assertTrue(pager.hasNextPage())
        self.assertTrue(pager.goToPreviousPage())
        self.assertEqual(pager.selectedPageIndex(), 3)
        self.assertTrue(pager.goToNextPage())
        self.assertEqual(pager.selectedPageIndex(), 4)

        configured = fluentqt.PipsPager(
            numberOfPages=9,
            selectedPageIndex=5,
            maxVisiblePips=5,
            selectionAnimationEnabled=False,
        )
        self.assertEqual(configured.numberOfPages(), 9)
        self.assertEqual(configured.selectedPageIndex(), 5)
        self.assertEqual(configured.maxVisiblePips(), 5)
        self.assertFalse(configured.selectionAnimationEnabled())
        meta_object = configured.metaObject()
        self.assertEqual(
            meta_object.indexOfProperty("selectedVisualOffset"),
            -1,
        )
        self.assertEqual(
            meta_object.indexOfProperty("visibleWindowOffset"),
            -1,
        )

    def test_info_bar_properties_signals_and_action_facade(self):
        action = fluentqt.Button("Details")
        bar = fluentqt.InfoBar(
            title="Bindings ready",
            message="Native action hosting",
            severity=fluentqt.InfoBar.InfoBarSeverity.Success,
            isClosable=False,
            actionWidget=action,
        )
        title_changes = []
        message_changes = []
        severity_changes = []
        action_changes = []
        bar.titleChanged.connect(title_changes.append)
        bar.messageChanged.connect(message_changes.append)
        bar.severityChanged.connect(severity_changes.append)
        bar.actionWidgetChanged.connect(action_changes.append)

        self.assertEqual(bar.title(), "Bindings ready")
        self.assertEqual(bar.message(), "Native action hosting")
        self.assertEqual(
            bar.severity(),
            fluentqt.InfoBar.InfoBarSeverity.Success,
        )
        self.assertFalse(bar.isClosable())
        self.assertIs(bar.actionWidget(), action)
        self.assertIs(bar._fluentqt_action_widget, action)
        self.assertIs(action.parentWidget(), bar)

        bar.setTitle("Updated")
        bar.setMessage("Updated message")
        bar.setSeverity(fluentqt.InfoBar.InfoBarSeverity.Warning)
        self.assertEqual(title_changes, ["Updated"])
        self.assertEqual(message_changes, ["Updated message"])
        self.assertEqual(
            severity_changes,
            [fluentqt.InfoBar.InfoBarSeverity.Warning],
        )

        replacement = fluentqt.HyperlinkButton("Learn more")
        bar.setActionWidget(replacement)
        self.assertIsNone(action.parent())
        self.assertTrue(Shiboken.isValid(action))
        self.assertIs(bar.actionWidget(), replacement)
        self.assertIs(bar._fluentqt_action_widget, replacement)
        self.assertEqual(action_changes, [replacement])

        taken = bar.takeActionWidget()
        self.assertIs(taken, replacement)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))
        self.assertIsNone(bar.actionWidget())
        self.assertIsNone(bar._fluentqt_action_widget)
        self.assertEqual(action_changes, [replacement, None])

    def test_info_bar_retains_python_action_and_tracks_external_delete(self):
        class PythonAction(fluentqt.Button):
            def __init__(self):
                super().__init__("Python action")
                self.marker = "python-subclass"

        bar = fluentqt.InfoBar()
        action = PythonAction()
        action_ref = weakref.ref(action)
        changes = []
        bar.actionWidgetChanged.connect(changes.append)
        bar.setActionWidget(action)

        del action
        gc.collect()
        hosted = bar.actionWidget()
        self.assertIs(hosted, action_ref())
        self.assertEqual(hosted.marker, "python-subclass")
        self.assertIs(bar._fluentqt_action_widget, hosted)

        # Exercise the supported Qt-owned external-destruction path. Direct
        # Shiboken.delete() on a still-parented Python subclass can fast-fail
        # inside PySide6 6.2.4 on Windows before Qt can finish its destroyed
        # signal chain, so that low-level wrapper operation is not part of the
        # public lifecycle contract.
        hosted.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(hosted))
        self.assertIsNone(bar.actionWidget())
        self.assertIsNone(bar._fluentqt_action_widget)
        self.assertEqual(len(changes), 2)
        self.assertIsNone(changes[-1])

    def test_info_bar_rejects_invalid_actions(self):
        parent = QWidget()
        bar = fluentqt.InfoBar(parent)

        with self.assertRaisesRegex(
            ValueError,
            "host or its ancestor",
        ):
            bar.setActionWidget(bar)
        with self.assertRaisesRegex(
            ValueError,
            "host or its ancestor",
        ):
            bar.setActionWidget(parent)
        self.assertIsNone(bar.actionWidget())

    def test_info_bar_hosted_action_gc_stress(self):
        for _ in range(25):
            bar = fluentqt.InfoBar()
            action = fluentqt.Button("Action")
            bar.setActionWidget(action)
            bar_ref = weakref.ref(bar)
            del bar
            gc.collect()
            self.assertIsNone(bar_ref())
            self.assertFalse(Shiboken.isValid(action))
            del action

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
        owned_window_ref = weakref.ref(owned_window)
        del owned_window
        gc.collect()
        self.assertIsNone(owned_window_ref())
        self.assertFalse(Shiboken.isValid(owned_child))

    def test_expander_constructor_routes_content_through_facade(self):
        content = QWidget()
        expander = fluentqt.Expander(contentWidget=content)

        self.assertIs(expander.contentWidget(), content)
        self.assertIs(expander._fluentqt_hosted_content, content)
        self.assertEqual(
            expander.contentOwnership(),
            fluentqt.WidgetOwnership.Borrowed,
        )

        expander_ref = weakref.ref(expander)
        del expander
        gc.collect()
        self.assertIsNone(expander_ref())
        self.assertTrue(Shiboken.isValid(content))
        self.assertIsNone(content.parent())

    def test_expander_owned_content_lifecycle(self):
        expander = fluentqt.Expander()
        first = QWidget()
        second = QWidget()

        expander.setOwnedContentWidget(first)
        self.assertIs(expander.contentWidget(), first)
        self.assertIs(expander._fluentqt_hosted_content, first)
        self.assertEqual(
            expander.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

        expander.setOwnedContentWidget(second)
        self.assertFalse(Shiboken.isValid(first))
        self.assertIs(expander.contentWidget(), second)

        expander.setOwnedContentWidget(None)
        self.assertFalse(Shiboken.isValid(second))
        self.assertIsNone(expander.contentWidget())
        self.assertIsNone(expander._fluentqt_hosted_content)
        self.assertEqual(
            expander.contentOwnership(),
            fluentqt.WidgetOwnership.Borrowed,
        )

        owned_expander = fluentqt.Expander()
        owned_content = QWidget()
        owned_expander.setOwnedContentWidget(owned_content)
        owned_expander_ref = weakref.ref(owned_expander)
        del owned_expander
        gc.collect()
        self.assertIsNone(owned_expander_ref())
        self.assertFalse(Shiboken.isValid(owned_content))

    def test_expander_borrowed_content_lifecycle(self):
        class PythonContent(QWidget):
            def __init__(self):
                super().__init__()
                self.marker = "expander-borrowed-subclass"

        expander = fluentqt.Expander()
        first = PythonContent()
        first_ref = weakref.ref(first)
        expander.setContentWidget(first)
        del first
        gc.collect()

        hosted = expander.contentWidget()
        self.assertIs(hosted, first_ref())
        self.assertEqual(hosted.marker, "expander-borrowed-subclass")
        self.assertIs(expander._fluentqt_hosted_content, hosted)
        self.assertEqual(
            expander.contentOwnership(),
            fluentqt.WidgetOwnership.Borrowed,
        )

        second = QWidget()
        expander.setBorrowedContentWidget(second)
        self.assertTrue(Shiboken.isValid(hosted))
        self.assertIsNone(hosted.parent())

        expander.setContentWidget(None)
        self.assertTrue(Shiboken.isValid(second))
        self.assertIsNone(second.parent())
        self.assertIsNone(expander.contentWidget())

        previous_parent = QWidget()
        previous_parent_ref = weakref.ref(previous_parent)
        parented = QWidget(previous_parent)
        parented_expander = fluentqt.Expander()
        parented_expander.setBorrowedContentWidget(parented)
        del previous_parent
        gc.collect()
        self.assertIsNone(previous_parent_ref())
        self.assertTrue(Shiboken.isValid(parented))

        parented_expander_ref = weakref.ref(parented_expander)
        del parented_expander
        gc.collect()
        self.assertIsNone(parented_expander_ref())
        self.assertTrue(Shiboken.isValid(parented))
        self.assertIsNone(parented.parent())

    def test_expander_reparented_content_lifecycle(self):
        first_parent = QWidget()
        first = QWidget(first_parent)
        second_parent = QWidget()
        second = QWidget(second_parent)
        expander = fluentqt.Expander()

        expander.setReparentedContentWidget(first)
        self.assertIsNot(first.parent(), first_parent)
        self.assertIs(expander._fluentqt_original_parent, first_parent)
        self.assertEqual(
            expander.contentOwnership(),
            fluentqt.WidgetOwnership.Reparented,
        )

        expander.setReparentedContentWidget(second)
        self.assertTrue(Shiboken.isValid(first))
        self.assertIs(first.parent(), first_parent)
        self.assertIsNot(second.parent(), second_parent)

        expander.setReparentedContentWidget(None)
        self.assertTrue(Shiboken.isValid(second))
        self.assertIs(second.parent(), second_parent)
        self.assertIsNone(expander.contentWidget())

        restore_parent = QWidget()
        restored = QWidget(restore_parent)
        restoring_expander = fluentqt.Expander()
        restoring_expander.setReparentedContentWidget(restored)
        restoring_expander_ref = weakref.ref(restoring_expander)
        del restoring_expander
        gc.collect()
        self.assertIsNone(restoring_expander_ref())
        self.assertTrue(Shiboken.isValid(restored))
        self.assertIs(restored.parent(), restore_parent)

    def test_expander_reparented_keeps_original_parent_alive(self):
        original_parent = QWidget()
        original_parent_ref = weakref.ref(original_parent)
        child = QWidget(original_parent)
        expander = fluentqt.Expander()
        expander.setReparentedContentWidget(child)

        del original_parent
        gc.collect()
        self.assertIsNotNone(original_parent_ref())
        self.assertIs(
            expander._fluentqt_original_parent,
            original_parent_ref(),
        )

        taken = expander.takeContentWidget()
        gc.collect()
        self.assertIs(taken, child)
        self.assertIsNone(taken.parent())
        self.assertIsNone(original_parent_ref())

    def test_expander_rejects_invalid_content_and_mode_changes(self):
        parent = QWidget()
        expander = fluentqt.Expander(parent)
        child = QWidget()
        expander.setBorrowedContentWidget(child)

        with self.assertRaisesRegex(
            ValueError,
            "takeContentWidget",
        ):
            expander.setOwnedContentWidget(child)
        with self.assertRaisesRegex(
            ValueError,
            "host or its ancestor",
        ):
            expander.setBorrowedContentWidget(expander)
        with self.assertRaisesRegex(
            ValueError,
            "host or its ancestor",
        ):
            expander.setBorrowedContentWidget(parent)

        self.assertIs(expander.parent(), parent)
        self.assertIs(expander.contentWidget(), child)

    def test_expander_take_handles_all_modes(self):
        for mode_name, setter_name in (
            ("Owned", "setOwnedContentWidget"),
            ("Borrowed", "setBorrowedContentWidget"),
            ("Reparented", "setReparentedContentWidget"),
        ):
            with self.subTest(mode=mode_name):
                original_parent = (
                    QWidget() if mode_name == "Reparented" else None
                )
                child = QWidget(original_parent)
                expander = fluentqt.Expander()
                getattr(expander, setter_name)(child)

                taken = expander.takeContentWidget()
                self.assertIs(taken, child)
                self.assertIsNone(taken.parent())
                self.assertTrue(Shiboken.ownedByPython(taken))
                self.assertIsNone(expander._fluentqt_hosted_content)
                self.assertIsNone(expander._fluentqt_original_parent)
                self.assertEqual(
                    expander.contentOwnership(),
                    fluentqt.WidgetOwnership.Borrowed,
                )

                expander_ref = weakref.ref(expander)
                del expander
                gc.collect()
                self.assertIsNone(expander_ref())
                self.assertTrue(Shiboken.isValid(taken))
                taken_ref = weakref.ref(taken)
                del taken
                del child
                gc.collect()
                self.assertIsNone(taken_ref())

    def test_expander_owned_gc_stress(self):
        for _ in range(25):
            expander = fluentqt.Expander()
            child = QWidget()
            expander.setOwnedContentWidget(child)
            expander_ref = weakref.ref(expander)
            del expander
            gc.collect()
            self.assertIsNone(expander_ref())
            self.assertFalse(Shiboken.isValid(child))
            del child

    def test_expander_borrowed_gc_stress(self):
        for _ in range(25):
            expander = fluentqt.Expander()
            child = QWidget()
            expander.setBorrowedContentWidget(child)
            expander_ref = weakref.ref(expander)
            del expander
            gc.collect()
            self.assertIsNone(expander_ref())
            self.assertTrue(Shiboken.isValid(child))
            self.assertIsNone(child.parent())
            del child
            gc.collect()

    def test_expander_reparented_gc_stress(self):
        for _ in range(25):
            original_parent = QWidget()
            child = QWidget(original_parent)
            expander = fluentqt.Expander()
            expander.setReparentedContentWidget(child)
            expander_ref = weakref.ref(expander)
            del expander
            gc.collect()
            self.assertIsNone(expander_ref())
            self.assertTrue(Shiboken.isValid(child))
            self.assertIs(child.parent(), original_parent)
            del original_parent
            gc.collect()
            self.assertFalse(Shiboken.isValid(child))
            del child

    def test_accordion_properties_signals_and_item_facade(self):
        class PythonItem(fluentqt.Expander):
            def __init__(self, title):
                super().__init__()
                self.setHeaderText(title)
                self.setAnimationEnabled(False)
                self.marker = "python-accordion-item"

        accordion = fluentqt.Accordion()
        self.assertEqual(
            accordion.expansionMode(),
            fluentqt.Accordion.ExpansionMode.Single,
        )
        self.assertEqual(accordion.count(), 0)

        mode_changes = []
        count_changes = []
        added = []
        removed = []
        expansion_changes = []
        accordion.expansionModeChanged.connect(mode_changes.append)
        accordion.countChanged.connect(count_changes.append)
        accordion.itemAdded.connect(
            lambda index, item: added.append((index, item))
        )
        accordion.itemRemoved.connect(
            lambda index, item: removed.append((index, item))
        )
        accordion.itemExpansionChanged.connect(
            lambda index, expanded: expansion_changes.append(
                (index, expanded)
            )
        )

        first = PythonItem("First")
        second = PythonItem("Second")
        self.assertTrue(accordion.addItem(first))
        self.assertTrue(accordion.insertOwnedItem(0, second))
        self.assertEqual(accordion.count(), 2)
        self.assertIs(accordion.itemAt(0), second)
        self.assertIs(accordion.itemAt(1), first)
        self.assertEqual(first.marker, "python-accordion-item")
        self.assertEqual(accordion.indexOf(second), 0)
        self.assertEqual(
            accordion.itemOwnershipAt(0),
            fluentqt.WidgetOwnership.Owned,
        )
        self.assertEqual(
            accordion.itemOwnershipAt(1),
            fluentqt.WidgetOwnership.Borrowed,
        )
        self.assertEqual(count_changes, [1, 2])
        self.assertEqual(added, [(0, first), (0, second)])

        first.setExpanded(True)
        second.setExpanded(True)
        self.assertFalse(first.isExpanded())
        self.assertTrue(second.isExpanded())
        self.assertGreaterEqual(len(expansion_changes), 3)

        accordion.setExpansionMode(
            fluentqt.Accordion.ExpansionMode.Multiple
        )
        accordion.setExpansionMode(
            fluentqt.Accordion.ExpansionMode.Multiple
        )
        self.assertEqual(
            mode_changes,
            [fluentqt.Accordion.ExpansionMode.Multiple],
        )
        first.setExpanded(True)
        self.assertTrue(first.isExpanded())
        self.assertTrue(second.isExpanded())

        taken = accordion.takeItem(0)
        self.assertIs(taken, second)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))
        self.assertEqual(removed, [(0, second)])
        self.assertTrue(accordion.removeItem(0))
        self.assertTrue(Shiboken.isValid(first))
        self.assertIsNone(first.parent())
        self.assertEqual(accordion.count(), 0)

    def test_accordion_owned_item_lifecycle(self):
        accordion = fluentqt.Accordion()
        removed_item = fluentqt.Expander()
        self.assertTrue(accordion.addOwnedItem(removed_item))
        self.assertTrue(accordion.removeItem(0))
        self.assertFalse(Shiboken.isValid(removed_item))

        owned_item = fluentqt.Expander()
        self.assertTrue(accordion.addOwnedItem(owned_item))
        accordion_ref = weakref.ref(accordion)
        del accordion
        gc.collect()
        self.assertIsNone(accordion_ref())
        self.assertFalse(Shiboken.isValid(owned_item))

    def test_accordion_borrowed_item_lifecycle(self):
        class PythonItem(fluentqt.Expander):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.marker = "borrowed-accordion-subclass"

        previous_parent = QWidget()
        previous_parent_ref = weakref.ref(previous_parent)
        item = PythonItem(previous_parent)
        item_ref = weakref.ref(item)
        accordion = fluentqt.Accordion()
        self.assertTrue(accordion.addBorrowedItem(item))
        self.assertIs(item.parentWidget(), accordion)

        del previous_parent
        del item
        gc.collect()
        self.assertIsNone(previous_parent_ref())
        hosted = accordion.itemAt(0)
        self.assertIs(hosted, item_ref())
        self.assertEqual(hosted.marker, "borrowed-accordion-subclass")

        accordion_ref = weakref.ref(accordion)
        del accordion
        gc.collect()
        self.assertIsNone(accordion_ref())
        self.assertTrue(Shiboken.isValid(hosted))
        self.assertIsNone(hosted.parent())

    def test_accordion_reparented_item_lifecycle(self):
        original_parent = QWidget()
        original_parent_ref = weakref.ref(original_parent)
        item = fluentqt.Expander(original_parent)
        accordion = fluentqt.Accordion()
        self.assertTrue(accordion.addReparentedItem(item))
        self.assertIs(item.parentWidget(), accordion)

        del original_parent
        gc.collect()
        self.assertIsNotNone(original_parent_ref())

        taken = accordion.takeItem(0)
        gc.collect()
        self.assertIs(taken, item)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))
        self.assertIsNone(original_parent_ref())

        restore_parent = QWidget()
        restored = fluentqt.Expander(restore_parent)
        restoring_accordion = fluentqt.Accordion()
        self.assertTrue(
            restoring_accordion.insertReparentedItem(0, restored)
        )
        restoring_ref = weakref.ref(restoring_accordion)
        del restoring_accordion
        gc.collect()
        self.assertIsNone(restoring_ref())
        self.assertTrue(Shiboken.isValid(restored))
        self.assertIs(restored.parent(), restore_parent)

    def test_accordion_rejects_duplicates_and_tracks_external_delete(self):
        accordion = fluentqt.Accordion()
        item = fluentqt.Expander()
        self.assertFalse(accordion.addItem(None))
        self.assertTrue(accordion.addItem(item))
        self.assertFalse(accordion.addOwnedItem(item))
        self.assertEqual(accordion.count(), 1)

        item.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(item))
        self.assertEqual(accordion.count(), 0)
        self.assertEqual(accordion._fluentqt_item_records, {})

    def test_accordion_owned_gc_stress(self):
        for _ in range(25):
            accordion = fluentqt.Accordion()
            item = fluentqt.Expander()
            accordion.addOwnedItem(item)
            accordion_ref = weakref.ref(accordion)
            del accordion
            gc.collect()
            self.assertIsNone(accordion_ref())
            self.assertFalse(Shiboken.isValid(item))
            del item

    def test_accordion_borrowed_gc_stress(self):
        for _ in range(25):
            accordion = fluentqt.Accordion()
            item = fluentqt.Expander()
            accordion.addBorrowedItem(item)
            accordion_ref = weakref.ref(accordion)
            del accordion
            gc.collect()
            self.assertIsNone(accordion_ref())
            self.assertTrue(Shiboken.isValid(item))
            self.assertIsNone(item.parent())
            del item
            gc.collect()

    def test_accordion_reparented_gc_stress(self):
        for _ in range(25):
            original_parent = QWidget()
            item = fluentqt.Expander(original_parent)
            accordion = fluentqt.Accordion()
            accordion.addReparentedItem(item)
            accordion_ref = weakref.ref(accordion)
            del accordion
            gc.collect()
            self.assertIsNone(accordion_ref())
            self.assertTrue(Shiboken.isValid(item))
            self.assertIs(item.parent(), original_parent)
            del original_parent
            gc.collect()
            self.assertFalse(Shiboken.isValid(item))
            del item

    def test_stack_view_properties_signals_and_navigation(self):
        class PythonPage(QWidget):
            def __init__(self):
                super().__init__()
                self.marker = "python-stack-page"

        stack = fluentqt.StackView()
        stack.setTransitionAnimationEnabled(False)
        self.assertEqual(stack.depth(), 0)
        self.assertIsNone(stack.currentItem())
        self.assertIsNone(stack.initialItem())
        self.assertFalse(stack.canPop())
        self.assertEqual(stack.orientation(), Qt.Horizontal)
        self.assertEqual(
            stack.transitionType(),
            fluentqt.StackView.StackViewTransitionType.SlideFade,
        )

        pushed = []
        popped = []
        depth_changes = []
        current_changes = []
        stack.itemPushed.connect(pushed.append)
        stack.itemPopped.connect(popped.append)
        stack.depthChanged.connect(depth_changes.append)
        stack.currentItemChanged.connect(current_changes.append)

        root = PythonPage()
        borrowed = PythonPage()
        self.assertTrue(stack.push(root))
        self.assertTrue(stack.pushBorrowedItem(borrowed))
        self.assertEqual(stack.depth(), 2)
        self.assertIs(stack.initialItem(), root)
        self.assertIs(stack.currentItem(), borrowed)
        self.assertEqual(borrowed.marker, "python-stack-page")
        self.assertEqual(
            stack.itemStatus(root),
            fluentqt.StackView.StackViewItemStatus.Inactive,
        )
        self.assertEqual(
            stack.itemStatus(borrowed),
            fluentqt.StackView.StackViewItemStatus.Active,
        )
        self.assertEqual(pushed, [root, borrowed])
        self.assertEqual(depth_changes, [1, 2])

        stack.setCurrentWidget(root)
        self.assertIs(stack.currentItem(), root)
        stack.setCurrentWidget(borrowed)
        self.assertIs(stack.currentItem(), borrowed)
        self.assertTrue(stack.pop())
        self.assertEqual(popped, [borrowed])
        self.assertEqual(stack.depth(), 1)
        self.assertIs(stack.currentItem(), root)
        self.assertIsNone(borrowed.parent())
        self.assertNotIn(id(borrowed), stack._fluentqt_page_records)
        self.assertGreaterEqual(len(current_changes), 3)

        replacement = QWidget()
        self.assertTrue(stack.replaceBorrowedItem(replacement))
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(root))
        self.assertIs(stack.currentItem(), replacement)
        self.assertEqual(stack.depth(), 1)

        stack.setOrientation(Qt.Vertical)
        stack.setTransitionDuration(-10)
        stack.setTransitionType(
            fluentqt.StackView.StackViewTransitionType.ScaleFade
        )
        self.assertEqual(stack.orientation(), Qt.Vertical)
        self.assertEqual(stack.transitionDuration(), 0)
        self.assertEqual(
            stack.transitionType(),
            fluentqt.StackView.StackViewTransitionType.ScaleFade,
        )

    def test_stack_view_constructor_routes_initial_page_through_facade(self):
        page = QWidget()
        stack = fluentqt.StackView(initialItem=page)
        stack.setTransitionAnimationEnabled(False)
        self.assertIs(stack.initialItem(), page)
        self.assertIs(stack.currentItem(), page)
        self.assertEqual(stack.depth(), 1)
        self.assertIn(id(page), stack._fluentqt_page_records)

        stack_ref = weakref.ref(stack)
        del stack
        gc.collect()
        self.assertIsNone(stack_ref())
        self.assertFalse(Shiboken.isValid(page))

    def test_stack_view_owned_page_lifecycle(self):
        stack = fluentqt.StackView()
        stack.setTransitionAnimationEnabled(False)
        root = QWidget()
        owned = QWidget()
        self.assertTrue(stack.pushBorrowedItem(root))
        self.assertTrue(stack.pushOwnedItem(owned))
        self.assertTrue(stack.pop())
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        self.app.processEvents()
        self.assertFalse(Shiboken.isValid(owned))
        self.assertTrue(Shiboken.isValid(root))

        second_owned = QWidget()
        self.assertTrue(stack.pushOwnedItem(second_owned))
        stack_ref = weakref.ref(stack)
        del stack
        gc.collect()
        self.assertIsNone(stack_ref())
        self.assertFalse(Shiboken.isValid(second_owned))
        self.assertTrue(Shiboken.isValid(root))
        self.assertIsNone(root.parent())

    def test_stack_view_borrowed_page_lifecycle(self):
        class PythonPage(QWidget):
            def __init__(self):
                super().__init__()
                self.marker = "borrowed-stack-subclass"

        stack = fluentqt.StackView()
        stack.setTransitionAnimationEnabled(False)
        root = QWidget()
        page = PythonPage()
        page_ref = weakref.ref(page)
        self.assertTrue(stack.pushOwnedItem(root))
        self.assertTrue(stack.pushBorrowedItem(page))
        del page
        gc.collect()

        hosted = stack.currentItem()
        self.assertIs(hosted, page_ref())
        self.assertEqual(hosted.marker, "borrowed-stack-subclass")
        self.assertTrue(stack.pop())
        self.assertTrue(Shiboken.isValid(hosted))
        self.assertIsNone(hosted.parent())
        self.assertEqual(stack._fluentqt_page_records.keys(), {id(root)})

        stack_ref = weakref.ref(stack)
        del stack
        gc.collect()
        self.assertIsNone(stack_ref())
        self.assertTrue(Shiboken.isValid(hosted))
        self.assertIsNone(hosted.parent())

    def test_stack_view_reparented_page_lifecycle(self):
        original_parent = QWidget()
        original_parent_ref = weakref.ref(original_parent)
        page = QWidget(original_parent)
        stack = fluentqt.StackView()
        stack.setTransitionAnimationEnabled(False)
        self.assertTrue(stack.pushOwnedItem(QWidget()))
        self.assertTrue(stack.pushReparentedItem(page))
        self.assertIs(page.parent(), stack)

        del original_parent
        gc.collect()
        self.assertIsNotNone(original_parent_ref())
        restored_parent = original_parent_ref()
        self.assertTrue(stack.pop())
        self.assertIsNotNone(restored_parent)
        self.assertIs(page.parent(), restored_parent)
        self.assertNotIn(id(page), stack._fluentqt_page_records)

        second_stack = fluentqt.StackView()
        second_stack.setTransitionAnimationEnabled(False)
        second_parent = QWidget()
        second_page = QWidget(second_parent)
        self.assertTrue(
            second_stack.setInitialReparentedItem(second_page)
        )
        second_stack_ref = weakref.ref(second_stack)
        del second_stack
        gc.collect()
        self.assertIsNone(second_stack_ref())
        self.assertTrue(Shiboken.isValid(second_page))
        self.assertIs(second_page.parent(), second_parent)

    def test_stack_view_bulk_navigation_and_indexed_replacement(self):
        stack = fluentqt.StackView()
        stack.setTransitionAnimationEnabled(False)
        pages = [QWidget(), QWidget(), QWidget()]
        self.assertTrue(stack.pushBorrowedItems(pages))
        self.assertEqual(stack.depth(), 3)
        self.assertIs(stack.currentItem(), pages[-1])
        self.assertEqual(
            [stack.itemAt(index) for index in range(stack.depth())],
            pages,
        )

        self.assertTrue(stack.popToItem(pages[0]))
        self.assertEqual(stack.depth(), 1)
        self.assertIs(stack.currentItem(), pages[0])
        self.assertIsNone(pages[1].parent())
        self.assertIsNone(pages[2].parent())

        replacement_parent = QWidget()
        replacement = QWidget(replacement_parent)
        self.assertTrue(
            stack.replaceReparentedItemAt(0, replacement)
        )
        self.assertIsNone(pages[0].parent())
        self.assertIs(stack.currentItem(), replacement)
        self.assertTrue(stack.clear())
        self.assertIs(replacement.parent(), replacement_parent)
        self.assertEqual(stack._fluentqt_page_records, {})

    def test_stack_view_retains_page_until_animated_pop_finishes(self):
        stack = fluentqt.StackView()
        stack.resize(360, 220)
        stack.setTransitionDuration(40)
        root = QWidget()
        page = QWidget()
        self.assertTrue(stack.pushOwnedItem(root))
        self.assertTrue(stack.pushBorrowedItem(page))
        self.assertTrue(stack.busy())
        wait_for_events(80)
        self.assertFalse(stack.busy())

        self.assertTrue(stack.pop())
        self.assertTrue(stack.busy())
        self.assertIn(id(page), stack._fluentqt_page_records)
        self.assertIs(page.parent(), stack)
        wait_for_events(80)
        self.assertFalse(stack.busy())
        self.assertNotIn(id(page), stack._fluentqt_page_records)
        self.assertIsNone(page.parent())

    def test_stack_view_blocks_unsafe_base_mutation_and_rolls_back(self):
        stack = fluentqt.StackView()
        page = QWidget()
        self.assertTrue(stack.pushBorrowedItem(page))
        candidate_parent = QWidget()
        candidate = QWidget(candidate_parent)
        self.assertFalse(stack.replaceOwnedItemAt(5, candidate))
        self.assertIs(candidate.parent(), candidate_parent)
        self.assertEqual(
            stack._fluentqt_page_records.keys(),
            {id(page)},
        )

        self.assertFalse(stack.pushOwnedItem(page))
        self.assertIs(page.parent(), stack)
        with self.assertRaisesRegex(RuntimeError, "ownership-aware"):
            stack.insertWidget(0, QWidget())
        with self.assertRaisesRegex(RuntimeError, "pushOwnedItem"):
            stack.addWidget(QWidget())
        with self.assertRaisesRegex(RuntimeError, "pop"):
            stack.removeWidget(page)
        with self.assertRaisesRegex(ValueError, "already be hosted"):
            stack.setCurrentWidget(QWidget())
        with self.assertRaisesRegex(TypeError, "defaultItemOwnership"):
            fluentqt.StackView(
                defaultItemOwnership=fluentqt.WidgetOwnership.Borrowed
            )

        ancestor = QWidget()
        nested_stack = fluentqt.StackView(ancestor)
        with self.assertRaisesRegex(ValueError, "host or its ancestor"):
            nested_stack.pushBorrowedItem(ancestor)

    def test_stack_view_owned_gc_stress(self):
        for _ in range(25):
            stack = fluentqt.StackView()
            stack.setTransitionAnimationEnabled(False)
            page = QWidget()
            stack.pushOwnedItem(page)
            stack_ref = weakref.ref(stack)
            del stack
            gc.collect()
            self.assertIsNone(stack_ref())
            self.assertFalse(Shiboken.isValid(page))
            del page

    def test_stack_view_borrowed_gc_stress(self):
        for _ in range(25):
            stack = fluentqt.StackView()
            stack.setTransitionAnimationEnabled(False)
            page = QWidget()
            stack.pushBorrowedItem(page)
            stack_ref = weakref.ref(stack)
            del stack
            gc.collect()
            self.assertIsNone(stack_ref())
            self.assertTrue(Shiboken.isValid(page))
            self.assertIsNone(page.parent())
            del page
            gc.collect()

    def test_stack_view_reparented_gc_stress(self):
        for _ in range(25):
            original_parent = QWidget()
            page = QWidget(original_parent)
            stack = fluentqt.StackView()
            stack.setTransitionAnimationEnabled(False)
            stack.pushReparentedItem(page)
            stack_ref = weakref.ref(stack)
            del stack
            gc.collect()
            self.assertIsNone(stack_ref())
            self.assertTrue(Shiboken.isValid(page))
            self.assertIs(page.parent(), original_parent)
            del original_parent
            gc.collect()
            self.assertFalse(Shiboken.isValid(page))
            del page

    def test_scroll_view_constructor_routes_content_through_facade(self):
        content = QWidget()
        view = fluentqt.ScrollView(contentWidget=content)

        self.assertIs(view.contentWidget(), content)
        self.assertIs(view._fluentqt_hosted_content, content)
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

        view_ref = weakref.ref(view)
        del view
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertFalse(Shiboken.isValid(content))

    def test_scroll_view_owned_content_lifecycle(self):
        view = fluentqt.ScrollView()
        first = QWidget()
        second = QWidget()

        view.setContentWidget(first)
        self.assertIs(view.contentWidget(), first)
        self.assertIs(view._fluentqt_hosted_content, first)
        self.assertIs(first.parent(), view.viewport())
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

        view.setOwnedContentWidget(second)
        self.assertFalse(Shiboken.isValid(first))
        self.assertIs(view.contentWidget(), second)
        self.assertIs(view._fluentqt_hosted_content, second)

        view.setContentWidget(None)
        self.assertFalse(Shiboken.isValid(second))
        self.assertIsNone(view.contentWidget())
        self.assertIsNone(view._fluentqt_hosted_content)
        self.assertIsNone(view._fluentqt_original_parent)
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

        rejected = QWidget()
        with self.assertRaises(TypeError):
            view.setContentWidget(
                rejected,
                fluentqt.WidgetOwnership.Borrowed,
            )
        with self.assertRaises(TypeError):
            native.fluent.ScrollView.setContentWidget(
                view,
                rejected,
                fluentqt.WidgetOwnership.Borrowed,
            )
        self.assertTrue(Shiboken.isValid(rejected))
        rejected_ref = weakref.ref(rejected)
        del rejected
        gc.collect()
        self.assertIsNone(rejected_ref())
        view_ref = weakref.ref(view)
        del view
        gc.collect()
        self.assertIsNone(view_ref())

        previous_parent = QWidget()
        previous_parent_ref = weakref.ref(previous_parent)
        parented = QWidget(previous_parent)
        parented_view = fluentqt.ScrollView()
        parented_view.setContentWidget(parented)
        self.assertIs(parented.parent(), parented_view.viewport())
        del previous_parent
        gc.collect()
        self.assertIsNone(previous_parent_ref())
        self.assertTrue(Shiboken.isValid(parented))
        parented_view_ref = weakref.ref(parented_view)
        del parented_view
        gc.collect()
        self.assertIsNone(parented_view_ref())
        self.assertFalse(Shiboken.isValid(parented))

    def test_scroll_view_borrowed_content_lifecycle(self):
        class PythonContent(QWidget):
            def __init__(self):
                super().__init__()
                self.marker = "borrowed-subclass"

        view = fluentqt.ScrollView()
        first = PythonContent()
        first_ref = weakref.ref(first)
        view.setBorrowedContentWidget(first)
        del first
        gc.collect()

        hosted = view.contentWidget()
        self.assertIs(hosted, first_ref())
        self.assertEqual(hosted.marker, "borrowed-subclass")
        self.assertIs(view._fluentqt_hosted_content, hosted)
        self.assertIsNone(view._fluentqt_original_parent)
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Borrowed,
        )

        second = QWidget()
        view.setBorrowedContentWidget(second)
        self.assertTrue(Shiboken.isValid(hosted))
        self.assertIsNone(hosted.parent())
        self.assertIs(view.contentWidget(), second)

        view.setBorrowedContentWidget(None)
        self.assertTrue(Shiboken.isValid(second))
        self.assertIsNone(second.parent())
        self.assertIsNone(view.contentWidget())
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

        previous_parent = QWidget()
        previous_parent_ref = weakref.ref(previous_parent)
        parented = QWidget(previous_parent)
        parented_view = fluentqt.ScrollView()
        parented_view.setBorrowedContentWidget(parented)
        self.assertIs(parented.parent(), parented_view.viewport())
        del previous_parent
        gc.collect()
        self.assertIsNone(previous_parent_ref())
        self.assertTrue(Shiboken.isValid(parented))

        parented_view_ref = weakref.ref(parented_view)
        del parented_view
        gc.collect()
        self.assertIsNone(parented_view_ref())
        self.assertTrue(Shiboken.isValid(parented))
        self.assertIsNone(parented.parent())

    def test_scroll_view_reparented_content_lifecycle(self):
        first_parent = QWidget()
        first = QWidget(first_parent)
        second_parent = QWidget()
        second = QWidget(second_parent)
        view = fluentqt.ScrollView()

        view.setReparentedContentWidget(first)
        self.assertIs(first.parent(), view.viewport())
        self.assertIs(view._fluentqt_original_parent, first_parent)
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Reparented,
        )

        view.setReparentedContentWidget(second)
        self.assertTrue(Shiboken.isValid(first))
        self.assertIs(first.parent(), first_parent)
        self.assertIs(second.parent(), view.viewport())
        self.assertIs(view._fluentqt_original_parent, second_parent)

        view.setReparentedContentWidget(None)
        self.assertTrue(Shiboken.isValid(second))
        self.assertIs(second.parent(), second_parent)
        self.assertIsNone(view.contentWidget())
        self.assertIsNone(view._fluentqt_original_parent)
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

        restore_parent = QWidget()
        restored = QWidget(restore_parent)
        restoring_view = fluentqt.ScrollView()
        restoring_view.setReparentedContentWidget(restored)
        restoring_view_ref = weakref.ref(restoring_view)
        del restoring_view
        gc.collect()
        self.assertIsNone(restoring_view_ref())
        self.assertTrue(Shiboken.isValid(restored))
        self.assertIs(restored.parent(), restore_parent)

    def test_scroll_view_reparented_keeps_original_parent_alive(self):
        original_parent = QWidget()
        original_parent_ref = weakref.ref(original_parent)
        child = QWidget(original_parent)
        view = fluentqt.ScrollView()
        view.setReparentedContentWidget(child)

        del original_parent
        gc.collect()
        self.assertIsNotNone(original_parent_ref())
        self.assertIs(
            view._fluentqt_original_parent,
            original_parent_ref(),
        )

        taken = view.takeContentWidget()
        gc.collect()
        self.assertIs(taken, child)
        self.assertIsNone(taken.parent())
        self.assertIsNone(original_parent_ref())

    def test_scroll_view_requires_take_before_mode_change(self):
        view = fluentqt.ScrollView()
        child = QWidget()
        view.setBorrowedContentWidget(child)

        with self.assertRaisesRegex(
            ValueError,
            "takeContentWidget",
        ):
            view.setOwnedContentWidget(child)
        with self.assertRaisesRegex(
            ValueError,
            "takeContentWidget",
        ):
            view.setReparentedContentWidget(child)

        taken = view.takeContentWidget()
        view.setOwnedContentWidget(taken)
        self.assertEqual(
            view.contentOwnership(),
            fluentqt.WidgetOwnership.Owned,
        )

    def test_scroll_view_take_returns_python_ownership(self):
        class PythonContent(QWidget):
            def __init__(self):
                super().__init__()
                self.marker = "python-subclass"

        view = fluentqt.ScrollView()
        content = PythonContent()
        view.setWidget(content)
        del content
        gc.collect()

        hosted = view.contentWidget()
        self.assertIsInstance(hosted, PythonContent)
        self.assertEqual(hosted.marker, "python-subclass")
        taken = view.takeWidget()
        self.assertIs(taken, hosted)
        self.assertIsNone(view._fluentqt_hosted_content)
        self.assertIsNone(view._fluentqt_original_parent)
        self.assertIsNone(taken.parent())
        self.assertTrue(Shiboken.ownedByPython(taken))

        view_ref = weakref.ref(view)
        del view
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertTrue(Shiboken.isValid(taken))
        taken_ref = weakref.ref(taken)
        del taken
        del hosted
        gc.collect()
        self.assertIsNone(taken_ref())

    def test_scroll_view_take_handles_non_owned_modes(self):
        for mode_name, setter_name in (
            ("Borrowed", "setBorrowedContentWidget"),
            ("Reparented", "setReparentedContentWidget"),
        ):
            with self.subTest(mode=mode_name):
                original_parent = (
                    QWidget() if mode_name == "Reparented" else None
                )
                child = QWidget(original_parent)
                view = fluentqt.ScrollView()
                getattr(view, setter_name)(child)

                taken = view.takeContentWidget()
                self.assertIs(taken, child)
                self.assertIsNone(taken.parent())
                self.assertTrue(Shiboken.ownedByPython(taken))
                self.assertIsNone(view._fluentqt_hosted_content)
                self.assertIsNone(view._fluentqt_original_parent)
                self.assertEqual(
                    view.contentOwnership(),
                    fluentqt.WidgetOwnership.Owned,
                )

                view_ref = weakref.ref(view)
                del view
                gc.collect()
                self.assertIsNone(view_ref())
                self.assertTrue(Shiboken.isValid(taken))
                taken_ref = weakref.ref(taken)
                del taken
                del child
                gc.collect()
                self.assertIsNone(taken_ref())

    def test_qscrollarea_owned_gc_stress(self):
        for _ in range(25):
            view = QScrollArea()
            child = QWidget()
            view.setWidget(child)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertFalse(Shiboken.isValid(child))
            del child

    def test_scroll_view_facade_gc_stress_without_content(self):
        for _ in range(25):
            view = fluentqt.ScrollView()
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())

    def test_scroll_view_native_owned_gc_stress(self):
        for _ in range(25):
            view = native.fluent.ScrollView()
            child = QWidget()
            view.setContentWidget(child)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertFalse(Shiboken.isValid(child))
            del child

    def test_scroll_view_owned_gc_stress(self):
        for _ in range(25):
            view = fluentqt.ScrollView()
            child = QWidget()
            view.setContentWidget(child)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertFalse(Shiboken.isValid(child))
            del child

    def test_scroll_view_borrowed_gc_stress(self):
        for _ in range(25):
            view = fluentqt.ScrollView()
            child = QWidget()
            view.setBorrowedContentWidget(child)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertTrue(Shiboken.isValid(child))
            self.assertIsNone(child.parent())
            del child
            gc.collect()

    def test_scroll_view_reparented_gc_stress(self):
        for _ in range(25):
            original_parent = QWidget()
            child = QWidget(original_parent)
            view = fluentqt.ScrollView()
            view.setReparentedContentWidget(child)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertTrue(Shiboken.isValid(child))
            self.assertIs(child.parent(), original_parent)
            del original_parent
            gc.collect()
            self.assertFalse(Shiboken.isValid(child))
            del child

    def test_scroll_view_qscrollarea_base_owned_gc_stress(self):
        for _ in range(25):
            view = native.fluent.ScrollView()
            child = QWidget()
            QScrollArea.setWidget(view, child)
            view_ref = weakref.ref(view)
            del view
            gc.collect()
            self.assertIsNone(view_ref())
            self.assertFalse(Shiboken.isValid(child))
            del child


if __name__ == "__main__":
    unittest.main()
