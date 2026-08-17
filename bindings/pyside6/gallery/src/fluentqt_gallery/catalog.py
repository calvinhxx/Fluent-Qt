"""Native-parity data model for the standalone PySide6 Gallery."""

from __future__ import annotations

from dataclasses import dataclass
import json
from pathlib import Path
import re
from typing import Iterable


@dataclass(frozen=True)
class GallerySampleEntry:
    """One SampleCard authored by the canonical C++ Gallery."""

    id: str
    title: str
    description: str
    cpp_snippet: str
    preview_orientation: str | None
    preview_spacing: int | None


@dataclass(frozen=True)
class GalleryCategory:
    """One native Gallery component category."""

    id: str
    title: str
    components: tuple[str, ...]


@dataclass(frozen=True)
class GalleryEntry:
    """One native component route and all of its SampleCards."""

    route_id: str
    title: str
    name: str
    category_id: str
    description: str
    samples: tuple[GallerySampleEntry, ...]

    @property
    def support_type(self) -> bool:
        """Compatibility shim: component routes are never support types."""

        return False


@dataclass(frozen=True)
class GalleryRoute:
    """One route in the same order as the native Gallery navigation model."""

    id: str
    title: str
    kind: str
    parent_id: str
    description: str


def _load_contract() -> dict[str, object]:
    path = Path(__file__).with_name("contract.json")
    if not path.is_file():
        raise RuntimeError(
            "PySide6 Gallery contract is missing: {0}. Reconfigure the "
            "FluentQt PySide6 build or reinstall the wheel.".format(path)
        )
    contract = json.loads(path.read_text(encoding="utf-8"))
    if contract.get("schema_version") != 1:
        raise RuntimeError(
            "Unsupported PySide6 Gallery contract schema: {0}".format(
                contract.get("schema_version")
            )
        )
    summary = contract.get("summary", {})
    expected = {
        "route_count": 90,
        "component_count": 69,
        "sample_count": 205,
    }
    if summary != expected:
        raise RuntimeError(
            "Unexpected native Gallery contract summary: {0!r}; expected {1!r}"
            .format(summary, expected)
        )
    return contract


CONTRACT = _load_contract()

CATEGORIES = tuple(
    GalleryCategory(
        id=category["id"],
        title=category["title"],
        components=tuple(category["components"]),
    )
    for category in CONTRACT["categories"]
)

ENTRIES = tuple(
    GalleryEntry(
        route_id=component["id"],
        title=component["title"],
        name=component["api_type"],
        category_id=component["category_id"],
        description=component["description"],
        samples=tuple(
            GallerySampleEntry(
                id=sample["id"],
                title=sample["title"],
                description=sample["description"],
                cpp_snippet=sample["cpp_snippet"],
                preview_orientation=(sample.get("preview_layout") or {}).get(
                    "orientation"
                ),
                preview_spacing=(sample.get("preview_layout") or {}).get("spacing"),
            )
            for sample in component["samples"]
        ),
    )
    for component in CONTRACT["components"]
)

ROUTES = tuple(
    GalleryRoute(
        id=route["id"],
        title=route["title"],
        kind=route["kind"],
        parent_id=route["parent_id"],
        description=route["description"],
    )
    for route in CONTRACT["routes"]
)

SUPPORT_TYPES = frozenset(CONTRACT["binding_support_types"])
CATEGORY_BY_ID = {category.id: category for category in CATEGORIES}
ENTRY_BY_ROUTE_ID = {entry.route_id: entry for entry in ENTRIES}
ENTRY_BY_NAME = {entry.name: entry for entry in ENTRIES}
ROUTE_BY_ID = {route.id: route for route in ROUTES}
SAMPLE_BY_KEY = {
    (entry.route_id, sample.id): sample
    for entry in ENTRIES
    for sample in entry.samples
}


def display_name(name: str) -> str:
    """Split a public CamelCase symbol into a readable title."""

    return re.sub(r"(?<!^)(?=[A-Z])", " ", name)


def entries_for_category(category_id: str) -> tuple[GalleryEntry, ...]:
    """Return component routes in native catalog order for a category."""

    category = CATEGORY_BY_ID[category_id]
    return tuple(ENTRY_BY_ROUTE_ID[route_id] for route_id in category.components)


def search_entries(query: str) -> tuple[GalleryEntry, ...]:
    """Return case-insensitive route, type, title, and description matches."""

    needle = query.strip().casefold()
    if not needle:
        return ENTRIES
    return tuple(
        entry
        for entry in ENTRIES
        if needle in entry.route_id.casefold()
        or needle in entry.name.casefold()
        or needle in entry.title.casefold()
        or needle in entry.description.casefold()
    )


def catalog_coverage_errors(manifest_classes: Iterable[str]) -> list[str]:
    """Compare routed plus embedded support types with the binding manifest."""

    manifest = set(manifest_classes)
    routed = {entry.name for entry in ENTRIES}
    covered = routed | set(SUPPORT_TYPES)
    errors = []
    if len(ENTRIES) != len(routed):
        errors.append("Gallery component routes contain duplicate API types")
    missing = sorted(manifest - covered)
    extra = sorted(covered - manifest)
    if missing:
        errors.append("Gallery coverage is missing: {0}".format(", ".join(missing)))
    if extra:
        errors.append(
            "Gallery coverage has non-manifest classes: {0}".format(", ".join(extra))
        )
    return errors


__all__ = [
    "CATEGORIES",
    "CATEGORY_BY_ID",
    "CONTRACT",
    "ENTRIES",
    "ENTRY_BY_NAME",
    "ENTRY_BY_ROUTE_ID",
    "GalleryCategory",
    "GalleryEntry",
    "GalleryRoute",
    "GallerySampleEntry",
    "ROUTES",
    "ROUTE_BY_ID",
    "SAMPLE_BY_KEY",
    "SUPPORT_TYPES",
    "catalog_coverage_errors",
    "display_name",
    "entries_for_category",
    "search_entries",
]
