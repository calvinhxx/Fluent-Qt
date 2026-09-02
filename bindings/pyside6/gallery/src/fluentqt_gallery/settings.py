"""Standalone application settings matching the native C++ Gallery."""

from __future__ import annotations

from enum import IntEnum
from pathlib import Path
import sys

import fluentqt
from PySide6.QtCore import (
    QCoreApplication,
    QEvent,
    QObject,
    QRect,
    QSettings,
    QStandardPaths,
    QTimer,
    Qt,
    Signal,
)
from PySide6.QtGui import QColor, QGuiApplication
from PySide6.QtWidgets import QApplication

from .identity import APPLICATION_NAME, ORGANIZATION_NAME


class ThemeMode(IntEnum):
    System = 0
    Light = 1
    Dark = 2
    HighContrast = 3


MotionMode = fluentqt.MotionMode


class NavigationStyle(IntEnum):
    Auto = 0
    Left = 1
    LeftCompact = 2
    LeftMinimal = 3
    Top = 4


class CloseBehavior(IntEnum):
    Minimize = 0
    Tray = 1
    Quit = 2


_ACCENT_KEY = "appearance/accent/fluent"
_LEGACY_ACCENT_KEY = "settings/accent/0"


def persistence_available() -> bool:
    return (
        QCoreApplication.organizationName() == ORGANIZATION_NAME
        and QCoreApplication.applicationName() == APPLICATION_NAME
    )


def config_file_path() -> Path:
    return Path(
        QStandardPaths.writableLocation(
            QStandardPaths.StandardLocation.AppLocalDataLocation
        )
    ) / "config.ini"


def _config_settings() -> QSettings:
    return QSettings(str(config_file_path()), QSettings.IniFormat)


def _bounded(value: object, low: int, high: int, fallback: int) -> int:
    try:
        return max(low, min(int(value), high))
    except (TypeError, ValueError):
        return fallback


def _windows_apps_use_light_theme() -> bool | None:
    """Read the Windows app color preference used by the native Gallery."""

    if sys.platform != "win32":
        return None
    registry = QSettings(
        r"HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion"
        r"\Themes\Personalize",
        QSettings.NativeFormat,
    )
    key = "AppsUseLightTheme"
    if not registry.contains(key):
        return None
    try:
        return int(registry.value(key, 1)) != 0
    except (TypeError, ValueError):
        return None


def _system_theme() -> fluentqt.Theme:
    app = QApplication.instance()
    if app is not None:
        try:
            scheme = QGuiApplication.styleHints().colorScheme()
            if scheme == Qt.ColorScheme.Dark:
                return fluentqt.Theme.Dark
            if scheme == Qt.ColorScheme.Light:
                return fluentqt.Theme.Light
        except AttributeError:
            pass
        use_light_theme = _windows_apps_use_light_theme()
        if use_light_theme is not None:
            return (
                fluentqt.Theme.Light
                if use_light_theme
                else fluentqt.Theme.Dark
            )
        palette = app.palette()
        if (
            palette.window().color().lightness()
            < palette.windowText().color().lightness()
        ):
            return fluentqt.Theme.Dark
    return fluentqt.Theme.Light


