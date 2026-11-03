"""Window-level commands matching ``GalleryCommands``."""

from __future__ import annotations

from collections.abc import Sequence
from enum import Enum
import sys

import fluentqt
from PySide6.QtCore import QObject, Qt
from PySide6.QtGui import QAction, QKeySequence
from PySide6.QtWidgets import QMenu, QMenuBar, QWidget


class CommandGroup(Enum):
    View = "View"
    Go = "Go"
    Help = "Help"


class GalleryCommands(QObject):
    """Gallery's window-level commands and the places that list them.

    Each command is one QAction that owns its text, shortcuts, and enabled
    state. Registering it adds it to the window so its shortcut works, to the
    searchable command palette, and on macOS to the system menu bar, so every
    shortcut can be looked up instead of remembered.
    """

    def __init__(self, window: QWidget) -> None:
        super().__init__(window)
        self.setObjectName("galleryCommands")
        self._window = window
        self._commands: list[tuple[CommandGroup, QAction]] = []
        self._palette = fluentqt.CommandPalette(window)
        self._palette.setObjectName("galleryCommandPalette")
        self._palette.setPlaceholderText("Search commands and shortcuts")
        self._menu_bar: QMenuBar | None = None
        self._menus: dict[CommandGroup, QMenu] = {}
        if sys.platform == "darwin":
            # A parentless menu bar is the macOS global menu bar; other
            # platforms keep the Fluent title bar. It lives as long as the window.
            menu_bar = QMenuBar(None)
            menu_bar.setObjectName("galleryMenuBar")
            for group in CommandGroup:
                menu = menu_bar.addMenu(group.value)
                menu.setObjectName("galleryMenu{0}".format(group.value))
                self._menus[group] = menu
            window.destroyed.connect(menu_bar.deleteLater)
            self._menu_bar = menu_bar

    def add_action(self, group: CommandGroup, action: QAction) -> None:
        """Register a caller-owned ``action`` under ``group``."""

        if action is None:
            return
        self._commands.append((group, action))
        self._window.addAction(action)
        self._palette.addCommand(action, group.value)
        menu = self._menus.get(group)
        if menu is not None:
            menu.addAction(action)

    def add_command(
        self,
        group: CommandGroup,
        object_name: str,
        text: str,
        shortcuts: Sequence[QKeySequence] = (),
    ) -> QAction:
        """Create and register a window-owned command."""

        action = QAction(text, self._window)
        action.setObjectName(object_name)
        action.setShortcuts(list(shortcuts))
        action.setShortcutContext(Qt.WindowShortcut)
        self.add_action(group, action)
        return action

    def commands(self, group: CommandGroup) -> list[QAction]:
        return [action for command_group, action in self._commands if command_group == group]

    @property
    def palette(self) -> fluentqt.CommandPalette:
        return self._palette

    def show_palette(self) -> None:
        """Open the searchable list of every command with its shortcut."""

        if self._palette.isOpen():
            return
        self._palette.setQuery("")
        self._palette.open()


__all__ = ["CommandGroup", "GalleryCommands"]
