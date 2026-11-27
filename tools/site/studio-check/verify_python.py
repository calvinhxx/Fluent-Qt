"""Run generated Python pages, then load the original .ui files in Qt Designer."""
import argparse
import json
import os
from pathlib import Path
import runpy
import signal
import site
import subprocess
import sys
import tempfile

from PySide6.QtCore import QDate, QTime, Qt
from PySide6.QtGui import QColor
from PySide6.QtWidgets import QApplication, QWidget
from fluentqt import initialize_resources
from fluentqt.collections import DataGrid
from fluentqt.layout import Accordion
from fluentqt.date_time import DatePicker, DateTimeRangePicker, TimePicker
from fluentqt.textfields import Label
from fluentqt.foundation import accent_color, set_accent_color

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("directory", type=Path)
parser.add_argument("--designer-tools", type=Path, required=True)
args = parser.parse_args()
directory = args.directory.resolve()
names = sorted(json.loads((directory / "templates.json").read_text(encoding="utf-8")))
app = QApplication([])
initialize_resources()
sys.path.insert(0, str(args.designer_tools.resolve()))
from compile_ui import compile_form


def load_form(path):
    namespace = {}
    exec(compile(compile_form(path), str(path), "exec"), namespace)
    form = QWidget()
    namespace["Ui_StudioPage"]().setupUi(form)
    return form


def verify_zoned_range(widget):
    picker = widget.findChild(DateTimeRangePicker)
    assert picker.startDateTime().toString(Qt.DateFormat.ISODate) == "2031-03-14T22:10:00+05:30"
    assert picker.endDateTime().toString(Qt.DateFormat.ISODate) == "2031-03-15T10:20:00Z"


host = QWidget()
host.show()
host_accent = QColor("#AA3366")
set_accent_color(host_accent)
for name in names:
    form = load_form(directory / (name + ".ui"))
    page = runpy.run_path(str(directory / (name + ".py")))["create_studio_page"](host)
    page.show()
    app.processEvents()
    assert page.isVisible() and accent_color() == host_accent, name
    if name == "device-monitor":
        for widget in (form, page):
            model = widget.findChild(DataGrid).model()
            assert model is not None and (model.rowCount(), model.columnCount()) == (4, 4), name
            assert model.index(0, 0).data() == "plc-edge-01", name
    if name == "accordion-pickers":
        for widget in (form, page):
            sections = widget.findChild(Accordion)
            assert sections.count() == 2
            assert [sections.itemAt(i).headerText() for i in range(2)] == ["Release Notes", "Details"]
            assert sections.itemAt(0).findChild(Label).text() == "Release Notes"
            empty_date, chosen_date = widget.findChildren(DatePicker)
            empty_time, chosen_time = widget.findChildren(TimePicker)
            assert not empty_date.selectedDate().isValid() and chosen_date.selectedDate() == QDate(2031, 3, 14)
            assert not empty_time.selectedTime().isValid() and chosen_time.selectedTime() == QTime(16, 12)
    if name == "zoned-range":
        for widget in (form, page):
            verify_zoned_range(widget)
    page.close()
    print("Python verified", name, flush=True)

inspector = Path(__file__).resolve().parent
tool = Path(sys.executable).with_name("pyside6-designer")
with tempfile.TemporaryDirectory(prefix="studio-designer-") as settings:
    saved_range = Path(settings) / "zoned-range.ui"
    env = dict(os.environ, STUDIO_EXPORT_DIR=str(directory), XDG_CONFIG_HOME=settings,
               STUDIO_SAVED_RANGE_UI=str(saved_range),
               PYTHONPATH=os.pathsep.join(map(str, [args.designer_tools.resolve(), inspector, *site.getsitepackages()])),
               PYSIDE_DESIGNER_PLUGINS=os.pathsep.join(map(str, [args.designer_tools.resolve(), inspector])))
    receipt = directory / "designer-verified.json"
    receipt.unlink(missing_ok=True)
    proc = subprocess.Popen([tool, "-style", "fusion", *(directory / (name + ".ui") for name in names)],
                            env=env, start_new_session=True)
    try:
        if proc.wait(timeout=60) != 0 or json.loads(receipt.read_text(encoding="utf-8")) != names:
            raise RuntimeError("Qt Designer did not verify all generated forms")
        if "zoned-range" in names:
            verify_zoned_range(load_form(saved_range))
            print("Designer round-trip verified zoned-range", flush=True)
    finally:
        if proc.poll() is None:
            if os.name == "nt":
                subprocess.run(["taskkill", "/pid", str(proc.pid), "/t", "/f"], check=False)
            else:
                os.killpg(proc.pid, signal.SIGTERM)
            proc.wait()
