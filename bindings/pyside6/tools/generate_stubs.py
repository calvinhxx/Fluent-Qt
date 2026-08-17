"""Generate type stubs for the native module and Python facade package.

The native stub is produced from Shiboken's own signature metadata.  The
facade stubs preserve the Python-only ownership helpers and module re-exports
that do not exist in the generated C++ extension.

Keep this script compatible with Python 3.10, the PySide 6.2 baseline.
"""

import argparse
import ast
import importlib
import inspect
import logging
import os
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace
import typing
import warnings


NATIVE_MODULE_NAME = "_fluentqt"
NATIVE_MODULE_NAMES = {
    NATIVE_MODULE_NAME,
    "fluentqt._fluentqt",
}
PUBLIC_DUNDER_METHODS = {
    "__copy__",
    "__eq__",
    "__getattr__",
    "__hash__",
    "__init__",
    "__ne__",
}
MODULE_FUNCTION_SIGNATURES = {
    ("fluentqt", "binding_build_info"): "() -> dict[str, object]",
    ("fluentqt", "initialize_resources"): "() -> bool",
    ("fluentqt", "prepare_high_dpi_application"): "() -> None",
    ("fluentqt.foundation", "accent_color"):
        "() -> PySide6.QtGui.QColor",
    ("fluentqt.foundation", "apply_user_theme"): "() -> None",
    ("fluentqt.foundation", "anchors"):
        "(*, left: Any = ..., right: Any = ..., top: Any = ..., "
        "bottom: Any = ..., horizontal_center: Any = ..., "
        "vertical_center: Any = ..., center_in: Any = ..., "
        "top_right: Any = ..., fill: Any = ...) -> AnchorSpec",
    ("fluentqt.foundation", "bind"):
        "(source: PySide6.QtCore.QObject, source_property: str, "
        "target: PySide6.QtCore.QObject, target_property: str, "
        "mode: BindingMode = ...) -> None",
    ("fluentqt.foundation", "current_theme"): "() -> Theme",
    ("fluentqt.foundation", "font_for_role"):
        "(role: FontRole = ...) -> PySide6.QtGui.QFont",
    ("fluentqt.foundation", "font_scale"): "() -> float",
    ("fluentqt.foundation", "reset_theme_tokens"): "() -> None",
    ("fluentqt.foundation", "set_accent_color"):
        "(color: PySide6.QtGui.QColor) -> None",
    ("fluentqt.foundation", "set_font_scale"): "(scale: float) -> None",
    ("fluentqt.foundation", "set_theme"): "(theme: Theme) -> None",
    ("fluentqt.foundation", "theme_revision"): "() -> int",
}
MODULE_VARIABLE_TYPES = {
    ("fluentqt", "__api_version__"): "str",
    ("fluentqt", "__version__"): "str",
}
CLASS_METHOD_SIGNATURES = {
    ("fluentqt.foundation", "FluentWidget", "effective_theme"):
        "(self) -> Theme",
    ("fluentqt.foundation", "FluentWidget", "on_theme_updated"):
        "(self) -> None",
    ("fluentqt.foundation", "FluentWidget", "theme_font"):
        "(self, role: FontRole = ...) -> PySide6.QtGui.QFont",
    ("fluentqt.foundation", "FluentWidget", "theme_tokens"):
        "(self) -> ThemeTokens",
    ("fluentqt.foundation", "StateGroup", "add"):
        "(self, name: str, changes: "
        "Mapping[PySide6.QtCore.QObject, Mapping[str, Any]]) -> StateGroup",
    ("fluentqt.foundation", "StateGroup", "__init__"):
        "(self, parent: PySide6.QtCore.QObject | None = ...) -> None",
    ("fluentqt.foundation", "StateGroup", "clear"):
        "(self) -> None",
    ("fluentqt.foundation", "StateGroup", "has"):
        "(self, name: str) -> bool",
    ("fluentqt.foundation", "StateGroup", "set"):
        "(self, name: str = ...) -> None",
    ("fluentqt.design", "ThemeTokens", "__getattr__"):
        "(self, name: str) -> Any",
}
CLASS_ATTRIBUTE_TYPES = {
    ("fluentqt.foundation", "StateGroup", "state_changed"):
        "PySide6.QtCore.SignalInstance",
}


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--extension", required=True)
    parser.add_argument("--package-dir", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--native-only", action="store_true", help=argparse.SUPPRESS)
    return parser.parse_args()


def import_signature_module(suffix):
    errors = []
    for prefix in (
        "shibokensupport.signature",
        "PySide6.support.signature",
    ):
        try:
            return importlib.import_module("{0}.{1}".format(prefix, suffix))
        except ImportError as error:
            errors.append(str(error))
    raise RuntimeError(
        "Unable to import Shiboken signature support {0}: {1}".format(
            suffix,
            "; ".join(errors),
        )
    )


def shiboken_generator_options():
    """Return options shared by the supported Shiboken stub generators."""
    return SimpleNamespace(
        _pyside_call=False,
        check=False,
        feature=[],
        features=[],
        # Shiboken 6.2 implements its CI check by executing the generated
        # .pyi.  That puts fluentqt/ first on sys.path, where collections.py
        # shadows Python's standard-library collections module.  The explicit
        # ast.parse call and verify_stubs.py provide a safe syntax/API gate.
        is_ci=False,
        logger=logging.getLogger("fluentqt.generate_stubs"),
        quiet=True,
        sys_path=None,
    )


def generate_native_stub(extension, package_dir):
    # Importing shiboken6 installs the virtual shibokensupport package used by
    # recent releases.  The PySide6 fallback keeps the Qt 6.2 layout working.
    import PySide6
    import PySide6.QtCore
    import PySide6.QtGui
    import PySide6.QtWidgets
    import shiboken6  # noqa: F401

    generator = import_signature_module("lib.pyi_generator")
    enum_sig = import_signature_module("lib.enum_sig")
    tool = import_signature_module("lib.tool")

    # shiboken6-genpyi historically initialized these globals only for pure
    # Shiboken modules.  FluentQt also references PySide types, so initialize
    # the same context explicitly across the supported generator versions.
    generator.PySide6 = PySide6
    generator.inspect = inspect
    generator.typing = typing
    generator.HintingEnumerator = enum_sig.HintingEnumerator
    generator.build_brace_pattern = tool.build_brace_pattern

    options = shiboken_generator_options()
    with warnings.catch_warnings(record=True) as signature_warnings:
        warnings.simplefilter("always", RuntimeWarning)
        generator.generate_pyi(str(extension), str(package_dir), options)

    native_stub = package_dir / "{0}.pyi".format(NATIVE_MODULE_NAME)
    if not native_stub.is_file():
        raise RuntimeError(
            "Shiboken did not create {0}".format(native_stub)
        )

    contents = native_stub.read_text(encoding="utf-8")
    contents = contents.replace(
        "import _fluentqt\n",
        "import fluentqt._fluentqt as _fluentqt\n",
        1,
    )
    # typing.Self was added in Python 3.11.  Preserve the declared Python 3.10
    # floor without adding a runtime typing_extensions dependency.
    contents = contents.replace("typing.Self", "typing.Any")
    ast.parse(contents, filename=str(native_stub))
    native_stub.write_text(contents, encoding="utf-8")

    if signature_warnings:
        print(
            "Generated native stub with {0} recoverable Shiboken signature "
            "warning(s); unresolved defaults are represented by ellipses.".format(
                len(signature_warnings)
            ),
            file=sys.stderr,
        )


def render_parameters(callable_object, include_self):
    try:
        signature = inspect.signature(callable_object)
    except (TypeError, ValueError):
        if include_self:
            return "self, *args: Any, **kwargs: Any"
        return "*args: Any, **kwargs: Any"

    parameters = []
    positional_only_count = 0
    has_var_positional = False
    inserted_keyword_separator = False
    for parameter in signature.parameters.values():
        if parameter.kind == inspect.Parameter.POSITIONAL_ONLY:
            positional_only_count += 1
        if parameter.kind == inspect.Parameter.VAR_POSITIONAL:
            has_var_positional = True
            parameters.append("*{0}: Any".format(parameter.name))
            continue
        if parameter.kind == inspect.Parameter.VAR_KEYWORD:
            parameters.append("**{0}: Any".format(parameter.name))
            continue
        if (
            parameter.kind == inspect.Parameter.KEYWORD_ONLY
            and not has_var_positional
            and not inserted_keyword_separator
        ):
            parameters.append("*")
            inserted_keyword_separator = True

        if include_self and parameter.name in {"self", "cls"}:
            rendered = parameter.name
        else:
            rendered = "{0}: Any".format(parameter.name)
        if parameter.default is not inspect.Parameter.empty:
            rendered += " = ..."
        parameters.append(rendered)

    if positional_only_count:
        parameters.insert(positional_only_count, "/")
    if include_self and not parameters:
        parameters.append("self")
    return ", ".join(parameters)


def callable_stub(name, callable_object, include_self=True, indent=""):
    return_type = "None" if name == "__init__" else "Any"
    parameters = render_parameters(callable_object, include_self)
    return "{0}def {1}({2}) -> {3}: ...".format(
        indent,
        name,
        parameters,
        return_type,
    )


def module_function_stub(module_name, name, callable_object):
    signature = MODULE_FUNCTION_SIGNATURES.get((module_name, name))
    if signature is not None:
        return "def {0}{1}: ...".format(name, signature)
    return callable_stub(name, callable_object, include_self=False)


def is_public_method(name):
    return not name.startswith("_") or name in PUBLIC_DUNDER_METHODS


def python_facade_members(public_class):
    members = {}
    for base in public_class.__mro__:
        include_all = (base.__module__ or "").startswith("fluentqt")
        for name, value in vars(base).items():
            if not is_public_method(name):
                continue
            unwrapped = value
            if isinstance(value, (staticmethod, classmethod)):
                unwrapped = value.__func__
            elif isinstance(value, property):
                unwrapped = value.fget
            extension_module = getattr(unwrapped, "__module__", "") or ""
            is_python_extension = extension_module.startswith("fluentqt")
            if not include_all and not is_python_extension:
                continue
            if isinstance(value, (staticmethod, classmethod, property)):
                members.setdefault(name, value)
                continue
            if inspect.isclass(value):
                continue
            if callable(value):
                members.setdefault(name, value)
    return members


def native_expression(value):
    return "_native.{0}".format(value.__qualname__)


def native_base(public_class):
    for base in public_class.__mro__[1:]:
        if base.__module__ == NATIVE_MODULE_NAME:
            return base
    raise RuntimeError(
        "Python facade {0} has no native FluentQt base".format(
            public_class.__qualname__
        )
    )


def optional_native_base(public_class):
    for base in public_class.__mro__[1:]:
        if base.__module__ == NATIVE_MODULE_NAME:
            return base
    return None


def pure_attribute_type(value):
    if isinstance(value, bool):
        return "bool"
    if isinstance(value, int):
        return "int"
    if isinstance(value, float):
        return "float"
    if isinstance(value, str):
        return "str"
    if inspect.isclass(value) and (value.__module__ or "").startswith(
        "fluentqt"
    ):
        return "type[{0}]".format(value.__name__)
    return "Any"


def pure_python_class_stub(name, public_class, indent=""):
    try:
        is_mapping = issubclass(public_class, dict)
    except TypeError:
        is_mapping = False
    base = "(dict[str, Any])" if is_mapping else ""
    lines = ["{0}class {1}{2}:".format(indent, name, base)]
    body = []

    for member_name, value in vars(public_class).items():
        if member_name.startswith("_"):
            continue
        if inspect.isclass(value) and (
            value.__module__ == public_class.__module__
            and value.__qualname__.startswith(public_class.__qualname__ + ".")
        ):
            body.extend(
                pure_python_class_stub(
                    member_name,
                    value,
                    indent=indent + "    ",
                )
            )
            continue
        if isinstance(value, (staticmethod, classmethod, property)):
            continue
        if callable(value) and not inspect.isclass(value):
            continue
        body.append(
            "{0}    {1}: {2}".format(
                indent,
                member_name,
                pure_attribute_type(value),
            )
        )

    facade_members = python_facade_members(public_class)
    for member_name in sorted(facade_members):
        value = facade_members[member_name]
        signature = CLASS_METHOD_SIGNATURES.get(
            (public_class.__module__, name, member_name)
        )
        if signature is not None:
            body.append(
                "{0}    def {1}{2}: ...".format(
                    indent, member_name, signature
                )
            )
            continue
        if isinstance(value, staticmethod):
            body.append("{0}    @staticmethod".format(indent))
            body.append(
                callable_stub(
                    member_name,
                    value.__func__,
                    include_self=False,
                    indent=indent + "    ",
                )
            )
        elif isinstance(value, classmethod):
            body.append("{0}    @classmethod".format(indent))
            body.append(
                callable_stub(
                    member_name,
                    value.__func__,
                    include_self=True,
                    indent=indent + "    ",
                )
            )
        elif isinstance(value, property):
            body.append("{0}    @property".format(indent))
            body.append(
                callable_stub(
                    member_name,
                    value.fget,
                    include_self=True,
                    indent=indent + "    ",
                )
            )
        else:
            body.append(
                callable_stub(
                    member_name,
                    value,
                    include_self=True,
                    indent=indent + "    ",
                )
            )

    if not body:
        body.append("{0}    ...".format(indent))
    lines.extend(body)
    return lines


def class_stub(name, public_class):
    facade_members = python_facade_members(public_class)
    is_python_facade = (public_class.__module__ or "").startswith("fluentqt")
    if not is_python_facade and not facade_members:
        return ["{0} = {1}".format(name, native_expression(public_class))]

    native_facade_base = (
        optional_native_base(public_class) if is_python_facade else None
    )
    if is_python_facade and native_facade_base is None:
        return pure_python_class_stub(name, public_class)
    base = native_facade_base if is_python_facade else public_class
    lines = ["class {0}({1}):".format(name, native_expression(base))]
    if not facade_members:
        lines.append("    ...")
        return lines

    for member_name in sorted(facade_members):
        value = facade_members[member_name]
        attribute_type = CLASS_ATTRIBUTE_TYPES.get(
            (public_class.__module__, name, member_name)
        )
        if attribute_type is not None:
            lines.append("    {0}: {1}".format(member_name, attribute_type))
            continue
        signature = CLASS_METHOD_SIGNATURES.get(
            (public_class.__module__, name, member_name)
        )
        if signature is not None:
            lines.append(
                "    def {0}{1}: ...".format(member_name, signature)
            )
            continue
        if isinstance(value, staticmethod):
            lines.append("    @staticmethod")
            lines.append(
                callable_stub(
                    member_name,
                    value.__func__,
                    include_self=False,
                    indent="    ",
                )
            )
        elif isinstance(value, classmethod):
            lines.append("    @classmethod")
            lines.append(
                callable_stub(
                    member_name,
                    value.__func__,
                    include_self=True,
                    indent="    ",
                )
            )
        elif isinstance(value, property):
            lines.append("    @property")
            lines.append(
                callable_stub(
                    member_name,
                    value.fget,
                    include_self=True,
                    indent="    ",
                )
            )
        else:
            lines.append(
                callable_stub(
                    member_name,
                    value,
                    include_self=True,
                    indent="    ",
                )
            )
    return lines


def render_all(export_names):
    lines = ["__all__ = ["]
    lines.extend('    "{0}",'.format(name) for name in export_names)
    lines.append("]")
    return lines


def generate_facade_stub(module, output_path):
    export_names = list(module.__all__)
    lines = [
        '"""Generated typing facade for {0}."""'.format(module.__name__),
        "",
        "from collections.abc import Mapping",
        "from typing import Any",
        "import PySide6.QtCore",
        "import PySide6.QtGui",
        "from . import _fluentqt as _native",
        "",
    ]
    if module.__name__ == "fluentqt.foundation":
        lines.insert(-1, "from .design import ThemeTokens")
    for name in export_names:
        value = getattr(module, name)
        if inspect.isclass(value):
            lines.extend(class_stub(name, value))
        elif value.__class__.__module__ == "enum" and inspect.isclass(value):
            lines.append("{0} = {1}".format(name, native_expression(value)))
        elif getattr(value, "__module__", "") in NATIVE_MODULE_NAMES:
            lines.append("{0} = {1}".format(name, native_expression(value)))
        elif callable(value):
            lines.append(module_function_stub(module.__name__, name, value))
        else:
            lines.append("{0}: Any".format(name))
    lines.append("")
    lines.extend(render_all(export_names))
    lines.append("")
    contents = "\n".join(lines)
    ast.parse(contents, filename=str(output_path))
    output_path.write_text(contents, encoding="utf-8")


def root_imports(source_path):
    tree = ast.parse(source_path.read_text(encoding="utf-8"))
    imports = []
    functions = []
    for node in tree.body:
        if isinstance(node, ast.ImportFrom) and node.level == 1:
            imports.append(node)
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            if not node.name.startswith("_"):
                functions.append(node.name)
    return imports, functions


def generate_root_stub(package, package_dir):
    source_path = package_dir / "__init__.py"
    output_path = package_dir / "__init__.pyi"
    imports, function_names = root_imports(source_path)
    export_names = list(package.__all__)
    lines = [
        '"""Generated public typing surface for FluentQt."""',
        "",
        "from typing import Any",
    ]
    for node in imports:
        module = node.module or ""
        for alias in node.names:
            if alias.name not in export_names:
                continue
            public_name = alias.asname or alias.name
            lines.append(
                "from .{0} import {1} as {2}".format(
                    module,
                    alias.name,
                    public_name,
                )
            )
    lines.append("")
    for name in function_names:
        if name in export_names:
            lines.append(
                module_function_stub(
                    package.__name__,
                    name,
                    getattr(package, name),
                )
            )
    for name in export_names:
        variable_type = MODULE_VARIABLE_TYPES.get((package.__name__, name))
        if variable_type is not None:
            lines.append("{0}: {1}".format(name, variable_type))
    lines.append("")
    lines.extend(render_all(export_names))
    lines.append("")
    contents = "\n".join(lines)
    ast.parse(contents, filename=str(output_path))
    output_path.write_text(contents, encoding="utf-8")


def generate_facade_stubs(package_dir):
    sys.path.insert(0, str(package_dir.parent))
    try:
        package = importlib.import_module("fluentqt")
        for source_path in sorted(package_dir.glob("*.py")):
            if source_path.name == "__init__.py":
                continue
            module_name = "fluentqt.{0}".format(source_path.stem)
            module = importlib.import_module(module_name)
            if not hasattr(module, "__all__"):
                continue
            generate_facade_stub(
                module,
                package_dir / "{0}.pyi".format(source_path.stem),
            )
        generate_root_stub(package, package_dir)
    finally:
        sys.path.pop(0)


def main():
    args = parse_args()
    extension = Path(args.extension).resolve()
    package_dir = Path(args.package_dir).resolve()
    manifest = Path(args.manifest).resolve()
    if not extension.is_file():
        raise RuntimeError("FluentQt extension does not exist: {0}".format(extension))
    if not package_dir.is_dir():
        raise RuntimeError("Package directory does not exist: {0}".format(package_dir))
    if not manifest.is_file():
        raise RuntimeError("API manifest does not exist: {0}".format(manifest))

    if args.native_only:
        generate_native_stub(extension, package_dir)
        return

    command = [
        sys.executable,
        str(Path(__file__).resolve()),
        "--extension",
        str(extension),
        "--package-dir",
        str(package_dir),
        "--manifest",
        str(manifest),
        "--native-only",
    ]
    subprocess.check_call(command)
    generate_facade_stubs(package_dir)
    print("Generated FluentQt type stubs in {0}".format(package_dir))


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        sys.stderr.write("FluentQt stub generation failed: {0}\n".format(error))
        sys.exit(1)
