#!/usr/bin/env python3
"""Shared loader and validator for multi-locale component, API, release, and playground copy."""

from __future__ import annotations

from functools import lru_cache
import hashlib
import json
from pathlib import Path
import re

from site_locales import LOCALE_KEYS

ROOT = Path(__file__).resolve().parents[2]
LOCALES_ROOT = Path(__file__).with_name("locales")
PLAYGROUNDS_DIR = ROOT / "app/assets/playgrounds"
EXTRA_LOCALE_KEYS: tuple[str, ...] = tuple(k for k in LOCALE_KEYS if k not in ("en", "zh"))

PLACEHOLDER_RE = re.compile(r"\{[a-zA-Z0-9_]+\}")
BACKTICK_RE = re.compile(r"`[^`]+`")

PLAYGROUND_UI_BASELINE: dict[str, dict[str, object]] = {
    "en": {
        "preview": "Property preview",
        "close": "Close preview",
        "more": "More properties",
        "reset": "Reset properties",
        "copy": "Copy code",
        "copied": "Copied",
        "copying": "Copying…",
        "failed": "Could not copy. Try again.",
        "loading": "Loading preview…",
        "unavailable": "Preview could not load",
        "retry": "Try again",
        "invalid": "Invalid input. This change was not applied.",
        "search": "Find a property or API",
        "follow": "Follow theme",
        "empty": "No matching properties",
        "language": "Code language",
        "numeric": "Enter a valid number.",
        "noTargets": "No editable components in this example.",
        "interactivePreview": "interactive preview",
        "propertyCategories": "Property categories",
        "catBasic": "Basic",
        "catIconLayout": "Icon & Layout",
        "catAppearance": "Style & State",
        "catIcons": "Icons",
        "catLayout": "Layout",
        "sourceCode": "Source Code",
        "resizeSplit": "Resize split panes",
        "dragSplit": "Drag to resize panes",
        "previewView": "Preview view",
        "properties": "Properties",
        "code": "Code",
        "clear": "Clear",
        "am": "AM",
        "pm": "PM",
        "time12Template": "{time} {period}",
        "yearMonthTemplate": "{monthName} {year}",
        "shortMonthTemplate": "{shortMonth}",
        "yearLabelTemplate": "{year}",
        "monthNames": [
            "January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December",
        ],
        "shortMonths": [
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
        ],
        "weekdays": ["Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"],
        "firstDayOfWeek": 0,
        "zoomOutCalendar": "Zoom out calendar level",
        "previous": "Previous",
        "next": "Next",
        "hour": "Hour",
        "minute": "Min",
        "amPm": "AM/PM",
        "today": "Today",
        "confirm": "Confirm",
        "cancel": "Cancel",
        "notSet": "Not set",
        "noSelection": "No selection",
        "pageTemplate": "Page {page}",
        "node": "Node",
        "removeRow": "Remove row",
        "addRow": "Add row",
        "ascendingXError": "Keep X values in ascending order",
        "removeRowTemplate": "Remove row {row}",
        "checkableHint": 'Enables toggle button behavior. Toggle "Checked" below to see the active state.',
        "focusVisualHint": "Focus the component with Tab or a click to inspect its focus ring.",
        "displayText": "Display text",
        "useSameText": "Use the same text",
        "differentTextPerState": "Set different text for each state",
        "mainIcon": "Main icon",
        "icon": "Icon",
        "dropdownIndicator": "Dropdown indicator",
        "queryIcon": "Query icon",
        "eventClicked": "clicked",
        "eventToggled": "toggled",
        "eventBrowseRequested": "browseRequested",
        "eventFilesDropped": "filesDropped",
        "fixedRangeHint": "Only one value fits. Adjust the bounds or minimum separation first.",
        "hoverHint": "Move the pointer over the component to see its hover effect.",
        "chooseMainIconHint": "Choose a main icon to adjust its placement and transforms.",
        "enterGlyphHint": "Enter a glyph to adjust its size, rotation and color.",
        "textOnlyIconHint": "Text only hides the icon. Choose an icon layout to adjust its transforms.",
        "stackedIconHint": "Place the icon above or below the text to adjust rotation and scale.",
        "chooseDropdownHint": "Choose a dropdown indicator to adjust its size and position.",
    },
}

