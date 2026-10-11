"""Small, explicit authoring contract for an editable scene region."""

from dataclasses import dataclass
from typing import Callable


@dataclass(frozen=True)
class Field:
    key: str
    label: str
    kind: str
    minimum: int = 0
    maximum: int = 100
    step: int = 1
    choices: tuple[tuple[str, str], ...] = ()

    def validate(self, value):
        if self.kind == "int":
            valid = (type(value) is int and self.minimum <= value <= self.maximum
                     and (value - self.minimum) % self.step == 0)
        elif self.kind == "bool":
            valid = type(value) is bool
        elif self.kind == "choice":
            valid = type(value) is str and value in dict(self.choices)
        else:
            valid = False
        if not valid:
            raise ValueError(f"Invalid value for {self.key}: {value!r}")
        return value


@dataclass(frozen=True)
class Region:
    key: str
    title: str
    widget: object
    fields: tuple[Field, ...]
    reveal: Callable | None = None


def register_region(root, widget, key: str, title: str, fields, *, reveal=None):
    """Bind one widget to the same key in the scene's PREVIEW_EDITS literal.

    Regions live on the scene root and expire with it. Reveal is an optional
    application-owned callback, used only when explicitly selecting a region.
    """
    fields = tuple(fields)
    if not key or not title or not fields or len({field.key for field in fields}) != len(fields):
        raise ValueError("Regions require a key, a title, and unique editable fields.")
    if any(field.kind not in {"int", "bool", "choice"} or field.step <= 0 for field in fields):
        raise ValueError("Unsupported field definition.")
    regions = getattr(root, "_fluentqt_edit_regions", None)
    if regions is None:
        regions = root._fluentqt_edit_regions = {}
    if key in regions:
        raise ValueError(f"Duplicate editable region: {key}")
    regions[key] = Region(key, title, widget, fields, reveal)


def regions_for(root):
    return dict(getattr(root, "_fluentqt_edit_regions", {}))
