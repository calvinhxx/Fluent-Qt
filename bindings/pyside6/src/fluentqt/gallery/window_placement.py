"""Restore and persist the Python Gallery window like the C++ application."""

from __future__ import annotations

import math

from PySide6.QtCore import QEvent, QObject, QPoint, QRect, QSize, Qt, QTimer
from PySide6.QtGui import QGuiApplication, QScreen
from PySide6.QtWidgets import QApplication, QWidget

from .settings import GallerySettings
from .visual import _single_shot


MIN_WIDTH = 460
MIN_HEIGHT = 500
INITIAL_WIDTH = 1180
INITIAL_HEIGHT = 760
INITIAL_WIDTH_PERCENT = 72
INITIAL_HEIGHT_PERCENT = 78
INITIAL_MIN_WIDTH = 900
INITIAL_MIN_HEIGHT = 600
INITIAL_MAX_WIDTH = 1440
INITIAL_MAX_HEIGHT = 900
LEFT_PANEL_RESERVE = 160


def _qround(value: float) -> int:
    return (
        math.floor(value + 0.5)
        if value >= 0.0
        else math.ceil(value - 0.5)
    )


def _proportional_extent(
    available: int,
    percentage: int,
    preferred_minimum: int,
    preferred_maximum: int,
) -> int:
    if available <= 0:
        return 0
    lower = min(available, preferred_minimum)
    upper = min(available, preferred_maximum)
    return max(
        lower,
        min(_qround(available * percentage / 100.0), upper),
    )


def effective_minimum_size(available_size: QSize) -> QSize:
    if not available_size.isValid() or available_size.isEmpty():
        return QSize(MIN_WIDTH, MIN_HEIGHT)
    return QSize(
        min(MIN_WIDTH, available_size.width()),
        min(MIN_HEIGHT, available_size.height()),
    )


def recommended_initial_size(available_size: QSize) -> QSize:
    if not available_size.isValid() or available_size.isEmpty():
        return QSize(INITIAL_WIDTH, INITIAL_HEIGHT)
    return QSize(
        _proportional_extent(
            available_size.width(),
            INITIAL_WIDTH_PERCENT,
            INITIAL_MIN_WIDTH,
            INITIAL_MAX_WIDTH,
        ),
        _proportional_extent(
            available_size.height(),
            INITIAL_HEIGHT_PERCENT,
            INITIAL_MIN_HEIGHT,
            INITIAL_MAX_HEIGHT,
        ),
    )


def constrain_geometry(
    requested: QRect,
    available: QRect,
    minimum_size: QSize,
) -> QRect:
    if not available.isValid() or available.isEmpty():
        return QRect(requested)
    effective_minimum = QSize(
        min(max(1, minimum_size.width()), available.width()),
        min(max(1, minimum_size.height()), available.height()),
    )
    size = requested.size()
    if not size.isValid() or size.isEmpty():
        size = effective_minimum
    size.setWidth(
        max(
            effective_minimum.width(),
            min(size.width(), available.width()),
        )
    )
    size.setHeight(
        max(
            effective_minimum.height(),
            min(size.height(), available.height()),
        )
    )
    maximum_x = available.right() - size.width() + 1
    maximum_y = available.bottom() - size.height() + 1
    return QRect(
        QPoint(
            max(available.left(), min(requested.x(), maximum_x)),
            max(available.top(), min(requested.y(), maximum_y)),
        ),
        size,
    )


def _centered_initial_geometry(available: QRect, size: QSize) -> QRect:
    horizontal_slack = max(0, available.width() - size.width())
    vertical_slack = max(0, available.height() - size.height())
    left_reserve = min(LEFT_PANEL_RESERVE, horizontal_slack)
    return QRect(
        QPoint(
            available.x()
            + left_reserve
            + (horizontal_slack - left_reserve) // 2,
            available.y() + vertical_slack // 2,
        ),
        size,
    )


def restored_geometry(
    saved_geometry: QRect,
    available: QRect,
    minimum_size: QSize,
) -> QRect:
    saved_size_is_usable = (
        saved_geometry.width() >= minimum_size.width()
        and saved_geometry.height() >= minimum_size.height()
    )
    if (
        saved_geometry.isValid()
        and not saved_geometry.isEmpty()
        and saved_size_is_usable
    ):
        return constrain_geometry(saved_geometry, available, minimum_size)
    return _centered_initial_geometry(
        available,
        recommended_initial_size(available.size()),
    )


