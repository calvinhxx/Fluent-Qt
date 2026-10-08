#!/usr/bin/env python3
"""Qt/browser-free checks for source input geometry and Pages payloads."""

import importlib.util
from argparse import Namespace
import copy
import json
from pathlib import Path
import tempfile
import unittest


SPEC = importlib.util.spec_from_file_location(
    "wasm_browser_smoke", Path(__file__).with_name("run-wasm-browser-smoke.py")
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
STAGE_SPEC = importlib.util.spec_from_file_location(
    "wasm_pages_stage", Path(__file__).with_name("stage-wasm-pages.py")
)
STAGE = importlib.util.module_from_spec(STAGE_SPEC)
STAGE_SPEC.loader.exec_module(STAGE)


class MinimalExamplePresentationTest(unittest.TestCase):
    def spatial_state(self):
        return {"window": {"width": 720, "height": 520},
                "card": {"width": 320, "height": 200},
                "view": {"width": 696, "height": 460},
                "button": {"width": 153, "height": 33},
                "rotation": [6, -12, 0], "maximumTilt": [3, 5],
                "surfaceIntensity": 0.6, "text": "Hello from FluentQt",
                "spatialEnabled": True, "projected": True, "buttonCenter": [640, 400]}

    def test_spatial_rejects_different_card_pose_or_button_content(self):
        state = self.spatial_state()
        MODULE.validate_hello_world_spatial_state(state, "Hello from FluentQt")
        for key, value in (("card", {"width": 280, "height": 200}),
                           ("rotation", [-4, 6, 0]), ("text", "Enable Spatial")):
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                changed = copy.deepcopy(state)
                changed[key] = value
                MODULE.validate_hello_world_spatial_state(changed, "Hello from FluentQt")

    def test_spatial_requires_finite_projected_click_coordinates(self):
        state = self.spatial_state()
        state["buttonCenter"] = [float("nan"), 400]
        with self.assertRaises(RuntimeError):
            MODULE.validate_hello_world_spatial_state(state, "Hello from FluentQt")

    def test_plugin_normalizes_equal_buttons_to_available_content(self):
        from wasm_plugin_contract import validate_plugin_presentation
        for width, button_width in ((600, 270), (576, 258)):
            with self.subTest(width=width):
                presentation = {
                    "window": [600, 500], "content": [width, 450],
                    "margins": [24, 24, 24, 24], "spacing": 12,
                    "status": {"text": "Extension inactive; created greetings stay available"},
                    "activation": {"text": "Activate extension", "enabled": True,
                                   "x": 24, "y": 53, "width": button_width, "height": 33},
                    "creation": {"text": "Create greeting", "enabled": False,
                                 "x": 36 + button_width, "y": 53,
                                 "width": button_width, "height": 33},
                }
                validate_plugin_presentation({"presentation": presentation})
                presentation["creation"]["y"] = 100
                with self.assertRaises(RuntimeError):
                    validate_plugin_presentation({"presentation": presentation})

    def test_plugin_measures_two_and_four_greeting_gaps_from_visible_geometry(self):
        from wasm_plugin_contract import validate_plugin_greeting_spacing
        for count in (2, 4):
            with self.subTest(count=count):
                state = {"greetings": [{"x": 185, "y": 227 + 45 * index,
                                         "width": 528, "height": 33}
                                        for index in range(count)]}
                self.assertEqual(validate_plugin_greeting_spacing(state, count),
                                 [12] * (count - 1))
                for index, greeting in enumerate(state["greetings"]):
                    greeting["y"] = 227 + 33 * index
                with self.assertRaisesRegex(RuntimeError, "observed.*0"):
                    validate_plugin_greeting_spacing(state, count)

    def test_plugin_spacing_requires_all_greetings_to_have_visible_geometry(self):
        from wasm_plugin_contract import validate_plugin_greeting_spacing
        for greetings in ([{"x": 0, "y": 0, "width": 20, "height": 33}],
                          [{"x": 0, "y": 0, "width": 20, "height": 33}, {}],
                          [{"x": 0, "y": 0, "width": 20, "height": 0},
                           {"x": 0, "y": 45, "width": 20, "height": 33}]):
            with self.subTest(greetings=greetings), self.assertRaises(RuntimeError):
                validate_plugin_greeting_spacing({"greetings": greetings}, 2)


class PagesSpatialPayloadTest(unittest.TestCase):
    def stage_fixture(self, directory):
        source = directory / "source"
        destination = directory / "pages"
        groups = {
            "": STAGE.PAYLOAD_FILES,
            "hello-world": STAGE.HELLO_WORLD_PAYLOAD_FILES,
            "hello-world-spatial": STAGE.HELLO_WORLD_SPATIAL_PAYLOAD_FILES,
            "plugin-extension": STAGE.PLUGIN_EXTENSION_PAYLOAD_FILES,
        }
        for folder, names in groups.items():
            for name in names:
                path = source / folder / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(str(path.relative_to(source)))
        (source / "index.html").write_text(
            "qtloader.js fluent_qt_gallery.js licenses.html app-icon.png")
        return Namespace(source=source, destination=destination, qt_version="6.9.3",
                         emscripten_version="3.1.70", validation_mode="full", commit="test")

    def test_stages_independent_spatial_runtime_and_metadata(self):
        with tempfile.TemporaryDirectory() as temporary:
            args = self.stage_fixture(Path(temporary))
            STAGE.stage(args)
            for name in STAGE.HELLO_WORLD_SPATIAL_PAYLOAD_FILES:
                self.assertEqual((args.destination / "hello-world-spatial" / name).read_bytes(),
                                 (args.source / "hello-world-spatial" / name).read_bytes())
            self.assertEqual(json.loads((args.destination / "build-info.json").read_text())[
                "validation_mode"], "full")

    def test_rejects_missing_spatial_binary_before_staging(self):
        with tempfile.TemporaryDirectory() as temporary:
            args = self.stage_fixture(Path(temporary))
            (args.source / "hello-world-spatial/fluentqt_hello_world_spatial.wasm").unlink()
            with self.assertRaisesRegex(RuntimeError, "hello-world-spatial/.*wasm"):
                STAGE.stage(args)
            self.assertFalse(args.destination.exists())


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
