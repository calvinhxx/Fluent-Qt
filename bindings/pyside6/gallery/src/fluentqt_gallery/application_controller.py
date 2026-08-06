"""Standalone application close behavior matching the C++ Gallery."""

from __future__ import annotations

import sys

import fluentqt
from PySide6.QtCore import QEvent, QObject, QRectF, Qt
from PySide6.QtGui import QColor, QCloseEvent, QFont, QMouseEvent, QPainter
from PySide6.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QMenu,
    QSizePolicy,
    QSystemTrayIcon,
    QVBoxLayout,
    QWidget,
)

from .foundation_pages import _theme_tokens
from .settings import CloseBehavior, GallerySettings
from .visual import _single_shot, app_icon, css_color


def status_area_name() -> str:
    return "menu bar" if sys.platform == "darwin" else "system tray"


def keep_running_choice() -> str:
    return "Keep in {0}".format(status_area_name())


def minimize_description() -> str:
    if sys.platform == "darwin":
        return "Keep it available from the Dock."
    return "Keep it available from the taskbar."


def keep_running_description() -> str:
    return "Reopen it from the {0} icon.".format(status_area_name())


def _application_is_saving_session(app: QApplication | None) -> bool:
    if app is None:
        return False
    is_saving_session = getattr(app, "isSavingSession", None)
    return bool(is_saving_session()) if callable(is_saving_session) else False


def _is_wayland_platform() -> bool:
    return QApplication.platformName().casefold().startswith("wayland")


def _window_is_active(window: QWidget | None) -> bool:
    return bool(window is not None and window.isActiveWindow())


def _request_application_attention(window: QWidget) -> None:
    app = QApplication.instance()
    if app is not None:
        app.alert(window, 3000)


class _CloseBehaviorChoiceRow(QWidget):
    def __init__(
        self,
        behavior: CloseBehavior,
        glyph: str,
        title: str,
        description: str,
        parent: QWidget,
    ) -> None:
        super().__init__(parent)
        self.behavior = behavior
        self._glyph = glyph
        self._selected = False
        self._hovered = False
        self._pressed = False
        self._on_activate = None
        self.setObjectName("galleryCloseBehaviorRow{0}".format(int(behavior)))
        self.setAccessibleName(title)
        self.setAccessibleDescription(description)
        self.setCursor(Qt.PointingHandCursor)
        self.setMouseTracking(True)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        self.setFixedHeight(42)

        layout = QHBoxLayout(self)
        layout.setContentsMargins(8, 0, 8, 0)
        layout.setSpacing(8)
        self._icon_slot = QWidget(self)
        self._icon_slot.setFixedSize(16, 16)
        self._icon_slot.setAttribute(Qt.WA_TransparentForMouseEvents)
        layout.addWidget(self._icon_slot, 0, Qt.AlignVCenter)
        text_layout = QVBoxLayout()
        text_layout.setContentsMargins(0, 3, 0, 3)
        text_layout.setSpacing(0)
        self._title = fluentqt.Label(title, self)
        self._title.setAttribute(Qt.WA_TransparentForMouseEvents)
        self._title.setFluentTypography(fluentqt.FontRole.BodyStrong)
        self._description = fluentqt.Label(description, self)
        self._description.setAttribute(Qt.WA_TransparentForMouseEvents)
        self._description.setFluentTypography(fluentqt.FontRole.Caption)
        self._description.setWordWrap(False)
        text_layout.addWidget(self._title)
        text_layout.addWidget(self._description)
        layout.addLayout(text_layout, 1)
        self.refresh_theme()

    def set_selected(self, selected: bool) -> None:
        selected = bool(selected)
        if self._selected == selected:
            return
        self._selected = selected
        self.update()

    def set_on_activate(self, callback) -> None:
        self._on_activate = callback

    def refresh_theme(self) -> None:
        colors = _theme_tokens()
        self._description.setStyleSheet(
            "color: {0}; background: transparent;".format(
                css_color(colors["textSecondary"])
            )
        )
        self.update()

    def paintEvent(self, event) -> None:
        del event
        colors = _theme_tokens()
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)
        row_rect = QRectF(self.rect()).adjusted(0.5, 0.5, -0.5, -0.5)
        fill = QColor(Qt.transparent)
        if self._selected:
            fill = QColor(colors["accentDefault"])
            fill.setAlpha(
                28 if fluentqt.current_theme() == fluentqt.Theme.Dark else 16
            )
        elif self._pressed:
            fill = QColor(colors["subtleTertiary"])
        elif self._hovered:
            fill = QColor(colors["subtleSecondary"])
        painter.setPen(Qt.NoPen)
        painter.setBrush(fill)
        painter.drawRoundedRect(row_rect, 6, 6)
        icon_font = QFont("FluentQt Icons")
        icon_font.setPixelSize(12)
        painter.setFont(icon_font)
        painter.setPen(
            colors["textAccentPrimary"]
            if self._selected
            else colors["textSecondary"]
        )
        painter.drawText(
            QRectF(self._icon_slot.geometry()), Qt.AlignCenter, self._glyph
        )

    def enterEvent(self, event) -> None:
        super().enterEvent(event)
        self._hovered = True
        self.update()

    def leaveEvent(self, event) -> None:
        super().leaveEvent(event)
        self._hovered = False
        self._pressed = False
        self.update()

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.LeftButton:
            self._pressed = True
            self.update()
            event.accept()
            return
        super().mousePressEvent(event)

    def mouseReleaseEvent(self, event: QMouseEvent) -> None:
        activate = (
            event.button() == Qt.LeftButton
            and self._pressed
            and self.rect().contains(event.position().toPoint())
        )
        self._pressed = False
        self.update()
        if activate:
            if self._on_activate is not None:
                self._on_activate()
            event.accept()
            return
        super().mouseReleaseEvent(event)


