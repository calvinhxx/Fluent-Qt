#!/usr/bin/env python3

import importlib.util
import sys
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("classify_ci_changes.py")
SPEC = importlib.util.spec_from_file_location("classify_ci_changes", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ClassifyCiChangesTest(unittest.TestCase):
    def assert_classification(self, paths, *, native, pyside, wasm):
        result = MODULE.classify_changes(paths)
        self.assertEqual(result.should_build, native)
        self.assertEqual(result.should_build_pyside, pyside)
        self.assertEqual(result.should_build_wasm, wasm)

    def test_documentation_only_skips_all_builds(self):
        self.assert_classification(
            [
                "README.md",
                "docs/development/testing-workflow.md",
                "site/index.html",
                "tools/site/generate_localized_site.py",
            ],
            native=False,
            pyside=False,
            wasm=False,
        )

    def test_ai_contract_only_skips_native_builds(self):
        self.assert_classification(
            [
                "llms.txt",
                ".agents/skills/build-fluentqt-gui/SKILL.md",
                "docs/ai/guidance.json",
                "tools/ai/query_ai_catalog.py",
                ".agents/skills/build-fluentqt-gui/agents/openai.yaml",
            ],
            native=False,
            pyside=False,
            wasm=False,
        )

    def test_library_change_runs_both_matrices(self):
        self.assert_classification(
            ["src/components/basicinput/Button.cpp"],
            native=True,
            pyside=True,
            wasm=True,
        )

    def test_binding_change_runs_both_matrices(self):
        self.assert_classification(
            ["bindings/pyside6/native/typesystem_fluentqt.xml"],
            native=True,
            pyside=True,
            wasm=False,
        )

    def test_gallery_asset_change_runs_pyside_matrix(self):
        self.assert_classification(
            ["app/assets/control_images/Button.png"],
            native=True,
            pyside=True,
            wasm=True,
        )

    def test_native_test_only_change_skips_pyside_matrix(self):
        self.assert_classification(
            ["tests/components/basicinput/TestButton.cpp"],
            native=True,
            pyside=False,
            wasm=False,
        )

    def test_unrelated_workflow_change_skips_pyside_matrix(self):
        self.assert_classification(
            [".github/workflows/site.yml"],
            native=True,
            pyside=False,
            wasm=False,
        )

    def test_ci_workflow_change_runs_pyside_matrix(self):
        self.assert_classification(
            [".github/workflows/ci.yml"],
            native=True,
            pyside=True,
            wasm=True,
        )

    def test_python_module_change_runs_pyside_matrix(self):
        self.assert_classification(
            [".github/workflows/ci-python.yml"],
            native=True,
            pyside=True,
            wasm=False,
        )

    def test_cpp_module_change_skips_pyside_matrix(self):
        self.assert_classification(
            [".github/workflows/ci-cpp.yml", ".github/ci-cpp-matrix.json"],
            native=True,
            pyside=False,
            wasm=False,
        )

    def test_mixed_test_and_library_change_runs_pyside_matrix(self):
        self.assert_classification(
            [
                "tests/components/basicinput/TestButton.cpp",
                "src/components/basicinput/Button.h",
            ],
            native=True,
            pyside=True,
            wasm=True,
        )

    def test_wasm_module_change_runs_wasm_only(self):
        self.assert_classification(
            [".github/workflows/ci-wasm.yml"],
            native=True,
            pyside=False,
            wasm=True,
        )

    def test_wasm_smoke_change_runs_wasm_only(self):
        self.assert_classification(
            [".github/scripts/run-wasm-browser-smoke.py"],
            native=True,
            pyside=False,
            wasm=True,
        )

    def test_wasm_adapter_change_runs_wasm_only(self):
        self.assert_classification(
            ["platforms/webassembly/CMakeLists.txt"],
            native=True,
            pyside=False,
            wasm=True,
        )

    def test_empty_input_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "No changed files"):
            MODULE.classify_changes([])


if __name__ == "__main__":
    unittest.main()
