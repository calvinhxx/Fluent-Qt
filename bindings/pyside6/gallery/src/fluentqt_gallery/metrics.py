"""Standalone Python Gallery metrics matching ``GalleryWindowMetrics.h``."""

from __future__ import annotations

import sys

from PySide6.QtCore import QMargins


TITLE_BAR_HEIGHT = 42


def drawer_title_bar_avoidance_margins() -> QMargins:
    """Keep same-window drawers below the macOS custom title bar."""

    return (
        QMargins(0, TITLE_BAR_HEIGHT, 0, 0)
        if sys.platform == "darwin"
        else QMargins()
    )