class GallerySettings(QObject):
    themeModeChanged = Signal(int)
    motionModeChanged = Signal(int)
    accentColorChanged = Signal(QColor)
    navigationStyleChanged = Signal(int)
    windowEffectChanged = Signal(int)
    closeBehaviorChanged = Signal(int)

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self.theme_mode = ThemeMode.System
        self.motion_mode = MotionMode.Full
        self.navigation_style = NavigationStyle.Auto
        self.window_effect = 1
        self.close_behavior = CloseBehavior.Tray
        self.window_normal_geometry = QRect()
        self.window_screen_name = ""
        self.window_maximized = False
        self.close_behavior_confirmed = False
        self.intro_completed = False
        self._load()
        self.apply_motion_mode()
        self.apply_user_theme()
        self.apply_theme_mode()
        app = QApplication.instance()
        if app is not None:
            app.installEventFilter(self)
        self._system_theme_poll = QTimer(self)
        self._system_theme_poll.setInterval(1000)
        self._system_theme_poll.timeout.connect(self._poll_system_theme)
        self._system_theme_poll.start()

    def _load(self) -> None:
        if not persistence_available():
            return
        settings = _config_settings()
        self.theme_mode = ThemeMode(
            _bounded(settings.value("settings/themeMode", 0), 0, 3, 0)
        )
        self.motion_mode = MotionMode(
            _bounded(settings.value("settings/motionMode", 0), 0, 2, 0)
        )
        if settings.contains(_LEGACY_ACCENT_KEY) and not settings.contains(
            _ACCENT_KEY
        ):
            settings.setValue(
                _ACCENT_KEY, settings.value(_LEGACY_ACCENT_KEY, "")
            )
        settings.remove(_LEGACY_ACCENT_KEY)
        self.navigation_style = NavigationStyle(
            _bounded(
                settings.value("settings/navigationStyle", 0), 0, 4, 0
            )
        )
        self.window_effect = _bounded(
            settings.value("settings/windowEffect", 1), 0, 2, 1
        )
        self.close_behavior = CloseBehavior(
            _bounded(settings.value("settings/closeBehavior", 1), 0, 2, 1)
        )
        geometry = settings.value("window/normalGeometry", QRect())
        self.window_normal_geometry = (
            geometry if isinstance(geometry, QRect) else QRect()
        )
        self.window_screen_name = str(
            settings.value("window/screenName", "") or ""
        )
        self.window_maximized = bool(
            settings.value("window/maximized", False, type=bool)
        )
        self.close_behavior_confirmed = bool(
            settings.value(
                "settings/closeBehaviorConfirmed", False, type=bool
            )
        )
        self.intro_completed = bool(
            settings.value("intro/completed", False, type=bool)
        )

    def apply_user_theme(self) -> None:
        fluentqt.apply_user_theme()
        if persistence_available():
            value = _config_settings().value(_ACCENT_KEY, "")
            accent = QColor(str(value))
            if accent.isValid():
                fluentqt.set_accent_color(accent)

    def apply_theme_mode(self) -> None:
        theme = _system_theme()
        if self.theme_mode == ThemeMode.Light:
            theme = fluentqt.Theme.Light
        elif self.theme_mode == ThemeMode.Dark:
            theme = fluentqt.Theme.Dark
        elif self.theme_mode == ThemeMode.HighContrast:
            theme = fluentqt.Theme.HighContrast
        fluentqt.set_theme(theme)

    def apply_motion_mode(self) -> None:
        fluentqt.set_motion_mode(self.motion_mode)

    def set_theme_mode(self, mode: int | ThemeMode) -> None:
        next_mode = ThemeMode(_bounded(mode, 0, 3, 0))
        if self.theme_mode == next_mode:
            self.apply_theme_mode()
            return
        self.theme_mode = next_mode
        if persistence_available():
            _config_settings().setValue(
                "settings/themeMode", int(self.theme_mode)
            )
        self.apply_theme_mode()
        self.themeModeChanged.emit(int(self.theme_mode))

    def set_motion_mode(self, mode: int | MotionMode) -> None:
        next_mode = MotionMode(_bounded(mode, 0, 2, 0))
        if self.motion_mode == next_mode:
            return
        self.motion_mode = next_mode
        if persistence_available():
            _config_settings().setValue(
                "settings/motionMode", int(self.motion_mode)
            )
        self.apply_motion_mode()
        self.motionModeChanged.emit(int(self.motion_mode))

    def set_accent_color(self, accent: QColor) -> None:
        if not accent.isValid():
            return
        fluentqt.set_accent_color(accent)
        if persistence_available():
            _config_settings().setValue(
                _ACCENT_KEY, accent.name(QColor.HexArgb)
            )
        self.accentColorChanged.emit(QColor(fluentqt.accent_color()))

    def reset_accent_color(self) -> None:
        if persistence_available():
            _config_settings().remove(_ACCENT_KEY)
        fluentqt.apply_user_theme()
        self.accentColorChanged.emit(QColor(fluentqt.accent_color()))

    def set_navigation_style(
        self, style: int | NavigationStyle
    ) -> None:
        next_style = NavigationStyle(_bounded(style, 0, 4, 0))
        if self.navigation_style == next_style:
            return
        self.navigation_style = next_style
        if persistence_available():
            _config_settings().setValue(
                "settings/navigationStyle", int(self.navigation_style)
            )
        self.navigationStyleChanged.emit(int(self.navigation_style))

    def set_window_effect(self, effect: int) -> None:
        next_effect = _bounded(effect, 0, 2, 1)
        if self.window_effect == next_effect:
            return
        self.window_effect = next_effect
        if persistence_available():
            _config_settings().setValue(
                "settings/windowEffect", self.window_effect
            )
        self.windowEffectChanged.emit(self.window_effect)

    def set_close_behavior(self, behavior: int | CloseBehavior) -> None:
        next_behavior = CloseBehavior(_bounded(behavior, 0, 2, 1))
        if self.close_behavior == next_behavior:
            return
        self.close_behavior = next_behavior
        if persistence_available():
            _config_settings().setValue(
                "settings/closeBehavior", int(self.close_behavior)
            )
        self.closeBehaviorChanged.emit(int(self.close_behavior))

    def set_close_behavior_confirmed(self, confirmed: bool) -> None:
        confirmed = bool(confirmed)
        if self.close_behavior_confirmed == confirmed:
            return
        self.close_behavior_confirmed = confirmed
        if persistence_available():
            settings = _config_settings()
            settings.setValue(
                "settings/closeBehaviorConfirmed", confirmed
            )
            if confirmed:
                settings.setValue(
                    "settings/closeBehavior", int(self.close_behavior)
                )

    def set_intro_completed(self, completed: bool) -> None:
        completed = bool(completed)
        if self.intro_completed == completed:
            return
        self.intro_completed = completed
        if persistence_available():
            _config_settings().setValue("intro/completed", completed)

    def set_window_placement(
        self, geometry: QRect, screen_name: str, maximized: bool
    ) -> None:
        self.window_normal_geometry = QRect(geometry)
        self.window_screen_name = str(screen_name)
        self.window_maximized = bool(maximized)
        if not persistence_available():
            return
        settings = _config_settings()
        settings.setValue("window/normalGeometry", self.window_normal_geometry)
        settings.setValue("window/screenName", self.window_screen_name)
        settings.setValue("window/maximized", self.window_maximized)

    def _poll_system_theme(self) -> None:
        if self.theme_mode == ThemeMode.System:
            self.apply_theme_mode()

    def eventFilter(self, watched, event) -> bool:
        if (
            watched is QApplication.instance()
            and event is not None
            and event.type() == QEvent.Type.ApplicationPaletteChange
            and self.theme_mode == ThemeMode.System
        ):
            self.apply_theme_mode()
        return super().eventFilter(watched, event)


_SETTINGS: GallerySettings | None = None


def gallery_settings() -> GallerySettings:
    global _SETTINGS
    if _SETTINGS is None:
        _SETTINGS = GallerySettings(QApplication.instance())
    return _SETTINGS


__all__ = [
    "CloseBehavior",
    "GallerySettings",
    "MotionMode",
    "NavigationStyle",
    "ThemeMode",
    "config_file_path",
    "gallery_settings",
    "persistence_available",
]
