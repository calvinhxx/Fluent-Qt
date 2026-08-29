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
                "tools/docs/validate_documentation.py",
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

    def test_component_api_policy_runs_native_validation(self):
        self.assert_classification(
            ["docs/development/component-api-policy.json"],
            native=True,
            pyside=False,
            wasm=False,
        )

    def test_visual_evidence_inventory_runs_native_validation(self):
        self.assert_classification(
            ["docs/development/visual-evidence-inventory.json"],
            native=True,
            pyside=False,
            wasm=False,
        )

    def test_technical_debt_roadmap_runs_native_validation(self):
        self.assert_classification(
            ["docs/development/technical-debt-roadmap.md"],
            native=True,
            pyside=False,
            wasm=False,
        )

    def test_canonical_component_catalog_runs_native_validation(self):
        self.assert_classification(
            ["site/api/catalog.json"],
            native=True,
            pyside=False,
            wasm=False,
        )

    def test_renamed_governance_files_keep_native_validation(self):
        rename_pairs = (
            (
                "docs/development/component-api-policy-v2.json",
                "docs/development/component-api-policy.json",
            ),
            (
                "docs/development/technical-debt-roadmap-v2.md",
                "docs/development/technical-debt-roadmap.md",
            ),
            (
                "docs/development/visual-evidence-inventory-v2.json",
                "docs/development/visual-evidence-inventory.json",
            ),
            ("site/api/catalog-v2.json", "site/api/catalog.json"),
        )
        for current_path, previous_path in rename_pairs:
            with self.subTest(previous_path=previous_path):
                self.assert_classification(
                    [current_path, previous_path],
                    native=True,
                    pyside=False,
                    wasm=False,
                )

    def test_github_pages_include_rename_sources(self):
        result = MODULE.classify_github_file_pages(
            [
                [
                    {
                        "filename": (
                            "docs/development/visual-evidence-inventory-v2.json"
                        ),
                        "previous_filename": (
                            "docs/development/visual-evidence-inventory.json"
                        ),
                    }
                ]
            ],
            1,
        )
        self.assertEqual(
            result,
            MODULE.ChangeClassification(True, False, False),
        )

    def test_github_pages_preserve_exact_documentation_only_classification(self):
        result = MODULE.classify_github_file_pages(
            [
                [{"filename": "README.md"}],
                [{"filename": "docs/development/testing-workflow.md"}],
            ],
            2,
        )
        self.assertEqual(
            result,
            MODULE.ChangeClassification(False, False, False),
        )

    def test_github_file_cap_and_count_mismatch_enable_every_matrix(self):
        pages = [[{"filename": "README.md"}]]
        for expected_count in (2, MODULE.GITHUB_PULL_FILES_LIMIT + 1):
            with self.subTest(expected_count=expected_count):
                self.assertEqual(
                    MODULE.classify_github_file_pages(pages, expected_count),
                    MODULE.ChangeClassification(True, True, True),
                )

    def test_github_file_payload_is_closed(self):
        invalid_payloads = (
            {},
            [["README.md"]],
            [[{"previous_filename": "README.md"}]],
            [[{"filename": "README.md", "previous_filename": ""}]],
        )
        for payload in invalid_payloads:
            with self.subTest(payload=payload):
                with self.assertRaises(ValueError):
                    MODULE.classify_github_file_pages(payload, 1)

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

    def test_onboarding_tool_runs_native_matrix_only(self):
        self.assert_classification(
            ["tools/onboarding/fluentqt_doctor.py"],
            native=True,
            pyside=False,
            wasm=False,
        )

    def test_shared_starter_creator_runs_native_and_pyside_matrices(self):
        self.assert_classification(
            ["tools/onboarding/fluentqt_create.py"],
            native=True,
            pyside=True,
            wasm=False,
        )

    def test_first_window_trial_runs_native_and_pyside_matrices(self):
        self.assert_classification(
            ["tools/onboarding/fluentqt_trial.py"],
            native=True,
            pyside=True,
            wasm=False,
        )

    def test_python_starter_runs_native_and_pyside_matrices(self):
        self.assert_classification(
            [
                "tools/onboarding/starters/pyside6-workbench/"
                "src/app_module/app/main.py.in"
            ],
            native=True,
            pyside=True,
            wasm=False,
        )

    def test_cpp_starter_skips_pyside_matrix(self):
        self.assert_classification(
            [
                "tools/onboarding/starters/cpp-workbench/"
                "src/app/main.cpp.in"
            ],
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
