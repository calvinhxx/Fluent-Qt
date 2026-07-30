"""Smoke-test an installed FluentQt wheel without using the source tree."""

from importlib import metadata
import os
from pathlib import Path
import sys

import fluentqt
import fluentqt._fluentqt as native
import PySide6
import shiboken6

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import Qt, qVersion
from PySide6.QtWidgets import QApplication, QWidget
from shiboken6 import Shiboken


def require_installed_below_prefix(path):
    prefix = Path(sys.prefix).resolve()
    resolved = Path(path).resolve()
    try:
        common = os.path.commonpath((str(prefix), str(resolved)))
    except ValueError:
        common = ""
    if os.path.normcase(common) != os.path.normcase(str(prefix)):
        raise AssertionError(
            "Expected {0} below clean environment {1}".format(resolved, prefix)
        )


def windows_loaded_modules():
    if sys.platform != "win32":
        return []

    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    get_current_process = kernel32.GetCurrentProcess
    get_current_process.argtypes = ()
    get_current_process.restype = wintypes.HANDLE
    process = get_current_process()
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    enum_modules = psapi.EnumProcessModules
    enum_modules.argtypes = (
        wintypes.HANDLE,
        ctypes.POINTER(wintypes.HMODULE),
        wintypes.DWORD,
        ctypes.POINTER(wintypes.DWORD),
    )
    enum_modules.restype = wintypes.BOOL
    module_filename = psapi.GetModuleFileNameExW
    module_filename.argtypes = (
        wintypes.HANDLE,
        wintypes.HMODULE,
        wintypes.LPWSTR,
        wintypes.DWORD,
    )
    module_filename.restype = wintypes.DWORD

    capacity = 256
    while True:
        modules = (wintypes.HMODULE * capacity)()
        needed = wintypes.DWORD()
        if not enum_modules(
            process,
            modules,
            ctypes.sizeof(modules),
            ctypes.byref(needed),
        ):
            raise ctypes.WinError(ctypes.get_last_error())
        count = needed.value // ctypes.sizeof(wintypes.HMODULE)
        if count <= capacity:
            break
        capacity = count

    paths = []
    for module in modules[:count]:
        buffer = ctypes.create_unicode_buffer(32768)
        if module_filename(process, module, buffer, len(buffer)):
            paths.append(Path(buffer.value).resolve())
    return paths


def verify_windows_runtime_dependencies():
    if sys.platform != "win32":
        return

    loaded = windows_loaded_modules()
    by_name = {path.name.lower(): path for path in loaded}
    required_qt = ("qt6core.dll", "qt6gui.dll", "qt6widgets.dll")
    missing_qt = [name for name in required_qt if name not in by_name]
    if missing_qt:
        raise AssertionError(
            "Expected loaded Qt dependencies: {0}".format(
                ", ".join(missing_qt)
            )
        )

    runtime_paths = [by_name[name] for name in required_qt]
    for prefix in ("pyside6", "shiboken6"):
        matches = [
            path
            for name, path in by_name.items()
            if name.startswith(prefix) and name.endswith(".dll")
        ]
        if not matches:
            raise AssertionError(
                "Expected a loaded {0} runtime DLL".format(prefix)
            )
        runtime_paths.extend(matches)

    runtime_paths = sorted(set(runtime_paths), key=lambda path: str(path).lower())
    for path in runtime_paths:
        require_installed_below_prefix(path)
        print("Windows wheel dependency: {0}".format(path))


def macos_loaded_images():
    if sys.platform != "darwin":
        return []

    import ctypes

    dyld = ctypes.CDLL(None)
    image_count = dyld._dyld_image_count
    image_count.argtypes = ()
    image_count.restype = ctypes.c_uint32
    image_name = dyld._dyld_get_image_name
    image_name.argtypes = (ctypes.c_uint32,)
    image_name.restype = ctypes.c_char_p

    paths = []
    for index in range(image_count()):
        value = image_name(index)
        if value:
            paths.append(Path(os.fsdecode(value)).resolve())
    return paths


