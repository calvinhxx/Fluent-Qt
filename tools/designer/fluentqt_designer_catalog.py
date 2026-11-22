"""Shared Designer metadata. Importing this module does not load Qt."""
import json
from pathlib import Path
from xml.etree import ElementTree as ET

DIRECTORY = Path(__file__).resolve().parent
WIDGETS = json.loads((DIRECTORY / "widgets.json").read_text(encoding="utf-8"))


PAGE_METHODS = {"FluentField": "setEditor", "FluentExpander": "setContentWidget",
                "FluentScrollView": "setContentWidget"}


def form_data_property(entry):
    if entry["group"] == "Charts":
        return "formSeries"
    if entry["name"] in {"FluentStepper", "FluentBreadcrumb", "FluentPivot", "FluentSelectorBar", "FluentListView"}:
        return "formItems"
    return None


def widget_xml(entry):
    name = entry["name"].removeprefix("Fluent")
    root = ET.Element("ui", language="c++")
    widget = ET.SubElement(root, "widget", {"class": entry["name"], "name": name[:1].lower() + name[1:]})
    rect = ET.SubElement(ET.SubElement(widget, "property", name="geometry"), "rect")
    for key, value in zip(("x", "y", "width", "height"), (0, 0, *entry["size"])):
        ET.SubElement(rect, key).text = str(value)
    if "property" in entry:
        prop = ET.SubElement(widget, "property", name=entry["property"])
        ET.SubElement(prop, "string").text = entry["value"]
    if entry["name"] in PAGE_METHODS:
        custom = ET.SubElement(ET.SubElement(root, "customwidgets"), "customwidget")
        ET.SubElement(custom, "class").text = entry["name"]
        ET.SubElement(custom, "addpagemethod").text = "setFormPage"
    return ET.tostring(root, encoding="unicode")
