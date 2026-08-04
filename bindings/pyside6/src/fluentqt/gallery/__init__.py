"""Wheel-installed Gallery for FluentQt's public PySide6 API."""

from __future__ import annotations

from typing import TYPE_CHECKING


if TYPE_CHECKING:
    from .window import GalleryWindow as GalleryWindow


def main(argv: list[str] | None = None) -> int:
    """Run the Python Gallery without importing its Qt UI eagerly."""

    import fluentqt

    fluentqt.prepare_high_dpi_application()
    from .app import main as run

    return run(argv)


def __getattr__(name: str) -> object:
    if name == "GalleryWindow":
        from .window import GalleryWindow

        return GalleryWindow
    raise AttributeError(name)


__all__ = ["GalleryWindow", "main"]