def verify_macos_runtime_dependencies():
    if sys.platform != "darwin":
        return

    loaded = macos_loaded_images()
    requirements = {
        "QtCore": lambda path: (
            path.name == "QtCore" and "QtCore.framework" in path.parts
        ),
        "QtGui": lambda path: (
            path.name == "QtGui" and "QtGui.framework" in path.parts
        ),
        "QtWidgets": lambda path: (
            path.name == "QtWidgets" and "QtWidgets.framework" in path.parts
        ),
        "PySide6": lambda path: (
            path.name.startswith("libpyside6") and path.suffix == ".dylib"
        ),
        "Shiboken6": lambda path: (
            path.name.startswith("libshiboken6") and path.suffix == ".dylib"
        ),
    }

    runtime_paths = []
    for name, predicate in requirements.items():
        matches = [path for path in loaded if predicate(path)]
        if not matches:
            raise AssertionError(
                "Expected a loaded {0} runtime dependency".format(name)
            )
        runtime_paths.extend(matches)

    runtime_paths = sorted(set(runtime_paths), key=lambda path: str(path).lower())
    for path in runtime_paths:
        require_installed_below_prefix(path)
        print("macOS wheel dependency: {0}".format(path))


def main():
    require_installed_below_prefix(fluentqt.__file__)
    require_installed_below_prefix(native.__file__)

    expected_version = os.environ["FLUENTQT_EXPECTED_VERSION"]
    if metadata.version("FluentQt") != expected_version:
        raise AssertionError("Installed wheel metadata has the wrong version")

    app = QApplication.instance() or QApplication([])
    if not fluentqt.initialize_resources():
        raise AssertionError("FluentQt resources could not be initialized")

    info = fluentqt.binding_build_info()
    if info["fluentqt_version"] != expected_version:
        raise AssertionError("Native FluentQt version does not match the wheel")
    if info["pyside6_version"] != PySide6.__version__:
        raise AssertionError("PySide6 build and runtime versions differ")
    if info["shiboken6_version"] != shiboken6.__version__:
        raise AssertionError("Shiboken6 build and runtime versions differ")
    if info["qt_compile_version"] != qVersion():
        raise AssertionError("Qt build and runtime versions differ")

    controls = [
        fluentqt.Button("Button"),
        fluentqt.CheckBox("CheckBox"),
        fluentqt.HyperlinkButton("HyperlinkButton"),
        fluentqt.RadioButton("RadioButton"),
        fluentqt.RepeatButton("RepeatButton"),
        fluentqt.Slider(Qt.Horizontal),
        fluentqt.ToggleButton("ToggleButton"),
        fluentqt.ToggleSwitch(),
        fluentqt.Label("Label"),
        fluentqt.LineEdit(),
        fluentqt.NumberBox(),
        fluentqt.PasswordBox(),
        fluentqt.InfoBadge(),
        fluentqt.ProgressBar(),
        fluentqt.ProgressRing(),
        fluentqt.Shimmer(),
        fluentqt.Divider(),
    ]
    if any(not Shiboken.isValid(control) for control in controls):
        raise AssertionError("A wheel-installed component has an invalid wrapper")

    previous_theme = fluentqt.current_theme()
    try:
        fluentqt.set_theme(fluentqt.Theme.Dark)
        fluentqt.apply_style_theme(fluentqt.StyleTheme.Material)
        if (
            fluentqt.current_design_language()
            != fluentqt.DesignLanguage.DesignMaterial
        ):
            raise AssertionError("Installed theme adapter did not update tokens")
    finally:
        fluentqt.reset_theme_tokens()
        fluentqt.set_theme(previous_theme)

    window = fluentqt.Window()
    child = QWidget()
    window.setContentWidget(child)
    Shiboken.delete(window)
    if Shiboken.isValid(child):
        raise AssertionError("Window did not own its installed content widget")

    verify_windows_runtime_dependencies()
    verify_macos_runtime_dependencies()

    print(
        "FluentQt {0} wheel smoke passed with PySide6 {1} / Qt {2}".format(
            expected_version,
            PySide6.__version__,
            qVersion(),
        )
    )
    app.processEvents()


if __name__ == "__main__":
    main()