PLAYGROUND_OPTIONS_BASELINE: dict[str, dict[str, str]] = {
    "en": {
        "ChevronDown": "Down", "ChevronUp": "Up", "ChevronRight": "Right",
        "Standard": "Standard", "Accent": "Accent", "Subtle": "Subtle",
        "Small": "Small", "StandardSize": "Default", "Medium": "Medium",
        "Large": "Large", "ExtraLarge": "Extra large", "None": "None",
        "Add": "Add", "CheckMark": "CheckMark", "Save": "Save",
        "FavoriteStar": "Favorite star", "Settings": "Settings", "Search": "Search",
        "Mail": "Mail", "Home": "Home", "Document": "Document",
        "Info": "Info", "Warning": "Warning", "ErrorBadge12": "Error badge",
        "TextOnly": "Text only", "IconBefore": "Icon before text", "IconAfter": "Icon after text",
        "IconOnly": "Icon only", "IconAbove": "Icon above text", "IconBelow": "Icon below text",
        "Rest": "Rest", "Hover": "Hover", "Pressed": "Pressed", "Disabled": "Disabled",
        "Unchecked": "Unchecked", "PartiallyChecked": "Partially checked", "Checked": "Checked",
        "Horizontal": "Horizontal", "Vertical": "Vertical",
        "NoTicks": "No ticks", "TicksAbove": "Ticks above", "TicksBelow": "Ticks below",
        "TicksLeft": "Ticks left", "TicksRight": "Ticks right", "TicksBothSides": "Both sides",
        "Peek": "Peek", "Hidden": "Hidden", "Visible": "Visible",
        "Left": "Left", "Right": "Right", "Leading": "Leading", "Trailing": "Trailing",
        "Single": "Single", "Multiple": "Multiple", "Extended": "Extended",
        "Compact": "Compact", "Inline": "Inline",
        "Caption": "Caption", "Body": "Body", "BodyStrong": "Body strong",
        "BodyLarge": "Body large", "BodyLargeStrong": "Body large strong",
        "Subtitle": "Subtitle", "Title": "Title", "TitleLarge": "Title large", "Display": "Display",
        "ElideLeft": "Elide left", "ElideRight": "Elide right",
        "ElideMiddle": "Elide middle", "ElideNone": "No elide",
        "Auto": "Auto", "Automatic": "Automatic", "Stacked": "Stacked",
        "Circular": "Circular", "Square": "Square",
        "Available": "Available", "Away": "Away", "Busy": "Busy",
        "DoNotDisturb": "Do not disturb", "Offline": "Offline",
        "Dot": "Dot", "Icon": "Icon", "Value": "Value",
        "Informational": "Informational", "Attention": "Attention", "Caution": "Caution",
        "Success": "Success", "Critical": "Critical", "Error": "Error",
        "Validating": "Validating", "Running": "Running", "Paused": "Paused",
        "Branded": "Branded", "Simple": "Simple", "Custom": "Custom",
        "ImageCard": "Image card", "AvatarTextRow": "Avatar & text", "TextBlock": "Text block",
        "Top": "Top", "TopStart": "Top start", "TopEnd": "Top end",
        "Bottom": "Bottom", "BottomStart": "Bottom start", "BottomEnd": "Bottom end",
        "TopLeft": "Top left", "TopRight": "Top right",
        "BottomLeft": "Bottom left", "BottomRight": "Bottom right",
        "LeftTop": "Left top", "LeftBottom": "Left bottom",
        "RightTop": "Right top", "RightBottom": "Right bottom", "Full": "Full / Center",
        "Day": "Day", "Month": "Month", "Year": "Year",
        "Monday": "Monday", "Tuesday": "Tuesday", "Wednesday": "Wednesday",
        "Thursday": "Thursday", "Friday": "Friday", "Saturday": "Saturday", "Sunday": "Sunday",
        "FullMonthName": "Full month name", "AbbreviatedMonthName": "Abbreviated month",
        "NumericMonth": "Numeric month", "TwoDigitMonth": "Two-digit month",
        "DayInteger": "Day integer", "TwoDigitDay": "Two-digit day",
        "DayIntegerWithAbbreviatedWeekday": "Day & weekday",
        "FullYear": "Full year", "TwoDigitYear": "Two-digit year",
        "TwelveHourClock": "12-hour clock", "TwentyFourHourClock": "24-hour clock",
        "Layer": "Layer", "LayerAlt": "Layer alt", "Canvas": "Canvas",
        "Beginning": "Beginning", "Middle": "Middle", "List": "List", "Detail": "Detail",
        "LeftCompact": "Left compact", "LeftMinimal": "Left minimal",
        "ScrollButtons": "Scroll buttons", "MoreButton": "More button",
        "Equal": "Equal", "SizeToContent": "Size to content",
        "OnHover": "On hover", "Always": "Always", "Manual": "Manual",
        "Contain": "Contain", "ScaleDown": "Scale down", "Zoom": "Zoom", "Pan": "Pan",
        "Collapsed": "Collapsed", "VisibleOnPointerOver": "On hover",
        "FastTransformation": "Fast", "SmoothTransformation": "Smooth",
        "Line": "Line", "Area": "Area", "Bar": "Bar", "HorizontalBar": "Horizontal bar",
        "Pie": "Pie", "Donut": "Donut", "Scatter": "Scatter", "Sparkline": "Sparkline",
        "Alternate": "Alternate", "SlideFade": "Slide fade", "ScaleFade": "Scale fade",
        "Immediate": "Immediate", "Transient": "Transient",
        "Solid": "Solid", "Transparent": "Transparent",
        "FlowingRibbons": "Flowing ribbons", "FloatingDots": "Floating dots", "Starfield": "Starfield",
        "Raster": "CPU raster", "OpenGL": "OpenGL", "Mica": "Mica", "Acrylic": "Acrylic",
        "Design": "Design", "Engineering": "Engineering", "Research": "Research",
    },
}


