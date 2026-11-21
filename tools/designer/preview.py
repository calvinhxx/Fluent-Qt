"""Run a saved Designer form with the FluentQt PySide6 widgets."""

import argparse
from pathlib import Path
from xml.etree import ElementTree as ET

from PySide6.QtCore import QFile, QIODevice
from PySide6.QtUiTools import QUiLoader
from PySide6.QtWidgets import QApplication

import fluentqt
import fluentqt_designer
from fluentqt_designer_forms import make_form_host, show_form_host


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("form", type=Path, nargs="?", default=Path(__file__).with_name("preview.ui"))
    args = parser.parse_args()
    fluentqt.prepare_high_dpi_application()
    app = QApplication([])
    fluentqt.initialize_resources()
    loader = QUiLoader()
    try:
        classes = {w.get("class", "") for w in ET.parse(args.form).iter("widget")}
        for name in sorted(classes):
            if name.startswith("Fluent"):
                loader.registerCustomWidget(getattr(fluentqt_designer, name))
    except (OSError, ET.ParseError, AttributeError, ImportError) as exc:
        parser.error(str(exc))
    form = QFile(str(args.form))
    if not form.open(QIODevice.OpenModeFlag.ReadOnly):
        parser.error(form.errorString())
    try:
        window = loader.load(form)
    finally:
        form.close()
    if window is None:
        parser.error(loader.errorString())
    try:
        host = make_form_host(window)
    except (ValueError, ImportError) as exc:
        parser.error(str(exc))
    show_form_host(host)
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
