"""Generate the C++ form adapters and plugin registration for one SDK configuration."""
import argparse
from pathlib import Path
from xml.etree import ElementTree as ET

from fluentqt_designer_catalog import DIRECTORY, WIDGETS, widget_xml


def write(path, content):
    if not path.exists() or path.read_text(encoding="utf-8") != content:
        path.write_text(content, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("extensions", nargs="*")
    args = parser.parse_args()
    widgets = [w for w in WIDGETS if not w.get("extension") or w["extension"] in args.extensions]
    args.output.mkdir(parents=True, exist_ok=True)
    header = ["#pragma once", ""]
    header += ["#include " + path for path in sorted({w["header"] for w in widgets})]
    header.append('#include "DesignerSurfaces.h"')
    entries = []
    root = ET.Element("RCC")
    resources = ET.SubElement(root, "qresource", prefix="/fluentqt/designer/icons")
    for widget in widgets:
        name, base = widget["name"], widget["cpp"]
        params = "QString(), parent" if widget.get("textConstructor") else "parent"
        if "adapter" in widget:
            base = "fluentqt_designer::" + widget["adapter"] + "Button"
            params = "fluentqt_designer::Surface::" + widget["surface"] + ", parent"
        header.extend(["", f"class {name} : public {base} {{", "    Q_OBJECT", "public:",
                       f"    explicit {name}(QWidget* parent = nullptr) : {base}({params}) {{}}", "};"])
        container = "true" if widget.get("container") else "false"
        entries.append(f'entry<{name}>(this, "FluentQt / {widget["group"]}", '
                       f'R"xml({widget_xml(widget)})xml", {container}),')
        icon = f"{name}.png"
        ET.SubElement(resources, "file", alias=icon).text = (DIRECTORY / "icons" / icon).as_posix()
    write(args.output / "fluentqt_designer.h", "\n".join(header) + "\n")
    write(args.output / "widget_entries.inc", "\n".join(entries) + "\n")
    write(args.output / "widget_icons.qrc", ET.tostring(root, encoding="unicode") + "\n")


if __name__ == "__main__":
    main()
