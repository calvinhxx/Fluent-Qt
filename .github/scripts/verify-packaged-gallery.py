#!/usr/bin/env python3
"""Check the packaged desktop Gallery catalog and a real QML scene."""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


@contextmanager
def extracted_package(package, dest):
    if package.suffix == ".dmg":
        subprocess.run(["hdiutil", "attach", "-readonly", "-nobrowse", "-mountpoint", dest, package],
                       input=b"Y\n", stdout=subprocess.DEVNULL, check=True, timeout=120)
        try:
            yield dest
        finally:
            subprocess.run(["hdiutil", "detach", dest], check=True, timeout=60)
    elif package.suffix == ".deb":
        subprocess.run(["dpkg-deb", "--extract", package, dest], check=True, timeout=120)
        yield dest
    elif package.suffix == ".exe":
        subprocess.run(["7z", "x", "-y", f"-o{dest}", package],
                       check=True, stdout=subprocess.DEVNULL, timeout=120)
        yield dest
    else:
        raise ValueError(f"Unsupported desktop installer: {package}")


def verify(package_root, work):
    executable_name = {"win32": "fluent_qt_gallery.exe", "darwin": "Fluent-Qt Gallery"}.get(
        sys.platform, "fluent_qt_gallery")
    binaries = [p for p in package_root.rglob(executable_name)
                if p.is_file() and p.parent.name in ("bin", "MacOS")]
    if len(binaries) != 1:
        raise ValueError(f"Expected one packaged Gallery executable, found {binaries}")
    catalog = Path(__file__).resolve().parents[2] / "docs/ai/generated/fluentqt-ai-catalog.json"
    expected = sorted(c["id"] for c in json.loads(catalog.read_text(encoding="utf-8"))["components"])

    status = "qmlBridgeLifecycleStatus"
    def check_status(text):
        return {"action": "wait", "milliseconds": 50, "target": status, "expect": {"text": text}}

    steps = [check_status("QML loaded"), {"action": "click", "target": "qmlBridgeDeactivate"},
             check_status("QML stopped"), {"action": "click", "target": "qmlBridgeReload"},
             check_status("QML loaded")]
    actions = work / "actions.json"
    actions.write_text(json.dumps({"schema_version": 1, "steps": steps}), encoding="utf-8")
    report = work / "report.json"
    env = os.environ.copy()
    for key in ("QML_IMPORT_PATH", "QML2_IMPORT_PATH", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH",
                "QT_QPA_PLATFORM", "DYLD_FRAMEWORK_PATH", "DYLD_LIBRARY_PATH", "LD_LIBRARY_PATH"):
        env.pop(key, None)
    env["QT_QUICK_BACKEND"] = "software"
    if sys.platform == "win32":
        env["PATH"] = str(Path(os.environ["SystemRoot"]) / "System32")
    elif sys.platform.startswith("linux"):
        env["QT_QPA_PLATFORM"] = "offscreen"
    subprocess.run([str(binaries[0]), "--preview", "--route", "qml-view", "--sample", "qml-view-lifecycle",
                    "--actions", str(actions), "--report", str(report)], cwd=work, env=env, timeout=60, check=True)
    result = json.loads(report.read_text(encoding="utf-8"))
    actual = sorted(result["component_routes"])
    if result["status"] != "ok" or actual != expected:
        raise RuntimeError(f"Packaged catalog or QML preview failed: {actual}")
    print(f"Verified {len(actual)} component routes and QmlView load, deactivate and reload.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path, help="Gallery DMG, DEB or NSIS installer")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="fluentqt-package-smoke-") as temporary:
        work = Path(temporary)
        dest = work / "package"
        dest.mkdir()
        with extracted_package(args.package.resolve(), dest) as package_root:
            verify(package_root, work)


if __name__ == "__main__":
    main()