def text_key(en_text: str) -> str:
    """Return the 8-char SHA-256 hex digest key for an English source string."""
    return hashlib.sha256(en_text.encode("utf-8")).hexdigest()[:8]


def placeholders(text: str) -> list[str]:
    return sorted(PLACEHOLDER_RE.findall(text))


def _load_locale_json(domain: str, language: str) -> dict:
    path = LOCALES_ROOT / domain / f"{language}.json"
    if not path.is_file():
        raise ValueError(f"Missing {domain} locale file: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


@lru_cache(maxsize=None)
def component_locale(language: str) -> dict:
    return _load_locale_json("components", language)


@lru_cache(maxsize=None)
def api_locale(language: str) -> dict[str, str]:
    return _load_locale_json("api", language)


@lru_cache(maxsize=None)
def release_locale(language: str) -> dict[str, str]:
    return _load_locale_json("releases", language)


@lru_cache(maxsize=None)
def playground_locale(language: str) -> dict:
    return _load_locale_json("playground", language)


def translate_api_description(description: dict, language: str) -> str:
    en = description.get("en") or ""
    if language == "en":
        return en
    if language == "zh" and description.get("zh"):
        return description["zh"]
    if not en:
        return description.get("zh") or ""
    return api_locale(language).get(text_key(en)) or en


def translate_release_text(value: dict, language: str) -> str:
    if language in ("en", "zh"):
        return value[language]
    en = value["en"]
    return release_locale(language).get(text_key(en)) or en


def localized_component(doc: dict, language: str) -> dict:
    if language in ("en", "zh"):
        return {
            "name": doc["name"][language],
            "description": doc["description"][language],
            "previews": {
                p["id"]: {"name": p["name"][language], "alt": p["alt"][language]}
                for p in doc["previews"]
            },
            "examples": {
                e["sample"]: {"name": e["name"][language], "description": e["description"][language]}
                for e in doc["examples"]
            },
        }
    entry = component_locale(language)[doc["id"]]
    if "examples" in entry:
        return entry
    from documentation_guides import guide_ui

    ui = guide_ui(language)
    preview = doc["previews"][0]
    example = doc["examples"][0]
    title = display_component_title(doc["name"]["en"], entry["name"], language)
    return {
        "name": entry["name"],
        "description": entry["description"],
        "previews": {
            preview["id"]: {
                "name": ui["exampleHeading"],
                "alt": f"{title}: {entry['example']}",
            }
        },
        "examples": {
            example["sample"]: {
                "name": entry["example"],
                "description": entry["usage"],
            }
        },
    }


def display_component_title(component_title: str, localized_name: str, language: str) -> str:
    if language == "en" or localized_name.casefold() == component_title.casefold():
        return component_title
    return f"{component_title} {localized_name}"


def _walk_bilingual(node: object, pairs: dict[str, str]) -> None:
    if isinstance(node, dict):
        if "en" in node and "zh" in node and isinstance(node["en"], str) and isinstance(node["zh"], str):
            pairs.setdefault(node["en"], node["zh"])
            return
        for value in node.values():
            _walk_bilingual(value, pairs)
    elif isinstance(node, list):
        for item in node:
            _walk_bilingual(item, pairs)


@lru_cache(maxsize=1)
def playground_en_zh_pairs() -> dict[str, str]:
    pairs: dict[str, str] = {}
    for path in sorted(PLAYGROUNDS_DIR.glob("*.json")):
        if path.name == "components.json":
            continue
        _walk_bilingual(json.loads(path.read_text(encoding="utf-8")), pairs)
    return pairs


@lru_cache(maxsize=1)
def playground_en_strings() -> dict[str, str]:
    mapping: dict[str, str] = {}
    for en_text in sorted(playground_en_zh_pairs()):
        key = text_key(en_text)
        if key in mapping and mapping[key] != en_text:
            raise ValueError(f"Hash collision in playground strings for {key}: {en_text!r} vs {mapping[key]!r}")
        mapping[key] = en_text
    return mapping


def playground_runtime_payload(language: str) -> bytes:
    data = playground_locale(language)
    en_map = playground_en_strings()
    strings = {
        en_text: data["strings"][digest]
        for digest, en_text in en_map.items()
        if digest in data["strings"]
    }
    payload = {
        "ui": data["ui"],
        "options": data["options"],
        "strings": strings,
    }
    return json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")


def playground_runtime_digest(language: str) -> str:
    return hashlib.sha256(playground_runtime_payload(language)).hexdigest()[:12]


def python_api_pairs(catalog: dict) -> dict[str, str]:
    from public_api import python_only_members, resolve_type

    pairs = {}
    for component in catalog["components"]:
        record = resolve_type(component["cpp"]["qualified_type"])
        for member in python_only_members(record, component["python"]):
            description = member["description"]
            if description["en"]:
                pairs.setdefault(description["en"], description["zh"])
    return pairs


def required_api_pairs(catalog: dict | None = None) -> dict[str, str]:
    from public_api import inventory
    if catalog is None:
        from generate_api_reference import generate_reference
        catalog = generate_reference()

    pairs = python_api_pairs(catalog)
    for record in inventory():
        en = record.get("description", {}).get("en")
        if en:
            pairs.setdefault(en, record["description"].get("zh", ""))
        for member in record["members"]:
            m_en = member.get("description", {}).get("en")
            if m_en:
                pairs.setdefault(m_en, member["description"].get("zh", ""))
            for val in member.get("values", []):
                v_en = val.get("description", {}).get("en")
                if v_en:
                    pairs.setdefault(v_en, val["description"].get("zh", ""))
    return pairs


def required_release_pairs() -> dict[str, str]:
    from release_history import load_history

    pairs: dict[str, str] = {}
    _walk_bilingual(load_history(), pairs)
    return pairs


def validate_docs_locales(documents: list[dict], catalog: dict | None = None) -> None:
    """Validate 100% key coverage, non-empty strings, and placeholders across all extra locales."""
    from documentation_guides import guide_ui

    catalog = catalog if catalog is not None else documents[0]["catalog"]
    api_pairs = required_api_pairs(catalog)
    api_en_map = {text_key(en): en for en in api_pairs}
    if len(api_en_map) != len(api_pairs):
        raise ValueError("Hash collision in required API descriptions")
    # C++ headers supply inline translations; wrapper docstrings use this catalog.
    missing_zh = {text_key(en) for en, zh in python_api_pairs(catalog).items() if not zh}
    zh = api_locale("zh")
    if set(zh) != missing_zh or any(not isinstance(text, str) or not text.strip() for text in zh.values()):
        raise ValueError("Incomplete supplemental Simplified Chinese API translations")
    release_en_map = {text_key(en): en for en in sorted(required_release_pairs())}
    if len(release_en_map) != len(required_release_pairs()):
        raise ValueError("Hash collision in required release descriptions")
    pg_en_map = playground_en_strings()

    ui_en = guide_ui("en")
    expected_pg_ui = PLAYGROUND_UI_BASELINE["en"]
    expected_pg_options = set(PLAYGROUND_OPTIONS_BASELINE["en"])

    for language in EXTRA_LOCALE_KEYS:
        # 1. Guide & docs-shell UI
        guide_ui(language)
        if not isinstance(guide_ui(language), dict) or set(guide_ui(language)) != set(ui_en):
            raise ValueError(f"Guide UI key mismatch in {language}")

        # 2. Components
        comp_map = component_locale(language)
        expected_ids = {doc["id"] for doc in documents}
        if set(comp_map) != expected_ids:
            raise ValueError(f"Component locale key mismatch in {language}: {set(comp_map) ^ expected_ids}")
        for doc in documents:
            cid = doc["id"]
            entry = comp_map[cid]
            if not isinstance(entry.get("name"), str) or not entry["name"].strip():
                raise ValueError(f"Empty component name in {language}/{cid}")
            if not isinstance(entry.get("description"), str) or not entry["description"].strip():
                raise ValueError(f"Empty component description in {language}/{cid}")
            if "examples" in entry:
                expected_pvs = {p["id"] for p in doc["previews"]}
                if set(entry.get("previews", {})) != expected_pvs:
                    raise ValueError(f"Preview keys mismatch in {language}/{cid}")
                for pid in expected_pvs:
                    pv = entry["previews"][pid]
                    if not isinstance(pv.get("name"), str) or not pv["name"].strip():
                        raise ValueError(f"Empty preview name in {language}/{cid}/{pid}")
                    if not isinstance(pv.get("alt"), str) or not pv["alt"].strip():
                        raise ValueError(f"Empty preview alt in {language}/{cid}/{pid}")
                expected_exs = {e["sample"] for e in doc["examples"]}
                if set(entry.get("examples", {})) != expected_exs:
                    raise ValueError(f"Example keys mismatch in {language}/{cid}")
                for sample_id in expected_exs:
                    ex = entry["examples"][sample_id]
                    if not isinstance(ex.get("name"), str) or not ex["name"].strip():
                        raise ValueError(f"Empty example name in {language}/{cid}/{sample_id}")
                    if not isinstance(ex.get("description"), str) or not ex["description"].strip():
                        raise ValueError(f"Empty example description in {language}/{cid}/{sample_id}")
            elif len(doc["previews"]) == 1 and len(doc["examples"]) == 1:
                if not isinstance(entry.get("example"), str) or not entry["example"].strip():
                    raise ValueError(f"Empty component example title in {language}/{cid}")
                if not isinstance(entry.get("usage"), str) or not entry["usage"].strip():
                    raise ValueError(f"Empty component usage in {language}/{cid}")
            else:
                raise ValueError(f"Multi-preview/example component {cid} in {language} requires full schema")

        # 3. API descriptions
        api_map = api_locale(language)
        if set(api_map) != set(api_en_map):
            raise ValueError(f"API locale key mismatch in {language}: missing={len(set(api_en_map) - set(api_map))}, extra={len(set(api_map) - set(api_en_map))}")
        for digest, text in api_map.items():
            if not isinstance(text, str) or not text.strip():
                raise ValueError(f"Empty API translation in {language}/{digest}")

        # 4. Releases
        rel_map = release_locale(language)
        if set(rel_map) != set(release_en_map):
            raise ValueError(f"Release locale key mismatch in {language}: missing={len(set(release_en_map) - set(rel_map))}, extra={len(set(rel_map) - set(release_en_map))}")
        for digest, en_text in release_en_map.items():
            text = rel_map[digest]
            if not isinstance(text, str) or not text.strip():
                raise ValueError(f"Empty release translation in {language}/{digest}")
            if sorted(BACKTICK_RE.findall(text)) != sorted(BACKTICK_RE.findall(en_text)):
                raise ValueError(f"Inline code span mismatch in {language} releases/{digest}: {en_text!r} -> {text!r}")

        # 5. Playground runtime locale
        pg_data = playground_locale(language)
        if set(pg_data) != {"ui", "options", "strings"}:
            raise ValueError(f"Playground top-level key mismatch in {language}")
        pg_ui = pg_data["ui"]
        if set(pg_ui) != set(expected_pg_ui):
            raise ValueError(f"Playground UI key mismatch in {language}: {set(pg_ui) ^ set(expected_pg_ui)}")
        for k, baseline_val in expected_pg_ui.items():
            val = pg_ui[k]
            if k == "firstDayOfWeek":
                if val not in (0, 1):
                    raise ValueError(f"Invalid firstDayOfWeek in {language}: {val!r}")
            elif isinstance(baseline_val, list):
                if not isinstance(val, list) or len(val) != len(baseline_val) or any(not isinstance(x, str) or not x.strip() for x in val):
                    raise ValueError(f"Invalid playground array {k} in {language}")
            else:
                if not isinstance(val, str) or not val.strip():
                    raise ValueError(f"Empty playground UI key {k} in {language}")
                if placeholders(val) != placeholders(baseline_val):
                    raise ValueError(f"Placeholder mismatch in {language}/playground/ui.{k}")
        pg_opts = pg_data["options"]
        if set(pg_opts) != expected_pg_options:
            raise ValueError(f"Playground options key mismatch in {language}: {set(pg_opts) ^ expected_pg_options}")
        for k, val in pg_opts.items():
            if not isinstance(val, str) or not val.strip():
                raise ValueError(f"Empty playground option {k} in {language}")
        pg_strings = pg_data["strings"]
        if set(pg_strings) != set(pg_en_map):
            raise ValueError(f"Playground string key mismatch in {language}: missing={len(set(pg_en_map) - set(pg_strings))}, extra={len(set(pg_strings) - set(pg_en_map))}")
        for digest, text in pg_strings.items():
            if not isinstance(text, str) or not text.strip():
                raise ValueError(f"Empty playground string in {language}/{digest}")
            if placeholders(text) != placeholders(pg_en_map[digest]):
                raise ValueError(f"Placeholder mismatch in {language}/playground/strings/{digest}")
