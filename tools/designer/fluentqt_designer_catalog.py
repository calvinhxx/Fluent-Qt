"""Shared Designer metadata. Importing this module does not load Qt."""
import json
from pathlib import Path
from xml.etree import ElementTree as ET

DIRECTORY = Path(__file__).resolve().parent
WIDGETS = json.loads((DIRECTORY / "widgets.json").read_text(encoding="utf-8"))


PAGE_METHODS = {"FluentField": "setEditor", "FluentExpander": "setContentWidget",
                "FluentScrollView": "setContentWidget"}

PROPERTY_TOOLTIPS = {
    "FluentTextEdit": {
        "autoResizeEnabled": (
            "By default, the editor adjusts its height to fit the text. "
            "Uncheck to let the parent layout set its height.\n"
            "默认按文本内容调整高度。要让编辑框随父布局拉伸，请取消勾选。"
        ),
    },
}


def form_data_property(entry):
    if entry["name"] == "FluentDataGrid":
        return "formTable"
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
    tooltips = PROPERTY_TOOLTIPS.get(entry["name"], {})
    if entry["name"] in PAGE_METHODS or tooltips:
        custom = ET.SubElement(ET.SubElement(root, "customwidgets"), "customwidget")
        ET.SubElement(custom, "class").text = entry["name"]
        if entry["name"] in PAGE_METHODS:
            ET.SubElement(custom, "addpagemethod").text = "setFormPage"
        if tooltips:
            specs = ET.SubElement(custom, "propertyspecifications")
            for prop_name, text in tooltips.items():
                ET.SubElement(specs, "tooltip", name=prop_name).text = text
    return ET.tostring(root, encoding="us-ascii").decode("ascii")
