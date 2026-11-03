"""Browser-style zoom matching ``GalleryInterfaceZoomController``."""

from __future__ import annotations

from collections.abc import Callable, Sequence

import fluentqt
from fluentqt._qt_widgets import send_accessible_announcement
from PySide6.QtCore import QEvent, QObject, QPoint, QSize, Qt, QTimer, Slot
from PySide6.QtGui import QAction, QKeySequence
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget

from .interface_zoom import (
    DEFAULT_PERCENT,
    apply_interface_zoom,
    applied_zoom_percent,
    supports_live_interface_zoom,
    zoomed_in,
    zoomed_out,
)
from .metrics import TITLE_BAR_HEIGHT
from .settings import GallerySettings
from .window_placement import app_window_minimum_size

# Long enough to read the level, short enough to stay out of the way.
_BUBBLE_IDLE_CLOSE_MS = 2000
_BUBBLE_EDGE_MARGIN = 12
_BUBBLE_TITLE_BAR_GAP = 4


def _percent_text(percent: int) -> str:
    return "{0}%".format(percent)


class GalleryInterfaceZoomController(QObject):
    """Own browser-style zoom for one window: its commands and saved level.

    The native window keeps its size while its content relayouts at the new
    scale. Its logical minimum follows ``app_window_minimum_size()``, so the
    native minimum stays fixed. Like a browser, each zoom command shows a
    compact bubble below the trailing title bar with zoom out, the level, zoom
    in, and Reset; it closes shortly after the last change.
    """

    def __init__(self, window: QWidget, settings: GallerySettings) -> None:
        super().__init__(window)
        self.setObjectName("galleryInterfaceZoomController")
        self._window = window
        self._settings = settings
        # The application applies the saved zoom before creating its first window.
        self._applied_percent = applied_zoom_percent()
        window.setMinimumSize(self._minimum_size_at(self._applied_percent))
        self._commands: list[QAction] = []

        # Accept Ctrl/Cmd+= as well as Ctrl/Cmd++, so zooming in needs no Shift.
        self._add_command(
            "galleryZoomInAction",
            "Zoom In",
            (
                QKeySequence(Qt.CTRL | Qt.Key_Equal),
                QKeySequence(QKeySequence.StandardKey.ZoomIn),
            ),
            self.zoom_in,
        )
        self._add_command(
            "galleryZoomOutAction",
            "Zoom Out",
            (QKeySequence(QKeySequence.StandardKey.ZoomOut),),
            self.zoom_out,
        )
        self._add_command(
            "galleryActualSizeAction",
            "Actual Size",
            (QKeySequence(Qt.CTRL | Qt.Key_0),),
            self.reset_zoom,
        )

        self._bubble: fluentqt.Popup | None = None
        self._idle_timer = QTimer(self)
        self._idle_timer.setSingleShot(True)
        self._idle_timer.setInterval(_BUBBLE_IDLE_CLOSE_MS)
        self._idle_timer.timeout.connect(self._close_bubble_when_idle)
        # Keep the bubble at the trailing edge while the logical window size changes.
        window.installEventFilter(self)
        settings.interfaceZoomPercentChanged.connect(self._apply_saved_zoom)

    def commands(self) -> list[QAction]:
        """Zoom In, Zoom Out, and Actual Size commands, in menu order."""

        return list(self._commands)

    def _add_command(
        self,
        object_name: str,
        text: str,
        keys: Sequence[QKeySequence],
        command: Callable[[], None],
    ) -> None:
        action = QAction(text, self._window)
        action.setObjectName(object_name)
        action.setShortcuts(list(keys))
        action.setShortcutContext(Qt.WindowShortcut)
        action.triggered.connect(command)
        self._commands.append(action)

    @Slot()
    def zoom_in(self) -> None:
        self._change_zoom(zoomed_in(self._settings.interface_zoom_percent))

    @Slot()
    def zoom_out(self) -> None:
        self._change_zoom(zoomed_out(self._settings.interface_zoom_percent))

    @Slot()
    def reset_zoom(self) -> None:
        self._change_zoom(DEFAULT_PERCENT)

    def eventFilter(self, watched: QObject, event: QEvent) -> bool:
        if (
            watched is self._window
            and event.type() == QEvent.Type.Resize
            and self._bubble is not None
            and self._bubble.isOpen()
        ):
            self._place_bubble()
        return super().eventFilter(watched, event)

    def _change_zoom(self, percent: int) -> None:
        self._settings.set_interface_zoom_percent(percent)
        self._show_bubble()
        announcement = "Zoom " + self._level_label.text()
        if not self._restart_hint.isHidden():
            announcement += ". " + self._restart_hint.text()
        send_accessible_announcement(self._bubble, announcement)

    @Slot(int)
    def _apply_saved_zoom(self, percent: int) -> None:
        if percent != self._applied_percent and supports_live_interface_zoom():
            # Relax the logical minimum before rescaling and tighten it
            # afterwards, so neither scale can enlarge the native window.
            current_minimum = self._window.minimumSize()
            self._window.setMinimumSize(
                current_minimum.boundedTo(app_window_minimum_size(QSize(), percent))
            )
            if not apply_interface_zoom(percent / 100.0):
                self._window.setMinimumSize(current_minimum)
                return
            self._applied_percent = percent
            self._window.setMinimumSize(self._minimum_size_at(percent))
        if self._bubble is not None:
            self._sync_bubble()
            if self._bubble.isOpen():
                self._place_bubble()

    def _build_bubble(self) -> None:
        bubble = fluentqt.Popup(self._window)
        self._bubble = bubble
        bubble.setObjectName("galleryInterfaceZoomBubble")
        bubble.setAccessibleName("Zoom")
        # Preserve the editor's focus throughout opening, including validation
        # and editingFinished handlers, just like the C++ Gallery bubble.
        bubble._setFocusOnOpenEnabled(False)
        bubble.closed.connect(self._idle_timer.stop)

        column = QVBoxLayout(bubble)
        column.setContentsMargins(8, 6, 8, 6)
        column.setSpacing(4)
        row = QHBoxLayout()
        row.setSpacing(4)
        column.addLayout(row)

        def add_step_button(
            object_name: str, glyph: str, name: str, command: Callable[[], None]
        ) -> fluentqt.Button:
            button = fluentqt.Button(bubble)
            button.setObjectName(object_name)
            button.setFluentStyle(fluentqt.Button.ButtonStyle.Subtle)
            button.setFluentLayout(fluentqt.Button.ButtonLayout.IconOnly)
            button.setFluentSize(fluentqt.Button.ButtonSize.Small)
            button.setIconGlyph(glyph, fluentqt.IconSize.Standard)
            button.setAccessibleName(name)
            # Clicking keeps focus on the page, so only hover or keyboard
            # focus holds the bubble open.
            button.setFocusPolicy(Qt.TabFocus)
            button.clicked.connect(command)
            row.addWidget(button)
            return button

        self._zoom_out_button = add_step_button(
            "galleryZoomOutButton", fluentqt.Icons.ZoomOut, "Zoom out", self.zoom_out
        )
        self._level_label = fluentqt.Label(_percent_text(self._applied_percent), bubble)
        self._level_label.setObjectName("galleryZoomLevelLabel")
        self._level_label.setFluentTypography(fluentqt.Typography.FontRole.BodyStrong)
        self._level_label.setAlignment(Qt.AlignCenter)
        # Reserve the widest level so the bubble never shifts while stepping.
        self._level_label.setMinimumWidth(
            self._level_label.fontMetrics().horizontalAdvance("200%") + 8
        )
        row.addWidget(self._level_label)
        self._zoom_in_button = add_step_button(
            "galleryZoomInButton", fluentqt.Icons.ZoomIn, "Zoom in", self.zoom_in
        )
        row.addSpacing(8)

        self._reset_button = fluentqt.Button("Reset", bubble)
        self._reset_button.setObjectName("galleryZoomResetButton")
        self._reset_button.setFluentStyle(fluentqt.Button.ButtonStyle.Standard)
        self._reset_button.setFluentSize(fluentqt.Button.ButtonSize.Small)
        self._reset_button.setAccessibleName("Reset zoom")
        self._reset_button.setFocusPolicy(Qt.TabFocus)
        self._reset_button.clicked.connect(self.reset_zoom)
        row.addWidget(self._reset_button)

        # Builds without live zoom keep the current scale until the next launch.
        self._restart_hint = fluentqt.Label("Applies after restarting Gallery", bubble)
        self._restart_hint.setObjectName("galleryZoomRestartHint")
        self._restart_hint.setFluentTypography(fluentqt.Typography.FontRole.Caption)
        self._restart_hint.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        self._restart_hint.setVisible(False)
        column.addWidget(self._restart_hint)

    def _show_bubble(self) -> None:
        if self._bubble is None:
            self._build_bubble()
        self._sync_bubble()
        self._place_bubble()
        if not self._bubble.isOpen():
            self._bubble.open()
        self._idle_timer.start()

    def _place_bubble(self) -> None:
        bubble = self._bubble
        bubble.adjustSize()
        margins = bubble.contentsMargins()
        # Popup reserves its shadow in its contents margins.
        card_width = bubble.width() - margins.left() - margins.right()
        if self._window.layoutDirection() == Qt.RightToLeft:
            x = _BUBBLE_EDGE_MARGIN
        else:
            x = self._window.width() - card_width - _BUBBLE_EDGE_MARGIN
        bubble.setPosition(self._window, QPoint(x, TITLE_BAR_HEIGHT + _BUBBLE_TITLE_BAR_GAP))

    def _sync_bubble(self) -> None:
        percent = self._settings.interface_zoom_percent
        self._level_label.setText(_percent_text(percent))
        self._zoom_out_button.setEnabled(zoomed_out(percent) != percent)
        self._zoom_in_button.setEnabled(zoomed_in(percent) != percent)
        self._reset_button.setEnabled(percent != DEFAULT_PERCENT)
        self._restart_hint.setVisible(percent != self._applied_percent)

    @Slot()
    def _close_bubble_when_idle(self) -> None:
        bubble = self._bubble
        if bubble is None or not bubble.isOpen():
            return
        # A hovered or focused bubble stays open until the pointer or focus
        # leaves it.
        if bubble.underMouse() or self._contains_focus():
            self._idle_timer.start()
            return
        bubble.close()

    def _contains_focus(self) -> bool:
        focused = QApplication.focusWidget()
        return focused is not None and (
            focused is self._bubble or self._bubble.isAncestorOf(focused)
        )

    def _minimum_size_at(self, percent: int) -> QSize:
        screen = self._window.screen()
        return app_window_minimum_size(
            screen.availableGeometry().size() if screen is not None else QSize(),
            percent,
        )


__all__ = ["GalleryInterfaceZoomController"]
