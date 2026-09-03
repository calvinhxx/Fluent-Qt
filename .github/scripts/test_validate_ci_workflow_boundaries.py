#!/usr/bin/env python3

import importlib.util
from pathlib import Path
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

    def test_repository_workflow_boundaries_are_valid(self):
        self.assertEqual(MODULE.validate_boundaries(), [])

    def test_ci_plan_requires_generated_site_freshness_checks(self):
        commands = (
            "          python3 tools/site/generate_localized_site.py --check\n",
            "          python3 tools/site/generate_api_reference.py --check\n",
        )
        for command in commands:
            with self.subTest(command=command.strip()):
                errors = self.workflow_errors_with_replacement("ci.yml", command, "")
                self.assertTrue(
                    any("missing orchestration contract" in error for error in errors),
                    "ci.yml accepted a missing generated-site freshness check",
                )

    def test_audited_third_party_actions_require_immutable_revisions(self):
        cases = (
            (
                "pypa/gh-action-pypi-publish@dc37677b2e1c63e2034f94d8a5b11f265b73ba33 # release/v1",
                "pypa/gh-action-pypi-publish@release/v1",
            ),
            (
                "jurplel/install-qt-action@48d3ad6db93f3627c8ee7a0454bc6f3744f7e730 # v4.3.1",
                "jurplel/install-qt-action@v4",
            ),
        )
        for pinned, mutable in cases:
            with self.subTest(action=mutable):
                prefix = "      - uses: "
                self.assertEqual(
                    MODULE.pinned_action_errors("fixture.yml", prefix + pinned), []
                )
                errors = MODULE.pinned_action_errors(
                    "fixture.yml", prefix + mutable
                )
                self.assertTrue(
                    any("must pin" in error for error in errors),
                    f"workflow validator accepted mutable action {mutable}",
                )
                quoted_errors = MODULE.pinned_action_errors(
                    "fixture.yml", prefix + f'"{mutable}"'
                )
                self.assertTrue(
                    any("must pin" in error for error in quoted_errors),
                    f"workflow validator accepted quoted mutable action {mutable}",
                )

    def test_pages_pipeline_actions_require_audited_revisions(self):
        for workflow_name, actions in MODULE.PAGES_PIPELINE_ACTION_REVISIONS.items():
            workflow = MODULE.read_workflow(workflow_name)
            for action, (revision, _) in actions.items():
                with self.subTest(workflow=workflow_name, action=action):
                    pinned = f"{action}@{revision}"
                    mutable = f"{action}@v0"
                    self.assertIn(pinned, workflow)
                    errors = MODULE.required_action_revision_errors(
                        workflow_name, workflow.replace(pinned, mutable, 1)
                    )
                    self.assertTrue(
                        any("must pin" in error for error in errors),
                        f"workflow validator accepted mutable action {action}",
                    )

    def test_pages_pipeline_rejects_an_unaudited_remote_action(self):
        pages = MODULE.read_workflow("pages.yml")
        errors = MODULE.required_action_revision_errors(
            "pages.yml",
            pages.replace(
                "    steps:\n",
                "    steps:\n      - uses: example/unreviewed-action@main\n",
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
            "    needs: [authorize, wasm]\n",
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
            "    needs: [authorize, wasm]\n",
            "    name: Deploy GitHub Pages / needs: [authorize, wasm]\n"
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
            "         (github.event_name == 'push' && needs.wasm.result == 'skipped'))\n",
            "         (github.event_name != 'workflow_dispatch' && needs.wasm.result == 'skipped'))\n",
        )
        self.assertTrue(
            any("deployment condition" in error for error in errors),
            "workflow validator accepted an open-ended Pages caller event",
        )

    def test_pages_deploy_condition_cannot_be_made_unconditionally_true(self):
        errors = self.workflow_errors_with_replacement(
            "pages.yml",
            "         (github.event_name == 'push' && needs.wasm.result == 'skipped'))\n",
            "         (github.event_name == 'push' && needs.wasm.result == 'skipped')) || true\n",
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
        deploy_action = (
            "        uses: actions/deploy-pages@"
            "d6db90164ac5ed86f2b6aed7e0febac5b3c0c03e # v4.0.5\n"
        )
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
            "    needs: [plan, wasm]\n"
            "    if: ${{ github.event_name == 'push' && needs.wasm.result == 'success' }}\n",
            "    name: Deploy validated WebAssembly Gallery / github.event_name == 'push' / needs.wasm.result == 'success'\n"
            "    needs: [plan, wasm]\n"
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
                "          python3 tools/quality/check_cpp_format.py --changed-from \"$PR_BASE_SHA\" --clang-format \"$formatter\"\n",
                "          true || python3 tools/quality/check_cpp_format.py --changed-from \"$PR_BASE_SHA\" --clang-format \"$formatter\"\n",
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
            "workflow validator accepted more than 300 active lines",
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


if __name__ == "__main__":
    unittest.main()
