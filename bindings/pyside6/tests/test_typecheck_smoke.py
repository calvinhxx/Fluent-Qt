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
assert_type(fluentqt.current_theme(), fluentqt.Theme)
assert_type(fluentqt.current_design_language(), fluentqt.DesignLanguage)
assert_type(fluentqt.accent_color(), QColor)
assert_type(fluentqt.font_for_role(fluentqt.FontRole.Body), QFont)
assert_type(fluentqt.font_scale(), float)
assert_type(fluentqt.theme_revision(), int)

button = Button("Typed button")
button.setFluentStyle(Button.ButtonStyle.Accent)

scroll_view = ScrollView()
scroll_view.setOwnedContentWidget(QWidget())

window = Window()
assert_type(window.titleBar(), TitleBar)
assert_type(window.backdropState(), BackdropState)
