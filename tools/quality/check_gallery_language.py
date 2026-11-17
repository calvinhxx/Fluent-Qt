#!/usr/bin/env python3
"""Reject mixed-language Gallery literals without restricting UILib or IME tests.

This catches accidental CJK fixture/copy leakage, not the language of arbitrary
prose. English copy remains a review contract; Unicode user input stays valid.
"""

from __future__ import annotations

import argparse
import ast
import json
from pathlib import Path
import re
import unittest

from check_compatibility_boundaries import CPP_SUFFIXES, CPP_TOKENS


ROOT = Path(__file__).resolve().parents[2]
CJK = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff\u3040-\u30ff\uac00-\ud7af]")
UNICODE_ESCAPE = re.compile(r"\\u([0-9a-fA-F]{4})|\\U([0-9a-fA-F]{8})")


def python_literals(source: str):
    """Allow source comments/docstrings; literal snippets are displayed copy."""
    tree = ast.parse(source)
    docstrings = set()
    for node in ast.walk(tree):
        if isinstance(node, (ast.Module, ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef)):
            if (node.body and isinstance(node.body[0], ast.Expr)
                    and isinstance(node.body[0].value, ast.Constant)
                    and isinstance(node.body[0].value.value, str)):
                docstrings.add(id(node.body[0].value))
    for node in ast.walk(tree):
        if (isinstance(node, ast.Constant) and isinstance(node.value, str)
                and id(node) not in docstrings):
            yield node.lineno, node.value


def cpp_literals(source: str):
    for token in CPP_TOKENS.finditer(source):
        if token["comment"]:
            continue
        value = UNICODE_ESCAPE.sub(lambda m: chr(int(m[1] or m[2], 16)), token[0])
        yield source.count("\n", 0, token.start()) + 1, value


def json_strings(value, *, playground=False, path=(), component=None):
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        # Website labels are localized metadata, not Gallery example values.
        # Keep every default, preset value, API name and English label checked.
        metadata_path = path[1:] if path[:1] == ("*",) else path
        if playground and not metadata_path:
            component = value.get("component")
        if len(metadata_path) == 4 and metadata_path[:3] in {
            ("properties", "*", "valueLabels"),
            ("properties", "*", "optionLabels"),
        }:
            metadata_path = (*metadata_path[:3], "*")
        if playground and metadata_path in {
            ("properties", "*", "label"), ("properties", "*", "description"),
            ("properties", "*", "effectHint"), ("properties", "*", "disabledHint"),
            ("properties", "*", "valueLabels", "*"),
            ("properties", "*", "optionLabels", "*"),
            ("properties", "*", "columns", "*", "label"),
            ("presets", "*", "label"), ("categories", "*", "label"), ("description",),
        }:
            if (set(value) != {"en", "zh"}
                    or any(not isinstance(text, str) or not text.strip() for text in value.values())):
                raise ValueError(f"Playground translation at {'.'.join(path)} requires en/zh strings")
            yield value["en"]
            return
        for key, child in value.items():
            # This optional locale-format value is input data, not English Gallery copy.
            # Keep defaults, recipes and labels checked, including the English option label.
            if (playground and component == "calendar-date-picker"
                    and metadata_path == ("properties", "*") and key == "options"
                    and value.get("id") == "displayFormat" and value.get("property") == "displayFormat"
                    and value.get("setter") == "setDisplayFormat" and isinstance(child, list)):
                child = [option for option in child if option != "yyyy年M月d日"]
            yield from json_strings(child, playground=playground, path=(*path, key), component=component)
    elif isinstance(value, list):
        for child in value:
            yield from json_strings(child, playground=playground, path=(*path, "*"), component=component)


def validate(root: Path) -> list[str]:
    errors = []
    for scope in ("app", "bindings/pyside6/gallery/src"):
        for path in sorted((root / scope).rglob("*")):
            if path.suffix not in CPP_SUFFIXES | {".py", ".json"} or not path.is_file():
                continue
            source = path.read_text(encoding="utf-8")
            try:
                if path.suffix == ".py":
                    values = python_literals(source)
                elif path.suffix == ".json":
                    playground = path.parent == root / "app/assets/playgrounds"
                    values = [(1, text) for text in json_strings(json.loads(source), playground=playground)]
                else:
                    values = cpp_literals(source)
                for line, value in values:
                    value = UNICODE_ESCAPE.sub(lambda m: chr(int(m[1] or m[2], 16)), value)
                    match = CJK.search(value)
                    if match:
                        excerpt = value[max(0, match.start() - 20):match.start() + 60]
                        errors.append(f"{path.relative_to(root)}:{line}: Gallery defaults and displayed "
                                      f"source must use English: {excerpt!r}")
            except (SyntaxError, ValueError) as error:
                errors.append(f"{path.relative_to(root)}: cannot inspect Gallery source: {error}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=ROOT)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        suite = unittest.defaultTestLoader.discover(str(Path(__file__).parent), "test_check_gallery_language.py")
        if not unittest.TextTestRunner().run(suite).wasSuccessful():
            return 1
    errors = validate(args.project_root)
    if errors:
        print("\n".join(errors))
        return 1
    print("Gallery language literals are consistent; UILib and multilingual tests remain unrestricted.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
