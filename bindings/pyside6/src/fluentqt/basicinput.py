"""Basic-input components."""

from . import _fluentqt as _native

Button = _native.fluent.Button
CheckBox = _native.fluent.CheckBox
ColorPicker = _native.fluent.ColorPicker
_NativeComboBox = _native.fluent.ComboBox
CompoundButton = _native.fluent.CompoundButton
DropDownButton = _native.fluent.DropDownButton
HyperlinkButton = _native.fluent.HyperlinkButton
MultiSelectComboBox = _native.fluent.MultiSelectComboBox
RadioButton = _native.fluent.RadioButton
RatingControl = _native.fluent.RatingControl
RepeatButton = _native.fluent.RepeatButton
Slider = _native.fluent.Slider
SplitButton = _native.fluent.SplitButton
ToggleButton = _native.fluent.ToggleButton
ToggleSplitButton = _native.fluent.ToggleSplitButton
ToggleSwitch = _native.fluent.ToggleSwitch


class ComboBox(_NativeComboBox):
    """Fluent text dropdown with native QComboBox model semantics.

    The native popup owns its Fluent ``ListView`` and row delegate. Replacing
    the inherited QComboBox view or delegate would only mutate Qt's unused
    fallback popup, so those customization entry points fail explicitly.
    Supply item text/data through the model and use ordinary ComboBox signals.

    A custom line editor passed to ``setLineEdit()`` is adopted and destroyed
    by the ComboBox, matching QComboBox. Do not delete an installed editor
    directly; replace it or disable editable mode instead.
    """

    def setView(self, _view):
        raise NotImplementedError(
            "ComboBox owns its Fluent dropdown view; custom QComboBox views "
            "are not supported"
        )

    def view(self):
        raise NotImplementedError(
            "ComboBox's internal Fluent dropdown view is not public"
        )

    def setItemDelegate(self, _delegate):
        raise NotImplementedError(
            "ComboBox owns its Fluent dropdown delegate; provide text and "
            "data through the model"
        )

    def itemDelegate(self):
        raise NotImplementedError(
            "ComboBox's internal Fluent dropdown delegate is not public"
        )

__all__ = [
    "Button",
    "CheckBox",
    "ColorPicker",
    "ComboBox",
    "CompoundButton",
    "DropDownButton",
    "HyperlinkButton",
    "MultiSelectComboBox",
    "RadioButton",
    "RatingControl",
    "RepeatButton",
    "Slider",
    "SplitButton",
    "ToggleButton",
    "ToggleSplitButton",
    "ToggleSwitch",
]
