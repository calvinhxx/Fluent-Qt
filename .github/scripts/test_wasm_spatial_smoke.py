#!/usr/bin/env python3
"""Qt/browser-free checks for the Spatial browser result boundary."""

import importlib.util
from pathlib import Path
import unittest


SPEC = importlib.util.spec_from_file_location(
    "wasm_spatial_smoke", Path(__file__).with_name("run-wasm-spatial-smoke.py")
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class SpatialResultTest(unittest.TestCase):
    def test_fallback_requires_usable_two_dimensional_presentation(self):
        for metrics in ({}, {"fallback": True}, {"two_dimensional_usable": True}):
            with self.subTest(metrics=metrics), self.assertRaises(RuntimeError):
                MODULE.validate_spatial_result(
                    {"state": "pass", "metrics": metrics}, [], expect_fallback=True
                )
        MODULE.validate_spatial_result(
            {"state": "pass", "metrics": {"fallback": True, "two_dimensional_usable": True}},
            [], expect_fallback=True,
        )

    def test_fallback_is_not_hardware_validation(self):
        with self.assertRaisesRegex(RuntimeError, "cannot pass hardware"):
            MODULE.validate_spatial_result(
                {"state": "pass", "metrics": {"fallback": True}}, [], expect_fallback=False
            )

    def test_page_error_overrides_a_passing_probe(self):
        with self.assertRaisesRegex(RuntimeError, "context lost"):
            MODULE.validate_spatial_result(
                {"state": "pass"}, ["context lost"], expect_fallback=False
            )

    def test_probe_failure_is_not_reclassified(self):
        with self.assertRaisesRegex(RuntimeError, "timed out"):
            MODULE.validate_spatial_result(
                {"state": "fail", "detail": "timed out"}, [], expect_fallback=False
            )


if __name__ == "__main__":
    unittest.main()
