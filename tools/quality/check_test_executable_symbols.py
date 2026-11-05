#!/usr/bin/env python3
"""Reject test-local C++ names that would collide inside a shared test executable.

add_qt_test_module links several modules into one executable per test directory
and CI tier. A class or inline helper defined at namespace scope in two of those
sources is one entity to the linker: it keeps one definition and silently drops
the other. Duplicate strong symbols already fail the link, so this check covers
the silent cases: types and inline, template or constexpr functions with
external linkage. Put file-local helpers in an anonymous namespace instead.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
from pathlib import Path
import re
import sys
from typing import Callable, Iterable
import unittest


CPP_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".m", ".mm"}
COMPILED_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".m", ".mm"}
TYPE_KEYWORDS = {"class", "struct", "union", "enum"}
FUNCTION_LINKAGE_KEYWORDS = {"inline", "template", "constexpr"}
IDENTIFIER = re.compile(r"[A-Za-z_]\w*$")
MACRO = re.compile(r"[A-Z][A-Z0-9_]*$")
TOKEN = re.compile(r"[A-Za-z_]\w*|::|\S")


def strip_cpp(text: str) -> str:
    """Remove comments, literals and preprocessor lines, keeping line structure."""
    text = re.sub(r'R"([^()\\\s]{0,16})\((?:.|\n)*?\)\1"', '""', text)
    text = re.sub(r"/\*(?:.|\n)*?\*/", lambda match: "\n" * match.group().count("\n"), text)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r'"(?:\\.|[^"\\\n])*"', '""', text)
    text = re.sub(r"'(?:\\.|[^'\\\n])*'", "''", text)
    return re.sub(r"(?m)^[ \t]*#(?:[^\n]*\\\n)*[^\n]*$", "", text)


def _skip_balanced(tokens: list[str], start: int, opening: str, closing: str) -> int:
    depth = 0
    for index in range(start, len(tokens)):
        if tokens[index] == opening:
            depth += 1
        elif tokens[index] == closing:
            depth -= 1
            if depth == 0:
                return index + 1
    return len(tokens)


def _declaration_head(statement: list[str]) -> tuple[list[str], bool, bool]:
    """Drop leading attributes, macro invocations and template headers.

    Returns the remaining tokens, whether a template header was present, and
    whether it was an explicit specialization (whose owner is the primary).
    """
    tokens = list(statement)
    is_template = False
    is_specialization = False
    while tokens:
        if tokens[0] == "template" and len(tokens) > 1 and tokens[1] == "<":
            is_specialization = is_specialization or tokens[2:3] == [">"]
            tokens = tokens[_skip_balanced(tokens, 1, "<", ">"):]
            is_template = True
        elif tokens[:2] == ["[", "["]:
            tokens = tokens[_skip_balanced(tokens, 0, "[", "]"):]
        elif len(tokens) > 1 and MACRO.match(tokens[0]) and tokens[1] == "(":
            tokens = tokens[_skip_balanced(tokens, 1, "(", ")"):]
        else:
            break
    return tokens, is_template, is_specialization


def _type_name(tokens: list[str]) -> str | None:
    if not tokens or tokens[0] not in TYPE_KEYWORDS:
        return None
    index = 1
    if tokens[0] == "enum" and index < len(tokens) and tokens[index] in {"class", "struct"}:
        index += 1
    while index < len(tokens) and tokens[index] == "[":
        index = _skip_balanced(tokens, index, "[", "]")
    if index >= len(tokens) or not IDENTIFIER.match(tokens[index]) or tokens[index] == "final":
        return None
    # A qualified or specialized name refers to an entity declared elsewhere.
    if tokens[index + 1:index + 2] in (["::"], ["<"]):
        return None
    return tokens[index]


def _function_name(tokens: list[str], is_template: bool) -> str | None:
    if "(" not in tokens:
        return None
    head = tokens[: tokens.index("(")]
    if not head or "=" in head or "static" in head or "::" in head[-2:]:
        return None
    if not is_template and not FUNCTION_LINKAGE_KEYWORDS.intersection(head):
        return None
    name = head[-1]
    if not IDENTIFIER.match(name) or name == "operator" or MACRO.match(name):
        return None
    return name


def external_definitions(text: str) -> list[tuple[str, str]]:
    """Return (kind, qualified name) for externally linked definitions at namespace scope."""
    definitions: list[tuple[str, str]] = []
    # Each scope is (is_namespace, visible name component or None for anonymous).
    scopes: list[tuple[bool, str | None]] = []
    statement: list[str] = []
    for token in TOKEN.findall(strip_cpp(text)):
        if token == "{":
            words = statement[:2]
            if "namespace" in words:
                after = statement[statement.index("namespace") + 1:]
                name = "::".join(part for part in after if part != "::") if after else None
                scopes.append((True, name or None))
            elif statement[:2] == ["extern", '"']:
                scopes.append((True, ""))
            else:
                at_namespace_scope = all(is_namespace for is_namespace, _ in scopes)
                visible = all(name is not None for _, name in scopes)
                if at_namespace_scope and visible:
                    prefix = "::".join(name for _, name in scopes if name)
                    tokens, is_template, is_specialization = _declaration_head(statement)
                    type_name = None if is_specialization else _type_name(tokens)
                    function_name = (None if type_name or is_specialization
                                     else _function_name(tokens, is_template))
                    if type_name:
                        definitions.append(("type", f"{prefix}::{type_name}" if prefix else type_name))
                    elif function_name:
                        definitions.append(
                            ("inline function", f"{prefix}::{function_name}" if prefix else function_name)
                        )
                scopes.append((False, None))
            statement = []
        elif token == "}":
            if scopes:
                scopes.pop()
            statement = []
        elif token == ";":
            statement = []
        else:
            statement.append(token)
    return definitions


def read_manifest(path: Path) -> list[tuple[str, str, Path]]:
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 3 or not all(fields):
            raise ValueError(f"{path}: malformed manifest row: {line!r}")
        rows.append((fields[0], fields[1], Path(fields[2])))
    return rows


def find_conflicts(
    rows: Iterable[tuple[str, str, Path]],
    read_text: Callable[[Path], str] = lambda path: path.read_text(encoding="utf-8", errors="replace"),
) -> list[str]:
    executables: dict[str, list[tuple[str, Path]]] = defaultdict(list)
    for executable, module, source in rows:
        executables[executable].append((module, source))

    errors: list[str] = []
    for executable, members in sorted(executables.items()):
        modules_by_source: dict[Path, set[str]] = defaultdict(set)
        for module, source in members:
            modules_by_source[source].add(module)
        for source, modules in sorted(modules_by_source.items()):
            if source.suffix.lower() in COMPILED_SUFFIXES and len(modules) > 1:
                errors.append(
                    f"{executable}: {source} is compiled by {', '.join(sorted(modules))}; "
                    "move shared helpers into a test support library"
                )

        owners: dict[tuple[str, str], dict[Path, str]] = defaultdict(dict)
        for source, modules in sorted(modules_by_source.items()):
            if source.suffix.lower() not in CPP_SUFFIXES or not source.is_file():
                continue
            module = sorted(modules)[0]
            for definition in set(external_definitions(read_text(source))):
                owners[definition].setdefault(source, module)
        for (kind, name), files in sorted(owners.items()):
            if len(files) > 1:
                locations = ", ".join(f"{path} ({module})" for path, module in sorted(files.items()))
                errors.append(
                    f"{executable}: {kind} '{name}' is defined in {locations}; "
                    "wrap test-local helpers in an anonymous namespace"
                )
    return errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--manifest", type=Path, required=True,
                        help="fluent_qt_test_executables.tsv from a configured test build")
    parser.add_argument("--self-test", action="store_true",
                        help="run the scanner regressions before checking the manifest")
    args = parser.parse_args(argv)
    if args.self_test:
        suite = unittest.defaultTestLoader.discover(
            str(Path(__file__).parent), pattern="test_check_test_executable_symbols.py")
        if not unittest.TextTestRunner(verbosity=1).run(suite).wasSuccessful():
            return 1
    try:
        rows = read_manifest(args.manifest)
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    errors = find_conflicts(rows)
    for error in errors:
        print(f"error: {error}", file=sys.stderr)
    if errors:
        return 1
    executables = {executable for executable, _, _ in rows}
    print(f"Checked {len(rows)} module sources in {len(executables)} test executables.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
