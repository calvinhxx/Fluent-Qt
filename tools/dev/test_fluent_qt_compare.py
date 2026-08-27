#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("fluent_qt_compare.py")
SPEC = importlib.util.spec_from_file_location("fluent_qt_compare", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class FluentQtCompareTest(unittest.TestCase):
    def test_builds_matching_live_and_native_commands(self):
        args = MODULE.parse_args(
            [
                "--route",
                "button",
                "--sample",
                "button-styles",
                "--scene",
                "fork.preview.py",
                "--theme",
                "dark",
                "--size",
                "720x540",
                "--rtl",
                "--no-build",
            ]
        )
        output = Path("/tmp/compare")
        live = MODULE.live_command(args, output)
        native = MODULE.native_command(args, output)
        for expected in ("dark", "720x540", "--rtl"):
            self.assertIn(expected, live)
            self.assertIn(expected, native)
        self.assertIn("--scene", live)
        self.assertNotIn("--route", live)
        self.assertIn("--route", native)
        self.assertIn("--no-build", native)

    def test_writes_ok_document_for_matching_artifacts(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            scene = output / "fork.preview.py"
            scene.write_text("# edited scene\n", encoding="utf-8")
            (output / "live.png").write_bytes(b"live")
            (output / "native.png").write_bytes(b"native")
            (output / "live.json").write_text(
                json.dumps(
                    {
                        "scene": str(scene),
                        "window": {
                            "theme": "dark",
                            "layout_direction": "rtl",
                            "width": 720,
                            "height": 540,
                        },
                        "snapshot": {"written": True},
                        "reload": {"last_error": None},
                    }
                ),
                encoding="utf-8",
            )
            (output / "native.json").write_text(
                json.dumps(
                    {
                        "status": "ok",
                        "selection": {
                            "route": "button",
                            "sample": "button-styles",
                        },
                        "scene": {
                            "theme": "dark",
                            "layout_direction": "rtl",
                            "actual_width": 720,
                            "actual_height": 540,
                        },
                        "artifacts": {"snapshot": {"written": True}},
                    }
                ),
                encoding="utf-8",
            )
            args = MODULE.parse_args(
                [
                    "--route",
                    "button",
                    "--sample",
                    "button-styles",
                    "--scene",
                    str(scene),
                    "--theme",
                    "dark",
                    "--size",
                    "720x540",
                    "--rtl",
                    "--output-dir",
                    str(output),
                ]
            )
            document = MODULE.comparison_document(args, output, 0, 0)
            self.assertEqual(document["status"], "ready-for-review")
            self.assertTrue(all(document["checks"].values()))
            self.assertEqual(document["live"]["scene"], str(scene))
            html = MODULE.comparison_html(document)
            self.assertIn('src="live.png"', html)
            self.assertIn('src="native.png"', html)
            self.assertIn("not pixel equality", html)

    def test_rejects_size_below_live_host_minimum(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                MODULE.parse_args(["--route", "button", "--size", "400x300"])


if __name__ == "__main__":
    unittest.main()
