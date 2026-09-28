#!/usr/bin/env python3
"""Run binding contracts, excluding methods already registered in isolated CTest processes."""

from __future__ import annotations

import argparse
import importlib
from pathlib import Path
import unittest


def test_cases(suite):
    for item in suite:
        if isinstance(item, unittest.TestSuite):
            yield from test_cases(item)
        else:
            yield item


def binding_suite(module, excluded: set[str]) -> unittest.TestSuite:
    cases = list(test_cases(unittest.defaultTestLoader.loadTestsFromModule(module)))
    names = {case.id().split(".", 1)[1] for case in cases}
    unknown = excluded - names
    if unknown:
        raise ValueError(f"Isolated binding tests were not found: {', '.join(sorted(unknown))}")
    selected = [case for case in cases if case.id().split(".", 1)[1] not in excluded]
    if not selected:
        raise ValueError("No binding contracts remain after isolation filtering")
    return unittest.TestSuite(selected)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exclude-file", type=Path)
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()
    excluded = set(args.exclude_file.read_text(encoding="utf-8").splitlines()) if args.exclude_file else set()
    suite = binding_suite(importlib.import_module("test_bindings"), excluded)
    return 0 if unittest.TextTestRunner(verbosity=2 if args.verbose else 1).run(suite).wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
