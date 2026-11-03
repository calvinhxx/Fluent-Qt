"""Browser-style interface zoom levels matching ``GalleryInterfaceZoom.h``."""

from __future__ import annotations

import fluentqt._fluentqt as _native


DEFAULT_PERCENT = 100
LEVELS = (50, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200)


def nearest_level(percent: int) -> int:
    """Return the supported level closest to ``percent``; ties keep the smaller."""

    nearest = LEVELS[0]
    for level in LEVELS:
        if abs(level - percent) < abs(nearest - percent):
            nearest = level
    return nearest


def zoomed_in(percent: int) -> int:
    """Return the next larger level, staying at the largest level."""

    current = nearest_level(percent)
    return next((level for level in LEVELS if level > current), current)


def zoomed_out(percent: int) -> int:
    """Return the next smaller level, staying at the smallest level."""

    current = nearest_level(percent)
    return next((level for level in reversed(LEVELS) if level < current), current)


def supports_interface_zoom() -> bool:
    """All desktop builds offer zoom; private headers enable live changes."""
    return True


def supports_live_interface_zoom() -> bool:
    """Report whether a change can be applied without restarting."""

    return bool(_native.supportsInterfaceZoomForBinding())


def prepare_interface_zoom(zoom: float) -> bool:
    """Apply saved scale before constructing QApplication."""
    return bool(_native.prepareInterfaceZoomForBinding(float(zoom)))


def applied_zoom_percent() -> int:
    """Return applied zoom independently of a pending saved choice."""
    return round(float(_native.appliedInterfaceZoomForBinding()) * 100)


def apply_interface_zoom(zoom: float) -> bool:
    """Rescale every window by ``zoom`` while keeping each native window size.

    Widgets receive the smaller or larger logical size and relayout, as a web
    page does under browser zoom. Returns False when the runtime cannot zoom.
    """

    return bool(_native.applyInterfaceZoomForBinding(float(zoom)))


__all__ = [
    "DEFAULT_PERCENT",
    "LEVELS",
    "apply_interface_zoom",
    "nearest_level",
    "supports_interface_zoom",
    "supports_live_interface_zoom",
    "prepare_interface_zoom",
    "applied_zoom_percent",
    "zoomed_in",
    "zoomed_out",
]
