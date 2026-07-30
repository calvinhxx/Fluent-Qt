import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import Qt, QUrl
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget
from fluentqt.basicinput import (
    Button,
    CheckBox,
    HyperlinkButton,
    RadioButton,
    RepeatButton,
    Slider,
    ToggleButton,
    ToggleSwitch,
)
from fluentqt.layout import Divider
from fluentqt.status_info import InfoBadge, ProgressBar, ProgressRing, Shimmer
from fluentqt.textfields import Label, LineEdit, NumberBox, PasswordBox
from fluentqt.windowing import Window


app = QApplication(sys.argv)
fluentqt.initialize_resources()
app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

window = Window()
window.setWindowTitle("FluentQt PySide6 Controls")
window.resize(640, 860)

content = QWidget()
layout = QVBoxLayout(content)
layout.setContentsMargins(32, 32, 32, 32)
layout.setSpacing(16)

title = Label("PySide6 Controls")
title.setFluentTypography(fluentqt.FontRole.Title)
layout.addWidget(title)
layout.addWidget(Divider())

theme_row = QHBoxLayout()
theme_button = Button("Switch to Dark")
style_button = Button("Style: Fluent")
style_button.setFluentStyle(Button.ButtonStyle.Accent)
theme_row.addWidget(theme_button)
theme_row.addWidget(style_button)
theme_row.addStretch()
layout.addLayout(theme_row)

check_box = CheckBox("Animate shimmer")
radio_button = RadioButton("Recommended option")
radio_button.setChecked(True)
toggle_button = ToggleButton("Pinned")
toggle_switch = ToggleSwitch()
toggle_switch.setOnContent("On")
toggle_switch.setOffContent("Off")

layout.addWidget(check_box)
layout.addWidget(radio_button)
layout.addWidget(toggle_button)
layout.addWidget(toggle_switch)

repeat_row = QHBoxLayout()
repeat_button = RepeatButton("Click and hold")
repeat_button.setDelay(250)
repeat_button.setInterval(80)
repeat_count = Label("Clicks: 0")
repeat_row.addWidget(repeat_button)
repeat_row.addWidget(repeat_count)
repeat_row.addStretch()
layout.addLayout(repeat_row)

hyperlink = HyperlinkButton("calvinhxx/Fluent-Qt")
hyperlink.setUrl(QUrl("https://github.com/calvinhxx/Fluent-Qt"))
hyperlink.setShowUnderline(True)
layout.addWidget(hyperlink)

badge_row = QHBoxLayout()
badge_label = Label("Unread messages")
info_badge = InfoBadge()
info_badge.setValue(7)
info_badge.setDisplayMode(InfoBadge.InfoBadgeDisplayMode.Value)
info_badge.setStatus(InfoBadge.InfoBadgeStatus.Attention)
badge_row.addWidget(badge_label)
badge_row.addStretch()
badge_row.addWidget(info_badge)
layout.addLayout(badge_row)

shimmer = Shimmer()
shimmer.setAnimationEnabled(False)
shimmer.setShimmerProgress(0.35)
shimmer.setShimmerTemplate(Shimmer.ShimmerTemplate.AvatarTextRow)
shimmer.setFixedHeight(56)
layout.addWidget(shimmer)

slider = Slider(Qt.Horizontal)
slider.setRange(0, 100)
slider.setValue(60)
layout.addWidget(slider)

progress_row = QHBoxLayout()
progress_bar = ProgressBar()
progress_bar.setValue(60)
progress_ring = ProgressRing()
progress_ring.setIsIndeterminate(False)
progress_ring.setIsActive(True)
progress_ring.setBackgroundVisible(True)
progress_ring.setValue(60)
progress_row.addWidget(progress_bar)
progress_row.addWidget(progress_ring)
progress_row.addStretch()
layout.addLayout(progress_row)

line_edit = LineEdit()
line_edit.setPlaceholderText("Type a message")
layout.addWidget(line_edit)

password = PasswordBox()
password.setHeader("Password")
password.setPlaceholderText("Enter password")
layout.addWidget(password)

number = NumberBox()
number.setHeader("Quantity")
number.setRange(0.0, 10.0)
number.setValue(1.0)
number.setSpinButtonPlacementMode(
    NumberBox.SpinButtonPlacementMode.Inline
)
layout.addWidget(number)
layout.addStretch()


def toggle_theme():
    dark = fluentqt.current_theme() != fluentqt.Theme.Dark
    fluentqt.set_theme(fluentqt.Theme.Dark if dark else fluentqt.Theme.Light)
    theme_button.setText("Switch to Light" if dark else "Switch to Dark")


styles = [
    ("Fluent", fluentqt.StyleTheme.Fluent),
    ("Material", fluentqt.StyleTheme.Material),
    ("macOS", fluentqt.StyleTheme.MacOS),
]
style_index = 0
click_count = 0


def count_repeat_click():
    global click_count
    click_count += 1
    repeat_count.setText(f"Clicks: {click_count}")


def cycle_style():
    global style_index
    style_index = (style_index + 1) % len(styles)
    name, style = styles[style_index]
    fluentqt.apply_style_theme(style)
    style_button.setText(f"Style: {name}")


theme_button.clicked.connect(toggle_theme)
style_button.clicked.connect(cycle_style)
repeat_button.clicked.connect(count_repeat_click)
check_box.toggled.connect(shimmer.setAnimationEnabled)

window.setContentWidget(content)
window.show()

sys.exit(app.exec())
