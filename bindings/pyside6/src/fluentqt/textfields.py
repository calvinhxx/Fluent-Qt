"""Text-field components."""

from . import _fluentqt as _native
from ._fluentqt import FontRole

Label = _native.fluent.Label
LineEdit = _native.fluent.LineEdit
NumberBox = _native.fluent.NumberBox
PasswordBox = _native.fluent.PasswordBox

__all__ = ["FontRole", "Label", "LineEdit", "NumberBox", "PasswordBox"]
