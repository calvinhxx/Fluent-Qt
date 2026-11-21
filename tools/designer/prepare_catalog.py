"""Refresh the standalone Designer catalog and fixed-size icons from the Gallery."""
import argparse
import json
import os
import subprocess
from xml.etree import ElementTree as ET
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
CATALOG = ROOT / "docs/ai/generated/fluentqt-ai-catalog.json"

# A form body can be laid out freely before the application gives it to its owner.
FORM_TEMPLATES = {"Window", "Dialog", "ContentDialog", "SpatialItem"}
OVERLAYS = {
    "Popup": "Open popup", "Flyout": "Open flyout", "TeachingTip": "Show teaching tip",
    "CoachMark": "Show coach mark", "DrawerView": "Open drawer", "SplashScreen": "Show splash screen",
    "Toast": "Show notification", "ToolTip": "Show tip", "Menu": "Open menu",
    "CommandBarFlyout": "Show commands", "CommandPalette": "Find a command",
}
ADAPTERS = {"Menu": "Commands", "CommandBarFlyout": "Commands", "CommandPalette": "Commands",
            "SplashScreen": "TimedMessage", "Toast": "TimedMessage"}

TEXT_PROPERTY = {
    "Button": "Button", "CheckBox": "Check box", "RadioButton": "Radio button",
    "CompoundButton": "Compound button", "DropDownButton": "Choose", "HyperlinkButton": "Link",
    "RepeatButton": "Repeat", "SplitButton": "Choose", "ToggleButton": "Toggle",
    "ToggleSplitButton": "Toggle", "Label": "Label",
}
DEFAULT_STRINGS = {
    **{name: ("text", text) for name, text in TEXT_PROPERTY.items()},
    "LineEdit": ("placeholderText", "Enter text"), "PasswordBox": ("placeholderText", "Password"),
    "AutoSuggestBox": ("placeholderText", "Search"), "Expander": ("headerText", "Details"),
    "InfoBar": ("title", "Information"), "FontIcon": ("glyph", "Home"), "CodeBlock": ("code", "print(42)"),
}
# A widget without model data still needs a usable drop target in an unlaid-out form.
GROUP_SIZE = {
    "charts": (240, 140), "collections": (240, 144), "layout": (220, 108),
    "navigation": (220, 44), "scrolling": (220, 132), "particles": (240, 150),
    "spatial": (240, 150), "qml-bridge": (240, 150),
}
SIZES = {
    "FontIcon": (24, 24), "ColorPicker": (260, 260), "CalendarView": (320, 320),
    "DateTimeRangePicker": (320, 40), "RangeSlider": (180, 36), "ToggleSwitch": (90, 32),
    "CheckBox": (110, 32), "Card": (200, 100), "Divider": (160, 8), "Field": (220, 64),
    "DataGridToolBar": (280, 44), "MenuBar": (260, 30), "CommandBar": (260, 44),
    "NavigationView": (360, 220), "TabView": (260, 46), "Stepper": (260, 68),
    "ScrollBar": (16, 100), "AnnotatedScrollBar": (36, 100), "PipsPager": (140, 32),
    "InfoBadge": (24, 24), "Avatar": (48, 48), "InfoBar": (260, 54),
    "ProgressRing": (40, 40), "Shimmer": (160, 40), "TextEdit": (220, 110),
    "CodeBlock": (240, 96), "TitleBar": (260, 40),
}


