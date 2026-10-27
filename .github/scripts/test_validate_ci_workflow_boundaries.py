#!/usr/bin/env python3

import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import unittest
from unittest import mock


SCRIPT = Path(__file__).with_name("validate-ci-workflow-boundaries.py")
SPEC = importlib.util.spec_from_file_location(
    "validate_ci_workflow_boundaries", SCRIPT
)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ValidateCiWorkflowBoundariesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workflow = MODULE.read_workflow("ci-cpp.yml")

    def assert_contract_error(self, workflow: str) -> None:
        errors = MODULE.validate_cpp_execution_contract(workflow)
        self.assertTrue(errors, "mutated C++ Test step unexpectedly passed")

    def assert_plan_contract_error(self, workflow: str) -> None:
        errors = MODULE.validate_cpp_plan_contract(workflow)
        self.assertTrue(errors, "mutated C++ plan job unexpectedly passed")

    def assert_integration_contract_error(self, workflow: str) -> None:
        errors = MODULE.validate_cpp_integration_contract(workflow)
        self.assertTrue(errors, "mutated C++ integration job unexpectedly passed")

    def workflow_errors_with_replacement(
        self, workflow_name: str, original: str, replacement: str
    ) -> list[str]:
        original_read = MODULE.read_workflow

        def with_replacement(name: str) -> str:
            contents = original_read(name)
            if name != workflow_name:
                return contents
            self.assertIn(original, contents)
            return contents.replace(original, replacement, 1)

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=with_replacement
        ):
            return MODULE.validate_boundaries()

    @staticmethod
    def reindent_job(workflow: str, job_id: str, entry: str) -> str:
        section = MODULE.job_section(workflow, job_id)
        lines = section.splitlines()
        replacement = "\n".join(
            [lines[0], f"     {entry}", *[f" {line}" for line in lines[1:]]]
        )
        if section.endswith("\n"):
            replacement += "\n"
        return workflow.replace(section, replacement, 1)

    def test_repository_cpp_execution_contract_is_valid(self):
        self.assertEqual(
            MODULE.validate_cpp_execution_contract(self.workflow), []
        )

    def test_repository_cpp_plan_contract_is_valid(self):
        self.assertEqual(MODULE.validate_cpp_plan_contract(self.workflow), [])

    def test_repository_cpp_integration_contract_is_valid(self):
        self.assertEqual(
            MODULE.validate_cpp_integration_contract(self.workflow), []
        )

    def test_source_package_inventory_requires_module_and_example_entries(self):
        for path in MODULE.SOURCE_PACKAGE_CMAKE_ENTRIES:
            with self.subTest(path=path):
                original = f"              {path} \\\n"
                self.assertIn(original, self.workflow)
                self.assert_integration_contract_error(
                    self.workflow.replace(original, "", 1)
                )
                # A comment containing the same path cannot satisfy inventory.
                self.assert_integration_contract_error(
                    self.workflow.replace(original, f"          # {path}\n", 1)
                )

    def test_source_package_inventory_keeps_missing_files_fatal(self):
        original = 'if ! grep -x "FluentQt-ci-source/$source_file" \\'
        self.assertIn(original, self.workflow)
        self.assert_integration_contract_error(
            self.workflow.replace(original, original.replace("! ", ""), 1)
        )
        step = MODULE.named_step_section(
            MODULE.job_section(self.workflow, "integration"),
            "Verify FluentQt library source package",
        )
        self.assertTrue(step)
        self.assert_integration_contract_error(self.workflow.replace(step, "", 1))

    def test_repository_workflow_boundaries_are_valid(self):
        self.assertEqual(MODULE.validate_boundaries(), [])

    def test_desktop_candidate_requires_installed_arm64_validation(self):
        for requirement in (
            "smoke-installed-windows-gallery.ps1",
            "-RequireQt6SpatialRuntime",
            "needs: [plan, package, windows_arm64_install]",
            "needs.windows_arm64_install.result == 'success'",
            "needs.plan.outputs.windows_arm64 == 'false' && needs.windows_arm64_install.result == 'skipped'",
        ):
            with self.subTest(requirement=requirement):
                errors = self.workflow_errors_with_replacement(
                    "desktop-release-candidate.yml", requirement, "removed"
                )
                self.assertTrue(any("packaging contract" in error for error in errors))

    def test_optional_spatial_binding_cannot_silently_skip_coverage(self):
        for requirement in ("-DFLUENT_QT_BUILD_SPATIAL=ON", "assert SPATIAL_AVAILABLE",
                            "-R '^test_pyside6_gallery_spatial_backdrop_native$'"):
            with self.subTest(requirement=requirement):
                errors = self.workflow_errors_with_replacement("ci-python.yml", requirement, "")
                self.assertTrue(any("optional Spatial validation" in error for error in errors))

    def test_ci_plan_requires_committed_api_catalog_check(self):
        errors = self.workflow_errors_with_replacement(
            "ci.yml", "          python3 tools/site/generate_api_reference.py --check\n", "")
        self.assertTrue(
            any("missing orchestration contract" in error for error in errors),
            "ci.yml accepted a stale committed API catalog",
        )

    def test_site_cache_keys_follow_the_qt_version(self):
        for key in ("qt${{ env.QT_VERSION }}-v1-", "${{ runner.arch }}-${{ env.QT_VERSION }}"):
            with self.subTest(key=key):
                errors = self.workflow_errors_with_replacement(
                    "ci-site.yml", key, key.replace("${{ env.QT_VERSION }}", "6.9.3"))
                self.assertTrue(any("env.QT_VERSION" in error for error in errors))

    def test_catalog_generation_cannot_be_removed_from_plan_or_site(self):
        for workflow, command in (("ci.yml", "python3 tools/site/build_site.py --prepare"),
                                   ("ci-site.yml", "python tools/site/build_site.py --prepare")):
            with self.subTest(workflow=workflow):
                errors = self.workflow_errors_with_replacement(workflow, command, "echo skipped")
                self.assertTrue(any("generate catalog inputs" in error for error in errors))

    def test_capture_environment_is_shared_with_cache_lookup(self):
        errors = self.workflow_errors_with_replacement(
            "ci-site.yml", '      QT_SCALE_FACTOR: "2"\n', '')
        self.assertTrue(any("share the capture environment" in error for error in errors))

    def test_site_generation_and_tests_cannot_be_skipped(self):
        for command in (
            'python tools/site/build_site.py --output build/site-pages --commit "$GITHUB_SHA"',
            'python tools/site/test_generate_api_reference.py',
        ):
            with self.subTest(command=command):
                errors = self.workflow_errors_with_replacement("ci-site.yml", command, "echo skipped")
                self.assertTrue(any("must generate and validate" in error for error in errors))

    def test_site_failure_must_fail_the_ci_gate(self):
        errors = self.workflow_errors_with_replacement(
            "ci.yml", 'if [[ "$SITE_RESULT" != "success" ]]; then', 'if false; then')
        self.assertTrue(any("CI Gate script" in error for error in errors))

    def test_site_receipt_must_be_checked_before_deployment(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            'run: python3 tools/site/build_site.py --verify build/site-pages --commit "$EXPECTED_COMMIT"',
            'run: echo skipped')
        self.assertTrue(any("integrity and source commit" in error for error in errors))

    def test_pages_must_use_generated_site_instead_of_committed_snapshot(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml", 'cp -R build/site-pages/. build/pages/', 'cp -R site/. build/pages/')
        self.assertTrue(any("site assembly" in error for error in errors))

    def test_third_party_actions_require_immutable_revisions(self):
        cases = (
            (
                "pypa/gh-action-pypi-publish@" + "a" * 40 + " # release/v1",
                "pypa/gh-action-pypi-publish@release/v1",
            ),
            (
                "jurplel/install-qt-action@" + "b" * 40 + " # v999.0.0",
                "jurplel/install-qt-action@v4",
            ),
        )
        for pinned, mutable in cases:
            with self.subTest(action=mutable):
                prefix = "      - uses: "
                self.assertEqual(
                    MODULE.immutable_action_errors("fixture.yml", prefix + pinned), []
                )
                errors = MODULE.immutable_action_errors(
                    "fixture.yml", prefix + mutable
                )
                self.assertTrue(
                    any("must pin" in error for error in errors),
                    f"workflow validator accepted mutable action {mutable}",
                )
                quoted_errors = MODULE.immutable_action_errors(
                    "fixture.yml", prefix + f'"{mutable}"'
                )
                self.assertTrue(
                    any("must pin" in error for error in quoted_errors),
                    f"workflow validator accepted quoted mutable action {mutable}",
                )

    def test_pages_pipeline_actions_require_immutable_revisions(self):
        for workflow_name, actions in MODULE.PAGES_PIPELINE_ACTIONS.items():
            workflow = MODULE.read_workflow(workflow_name)
            for action in actions:
                with self.subTest(workflow=workflow_name, action=action):
                    pinned = re.search(
                        rf"{re.escape(action)}@[0-9a-fA-F]{{40}}", workflow
                    ).group(0)
                    mutable = f"{action}@v0"
                    self.assertIn(pinned, workflow)
                    errors = MODULE.required_action_errors(
                        workflow_name, workflow.replace(pinned, mutable, 1)
                    )
                    self.assertTrue(
                        any("must pin" in error for error in errors),
                        f"workflow validator accepted mutable action {action}",
                    )

    @staticmethod
    def updated_action_pins(contents: str) -> str:
        # Model Dependabot changing only YAML references and version comments.
        return re.sub(
            r"(uses: [^\s@]+/[^\s@]+)@[0-9a-fA-F]{40}(?:[ \t]+#[^\n]*)?",
            lambda match: match.group(1) + "@" + "f" * 40 + " # v999.0.0",
            contents,
        )

    def test_dependabot_updates_do_not_require_python_version_changes(self):
        original_read = Path.read_text
        updated_paths: set[Path] = set()

        def read_updated(path: Path, *args, **kwargs) -> str:
            contents = original_read(path, *args, **kwargs)
            if path.parent == MODULE.WORKFLOWS:
                updated = self.updated_action_pins(contents)
                if updated != contents:
                    updated_paths.add(path)
                return updated
            return contents

        with mock.patch.object(Path, "read_text", read_updated):
            self.assertEqual(MODULE.validate_boundaries(), [])
            self.assertEqual(
                MODULE.validate_cpp_plan_contract(MODULE.read_workflow("ci-cpp.yml")), []
            )
        self.assertIn(MODULE.WORKFLOWS / "pages.yml", updated_paths)
        self.assertIn(MODULE.WORKFLOWS / "ci-wasm.yml", updated_paths)
        self.assertIn(MODULE.WORKFLOWS / "ci-python.yml", updated_paths)

    def test_pages_pipeline_keeps_action_invocation_counts(self):
        for workflow_name, actions in MODULE.PAGES_PIPELINE_ACTIONS.items():
            workflow = MODULE.read_workflow(workflow_name)
            for action in actions:
                action_line = re.search(
                    rf"^.*uses: {re.escape(action)}@[^\n]*\n", workflow, re.MULTILINE
                ).group(0)
                for replacement in ("", action_line + action_line):
                    with self.subTest(workflow=workflow_name, action=action, replacement=replacement):
                        errors = MODULE.required_action_errors(
                            workflow_name, workflow.replace(action_line, replacement, 1)
                        )
                        self.assertTrue(any("exactly" in error for error in errors), errors)

    def test_step_contracts_reject_mutable_or_missing_action_revisions(self):
        cases = (
            ("pages.yml", "actions/deploy-pages", MODULE.pages_deployment_boundary_errors),
            ("ci-wasm.yml", "actions/cache", MODULE.wasm_supply_chain_errors),
            ("ci-cpp.yml", "actions/checkout", MODULE.validate_cpp_plan_contract),
        )
        for workflow_name, action, validate in cases:
            workflow = MODULE.read_workflow(workflow_name)
            for revision in ("@v999", "@abcdef1", "@<commit>", ""):
                with self.subTest(workflow=workflow_name, revision=revision):
                    mutated = re.sub(
                        rf"{re.escape(action)}@[0-9a-fA-F]{{40}}",
                        action + revision, workflow, count=1,
                    )
                    errors = validate(mutated)
                    self.assertTrue(any("must pin" in error for error in errors), errors)

    def test_updated_pins_still_require_expected_actions_and_inputs(self):
        cases = (
            ("pages.yml", "actions/deploy-pages@", "example/deploy-pages@", "unaudited remote action"),
            ("pages.yml", "persist-credentials: false", "persist-credentials: true", "deploy checkout"),
            ("pages.yml", "run-id: ${{ github.run_id }}", "run-id: 123", "artifact download"),
            ("ci-wasm.yml", "actions/cache@", "example/cache@", "unaudited remote action"),
            ("ci-wasm.yml", "path: ${{ runner.temp }}/emsdk", "path: /tmp/other", "exactly cache emsdk"),
            ("ci-cpp.yml", "actions/checkout@", "example/checkout@", "checkout step"),
            ("ci-cpp.yml", "fetch-depth: 2", "fetch-depth: 1", "checkout step"),
        )
        original_read = MODULE.read_workflow
        for workflow_name, original, replacement, expected_error in cases:
            with self.subTest(workflow=workflow_name, replacement=replacement):
                def with_change(name: str) -> str:
                    contents = self.updated_action_pins(original_read(name))
                    if name == workflow_name:
                        self.assertIn(original, contents)
                        contents = contents.replace(original, replacement, 1)
                    return contents

                with mock.patch.object(MODULE, "read_workflow", side_effect=with_change):
                    errors = MODULE.validate_boundaries()
                self.assertTrue(any(expected_error in error for error in errors), errors)

    def test_all_remote_actions_require_full_immutable_references(self):
        for ref in (
            "actions/checkout@v6", "example/action@main", "example/action@abcdef1",
            "docker://alpine:latest", "example/action", "example/action@<commit>", "",
        ):
            with self.subTest(ref=ref):
                for value in (ref, f'"{ref}"', f"'{ref}'"):
                    self.assertTrue(
                        MODULE.immutable_action_errors("fixture.yml", f"  - uses: {value}\n")
                    )
        for ref in (
            "./.github/workflows/local.yml", "./.github/actions/build",
            "example/action@" + "a" * 40,
            "example/repo/.github/workflows/build.yml@" + "b" * 40,
            "docker://alpine@sha256:" + "c" * 64,
        ):
            with self.subTest(ref=ref):
                self.assertEqual(
                    MODULE.immutable_action_errors("fixture.yml", f"  - uses: {ref} # version\n"),
                    [],
                )

    def test_pages_pipeline_rejects_an_unaudited_remote_action(self):
        pages = MODULE.read_workflow("pages.yml")
        errors = MODULE.required_action_errors(
            "pages.yml",
            pages.replace(
                "    steps:\n",
                "    steps:\n      - uses: example/unreviewed-action@" + "a" * 40 + "\n",
                1,
            ),
        )
        self.assertTrue(
            any("unaudited remote action" in error for error in errors),
            "workflow validator accepted a new remote action in the Pages chain",
        )

    def test_pages_authorization_requires_main(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            '          if [[ "$DEPLOY_REF" != "refs/heads/main" ]]; then\n',
            '          if [[ -z "$DEPLOY_REF" ]]; then\n',
        )
        self.assertTrue(
            any("source authorization" in error for error in errors),
            "workflow validator accepted Pages deployment from a non-main ref",
        )

    def test_pages_authorization_rejects_unlisted_events(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "            push|workflow_dispatch) ;;\n",
            "            *) ;;\n",
        )
        self.assertTrue(
            any("source authorization" in error for error in errors),
            "workflow validator accepted every Pages caller event",
        )

    def test_pages_header_rejects_quoted_direct_push_trigger(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "  workflow_dispatch:\n",
            "  workflow_dispatch:\n"
            '  "push":\n',
        )
        self.assertTrue(
            any("workflow header" in error for error in errors),
            "workflow validator accepted a quoted direct Pages push trigger",
        )

    def test_pages_header_rejects_write_permissions(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "permissions:\n  contents: read\n",
            "permissions:\n  actions: write\n  contents: write\n",
        )
        self.assertTrue(
            any("workflow header" in error for error in errors),
            "workflow validator accepted write permissions for every Pages job",
        )

    def test_pages_authorize_job_cannot_gain_job_level_permissions(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "    timeout-minutes: 5\n",
            "    timeout-minutes: 5\n"
            "    permissions: write-all\n",
        )
        self.assertTrue(
            any("authorize job" in error for error in errors),
            "workflow validator accepted a privileged authorization job",
        )

    def test_pages_deploy_must_depend_on_authorization(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "    needs: [authorize, wasm, site]\n",
            "    needs: wasm\n",
        )
        self.assertTrue(
            any("job-level structure" in error for error in errors),
            "workflow validator accepted deployment without source authorization",
        )

    def test_pages_deploy_needs_cannot_hide_in_the_job_name(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "    name: Deploy GitHub Pages\n"
            "    needs: [authorize, wasm, site]\n",
            "    name: Deploy GitHub Pages / needs: [authorize, wasm, site]\n"
            "    needs: wasm\n",
        )
        self.assertTrue(
            any("job-level structure" in error for error in errors),
            "workflow validator accepted deploy dependencies hidden in the job name",
        )

    def test_pages_manual_wasm_cannot_bypass_authorization_with_a_comment(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "    needs: authorize\n",
            "    needs: []\n"
            "    # needs: authorize\n",
        )
        self.assertTrue(
            any("manual WASM recovery job" in error for error in errors),
            "workflow validator accepted a comment decoy for WASM authorization",
        )

    def test_pages_manual_wasm_cannot_run_on_every_event_with_a_comment(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "    if: ${{ github.event_name == 'workflow_dispatch' }}\n",
            "    if: ${{ true }}\n"
            "    # if: ${{ github.event_name == 'workflow_dispatch' }}\n",
        )
        self.assertTrue(
            any("manual WASM recovery job" in error for error in errors),
            "workflow validator accepted a comment decoy for the manual WASM event",
        )

    def test_pages_deploy_cannot_accept_every_non_dispatch_event(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "         (github.event_name == 'push' && needs.wasm.result == 'skipped' && needs.site.result == 'skipped'))\n",
            "         (github.event_name != 'workflow_dispatch' && needs.wasm.result == 'skipped' && needs.site.result == 'skipped'))\n",
        )
        self.assertTrue(
            any("deployment condition" in error for error in errors),
            "workflow validator accepted an open-ended Pages caller event",
        )

    def test_pages_deploy_condition_cannot_be_made_unconditionally_true(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "         (github.event_name == 'push' && needs.wasm.result == 'skipped' && needs.site.result == 'skipped'))\n",
            "         (github.event_name == 'push' && needs.wasm.result == 'skipped' && needs.site.result == 'skipped')) || true\n",
        )
        self.assertTrue(
            any("deployment condition" in error for error in errors),
            "workflow validator accepted an additive Pages condition bypass",
        )

    def test_pages_checkout_cannot_use_a_comment_as_commit_provenance(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "          ref: ${{ github.sha }}\n",
            "          ref: refs/heads/main\n"
            "          # ref: ${{ github.sha }}\n",
        )
        self.assertTrue(
            any("deploy checkout" in error for error in errors),
            "workflow validator accepted a comment decoy for the checkout commit",
        )

    def test_pages_download_cannot_override_the_current_run(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "          run-id: ${{ github.run_id }}\n",
            "          run-id: ${{ github.run_id }}\n"
            "          run-id: 1\n",
        )
        self.assertTrue(
            any("artifact download" in error for error in errors),
            "workflow validator accepted a duplicate run-id override",
        )

    def test_pages_deploy_rejects_an_extra_post_provenance_tamper_step(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "      - name: Assemble Pages site\n",
            "      - name: Tamper with verified WebAssembly\n"
            "        run: echo changed > build/wasm-pages/index.html\n\n"
            "      - name: Assemble Pages site\n",
        )
        self.assertTrue(
            any("exact audited set" in error for error in errors),
            "workflow validator accepted an extra post-provenance tamper step",
        )

    def test_pages_deploy_rejects_provenance_after_deployment(self):
        original_read = MODULE.read_workflow
        pages = original_read("pages.yml")
        deploy = MODULE.job_section(pages, "deploy")
        provenance = MODULE.named_step_section(
            deploy, "Verify C++ Web Gallery provenance"
        )
        self.assertTrue(provenance)
        mutated_pages = pages.replace(provenance, "", 1)
        deploy_action = re.search(
            r"^        uses: actions/deploy-pages@[^\n]+\n", pages, re.MULTILINE
        ).group(0)
        mutated_pages = mutated_pages.replace(
            deploy_action,
            deploy_action + "\n" + provenance,
            1,
        )

        def with_reordered_provenance(name: str) -> str:
            return mutated_pages if name == "pages.yml" else original_read(name)

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=with_reordered_provenance
        ):
            errors = MODULE.validate_boundaries()
        self.assertTrue(
            any("provenance order" in error for error in errors),
            "workflow validator accepted provenance verification after deployment",
        )

    def test_pages_final_deploy_step_cannot_be_silently_disabled(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "        id: deployment\n",
            "        id: deployment\n"
            "        if: ${{ false }}\n",
        )
        self.assertTrue(
            any("final Deploy step" in error for error in errors),
            "workflow validator accepted a disabled final Pages deployment",
        )

    def test_pages_caller_requires_exact_read_and_deploy_permissions(self):
        errors = self.workflow_errors_with_replacement(
            "ci.yml",
            "    permissions:\n"
            "      actions: read\n"
            "      contents: read\n"
            "      pages: write\n"
            "      id-token: write\n"
            "    uses: ./.github/workflows/pages.yml\n",
            "    permissions:\n"
            "      pages: write\n"
            "      id-token: write\n"
            "    uses: ./.github/workflows/pages.yml\n",
        )
        self.assertTrue(
            any("automatic Pages caller" in error for error in errors),
            "workflow validator accepted a Pages caller without artifact read permissions",
        )

    def test_pages_caller_condition_cannot_hide_in_the_job_name(self):
        errors = self.workflow_errors_with_replacement(
            "ci.yml",
            "    name: Deploy validated WebAssembly Gallery\n"
            "    needs: [plan, wasm, site]\n"
            "    if: ${{ github.event_name == 'push' && needs.wasm.result == 'success' && needs.site.result == 'success' }}\n",
            "    name: Deploy validated WebAssembly Gallery / github.event_name == 'push' / needs.wasm.result == 'success'\n"
            "    needs: [plan, wasm, site]\n"
            "    if: ${{ true }}\n",
        )
        self.assertTrue(
            any("automatic Pages caller" in error for error in errors),
            "workflow validator accepted a caller condition hidden in its name",
        )

    def test_pages_artifact_commit_provenance_cannot_be_removed(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            '          if [[ "$actual_commit" != "$EXPECTED_COMMIT" ]]; then\n',
            '          if [[ -z "$actual_commit" ]]; then\n',
        )
        self.assertTrue(
            any("WASM artifact commit" in error for error in errors),
            "workflow validator accepted an artifact without commit provenance",
        )

    def test_pages_requires_a_full_validation_artifact(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            '          if [[ "$validation_mode" != "full" ]]; then\n',
            '          if [[ -z "$validation_mode" ]]; then\n',
        )
        self.assertTrue(
            any("full-validation provenance" in error for error in errors),
            "workflow validator accepted a non-full Pages artifact",
        )

    def test_pages_permissions_stay_on_the_deploy_job(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "permissions:\n  contents: read\n",
            "permissions:\n  contents: read\n  pages: write\n  id-token: write\n",
        )
        self.assertTrue(
            any("workflow header" in error for error in errors),
            "workflow validator accepted workflow-wide Pages write permissions",
        )

    def test_wasm_spatial_fallback_cannot_be_disabled_or_claim_hardware(self):
        for replacement in ("            --quality \\\n", "            --software-gl || true \\\n"):
            with self.subTest(replacement=replacement):
                errors = self.workflow_errors_with_replacement(
                    "ci-wasm.yml", "            --software-gl \\\n", replacement,
                )
                self.assertTrue(any("fail-closed Spatial software fallback" in error for error in errors))

    def test_wasm_spatial_failure_evidence_must_be_retained(self):
        errors = self.workflow_errors_with_replacement(
            "ci-wasm.yml",
            "      - name: Upload Spatial fallback evidence\n        if: ${{ always() }}\n",
            "      - name: Upload Spatial fallback evidence\n        if: ${{ success() }}\n",
        )
        self.assertTrue(any("retain Spatial fallback failure evidence" in error for error in errors))

    def test_wasm_browser_diagnostics_retain_failures_with_bounded_paths(self):
        for before, after in (
            (
                "      - name: Upload browser smoke diagnostics\n        if: ${{ always() }}\n",
                "      - name: Upload browser smoke diagnostics\n        if: ${{ success() }}\n",
            ),
            ("            build/wasm/command-palette-validation\n", "            .git\n"),
            ("            build/wasm/data-grid-validation\n", ""),
        ):
            with self.subTest(replacement=after):
                errors = self.workflow_errors_with_replacement("ci-wasm.yml", before, after)
                self.assertTrue(any("bounded browser failure diagnostics" in error for error in errors))

    def test_wasm_source_fetch_and_unit_checks_cannot_be_disabled_or_fail_open(self):
        commands = (
            "python tools/wasm/test_qt_accessibility_overlay.py",
            'python tools/wasm/qt_accessibility_overlay.py fetch --cache "$FLUENT_QT_WASM_SOURCE_CACHE"',
        )
        for command in commands:
            for replacement in ("# " + command, "echo " + command, command + " || true", ""):
                with self.subTest(command=command, replacement=replacement):
                    errors = self.workflow_errors_with_replacement("ci-wasm.yml", command, replacement)
                    self.assertTrue(any("explicitly verify and fetch pinned Qt sources" in error for error in errors))

    def test_wasm_source_cache_requires_manifest_identity_and_shared_path(self):
        for original, replacement, expected in (
            ("be09b211-${{ hashFiles('src/compatibility/private/wasm/qt-6.9.3/manifest.json') }}",
             "be09b211", "key the pinned Qt source cache"),
            ('FLUENT_QT_WASM_SOURCE_CACHE=$RUNNER_TEMP/fluentqt-qt-source-cache',
             'FLUENT_QT_WASM_SOURCE_CACHE=/tmp/other', "share its pinned Qt source cache"),
        ):
            with self.subTest(original=original):
                errors = self.workflow_errors_with_replacement("ci-wasm.yml", original, replacement)
                self.assertTrue(any(expected in error for error in errors))

    def test_wasm_emsdk_revision_cannot_drift(self):
        errors = self.workflow_errors_with_replacement(
            "ci-wasm.yml",
            MODULE.EMSDK_REPOSITORY_REVISION,
            "0" * 40,
        )
        self.assertTrue(
            any("emsdk revision" in error for error in errors),
            "workflow validator accepted an unaudited emsdk revision",
        )

    def test_wasm_cannot_clone_the_mutable_emsdk_default_branch(self):
        errors = self.workflow_errors_with_replacement(
            "ci-wasm.yml",
            '          git -C "$EMSDK_ROOT" fetch --depth 1 origin \\\n'
            '            "refs/tags/$EMSCRIPTEN_VERSION"\n',
            "          git clone --depth 1 https://github.com/emscripten-core/emsdk.git \\\n"
            '            "$EMSDK_ROOT"\n',
        )
        self.assertTrue(
            any("exactly install and activate" in error for error in errors),
            "workflow validator accepted the mutable emsdk default branch",
        )

    def test_wasm_cannot_change_refs_after_verifying_emsdk_revision(self):
        errors = self.workflow_errors_with_replacement(
            "ci-wasm.yml",
            '          git -C "$EMSDK_ROOT" checkout --detach "$EMSDK_REPOSITORY_REVISION"\n',
            '          git -C "$EMSDK_ROOT" checkout --detach "$EMSDK_REPOSITORY_REVISION"\n'
            '          git -C "$EMSDK_ROOT" fetch origin main\n'
            '          git -C "$EMSDK_ROOT" checkout --detach FETCH_HEAD\n',
        )
        self.assertTrue(
            any("exactly install and activate" in error for error in errors),
            "workflow validator accepted a mutable ref after emsdk verification",
        )

    def test_wasm_cannot_mutate_emsdk_in_a_later_step(self):
        errors = self.workflow_errors_with_replacement(
            "ci-wasm.yml",
            "      - name: Configure and build WebAssembly targets\n",
            "      - name: Replace verified Emscripten checkout\n"
            "        shell: bash\n"
            "        run: |\n"
            '          git -C "$EMSDK_ROOT" fetch origin main\n'
            '          git -C "$EMSDK_ROOT" checkout --detach FETCH_HEAD\n\n'
            "      - name: Configure and build WebAssembly targets\n",
        )
        self.assertTrue(
            any(
                "exact audited set" in error
                or "outside its audited install step" in error
                for error in errors
            ),
            "workflow validator accepted a cross-step mutable emsdk checkout",
        )

    def test_wasm_build_cannot_source_an_alternate_emsdk(self):
        errors = self.workflow_errors_with_replacement(
            "ci-wasm.yml",
            '          source "$EMSDK_ROOT/emsdk_env.sh"\n',
            '          source "/tmp/other-emsdk/emsdk_env.sh"\n',
        )
        self.assertTrue(
            any(
                "configure/build step" in error
                or "source only the audited emsdk" in error
                for error in errors
            ),
            "workflow validator accepted an alternate Emscripten environment",
        )

    def test_wasm_emsdk_cache_key_cannot_use_a_revision_comment_decoy(self):
        expected_key = (
            "          key: fluentqt-emsdk-${{ runner.os }}-${{ runner.arch }}-"
            "3.1.70-2514ec738de72cebbba7f4fdba0cf2fabcb779a5\n"
        )
        errors = self.workflow_errors_with_replacement(
            "ci-wasm.yml",
            expected_key,
            "          key: fluentqt-emsdk-${{ runner.os }}-${{ runner.arch }}-3.1.70\n"
            "          # 2514ec738de72cebbba7f4fdba0cf2fabcb779a5\n",
        )
        self.assertTrue(
            any("exactly cache emsdk" in error for error in errors),
            "workflow validator accepted a comment decoy for the emsdk cache revision",
        )

    def test_wasm_system_library_cache_key_requires_the_audited_revision(self):
        expected_key = (
            "          key: fluentqt-em-cache-${{ runner.os }}-${{ runner.arch }}-"
            "3.1.70-2514ec738de72cebbba7f4fdba0cf2fabcb779a5\n"
        )
        errors = self.workflow_errors_with_replacement(
            "ci-wasm.yml",
            expected_key,
            "          key: fluentqt-em-cache-${{ runner.os }}-${{ runner.arch }}-3.1.70\n"
            "          # 2514ec738de72cebbba7f4fdba0cf2fabcb779a5\n",
        )
        self.assertTrue(
            any("system libraries" in error for error in errors),
            "workflow validator accepted a stale Emscripten system-library cache key",
        )

    def test_release_inputs_cannot_be_interpolated_into_shell_source(self):
        release = MODULE.read_workflow("release.yml")
        errors = MODULE.release_input_boundary_errors(
            release.replace(
                '            tag="$RELEASE_TAG_INPUT"',
                '            tag="${{ inputs.tag }}"',
                1,
            )
        )
        self.assertTrue(
            any("must not interpolate workflow inputs" in error for error in errors),
            "workflow validator accepted a workflow input in shell source",
        )

    def test_release_inputs_require_explicit_environment_mappings(self):
        release = MODULE.read_workflow("release.yml")
        errors = MODULE.release_input_boundary_errors(
            release.replace(
                "          RELEASE_TAG_INPUT: ${{ inputs.tag }}\n", "", 1
            )
        )
        self.assertTrue(
            any("RELEASE_TAG_INPUT" in error for error in errors),
            "workflow validator accepted a missing release input mapping",
        )

    def test_cpp_test_selection_must_reach_reusable_workflow(self):
        original_read = MODULE.read_workflow

        def without_selected_targets(name: str) -> str:
            contents = original_read(name)
            if name != "ci.yml":
                return contents
            return contents.replace(
                "      cpp_test_targets: ${{ needs.plan.outputs.cpp_test_targets }}\n",
                "",
                1,
            )

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=without_selected_targets
        ):
            errors = MODULE.validate_boundaries()
        self.assertTrue(
            any("cpp_test_targets" in error for error in errors),
            "workflow validator accepted a disconnected C++ test selection",
        )

    def test_cpp_changed_format_check_cannot_be_noop(self):
        original_read = MODULE.read_workflow

        def with_noop_format_check(name: str) -> str:
            contents = original_read(name)
            if name != "ci-cpp.yml":
                return contents
            return contents.replace(
                "            python3 tools/quality/check_cpp_format.py --changed-from \"$PR_BASE_SHA\"\n",
                "            true || python3 tools/quality/check_cpp_format.py --changed-from \"$PR_BASE_SHA\"\n",
                1,
            )

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=with_noop_format_check
        ):
            errors = MODULE.validate_boundaries()
        self.assertTrue(
            any("changed-file format check" in error for error in errors),
            "workflow validator accepted a disabled C++ format check",
        )

    def test_cpp_formatter_installation_keeps_the_version_pin(self):
        for replacement in ("clang-format", "clang-format==15.0.0"):
            with self.subTest(replacement=replacement):
                errors = self.workflow_errors_with_replacement(
                    "ci-cpp.yml", "clang-format==22.1.8", replacement)
                self.assertTrue(any("pinned C++ formatter" in error for error in errors))

    def test_cpp_selected_matrix_must_build_selected_targets(self):
        original_read = MODULE.read_workflow

        def without_selected_build_targets(name: str) -> str:
            contents = original_read(name)
            if name != "ci-cpp.yml":
                return contents
            return contents.replace(
                '                  .build_targets = (\n',
                '                  .name = (\n',
                1,
            )

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=without_selected_build_targets
        ):
            errors = MODULE.validate_boundaries()
        self.assertTrue(
            any("matrix-selection script" in error for error in errors),
            "workflow validator accepted selected tests that were not built",
        )

    def test_duplicate_or_noncanonical_job_ids_are_rejected(self):
        original_read = MODULE.read_workflow
        shadow_keys = (
            "  ci-gate:",
            '  "ci-gate":',
            '  "ci\\x2dgate":',
            "  !!str ci-gate:",
            "  ? ci-gate\n  :",
        )
        shadow_body = (
            "\n    name: Shadow gate\n"
            "    continue-on-error: true\n"
            "    runs-on: ubuntu-latest\n"
            "    steps:\n"
            "      - run: exit 1\n"
        )
        for shadow_key in shadow_keys:
            with self.subTest(shadow_key=shadow_key):
                def with_shadow_job(name: str) -> str:
                    contents = original_read(name)
                    if name != "ci.yml":
                        return contents
                    return contents + "\n" + shadow_key + shadow_body

                with mock.patch.object(
                    MODULE, "read_workflow", side_effect=with_shadow_job
                ):
                    errors = MODULE.validate_boundaries()
                self.assertTrue(
                    any(
                        "canonical job id" in error
                        or "repeat job id" in error
                        for error in errors
                    ),
                    f"workflow validator accepted shadow job id: {shadow_key}",
                )

    def test_duplicate_or_noncanonical_jobs_roots_are_rejected(self):
        original_read = MODULE.read_workflow
        replacements = (
            '"jobs":',
            '"j\\x6fbs":',
            "!!str jobs:",
            "? jobs\n:",
        )
        for replacement in replacements:
            with self.subTest(replacement=replacement):
                def with_noncanonical_jobs_root(name: str) -> str:
                    contents = original_read(name)
                    if name != "ci.yml":
                        return contents
                    return contents.replace("\njobs:\n", f"\n{replacement}\n", 1)

                with mock.patch.object(
                    MODULE,
                    "read_workflow",
                    side_effect=with_noncanonical_jobs_root,
                ):
                    errors = MODULE.validate_boundaries()
                self.assertTrue(
                    any("top-level jobs" in error for error in errors),
                    f"workflow validator accepted jobs root: {replacement}",
                )

        def with_duplicate_jobs_root(name: str) -> str:
            contents = original_read(name)
            if name != "ci.yml":
                return contents
            return contents + (
                "\njobs:\n"
                "  ci-gate:\n"
                "    name: Shadow gate\n"
                "    continue-on-error: true\n"
                "    runs-on: ubuntu-latest\n"
                "    steps:\n"
                "      - run: exit 1\n"
            )

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=with_duplicate_jobs_root
        ):
            errors = MODULE.validate_boundaries()
        self.assertTrue(
            any("repeat workflow-level key: jobs" in error for error in errors),
            "workflow validator accepted a duplicate top-level jobs mapping",
        )

    def test_shell_comment_syntax_cannot_hide_a_second_jobs_mapping(self):
        original_read = MODULE.read_workflow

        def with_hidden_jobs_root(name: str) -> str:
            contents = original_read(name)
            if name != "ci.yml":
                return contents
            return contents + (
                "\n          <#\n"
                "jobs:\n"
                "  ci-gate:\n"
                "    name: Shadow gate\n"
                "    continue-on-error: true\n"
                "    runs-on: ubuntu-latest\n"
                "    steps:\n"
                "      - shell: bash\n"
                "        run: |\n"
                "          exit 1\n"
                "          #>\n"
            )

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=with_hidden_jobs_root
        ):
            errors = MODULE.validate_boundaries()
        self.assertTrue(
            any("repeat workflow-level key: jobs" in error for error in errors),
            "shell comment syntax hid a duplicate top-level jobs mapping",
        )

    def test_reusable_modules_reject_fail_open_job_controls(self):
        original_read = MODULE.read_workflow
        cases = (
            ("ci-wasm.yml", "build", "continue-on-error: true"),
            ("ci-wasm.yml", "build", "if: false"),
            ("ci-python.yml", "plan", "continue-on-error: true"),
            ("ci-python.yml", "plan", "if: false"),
            ("ci-python.yml", "pyside6_linux", "continue-on-error: true"),
            ("ci-python.yml", "pyside6_windows", '"continue-on-error": true'),
        )
        for workflow_name, job_id, control in cases:
            with self.subTest(
                workflow_name=workflow_name,
                job_id=job_id,
                control=control,
            ):
                def with_fail_open_job(name: str) -> str:
                    contents = original_read(name)
                    if name != workflow_name:
                        return contents
                    return contents.replace(
                        f"  {job_id}:\n",
                        f"  {job_id}:\n    {control}\n",
                        1,
                    )

                with mock.patch.object(
                    MODULE, "read_workflow", side_effect=with_fail_open_job
                ):
                    errors = MODULE.validate_boundaries()
                self.assertTrue(
                    errors,
                    f"workflow validator accepted {workflow_name} {job_id} {control}",
                )

    def test_pr_file_collection_is_counted_and_fail_closed(self):
        original_read = MODULE.read_workflow

        def without_expected_count(name: str) -> str:
            contents = original_read(name)
            return contents.replace(
                '--expected-count "$PR_CHANGED_FILES"',
                '# --expected-count "$PR_CHANGED_FILES"\n'
                "                --expected-count 1",
            )

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=without_expected_count
        ):
            errors = MODULE.validate_boundaries()
        self.assertTrue(
            any("$PR_CHANGED_FILES" in error for error in errors),
            "workflow validator accepted an untrusted fixed file count",
        )

    def test_compact_orchestration_ignores_blank_and_comment_lines(self):
        original_read = MODULE.read_workflow

        def with_inactive_padding(name: str) -> str:
            contents = original_read(name)
            if name != "ci.yml":
                return contents
            return contents + ("\n# maintenance note" * 32) + ("\n" * 32)

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=with_inactive_padding
        ):
            self.assertEqual(MODULE.validate_boundaries(), [])

    def test_compact_orchestration_rejects_active_bloat(self):
        original_read = MODULE.read_workflow

        def with_active_bloat(name: str) -> str:
            contents = original_read(name)
            if name != "ci.yml":
                return contents
            padding = "".join(
                f"\n          echo maintenance-{index}" for index in range(16)
            )
            return contents + padding

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=with_active_bloat
        ):
            errors = MODULE.validate_boundaries()
        self.assertTrue(
            any("compact orchestration-only workflow" in error for error in errors),
            "workflow validator accepted excessive active orchestration lines",
        )

    def test_pr_file_contract_rejects_active_echo_bait(self):
        original_read = MODULE.read_workflow

        def with_echo_bait(name: str) -> str:
            contents = original_read(name)
            if name != "ci.yml":
                return contents
            contents = contents.replace(
                '--expected-count "$PR_CHANGED_FILES" \\\n',
                '--expected-count "$(jq \'add | length\' <<< "$changed_files_json")" \\\n',
                1,
            )
            return contents.replace(
                "          for value in \"$should_build\"",
                '          echo \'--expected-count "$PR_CHANGED_FILES"\'\n'
                "          for value in \"$should_build\"",
                1,
            )

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=with_echo_bait
        ):
            errors = MODULE.validate_boundaries()
        self.assertTrue(
            any("classification script must exactly" in error for error in errors),
            "workflow validator accepted an echo decoy for the trusted PR count",
        )

    def test_pr_file_contract_rejects_noop_classifier_decoy(self):
        original_read = MODULE.read_workflow

        def with_noop_decoy(name: str) -> str:
            contents = original_read(name)
            if name != "ci.yml":
                return contents
            return contents.replace(
                "              python3 .github/scripts/classify_ci_changes.py",
                "              true || python3 .github/scripts/classify_ci_changes.py",
                1,
            )

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=with_noop_decoy
        ):
            errors = MODULE.validate_boundaries()
        self.assertTrue(
            any("classification script must exactly" in error for error in errors),
            "workflow validator accepted a no-op classifier invocation",
        )

    def test_plan_job_cannot_continue_on_error_or_be_disabled(self):
        original_read = MODULE.read_workflow
        for control in (
            "continue-on-error: true",
            '"continue-on-error": true',
            "if: ${{ false }}",
            '"if": ${{ false }}',
            "if : ${{ false }}",
        ):
            with self.subTest(control=control):
                def with_fail_open_plan(name: str) -> str:
                    contents = original_read(name)
                    if name != "ci.yml":
                        return contents
                    return contents.replace(
                        "  plan:\n",
                        f"  plan:\n    {control}\n",
                        1,
                    )

                with mock.patch.object(
                    MODULE, "read_workflow", side_effect=with_fail_open_plan
                ):
                    errors = MODULE.validate_boundaries()
                self.assertTrue(
                    any("ci.yml plan job must" in error for error in errors),
                    f"workflow validator accepted plan control: {control}",
                )

    def test_ci_gate_job_cannot_continue_on_error(self):
        original_read = MODULE.read_workflow

        def with_fail_open_gate(name: str) -> str:
            contents = original_read(name)
            if name != "ci.yml":
                return contents
            return contents.replace(
                "  ci-gate:\n",
                "  ci-gate:\n    continue-on-error: true\n",
                1,
            )

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=with_fail_open_gate
        ):
            errors = MODULE.validate_boundaries()
        self.assertTrue(
            any("ci-gate job must not continue" in error for error in errors),
            "workflow validator accepted a fail-open CI Gate",
        )

    def test_ci_gate_requires_boolean_classification_outputs(self):
        original_read = MODULE.read_workflow
        guard = (
            '          for value in "$SHOULD_BUILD" "$SHOULD_BUILD_PYSIDE" '
            '"$SHOULD_BUILD_WASM"; do\n'
            '            [[ "$value" == "true" || "$value" == "false" ]] || {\n'
            '              echo "::error::Invalid or missing CI classification output: $value"\n'
            "              exit 1\n"
            "            }\n"
            "          done\n"
        )

        def without_output_guard(name: str) -> str:
            contents = original_read(name)
            if name != "ci.yml":
                return contents
            self.assertIn(guard, contents)
            return contents.replace(guard, "", 1)

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=without_output_guard
        ):
            errors = MODULE.validate_boundaries()
        self.assertTrue(
            any("invalid or missing classification" in error for error in errors),
            "workflow validator accepted empty classification outputs",
        )

    def test_cpp_plan_cannot_continue_on_error_or_be_disabled(self):
        for control in (
            "continue-on-error: true",
            '"continue-on-error": true',
            "if: ${{ false }}",
            '"if": ${{ false }}',
            "if : ${{ false }}",
        ):
            with self.subTest(control=control):
                self.assert_plan_contract_error(
                    self.workflow.replace(
                        "  plan:\n",
                        f"  plan:\n    {control}\n",
                        1,
                    )
                )

    def test_job_body_cannot_shift_its_root_indentation(self):
        original_read = MODULE.read_workflow

        def with_shifted_gate(name: str) -> str:
            contents = original_read(name)
            if name != "ci.yml":
                return contents
            return self.reindent_job(
                contents, "ci-gate", "continue-on-error: true"
            )

        with mock.patch.object(
            MODULE, "read_workflow", side_effect=with_shifted_gate
        ):
            errors = MODULE.validate_boundaries()
        self.assertTrue(
            any("four spaces of indentation" in error for error in errors),
            "workflow validator accepted a five-space ci-gate body",
        )

        self.assert_contract_error(
            self.reindent_job(
                self.workflow, "build", "continue-on-error: true"
            )
        )

    def test_cpp_integration_cannot_be_disabled_or_made_fail_open(self):
        for control in (
            "continue-on-error: true",
            '"continue-on-error": true',
            "if: ${{ false }}",
            '"if": ${{ false }}',
            "!!str if: false",
        ):
            with self.subTest(control=control):
                self.assert_integration_contract_error(
                    self.workflow.replace(
                        "  integration:\n",
                        f"  integration:\n    {control}\n",
                        1,
                    )
                )

    def test_cpp_plan_quality_validators_cannot_be_removed_or_bypassed(self):
        commands = (
            "python3 tools/quality/validate_component_api.py --project-root . --self-test",
            "python3 tools/quality/validate_visual_evidence_inventory.py --project-root . --self-test",
        )
        for command in commands:
            for replacement in ("", f"true || {command}"):
                with self.subTest(command=command, replacement=replacement):
                    self.assert_plan_contract_error(
                        self.workflow.replace(command, replacement, 1)
                    )

    def test_cpp_plan_rejects_standalone_matrix_validator_in_any_step(self):
        command = "python3 .github/scripts/validate-ci-cpp-matrix.py"
        mutations = (
            self.workflow.replace(
                "      - name: Select C++ validation matrix\n",
                "      - name: Redundant matrix validation\n"
                f"        run: {command}\n\n"
                "      - name: Select C++ validation matrix\n",
                1,
            ),
            self.workflow.replace(
                "        run: |\n          set -euo pipefail\n",
                f"        run: |\n          {command}\n          set -euo pipefail\n",
                1,
            ),
            self.workflow.replace(
                "      - name: Select C++ validation matrix\n",
                "      - name: Redundant matrix validation\n"
                "        run: python3  .github/scripts/validate-ci-cpp-matrix.py\n\n"
                "      - name: Select C++ validation matrix\n",
                1,
            ),
            self.workflow.replace(
                "        run: |\n          set -euo pipefail\n",
                "        run: |\n"
                "          python3 ./.github/scripts/validate-ci-cpp-matrix.py\n"
                "          set -euo pipefail\n",
                1,
            ),
            self.workflow.replace(
                "      - name: Select C++ validation matrix\n",
                "      - name: Redundant matrix validation\n"
                '        run: python3 "$PWD/.github/scripts/validate-ci-cpp-matrix.py"\n\n'
                "      - name: Select C++ validation matrix\n",
                1,
            ),
            self.workflow.replace(
                "      - name: Select C++ validation matrix\n",
                "      - name: Redundant matrix validation\n"
                '        run: python3 .github/scripts/validate-ci-cpp-"matrix.py"\n\n'
                "      - name: Select C++ validation matrix\n",
                1,
            ),
            self.workflow.replace(
                "      - name: Select C++ validation matrix\n",
                "      - name: Redundant matrix validation\n"
                "        run: python3 .github/scripts/validate-ci-cpp-ma\\trix.py\n\n"
                "      - name: Select C++ validation matrix\n",
                1,
            ),
        )
        for workflow in mutations:
            with self.subTest(workflow=workflow):
                self.assert_plan_contract_error(workflow)

    def test_noncanonical_job_level_control_syntax_is_rejected(self):
        original_read = MODULE.read_workflow
        cases = (
            ("ci.yml", "plan", '    "\\x69\\x66": ${{ false }}'),
            ("ci.yml", "ci-gate", "    ? continue-on-error\n    : true"),
            ("ci-cpp.yml", "build", "    !!str continue-on-error: true"),
            ("ci-cpp.yml", "plan", "    <<: {if: false}"),
        )
        for workflow_name, job_id, entry in cases:
            with self.subTest(
                workflow_name=workflow_name, job_id=job_id, entry=entry
            ):
                def with_noncanonical_control(name: str) -> str:
                    contents = original_read(name)
                    if name != workflow_name:
                        return contents
                    return contents.replace(
                        f"  {job_id}:\n",
                        f"  {job_id}:\n{entry}\n",
                        1,
                    )

                with mock.patch.object(
                    MODULE,
                    "read_workflow",
                    side_effect=with_noncanonical_control,
                ):
                    errors = MODULE.validate_boundaries()
                self.assertTrue(
                    any("plain job-level key" in error for error in errors),
                    f"workflow validator accepted noncanonical control: {entry}",
                )

    def test_test_step_cannot_be_disabled(self):
        self.assert_contract_error(
            self.workflow.replace(
                "if: ${{ matrix.build == true && matrix.test == true }}",
                "if: ${{ false }}",
                1,
            )
        )

    def test_ctest_invocation_is_required(self):
        self.assert_contract_error(
            self.workflow.replace("          ctest @testArgs\n", "", 1)
        )

    def test_spatial_failure_evidence_stays_in_selected_build_tree(self):
        self.assert_contract_error(
            self.workflow.replace(
                "FLUENT_QT_SPATIAL_EVIDENCE: ${{ github.workspace }}/build/${{ matrix.preset }}/Testing/SpatialEvidence",
                "FLUENT_QT_SPATIAL_EVIDENCE: ${{ github.workspace }}/evidence",
                1,
            )
        )

    def test_commented_ctest_invocation_cannot_satisfy_contract(self):
        self.assert_contract_error(
            self.workflow.replace(
                "          ctest @testArgs\n",
                '          Write-Host "skip" # ctest @testArgs\n',
                1,
            )
        )

    def test_block_commented_fake_test_step_cannot_satisfy_contract(self):
        disabled = self.workflow.replace("      - name: Test\n", "      - name: Disabled Test\n", 1)
        fake = """
          <#
      - name: Test
        if: ${{ matrix.build == true && matrix.test == true }}
        shell: pwsh
        env:
          QT_QPA_PLATFORM: ${{ matrix.qt_qpa_platform || 'offscreen' }}
          SKIP_VISUAL_TEST: 1
          ASAN_OPTIONS: ${{ matrix.asan_options || '' }}
          UBSAN_OPTIONS: ${{ matrix.ubsan_options || '' }}
        run: |
          ctest @testArgs
          #>
"""
        self.assert_contract_error(disabled + fake)

    def test_commented_original_condition_cannot_satisfy_contract(self):
        self.assert_contract_error(
            self.workflow.replace(
                "if: ${{ matrix.build == true && matrix.test == true }}",
                "if: ${{ false }} # if: ${{ matrix.build == true && matrix.test == true }}",
                1,
            )
        )

    def test_matrix_include_and_exclude_labels_are_required(self):
        for fragment in (
            '$testLabels = "${{ matrix.test_labels }}"',
            '$excludeLabels = "${{ matrix.exclude_labels }}"',
            '$testArgs += @("-L", $testLabels)',
            '$testArgs += @("-LE", $excludeLabels)',
        ):
            with self.subTest(fragment=fragment):
                self.assert_contract_error(
                    self.workflow.replace(fragment, "# removed", 1)
                )

    def test_no_tests_error_guard_is_required(self):
        self.assert_contract_error(
            self.workflow.replace('            "--no-tests=error"\n', "", 1)
        )

    def test_test_step_cannot_continue_on_error(self):
        self.assert_contract_error(
            self.workflow.replace(
                "      - name: Test\n"
                "        if: ${{ matrix.build == true && matrix.test == true }}\n"
                "        shell: pwsh\n"
                "        env:\n",
                "      - name: Test\n"
                "        if: ${{ matrix.build == true && matrix.test == true }}\n"
                "        shell: pwsh\n"
                "        continue-on-error: true\n"
                "        env:\n",
                1,
            )
        )

    def test_build_job_cannot_be_disabled(self):
        self.assert_contract_error(
            self.workflow.replace(
                "  build:\n",
                "  build:\n    if: ${{ false }}\n",
                1,
            )
        )

    def test_build_job_cannot_continue_on_error(self):
        self.assert_contract_error(
            self.workflow.replace(
                "  build:\n",
                "  build:\n    continue-on-error: true\n",
                1,
            )
        )

    def test_ctest_cannot_be_hidden_in_false_condition(self):
        self.assert_contract_error(
            self.workflow.replace(
                "          ctest @testArgs\n",
                "          if ($false) {\n"
                "            ctest @testArgs\n"
                "          }\n",
                1,
            )
        )

    def test_ctest_cannot_be_hidden_in_unused_function(self):
        self.assert_contract_error(
            self.workflow.replace(
                "          ctest @testArgs\n",
                "          function Invoke-Tests {\n"
                "            ctest @testArgs\n"
                "          }\n",
                1,
            )
        )

    def test_early_exit_or_return_cannot_skip_ctest(self):
        for statement in ("exit 0", "return"):
            with self.subTest(statement=statement):
                self.assert_contract_error(
                    self.workflow.replace(
                        "          ctest @testArgs\n",
                        f"          {statement}\n          ctest @testArgs\n",
                        1,
                    )
                )


class CiSourceContractsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location(
            "check_ci_source_contracts", Path(__file__).with_name("check-ci-source-contracts.py")
        )
        cls.source = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.source)

    def test_repository_source_checks_are_unconditional(self):
        self.assertEqual(MODULE.validate_ci_source_contracts_contract(MODULE.read_workflow("ci.yml")), [])

    def test_legacy_preflight_alone_cannot_omit_font_checks(self):
        workflow = MODULE.read_workflow("ci.yml").replace(
            "python3 .github/scripts/check-ci-source-contracts.py",
            "python3 tools/dev/fluent_qt_preflight.py --checks-only",
            1,
        )
        self.assertTrue(MODULE.validate_ci_source_contracts_contract(workflow))

    def test_source_checks_cannot_be_commented_disabled_or_fail_open(self):
        workflow = MODULE.read_workflow("ci.yml")
        command = "python3 .github/scripts/check-ci-source-contracts.py"
        for replacement in ("", "# " + command, "echo " + command,
                            "true || " + command, command + " || true", "exit 0\n          " + command):
            with self.subTest(replacement=replacement):
                self.assertTrue(MODULE.validate_ci_source_contracts_contract(workflow.replace(command, replacement, 1)))
        step = MODULE.named_step_section(MODULE.job_section(workflow, "plan"), "Validate CI orchestration contracts")
        for control in ("if: ${{ false }}", "continue-on-error: true"):
            for position in ("        shell: bash\n", "          node --test"):
                with self.subTest(control=control, position=position):
                    mutated = step.replace(position, "        " + control + "\n" + position, 1)
                    self.assertTrue(MODULE.validate_ci_source_contracts_contract(workflow.replace(step, mutated, 1)))

    def test_font_checks_use_an_isolated_interpreter_and_execute_all_contracts(self):
        directory = MODULE.ROOT / "build/font-check-unit-fixture"
        python = directory / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
        with mock.patch.dict(os.environ, {"PYTHONPATH": "external", "PYTHONHOME": "external"}), \
                mock.patch.object(self.source.subprocess, "run") as run:
            self.source.run_font_contracts(directory)
        self.assertEqual([call.args[0] for call in run.call_args_list], [
            [sys.executable, "-m", "venv", str(directory)],
            [str(python), "-m", "pip", "install", "--disable-pip-version-check", "-r", "tools/fonts/requirements.txt"],
            [str(python), "tools/fonts/test_generate_typography_assets.py"],
            [str(python), "tools/fonts/generate_typography_assets.py", "--check"],
            [str(python), "tools/fonts/generate_typography_assets.py", "--check-web-fallback"],
        ])
        for call in run.call_args_list:
            self.assertTrue(call.kwargs["check"])
            self.assertEqual(call.kwargs["cwd"], MODULE.ROOT)
            self.assertNotIn("PYTHONPATH", call.kwargs["env"])
            self.assertNotIn("PYTHONHOME", call.kwargs["env"])

    def test_each_font_failure_stops_before_later_checks(self):
        for index in range(5):
            with self.subTest(failed_command=index), mock.patch.object(
                self.source.subprocess, "run",
                side_effect=[mock.Mock(returncode=0)] * index + [subprocess.CalledProcessError(7, "font check")],
            ) as run:
                with self.assertRaises(subprocess.CalledProcessError):
                    self.source.run_font_contracts(MODULE.ROOT / "build/font-check-unit-fixture")
                self.assertEqual(run.call_count, index + 1)

    def test_original_preflight_still_runs_after_font_checks(self):
        calls = mock.Mock()
        with mock.patch.object(self.source, "run_font_contracts") as fonts, \
                mock.patch.object(self.source.subprocess, "run") as preflight:
            calls.attach_mock(fonts, "fonts")
            calls.attach_mock(preflight, "preflight")
            self.assertEqual(self.source.main(), 0)
        self.assertEqual(calls.mock_calls, [
            mock.call.fonts(MODULE.ROOT / "build/ci-fonts-venv"),
            mock.call.preflight([sys.executable, "tools/dev/fluent_qt_preflight.py", "--checks-only"],
                                cwd=MODULE.ROOT, check=True),
        ])

    def test_font_and_existing_preflight_failures_propagate(self):
        with mock.patch.object(self.source, "run_font_contracts", side_effect=subprocess.CalledProcessError(17, "fonts")), \
                mock.patch.object(self.source.subprocess, "run") as preflight:
            self.assertEqual(self.source.main(), 17)
            preflight.assert_not_called()
        with mock.patch.object(self.source, "run_font_contracts"), mock.patch.object(
            self.source.subprocess, "run", side_effect=subprocess.CalledProcessError(23, "preflight")
        ):
            self.assertEqual(self.source.main(), 23)

    def test_font_dependency_is_exactly_pinned_to_the_generator_version(self):
        generator = (MODULE.ROOT / "tools/fonts/generate_typography_assets.py").read_text(encoding="utf-8")
        version = re.search(r'^FONTTOOLS_VERSION = "([^"]+)"$', generator, re.MULTILINE).group(1)
        requirements = (MODULE.ROOT / "tools/fonts/requirements.txt").read_text(encoding="utf-8")
        entries = [line.strip() for line in requirements.splitlines() if line.strip() and not line.startswith("#")]
        self.assertEqual(entries, [f"fonttools=={version}"])


if __name__ == "__main__":
    unittest.main()