class GalleryWindowPlacement(QObject):
    """Track the same normal geometry, screen, and maximized state as C++."""

    def __init__(
        self,
        window: QWidget,
        settings: GallerySettings,
        parent: QObject | None = None,
    ) -> None:
        super().__init__(parent)
        self._window = window
        self._settings = settings
        self._last_normal_geometry = QRect()
        self._screen_tracking_connected = False
        self._restoring = False
        self._save_timer = QTimer(self)
        self._save_timer.setSingleShot(True)
        self._save_timer.setInterval(250)
        self._save_timer.timeout.connect(self.save_now)
        window.installEventFilter(self)
        app = QApplication.instance()
        if app is not None:
            app.aboutToQuit.connect(self.save_now)

    def _restored_screen(self) -> QScreen | None:
        preferred_name = self._settings.window_screen_name
        for screen in QGuiApplication.screens():
            if preferred_name and screen.name() == preferred_name:
                return screen
        saved = self._settings.window_normal_geometry
        if saved.isValid():
            screen = QGuiApplication.screenAt(saved.center())
            if screen is not None:
                return screen
        return QGuiApplication.primaryScreen()

    def restore(self) -> bool:
        self._restoring = True
        screen = self._restored_screen()
        if screen is None:
            self._restoring = False
            return self._settings.window_maximized
        available = screen.availableGeometry()
        minimum = effective_minimum_size(available.size())
        self._window.setMinimumSize(minimum)
        target = restored_geometry(
            self._settings.window_normal_geometry,
            available,
            minimum,
        )
        self._window.setGeometry(target)
        self._last_normal_geometry = QRect(target)
        self._restoring = False
        return self._settings.window_maximized

    def save_now(self) -> None:
        if self._restoring:
            return
        state = self._window.windowState()
        if not (state & Qt.WindowMinimized or state & Qt.WindowFullScreen):
            candidate = (
                self._window.normalGeometry()
                if state & Qt.WindowMaximized
                else self._window.geometry()
            )
            if candidate.isValid() and not candidate.isEmpty():
                self._last_normal_geometry = QRect(candidate)
        if (
            not self._last_normal_geometry.isValid()
            or self._last_normal_geometry.isEmpty()
        ):
            self._last_normal_geometry = self._window.geometry()
        screen = self._window.screen()
        if screen is None:
            screen = QGuiApplication.screenAt(
                self._last_normal_geometry.center()
            )
        if screen is None:
            screen = QGuiApplication.primaryScreen()
        self._settings.set_window_placement(
            self._last_normal_geometry,
            screen.name() if screen is not None else "",
            bool(state & Qt.WindowMaximized),
        )

    def _connect_screen_tracking(self) -> None:
        handle = self._window.windowHandle()
        if self._screen_tracking_connected or handle is None:
            return
        self._screen_tracking_connected = True
        handle.screenChanged.connect(
            lambda screen: self._apply_screen_constraints(screen, True)
        )

    def _apply_screen_constraints(
        self,
        screen: QScreen | None,
        constrain_window: bool,
    ) -> None:
        if screen is None:
            screen = QGuiApplication.primaryScreen()
        if screen is None:
            return
        available = screen.availableGeometry()
        minimum = effective_minimum_size(available.size())
        self._window.setMinimumSize(minimum)
        state = self._window.windowState()
        if (
            not constrain_window
            or state & Qt.WindowMaximized
            or state & Qt.WindowMinimized
            or state & Qt.WindowFullScreen
        ):
            self._schedule_save()
            return
        constrained = constrain_geometry(
            self._window.geometry(), available, minimum
        )
        if constrained != self._window.geometry():
            self._restoring = True
            self._window.setGeometry(constrained)
            self._last_normal_geometry = QRect(constrained)
            self._restoring = False
        self._schedule_save()

    def _schedule_save(self) -> None:
        if not self._restoring:
            self._save_timer.start()

    def eventFilter(self, watched, event) -> bool:
        if watched is not self._window:
            return super().eventFilter(watched, event)
        event_type = event.type()
        if event_type == QEvent.Type.Show:
            self._connect_screen_tracking()
            self._schedule_save()
        elif event_type in (
            QEvent.Type.Move,
            QEvent.Type.Resize,
            QEvent.Type.WindowStateChange,
        ):
            self._schedule_save()
        elif event_type in tuple(
            value
            for value in (
                getattr(QEvent.Type, "ScreenChangeInternal", None),
                getattr(QEvent.Type, "DevicePixelRatioChange", None),
            )
            if value is not None
        ):
            _single_shot(
                0,
                self,
                lambda: self._apply_screen_constraints(
                    self._window.screen(), True
                ),
            )
        return super().eventFilter(watched, event)


__all__ = [
    "GalleryWindowPlacement",
    "constrain_geometry",
    "effective_minimum_size",
    "recommended_initial_size",
    "restored_geometry",
]
