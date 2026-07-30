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
    HyperlinkButton,
    RadioButton,
    RepeatButton,
    Slider,
    ToggleButton,
    ToggleSwitch,
)
from .foundation import (
    DesignLanguage,
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
from .layout import Divider
from .status_info import InfoBadge, ProgressBar, ProgressRing, Shimmer
from .textfields import Label, LineEdit, NumberBox, PasswordBox
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
    "BackdropBackend",
    "BackdropCapabilities",
    "BackdropEffect",
    "BackdropFidelity",
    "BackdropState",
    "BackdropSurfaceMode",
    "Button",
    "CheckBox",
    "DesignLanguage",
    "Divider",
    "FontRole",
    "HyperlinkButton",
    "InfoBadge",
    "Label",
    "LineEdit",
    "NumberBox",
    "PasswordBox",
    "ProgressBar",
    "ProgressRing",
    "RadioButton",
    "RepeatButton",
    "Slider",
    "Shimmer",
    "StyleTheme",
    "Theme",
    "ToggleButton",
    "ToggleSwitch",
    "Window",
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
