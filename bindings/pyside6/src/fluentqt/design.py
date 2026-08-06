"""Read-only Fluent design constants and theme-token views.

The C++ library keeps these values in namespaces.  Python exposes matching
namespace-style classes so examples can use semantic names without depending
on private-use font codepoints or mutable theme registries.
"""

from collections.abc import Mapping
import json
from pathlib import Path

from ._fluentqt import FontRole


class _ReadOnlyNamespaceMeta(type):
    def __setattr__(cls, name, value):
        raise AttributeError(
            "{0}.{1} is a read-only design constant".format(
                cls.__qualname__, name
            )
        )

    def __delattr__(cls, name):
        raise AttributeError(
            "{0}.{1} is a read-only design constant".format(
                cls.__qualname__, name
            )
        )


class _DesignNamespace(metaclass=_ReadOnlyNamespaceMeta):
    def __new__(cls, *args, **kwargs):
        del args, kwargs
        raise TypeError(
            "{0} is a namespace and cannot be instantiated".format(
                cls.__qualname__
            )
        )


def _load_icon_aliases():
    path = Path(__file__).with_name("_icon_aliases.json")
    try:
        values = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise RuntimeError(
            "FluentQt semantic icon aliases could not be loaded"
        ) from error
    if not isinstance(values, dict) or not all(
        isinstance(name, str) and isinstance(value, str)
        for name, value in values.items()
    ):
        raise RuntimeError("FluentQt semantic icon aliases are invalid")
    return values


Icons = _ReadOnlyNamespaceMeta(
    "Icons",
    (_DesignNamespace,),
    {
        "__doc__": "Semantic shortcuts for bundled Fluent System Icons.",
        "__module__": __name__,
        **_load_icon_aliases(),
    },
)


class IconSize(_DesignNamespace):
    """Optical icon sizes from ``Typography::IconSize``."""

    Compact = 12
    Standard = 16
    Large = 20
    XLarge = 24


class Typography(_DesignNamespace):
    """Python facade for the public C++ typography namespaces."""

    Icons = Icons
    IconSize = IconSize
    FontRole = FontRole


class Spacing(_DesignNamespace):
    """Spacing, padding, border, gap, and control-height constants."""

    BaseUnit = 4
    XSmall = 4
    Small = 8
    Medium = 12
    Standard = 16
    Large = 24
    XLarge = 32
    XXLarge = 48

    class Padding(_DesignNamespace):
        ControlHorizontal = 12
        ControlVertical = 8
        ComboBoxHorizontal = 11
        ComboBoxVertical = 4
        TextFieldHorizontal = 8
        TextFieldVertical = 4
        Card = 16
        Dialog = 24
        ListItemHorizontal = 12
        ListItemVertical = 8

    class Border(_DesignNamespace):
        Normal = 1
        Focused = 2

    class Gap(_DesignNamespace):
        Tight = 4
        Normal = 8
        Loose = 16
        Section = 24

    class ControlHeight(_DesignNamespace):
        Small = 24
        Standard = 32
        Large = 40


class CornerRadius(_DesignNamespace):
    """Corner-radius constants from the C++ design-token namespace."""

    None_ = 0
    Control = 4
    Overlay = 8
    Indicator = 1.5


def _wrap_token_value(value):
    if isinstance(value, Mapping):
        return _TokenMap(value)
    if isinstance(value, list):
        return [_wrap_token_value(item) for item in value]
    if isinstance(value, tuple):
        return tuple(_wrap_token_value(item) for item in value)
    return value


class _TokenMap(dict):
    """Dictionary compatible view with attribute access for token names."""

    def __init__(self, values=(), **kwargs):
        source = dict(values, **kwargs)
        super().__init__(
            (name, _wrap_token_value(value))
            for name, value in source.items()
        )

    def __getattr__(self, name):
        try:
            return self[name]
        except KeyError as error:
            raise AttributeError(name) from error

    def __dir__(self):
        names = set(super().__dir__())
        names.update(
            name for name in self if isinstance(name, str) and name.isidentifier()
        )
        return sorted(names)


class ThemeTokens(_TokenMap):
    """Effective widget tokens with both mapping and attribute access."""


__all__ = [
    "CornerRadius",
    "Icons",
    "IconSize",
    "Spacing",
    "ThemeTokens",
    "Typography",
]
