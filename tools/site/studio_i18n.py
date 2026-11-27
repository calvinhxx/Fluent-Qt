#!/usr/bin/env python3
"""Shared loader, validator, and runtime bundle builder for Page Studio locales."""

from __future__ import annotations

from functools import lru_cache
import json
from pathlib import Path
import re

from docs_i18n import EXTRA_LOCALE_KEYS, component_locale, playground_runtime_payload
from documentation_guides import guide_ui

STUDIO_LOCALES_ROOT = Path(__file__).with_name("locales") / "studio"
PLACEHOLDER_RE = re.compile(r"\{[a-zA-Z0-9_]+\}")

STUDIO_UI_KEYS: dict[str, str | tuple[str, ...]] = {
    "toolbar.code": "Export Code",
    "toolbar.ai": "AI Build",
    "toolbar.templates": "Templates",
    "templates.title": "Starter & Industry Presets",
    "templates.subtitle": "Load a runnable Fluent-Qt page preset to edit, play, or export as C++17, PySide6, or .ui",
    "templates.loadedToast": 'Loaded "{name}" · {undo} to undo',
    "ai.title": "AI New Page",
    "ai.run": "Generate",
    "ai.running": "Generating…",
    "ai.promptPlaceholder": "Describe the Fluent-Qt desktop UI you want to create (Enter to run)",
    "ai.settingsSummary": "AI Config",
    "ai.providerLabel": "Provider",
    "ai.providerAuto": "Free Cloud",
    "ai.providerCustom": "Custom API",
    "ai.workerUrlLabel": "Cloudflare Worker URL",
    "ai.workerHint": "Enter a deployed free Worker URL, or switch to Custom API to use your own key or Ollama.",
    "ai.presetLabel": "Presets",
    "ai.baseUrlLabel": "Base URL",
    "ai.modelLabel": "Model",
    "ai.apiKeyLabel": "API Key (stored locally in browser; optional for localhost Ollama)",
    "ai.statusGenerating": "Architecting Fluent-Qt desktop layout…",
    "ai.statusThinking": "Thinking through desktop layout…",
    "ai.statusStreaming": "Generating UI (~{widgets} widgets)…",
    "ai.statusEmptyPrompt": "Enter a description of the page you want to create.",
    "ai.generatedToast": "Generated {count} widgets ({source}) · {undo} to undo",
    "ai.modeCreate": "New Page",
    "ai.modeIterate": "Iterate Page",
    "ai.modeRefine": "Refine Selected",
    "ai.promptIteratePlaceholder": "Describe edits to apply on top of the current page (Enter to run)",
    "ai.promptRefinePlaceholder": "Describe how to refine ONLY the selected component/container",
    "ai.targetLabel": "Target: ",
    "ai.styleAuto": "Auto Match",
    "ai.styleWinui": "WinUI Light",
    "ai.styleCyber": "Cyber Dark",
    "ai.styleEditorial": "Coral Light",
    "ai.styleCreative": "Violet Dark",
    "ai.styleEmerald": "Emerald Light",
    "artboard.light": "Light",
    "artboard.dark": "Dark",
    "artboard.reset": "Reset",
    "mode.edit": "Edit",
    "mode.play": "Play",
    "viewport.desktop": "Desktop",
    "viewport.tablet": "Tablet",
    "viewport.compact": "Compact",
    "pane.palette": "Components",
    "pane.outline": "Layers",
    "pane.inspector": "Properties",
    "stage.retryWasm": "Retry WASM",
    "drawer.copy": "Copy",
    "drawer.download": "Download",
    "undoTitle": "Undo ({undo})",
    "redoTitle": "Redo ({redo})",
    "duplicateTitle": "Duplicate selected node ({duplicate})",
    "searchPlaceholder": "Search 103 widgets & layouts…",
    "anchorSectionTitle": "Anchor Placement (AnchorLayout)",
    "containerTag": "Container",
    "pageRoot": "Page",
    "inspectorRootTip": "Click or drag widgets freely on canvas to edit layout and properties, or drag controls from the left.",
    "clearCanvasBtn": "Clear Canvas",
    "restoreSampleBtn": "Restore Sample",
    "wasmLoading": "Starting WASM…",
    "wasmReady": "C++ WASM Ready",
    "schematicMode": "Schematic Preview",
    "loader.title": "Starting C++ Qt Live Canvas",
    "loader.fetching": "Loading WebAssembly runtime…",
    "loader.mounting": "Initializing widget tree & layout…",
    "rootProtected": "Root page container cannot be deleted or moved",
    "copiedCode": "Code copied to clipboard",
    "emptyContainer": "Drop widgets here or click + in the palette",
    "noSelectionProps": "No configurable properties",
    "playHint": "Play mode · Interact with live widgets on canvas",
    "propsResetToast": "Properties reset to defaults",
    "dragInto": "Drop into",
    "dragMoveTo": "Move to",
    "dragFreeAnchor": "Free anchor",
    "dragNoop": "Keep position",
    "dragCancelHint": "Esc to cancel",
    "intro.guideTitle": "Studio Guide",
    "intro.title": "Welcome to Fluent-Qt Studio",
    "intro.subtitle": "Real C++ Qt Widgets visual workbench · drag & drop, AI build, and 3-target code export",
    "intro.step1": "Drag 103 real Qt widgets onto the stage, or press {ai} to build with AI",
    "intro.step2": "Switch isolated artboard themes & accents, or toggle Play for live interaction",
    "intro.step3": "Export production C++17, PySide6, or shared Qt Designer .ui files",
    "intro.startTour": "Start 30s Tour",
    "intro.tryAi": "✦ Try AI Build",
    "intro.loadSample": "Load Sample Dashboard",
    "intro.sampleLoadedToast": "Loaded Analytics Dashboard sample · {undo} to undo",
    "tour.prev": "Back",
    "tour.next": "Next",
    "tour.done": "Start Building",
    "tour.step1Title": "103 Real C++ Fluent-Qt Widgets",
    "tour.step1Desc": "Search categories or drag any layout, chart, or widget onto the canvas — powered by real Qt Widgets in WebAssembly.",
    "tour.step2Title": "Generate Pages or Refine Selection ({ai})",
    "tour.step2Desc": "Describe any desktop UI to generate full layouts, iterate on the current page, or select a widget and click ✦ to refine just that part.",
    "tour.step3Title": "Isolated Artboard & Live Play Mode",
    "tour.step3Desc": "Switch artboard light/dark themes and 6 accent colors without changing the outer site theme, or toggle Play mode to interact live.",
    "tour.step4Title": "Export C++, PySide6 & Qt Designer .ui",
    "tour.step4Desc": "Fine-tune real Qt properties on the right, then export ready-to-build C++17, PySide6, or shared Qt Designer .ui files.",
    "aiTokens": "{in}in/{out}out",
    "aiCached": "{base} · {rate}% cached · {tok}",
    "addWidget": "Add {type}",
    "defaultGridHeaders": ("Item", "Details", "Status"),
}