class CloseBehaviorPromptContent(QWidget):
    def __init__(
        self,
        current: CloseBehavior,
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self.setObjectName("galleryCloseBehaviorPromptContent")
        self.setFixedWidth(300)
        self._selected = CloseBehavior(current)
        self.rows: list[_CloseBehaviorChoiceRow] = []
        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(4)
        self._add_choice(
            root,
            CloseBehavior.Minimize,
            "\ue921",
            "Minimize window",
            minimize_description(),
        )
        self._add_choice(
            root,
            CloseBehavior.Tray,
            "\ued1a",
            keep_running_choice(),
            keep_running_description(),
        )
        self._add_choice(
            root,
            CloseBehavior.Quit,
            "\ue7e8",
            "Quit the app",
            "Stop the Gallery completely.",
        )
        self.setFixedHeight(42 * 3 + 4 * 2)
        self._sync_selection()

    @property
    def selected_behavior(self) -> CloseBehavior:
        return self._selected

    def _add_choice(
        self,
        root: QVBoxLayout,
        behavior: CloseBehavior,
        glyph: str,
        title: str,
        description: str,
    ) -> None:
        row = _CloseBehaviorChoiceRow(
            behavior, glyph, title, description, self
        )
        row.set_on_activate(lambda value=behavior: self._select(value))
        self.rows.append(row)
        root.addWidget(row)

    def _select(self, behavior: CloseBehavior) -> None:
        self._selected = behavior
        self._sync_selection()

    def _sync_selection(self) -> None:
        for row in self.rows:
            row.set_selected(row.behavior == self._selected)


class GalleryApplicationController(QObject):
    """Own the first-close prompt, status icon, and restore behavior."""

    def __init__(
        self,
        window,
        settings: GallerySettings,
        parent: QObject | None = None,
        *,
        setup_status_item: bool = True,
    ) -> None:
        super().__init__(parent)
        self.setObjectName("galleryApplicationController")
        self._window = window
        self._settings = settings
        self._status_icon: QSystemTrayIcon | None = None
        self._status_menu: QMenu | None = None
        self._status_area_available = False
        self._restore_state = Qt.WindowNoState
        self._exit_requested = False
        self._close_request_scheduled = False
        self._close_prompt_open = False
        self._restore_generation = 0
        self._close_dialog = None
        self._close_content = None
        window.installEventFilter(self)
        app = QApplication.instance()
        if app is not None:
            app.installEventFilter(self)
        if setup_status_item:
            self._setup_status_item()

    def _setup_status_item(self) -> None:
        self._status_area_available = QSystemTrayIcon.isSystemTrayAvailable()
        icon = QSystemTrayIcon(app_icon(), self)
        icon.setObjectName("galleryStatusAreaIcon")
        icon.setToolTip("Fluent-Qt Gallery")
        menu = QMenu()
        menu.setObjectName("galleryStatusAreaMenu")
        open_action = menu.addAction("Open Fluent-Qt Gallery")
        settings_action = menu.addAction("Settings")
        menu.addSeparator()
        quit_action = menu.addAction("Quit Fluent-Qt Gallery")
        open_action.setObjectName("galleryStatusOpenAction")
        settings_action.setObjectName("galleryStatusSettingsAction")
        quit_action.setObjectName("galleryStatusQuitAction")
        open_action.triggered.connect(self.restore_window)
        settings_action.triggered.connect(self.open_settings)
        quit_action.triggered.connect(self.request_quit)
        icon.activated.connect(self._status_icon_activated)
        icon.setContextMenu(menu)
        icon.show()
        self._status_icon = icon
        self._status_menu = menu

    def _status_icon_activated(self, reason) -> None:
        if reason in (
            QSystemTrayIcon.ActivationReason.Trigger,
            QSystemTrayIcon.ActivationReason.DoubleClick,
        ):
            self.restore_window()

    def eventFilter(self, watched, event) -> bool:
        app = QApplication.instance()
        if watched is app and event.type() == QEvent.Type.Quit:
            self.arm_application_quit()
            return False
        if (
            watched is self._window
            and event.type() == QEvent.Type.Close
            and not self._exit_requested
        ):
            if _application_is_saving_session(app):
                return super().eventFilter(watched, event)
            close_event = event
            if isinstance(close_event, QCloseEvent):
                close_event.ignore()
            if not self._close_request_scheduled:
                self._close_request_scheduled = True
                _single_shot(0, self, self._handle_close_requested)
            return True
        if (
            sys.platform == "darwin"
            and watched is app
            and event.type() == QEvent.Type.ApplicationActivate
            and not self._window.isVisible()
            and not self._exit_requested
            and not self._close_prompt_open
        ):
            _single_shot(0, self, self.restore_window)
        return super().eventFilter(watched, event)

    def _handle_close_requested(self) -> None:
        self._close_request_scheduled = False
        if self._exit_requested or self._close_prompt_open:
            return
        if not self._settings.close_behavior_confirmed:
            self._show_close_behavior_dialog()
        else:
            self._apply_configured_close_behavior()

    def _show_close_behavior_dialog(self) -> None:
        if self._close_prompt_open:
            return
        self._close_prompt_open = True
        content = CloseBehaviorPromptContent(self._settings.close_behavior)
        dialog = fluentqt.ContentDialog(self._window)
        dialog.setObjectName("galleryCloseBehaviorDialog")
        dialog.setWindowTitle("Close behavior")
        dialog.setTitle("Close behavior")
        dialog.setContent(content)
        dialog.setPrimaryButtonText("Save")
        dialog.setCloseButtonText("Cancel")
        dialog.setDefaultButton(fluentqt.ContentDialogButton.Primary)
        dialog.setButtonBarHeight(52)
        dialog.setWindowModality(Qt.ApplicationModal)
        dialog.resize(380, 288)
        restore_chrome_interactive = self._window.isChromeInteractive()
        self._window.setChromeInteractive(False)
        self._close_dialog = dialog
        self._close_content = content

        def finished(result: int) -> None:
            self._close_prompt_open = False
            self._window.setChromeInteractive(restore_chrome_interactive)
            if result == fluentqt.ContentDialog.ResultPrimary:
                self._settings.set_close_behavior(
                    content.selected_behavior
                )
                self._settings.set_close_behavior_confirmed(True)
                self._apply_configured_close_behavior()
            self._close_dialog = None
            self._close_content = None
            dialog.deleteLater()

        dialog.finished.connect(finished)
        dialog.open()

    def _capture_restore_state(self) -> None:
        state = self._window.windowState()
        state &= ~Qt.WindowMinimized
        state &= ~Qt.WindowActive
        self._restore_state = state

    def _apply_configured_close_behavior(self) -> None:
        behavior = self._settings.close_behavior
        if behavior == CloseBehavior.Minimize:
            self._capture_restore_state()
            self._window.showMinimized()
        elif behavior == CloseBehavior.Tray:
            self._capture_restore_state()
            if self._status_area_available and self._status_icon is not None:
                self._window.hide()
            else:
                self._window.showMinimized()
        else:
            self.request_quit()

    def restore_window(self) -> None:
        if self._exit_requested:
            return
        was_hidden = not self._window.isVisible()
        was_minimized = bool(
            self._window.windowState() & Qt.WindowMinimized
        )
        was_active = _window_is_active(self._window)
        wayland_state_fallback = _is_wayland_platform() and not was_active
        needs_restore = was_hidden or was_minimized or wayland_state_fallback
        self._restore_generation += 1
        generation = self._restore_generation
        if needs_restore:
            target_state = (
                self._restore_state
                if was_hidden
                else self._window.windowState()
            )
            target_state &= ~Qt.WindowMinimized
            target_state &= ~Qt.WindowActive
            if sys.platform.startswith("linux"):
                self._window.hide()
                self._window.prepareForNativeRestore()
                delay = 100 if was_minimized or wayland_state_fallback else 0
                _single_shot(
                    delay,
                    self,
                    lambda: self._show_restored_window(
                        generation,
                        target_state,
                        was_hidden,
                        was_minimized,
                    ),
                )
            else:
                self._show_restored_window(
                    generation, target_state, was_hidden, was_minimized
                )
        else:
            self._complete_window_restore(generation, False)
        _single_shot(
            250,
            self,
            lambda: self._request_foreground_attention(generation),
        )

    def _request_foreground_attention(self, generation: int) -> None:
        if (
            generation != self._restore_generation
            or self._exit_requested
            or _window_is_active(self._window)
        ):
            return
        _request_application_attention(self._window)

    def _show_restored_window(
        self,
        generation: int,
        target_state,
        was_hidden: bool,
        was_minimized: bool,
    ) -> None:
        if generation != self._restore_generation or self._exit_requested:
            return
        if target_state & Qt.WindowFullScreen:
            self._window.showFullScreen()
        elif target_state & Qt.WindowMaximized:
            self._window.showMaximized()
        elif was_hidden and not was_minimized:
            self._window.show()
        else:
            self._window.showNormal()
        _single_shot(
            0,
            self,
            lambda: self._complete_window_restore(generation, True),
        )
        _single_shot(
            80,
            self,
            lambda: self._complete_window_restore(generation, False),
        )

    def _complete_window_restore(
        self, generation: int, refresh_native_frame: bool
    ) -> None:
        if generation != self._restore_generation or self._exit_requested:
            return
        if refresh_native_frame:
            self._window.reapplySystemBackdrop()
        self._window.requestForegroundActivation()

    def open_settings(self) -> None:
        self.restore_window()
        self._window.navigate("settings")

    def arm_application_quit(self) -> None:
        if self._exit_requested:
            return
        self._exit_requested = True
        if self._status_icon is not None:
            self._status_icon.hide()

    def request_quit(self) -> None:
        self.arm_application_quit()
        app = QApplication.instance()
        if app is not None:
            app.quit()


__all__ = [
    "CloseBehaviorPromptContent",
    "GalleryApplicationController",
    "keep_running_choice",
    "keep_running_description",
    "minimize_description",
    "status_area_name",
]
