"""Resolve visual documents from the canonical Gallery catalog and editorial text."""

import copy
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
CONFIG = ROOT / "tools/site/component-docs.json"
CATALOG = ROOT / "docs/ai/generated/fluentqt-ai-catalog.json"
TRANSLATIONS = ROOT / "tools/site/component-translations.json"


def catalog():
    return json.loads(CATALOG.read_text(encoding="utf-8"))


def config():
    value = json.loads(CONFIG.read_text(encoding="utf-8"))
    if value.get("schema_version") != 1:
        raise ValueError("Unsupported component documentation schema")
    curated = {item["id"]: item for item in value["components"]}
    if not value.get("include_catalog"):
        return copy.deepcopy(list(curated.values()))
    components = catalog()["components"]
    translations = json.loads(TRANSLATIONS.read_text(encoding="utf-8"))
    expected = {c["id"] for c in components} - curated.keys()
    if set(translations) != expected:
        raise ValueError(f"Component translations differ from the catalog: {set(translations) ^ expected}")
    documents = []
    for component in components:
        cid = component["id"]
        if cid in curated:
            documents.append(copy.deepcopy(curated[cid]))
            continue
        editorial = translations[cid]
        samples = component["samples"]
        sample_id = editorial.get("sample", samples[0]["id"])
        sample = next((s for s in samples if s["id"] == sample_id), None)
        if not sample or not sample["cpp_snippet"].strip():
            raise ValueError(f"Missing canonical example: {cid}/{sample_id}")
        name = {"en": component["title"], "zh": editorial["name"]}
        description = {"en": component["description"], "zh": editorial["description"]}
        preview = {"id": "basic", "sample": sample_id,
                   "name": {"en": "Example", "zh": "示例"},
                   "alt": {"en": f'{component["title"]}: {sample["title"]}',
                           "zh": f'{editorial["name"]}的真实 Gallery 示例'}}
        if "capture_size" in editorial:
            preview["capture_size"] = editorial["capture_size"]
        documents.append({"id": cid, "name": name, "description": description,
                          "previews": [preview],
                          "minimum_preview_height": editorial.get("height", 420 if component["category_id"] in
                              ("dialogs-flyouts", "menus-toolbars", "windowing") else 196),
                          "examples": [{"sample": sample_id, "preview": "basic",
                              "name": {"en": sample["title"], "zh": editorial.get("example", "基本用法")},
                              "description": {"en": sample["description"], "zh": editorial.get("usage", editorial["description"])}}]})
    return documents
