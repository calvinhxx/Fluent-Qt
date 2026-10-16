"""Private event capability adapters for supported PySide6 versions."""

from PySide6.QtCore import QEvent

__all__ = ["is_display_scale_change_event"]


_DISPLAY_SCALE_EVENTS = frozenset(
    value
    for value in (
        getattr(QEvent.Type, "ScreenChangeInternal", None),
        getattr(QEvent.Type, "DevicePixelRatioChange", None),
    )
    if value is not None
)


def is_display_scale_change_event(event: QEvent | None) -> bool:
    return event is not None and event.type() in _DISPLAY_SCALE_EVENTS
