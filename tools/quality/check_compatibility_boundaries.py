#!/usr/bin/env python3
"""Check compatibility ownership without importing Qt or building the library."""

from __future__ import annotations

import argparse
import ast
from collections import deque
from dataclasses import dataclass
from pathlib import Path
import posixpath
import re
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
CPP_ROOTS = ("app", "src", "include", "examples", "tests", "support", "bindings/pyside6/native")
PYTHON_ROOTS = ("bindings/pyside6/src/fluentqt", "bindings/pyside6/gallery/src/fluentqt_gallery")
CPP_SUFFIXES = {".cpp", ".cc", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".m", ".mm", ".inc"}
NATIVE_METHODS = (
    "setTransientParent", "transientParent", "startSystemMove", "startSystemResize",
    "nativeInterface", "platformNativeInterface", "winId", "effectiveWinId",
)
CPP_RULES = (
    ("qt-version", r"\b(?:QT_VERSION(?:_(?:CHECK|MAJOR|MINOR|PATCH))?|QT_DEPRECATED_SINCE)\b",
     "Qt version adaptation belongs in src/compatibility/"),
    ("platform-branch", r"\b(?:Q_OS_\w+|_WIN32|_WIN64|__APPLE__|__linux__|__EMSCRIPTEN__|"
     r"FLUENT_QUICK_\w+|FLUENT_HAS_ACCESSIBLE_\w+|FLUENT_OPENGL_WIDGET_\w+|FLUENT_QT_HAS_RHI)\b",
     "Components must call an adapter instead of selecting a platform or Qt capability"),
    ("runtime-platform", r"\b(?:(?:QGuiApplication|QApplication)\s*::\s*platformName|"
     r"WindowChromeCompat\s*::\s*currentPlatform|QOperatingSystemVersion\s*::\s*current|"
     r"QSysInfo\s*::\s*(?:kernelType|productType|productVersion))\s*\(",
     "Runtime platform selection belongs in an adapter"),
    ("runtime-version", r"\b(?:qVersion|QLibraryInfo\s*::\s*version)\s*\(",
     "Components must not select behavior from a runtime Qt version"),
    ("event-construction", r"\b(?:QMouseEvent|QEnterEvent)\s*(?:\w+\s*)?[({]",
     "Use compatibility event construction to preserve Qt event metadata"),
    ("native-operation", r"\b(?:" + "|".join(NATIVE_METHODS) + r")\s*\(|\bQNativeInterface\s*::|\bQPlatformNativeInterface\b",
     "Native window operations belong in a compatibility adapter"),
)
CPP_TOKENS = re.compile(
    r'R"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)"'
    r'|"(?:\\[\s\S]|[^"\\])*"'
    r"|(?<![\w'])(?:u8|u|U|L)?'(?:\\.|[^'\\\r\n])*'"
    r"|(?P<comment>/\*[\s\S]*?\*/|//[^\n]*)"
)
PLATFORM_HEADER = re.compile(
    r"^(?:(?:windows|qt_windows|dwmapi|winuser|shellapi|emscripten)\.h$|"
    r"(?:X11|xcb|wayland|Cocoa|AppKit|Foundation|emscripten)/|wayland-[^/]+\.h$|"
    r"(?:Qt\w+/)?qpa/|(?:QtGui/)?qguiapplication_platform\.h$)",
    re.IGNORECASE,
)


@dataclass(frozen=True, order=True)
class Violation:
    path: str
    line: int
    rule: str
    message: str

    def __str__(self):
        return f"{self.path}:{self.line}: {self.rule}: {self.message}"


def cpp_views(source: str):
    """Splice physical lines first; mask comments/literals without losing locations."""
    parts, origins = [], []
    continuing = False
    for number, line in enumerate(source.splitlines(keepends=True), 1):
        if not continuing:
            origins.append(number)
        continuing = bool(re.search(r"\\\r?\n$", line))
        parts.append(re.sub(r"\\\r?\n$", "", line) if continuing else line)
    logical = "".join(parts)

    def blank(match):
        return re.sub(r"[^\n]", " ", match[0])

    code = CPP_TOKENS.sub(blank, logical)
    includes = CPP_TOKENS.sub(lambda match: blank(match) if match["comment"] else match[0], logical)
    return code, includes, origins