def palette(gallery):
    groups = {group["id"]: group["title"] for group in gallery["categories"]}
    records = []
    for item in gallery["components"]:
        name, category = item["title"], item["category_id"]
        if name in FORM_TEMPLATES:
            continue
        cpp = item["cpp"]
        record = {
            "name": "Fluent" + name, "cpp": cpp["qualified_type"], "python": item["python"]["module"],
            "group": groups[category], "header": cpp["public_header"],
            "size": SIZES.get(name, GROUP_SIZE.get(category, (140, 32))),
        }
        if cpp["cmake_target"] != "FluentQt::FluentQt":
            record["extension"] = cpp["cmake_target"].split("::")[-1]
        if name in OVERLAYS:
            record.update(name=f"Fluent{name}Button", surface=name,
                          adapter=ADAPTERS.get(name, "Message"),
                          size=(170, 32), property="text", value=OVERLAYS[name])
        if name in DEFAULT_STRINGS:
            record["property"], record["value"] = DEFAULT_STRINGS[name]
        if name in {"HyperlinkButton", "SplitButton", "ToggleSplitButton"}:
            record["textConstructor"] = True
        if name == "Card":
            record["container"] = True
        records.append((record, ROOT / item["gallery"]["control_image"]))
    return records


def prepare_templates(check):
    from PySide6.QtCore import QFile, QIODevice, QLibraryInfo, QResource
    directory = HERE / "templates"
    rcc = Path(QLibraryInfo.path(QLibraryInfo.LibraryPath.LibraryExecutablesPath)) / (
        "rcc.exe" if os.name == "nt" else "rcc")
    entries = set()
    for manifest in sorted(directory.glob("*.qrc")):
        binary = manifest.with_suffix(".rcc")
        if not check:
            subprocess.run([str(rcc), "--binary", "--format-version", "1", "--no-compress",
                            "-o", str(binary), str(manifest)], check=True)
        mount = "/fluentqt-designer-check"
        if not QResource.registerResource(str(binary), mount):
            raise SystemExit(f"Missing Designer template resource: {binary}")
        try:
            for resource in ET.parse(manifest).getroot().findall("qresource"):
                for item in resource.findall("file"):
                    source = directory / item.text
                    entries.add(source.stem)
                    file = QFile(":" + mount + resource.get("prefix") + "/" + item.get("alias"))
                    if not file.open(QIODevice.OpenModeFlag.ReadOnly) or bytes(file.readAll()) != source.read_bytes():
                        raise SystemExit(f"Stale Designer template: {source}")
                    file.close()
        finally:
            QResource.unregisterResource(str(binary), mount)
    if entries != FORM_TEMPLATES:
        raise SystemExit(f"Unregistered Designer templates: {entries ^ FORM_TEMPLATES}")


def main():
    import sys
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    from PySide6.QtGui import QGuiApplication
    sys.path.insert(0, str(ROOT / "tools/gallery"))
    from export_control_images import matches_export, render_source
    app = QGuiApplication.instance() or QGuiApplication([])
    gallery = json.loads(CATALOG.read_text(encoding="utf-8"))
    missing = (FORM_TEMPLATES | OVERLAYS.keys()) - {item["title"] for item in gallery["components"]}
    if missing:
        raise SystemExit(f"Unknown Designer types: {sorted(missing)}")
    templates = {p.stem for p in (HERE / "templates").glob("*.ui")}
    if templates != FORM_TEMPLATES:
        raise SystemExit(f"Designer template inventory differs: {templates ^ FORM_TEMPLATES}")
    prepare_templates(args.check)
    records = palette(gallery)
    data = "[\n" + ",\n".join("  " + json.dumps(r, ensure_ascii=False) for r, _ in records) + "\n]\n"
    path = HERE / "widgets.json"
    icons = HERE / "icons"
    if args.check:
        if path.read_text(encoding="utf-8") != data:
            raise SystemExit("Designer catalog needs regeneration")
    else:
        path.write_text(data, encoding="utf-8")
        icons.mkdir(exist_ok=True)
    for record, source in records:
        path = icons / (record["name"] + ".png")
        image = render_source(source)
        if args.check:
            if not matches_export(path, image):
                raise SystemExit(f"Stale Designer icon: {path}")
        elif not image.save(str(path)):
            raise SystemExit(f"Could not write {path}")
    expected = {r["name"] for r, _ in records}
    if {p.stem for p in icons.glob("*.png")} != expected:
        raise SystemExit("Designer icon inventory differs from the catalog")
    print(f"{len(records)} Designer controls, {len(FORM_TEMPLATES)} form templates")


if __name__ == "__main__":
    main()
