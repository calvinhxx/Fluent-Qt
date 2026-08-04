"""Text-field components."""

from . import _fluentqt as _native
from ._fluentqt import FontRole

AutoSuggestBox = _native.fluent.AutoSuggestBox
EditingCommandRouter = _native.fluent.EditingCommandRouter
Label = _native.fluent.Label
LineEdit = _native.fluent.LineEdit
NumberBox = _native.fluent.NumberBox
PasswordBox = _native.fluent.PasswordBox
TextEdit = _native.fluent.TextEdit


def _text_edit_vertical_scroll_bar(self):
    """Return the Fluent scroll bar owned by this TextEdit."""
    scroll_bar = self.findChild(_native.fluent.ScrollBar)
    if scroll_bar is None or scroll_bar.parent() is not self:
        raise RuntimeError("TextEdit has no owned Fluent scroll bar")
    return scroll_bar


# Shiboken 6.2 omits the native cross-namespace pointer getter. Install one
# version-stable Python method on the native type instead of changing the
# public class identity or transferring ownership.
TextEdit.verticalScrollBar = _text_edit_vertical_scroll_bar

__all__ = [
    "AutoSuggestBox",
    "EditingCommandRouter",
    "FontRole",
    "Label",
    "LineEdit",
    "NumberBox",
    "PasswordBox",
    "TextEdit",
]