PLAYGROUND_SHARED_UI_MAP: dict[str, str] = {
    "catBasic": "catBasic",
    "catIconLayout": "catIconLayout",
    "catIcons": "catIcons",
    "catLayout": "catLayout",
    "catAppearance": "catAppearance",
    "resetProps": "reset",
    "followTheme": "follow",
    "mainIcon": "mainIcon",
    "icon": "icon",
    "dropdownIndicator": "dropdownIndicator",
    "queryIcon": "queryIcon",
    "notSet": "notSet",
    "propertyCategories": "propertyCategories",
    "checkableHint": "checkableHint",
    "pageTemplate": "pageTemplate",
}

GUIDE_CATEGORY_IDS: tuple[str, ...] = (
    "layout",
    "basic-input",
    "text-fields",
    "date-time",
    "navigation",
    "menus-toolbars",
    "status-info",
    "collections",
    "charts",
    "scrolling",
    "dialogs-flyouts",
)

STUDIO_EXTRA_CATEGORY_KEYS: tuple[str, ...] = (
    "windowing-foundation",
    "extensions",
)

STUDIO_LAYOUT_COMPONENT_KEYS: tuple[str, ...] = (
    "VBox",
    "HBox",
    "Grid",
    "AnchorBox",
)

COMPONENT_TYPE_BY_ID: dict[str, str] = {
    "combobox": "ComboBox",
    "checkbox": "CheckBox",
    "scrollbar": "ScrollBar",
    "annotated-scrollbar": "AnnotatedScrollBar",
    "dropdown-button": "DropDownButton",
    "multi-select-combobox": "MultiSelectComboBox",
    "tooltip": "ToolTip",
}

STUDIO_TEMPLATE_KEYS: tuple[str, ...] = (
    "starter",
    "device-monitor",
    "ai-workbench",
    "serial-console",
    "settings",
    "account-form",
    "analytics",
    "blank",
)

STUDIO_EXTRA_OPTION_KEYS: tuple[str, ...] = (
    "BottomBar",
    "BottomCenter",
    "Center",
    "End",
    "Fill",
    "LeftDock",
    "No",
    "RightDock",
    "Start",
    "Stretch",
    "TopBar",
    "TopCenter",
    "Yes",
)

