#!/usr/bin/env python3

"""Regression tests for validate_visual_evidence_inventory.py."""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


VALIDATOR_PATH = Path(__file__).with_name("validate_visual_evidence_inventory.py")
SPEC = importlib.util.spec_from_file_location("visual_inventory_validator", VALIDATOR_PATH)
assert SPEC is not None and SPEC.loader is not None
VALIDATOR = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = VALIDATOR
SPEC.loader.exec_module(VALIDATOR)


class VisualEvidenceInventoryValidatorTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.root = Path(self.temporary_directory.name)
        self.rule = VALIDATOR.RiskFamilyRule(
            capabilities=frozenset({"visual-risk"}),
            additions=frozenset(),
            exclusions=frozenset(),
            states=("normal-light", "native-input"),
            owner="maintainers:test",
            rationale="Test surfaces need deterministic geometry and human review.",
            manual_reason="Native input requires a human desktop review.",
        )
        self.rules = {"test-family": self.rule}
        self.catalog = {
            "components": [
                {
                    "id": "widget",
                    "capabilities": ["visual-risk"],
                    "tests": [
                        {
                            "source_url": (
                                "https://example.invalid/blob/main/"
                                "tests/components/TestWidget.cpp"
                            )
                        }
                    ],
                    "gallery": {
                        "route_id": "widget",
                        "sample_source_url": (
                            "https://example.invalid/blob/main/app/sample.cpp"
                        ),
                    },
                }
            ]
        }
        self.inventory = {
            "schema_version": 1,
            "phase_status": "active",
            "source_catalog": "site/api/catalog.json",
            "evidence_status_model": copy.deepcopy(
                VALIDATOR.EVIDENCE_STATUS_MODEL
            ),
            "risk_families": [
                {
                    "id": "test-family",
                    "severity": "high",
                    "owner": self.rule.owner,
                    "rationale": self.rule.rationale,
                    "components": ["widget"],
                    "required_states": list(self.rule.states),
                    "manual_contract": {
                        "status": "human-required",
                        "platforms": copy.deepcopy(VALIDATOR.MANUAL_PLATFORMS),
                        "procedure": VALIDATOR.MANUAL_PROCEDURE,
                        "reason": self.rule.manual_reason,
                    },
                }
            ],
            "standard_risk_components": {
                "status": "catalog-tracked",
                "reason": (
                    "No TD-3 high-risk family rule matched; these components remain "
                    "under focused test, accessibility, API, and manual VisualCheck "
                    "policy."
                ),
                "component_ids": [],
            },
            "components": [
                {
                    "id": "widget",
                    "family": "test-family",
                    "automated_evidence": [
                        {
                            "kind": "geometry",
                            "test_case": "WidgetTest.Geometry",
                            "states": ["normal-light"],
                            "execution": "ci",
                        }
                    ],
                    "manual_evidence": {
                        "status": "manual-required",
                        "coverage": "all-required-states",
                        "surface": "visual-check",
                        "test_case": "WidgetTest.VisualCheck",
                    },
                }
            ],
            "approval_host": copy.deepcopy(VALIDATOR.APPROVAL_HOST),
            "future_bundle_policy": copy.deepcopy(VALIDATOR.FUTURE_BUNDLE_POLICY),
            "platform_boundary": copy.deepcopy(VALIDATOR.PLATFORM_BOUNDARY),
            "representative_pixel_gates": [],
            "open_gaps": copy.deepcopy(VALIDATOR.OPEN_GAPS),
        }
        self.test_source = """
TEST_F(WidgetTest, Geometry) {
    EXPECT_TRUE(true);
}

TEST_F(WidgetTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP();
    }
    qApp->exec();
}
"""
        self.manual_allowlist = []
        self.local_desktop_allowlist = []
        self.roadmap_state = "Active"

    def write_fixture(self) -> None:
        self.write_json("site/api/catalog.json", self.catalog)
        self.write_json(
            "docs/development/visual-evidence-inventory.json", self.inventory
        )
        self.write_text("tests/components/TestWidget.cpp", self.test_source)
        allowlist = "\n".join(f"    {name}" for name in self.manual_allowlist)
        local_desktop = "\n".join(
            f"    {name}" for name in self.local_desktop_allowlist
        )
        self.write_text(
            "tests/CMakeLists.txt",
            "add_qt_test_module(test_widget components/TestWidget.cpp)\n"
            "set(FLUENT_QT_CI_FAST_TARGETS\n"
            "    test_widget\n"
            ")\n"
            "set(FLUENT_QT_CI_FULL_TARGETS\n"
            "    ${FLUENT_QT_CI_FAST_TARGETS}\n"
            ")\n"
            "set(FLUENT_QT_MANUAL_VISUAL_TESTS\n"
            f"{allowlist}\n"
            ")\n"
            "set(FLUENT_QT_LOCAL_DESKTOP_TESTS\n"
            f"{local_desktop}\n"
            ")\n",
        )
        self.write_text("app/sample.cpp", "// Gallery sample fixture.\n")
        self.write_text(
            "docs/development/technical-debt-roadmap.md",
            "| Phase | State | Scope | Exit condition |\n"
            "|---|---|---|---|\n"
            f"| TD-3 — High-risk visual regression rollout | {self.roadmap_state} "
            "| Test scope | Test exit |\n",
        )

    def write_text(self, relative_path: str, content: str) -> None:
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def write_bytes(self, relative_path: str, content: bytes) -> None:
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)

    def write_json(self, relative_path: str, value: object) -> None:
        self.write_text(relative_path, json.dumps(value, indent=2) + "\n")

    def validate(self, expected_pixel_gates=None):
        self.write_fixture()
        return VALIDATOR.validate(
            self.root,
            rules=self.rules,
            expected_pixel_gates=(
                {} if expected_pixel_gates is None else expected_pixel_gates
            ),
        )

    def assert_error_contains(self, needle: str, errors: list[str]) -> None:
        self.assertTrue(
            any(needle in error for error in errors),
            f"missing {needle!r} in {errors!r}",
        )

    def test_valid_fixture_passes_without_granting_manual_approval(self) -> None:
        summary, errors = self.validate()
        self.assertEqual(errors, [])
        self.assertEqual(summary.high_risk_components, 1)
        self.assertEqual(summary.manual_visual_surfaces, 1)

    def test_unknown_top_level_field_is_rejected(self) -> None:
        self.inventory["covered"] = True
        _, errors = self.validate()
        self.assert_error_contains("unsupported fields: covered", errors)

    def test_family_membership_cannot_be_self_declared(self) -> None:
        self.inventory["risk_families"][0]["components"] = []
        _, errors = self.validate()
        self.assert_error_contains("does not match its risk rule", errors)

    def test_catalog_complement_tracks_every_non_high_risk_component(self) -> None:
        self.catalog["components"].append(
            {
                "id": "ordinary-widget",
                "capabilities": ["ordinary"],
                "tests": [],
                "gallery": {"route_id": "ordinary-widget"},
            }
        )
        _, errors = self.validate()
        self.assert_error_contains("exact canonical catalog complement", errors)

        self.inventory["standard_risk_components"]["component_ids"] = [
            "ordinary-widget"
        ]
        _, errors = self.validate()
        self.assertEqual(errors, [])

    def test_duplicate_catalog_component_is_rejected(self) -> None:
        self.catalog["components"].append(copy.deepcopy(self.catalog["components"][0]))
        _, errors = self.validate()
        self.assert_error_contains("duplicate source catalog component", errors)

    def test_case_drift_is_rejected(self) -> None:
        self.catalog["components"][0]["id"] = "Widget"
        _, errors = self.validate()
        self.assertTrue(
            any(
                "unknown components" in error or "high-risk component" in error
                for error in errors
            )
        )

    def test_unknown_family_field_is_rejected(self) -> None:
        self.inventory["risk_families"][0]["covered"] = True
        _, errors = self.validate()
        self.assert_error_contains("unsupported fields: covered", errors)

    def test_manual_event_loop_with_nonstandard_name_requires_allowlist(self) -> None:
        self.test_source = self.test_source.replace("VisualCheck", "VisualGallery")
        manual = self.inventory["components"][0]["manual_evidence"]
        manual["test_case"] = "WidgetTest.VisualGallery"
        _, errors = self.validate()
        self.assert_error_contains("manual visual test is not labeled local-only", errors)

        self.manual_allowlist = ["WidgetTest.VisualGallery"]
        _, errors = self.validate()
        self.assertEqual(errors, [])

    def test_qapplication_exec_is_recognized_as_manual_event_loop(self) -> None:
        self.test_source = self.test_source.replace("qApp->exec()", "QApplication::exec()")
        _, errors = self.validate()
        self.assertEqual(errors, [])

    def test_visualcheck_name_without_event_loop_is_rejected(self) -> None:
        self.test_source += "\nTEST_F(WidgetTest, DecorativeVisualCheck) {\n"
        self.test_source += "    EXPECT_TRUE(true);\n}\n"
        _, errors = self.validate()
        self.assert_error_contains("has no guarded event-loop contract", errors)

    def test_commented_manual_contract_is_rejected(self) -> None:
        self.test_source = self.test_source.replace(
            'if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {\n'
            "        GTEST_SKIP();\n"
            "    }\n"
            "    qApp->exec();",
            "// qEnvironmentVariableIsSet(\"SKIP_VISUAL_TEST\");\n"
            "    // qApp->exec();\n"
            "    EXPECT_TRUE(true);",
        )
        _, errors = self.validate()
        self.assert_error_contains("has no guarded event-loop contract", errors)

    def test_manual_contract_in_unrelated_helper_is_rejected(self) -> None:
        self.test_source = self.test_source.replace(
            'if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {\n'
            "        GTEST_SKIP();\n"
            "    }\n"
            "    qApp->exec();",
            "EXPECT_TRUE(true);",
        )
        self.test_source += """
void unrelatedManualHelper() {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        return;
    }
    qApp->exec();
}
"""
        _, errors = self.validate()
        self.assert_error_contains("has no guarded event-loop contract", errors)

    def test_manual_guard_without_exit_is_rejected(self) -> None:
        self.test_source = self.test_source.replace(
            'if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {\n'
            "        GTEST_SKIP();\n"
            "    }",
            '(void)qEnvironmentVariableIsSet("SKIP_VISUAL_TEST");',
        )
        _, errors = self.validate()
        self.assert_error_contains("has no guarded event-loop contract", errors)

    def test_inverted_manual_guard_is_rejected(self) -> None:
        self.test_source = self.test_source.replace(
            'if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST"))',
            'if (!qEnvironmentVariableIsSet("SKIP_VISUAL_TEST"))',
        )
        _, errors = self.validate()
        self.assert_error_contains("has no guarded event-loop contract", errors)

    def test_manual_guard_after_event_loop_is_rejected(self) -> None:
        self.test_source = self.test_source.replace(
            'if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {\n'
            "        GTEST_SKIP();\n"
            "    }\n"
            "    qApp->exec();",
            'qApp->exec();\n'
            '    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {\n'
            "        GTEST_SKIP();\n"
            "    }",
        )
        _, errors = self.validate()
        self.assert_error_contains("has no guarded event-loop contract", errors)

    def test_manual_guard_nested_in_false_branch_is_rejected(self) -> None:
        self.test_source = self.test_source.replace(
            'if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {\n'
            "        GTEST_SKIP();\n"
            "    }",
            'if (false) {\n'
            '        if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {\n'
            "            GTEST_SKIP();\n"
            "        }\n"
            "    }",
        )
        _, errors = self.validate()
        self.assert_error_contains("has no guarded event-loop contract", errors)

    def test_manual_guard_in_uninvoked_lambda_is_rejected(self) -> None:
        self.test_source = self.test_source.replace(
            'if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {\n'
            "        GTEST_SKIP();\n"
            "    }",
            'const auto skipVisual = [] {\n'
            '        if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {\n'
            "            GTEST_SKIP();\n"
            "        }\n"
            "    };",
        )
        _, errors = self.validate()
        self.assert_error_contains("has no guarded event-loop contract", errors)

    def test_unreachable_or_nested_manual_event_loop_is_rejected(self) -> None:
        guard = (
            'if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {'
            " GTEST_SKIP(); }"
        )
        invalid_bodies = (
            f"return; {guard} qApp->exec();",
            f"{guard} return; qApp->exec();",
            f"{guard} GTEST_SKIP(); qApp->exec();",
            f"{guard} if (false) {{ qApp->exec(); }}",
            f"{guard} auto f = [] {{ qApp->exec(); }};",
            f"{guard} else {{ return; }} qApp->exec();",
            f"{guard} if (true) {{ return; }} qApp->exec();",
            f"{guard} do {{ return; }} while (false); qApp->exec();",
        )
        for body in invalid_bodies:
            with self.subTest(body=body):
                self.assertFalse(VALIDATOR.is_manual_visual_body(body))

    def test_explicit_headless_skip_after_visual_guard_is_allowed(self) -> None:
        body = (
            'if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {'
            ' GTEST_SKIP(); } '
            'if (tests::support::isHeadlessPlatform()) GTEST_SKIP(); '
            'qApp->exec();'
        )
        self.assertTrue(VALIDATOR.is_manual_visual_body(body))

    def test_headless_skip_cannot_precede_visual_guard(self) -> None:
        body = (
            'if (tests::support::isHeadlessPlatform()) GTEST_SKIP(); '
            'if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {'
            ' GTEST_SKIP(); } qApp->exec();'
        )
        self.assertFalse(VALIDATOR.is_manual_visual_body(body))

    def test_multiline_test_macro_is_rejected_as_undiscoverable(self) -> None:
        self.test_source = self.test_source.replace(
            "TEST_F(WidgetTest, Geometry)",
            "TEST_F(WidgetTest,\n       Geometry)",
        )
        _, errors = self.validate()
        self.assert_error_contains(
            "gtest_add_tests cannot discover a multiline TEST", errors
        )

    def test_manual_visual_source_must_belong_to_a_test_target(self) -> None:
        visual_case = """
TEST_F(WidgetTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP();
    }
    qApp->exec();
}
"""
        self.test_source = self.test_source.replace(visual_case, "")
        self.catalog["components"][0]["tests"].append(
            {
                "source_url": (
                    "https://example.invalid/blob/main/"
                    "tests/components/TestManual.cpp"
                )
            }
        )
        self.write_fixture()
        self.write_text("tests/components/TestManual.cpp", visual_case)
        _, errors = VALIDATOR.validate(
            self.root, rules=self.rules, expected_pixel_gates={}
        )
        self.assert_error_contains(
            "manual visual test source is not registered", errors
        )

    def test_automated_execution_lane_must_match_cmake(self) -> None:
        automated = self.inventory["components"][0]["automated_evidence"][0]
        automated["execution"] = "registered-only"
        _, errors = self.validate()
        self.assert_error_contains("execution must be ci", errors)

    def test_unreachable_cmake_append_does_not_grant_ci_execution(self) -> None:
        self.write_fixture()
        cmake = self.root / "tests/CMakeLists.txt"
        text = cmake.read_text(encoding="utf-8").replace(
            "set(FLUENT_QT_CI_FAST_TARGETS\n"
            "    test_widget\n"
            ")",
            "set(FLUENT_QT_CI_FAST_TARGETS)\n"
            "if(FALSE)\n"
            "    list(APPEND FLUENT_QT_CI_FAST_TARGETS test_widget)\n"
            "endif()",
            1,
        )
        cmake.write_text(text, encoding="utf-8")
        self.assertNotIn(
            "test_widget",
            VALIDATOR.cmake_target_list(
                cmake, "FLUENT_QT_CI_FAST_TARGETS"
            ),
        )
        _, errors = VALIDATOR.validate(
            self.root, rules=self.rules, expected_pixel_gates={}
        )
        self.assert_error_contains("execution must be registered-only", errors)

    def test_literal_false_cmake_expressions_do_not_grant_ci(self) -> None:
        for condition in (
            "FALSE AND TRUE",
            "FALSE OR FALSE",
            "NOT (TRUE)",
            "UNKNOWN AND FALSE",
            "1 EQUAL 0",
            "0 GREATER 1",
            '"a" STREQUAL "b"',
            'FALSE MATCHES "TRUE"',
            "1 VERSION_LESS 1",
        ):
            with self.subTest(condition=condition):
                self.write_fixture()
                cmake = self.root / "tests/CMakeLists.txt"
                text = cmake.read_text(encoding="utf-8").replace(
                    "set(FLUENT_QT_CI_FAST_TARGETS\n"
                    "    test_widget\n"
                    ")",
                    "set(FLUENT_QT_CI_FAST_TARGETS)\n"
                    f"if({condition})\n"
                    "    list(APPEND FLUENT_QT_CI_FAST_TARGETS test_widget)\n"
                    "endif()",
                    1,
                ).replace(
                    "set(FLUENT_QT_CI_FULL_TARGETS\n"
                    "    ${FLUENT_QT_CI_FAST_TARGETS}\n"
                    ")",
                    "set(FLUENT_QT_CI_FULL_TARGETS)",
                    1,
                )
                cmake.write_text(text, encoding="utf-8")
                _, errors = VALIDATOR.validate(
                    self.root, rules=self.rules, expected_pixel_gates={}
                )
                self.assert_error_contains(
                    "execution must be registered-only", errors
                )

    def test_unreachable_elseif_or_else_does_not_grant_ci(self) -> None:
        branches = (
            "if(SOME_CONFIG)\n"
            "elseif(FALSE)\n"
            "    list(APPEND FLUENT_QT_CI_FAST_TARGETS test_widget)\n"
            "endif()",
            "if(SOME_CONFIG)\n"
            "elseif(TRUE)\n"
            "else()\n"
            "    list(APPEND FLUENT_QT_CI_FAST_TARGETS test_widget)\n"
            "endif()",
        )
        for branch in branches:
            with self.subTest(branch=branch):
                self.write_fixture()
                cmake = self.root / "tests/CMakeLists.txt"
                text = cmake.read_text(encoding="utf-8").replace(
                    "set(FLUENT_QT_CI_FAST_TARGETS\n"
                    "    test_widget\n"
                    ")",
                    "set(FLUENT_QT_CI_FAST_TARGETS)\n" + branch,
                    1,
                ).replace(
                    "set(FLUENT_QT_CI_FULL_TARGETS\n"
                    "    ${FLUENT_QT_CI_FAST_TARGETS}\n"
                    ")",
                    "set(FLUENT_QT_CI_FULL_TARGETS)",
                    1,
                )
                cmake.write_text(text, encoding="utf-8")
                self.assertNotIn(
                    "test_widget",
                    VALIDATOR.cmake_target_list(
                        cmake, "FLUENT_QT_CI_FAST_TARGETS"
                    ),
                )
                _, errors = VALIDATOR.validate(
                    self.root, rules=self.rules, expected_pixel_gates={}
                )
                self.assert_error_contains(
                    "execution must be registered-only", errors
                )

    def test_later_cmake_removal_or_reset_revokes_ci_execution(self) -> None:
        mutations = (
            "list(REMOVE_ITEM FLUENT_QT_CI_FAST_TARGETS test_widget)\n"
            "list(REMOVE_ITEM FLUENT_QT_CI_FULL_TARGETS test_widget)\n",
            "set(FLUENT_QT_CI_FAST_TARGETS)\n"
            "set(FLUENT_QT_CI_FULL_TARGETS)\n",
        )
        for mutation in mutations:
            with self.subTest(mutation=mutation.split("(", 1)[0]):
                self.write_fixture()
                cmake = self.root / "tests/CMakeLists.txt"
                with cmake.open("a", encoding="utf-8") as stream:
                    stream.write(mutation)
                self.assertNotIn(
                    "test_widget",
                    VALIDATOR.cmake_target_list(
                        cmake, "FLUENT_QT_CI_FAST_TARGETS"
                    ),
                )
                self.assertNotIn(
                    "test_widget",
                    VALIDATOR.cmake_target_list(
                        cmake, "FLUENT_QT_CI_FULL_TARGETS"
                    ),
                )
                _, errors = VALIDATOR.validate(
                    self.root, rules=self.rules, expected_pixel_gates={}
                )
                self.assert_error_contains(
                    "execution must be registered-only", errors
                )

    def test_unreachable_cmake_registration_is_not_evidence(self) -> None:
        wrappers = (
            "if(FALSE)\n"
            "    add_qt_test_module(test_widget components/TestWidget.cpp)\n"
            "endif()",
            "if(1 EQUAL 0)\n"
            "    add_qt_test_module(test_widget components/TestWidget.cpp)\n"
            "endif()",
            "function(register_fake_widget)\n"
            "    add_qt_test_module(test_widget components/TestWidget.cpp)\n"
            "endfunction()",
            "macro(register_fake_widget)\n"
            "    add_qt_test_module(test_widget components/TestWidget.cpp)\n"
            "endmacro()",
        )
        for wrapper in wrappers:
            with self.subTest(wrapper=wrapper.split("(", 1)[0]):
                self.write_fixture()
                cmake = self.root / "tests/CMakeLists.txt"
                text = cmake.read_text(encoding="utf-8").replace(
                    "add_qt_test_module(test_widget components/TestWidget.cpp)",
                    wrapper,
                    1,
                )
                cmake.write_text(text, encoding="utf-8")
                _, errors = VALIDATOR.validate(
                    self.root, rules=self.rules, expected_pixel_gates={}
                )
                self.assertTrue(
                    any("not registered" in error for error in errors),
                    errors,
                )

    def test_cmake_string_decoys_are_not_test_registrations(self) -> None:
        decoys = (
            'set(DOC "add_qt_test_module(test_widget '
            'components/TestWidget.cpp)")',
            'message("add_qt_test_module(test_widget '
            'components/TestWidget.cpp)")',
            'set(DOC [=[add_qt_test_module(test_widget '
            'components/TestWidget.cpp)]=])',
            'set(DOC [=[)\nadd_qt_test_module(test_widget '
            'components/TestWidget.cpp)\n]=])',
        )
        for decoy in decoys:
            with self.subTest(command=decoy.split("(", 1)[0]):
                self.write_fixture()
                cmake = self.root / "tests/CMakeLists.txt"
                text = cmake.read_text(encoding="utf-8").replace(
                    "add_qt_test_module(test_widget components/TestWidget.cpp)",
                    decoy,
                    1,
                )
                cmake.write_text(text, encoding="utf-8")
                _, errors = VALIDATOR.validate(
                    self.root, rules=self.rules, expected_pixel_gates={}
                )
                self.assert_error_contains(
                    "not registered in a test target", errors
                )

    def test_orphan_cmake_registration_is_not_evidence(self) -> None:
        self.write_fixture()
        cmake = self.root / "tests/CMakeLists.txt"
        registration = (
            "add_qt_test_module(test_widget components/TestWidget.cpp)\n"
        )
        cmake.write_text(
            cmake.read_text(encoding="utf-8").replace(registration, "", 1),
            encoding="utf-8",
        )
        self.write_text("tests/orphan/CMakeLists.txt", registration)
        _, errors = VALIDATOR.validate(
            self.root, rules=self.rules, expected_pixel_gates={}
        )
        self.assertTrue(any("not registered" in error for error in errors), errors)

    def test_placement_state_requires_geometry_evidence(self) -> None:
        self.rule = VALIDATOR.RiskFamilyRule(
            capabilities=self.rule.capabilities,
            additions=self.rule.additions,
            exclusions=self.rule.exclusions,
            states=("normal-light", "placement", "native-input"),
            owner=self.rule.owner,
            rationale=self.rule.rationale,
            manual_reason=self.rule.manual_reason,
        )
        self.rules = {"test-family": self.rule}
        self.inventory["risk_families"][0]["required_states"] = list(
            self.rule.states
        )
        automated = self.inventory["components"][0]["automated_evidence"][0]
        automated["kind"] = "interaction"
        automated["states"] = ["normal-light", "placement"]
        _, errors = self.validate()
        self.assert_error_contains("require geometry evidence", errors)

    def test_contract_lane_counts_as_ci_outside_target_allowlists(self) -> None:
        self.test_source = self.test_source.replace("WidgetTest, Geometry", "WidgetTest, Contract_Geometry")
        automated = self.inventory["components"][0]["automated_evidence"][0]
        automated["test_case"] = "WidgetTest.Contract_Geometry"
        self.write_fixture()
        cmake = self.root / "tests/CMakeLists.txt"
        cmake.write_text(
            cmake.read_text(encoding="utf-8").replace("    test_widget\n", ""),
            encoding="utf-8",
        )
        _, errors = VALIDATOR.validate(
            self.root, rules=self.rules, expected_pixel_gates={}
        )
        self.assertEqual(errors, [])

    def test_local_desktop_allowlist_excludes_ci_target_evidence(self) -> None:
        self.local_desktop_allowlist = ["WidgetTest.Geometry"]
        _, errors = self.validate()
        self.assert_error_contains("execution must be registered-only", errors)

        self.inventory["components"][0]["automated_evidence"][0][
            "execution"
        ] = "registered-only"
        _, errors = self.validate()
        self.assertEqual(errors, [])

    def test_local_desktop_allowlist_excludes_contract_lane_evidence(self) -> None:
        self.test_source = self.test_source.replace(
            "WidgetTest, Geometry", "WidgetTest, Contract_Geometry"
        )
        automated = self.inventory["components"][0]["automated_evidence"][0]
        automated["test_case"] = "WidgetTest.Contract_Geometry"
        automated["execution"] = "registered-only"
        self.local_desktop_allowlist = ["WidgetTest.Contract_Geometry"]
        self.write_fixture()
        cmake = self.root / "tests/CMakeLists.txt"
        cmake.write_text(
            cmake.read_text(encoding="utf-8").replace("    test_widget\n", ""),
            encoding="utf-8",
        )
        _, errors = VALIDATOR.validate(
            self.root, rules=self.rules, expected_pixel_gates={}
        )
        self.assertEqual(errors, [])

    def test_commented_cmake_registration_is_not_evidence(self) -> None:
        self.write_fixture()
        cmake = self.root / "tests/CMakeLists.txt"
        text = cmake.read_text(encoding="utf-8")
        cmake.write_text(
            text.replace(
                "add_qt_test_module(test_widget components/TestWidget.cpp)",
                "# add_qt_test_module(test_widget components/TestWidget.cpp)",
            ),
            encoding="utf-8",
        )
        _, errors = VALIDATOR.validate(
            self.root, rules=self.rules, expected_pixel_gates={}
        )
        self.assert_error_contains("not registered in a test target", errors)

    def test_bracket_commented_cmake_registration_is_not_evidence(self) -> None:
        for opening, closing in (("#[[", "]]"), ("#[=[", "]=]")):
            with self.subTest(opening=opening):
                self.write_fixture()
                cmake = self.root / "tests/CMakeLists.txt"
                text = cmake.read_text(encoding="utf-8")
                cmake.write_text(
                    text.replace(
                        "add_qt_test_module(test_widget components/TestWidget.cpp)",
                        f"{opening}\n"
                        "add_qt_test_module(test_widget components/TestWidget.cpp)\n"
                        f"{closing}",
                    ),
                    encoding="utf-8",
                )
                _, errors = VALIDATOR.validate(
                    self.root, rules=self.rules, expected_pixel_gates={}
                )
                self.assert_error_contains("not registered in a test target", errors)

    def test_stale_manual_allowlist_entry_is_rejected(self) -> None:
        self.manual_allowlist = ["MissingTest.VisualGallery"]
        _, errors = self.validate()
        self.assert_error_contains("stale manual visual CMake entry", errors)

    def test_manual_surface_cannot_claim_pass(self) -> None:
        manual = self.inventory["components"][0]["manual_evidence"]
        manual["status"] = "pass"
        _, errors = self.validate()
        self.assert_error_contains("must remain manual-required", errors)

    def test_automated_evidence_cannot_claim_native_input(self) -> None:
        automated = self.inventory["components"][0]["automated_evidence"][0]
        automated["states"] = ["native-input"]
        _, errors = self.validate()
        self.assert_error_contains("cannot claim native-platform states", errors)

    def test_missing_automated_test_case_is_rejected(self) -> None:
        automated = self.inventory["components"][0]["automated_evidence"][0]
        automated["test_case"] = "WidgetTest.DoesNotExist"
        _, errors = self.validate()
        self.assert_error_contains("references a missing test case", errors)

    def test_duplicate_or_reordered_states_are_rejected(self) -> None:
        automated = self.inventory["components"][0]["automated_evidence"][0]
        automated["states"] = ["normal-light", "normal-light"]
        _, errors = self.validate()
        self.assert_error_contains("contains duplicates", errors)

    def test_automatic_baseline_updates_cannot_be_enabled(self) -> None:
        self.inventory["future_bundle_policy"]["automatic_baseline_updates"] = True
        _, errors = self.validate()
        self.assert_error_contains("future_bundle_policy has drifted", errors)

    def test_open_gap_cannot_be_silently_deleted(self) -> None:
        self.inventory["open_gaps"].pop()
        _, errors = self.validate()
        self.assert_error_contains("open gaps are incomplete", errors)

    def test_roadmap_cannot_claim_td3_complete_while_gaps_are_open(self) -> None:
        self.roadmap_state = "Complete"
        _, errors = self.validate()
        self.assert_error_contains("TD-3 state does not match", errors)

    def test_pseudo_png_is_not_pixel_evidence(self) -> None:
        baseline = b"png"
        baseline_path = "tests/visual-baselines/widget.png"
        gate = {
            "component_ids": ["widget"],
            "test_case": "VisualGateTest.Widget",
            "baseline": baseline_path,
            "sha256": hashlib.sha256(baseline).hexdigest(),
            "state_ids": ["normal-light"],
        }
        self.inventory["representative_pixel_gates"] = [
            {
                "id": "widget",
                "kind": "legacy-representative",
                **gate,
                "execution": "approval-host-only",
                "known_gap": VALIDATOR.LEGACY_PIXEL_KNOWN_GAP,
            }
        ]
        self.test_source += "\nTEST(VisualGateTest, Widget) { EXPECT_TRUE(true); }\n"
        self.write_fixture()
        self.write_bytes(baseline_path, baseline)
        _, errors = VALIDATOR.validate(
            self.root,
            rules=self.rules,
            expected_pixel_gates={"widget": gate},
        )
        self.assert_error_contains("not a decodable PNG header", errors)

    def test_traversal_test_source_is_rejected(self) -> None:
        source_url = self.catalog["components"][0]["tests"][0]["source_url"]
        self.catalog["components"][0]["tests"][0]["source_url"] = source_url.replace(
            "tests/components/TestWidget.cpp", "../TestWidget.cpp"
        )
        _, errors = self.validate()
        self.assert_error_contains("focused test source does not exist", errors)


if __name__ == "__main__":
    unittest.main()
