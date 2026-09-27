#!/usr/bin/env python3
"""Qt/browser-free checks for projected source input coordinates."""

import importlib.util
from pathlib import Path
import unittest


SPEC = importlib.util.spec_from_file_location(
    "wasm_browser_smoke", Path(__file__).with_name("run-wasm-browser-smoke.py")
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class SourceInputGeometryTest(unittest.TestCase):
    def test_accepts_projected_nonhorizontal_lines(self):
        MODULE.validate_source_input_probe({
            "state": "expanded", "header": [300, 820], "copy": [970, 870],
            "lines": [[[310, 890], [1000, 875]], [[310, 912], [1000, 897]]],
        })

    def test_closed_source_requires_only_header(self):
        MODULE.validate_source_input_probe({"state": "ready", "header": [300, 820]})

    def test_rejects_proxy_rectangle_instead_of_projected_points(self):
        with self.assertRaises(RuntimeError):
            MODULE.validate_source_input_probe({
                "state": "ready", "header": {"x": 300, "y": 820, "width": 700, "height": 32},
            })

    def test_expanded_source_requires_two_complete_segments(self):
        for lines in (None, [], [[[1, 2]]], [[[1, 2], [3, 4]]]):
            with self.subTest(lines=lines), self.assertRaises(RuntimeError):
                MODULE.validate_source_input_probe({
                    "state": "expanded", "header": [1, 2], "copy": [3, 4], "lines": lines,
                })

    def test_rejects_invalid_coordinate_values(self):
        for value in (float("nan"), float("inf"), "3", True):
            with self.subTest(value=value), self.assertRaises(RuntimeError):
                MODULE.validate_source_input_probe({"state": "ready", "header": [value, 2]})


if __name__ == "__main__":
    unittest.main()
