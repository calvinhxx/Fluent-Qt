"""Validate portable wheel checks and opt-in source checks without Qt."""

import importlib.util
from pathlib import Path, PurePosixPath
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock


SCRIPT = Path(__file__).with_name("test_gallery_wheel_smoke.py")
SPEC = importlib.util.spec_from_file_location("gallery_wheel_smoke", SCRIPT)
SMOKE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SMOKE)


class GalleryWheelSmokeContractsTest(unittest.TestCase):
    def test_runtime_coverage_supports_both_optional_module_states(self):
        contract = {
            "categories": [{"id": "basic"}, {"id": "spatial"}],
            "components": [
                {"id": "button", "category_id": "basic", "samples": [{"id": "button-basic"}]},
                {"id": "spatial-view", "category_id": "spatial", "samples": [{"id": "spatial-basic"}]},
            ],
            "routes": [{"id": "button", "parent_id": "basic"},
                       {"id": "spatial", "parent_id": ""},
                       {"id": "spatial-view", "parent_id": "spatial"}],
            "binding_support_types": ["Theme"],
        }
        for available in (False, True):
            with self.subTest(spatial_available=available):
                components = contract["components"] if available else contract["components"][:1]
                categories = [SimpleNamespace(id=c["id"]) for c in contract["categories"][:2 if available else 1]]
                entries = [SimpleNamespace(route_id=c["id"], samples=[SimpleNamespace(**s) for s in c["samples"]])
                           for c in components]
                routes = [SimpleNamespace(id=r["id"]) for r in contract["routes"][:3 if available else 1]]
                keys = {(c["id"], s["id"]) for c in components for s in c["samples"]}
                SMOKE.verify_runtime_catalog(contract, categories, entries, routes, {"Theme"}, keys, available)
                if available:
                    SMOKE.verify_runtime_catalog(contract, categories, entries, routes, {"Theme"}, keys,
                                                 available, require_spatial=True)
                else:
                    with self.assertRaisesRegex(AssertionError, "Release Gallery must include Spatial"):
                        SMOKE.verify_runtime_catalog(contract, categories, entries, routes, {"Theme"}, keys,
                                                     available, require_spatial=True)
                # Same-sized wrong catalog and missing ports must still fail.
                with self.assertRaisesRegex(AssertionError, "wrong coverage"):
                    SMOKE.verify_runtime_catalog(contract, categories, entries, routes, {"Theme"}, set(), available)
                routes[0].id = "wrong-component"
                with self.assertRaisesRegex(AssertionError, "wrong coverage"):
                    SMOKE.verify_runtime_catalog(contract, categories, entries, routes, {"Theme"}, keys, available)

    def images(self, root, names):
        directory = root / "assets/control_images"
        for name in names:
            path = directory / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"image fixture")
        return directory

    def record(self, names):
        return [PurePosixPath("fluentqt_gallery/assets/control_images") / name for name in names]

    def test_standalone_check_uses_installed_record(self):
        with tempfile.TemporaryDirectory() as temporary:
            package = Path(temporary)
            self.images(package, ["Placeholder.png", "charts/LineChart.png"])
            SMOKE.verify_control_images(package, self.record(["Placeholder.png", "charts/LineChart.png"]))

    def test_missing_and_replaced_images_fail_even_when_the_count_matches(self):
        for installed in (["Placeholder.png"], ["Placeholder.png", "charts/Wrong.png"]):
            with self.subTest(installed=installed), tempfile.TemporaryDirectory() as temporary:
                package = Path(temporary)
                self.images(package, installed)
                with self.assertRaisesRegex(AssertionError, "wheel RECORD"):
                    SMOKE.verify_control_images(package, self.record(["Placeholder.png", "charts/LineChart.png"]))

    def test_missing_record_cannot_silently_skip_asset_validation(self):
        for record in (None, []):
            with self.subTest(record=record), tempfile.TemporaryDirectory() as temporary:
                package = Path(temporary)
                self.images(package, ["Placeholder.png"])
                with self.assertRaisesRegex(AssertionError, "wheel RECORD"):
                    SMOKE.verify_control_images(package, record)

    def test_explicit_source_check_rejects_an_invalid_checkout(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(AssertionError, "project-root"):
                SMOKE.verify_source_contract(Path(temporary), Path(temporary), {})

    def test_source_check_catches_contract_and_source_asset_drift(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = root / "installed"
            self.images(package, ["Placeholder.png"])
            self.images(root / "app", ["Placeholder.png"])
            contract = {"summary": {"sample_count": 1}}
            with mock.patch.object(SMOKE, "native_contract", return_value=contract):
                SMOKE.verify_source_contract(root, package, contract)
                with self.assertRaisesRegex(AssertionError, "contract differs"):
                    SMOKE.verify_source_contract(root, package, {})
                self.images(root / "app", ["charts/Added.png"])
                with self.assertRaisesRegex(AssertionError, "images differ"):
                    SMOKE.verify_source_contract(root, package, contract)

    def test_ci_and_publication_keep_the_source_comparison_mandatory(self):
        root = SCRIPT.parents[4]
        for name in ("ci-python.yml", "python-release.yml"):
            contents = (root / ".github/workflows" / name).read_text(encoding="utf-8")
            calls = [line for line in contents.splitlines() if "test_gallery_wheel_smoke.py" in line]
            with self.subTest(workflow=name):
                self.assertTrue(calls)
                self.assertTrue(all(line.rstrip().endswith("--project-root .") for line in calls))


if __name__ == "__main__":
    unittest.main()