def cpp_includes(code: str, includes: str, origins: list[int]):
    for prefix in re.finditer(r"(?m)^[ \t]*#[ \t]*(?:include|import)\b", code):
        header = re.match(r'[ \t]*[<"]([^>"]+)[>"]', includes[prefix.end():])
        if header:
            yield header[1].replace("\\", "/"), origins[code.count("\n", 0, prefix.start())]


def cpp_violations(source: str, path: str) -> list[Violation]:
    adapter = path.startswith("src/compatibility/")
    component = path.startswith(("src/components/", "support/spatial/"))
    code, includes, origins = cpp_views(source)
    found = []

    def add(offset, rule, message):
        line = origins[code.count("\n", 0, offset)]
        found.append(Violation(path, line, rule, message))

    # This runtime test constructs both Qt event forms to test the adapters.
    version_fixture = path == "tests/components/TestQtCompat.cpp"
    for rule, pattern, message in CPP_RULES:
        if adapter or (rule == "qt-version" and version_fixture):
            continue
        if rule != "qt-version" and not component:
            continue
        for match in re.finditer(pattern, code):
            add(match.start(), rule, message)

    for name, line in cpp_includes(code, includes, origins):
        if adapter and "components" in name.split("/"):
            found.append(Violation(path, line, "adapter-dependency", "Compatibility adapters must not include components"))
        if component and PLATFORM_HEADER.match(name):
            found.append(Violation(path, line, "platform-header",
                                   "Include platform backend headers only in compatibility adapters"))
    return found


def adapter_dependencies(sources: dict[str, str]) -> list[Violation]:
    """Follow repository forwarding headers; external Qt headers are not parsed."""
    include_cache = {}

    def includes(path):
        if path not in include_cache:
            include_cache[path] = list(cpp_includes(*cpp_views(sources[path])))
        return include_cache[path]

    def resolve(path, header):
        for base in (posixpath.dirname(path), "src", "include", ""):
            candidate = posixpath.normpath(posixpath.join(base, header))
            if candidate in sources:
                return candidate
        return None

    found = []
    for path in sources:
        if not path.startswith("src/compatibility/"):
            continue
        for header, line in includes(path):
            target = resolve(path, header)
            queue = deque([(target, [header])]) if target else deque()
            visited = set()
            while queue:
                current, chain = queue.popleft()
                if current in visited:
                    continue
                visited.add(current)
                if current.startswith("src/components/"):
                    found.append(Violation(path, line, "adapter-dependency",
                                           "Adapter depends on a component via " + " -> ".join(chain)))
                    break
                for dependency, _ in includes(current):
                    target = resolve(current, dependency)
                    if target:
                        queue.append((target, [*chain, dependency]))
    return found


