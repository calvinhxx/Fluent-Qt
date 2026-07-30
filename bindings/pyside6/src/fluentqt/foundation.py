"""Theme and typography helpers for FluentQt bindings."""

from . import _fluentqt as _native
from ._fluentqt import (
    FontRole,
    accentColor,
    applyStyleTheme,
    currentDesignLanguage,
    currentTheme,
    fontScale,
    fontForRole,
    resetThemeTokens,
    setAccentColor,
    setFontScale,
    setTheme,
    themeRevision,
)

StyleTheme = _native.fluent.StyleTheme
Theme = _native.fluent.Theme
DesignLanguage = _native.fluent.DesignLanguage


def font_for_role(role=FontRole.Body):
    """Return the Fluent application font for a typography role."""
    return fontForRole(role)


def set_theme(theme):
    """Set the global Light or Dark visual theme."""
    setTheme(theme)


def current_theme():
    """Return the active global Light or Dark visual theme."""
    return currentTheme()


def apply_style_theme(style_theme):
    """Install a Fluent, Material, or macOS design-token preset."""
    applyStyleTheme(style_theme)


def set_accent_color(color):
    """Apply an in-memory accent color override."""
    setAccentColor(color)


def accent_color():
    """Return the active Light or Dark accent color."""
    return accentColor()


def reset_theme_tokens():
    """Restore built-in Fluent tokens without changing Light or Dark mode."""
    resetThemeTokens()


def set_font_scale(scale):
    """Set the runtime Fluent typography scale."""
    setFontScale(scale)


def font_scale():
    """Return the runtime Fluent typography scale."""
    return fontScale()


def current_design_language():
    """Return the structural design language selected by the style theme."""
    return currentDesignLanguage()


def theme_revision():
    """Return the token-registry revision counter."""
    return themeRevision()


__all__ = [
    "DesignLanguage",
    "FontRole",
    "StyleTheme",
    "Theme",
    "accent_color",
    "accentColor",
    "apply_style_theme",
    "applyStyleTheme",
    "current_design_language",
    "current_theme",
    "currentDesignLanguage",
    "currentTheme",
    "font_scale",
    "font_for_role",
    "fontScale",
    "fontForRole",
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
