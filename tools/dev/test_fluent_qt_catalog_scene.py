#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
from pathlib import Path
from types import SimpleNamespace
import sys
import unittest


SCRIPT = Path(__file__).with_name("fluent_qt_catalog_scene.py")
SPEC = importlib.util.spec_from_file_location("fluent_qt_catalog_scene", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class FluentQtCatalogSceneTest(unittest.TestCase):
    def test_finds_the_single_materialized_root_name(self):
        root = SimpleNamespace()
        root._fluentqt_gallery_source_namespace = {
            "gallery_parent": object(),
            "root": root,
            "alias": root,
        }
        result = SimpleNamespace(
            widget=root,
            route_id="button",
            sample_id="button-styles",
        )
        with self.assertRaises(RuntimeError):
            MODULE._root_name(result)

        root._fluentqt_gallery_source_namespace.pop("alias")
        self.assertEqual(MODULE._root_name(result), "root")

    def test_generated_scene_executes_preview_source_inside_build(self):
        source = MODULE.scene_source(
            route="button",
            sample="button-styles",
            title="Button · Button styles",
            preview_source=(
                "root = {'parent': globals().get('gallery_parent')}\n"
            ),
            root_name="root",
        )
        namespace = {}
        exec(compile(source, "<fork>", "exec"), namespace)
        parent = object()
        result = namespace["build"](parent)
        self.assertIs(result["parent"], parent)
        self.assertIsNone(namespace["gallery_parent"])
        self.assertEqual(namespace["CATALOG_ROUTE"], "button")
        self.assertIn("Gallery catalog", source)


if __name__ == "__main__":
    unittest.main()
