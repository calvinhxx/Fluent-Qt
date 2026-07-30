"""PySide6 bindings for FluentQt.

Importing this package does not create a QApplication or change global theme
state. Call ``prepare_high_dpi_application()`` before constructing the
application and ``initialize_resources()`` afterwards.
"""

from PySide6 import QtCore as _QtCore
from PySide6 import QtGui as _QtGui
from PySide6 import QtWidgets as _QtWidgets

from ._fluentqt import (
    bindingBuildInfo,
    initializeResources,
    prepareHighDpiApplication,
)
from .basicinput import (
    Button,
    CheckBox,
    ColorPicker,
    CompoundButton,
    HyperlinkButton,
    RadioButton,
    RatingControl,
    RepeatButton,
    Slider,
    ToggleButton,
    ToggleSwitch,
)
from .collections import StackView
from .date_time import CalendarView
from .foundation import (
    DesignLanguage,
    FontIcon,
    FontRole,
    StyleTheme,
    Theme,
    accent_color,
    accentColor,
    apply_style_theme,
    applyStyleTheme,
    current_design_language,
    current_theme,
    currentDesignLanguage,
    currentTheme,
    font_scale,
    font_for_role,
    fontScale,
    fontForRole,
    reset_theme_tokens,
    resetThemeTokens,
    set_accent_color,
    set_font_scale,
    set_theme,
    setAccentColor,
    setFontScale,
    setTheme,
    theme_revision,
    themeRevision,
)
from .layout import Accordion, Card, Divider, Expander
from .scrolling import (
    AnnotatedScrollBar,
    AnnotatedScrollBarLabel,
    PipsPager,
    ScrollBar,
    ScrollView,
    WidgetOwnership,
)
from .status_info import (
    Avatar,
    InfoBadge,
    InfoBar,
    ProgressBar,
    ProgressRing,
    Shimmer,
)
from .textfields import Label, LineEdit, NumberBox, PasswordBox, TextEdit
from .windowing import (
    BackdropBackend,
    BackdropCapabilities,
    BackdropEffect,
    BackdropFidelity,
    BackdropState,
    BackdropSurfaceMode,
    Window,
)


def prepare_high_dpi_application():
    """Apply FluentQt's pre-QApplication high-DPI settings."""
    prepareHighDpiApplication()


def initialize_resources():
    """Initialize the fonts, icons, and other compiled FluentQt resources."""
    return initializeResources()


def binding_build_info():
    """Return the FluentQt and Qt versions used to compile this extension."""
    return dict(bindingBuildInfo())


__all__ = [
    "Accordion",
    "BackdropBackend",
    "BackdropCapabilities",
    "BackdropEffect",
    "BackdropFidelity",
    "BackdropState",
    "BackdropSurfaceMode",
    "AnnotatedScrollBar",
    "AnnotatedScrollBarLabel",
    "Avatar",
    "Button",
    "CalendarView",
    "Card",
    "CheckBox",
    "ColorPicker",
    "CompoundButton",
    "DesignLanguage",
    "Divider",
    "Expander",
    "FontIcon",
    "FontRole",
    "HyperlinkButton",
    "InfoBadge",
    "InfoBar",
    "Label",
    "LineEdit",
    "NumberBox",
    "PasswordBox",
    "TextEdit",
    "PipsPager",
    "ProgressBar",
    "ProgressRing",
    "RadioButton",
    "RatingControl",
    "RepeatButton",
    "ScrollBar",
    "ScrollView",
    "Slider",
    "Shimmer",
    "StyleTheme",
    "StackView",
    "Theme",
    "ToggleButton",
    "ToggleSwitch",
    "Window",
    "WidgetOwnership",
    "accent_color",
    "accentColor",
    "apply_style_theme",
    "applyStyleTheme",
    "binding_build_info",
    "bindingBuildInfo",
    "current_design_language",
    "current_theme",
    "currentDesignLanguage",
    "currentTheme",
    "font_scale",
    "font_for_role",
    "fontScale",
    "fontForRole",
    "initialize_resources",
    "initializeResources",
    "prepare_high_dpi_application",
    "prepareHighDpiApplication",
    "reset_theme_tokens",
    "resetThemeTokens",
    "set_accent_color",
    "set_font_scale",
    "set_theme",
    "setAccentColor",
    "setFontScale",
    "setTheme",
    "theme_revision",
    "themeRevision",
]
