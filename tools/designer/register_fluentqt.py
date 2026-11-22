"""Register the installed FluentQt controls with pyside6-designer."""
from importlib.util import find_spec

from PySide6.QtCore import QResource
from PySide6.QtDesigner import QPyDesignerCustomWidgetCollection
from fluentqt import initialize_resources

from fluentqt_designer_catalog import DIRECTORY, PAGE_METHODS, widget_xml
from fluentqt_designer_containers import register_container
from fluentqt_designer import available_widgets

initialize_resources()
templates = ["fluentqt_form_templates"]
if find_spec("fluentqt.spatial") is not None:
    templates.append("fluentqt_spatial_template")
for resource in templates:
    path = DIRECTORY / "templates" / (resource + ".rcc")
    if not QResource.registerResource(str(path)):
        raise RuntimeError(f"Could not load FluentQt Designer templates from {path}")
for entry, widget in available_widgets():
    if entry["name"] in PAGE_METHODS:
        register_container(QPyDesignerCustomWidgetCollection, entry, widget)
        continue
    QPyDesignerCustomWidgetCollection.registerCustomWidget(
        widget,
        group="FluentQt / " + entry["group"],
        icon=str(DIRECTORY / "icons" / (entry["name"] + ".png")),
        module="fluentqt_designer.h",
        xml=widget_xml(entry),
        container=entry.get("container", False),
    )
