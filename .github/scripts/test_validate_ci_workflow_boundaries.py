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