STUDIO_EXTRA_PROP_KEYS: tuple[str, ...] = (
    "Active Index",
    "Align",
    "Alpha Channel",
    "Anchor Margin",
    "Anchor Position",
    "Animation Enabled",
    "Backdrop Effect",
    "Background Mode",
    "Border Visible",
    "Browse Button",
    "Camera Distance",
    "Chart Type",
    "Color Role",
    "Columns",
    "Context Text",
    "Default Margin",
    "Delay (ms)",
    "Display Mode",
    "Expansion Mode",
    "Files",
    "Filter Options",
    "Header Text",
    "Headers",
    "Helper Text",
    "Hex Color",
    "Hint Text",
    "Hover Lift",
    "Icon Size",
    "Image URL",
    "Include Time",
    "Indicator Dots",
    "Inline SVG",
    "Interactive",
    "Interval (ms)",
    "Is On",
    "Items (one per line)",
    "Label Position",
    "Layout Mode",
    "List Pane Width",
    "Lower Value",
    "Max",
    "Max Stars",
    "Media Theme",
    "Menu Items",
    "Milestones",
    "Min",
    "Min Height",
    "Min Lines",
    "Nav Buttons",
    "Nav Items",
    "Nodes",
    "Off Text",
    "On Text",
    "Padding",
    "Page Count",
    "Pane Open",
    "Particle Count",
    "Pointer Tracking",
    "Position X",
    "Position Y",
    "Primary Action",
    "Primary Actions",
    "Primary Text",
    "QML Sample",
    "Rating",
    "Read Only",
    "Render Mode",
    "Rows (one per line)",
    "Scale (%)",
    "Search Text",
    "Searchable",
    "Secondary Text",
    "Selected Index",
    "Series Name",
    "Show Presets",
    "Source Header",
    "Spacing",
    "Spatial Enabled",
    "Steps (one per line)",
    "Surface Intensity (%)",
    "Tags",
    "Target Header",
    "Top Menus",
    "Tree Items",
    "Trigger Button Text",
    "Typography",
    "Upper Value",
    "Validation Msg",
    "Validation State",
    "Values (comma-separated)",
    "Window Title",
    "Word Wrap",
    "Zoom (%)",
)

STUDIO_AI_ERROR_KEYS: tuple[str, ...] = (
    "emptyPrompt",
    "selectToRefine",
    "endpointAndModel",
    "reenterKey",
    "apiKeyRequired",
    "truncatedSkipped",
    "truncated",
    "canceled",
    "configureProvider",
    "quotaReached",
    "quotaHourly",
    "quotaMinute",
    "quotaLimited",
)


def component_type_from_id(component_id: str) -> str:
    """Map a canonical component ID from locales/components/*.json to its Studio catalog type."""
    if component_id in COMPONENT_TYPE_BY_ID:
        return COMPONENT_TYPE_BY_ID[component_id]
    return "".join(part[:1].upper() + part[1:] for part in component_id.split("-"))


def _format_component_label(component_type: str, localized_name: str) -> str:
    if not localized_name or localized_name.casefold() == component_type.casefold():
        return component_type
    return f"{localized_name} {component_type}"


@lru_cache(maxsize=len(EXTRA_LOCALE_KEYS))
def studio_locale(language: str) -> dict:
    """Load the authored Studio-specific locale overrides for one non-en/zh language."""
    if language not in EXTRA_LOCALE_KEYS:
        raise KeyError(f"Unsupported extra Studio locale: {language}")
    path = STUDIO_LOCALES_ROOT / f"{language}.json"
    return json.loads(path.read_text(encoding="utf-8"))


def _validate_mapping(
    section_name: str,
    language: str,
    mapping: object,
    expected_keys: set[str],
    baselines: dict[str, str | tuple[str, ...]] | None = None,
) -> None:
    if not isinstance(mapping, dict):
        raise ValueError(f"Studio {language}/{section_name} must be an object")
    actual_keys = set(mapping)
    if actual_keys != expected_keys:
        missing = sorted(expected_keys - actual_keys)
        extra = sorted(actual_keys - expected_keys)
        raise ValueError(
            f"Studio {language}/{section_name} key mismatch (missing={missing}, extra={extra})"
        )
    for key, value in mapping.items():
        baseline = baselines.get(key) if baselines else None
        if isinstance(baseline, tuple):
            if (
                not isinstance(value, list)
                or len(value) != len(baseline)
                or not all(isinstance(item, str) and item.strip() for item in value)
            ):
                raise ValueError(f"Studio {language}/{section_name}/{key} must be {len(baseline)} non-empty strings")
            continue
        if not isinstance(value, str) or not value.strip():
            raise ValueError(f"Empty Studio string in {language}/{section_name}/{key}")
        if isinstance(baseline, str):
            expected_placeholders = set(PLACEHOLDER_RE.findall(baseline))
            actual_placeholders = set(PLACEHOLDER_RE.findall(value))
            if actual_placeholders != expected_placeholders:
                raise ValueError(
                    f"Placeholder mismatch in {language}/{section_name}/{key}: "
                    f"expected {sorted(expected_placeholders)}, got {sorted(actual_placeholders)}"
                )


