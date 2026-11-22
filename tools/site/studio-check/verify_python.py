"""Run generated Python pages, then load the original .ui files in Qt Designer."""
import argparse
import json
import os
from pathlib import Path
import runpy
import signal
import subprocess
import sys
import tempfile

from PySide6.QtWidgets import QApplication, QWidget
from fluentqt import initialize_resources

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("directory", type=Path)
parser.add_argument("--designer-tools", type=Path, required=True)
args = parser.parse_args()
directory = args.directory.resolve()
names = sorted(json.loads((directory / "templates.json").read_text()))
app = QApplication([])
initialize_resources()
sys.path.insert(0, str(args.designer_tools.resolve()))
for name in names:
    generated = subprocess.check_output([Path(sys.executable).with_name("pyside6-uic"), directory / (name + ".ui")])
    namespace = {}
    exec(compile(generated, name + "_ui.py", "exec"), namespace)
    form = QWidget()
    namespace["Ui_StudioPage"]().setupUi(form)
    page = runpy.run_path(str(directory / (name + ".py")))["create_studio_page"]()
    page.show()
    app.processEvents()
    assert page.isVisible(), name
    page.close()
    print("Python verified", name, flush=True)

inspector = Path(__file__).resolve().parent
tool = Path(sys.executable).with_name("pyside6-designer")
with tempfile.TemporaryDirectory(prefix="studio-designer-") as settings:
    env = dict(os.environ, STUDIO_EXPORT_DIR=str(directory), XDG_CONFIG_HOME=settings,
               PYTHONPATH=os.pathsep.join(map(str, [args.designer_tools.resolve(), inspector])),
               PYSIDE_DESIGNER_PLUGINS=os.pathsep.join(map(str, [args.designer_tools.resolve(), inspector])))
    receipt = directory / "designer-verified.json"
    receipt.unlink(missing_ok=True)
    proc = subprocess.Popen([tool, "-style", "fusion", *(directory / (name + ".ui") for name in names)],
                            env=env, start_new_session=True)
    try:
        if proc.wait(timeout=60) != 0 or json.loads(receipt.read_text()) != names:
            raise RuntimeError("Qt Designer did not verify all generated forms")
    finally:
        if proc.poll() is None:
            os.killpg(proc.pid, signal.SIGTERM)
            proc.wait()
