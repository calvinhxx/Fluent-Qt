#!/usr/bin/env python3
"""Ownership regressions run with the Python standard library, without Qt."""

from contextlib import redirect_stderr, redirect_stdout
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("check_compatibility_boundaries.py")
SPEC = importlib.util.spec_from_file_location("compatibility_boundaries", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
COMPONENT = "src/components/menus_toolbars/Menu.cpp"
ADAPTER = "src/compatibility/private/PopupCompat_p.cpp"
FACADE = "bindings/pyside6/src/fluentqt/menus_toolbars.py"
PY_ADAPTER = "bindings/pyside6/src/fluentqt/_qt_widgets.py"
GALLERY = "bindings/pyside6/gallery/src/fluentqt_gallery/window.py"


class CppBoundaryTest(unittest.TestCase):
    def test_version_branches_and_intermediate_constants_are_rejected_project_wide(self):
        for source in (
            "#if QT_VERSION<0x060000\n#endif", "#if QT_VERSION_MAJOR == 5\n#endif",
            "#if defined(QT_VERSION_MINOR)\n#endif", "#if QT_DEPRECATED_SINCE(5, 15)\n#endif",
            "#if \\\nQT_VERSION >= QT_VERSION_CHECK(6, 0, 0)\n#endif",
            "return QT_VERSION >= QT_VERSION_CHECK(6, 0, 0);",
            "constexpr bool legacy = QT_VERSION < 0x060000;",
            "#define OLD_QT (QT_VERSION_MAJOR == 5)",
            "const int width = 1'000; auto old = QT_VERSION < 0x060000;",
        ):
            for path in (COMPONENT, "app/Sample.cpp", "tests/components/TestMenu.cpp"):
                with self.subTest(source=source, path=path):
                    self.assertTrue(MODULE.cpp_violations(source, path))
            self.assertFalse(MODULE.cpp_violations(source, ADAPTER))

    def test_indirect_capability_and_platform_guards_are_rejected_in_components(self):
        for token in (
            "FLUENT_QUICK_USES_RHI", "FLUENT_HAS_ACCESSIBLE_SELECTION_INTERFACE",
            "FLUENT_QT_HAS_RHI", "FLUENT_OPENGL_WIDGET_AVAILABLE", "Q_OS_MACOS",
            "__APPLE__", "_WIN32", "_WIN64", "__linux__", "__EMSCRIPTEN__",
        ):
            with self.subTest(token=token):
                self.assertTrue(MODULE.cpp_violations(f"#if {token}\n#endif", COMPONENT))
                self.assertTrue(MODULE.cpp_violations(f"#if {token}\n#endif", "support/spatial/Render.cpp"))
                self.assertFalse(MODULE.cpp_violations(f"#if {token}\n#endif", "app/platform/desktop/App.cpp"))

    def test_runtime_platform_and_version_selection_are_rejected(self):
        for source in (
            "QGuiApplication::platformName()", "QApplication::platformName()",
            "WindowChromeCompat::currentPlatform()", "qVersion()",
            "QLibraryInfo::version().majorVersion()", "QOperatingSystemVersion::current()",
            "QSysInfo::productType()",
        ):
            with self.subTest(source=source):
                self.assertTrue(MODULE.cpp_violations(source, COMPONENT))
                self.assertFalse(MODULE.cpp_violations(source, ADAPTER))

    def test_native_operations_are_rejected_even_without_a_platform_branch(self):
        for method in MODULE.NATIVE_METHODS:
            with self.subTest(method=method):
                self.assertTrue(MODULE.cpp_violations(f"window->{method}(host);", COMPONENT))
                self.assertFalse(MODULE.cpp_violations(f"window->{method}(host);", ADAPTER))
        self.assertTrue(MODULE.cpp_violations("QNativeInterface::QWindowsWindow* native;", COMPONENT))
        self.assertTrue(MODULE.cpp_violations("QPlatformNativeInterface* native;", COMPONENT))

    def test_pointer_reconstruction_is_rejected_but_handler_signatures_are_allowed(self):
        for source in (
            "QMouseEvent forwarded(event->type(), local, global);",
            "auto enter = new QEnterEvent(local, scene, global);",
            "return QMouseEvent{type, local, global};",
        ):
            self.assertTrue(MODULE.cpp_violations(source, COMPONENT))
        self.assertFalse(MODULE.cpp_violations("void mouseMoveEvent(QMouseEvent* event) override;", COMPONENT))

    def test_common_widget_flow_and_adapter_calls_remain_legal(self):
        source = """
            #include <QWindow>
            #if QT_CONFIG(accessibility)
            #endif
            auto window = widget->windowHandle();
            if (window && window->isExposed()) update();
            auto identity = widget->internalWinId();
            window->setSurfaceType(surfaceType);
            widget->setWindowFlags(Qt::Popup);
            widget->setAttribute(Qt::WA_NativeWindow);
            widget->grabMouse(); widget->releaseMouse();
            auto screen = QGuiApplication::screenAt(global);
            auto rect = screen->availableGeometry();
            const auto dpr = widget->devicePixelRatioF();
            auto icon = fluentIconPixmapForLogicalExtent(icon, size, window);
            return chrome.handleNativeEvent(eventType, message, result);
            popupHost.attach(menu, host);
        """
        self.assertFalse(MODULE.cpp_violations(source, COMPONENT))

    def test_literals_comments_samples_and_diagnostic_strings_are_ignored(self):
        source = r'''
            // popup->setTransientParent(host); QT_VERSION_CHECK
            /* #include <windows.h> */
            const char* sample = "#if Q_OS_WIN\nwindow->winId();";
            auto raw = R"cpp(
                #include <windows.h>
                #if QT_VERSION_MAJOR == 5
                QGuiApplication::platformName();
            )cpp";
            auto version = QT_VERSION_STR;
            auto letter = 'x';
        '''
        self.assertFalse(MODULE.cpp_violations(source, COMPONENT))

    def test_spliced_lines_keep_original_diagnostics_and_comment_semantics(self):
        source = "// ignored \\\nwindow->winId();\n\nwindow->setTran\\\nsientParent(host);\n"
        issues = MODULE.cpp_violations(source, COMPONENT)
        self.assertEqual([(issue.line, issue.rule) for issue in issues], [(4, "native-operation")])

    def test_platform_backend_headers_are_rejected(self):
        for header in (
            "windows.h", "qt_windows.h", "dwmapi.h", "X11/Xlib.h", "xcb/xcb.h", "AppKit/AppKit.h",
            "emscripten.h", "emscripten/emscripten.h", "QtGui/qguiapplication_platform.h",
            "QtGui/qpa/qplatformnativeinterface.h",
        ):
            with self.subTest(header=header):
                source = f'#include <{header}>\n'
                self.assertTrue(MODULE.cpp_violations(source, COMPONENT))
                self.assertFalse(MODULE.cpp_violations(source, ADAPTER))
        self.assertFalse(MODULE.cpp_violations('#include "windows.hpp"', COMPONENT))
        self.assertFalse(MODULE.cpp_violations('#include "emscripten_helpers.h"', COMPONENT))

    def test_adapter_cannot_depend_on_component_implementations(self):
        for header in ("components/basicinput/Button.h", "../../components/basicinput/Button.h"):
            self.assertTrue(MODULE.cpp_violations(f'#include "{header}"', ADAPTER))
        self.assertFalse(MODULE.cpp_violations('#include <QWidget>\n#include "WidgetCompat.h"', ADAPTER))
        self.assertFalse(MODULE.cpp_violations('auto sample = R"(#include "components/Foo.h")";', ADAPTER))

    def test_only_the_adapter_runtime_fixture_can_construct_both_qt_versions(self):
        source = "#if QT_VERSION_MAJOR == 5\n#endif"
        self.assertFalse(MODULE.cpp_violations(source, "tests/components/TestQtCompat.cpp"))
        self.assertTrue(MODULE.cpp_violations(source, "tests/components/TestQtCompatOther.cpp"))

    def test_adapter_dependency_check_follows_public_forwarders_and_local_cycles(self):
        sources = {
            ADAPTER: '#include <FluentQt/TextFields.h>',
            "include/FluentQt/TextFields.h": '#include "Shared.h"\n#include "components/textfields/LineEdit.h"',
            "include/FluentQt/Shared.h": '#include "TextFields.h"',
            "src/components/textfields/LineEdit.h": "#include <QWidget>",
        }
        issues = MODULE.adapter_dependencies(sources)
        self.assertEqual(len(issues), 1)
        self.assertEqual(issues[0].path, ADAPTER)
        self.assertIn("FluentQt/TextFields.h", issues[0].message)
        self.assertIn("components/textfields/LineEdit.h", issues[0].message)
        sources[ADAPTER] = '#include "compatibility/WidgetCompat.h"'
        sources["src/compatibility/WidgetCompat.h"] = "#include <QWidget>"
        self.assertFalse(MODULE.adapter_dependencies(sources))


class PythonBoundaryTest(unittest.TestCase):
    def test_import_aliases_and_indirect_version_values_cannot_bypass_the_boundary(self):
        for source in (
            "from PySide6.QtCore import qVersion as version\nlegacy = version() < '6.8'",
            "v = tuple(map(int, qVersion().split('.')))\nlegacy = v < (6, 8)",
            "from PySide6 import __version_info__ as version\nlegacy = version[:2] == (6, 2)",
            "from PySide6 import QtCore as core\nlegacy = core.qVersion() < '6.8'",
            "import PySide6 as qt\nlegacy = qt.__version_info__[:2] < (6, 8)",
            "from PySide6.QtCore import QLibraryInfo as Info\nlegacy = Info.version().majorVersion() == 6",
            "from PySide6 import QtCore as core\nlegacy = core.QLibraryInfo.version().minorVersion() < 5",
            "if qVersion().startswith('6.2'):\n    fallback()",
            "core = QtCore\nlegacy = core.qVersion() < '6.8'",
            "version = qVersion\nlegacy = version() < '6.8'",
            "Info = QLibraryInfo\nlegacy = Info.version().majorVersion() < 6",
            "core = QtCore\nversion = core.qVersion\ncurrent = version()\nlegacy = current < '6.8'",
        ):
            with self.subTest(source=source):
                self.assertTrue(MODULE.python_violations(source, FACADE))
                self.assertTrue(MODULE.python_violations(source, GALLERY))
                self.assertFalse(MODULE.python_violations(source, PY_ADAPTER))

    def test_capability_probes_and_attribute_error_fallbacks_belong_in_adapters(self):
        for source in (
            "from PySide6.QtCore import QEvent as Event\nlegacy = hasattr(Event.Type, 'DevicePixelRatioChange')",
            "event = getattr(QEvent.Type, 'DevicePixelRatioChange', None)",
            "from PySide6.QtCore import QEvent\nEvent = QEvent\nlegacy = hasattr(Event, 'DevicePixelRatioChange')",
            "from PySide6.QtGui import QGuiApplication\ntry:\n    QGuiApplication.styleHints().colorScheme()\nexcept AttributeError:\n    pass",
            "from PySide6.QtGui import QGuiApplication\nhints = QGuiApplication.styleHints()\ntry:\n    hints.colorScheme()\nexcept (ValueError, AttributeError):\n    pass",
        ):
            with self.subTest(source=source):
                self.assertTrue(MODULE.python_violations(source, FACADE))
                self.assertFalse(MODULE.python_violations(source, PY_ADAPTER))

    def test_diagnostics_and_application_attribute_checks_remain_legal(self):
        for source in (
            "info = {'qt_version': qVersion()}",
            "optional = getattr(widget, 'customProperty', None)",
            "sample = 'qVersion() < version'",
            "try:\n    widget.customProperty()\nexcept AttributeError:\n    pass",
        ):
            self.assertFalse(MODULE.python_violations(source, FACADE))

    def test_native_operations_do_not_need_a_conditional_to_be_rejected(self):
        for method in MODULE.NATIVE_METHODS:
            source = f"window.{method}()"
            self.assertTrue(MODULE.python_violations(source, FACADE))
            self.assertFalse(MODULE.python_violations(source, PY_ADAPTER))
        self.assertFalse(MODULE.python_violations("window.windowHandle().isExposed()", FACADE))

    def test_os_import_aliases_are_rejected_in_library_facades(self):
        for source in (
            "import sys as system\nwin = system.platform == 'win32'",
            "from sys import platform as host\nwin = host == 'win32'",
            "import os\nwin = os.name == 'nt'",
            "from platform import system as host\nname = host()",
            "from PySide6.QtGui import QGuiApplication as App\nname = App.platformName()",
        ):
            self.assertTrue(MODULE.python_violations(source, FACADE))
            self.assertFalse(MODULE.python_violations(source, PY_ADAPTER))

    def test_gallery_os_policy_and_package_dll_bootstrap_remain_legal(self):
        source = "import sys\nif sys.platform == 'darwin':\n    close_to_tray = True"
        self.assertFalse(MODULE.python_violations(source, GALLERY))
        self.assertFalse(MODULE.python_violations(source, "bindings/pyside6/src/fluentqt/__init__.py"))
        self.assertTrue(MODULE.python_violations("window.setTransientParent(host)", GALLERY))

    def test_private_adapters_cannot_import_facades_but_can_share_other_adapters(self):
        for source in (
            "from .menus_toolbars import CommandPalette",
            "from fluentqt import Button",
            "import fluentqt",
            "import fluentqt.menus_toolbars",
            "import fluentqt_gallery.visual",
            "from fluentqt_gallery import window",
        ):
            self.assertTrue(MODULE.python_violations(source, PY_ADAPTER))
        for source in (
            "from PySide6.QtWidgets import QWidget",
            "from ._qt_compat import delete_qobject",
            "from . import _qt_compat",
            "from fluentqt._qt_compat import delete_qobject",
            "import fluentqt._qt_compat",
        ):
            self.assertFalse(MODULE.python_violations(source, PY_ADAPTER))


class SourceGateTest(unittest.TestCase):
    def make_root(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        for directory in (*MODULE.CPP_ROOTS, *MODULE.PYTHON_ROOTS):
            (root / directory).mkdir(parents=True, exist_ok=True)
        (root / "src/components").mkdir()
        return root

    def test_new_untracked_sources_are_checked_without_git_or_qt(self):
        root = self.make_root()
        path = root / "src/components/NewMenu.hpp"
        path.write_text("window->setTransientParent(host);\n", encoding="utf-8")
        result = subprocess.run([sys.executable, "-S", str(SCRIPT), "--project-root", str(root)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/components/NewMenu.hpp:1: native-operation", result.stdout)
        path.write_text("popupHost.attach(menu, host);\n", encoding="utf-8")
        result = subprocess.run([sys.executable, "-S", str(SCRIPT), "--project-root", str(root)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_python_syntax_errors_fail_instead_of_skipping_the_file(self):
        root = self.make_root()
        (root / FACADE).write_text("if broken\n", encoding="utf-8")
        issues, count = MODULE.scan_project(root)
        self.assertEqual(count, 1)
        self.assertEqual(issues[0].rule, "python-syntax")

    def test_missing_or_empty_source_tree_cannot_pass(self):
        for root in (self.make_root(), Path(tempfile.gettempdir()) / "not-a-fluentqt-checkout"):
            with self.subTest(root=root), redirect_stderr(io.StringIO()):
                self.assertEqual(MODULE.main(["--project-root", str(root)]), 1)

    def test_repository_has_no_scattered_adaptations(self):
        with redirect_stdout(io.StringIO()) as output:
            result = MODULE.main(["--project-root", str(MODULE.ROOT)])
        self.assertEqual(result, 0, output.getvalue())


if __name__ == "__main__":
    unittest.main()
