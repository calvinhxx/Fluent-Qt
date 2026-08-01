"""Windowing components and backdrop state types."""

from . import _fluentqt as _native

BackdropBackend = _native.fluent.windowing.BackdropBackend
BackdropCapabilities = _native.fluent.windowing.BackdropCapabilities
BackdropEffect = _native.fluent.windowing.BackdropEffect
BackdropFidelity = _native.fluent.windowing.BackdropFidelity
BackdropState = _native.fluent.windowing.BackdropState
BackdropSurfaceMode = _native.fluent.windowing.BackdropSurfaceMode
TitleBar = _native.fluent.windowing.TitleBar
Window = _native.fluent.windowing.Window

__all__ = [
    "BackdropBackend",
    "BackdropCapabilities",
    "BackdropEffect",
    "BackdropFidelity",
    "BackdropState",
    "BackdropSurfaceMode",
    "TitleBar",
    "Window",
]