def python_adaptations(source: str) -> list[tuple[int, str]]:
    """Follow Qt imports and intermediate values; ordinary diagnostics remain legal."""
    tree = ast.parse(source)
    version_functions = {"qVersion"}
    version_types = {"QLibraryInfo"}
    version_values = set()
    qt_modules = {"PySide6", "QtCore", "QtGui", "QtWidgets"}
    qt_types = {"QEvent", "QPaintDevice", "QAccessible"}
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for name in node.names:
                if name.name.startswith("PySide6"):
                    qt_modules.add(name.asname or name.name.split(".")[0])
        if isinstance(node, ast.ImportFrom) and (node.module or "").startswith("PySide6"):
            for name in node.names:
                if name.name == "qVersion":
                    version_functions.add(name.asname or name.name)
                elif name.name == "QLibraryInfo":
                    version_types.add(name.asname or name.name)
                elif name.name in {"__version__", "__version_info__"}:
                    version_values.add(name.asname or name.name)
                elif name.name.startswith("Qt"):
                    qt_modules.add(name.asname or name.name)
                elif name.name.startswith("Q"):
                    qt_types.add(name.asname or name.name)

    def root_name(node):
        while isinstance(node, (ast.Attribute, ast.Call, ast.Subscript)):
            node = node.func if isinstance(node, ast.Call) else node.value
        return node.id if isinstance(node, ast.Name) else None

    def is_version_call(node):
        if not isinstance(node, ast.Call):
            return False
        return ((isinstance(node.func, ast.Name) and node.func.id in version_functions)
                or (isinstance(node.func, ast.Attribute) and node.func.attr == "qVersion"
                    and root_name(node.func) in qt_modules)
                or (isinstance(node.func, ast.Attribute) and node.func.attr == "version"
                    and (root_name(node.func) in version_types
                         or (isinstance(node.func.value, ast.Attribute)
                             and node.func.value.attr == "QLibraryInfo"
                             and root_name(node.func) in qt_modules))))

    def uses_version(node):
        return any(
            (isinstance(child, ast.Name) and child.id in version_values)
            or (isinstance(child, ast.Attribute) and child.attr in {"__version__", "__version_info__"}
                and root_name(child) in qt_modules)
            or is_version_call(child)
            for child in ast.walk(node)
        )

    assignments = [node for node in ast.walk(tree) if isinstance(node, (ast.Assign, ast.AnnAssign))]
    while True:
        previous = tuple(set(values) for values in (
            version_values, qt_types, qt_modules, version_functions, version_types))
        for node in assignments:
            if node.value is None:
                continue
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            for target in targets:
                names = {target.id} if isinstance(target, ast.Name) else set()
                if uses_version(node.value):
                    version_values.update(names)
                value = node.value
                if ((isinstance(value, ast.Name) and value.id in qt_modules)
                        or (isinstance(value, ast.Attribute) and value.attr.startswith("Qt")
                            and root_name(value) in qt_modules)):
                    qt_modules.update(names)
                if ((isinstance(value, ast.Name) and value.id in version_functions)
                        or (isinstance(value, ast.Attribute) and value.attr == "qVersion"
                            and root_name(value) in qt_modules)):
                    version_functions.update(names)
                if ((isinstance(value, ast.Name) and value.id in version_types)
                        or (isinstance(value, ast.Attribute) and value.attr == "QLibraryInfo"
                            and root_name(value) in qt_modules)):
                    version_types.update(names)
                type_alias = isinstance(node.value, (ast.Name, ast.Attribute))
                style_hints = (isinstance(node.value, ast.Call)
                               and isinstance(node.value.func, ast.Attribute)
                               and node.value.func.attr == "styleHints")
                if (type_alias or style_hints) and root_name(node.value) in qt_types | qt_modules:
                    qt_types.update(names)
        if previous == (version_values, qt_types, qt_modules, version_functions, version_types):
            break

    violations = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Compare) and uses_version(node):
            violations.append((node.lineno, "Qt/PySide version comparison"))
        if isinstance(node, (ast.If, ast.IfExp, ast.While)) and uses_version(node.test):
            violations.append((node.lineno, "Qt/PySide version condition"))
        if (isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
                and node.func.id in {"getattr", "hasattr"} and node.args):
            if root_name(node.args[0]) in qt_types | qt_modules:
                violations.append((node.lineno, "Qt API capability lookup"))
        if isinstance(node, ast.Try) and any(
            isinstance(child, ast.Name) and child.id == "AttributeError"
            for handler in node.handlers if handler.type is not None
            for child in ast.walk(handler.type)
        ):
            if any(isinstance(child, ast.Attribute) and root_name(child) in qt_types | qt_modules
                   for statement in node.body for child in ast.walk(statement)):
                violations.append((node.lineno, "Qt API availability fallback"))
    return violations


