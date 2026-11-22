"""Inspect the exported forms inside the actual Designer form editor."""
import json
import os
from pathlib import Path
import traceback
from xml.etree import ElementTree as ET

from PySide6.QtCore import QTimer
from PySide6.QtDesigner import QDesignerFormWindowInterface
from PySide6.QtWidgets import QApplication, QWidget

from fluentqt_designer_catalog import WIDGETS

DIRECTORY = Path(os.environ["STUDIO_EXPORT_DIR"])
templates = json.loads((DIRECTORY / "templates.json").read_text())
classes = {w.get("surface", w["name"].removeprefix("Fluent")): w["name"] for w in WIDGETS}


def verify(form, root):
    page = form.mainContainer()
    saved = ET.fromstring(form.contents())
    serialized = {w.get("name"): w.get("class") for w in saved.iter("widget")}

    def inspect(node, widget):
        kind = node["type"]
        if kind in ("Grid", "VBox", "HBox", "AnchorBox"):
            expected = {"Grid": "QGridLayout", "AnchorBox": "QGridLayout",
                        "VBox": "QVBoxLayout", "HBox": "QHBoxLayout"}[kind]
            assert widget.layout().inherits(expected), (node["id"], expected)
        elif node["id"] != root["id"]:
            assert serialized.get(node["id"]) == classes[kind], (node["id"], serialized.get(node["id"]))
            assert widget.inherits(classes[kind]), (node["id"], widget.metaObject().className())
        for index, child in enumerate(node.get("children", [])):
            control = widget.findChild(QWidget, child["id"])
            assert control is not None, child["id"]
            if kind == "Field":
                assert widget.editor() == control, child["id"]
            if kind == "Expander":
                assert widget.contentWidget().isAncestorOf(control), child["id"]
            if kind == "Grid":
                assert widget.layout().getItemPosition(widget.layout().indexOf(control))[:2] == divmod(index, node["props"]["columns"])
            if child.get("props", {}).get("anchorPosition") == "Center":
                center = control.mapTo(widget, control.rect().center())
                assert (center - widget.rect().center()).manhattanLength() <= 4, (child["id"], center, widget.rect())
            inspect(child, control)
        props = node.get("props", {})
        if kind in ("LineChart", "BarChart", "ListView"):
            assert widget.model() and widget.model().rowCount() == len(props.get("values", props.get("items"))), node["id"]
        if kind in ("Breadcrumb", "Stepper"):
            assert widget.itemCount() == len(props["items"]), node["id"]
        if kind == "ComboBox":
            assert [widget.itemText(i) for i in range(widget.count())] == props["items"], node["id"]
            assert widget.currentIndex() == props["currentIndex"], node["id"]
        if kind == "Button" and "fluentStyle" in props:
            assert widget.fluentStyle().name == props["fluentStyle"], node["id"]
        if kind == "Expander":
            assert widget.isExpanded() == props["expanded"], node["id"]
        if kind in ("Slider", "RatingControl", "ProgressRing", "ProgressBar", "InfoBadge"):
            assert widget.value() == props["value"], node["id"]
    inspect(root, page)


def inspect_forms():
    try:
        forms = {}
        for w in QApplication.allWidgets():
            f = QDesignerFormWindowInterface.findFormWindow(w)
            if f and f.mainContainer() and Path(f.fileName()).stem in templates:
                forms[Path(f.fileName()).stem] = f
        if forms.keys() != templates.keys():
            QTimer.singleShot(250, inspect_forms)
            return
        for name, form in sorted(forms.items()):
            form.core().formWindowManager().setActiveFormWindow(form)
            QApplication.processEvents()
            verify(form, templates[name])
            captures = os.environ.get("STUDIO_DESIGNER_CAPTURES")
            if captures:
                target = Path(captures)
                target.mkdir(parents=True, exist_ok=True)
                assert form.mainContainer().grab().save(str(target / (name + ".png")))
            print("Designer verified", name, flush=True)
        (DIRECTORY / "designer-verified.json").write_text(json.dumps(sorted(forms)))
        os._exit(0)
    except BaseException:
        traceback.print_exc()
        os._exit(1)


QTimer.singleShot(500, inspect_forms)
