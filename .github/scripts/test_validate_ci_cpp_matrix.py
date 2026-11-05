#!/usr/bin/env python3

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path

from classify_ci_changes import CppTestSelection
from select_ci_cpp_matrix import select_matrix


SCRIPT = Path(__file__).with_name("validate-ci-cpp-matrix.py")
SPEC = importlib.util.spec_from_file_location("validate_ci_cpp_matrix", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ValidateCiCppMatrixTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = json.loads(MODULE.DEFAULT_CATALOG.read_text(encoding="utf-8"))

    def test_repository_catalog_is_valid(self):
        self.assertEqual(MODULE.validate_catalog(self.catalog), [])

    def test_missing_scenario_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"].pop()
        self.assertTrue(any("is missing" in error for error in MODULE.validate_catalog(catalog)))

    def test_duplicate_id_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"][1]["id"] = catalog["scenarios"][0]["id"]
        self.assertTrue(any("duplicate scenario id" in error for error in MODULE.validate_catalog(catalog)))

    def test_unknown_mode_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"][0]["mode"] = "nightly"
        self.assertTrue(any("mode must be fast or full" in error for error in MODULE.validate_catalog(catalog)))

    def test_invalid_qt_source_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"][0]["qt_source"] = "system"
        self.assertTrue(any("qt_source" in error for error in MODULE.validate_catalog(catalog)))

    def test_non_positive_timeout_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"][0]["timeout_minutes"] = 0
        self.assertTrue(any("positive integer" in error for error in MODULE.validate_catalog(catalog)))

    def scenario(self, catalog, scenario_id):
        return next(item for item in catalog["scenarios"] if item["id"] == scenario_id)

    def test_parallel_ctest_requires_a_positive_job_count(self):
        for value in (0, True, "4", None):
            with self.subTest(value=value):
                catalog = copy.deepcopy(self.catalog)
                self.scenario(catalog, "linux-x64-qt62-full")["ctest_parallel"] = value
                self.assertTrue(any(
                    "ctest_parallel must be a positive integer" in error
                    for error in MODULE.validate_catalog(catalog)
                ))

    def test_parallel_ctest_cannot_exceed_pinned_runner_capacity(self):
        catalog = copy.deepcopy(self.catalog)
        self.scenario(catalog, "mac-arm64-qt62-full")["ctest_parallel"] = 4
        self.assertTrue(any(
            "must not exceed build_parallel 3" in error
            for error in MODULE.validate_catalog(catalog)
        ))

    def test_parallel_ctest_requires_pinned_capacity(self):
        catalog = copy.deepcopy(self.catalog)
        scenario = self.scenario(catalog, "linux-x64-qt62-full")
        scenario["build_parallel"] = ""
        self.assertTrue(any(
            "requires a pinned build_parallel" in error
            for error in MODULE.validate_catalog(catalog)
        ))

    def test_windows_and_native_platform_lanes_keep_serial_ctest(self):
        for scenario_id, message in (
            ("win-x64-qt62-platform", "must stay 1 on Windows"),
            ("win-arm64-qt693-native", "must stay 1 on a native QPA platform"),
        ):
            with self.subTest(lane=scenario_id):
                catalog = copy.deepcopy(self.catalog)
                scenario = self.scenario(catalog, scenario_id)
                scenario["build_parallel"] = "4"
                scenario["ctest_parallel"] = 2
                self.assertTrue(any(
                    message in error for error in MODULE.validate_catalog(catalog)
                ))

    def compiler_cache_errors_after(self, scenario_id, **fields):
        catalog = copy.deepcopy(self.catalog)
        self.scenario(catalog, scenario_id).update(fields)
        return [error for error in MODULE.validate_catalog(catalog) if "compiler cache" in error
                or "compiler_cache" in error]

    def test_fast_pr_lanes_reuse_identical_full_lane_snapshots(self):
        # The fast tier never runs on main, so it can only restore a full-tier owner.
        for fast_id, owner_id in (("linux-x64-qt62-fast", "linux-x64-qt62-full"),
                                  ("mac-arm64-qt62-build", "mac-arm64-qt62-full")):
            with self.subTest(lane=fast_id):
                fast = self.scenario(self.catalog, fast_id)
                owner = self.scenario(self.catalog, owner_id)
                self.assertEqual(fast["compiler_cache"], owner_id)
                self.assertEqual(owner["compiler_cache"], owner_id)
                self.assertEqual(owner["mode"], "full")
                selected = select_matrix(self.catalog, "fast", *CppTestSelection("none").output_values())
                job = next(job for job in selected["include"] if job["id"] == fast_id)
                self.assertEqual(job["compiler_cache"], owner_id)

    def test_compiler_cache_is_unix_only(self):
        for scenario in self.catalog["scenarios"]:
            if scenario["os"].startswith("windows"):
                self.assertEqual((scenario["compiler_cache"], scenario["compiler_cache_max_size"]), ("", ""))
        errors = self.compiler_cache_errors_after(
            "win-x64-qt62-platform", compiler_cache="win-x64-qt62-platform",
            compiler_cache_max_size="256M")
        self.assertTrue(any("MSBuild has no compiler launcher" in error for error in errors), errors)

    def test_compiler_cache_owner_must_be_a_full_lane(self):
        errors = self.compiler_cache_errors_after(
            "linux-x64-qt62-fast", compiler_cache="linux-x64-qt62-fast")
        self.assertTrue(any("owned by a full lane" in error for error in errors), errors)

    def test_compiler_cache_reference_must_name_a_compatible_owner(self):
        for owner, message in (
            ("linux-x64-qt62-missing", "must name a lane that owns that cache"),
            ("win-x64-qt62-fast", "must name a lane that owns that cache"),
            ("linux-x64-qt515-full", "must share qt_version"),
            ("linux-arm64-qt62-full", "must share os"),
        ):
            with self.subTest(owner=owner):
                errors = self.compiler_cache_errors_after(
                    "linux-x64-qt62-fast", compiler_cache=owner)
                self.assertTrue(any(message in error for error in errors), errors)

    def test_compiler_cache_size_is_explicit_and_consistent(self):
        for scenario_id, fields, message in (
            ("linux-x64-qt62-full", {"compiler_cache_max_size": "1G"}, "whole number of megabytes"),
            ("linux-x64-qt62-full", {"compiler_cache_max_size": ""}, "whole number of megabytes"),
            ("linux-x64-qt62-fast", {"compiler_cache_max_size": "256M"}, "must match linux-x64-qt62-full"),
            ("win-x64-qt62-fast", {"compiler_cache_max_size": "256M"}, "requires an enabled compiler cache"),
            ("linux-x64-qt62-full", {"compiler_cache": None}, "must be strings"),
        ):
            with self.subTest(lane=scenario_id, fields=fields):
                errors = self.compiler_cache_errors_after(scenario_id, **fields)
                self.assertTrue(any(message in error for error in errors), errors)

    def test_compiler_cache_owners_stay_inside_the_repository_budget(self):
        owners = [item for item in self.catalog["scenarios"] if item["compiler_cache"] == item["id"]]
        used = sum(int(item["compiler_cache_max_size"][:-1]) for item in owners)
        self.assertLessEqual(used, MODULE.COMPILER_CACHE_BUDGET_MB)
        errors = self.compiler_cache_errors_after(
            "mac-x64-qt62-gallery",
            compiler_cache_max_size=f"{MODULE.COMPILER_CACHE_BUDGET_MB - used + 193}M")
        self.assertTrue(any("repository budget" in error for error in errors), errors)

    def test_pr_selection_preserves_baseline_targets_and_labels_in_both_tiers(self):
        for tier in ("fast", "full"):
            owner = f"linux-x64-qt62-{tier}"
            baseline = self.scenario(self.catalog, owner)
            for scope in (CppTestSelection("selected", ("basicinput",)), CppTestSelection("all")):
                with self.subTest(tier=tier, scope=scope.scope):
                    _, labels, targets = scope.output_values()
                    jobs = select_matrix(self.catalog, tier, *scope.output_values())["include"]
                    self.assertIn(owner, {job["id"] for job in jobs})
                    for job in jobs:
                        source = self.scenario(self.catalog, job["id"])
                        if job["id"] != owner:
                            self.assertEqual(job["build_targets"], source["build_targets"])
                            self.assertEqual(job["test_labels"], source["test_labels"])
                            continue
                        expected = set(baseline["build_targets"].split()) | set(targets.split())
                        if scope.scope == "all":
                            expected.remove(f"fluent_qt_ci_{tier}_tests")
                        self.assertEqual(set(job["build_targets"].split()), expected)
                        self.assertEqual(job["test_labels"], f"({source['test_labels']}|{labels})")

    def test_candidate_only_replaces_the_windows_arm64_installer(self):
        scope = CppTestSelection("none").output_values()
        jobs = select_matrix(self.catalog, "full", *scope)["include"]
        candidate = select_matrix(self.catalog, "full", *scope, candidate_owns_package=True)["include"]
        self.assertIn("win-arm64-qt693-native", {job["id"] for job in candidate})
        self.assertEqual(candidate, [job for job in jobs if job["id"] != "win-arm64-qt693-cross"])
        self.assertEqual(len(jobs), len(candidate) + 1)
        installer = self.scenario(self.catalog, "win-arm64-qt693-cross")
        packages = json.loads(MODULE.DEFAULT_CATALOG.with_name("package-matrix.json").read_text(encoding="utf-8"))
        replacement = self.scenario(packages, "windows-arm64-qt693")
        self.assertIs(replacement["standard"], True)
        for field in ("os", "qt_version", "qt_arch", "host_qt_arch", "preset", "package_preset"):
            self.assertEqual(installer[field], replacement[field], field)

    def test_unsafe_or_disconnected_test_selection_is_rejected(self):
        for scope, labels, targets in (
            ("selected", "^(basicinput)$", "fluent_qt_windowing_tests"),
            ("selected", "^(unreviewed)$", "fluent_qt_unreviewed_tests"),
            ("all", "^qt$", "fluent_qt_ci_fast_tests"),
            ("none", "^qt$", ""),
        ):
            with self.subTest(scope=scope, labels=labels), self.assertRaises(ValueError):
                select_matrix(self.catalog, "full", scope, labels, targets)

    def test_minimal_qt_download_must_include_gallery_svg_dependency(self):
        catalog = copy.deepcopy(self.catalog)
        scenario = next(item for item in catalog["scenarios"] if item["id"] == "linux-x64-qt515-full")
        scenario["qt_archives"] = "qtbase icu"
        self.assertTrue(any("must include qtsvg" in error for error in MODULE.validate_catalog(catalog)))

    def test_non_boolean_first_window_trial_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"][0]["first_window_trial"] = "yes"
        self.assertTrue(
            any(
                "first_window_trial must be a boolean" in error
                for error in MODULE.validate_catalog(catalog)
            )
        )

    def test_first_window_trial_lane_set_is_locked(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"][0]["first_window_trial"] = False
        self.assertTrue(
            any(
                "fast first-window trials are missing" in error
                for error in MODULE.validate_catalog(catalog)
            )
        )

    def test_required_execution_lanes_cannot_disable_build_or_test(self):
        for contract in MODULE.REQUIRED_EXECUTION_LANES.values():
            for field in ("build", "test"):
                with self.subTest(lane=contract["id"], field=field):
                    catalog = copy.deepcopy(self.catalog)
                    scenario = next(
                        item
                        for item in catalog["scenarios"]
                        if item["id"] == contract["id"]
                    )
                    scenario[field] = False
                    errors = MODULE.validate_catalog(catalog)
                    self.assertTrue(
                        any("execution lane" in error for error in errors),
                        errors,
                    )

    def test_required_execution_lane_targets_and_labels_are_locked(self):
        for contract in MODULE.REQUIRED_EXECUTION_LANES.values():
            mutations = {
                "build_targets": "",
                "test_labels": "^nothing$",
                "exclude_labels": "",
            }
            for field, value in mutations.items():
                with self.subTest(lane=contract["id"], field=field):
                    catalog = copy.deepcopy(self.catalog)
                    scenario = next(
                        item
                        for item in catalog["scenarios"]
                        if item["id"] == contract["id"]
                    )
                    scenario[field] = value
                    errors = MODULE.validate_catalog(catalog)
                    self.assertTrue(
                        any("execution lane" in error for error in errors),
                        errors,
                    )

    def test_full_subset_cannot_also_build_unused_local_tests(self):
        for entry in self.catalog["scenarios"]:
            if entry["test_labels"] != "^ci_full$":
                continue
            with self.subTest(lane=entry["id"]):
                catalog = copy.deepcopy(self.catalog)
                scenario = next(item for item in catalog["scenarios"]
                                if item["id"] == entry["id"])
                scenario["build_targets"] += " fluent_qt_all_tests"
                self.assertTrue(any(
                    "must not build the exhaustive local test set" in error
                    for error in MODULE.validate_catalog(catalog)
                ))

    def test_required_execution_lanes_cannot_omit_optional_qml_bridge(self):
        for contract in MODULE.REQUIRED_EXECUTION_LANES.values():
            with self.subTest(lane=contract["id"]):
                catalog = copy.deepcopy(self.catalog)
                scenario = next(item for item in catalog["scenarios"]
                                if item["id"] == contract["id"])
                scenario["configure_args"] = " ".join(
                    option for option in scenario["configure_args"].split()
                    if option != "-DFLUENT_QT_BUILD_QML_BRIDGE=ON"
                )
                self.assertTrue(any(
                    "must configure -DFLUENT_QT_BUILD_QML_BRIDGE=ON" in error
                    for error in MODULE.validate_catalog(catalog)
                ))

    def test_sdk_installation_cannot_omit_a_configured_library_build(self):
        for target in ("FluentQt", "FluentQtSpatial", "FluentQtQmlBridge"):
            with self.subTest(target=target):
                catalog = copy.deepcopy(self.catalog)
                scenario = next(item for item in catalog["scenarios"]
                                if item["id"] == "mac-arm64-qt62-build")
                scenario["build_targets"] = " ".join(
                    value for value in scenario["build_targets"].split()
                    if value != target
                )
                self.assertTrue(any(
                    "SDK installation lane must build target " + target in error
                    for error in MODULE.validate_catalog(catalog)
                ))

    def test_sdk_installation_keeps_qml_bridge_enabled(self):
        catalog = copy.deepcopy(self.catalog)
        scenario = next(item for item in catalog["scenarios"]
                        if item["id"] == "mac-arm64-qt62-build")
        scenario["configure_args"] = "-DBUILD_TESTING=OFF"
        self.assertTrue(any(
            "SDK installation lane must enable QmlBridge" in error
            for error in MODULE.validate_catalog(catalog)
        ))

    def test_linux_fast_sdk_installation_builds_qml_bridge_without_test_selection(self):
        catalog = copy.deepcopy(self.catalog)
        scenario = next(item for item in catalog["scenarios"]
                        if item["id"] == "linux-x64-qt62-fast")
        scenario["build_targets"] = "fluent_qt_ci_fast_tests"
        self.assertTrue(any(
            "must build target FluentQtQmlBridge for SDK installation" in error
            for error in MODULE.validate_catalog(catalog)
        ))


if __name__ == "__main__":
    unittest.main()
