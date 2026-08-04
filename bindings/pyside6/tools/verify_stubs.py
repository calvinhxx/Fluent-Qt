"""Verify generated FluentQt stubs against the committed public API manifest."""

import argparse
import ast
import json
from pathlib import Path
import importlib
import os
import sys


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-dir", required=True)
    parser.add_argument("--manifest", required=True)
    return parser.parse_args()


def literal_all(tree, path):
    for node in tree.body:
        if not isinstance(node, ast.Assign):
            continue
        if not any(
            isinstance(target, ast.Name) and target.id == "__all__"
            for target in node.targets
        ):
            continue
        value = ast.literal_eval(node.value)
        if not isinstance(value, list) or not all(
            isinstance(name, str) for name in value
        ):
            raise RuntimeError("{0} has a non-literal __all__".format(path))
        return value
    raise RuntimeError("{0} does not declare __all__".format(path))


def top_level_symbols(tree):
    symbols = set()
    for node in tree.body:
        if isinstance(node, (ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef)):
            symbols.add(node.name)
        elif isinstance(node, ast.ImportFrom):
            symbols.update(alias.asname or alias.name for alias in node.names)
        elif isinstance(node, ast.Assign):
            symbols.update(
                target.id for target in node.targets if isinstance(target, ast.Name)
            )
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            symbols.add(node.target.id)
    return symbols


def class_methods(trees):
    methods = {}

    def visit(node):
        if isinstance(node, ast.ClassDef):
            class_members = methods.setdefault(node.name, set())
            class_members.update(
                child.name
                for child in node.body
                if isinstance(child, (ast.FunctionDef, ast.AsyncFunctionDef))
            )
        for child in ast.iter_child_nodes(node):
            visit(child)

    for tree in trees:
        visit(tree)
    return methods


def verify_module_pairs(package_dir):
    trees = {}
    for source_path in sorted(package_dir.glob("*.py")):
        stub_path = source_path.with_suffix(".pyi")
        if not stub_path.is_file():
            raise RuntimeError("Missing stub for {0}".format(source_path.name))
        source_tree = ast.parse(
            source_path.read_text(encoding="utf-8"),
            filename=str(source_path),
        )
        stub_contents = stub_path.read_text(encoding="utf-8")
        if "typing.Self" in stub_contents:
            raise RuntimeError(
                "{0} requires Python 3.11 typing.Self".format(stub_path.name)
            )
        stub_tree = ast.parse(stub_contents, filename=str(stub_path))
        source_exports = literal_all(source_tree, source_path)
        stub_exports = literal_all(stub_tree, stub_path)
        if source_exports != stub_exports:
            raise RuntimeError(
                "{0} does not match {1}.__all__".format(
                    stub_path.name,
                    source_path.name,
                )
            )
        missing_symbols = sorted(
            set(source_exports) - top_level_symbols(stub_tree)
        )
        if missing_symbols:
            raise RuntimeError(
                "{0} is missing exports: {1}".format(
                    stub_path.name,
                    ", ".join(missing_symbols),
                )
            )
        trees[stub_path.name] = stub_tree
    return trees


def main():
    args = parse_args()
    package_dir = Path(args.package_dir).resolve()
    manifest_path = Path(args.manifest).resolve()
    native_stub = package_dir / "_fluentqt.pyi"
    if not native_stub.is_file():
        raise RuntimeError("Missing native stub: {0}".format(native_stub))
    native_contents = native_stub.read_text(encoding="utf-8")
    if "\nimport _fluentqt\n" in native_contents:
        raise RuntimeError("Native stub contains an unqualified self import")
    native_tree = ast.parse(native_contents, filename=str(native_stub))

    facade_trees = verify_module_pairs(package_dir)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    root_tree = facade_trees["__init__.pyi"]
    root_symbols = top_level_symbols(root_tree)
    required_root = set(manifest["classes"])
    required_root.update(manifest["enums"])
    required_root.update(manifest["functions"])
    required_root.update(manifest["variables"])
    missing_root = sorted(required_root - root_symbols)
    if missing_root:
        raise RuntimeError(
            "Root stub is missing manifest symbols: {0}".format(
                ", ".join(missing_root)
            )
        )

    all_trees = [native_tree]
    all_trees.extend(facade_trees.values())
    methods = class_methods(all_trees)
    missing_classes = sorted(
        class_name
        for class_name in manifest["classes"]
        if class_name not in methods
    )
    if missing_classes:
        raise RuntimeError(
            "Generated stubs are missing classes: {0}".format(
                ", ".join(missing_classes)
            )
        )

    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    sys.path.insert(0, str(package_dir.parent))
    try:
        runtime_package = importlib.import_module("fluentqt")
    finally:
        sys.path.pop(0)

    missing_methods = []
    for class_name, expected_methods in manifest["methods"].items():
        actual_methods = methods.get(class_name, set())
        for method_name in expected_methods:
            if method_name in actual_methods:
                continue
            runtime_class = getattr(runtime_package, class_name)
            owner = next(
                (
                    base
                    for base in runtime_class.__mro__
                    if method_name in vars(base)
                ),
                None,
            )
            if owner is None:
                missing_methods.append("{0}.{1}".format(class_name, method_name))
                continue
            owner_module = owner.__module__ or ""
            if owner_module.startswith("PySide6."):
                continue
            if method_name in methods.get(owner.__name__, set()):
                continue
            missing_methods.append("{0}.{1}".format(class_name, method_name))
    if missing_methods:
        raise RuntimeError(
            "Generated stubs are missing methods: {0}".format(
                ", ".join(sorted(missing_methods))
            )
        )

    print(
        "Verified {0} stub files, {1} classes, {2} enums, {3} functions, "
        "and {4} variables".format(
            len(facade_trees) + 1,
            len(manifest["classes"]),
            len(manifest["enums"]),
            len(manifest["functions"]),
            len(manifest["variables"]),
        )
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        sys.stderr.write("FluentQt stub verification failed: {0}\n".format(error))
        sys.exit(1)
