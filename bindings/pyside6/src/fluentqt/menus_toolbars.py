"""Fluent menu surfaces used by dropdown and split-button controls."""

from . import _fluentqt as _native
from ._qt_widgets import (
    enforce_final_type as _enforce_final_type,
    install_legacy_callable_add_action as _install_legacy_callable_add_action,
)


CommandBar = _native.fluent.CommandBar
CommandBarFlyout = _enforce_final_type(_native.fluent.CommandBarFlyout)
_install_legacy_callable_add_action(CommandBar)
_install_legacy_callable_add_action(CommandBarFlyout)
FluentMenu = _native.fluent.FluentMenu
FluentMenuBar = _native.fluent.FluentMenuBar
FluentMenuItem = _native.fluent.FluentMenuItem


__all__ = [
    "CommandBar",
    "CommandBarFlyout",
    "FluentMenu",
    "FluentMenuBar",
    "FluentMenuItem",
]
