"""Static consumer contract for the generated FluentQt type stubs.

This file is checked by mypy in CI; it is not executed as a runtime test.
"""

from typing_extensions import assert_type

from PySide6.QtGui import QColor, QFont
from PySide6.QtWidgets import QWidget

import fluentqt
from fluentqt.basicinput import Button
from fluentqt.scrolling import ScrollView
from fluentqt.windowing import BackdropState, TitleBar, Window


fluentqt.prepare_high_dpi_application()
assert_type(fluentqt.__version__, str)
assert_type(fluentqt.__api_version__, str)
assert_type(fluentqt.initialize_resources(), bool)
assert_type(fluentqt.binding_build_info(), dict[str, object])
assert_type(fluentqt.inspect_widget(QWidget()), dict[str, object])
assert_type(fluentqt.current_theme(), fluentqt.Theme)
assert_type(fluentqt.accent_color(), QColor)
assert_type(fluentqt.font_for_role(fluentqt.FontRole.Body), QFont)
assert_type(fluentqt.font_scale(), float)
assert_type(fluentqt.theme_revision(), int)

fluent_widget = fluentqt.FluentWidget()
assert_type(fluent_widget.effective_theme(), fluentqt.Theme)
assert_type(fluent_widget.theme_font(), QFont)
assert_type(fluent_widget.theme_tokens(), fluentqt.ThemeTokens)
assert_type(fluentqt.Icons.Add, str)
assert_type(fluentqt.Typography.Icons.Add, str)
assert_type(fluentqt.IconSize.Standard, int)
assert_type(fluentqt.Spacing.Border.Focused, int)
assert_type(fluentqt.CornerRadius.Control, int)

button = Button("Typed button")
button.setFluentStyle(Button.ButtonStyle.Accent)
assert_type(fluentqt.bind(button, "enabled", fluent_widget, "enabled"), None)

states = fluentqt.StateGroup()
assert_type(
    states.add("disabled", {button: {"enabled": False}}),
    fluentqt.StateGroup,
)
anchor_spec = fluentqt.anchors(center_in=fluent_widget)
assert_type(anchor_spec, fluentqt.AnchorSpec)
anchor_layout = fluentqt.AnchorLayout(fluent_widget)
anchor_layout.addWidget(button, anchor_spec)

scroll_view = ScrollView()
scroll_view.setOwnedContentWidget(QWidget())

window = Window()
assert_type(window.titleBar(), TitleBar)
assert_type(window.backdropState(), BackdropState)
