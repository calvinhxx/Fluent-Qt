#!/usr/bin/env python3

"""Keep the top-level CI workflow free of C++ and PySide6 implementation."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"
MAX_ORCHESTRATOR_ACTIVE_LINES = 300
CANONICAL_WORKFLOW_LEVEL_ENTRY = re.compile(
    r"^(?P<name>[a-z][a-z0-9-]*):(?P<value>.*)$"
)
CANONICAL_JOB_ID_ENTRY = re.compile(
    r"^  (?P<name>[A-Za-z0-9_-]+):$"
)
CANONICAL_JOB_LEVEL_ENTRY = re.compile(
    r"^    (?P<name>[a-z][a-z0-9-]*):(?P<value>.*)$"
)
ALLOWED_WORKFLOW_LEVEL_KEYS = {
    "concurrency",
    "defaults",
    "env",
    "jobs",
    "name",
    "on",
    "permissions",
    "run-name",
}
ALLOWED_JOB_LEVEL_KEYS = {
    "env",
    "environment",
    "if",
    "name",
    "needs",
    "outputs",
    "permissions",
    "runs-on",
    "steps",
    "strategy",
    "timeout-minutes",
    "uses",
    "with",
    "continue-on-error",
}

EXPECTED_JOBS = {
    "ci.yml": {
        "plan",
        "cpp",
        "python",
        "wasm",
        "pages",
        "ci-gate",
        "release-ready",
    },
    "ci-cpp.yml": {"plan", "build", "integration"},
    "ci-wasm.yml": {"build"},
    "ci-python.yml": {
        "plan",
        "pyside6_linux",
        "pyside6_windows",
        "pyside6_macos",
        "pyside6_release",
        "pyside6_release_bundle",
        "pyside6_platform_summary",
    },
    "python-release.yml": {
        "preflight",
        "prepare",
        "publish_testpypi",
        "verify_testpypi",
        "publish_pypi",
        "verify_pypi",
    },
    "desktop-release-candidate.yml": {"plan", "package", "assemble"},
    "release-candidate.yml": {"plan", "desktop", "python", "ready"},
    "release.yml": {
        "preflight",
        "desktop_candidate",
        "source-package",
        "python_candidate",
        "publish",
        "publish_python",
    },
    "pages.yml": {"wasm", "deploy"},
}
UNCONDITIONAL_JOBS = {
    "ci-python.yml": {"plan"},
    "ci-wasm.yml": {"build"},
}


def read_workflow(name: str) -> str:
    return (WORKFLOWS / name).read_text(encoding="utf-8")


def job_section(contents: str, job_id: str) -> str:
    match = re.search(
        rf"^  {re.escape(job_id)}:\n(?P<body>.*?)(?=^  [A-Za-z0-9_-]+:\n|\Z)",
        contents,
        re.MULTILINE | re.DOTALL,
    )
    return match.group(0) if match else ""


def named_step_section(job: str, step_name: str) -> str:
    match = re.search(
        rf"^      - name: {re.escape(step_name)}\n"
        rf"(?P<body>.*?)(?=^      - name: |\Z)",
        job,
        re.MULTILINE | re.DOTALL,
    )
    return match.group(0) if match else ""


def uncommented_workflow_lines(contents: str) -> list[str]:
    """Return non-empty YAML/shell lines while preserving indentation."""
    lines: list[str] = []
    for raw_line in contents.splitlines():
        output: list[str] = []
        quote = ""
        index = 0
        while index < len(raw_line):
            character = raw_line[index]
            if quote:
                output.append(character)
                if quote == '"' and character == "`" and index + 1 < len(raw_line):
                    index += 1
                    output.append(raw_line[index])
                elif character == quote:
                    if quote == "'" and index + 1 < len(raw_line) and raw_line[index + 1] == "'":
                        index += 1
                        output.append(raw_line[index])
                    else:
                        quote = ""
                index += 1
                continue
            if character == "#":
                break
            if character in {'"', "'"}:
                quote = character
            output.append(character)
            index += 1
        active = "".join(output).rstrip()
        if active.strip():
            lines.append(active)
    return lines


def leading_space_count(line: str) -> int:
    return len(line) - len(line.lstrip(" "))


def workflow_job_map_errors(
    name: str,
    contents: str,
    expected_jobs: set[str],
) -> list[str]:
    """Require one canonical workflow root and one canonical key per job."""
    active = uncommented_workflow_lines(contents)
    errors: list[str] = []
    root_keys: list[str] = []
    for line in active:
        if leading_space_count(line) != 0:
            continue
        match = CANONICAL_WORKFLOW_LEVEL_ENTRY.fullmatch(line)
        if not match or match.group("name") not in ALLOWED_WORKFLOW_LEVEL_KEYS:
            errors.append(
                f"{name} must use an allowed plain workflow-level key: {line}"
            )
            continue
        root_keys.append(match.group("name"))

    for key in sorted(set(root_keys)):
        if root_keys.count(key) > 1:
            errors.append(f"{name} must not repeat workflow-level key: {key}")
    if root_keys.count("jobs") != 1 or "jobs:" not in active:
        errors.append(f"{name} must contain exactly one plain top-level jobs: mapping")
        return errors

    jobs_index = active.index("jobs:")
    job_map: list[str] = []
    for line in active[jobs_index + 1 :]:
        if leading_space_count(line) == 0:
            break
        job_map.append(line)
    if not job_map:
        errors.append(f"{name} jobs mapping must not be empty")
        return errors
    if min(leading_space_count(line) for line in job_map) != 2:
        errors.append(f"{name} job ids must use exactly two spaces of indentation")
        return errors

    job_ids: list[str] = []
    for line in job_map:
        if leading_space_count(line) != 2:
            continue
        match = CANONICAL_JOB_ID_ENTRY.fullmatch(line)
        if not match:
            errors.append(f"{name} must use a plain canonical job id: {line.strip()}")
            continue
        job_ids.append(match.group("name"))

    for job_id in sorted(set(job_ids)):
        if job_ids.count(job_id) > 1:
            errors.append(f"{name} must not repeat job id: {job_id}")
    actual_jobs = set(job_ids)
    if actual_jobs != expected_jobs or len(job_ids) != len(expected_jobs):
        errors.append(
            f"{name} jobs must be {sorted(expected_jobs)}, got {sorted(job_ids)}"
        )
    return errors


def job_level_lines(job: str) -> list[str]:
    return [
        line
        for line in uncommented_workflow_lines(job)
        if leading_space_count(line) == 4
    ]


def canonical_job_level_errors(job: str, context: str) -> list[str]:
    errors: list[str] = []
    active = uncommented_workflow_lines(job)
    body = active[1:]
    if body and min(leading_space_count(line) for line in body) != 4:
        errors.append(
            f"{context} job-level keys must use exactly four spaces of indentation"
        )
        return errors
    seen: set[str] = set()
    for line in job_level_lines(job):
        match = CANONICAL_JOB_LEVEL_ENTRY.fullmatch(line)
        if not match or match.group("name") not in ALLOWED_JOB_LEVEL_KEYS:
            errors.append(
                f"{context} job must use an allowed plain job-level key: {line.strip()}"
            )
            continue
        name = match.group("name")
        if name in seen:
            errors.append(f"{context} job must not repeat job-level key: {name}")
        seen.add(name)
    return errors


def job_level_controls(job: str) -> list[str]:
    controls: list[str] = []
    for line in job_level_lines(job):
        match = CANONICAL_JOB_LEVEL_ENTRY.fullmatch(line)
        if not match or match.group("name") not in {"if", "continue-on-error"}:
            continue
        value = match.group("value").strip()
        controls.append(
            f"{match.group('name')}: {value}" if value else f"{match.group('name')}:"
        )
    return controls


def validate_cpp_execution_contract(cpp: str) -> list[str]:
    raw_build = job_section(cpp, "build")
    active_cpp = "\n".join(uncommented_workflow_lines(cpp)) + "\n"
    build = job_section(active_cpp, "build")
    errors = canonical_job_level_errors(build, "ci-cpp.yml build")
    if "<#" in raw_build or "#>" in raw_build:
        errors.append(
            "ci-cpp.yml build job must not use PowerShell block comments"
        )
    test_step = named_step_section(build, "Test")
    if not test_step:
        return [*errors, "ci-cpp.yml build job must contain the matrix Test step"]
    forbidden_job_keys = job_level_controls(build)
    if forbidden_job_keys:
        errors.append(
            "ci-cpp.yml build job must not be disabled or made fail-open: "
            + ", ".join(forbidden_job_keys)
        )

    step_lines = uncommented_workflow_lines(test_step)
    expected_prelude = [
        "      - name: Test",
        "        if: ${{ matrix.build == true && matrix.test == true }}",
        "        shell: pwsh",
        "        env:",
        "          QT_QPA_PLATFORM: ${{ matrix.qt_qpa_platform || 'offscreen' }}",
        "          SKIP_VISUAL_TEST: 1",
        "          ASAN_OPTIONS: ${{ matrix.asan_options || '' }}",
        "          UBSAN_OPTIONS: ${{ matrix.ubsan_options || '' }}",
        "        run: |",
    ]
    expected_script = [
        '$testArgs = @(',
        '"--preset", "${{ matrix.preset }}",',
        '"--output-on-failure",',
        '"--timeout", "${{ matrix.ctest_timeout }}",',
        '"--no-tests=error"',
        ')',
        '$testLabels = "${{ matrix.test_labels }}"',
        '$excludeLabels = "${{ matrix.exclude_labels }}"',
        'if ($testLabels) {',
        '$testArgs += @("-L", $testLabels)',
        '}',
        'if ($excludeLabels) {',
        '$testArgs += @("-LE", $excludeLabels)',
        '}',
        "ctest @testArgs",
    ]
    if step_lines[: len(expected_prelude)] != expected_prelude:
        errors.append(
            "ci-cpp.yml matrix Test step prelude must match the fail-closed contract"
        )
    script_lines = [
        line.strip() for line in step_lines[len(expected_prelude) :]
    ]
    if script_lines != expected_script:
        errors.append(
            "ci-cpp.yml matrix Test script must exactly execute the approved "
            "ctest argument sequence"
        )
    return errors


def validate_cpp_plan_contract(cpp: str) -> list[str]:
    """Keep C++ catalog and repository-quality validation fail closed."""
    active_cpp = "\n".join(uncommented_workflow_lines(cpp)) + "\n"
    plan = job_section(active_cpp, "plan")
    errors = canonical_job_level_errors(plan, "ci-cpp.yml plan")
    controls = job_level_controls(plan)
    if controls:
        errors.append(
            "ci-cpp.yml plan job must not be disabled or made fail-open: "
            + ", ".join(controls)
        )
    step_headers = [
        line
        for line in uncommented_workflow_lines(plan)
        if leading_space_count(line) == 6 and line.lstrip().startswith("- ")
    ]
    expected_step_headers = [
        "      - name: Checkout",
        "      - name: Validate C++ CI catalogs",
        "      - name: Select C++ validation matrix",
    ]
    if step_headers != expected_step_headers:
        errors.append(
            "ci-cpp.yml plan must contain exactly the approved checkout, catalog, "
            "and matrix-selection steps"
        )

    checkout = named_step_section(plan, "Checkout")
    checkout_lines = uncommented_workflow_lines(checkout)
    if checkout_lines != [
        "      - name: Checkout",
        "        uses: actions/checkout@v6",
    ]:
        errors.append(
            "ci-cpp.yml checkout step must exactly use actions/checkout@v6"
        )

    catalog = named_step_section(plan, "Validate C++ CI catalogs")
    if not catalog:
        return [
            *errors,
            "ci-cpp.yml plan job must contain the C++ catalog validation step",
        ]
    catalog_lines = uncommented_workflow_lines(catalog)
    catalog_prelude = [
        "      - name: Validate C++ CI catalogs",
        "        shell: bash",
        "        run: |",
    ]
    catalog_script = [
        "python3 .github/scripts/test_validate_ci_cpp_matrix.py",
        "python3 .github/scripts/validate-package-matrix.py",
        "python3 .github/scripts/validate-project-metadata.py",
        "python3 tools/quality/validate_component_api.py --project-root . --self-test",
        "python3 tools/quality/validate_visual_evidence_inventory.py --project-root . --self-test",
    ]
    if catalog_lines[: len(catalog_prelude)] != catalog_prelude:
        errors.append(
            "ci-cpp.yml C++ catalog validation step prelude must match the "
            "fail-closed contract"
        )
    script_lines = [
        line.strip() for line in catalog_lines[len(catalog_prelude) :]
    ]
    if script_lines != catalog_script:
        errors.append(
            "ci-cpp.yml C++ catalog validation script must exactly execute the "
            "approved catalog and quality validators"
        )

    selection = named_step_section(plan, "Select C++ validation matrix")
    if not selection:
        return [
            *errors,
            "ci-cpp.yml plan job must contain the matrix-selection step",
        ]
    selection_lines = uncommented_workflow_lines(selection)
    selection_prelude = [
        "      - name: Select C++ validation matrix",
        "        id: matrix",
        "        env:",
        "          VALIDATION_MODE: ${{ inputs.mode }}",
        "        shell: bash",
        "        run: |",
    ]
    selection_script = [
        "set -euo pipefail",
        'case "$VALIDATION_MODE" in',
        "fast|full) ;;",
        "*)",
        'echo "::error::Unsupported C++ validation tier: $VALIDATION_MODE"',
        "exit 1",
        ";;",
        "esac",
        'matrix="$(',
        "jq -c \\",
        '--arg mode "$VALIDATION_MODE" \\',
        "'{include: [.scenarios[] |",
        "select(.mode == $mode) |",
        "del(.mode)]}' \\",
        ".github/ci-cpp-matrix.json",
        ')"',
        'scenario_count="$(jq \'.include | length\' <<< "$matrix")"',
        'if [[ "$scenario_count" == "0" ]]; then',
        'echo "::error::The $VALIDATION_MODE C++ matrix is empty."',
        "exit 1",
        "fi",
        'echo "matrix=$matrix" >> "$GITHUB_OUTPUT"',
        'echo "mode=$VALIDATION_MODE" >> "$GITHUB_OUTPUT"',
        'echo "Selected $scenario_count C++ $VALIDATION_MODE scenarios."',
    ]
    if selection_lines[: len(selection_prelude)] != selection_prelude:
        errors.append(
            "ci-cpp.yml matrix-selection step prelude must match the "
            "fail-closed contract"
        )
    selection_script_lines = [
        line.strip() for line in selection_lines[len(selection_prelude) :]
    ]
    if selection_script_lines != selection_script:
        errors.append(
            "ci-cpp.yml matrix-selection script must exactly validate and select "
            "the requested catalog tier"
        )
    return errors


def validate_cpp_integration_contract(cpp: str) -> list[str]:
    """Keep package and installed-consumer validation unconditional."""
    active_cpp = "\n".join(uncommented_workflow_lines(cpp)) + "\n"
    integration = job_section(active_cpp, "integration")
    errors = canonical_job_level_errors(integration, "ci-cpp.yml integration")
    controls = job_level_controls(integration)
    if controls:
        errors.append(
            "ci-cpp.yml integration job must not be disabled or made fail-open: "
            + ", ".join(controls)
        )
    return errors


def validate_job_level_contracts(name: str, contents: str) -> list[str]:
    """Reject fail-open controls outside workflows with specialized checks."""
    errors: list[str] = []
    unconditional_jobs = UNCONDITIONAL_JOBS.get(name, set())
    for job_id in sorted(EXPECTED_JOBS[name]):
        context = f"{name} {job_id}"
        job = job_section(contents, job_id)
        errors.extend(canonical_job_level_errors(job, context))
        controls = job_level_controls(job)
        if any(
            control.startswith("continue-on-error:") for control in controls
        ):
            errors.append(f"{context} job must not continue on error")
        if job_id in unconditional_jobs and any(
            control.startswith("if:") for control in controls
        ):
            errors.append(f"{context} job must not be conditional")
    return errors


def validate_pr_change_classification_contract(orchestrator: str) -> list[str]:
    """Require the trusted PR file count to reach the classifier executable."""
    active = "\n".join(uncommented_workflow_lines(orchestrator)) + "\n"
    plan = job_section(active, "plan")
    step = named_step_section(plan, "Classify pull-request changes")
    if not step:
        return ["ci.yml plan job must contain the pull-request classification step"]

    lines = uncommented_workflow_lines(step)
    expected_prelude = [
        "      - name: Classify pull-request changes",
        "        id: changes",
        "        env:",
        "          GH_TOKEN: ${{ github.token }}",
        "          PR_NUMBER: ${{ github.event.pull_request.number }}",
        "          PR_CHANGED_FILES: ${{ github.event.pull_request.changed_files }}",
        "        shell: bash",
        "        run: |",
    ]
    expected_script = [
        "set -euo pipefail",
        "should_build=true",
        "should_build_pyside=true",
        "should_build_wasm=true",
        'if [[ "${{ github.event_name }}" == "pull_request" ]]; then',
        'changed_files_json="$(',
        "gh api --paginate --slurp \\",
        '"repos/${GITHUB_REPOSITORY}/pulls/${PR_NUMBER}/files?per_page=100" \\',
        '--header "X-GitHub-Api-Version: 2022-11-28"',
        ')"',
        'if [[ -z "$changed_files_json" ]]; then',
        'echo "::error::The pull request did not report any changed files."',
        "exit 1",
        "fi",
        'classification="$(',
        "python3 .github/scripts/classify_ci_changes.py --github-files-json \\",
        '--expected-count "$PR_CHANGED_FILES" \\',
        '<<< "$changed_files_json"',
        ')"',
        'should_build="$(',
        "sed -n 's/^should_build=//p' <<< \"$classification\"",
        ')"',
        'should_build_pyside="$(',
        "sed -n 's/^should_build_pyside=//p' <<< \"$classification\"",
        ')"',
        'should_build_wasm="$(',
        "sed -n 's/^should_build_wasm=//p' <<< \"$classification\"",
        ')"',
        "fi",
        'for value in "$should_build" "$should_build_pyside" "$should_build_wasm"; do',
        '[[ "$value" == "true" || "$value" == "false" ]] || {',
        'echo "::error::Invalid CI change classification value: $value"',
        "exit 1",
        "}",
        "done",
        'if [[ "$should_build" == "false" && \\',
        '("$should_build_pyside" == "true" || "$should_build_wasm" == "true") ]]; then',
        'echo "::error::A specialized module cannot run when native validation is skipped."',
        "exit 1",
        "fi",
        'echo "should_build=$should_build" >> "$GITHUB_OUTPUT"',
        'echo "should_build_pyside=$should_build_pyside" >> "$GITHUB_OUTPUT"',
        'echo "should_build_wasm=$should_build_wasm" >> "$GITHUB_OUTPUT"',
        'if [[ "$should_build" == "true" ]]; then',
        'echo "The C++ validation module is required."',
        "else",
        'echo "Documentation-only pull request: skipping build validation."',
        "fi",
        'if [[ "$should_build_pyside" == "true" ]]; then',
        'echo "The PySide6 validation module is required."',
        "else",
        'echo "No binding-affecting files changed: skipping PySide6 validation."',
        "fi",
        'if [[ "$should_build_wasm" == "true" ]]; then',
        'echo "The WebAssembly validation module is required."',
        "else",
        'echo "No browser-affecting files changed: skipping WebAssembly validation."',
        "fi",
    ]
    errors: list[str] = []
    if lines[: len(expected_prelude)] != expected_prelude:
        errors.append(
            "ci.yml pull-request classification prelude must bind PR_CHANGED_FILES "
            "to github.event.pull_request.changed_files"
        )
    script = [line.strip() for line in lines[len(expected_prelude) :]]
    if script != expected_script:
        errors.append(
            "ci.yml pull-request classification script must exactly fetch every API "
            "page and pass --expected-count \"$PR_CHANGED_FILES\" to the executable"
        )
    return errors


def validate_ci_gate_execution_contract(orchestrator: str) -> list[str]:
    """Keep orchestration failures and missing classification outputs closed."""
    active = "\n".join(uncommented_workflow_lines(orchestrator)) + "\n"
    errors: list[str] = []
    plan = job_section(active, "plan")
    plan_conditions = [
        value
        for value in job_level_controls(plan)
        if value.startswith("if:")
    ]
    if plan_conditions:
        errors.append("ci.yml plan job must not be conditional")
    for job_id in EXPECTED_JOBS["ci.yml"]:
        job = job_section(active, job_id)
        errors.extend(canonical_job_level_errors(job, f"ci.yml {job_id}"))
        controls = job_level_controls(job)
        if any(value.startswith("continue-on-error:") for value in controls):
            errors.append(
                f"ci.yml {job_id} job must not continue on error"
            )

    gate = job_section(active, "ci-gate")
    controls = job_level_controls(gate)
    gate_conditions = [value for value in controls if value.startswith("if:")]
    if gate_conditions != ["if: ${{ always() }}"]:
        errors.append(
            "ci.yml CI Gate must run with exactly if: ${{ always() }}"
        )
    step = named_step_section(gate, "Verify required validation")
    if not step:
        errors.append("ci.yml CI Gate must contain its verification step")
        return errors

    lines = uncommented_workflow_lines(step)
    expected_prelude = [
        "      - name: Verify required validation",
        "        env:",
        "          PLAN_RESULT: ${{ needs.plan.result }}",
        "          CPP_RESULT: ${{ needs.cpp.result }}",
        "          PYTHON_RESULT: ${{ needs.python.result }}",
        "          WASM_RESULT: ${{ needs.wasm.result }}",
        "          SHOULD_BUILD: ${{ needs.plan.outputs.should_build }}",
        "          SHOULD_BUILD_PYSIDE: ${{ needs.plan.outputs.should_build_pyside }}",
        "          SHOULD_BUILD_WASM: ${{ needs.plan.outputs.should_build_wasm }}",
        "        shell: bash",
        "        run: |",
    ]
    expected_script = [
        "set -euo pipefail",
        'if [[ "$PLAN_RESULT" != "success" ]]; then',
        'echo "::error::CI planning finished with result: $PLAN_RESULT"',
        "exit 1",
        "fi",
        'for value in "$SHOULD_BUILD" "$SHOULD_BUILD_PYSIDE" "$SHOULD_BUILD_WASM"; do',
        '[[ "$value" == "true" || "$value" == "false" ]] || {',
        'echo "::error::Invalid or missing CI classification output: $value"',
        "exit 1",
        "}",
        "done",
        'if [[ "$SHOULD_BUILD" == "true" && "$CPP_RESULT" != "success" ]]; then',
        'echo "::error::The C++ validation module finished with result: $CPP_RESULT"',
        "exit 1",
        "fi",
        'if [[ "$SHOULD_BUILD" != "true" && "$CPP_RESULT" != "skipped" ]]; then',
        'echo "::error::C++ validation should have been skipped, got: $CPP_RESULT"',
        "exit 1",
        "fi",
        'if [[ "$SHOULD_BUILD_PYSIDE" == "true" && "$PYTHON_RESULT" != "success" ]]; then',
        'echo "::error::The PySide6 validation module finished with result: $PYTHON_RESULT"',
        "exit 1",
        "fi",
        'if [[ "$SHOULD_BUILD_PYSIDE" != "true" && "$PYTHON_RESULT" != "skipped" ]]; then',
        'echo "::error::PySide6 validation should have been skipped, got: $PYTHON_RESULT"',
        "exit 1",
        "fi",
        'if [[ "$SHOULD_BUILD_WASM" == "true" && "$WASM_RESULT" != "success" ]]; then',
        'echo "::error::The WebAssembly validation module finished with result: $WASM_RESULT"',
        "exit 1",
        "fi",
        'if [[ "$SHOULD_BUILD_WASM" != "true" && "$WASM_RESULT" != "skipped" ]]; then',
        'echo "::error::WebAssembly validation should have been skipped, got: $WASM_RESULT"',
        "exit 1",
        "fi",
        'echo "Required CI validation passed."',
    ]
    if lines[: len(expected_prelude)] != expected_prelude:
        errors.append(
            "ci.yml CI Gate prelude must bind every result and classification output"
        )
    script = [line.strip() for line in lines[len(expected_prelude) :]]
    if script != expected_script:
        errors.append(
            "ci.yml CI Gate script must exactly reject failed modules and invalid or "
            "missing classification outputs"
        )
    return errors


def validate_boundaries() -> list[str]:
    errors: list[str] = []
    contents: dict[str, str] = {}
    for name, expected_jobs in EXPECTED_JOBS.items():
        try:
            contents[name] = read_workflow(name)
        except OSError as error:
            errors.append(f"unable to read {name}: {error}")
            continue
        errors.extend(
            workflow_job_map_errors(name, contents[name], expected_jobs)
        )

    if errors:
        return errors

    orchestrator = contents["ci.yml"]
    cpp = contents["ci-cpp.yml"]
    wasm = contents["ci-wasm.yml"]
    python = contents["ci-python.yml"]
    python_release = contents["python-release.yml"]
    desktop_candidate = contents["desktop-release-candidate.yml"]
    release_candidate = contents["release-candidate.yml"]
    release = contents["release.yml"]
    pages = contents["pages.yml"]
    active_orchestrator_lines = uncommented_workflow_lines(orchestrator)
    active_orchestrator = "\n".join(active_orchestrator_lines) + "\n"
    errors.extend(validate_pr_change_classification_contract(orchestrator))
    errors.extend(validate_ci_gate_execution_contract(orchestrator))
    for name in sorted(EXPECTED_JOBS.keys() - {"ci.yml", "ci-cpp.yml"}):
        errors.extend(validate_job_level_contracts(name, contents[name]))

    if len(active_orchestrator_lines) > MAX_ORCHESTRATOR_ACTIVE_LINES:
        errors.append(
            "ci.yml must remain a compact orchestration-only workflow with no more "
            f"than {MAX_ORCHESTRATOR_ACTIVE_LINES} active lines"
        )
    for required in (
        "uses: ./.github/workflows/ci-cpp.yml",
        "uses: ./.github/workflows/ci-python.yml",
        "uses: ./.github/workflows/ci-wasm.yml",
        "uses: ./.github/workflows/pages.yml",
        "name: CI Gate",
        "name: Release ready",
        "python_release_bundle:",
        'python_release_bundle="true"',
        "build_release_bundle: ${{ needs.plan.outputs.python_release_bundle == 'true' }}",
        "run_compatibility_validation: true",
        "run_macos_release_validation: ${{ needs.plan.outputs.run_macos_release_validation == 'true' }}",
        'run_macos_release_validation="false"',
        'refs/tags/$tag^{tag}',
        'docs/releases/$tag.md',
        "release_max_parallel: 4",
        "Require a current main base for release pull requests",
        ".github/scripts/check-release-branch-freshness.py",
        "actions: read",
    ):
        if required not in active_orchestrator:
            errors.append(f"ci.yml is missing orchestration contract: {required}")
    if active_orchestrator.count('python_release_bundle="true"') != 1:
        errors.append(
            "ci.yml must reserve the complete Python bundle for scheduled validation; "
            "the release-candidate workflow owns stable promotion artifacts"
        )
    for forbidden in (
        "cmake --build",
        "install-qt-action",
        "PySide6==",
        "vcpkg-",
        "emsdk",
        "playwright",
    ):
        if forbidden in active_orchestrator:
            errors.append(f"ci.yml contains module implementation detail: {forbidden}")
    pages_call = job_section(active_orchestrator, "pages")
    for required in (
        "needs: [plan, wasm]",
        "github.event_name == 'push'",
        "needs.wasm.result == 'success'",
        "pages: write",
        "id-token: write",
    ):
        if required not in pages_call:
            errors.append(
                f"ci.yml automatic Pages deployment is missing contract: {required}"
            )
    release_ready = job_section(active_orchestrator, "release-ready")
    for required in (
        "needs: [plan, ci-gate]",
        "CI_GATE_RESULT: ${{ needs.ci-gate.result }}",
    ):
        if required not in release_ready:
            errors.append(f"ci.yml Release ready is missing gate contract: {required}")

    for name, module in (
        ("ci-cpp.yml", cpp),
        ("ci-python.yml", python),
        ("ci-wasm.yml", wasm),
    ):
        if "workflow_call:" not in module:
            errors.append(f"{name} must be a reusable workflow")
        if "needs.plan.outputs.should_build" in module:
            errors.append(f"{name} must not depend on orchestrator classification outputs")

    for required in (
        "Qt 6.9.3 / Emscripten 3.1.70",
        "wasm_singlethread",
        "aqtinstall==3.3.0",
        "playwright==1.58.0",
        "cmake --preset wasm",
        "cmake --build --preset wasm --parallel",
        "Verify installed FluentQt WebAssembly consumer",
        "build/wasm-installed-consumer",
        ".github/scripts/run-wasm-browser-smoke.py",
        ".github/scripts/stage-wasm-pages.py",
        "name: fluentqt-wasm-pages",
    ):
        if required not in wasm:
            errors.append(f"ci-wasm.yml is missing browser validation contract: {required}")
    for forbidden in ("VCPKG_ROOT", "PySide6==", "shiboken6_generator=="):
        if forbidden in wasm:
            errors.append(f"ci-wasm.yml contains unrelated module detail: {forbidden}")

    for required in (
        "uses: ./.github/workflows/ci-wasm.yml",
        "workflow_call:",
        "workflow_dispatch:",
        "mode: full",
        "name: fluentqt-wasm-pages",
        "path: build/pages",
        "needs: wasm",
        "github.event_name == 'workflow_dispatch'",
        "github.event_name != 'workflow_dispatch'",
        "needs.wasm.result == 'skipped'",
    ):
        if required not in pages:
            errors.append(f"pages.yml is missing WebAssembly deployment contract: {required}")
    if re.search(r"^  push:\s*$", pages, re.MULTILINE):
        errors.append("pages.yml must not rebuild WASM automatically outside main CI")

    if ".github/ci-cpp-matrix.json" not in cpp:
        errors.append("ci-cpp.yml must own the C++ matrix catalog")
    cpp_build = job_section(cpp, "build")
    errors.extend(validate_cpp_plan_contract(cpp))
    errors.extend(validate_cpp_execution_contract(cpp))
    errors.extend(validate_cpp_integration_contract(cpp))
    if "max-parallel: 4" not in cpp_build:
        errors.append(
            "ci-cpp.yml build matrix must cap parallel action downloads at 4"
        )
    for required in (
        "name: Smoke adaptive build parallelism",
        "tools/dev/fluent_qt_build.py --print-jobs",
        "--jobs auto",
        "--dry-run",
        'if ("${{ runner.os }}" -eq "Windows") { "python" } else { "python3" }',
    ):
        if required not in cpp_build:
            errors.append(
                f"ci-cpp.yml build matrix is missing adaptive build smoke: {required}"
            )
    for forbidden in ("pip install PySide6", "shiboken6_generator==", "pyside6_release:"):
        if forbidden in cpp:
            errors.append(f"ci-cpp.yml contains PySide6 execution detail: {forbidden}")

    if "bindings/pyside6/wheel-matrix.json" not in python:
        errors.append("ci-python.yml must own the PySide6 wheel matrix catalog")
    if "build_release_bundle:" not in python:
        errors.append("ci-python.yml must expose the optional Python release-bundle input")
    if "run_compatibility_validation:" not in python:
        errors.append("ci-python.yml must expose its representative compatibility lanes")
    if "run_macos_release_validation:" not in python:
        errors.append("ci-python.yml must expose its macOS representative lane")
    if "release_max_parallel:" not in python:
        errors.append("ci-python.yml must expose its release-wheel concurrency cap")
    if python.count("inputs.build_release_bundle") < 4:
        errors.append(
            "ci-python.yml must gate matrix selection, release jobs, bundle, and summary"
        )
    pyside_release = job_section(python, "pyside6_release")
    if "max-parallel: ${{ inputs.release_max_parallel }}" not in pyside_release:
        errors.append(
            "ci-python.yml release matrix must honor release_max_parallel"
        )
    release_bundle = job_section(python, "pyside6_release_bundle")
    for required in (
        "- pyside6_macos",
        "needs.pyside6_macos.result == 'success'",
    ):
        if required not in release_bundle:
            errors.append(
                f"ci-python.yml release bundle is missing macOS artifact dependency: {required}"
            )
    for job_id in ("pyside6_linux", "pyside6_windows"):
        compatibility_job = job_section(python, job_id)
        if "if: ${{ inputs.run_compatibility_validation }}" not in compatibility_job:
            errors.append(
                f"ci-python.yml {job_id} must honor run_compatibility_validation"
            )
        if "FLUENT_QT_BUILD_PYSIDE6_GALLERY=ON" not in compatibility_job:
            errors.append(
                f"ci-python.yml {job_id} must test the Gallery on the Qt 6.2 baseline"
            )
    macos_job = job_section(python, "pyside6_macos")
    if "if: ${{ inputs.run_macos_release_validation }}" not in macos_job:
        errors.append(
            "ci-python.yml pyside6_macos must honor run_macos_release_validation"
        )
    if "inputs.run_compatibility_validation" in macos_job:
        errors.append(
            "ci-python.yml pyside6_macos must remain independent of legacy compatibility lanes"
        )
    for required in (
        "actions: read",
        "name: Verify Python candidate platform coverage",
        ".github/scripts/verify-pyside-platform-artifacts.py",
        'verification_scope="release"',
        '--scope "$verification_scope"',
        ".github/scripts/select-pyside-release-matrix.py",
        ".github/scripts/assemble-pyside-release-bundle.py",
        "Prioritized representative scenarios:",
        "name: Test representative PySide6 bindings\n        if: ${{ matrix.extended_acceptance == true }}",
        "fluentqt-pyside6-qt624-cp310-linux-x64",
        "fluentqt-pyside6-qt624-cp310-windows-x64",
        "fluentqt-pyside6-qt693-cp311-macos-arm64",
        "name: PySide6 compatibility / Linux x64 / CPython 3.10 / Qt 6.2.4",
        "name: PySide6 compatibility / Windows x64 / CPython 3.10 / Qt 6.2.4",
        "name: PySide6 release / macOS ARM64 / CPython 3.11 / Qt 6.9.3",
        "name: Assemble canonical Python release bundle",
        "name: fluentqt-python-release-bundle",
        "Linux x64|linux|x64",
        "Linux ARM64|linux|arm64",
        "Windows x64|windows|x64",
        "Windows ARM64|windows|arm64",
        "macOS x64|macos|x64",
        "macOS ARM64|macos|arm64",
    ):
        if required not in python:
            errors.append(f"ci-python.yml is missing platform summary: {required}")
    for contract, expected_count in (
        ("Test core wheel in a clean virtual environment (fast)", 2),
        ("needs.plan.outputs.mode != 'full'", 2),
        ("Run extended installed-wheel acceptance", 1),
    ):
        actual_count = python.count(contract)
        if actual_count != expected_count:
            errors.append(
                f"ci-python.yml must contain {expected_count} occurrence(s) of "
                f"{contract!r}, found {actual_count}"
            )
    for forbidden in ("VCPKG_BINARY_SOURCES", "fluent_qt_ci_full_tests", "Library integration"):
        if forbidden in python:
            errors.append(f"ci-python.yml contains C++ matrix detail: {forbidden}")

    for required in (
        "workflow_call:",
        "source_ref:",
        "source_commit:",
        "package_set:",
        ".github/package-matrix.json",
        "name: fluentqt-desktop-package-${{ matrix.id }}",
        "name: fluentqt-desktop-release-candidate",
        ".github/scripts/assemble-desktop-release-candidate.py assemble",
        ".github/scripts/assemble-desktop-release-candidate.py verify",
        "for attempt in 1 2 3",
        "smoke-launch-macos-gallery.sh",
        "id: upload-package",
        "continue-on-error: true",
        "name: Retry package artifact upload",
        "steps.upload-package.outcome == 'failure'",
        "compression-level: 0",
        "overwrite: true",
        "failure() && steps.upload-package.outcome != 'failure'",
        "timeout-minutes: 3",
        "diagnostics-desktop-release-${{ matrix.id }}-${{ github.run_attempt }}",
    ):
        if required not in desktop_candidate:
            errors.append(
                "desktop-release-candidate.yml is missing packaging contract: "
                f"{required}"
            )
    if "workflow_dispatch:" in desktop_candidate or "push:" in desktop_candidate:
        errors.append(
            "desktop-release-candidate.yml must remain reusable-only"
        )

    for required in (
        "name: Release Candidate",
        "branches:\n      - main",
        "workflow_dispatch:",
        "uses: ./.github/workflows/desktop-release-candidate.yml",
        "uses: ./.github/workflows/ci-python.yml",
        "build_release_bundle: true",
        "run_compatibility_validation: false",
        "run_macos_release_validation: true",
        "release_max_parallel: 8",
        "name: Release Candidate ready",
        "refs/tags/$tag^{tag}",
        "docs/releases/$tag.md",
        "name: fluentqt-desktop-release-candidate",
        "name: fluentqt-python-release-bundle",
        "name: fluentqt-release-candidate-receipt",
        ".github/scripts/assemble-desktop-release-candidate.py verify",
        ".source.ci_run_id == $run_id",
        "cancel-in-progress: true",
    ):
        if required not in release_candidate:
            errors.append(
                f"release-candidate.yml is missing promotion contract: {required}"
            )
    for forbidden in ("contents: write", "id-token: write", "gh release"):
        if forbidden in release_candidate:
            errors.append(
                f"release-candidate.yml may not publish external state: {forbidden}"
            )

    for required in (
        "workflow_dispatch:",
        "name: Publish to TestPyPI",
        "name: Publish to PyPI",
        "environment:\n      name: ${{ matrix.environment_name }}",
        "uses: pypa/gh-action-pypi-publish@release/v1",
        "name: fluentqt-python-release-bundle",
        "name: fluentqt-python-core-publish-candidate",
        "name: fluentqt-python-gallery-publish-candidate",
        "dist/fluentqt-*.whl",
        "dist/fluentqt_gallery-*.whl",
        ".github/scripts/verify-python-package-index.py",
        ".github/scripts/install-python-release-from-index.py",
        "attestations: true",
        "skip-existing: ${{ needs.preflight.outputs.stage == 'all' || needs.preflight.outputs.recovery == 'true' }}",
        "- all",
        "needs.preflight.outputs.stage == 'all'",
        "source_tag:",
        "SOURCE_TAG: ${{ inputs.source_tag }}",
        "source_run_id:",
        "SOURCE_RUN_ID: ${{ inputs.source_run_id }}",
        "source_run_attempt:",
        "SOURCE_RUN_ATTEMPT: ${{ inputs.source_run_attempt }}",
        "source_run_id and source_run_attempt are reserved for stage=all.",
        '"$candidate_name" == "Release"',
        '"$candidate_path" == ".github/workflows/release.yml"',
        '"$candidate_name" == "Release Candidate"',
        '"$candidate_path" == ".github/workflows/release-candidate.yml"',
        "source_ref: ${{ steps.resolve.outputs.source_ref }}",
        "source_tag is only valid for manual TestPyPI recovery.",
        "TestPyPI source_tag recovery requires a published stable GitHub Release.",
        "ref: ${{ needs.preflight.outputs.source_ref }}",
        "for discovery_attempt in {1..12}",
        "Release evidence is not fully visible yet; retrying in 10 seconds.",
        'candidate_name" != "CI full"',
        'candidate_path" != ".github/workflows/ci.yml"',
    ):
        if required not in python_release:
            errors.append(
                f"python-release.yml is missing publication contract: {required}"
            )
    if "workflow_call:" in python_release:
        errors.append(
            "python-release.yml must remain a top-level Trusted Publisher workflow"
        )
    if python_release.count("id-token: write") != 2:
        errors.append(
            "python-release.yml must grant id-token: write to exactly two matrix publish jobs"
        )
    for required in (
        "actions: write",
        "name: Build Python release candidate",
        "uses: ./.github/workflows/ci-python.yml",
        "build_release_bundle: true",
        "run_macos_release_validation: true",
        "name: Resolve immutable release candidate",
        "name: fluentqt-desktop-release-candidate",
        "run-id: ${{ needs.preflight.outputs.candidate_run_id }}",
        "needs.preflight.outputs.promote_candidate == 'true'",
        "A manual stable publication must dispatch release.yml from $tag.",
        "name: Dispatch synchronized Python publication",
        "gh workflow run python-release.yml",
        "-f stage=all",
        'source_run_id="$(jq -r',
        'source_run_attempt="$(jq -r',
        "-f source_run_id=\"${{ steps.candidate.outputs.source_run_id }}\"",
        "-f source_run_attempt=\"${{ steps.candidate.outputs.source_run_attempt }}\"",
        "gh run watch",
        "--exit-status",
    ):
        if required not in release:
            errors.append(f"release.yml is missing Python publication orchestration: {required}")
    stable_publish = job_section(release, "publish")
    for required in (
        "needs: [preflight, desktop_candidate, source-package, python_candidate]",
        "always()",
        "needs.desktop_candidate.result == 'skipped'",
        "needs.desktop_candidate.result == 'success'",
        "needs.source-package.result == 'success'",
        "needs.preflight.outputs.promote_candidate == 'true'",
        "needs.preflight.outputs.promote_candidate != 'true'",
        "needs.python_candidate.result == 'success'",
        ".github/scripts/assemble-desktop-release-candidate.py verify",
        "name: release-source",
    ):
        if required not in stable_publish:
            errors.append(
                "release.yml stable publish must promote or build a complete candidate: "
                f"{required}"
            )
    for required in (
        "find release-dist -maxdepth 1 -type f -print0 | sort -z",
        "mapfile -d '' release_assets",
        '"${release_assets[@]}"',
    ):
        if required not in stable_publish:
            errors.append(
                f"release.yml is missing scoped release asset handling: {required}"
            )
    if "name: diagnostics-desktop-release-${{ matrix.id }}-${{ github.run_attempt }}" not in desktop_candidate:
        errors.append(
            "desktop-release-candidate.yml must keep diagnostics outside the candidate namespace"
        )
    for forbidden in (
        "name: release-diagnostics-${{ matrix.id }}",
        "release-dist/*",
    ):
        if forbidden in release:
            errors.append(
                f"release.yml may publish unintended release assets: {forbidden}"
            )
    if "id-token: write" in release:
        errors.append("release.yml must dispatch the top-level publisher, not receive OIDC")
    publisher_contracts = {
        "publish_testpypi": (
            "environment_name: testpypi",
            "environment_name: testpypi-gallery",
        ),
        "publish_pypi": (
            "environment_name: pypi",
            "environment_name: pypi-gallery",
        ),
    }
    for job_id, environments in publisher_contracts.items():
        section = job_section(python_release, job_id)
        if not section:
            errors.append(f"python-release.yml is missing job {job_id}")
            continue
        if "id-token: write" not in section:
            errors.append(f"{job_id} must receive the short-lived OIDC permission")
        for required in (
            "distribution: FluentQt",
            "distribution: FluentQt-Gallery",
            "candidate_artifact: fluentqt-python-core-publish-candidate",
            "candidate_artifact: fluentqt-python-gallery-publish-candidate",
            "name: ${{ matrix.candidate_artifact }}",
            "packages-dir: publisher-candidate/",
            *environments,
        ):
            if required not in section:
                errors.append(f"{job_id} is missing scoped publisher contract: {required}")
        for forbidden in ("actions/checkout", ".github/scripts/", "run:"):
            if forbidden in section:
                errors.append(
                    f"{job_id} must not execute repository code: {forbidden}"
                )
    for forbidden in (
        "PYPI_API_TOKEN",
        "TEST_PYPI_API_TOKEN",
        "secrets.PYPI",
        "password:",
    ):
        if forbidden in python_release:
            errors.append(
                f"python-release.yml must not use long-lived publishing credentials: {forbidden}"
            )

    return errors


def main() -> int:
    errors = validate_boundaries()
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1
    print("Validated modular CI workflow boundaries.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