def validate_studio_locales() -> None:
    """Validate 100% key coverage, placeholder parity, and shared doc reuse for all 7 extra Studio locales."""
    expected_sections = {"ui", "categories", "components", "templates", "props", "options", "aiErrors"}
    expected_ui_keys = set(STUDIO_UI_KEYS)
    expected_cat_keys = set(STUDIO_EXTRA_CATEGORY_KEYS)
    expected_comp_keys = set(STUDIO_LAYOUT_COMPONENT_KEYS)
    expected_tpl_keys = set(STUDIO_TEMPLATE_KEYS)
    expected_prop_keys = set(STUDIO_EXTRA_PROP_KEYS)
    expected_opt_keys = set(STUDIO_EXTRA_OPTION_KEYS)
    expected_ai_err_keys = set(STUDIO_AI_ERROR_KEYS)

    for language in EXTRA_LOCALE_KEYS:
        data = studio_locale(language)
        if not isinstance(data, dict) or set(data) != expected_sections:
            raise ValueError(f"Studio locale sections mismatch in {language}: {sorted(data)}")

        _validate_mapping("ui", language, data["ui"], expected_ui_keys, STUDIO_UI_KEYS)
        _validate_mapping("categories", language, data["categories"], expected_cat_keys)
        _validate_mapping("components", language, data["components"], expected_comp_keys)
        _validate_mapping("props", language, data["props"], expected_prop_keys)
        _validate_mapping("options", language, data["options"], expected_opt_keys)
        _validate_mapping("aiErrors", language, data["aiErrors"], expected_ai_err_keys)

        templates = data["templates"]
        if not isinstance(templates, dict) or set(templates) != expected_tpl_keys:
            raise ValueError(f"Studio {language}/templates key mismatch")
        for tid, tpl in templates.items():
            if (
                not isinstance(tpl, dict)
                or set(tpl) != {"label", "description"}
                or not isinstance(tpl["label"], str)
                or not tpl["label"].strip()
                or not isinstance(tpl["description"], str)
                or not tpl["description"].strip()
            ):
                raise ValueError(f"Invalid template localization in {language}/templates/{tid}")

        pg_payload = json.loads(playground_runtime_payload(language).decode("utf-8"))
        pg_ui = pg_payload["ui"]
        missing_pg_ui = [src for src in PLAYGROUND_SHARED_UI_MAP.values() if src not in pg_ui]
        if missing_pg_ui:
            raise ValueError(f"Missing Playground UI keys for Studio in {language}: {missing_pg_ui}")

        gui_cats = guide_ui(language).get("categories", {})
        missing_cats = [cid for cid in GUIDE_CATEGORY_IDS if cid not in gui_cats]
        if missing_cats:
            raise ValueError(f"Missing guide category translations for Studio in {language}: {missing_cats}")

        comp_map = component_locale(language)
        if len(comp_map) < 99:
            raise ValueError(f"Incomplete component locale dictionary for Studio in {language}")


@lru_cache(maxsize=len(EXTRA_LOCALE_KEYS))
def studio_runtime_payload(language: str) -> str:
    """Build the self-contained merged runtime JSON bundle for one extra Studio locale."""
    data = studio_locale(language)
    pg_payload = json.loads(playground_runtime_payload(language).decode("utf-8"))
    pg_ui = pg_payload["ui"]

    ui = dict(data["ui"])
    for studio_key, pg_key in PLAYGROUND_SHARED_UI_MAP.items():
        ui[studio_key] = pg_ui[pg_key]

    guide_cats = guide_ui(language)["categories"]
    categories = {cid: guide_cats[cid] for cid in GUIDE_CATEGORY_IDS}
    categories.update(data["categories"])

    components = dict(data["components"])
    for cid, entry in component_locale(language).items():
        comp_type = component_type_from_id(cid)
        components[comp_type] = _format_component_label(comp_type, entry["name"])

    options = dict(pg_payload["options"])
    options.update(data["options"])

    strings = dict(pg_payload["strings"])
    strings.update(data["props"])

    bundle = {
        "ui": ui,
        "categories": categories,
        "components": components,
        "templates": data["templates"],
        "options": options,
        "strings": strings,
        "aiErrors": data["aiErrors"],
    }
    return json.dumps(bundle, ensure_ascii=False, separators=(",", ":"))
