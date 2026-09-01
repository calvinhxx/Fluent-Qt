#!/usr/bin/env python3

import importlib.util
import re
import subprocess
import sys
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("classify_ci_changes.py")
PROJECT_ROOT = SCRIPT.parents[2]
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

    def assert_cpp_selection(self, paths, *, scope, labels, targets):
        result = MODULE.select_cpp_tests(paths)
        self.assertEqual(result.scope, scope)
        self.assertEqual(result.label_regex, labels)
        self.assertEqual(result.targets, tuple(targets))

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
                self.assertEqual(
                    MODULE.select_cpp_tests_from_github_file_pages(
                        pages, expected_count
                    ),
                    MODULE.CppTestSelection("all"),
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
        with self.assertRaisesRegex(ValueError, "No changed files"):
            MODULE.select_cpp_tests([])

    def test_each_component_directory_has_a_precise_test_selection(self):
        for group in MODULE.CPP_COMPONENT_TEST_GROUPS:
            with self.subTest(group=group):
                self.assert_cpp_selection(
                    [f"src/components/{group}/Changed.cpp"],
                    scope="selected",
                    labels=f"^({group})$",
                    targets=[f"fluent_qt_{group}_tests"],
                )

    def test_component_groups_match_registered_subdirectories(self):
        contents = (PROJECT_ROOT / "tests/components/CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        registered = set(re.findall(r"add_subdirectory\(([a-z_]+)\)", contents))
        self.assertEqual(registered, set(MODULE.CPP_COMPONENT_TEST_GROUPS))

    def test_component_test_path_selects_its_own_group(self):
        self.assert_cpp_selection(
            ["tests/components/date_time/TestDatePicker.cpp"],
            scope="selected",
            labels="^(date_time)$",
            targets=["fluent_qt_date_time_tests"],
        )

    def test_multiple_component_groups_are_stable_and_deduplicated(self):
        self.assert_cpp_selection(
            [
                "src/components/collections/ListView.cpp",
                "tests/components/basicinput/TestButton.cpp",
                "src/components/basicinput/Button.h",
            ],
            scope="selected",
            labels="^(basicinput|collections)$",
            targets=[
                "fluent_qt_basicinput_tests",
                "fluent_qt_collections_tests",
            ],
        )

    def test_wide_cpp_paths_fail_closed_to_all_tests(self):
        paths = (
            "src/components/foundation/FluentElement.cpp",
            "src/design/FluentTheme.cpp",
            "src/compatibility/QtCompat.h",
            "src/utils/FluentQtLogging.cpp",
            "include/FluentQt/FluentQt.h",
            "CMakeLists.txt",
            "cmake/FluentQtInstallHeaders.cmake",
            "tests/CMakeLists.txt",
            "tests/components/CMakeLists.txt",
            "tests/support/QtGTestMain.cpp",
            "src/components/unknown/NewComponent.cpp",
        )
        for path in paths:
            with self.subTest(path=path):
                self.assert_cpp_selection(
                    [path],
                    scope="all",
                    labels="^qt$",
                    targets=["fluent_qt_all_tests"],
                )

    def test_non_cpp_product_surfaces_do_not_select_component_tests(self):
        paths = (
            "README.md",
            "docs/development/testing-workflow.md",
            "app/pages/basicinput_page.py",
            "bindings/pyside6/native/typesystem_fluentqt.xml",
            "site/api/catalog.json",
            "tools/docs/validate_documentation.py",
        )
        for path in paths:
            with self.subTest(path=path):
                self.assert_cpp_selection(
                    [path], scope="none", labels="", targets=[]
                )

    def test_irrelevant_path_does_not_widen_a_precise_component_change(self):
        self.assert_cpp_selection(
            [
                "app/pages/basicinput_page.py",
                "src/components/basicinput/Button.cpp",
            ],
            scope="selected",
            labels="^(basicinput)$",
            targets=["fluent_qt_basicinput_tests"],
        )

    def test_component_rename_selects_source_and_destination_groups(self):
        selection = MODULE.select_cpp_tests_from_github_file_pages(
            [
                [
                    {
                        "filename": "src/components/layout/Card.cpp",
                        "previous_filename": (
                            "src/components/status_info/Card.cpp"
                        ),
                    }
                ]
            ],
            1,
        )
        self.assertEqual(selection.scope, "selected")
        self.assertEqual(selection.label_regex, "^(layout|status_info)$")
        self.assertEqual(
            selection.targets,
            ("fluent_qt_layout_tests", "fluent_qt_status_info_tests"),
        )

    def test_unknown_or_unsafe_component_names_cannot_reach_outputs(self):
        paths = (
            "src/components/basicinput|all/Injected.cpp",
            "src/components/basicinput/../../foundation/FluentElement.cpp",
            "src/components/basicinput\\Injected.cpp",
            " src/components/basicinput/Button.cpp",
            "src/components/basicinput/Button.cpp\rmalicious=true",
        )
        for path in paths:
            with self.subTest(path=path):
                self.assert_cpp_selection(
                    [path],
                    scope="all",
                    labels="^qt$",
                    targets=["fluent_qt_all_tests"],
                )
        with self.assertRaisesRegex(ValueError, "Unknown C\\+\\+ component"):
            MODULE._cpp_tests_for_groups({"basicinput|all"})
        invalid_selections = (
            MODULE.CppTestSelection("all\nmalicious=true"),
            MODULE.CppTestSelection("all", ("basicinput",)),
            MODULE.CppTestSelection("selected", ("basicinput|all",)),
        )
        for selection in invalid_selections:
            with self.subTest(selection=selection):
                with self.assertRaises(ValueError):
                    selection.output_values()

    def test_cli_outputs_only_single_line_allowlisted_values(self):
        completed = subprocess.run(
            [sys.executable, str(SCRIPT)],
            input=(
                "src/components/collections/ListView.cpp\n"
                "src/components/basicinput/Button.cpp\n"
            ),
            text=True,
            capture_output=True,
            check=True,
        )
        output = completed.stdout.splitlines()
        self.assertEqual(output[3], "cpp_test_scope=selected")
        self.assertEqual(
            output[4], "cpp_test_label_regex=^(basicinput|collections)$"
        )
        self.assertEqual(
            output[5],
            "cpp_test_targets=fluent_qt_basicinput_tests fluent_qt_collections_tests",
        )
        self.assertEqual(len(output), 6)
        for value in output[3:]:
            self.assertNotIn("\r", value)


if __name__ == "__main__":
    unittest.main()