def python_violations(source: str, path: str) -> list[Violation]:
    tree = ast.parse(source)
    library = path.startswith(PYTHON_ROOTS[0] + "/")
    adapter = library and Path(path).parent.as_posix() == PYTHON_ROOTS[0] and Path(path).name.startswith("_qt_")
    found = []
    if not adapter:
        found += [Violation(path, line, "python-qt-adaptation", message + "; use a private _qt_ adapter")
                  for line, message in python_adaptations(source)]

    aliases = {}
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for name in node.names:
                aliases[name.asname or name.name.split(".")[0]] = name.name if name.asname else name.name.split(".")[0]
        elif isinstance(node, ast.ImportFrom):
            for name in node.names:
                aliases[name.asname or name.name] = (node.module or "") + "." + name.name
            module = node.module or ""
            private_import = (module.startswith(("fluentqt._qt_", "_qt_"))
                              or (module in {"", "fluentqt"} and all(
                                  name.name.startswith("_qt_") for name in node.names)))
            if adapter and not private_import and (node.level or module in {"fluentqt", "fluentqt_gallery"}
                                                   or module.startswith(("fluentqt.", "fluentqt_gallery."))):
                found.append(Violation(path, node.lineno, "adapter-dependency",
                                       "Private Qt adapters must not import component facades"))
        if adapter and isinstance(node, ast.Import) and any(
            name.name in {"fluentqt", "fluentqt_gallery"} or name.name.startswith("fluentqt_gallery.")
            or (name.name.startswith("fluentqt.") and not name.name.startswith("fluentqt._qt_"))
            for name in node.names
        ):
            found.append(Violation(path, node.lineno, "adapter-dependency",
                                   "Private Qt adapters must not import component facades"))

    def qualified(node):
        if isinstance(node, ast.Name):
            return aliases.get(node.id, node.id)
        if isinstance(node, ast.Attribute):
            return qualified(node.value) + "." + node.attr
        return ""

    for node in ast.walk(tree):
        if not adapter and isinstance(node, ast.Call):
            if isinstance(node.func, ast.Attribute) and node.func.attr in NATIVE_METHODS:
                # Gallery process integration may legitimately acquire a native handle.
                if library or node.func.attr not in {"winId", "effectiveWinId"}:
                    found.append(Violation(path, node.lineno, "native-operation",
                                           "Native window operations belong in a private Qt adapter"))
        # The package entry point owns DLL loading; Gallery owns application OS policies.
        if library and not adapter and Path(path).name != "__init__.py":
            name = qualified(node)
            if name in {"sys.platform", "os.name", "platform.system", "platform.release", "platform.machine",
                        "platform.version", "platform.win32_ver", "platform.mac_ver",
                        "PySide6.QtGui.QGuiApplication.platformName",
                        "PySide6.QtWidgets.QApplication.platformName"}:
                found.append(Violation(path, node.lineno, "python-platform",
                                       "Platform detection in library facades belongs in a private Qt adapter"))
    return found


def scan_project(root: Path) -> tuple[list[Violation], int]:
    issues, count = [], 0
    cpp_sources = {}
    for directory in (*CPP_ROOTS, *PYTHON_ROOTS):
        if not (root / directory).is_dir():
            raise ValueError(f"Missing source directory: {root / directory}")
        for path in sorted((root / directory).rglob("*")):
            python = directory in PYTHON_ROOTS
            if not path.is_file() or path.suffix not in ({".py"} if python else CPP_SUFFIXES):
                continue
            relative = path.relative_to(root).as_posix()
            source = path.read_text(encoding="utf-8-sig")
            count += 1
            if not python:
                cpp_sources[relative] = source
            try:
                issues.extend((python_violations if python else cpp_violations)(source, relative))
            except SyntaxError as error:
                issues.append(Violation(relative, error.lineno or 1, "python-syntax", error.msg))
    if not count:
        raise ValueError(f"No source files found under {root}")
    # The include graph also catches public forwarding headers and renamed local headers.
    direct_dependencies = {(issue.path, issue.line) for issue in issues if issue.rule == "adapter-dependency"}
    issues.extend(issue for issue in adapter_dependencies(cpp_sources)
                  if (issue.path, issue.line) not in direct_dependencies)
    return sorted(set(issues)), count


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=ROOT)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if args.self_test:
        suite = unittest.defaultTestLoader.discover(str(Path(__file__).parent),
                                                  pattern="test_check_compatibility_boundaries.py")
        if not unittest.TextTestRunner(verbosity=1).run(suite).wasSuccessful():
            return 1
    try:
        issues, count = scan_project(args.project_root.resolve())
    except (OSError, UnicodeError, ValueError) as error:
        print(f"Compatibility boundary check failed: {error}", file=sys.stderr)
        return 1
    for issue in issues:
        print(issue)
    if issues:
        print("See docs/development/compatibility-policy.md#design-before-implementation")
        return 1
    print(f"Compatibility boundaries passed ({count} source files; no Qt runtime required).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
