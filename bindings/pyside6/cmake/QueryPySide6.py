"""Report the active PySide6/Shiboken6 wheel layout to CMake.

Keep this script compatible with the Python versions supported by PySide 6.2.
"""

from pathlib import Path
import shutil
import sys

import PySide6
from PySide6.QtCore import qVersion
from PySide6.QtGui import QImage
import shiboken6
import shiboken6_generator


def cmake_path(path):
    return str(path.resolve()).replace("\\", "/")


def cmake_value(value):
    return str(value).replace("\\", "/").replace(";", "\\;").replace('"', '\\"')


def find_generator(generator_root):
    executable_name = "shiboken6.exe" if sys.platform == "win32" else "shiboken6"
    candidates = [
        Path(sys.executable).resolve().parent / executable_name,
        generator_root / executable_name,
    ]
    located = shutil.which("shiboken6")
    if located:
        candidates.append(Path(located))
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise RuntimeError("Unable to locate the shiboken6 generator executable")


def find_include_root(shiboken_root, generator_root):
    for candidate in (shiboken_root / "include", generator_root / "include"):
        if (candidate / "sbkpython.h").is_file():
            return candidate
    raise RuntimeError("Unable to locate the Shiboken6 runtime headers")


def find_link_library(root, stem):
    if sys.platform == "win32":
        patterns = ("{0}*.lib".format(stem),)
    elif sys.platform == "darwin":
        patterns = ("lib{0}*.dylib".format(stem),)
    else:
        patterns = ("lib{0}*.so*".format(stem),)

    candidates = []
    for pattern in patterns:
        candidates.extend(root.rglob(pattern))
    candidates = [
        path for path in candidates
        if path.is_file() and "qml" not in path.name.lower()
    ]
    if not candidates:
        raise RuntimeError(
            "Unable to locate the {0} link library below {1}".format(stem, root)
        )
    candidates.sort(key=lambda path: (len(path.parts), len(path.name), path.name))
    return candidates[0]


def emit(name, value):
    print('set({0} "{1}")'.format(name, cmake_value(value)))


def has_safe_none_returns():
    """Detect a borrowed-Py_None ABI defect in some Linux ARM64 wheels."""
    image = QImage(1, 1, QImage.Format_ARGB32)
    image.setPixel(0, 0, 0xFF000000)
    before = sys.getrefcount(None)
    for _ in range(8):
        image.setPixel(0, 0, 0xFF000000)
    return sys.getrefcount(None) >= before


def main():
    pyside_root = Path(PySide6.__file__).resolve().parent
    shiboken_root = Path(shiboken6.__file__).resolve().parent
    generator_root = Path(shiboken6_generator.__file__).resolve().parent

    pyside_include = pyside_root / "include"
    pyside_typesystems = pyside_root / "typesystems"
    if not (pyside_include / "pyside.h").is_file():
        raise RuntimeError("Unable to locate the PySide6 headers")
    if not (pyside_typesystems / "typesystem_widgets.xml").is_file():
        raise RuntimeError("Unable to locate the PySide6 type-system files")

    qt_root = pyside_root / "Qt"
    qt_runtime_dir = qt_root / ("bin" if sys.platform == "win32" else "lib")

    emit("FLUENTQT_PYSIDE6_VERSION", PySide6.__version__)
    emit("FLUENTQT_PYSIDE6_QT_VERSION", qVersion())
    emit("FLUENTQT_SHIBOKEN6_VERSION", shiboken6.__version__)
    emit("FLUENTQT_SHIBOKEN6_GENERATOR_VERSION",
         shiboken6_generator.__version__)
    emit("FLUENTQT_PYSIDE6_SAFE_NONE_RETURNS",
         "TRUE" if has_safe_none_returns() else "FALSE")
    emit("FLUENTQT_SHIBOKEN6_GENERATOR",
         cmake_path(find_generator(generator_root)))
    emit("FLUENTQT_PYSIDE6_ROOT", cmake_path(pyside_root))
    emit("FLUENTQT_PYSIDE6_INCLUDE_DIR", cmake_path(pyside_include))
    emit("FLUENTQT_PYSIDE6_TYPESYSTEM_DIR", cmake_path(pyside_typesystems))
    emit("FLUENTQT_SHIBOKEN6_ROOT", cmake_path(shiboken_root))
    emit("FLUENTQT_SHIBOKEN6_INCLUDE_DIR",
         cmake_path(find_include_root(shiboken_root, generator_root)))
    emit("FLUENTQT_PYSIDE6_LIBRARY",
         cmake_path(find_link_library(pyside_root, "pyside6")))
    emit("FLUENTQT_SHIBOKEN6_LIBRARY",
         cmake_path(find_link_library(shiboken_root, "shiboken6")))
    emit("FLUENTQT_PYSIDE6_QT_RUNTIME_DIR", cmake_path(qt_runtime_dir))


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        sys.stderr.write("PySide6 discovery failed: {0}\n".format(error))
        sys.exit(1)
