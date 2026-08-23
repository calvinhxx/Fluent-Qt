#!/usr/bin/env python3

"""Measure the path from environment preflight to a working starter window."""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import time
from typing import Callable, Iterable, Mapping, Sequence


TRIAL_VERSION = "0.1.0"
ONBOARDING_ROOT = Path(__file__).resolve().parent
LAUNCHER = ONBOARDING_ROOT / "fluentqt"


@dataclass(frozen=True)
class CommandOutput:
    returncode: int
    output: str
    timed_out: bool = False


@dataclass(frozen=True)
class TrialStep:
    id: str
    status: str
    duration_ms: int
    command: list[str]
    detail: str = ""


Runner = Callable[
    [Sequence[str], Path | None, Mapping[str, str] | None, int], CommandOutput
]


def run_command(
    command: Sequence[str],
    cwd: Path | None,
    environ: Mapping[str, str] | None,
    timeout_seconds: int,
) -> CommandOutput:
    """Run one local trial step without a shell or network request."""
    try:
        completed = subprocess.run(
            list(command),
            cwd=cwd,
            env=None if environ is None else dict(environ),
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=timeout_seconds,
        )
        return CommandOutput(completed.returncode, completed.stdout.strip())
    except subprocess.TimeoutExpired:
        return CommandOutput(
            124,
            f"Timed out after {timeout_seconds} seconds.",
            timed_out=True,
        )
    except OSError as error:
        return CommandOutput(1, str(error))


def _tail(value: str, line_count: int = 12) -> str:
    lines = [line.rstrip() for line in value.splitlines() if line.strip()]
    return "\n".join(lines[-line_count:])


def _run_step(
    step_id: str,
    command: Sequence[str],
    *,
    cwd: Path | None,
    environ: Mapping[str, str] | None,
    timeout_seconds: int,
    runner: Runner,
) -> tuple[TrialStep, CommandOutput]:
    started = time.monotonic()
    output = runner(command, cwd, environ, timeout_seconds)
    duration_ms = max(0, round((time.monotonic() - started) * 1000))
    status = "passed" if output.returncode == 0 else "failed"
    detail = _tail(output.output)
    if status == "passed" and not detail:
        detail = "Completed successfully."
    return (
        TrialStep(step_id, status, duration_ms, list(command), detail),
        output,
    )


def _planned_step_ids(profile: str) -> tuple[str, ...]:
    if profile == "cpp":
        return ("doctor", "create", "configure", "build", "tests", "window")
    return ("doctor", "create", "tests", "window")


def _skipped_steps(ids: Iterable[str]) -> list[TrialStep]:
    return [TrialStep(step_id, "skipped", 0, []) for step_id in ids]


def _python_environment(target: Path) -> dict[str, str]:
    environment = dict(os.environ)
    source = str(target / "src")
    previous = environment.get("PYTHONPATH")
    environment["PYTHONPATH"] = (
        source if not previous else source + os.pathsep + previous
    )
    environment["QT_QPA_PLATFORM"] = "offscreen"
    return environment


def _cpp_commands(
    *,
    target: Path,
    starter: str,
    fluentqt_source: Path | None,
    cmake_prefix_path: str | None,
    toolchain_file: Path | None,
    configuration: str,
    parallel: int,
) -> list[tuple[str, list[str], Path | None, Mapping[str, str] | None]]:
    build = target / "build"
    configure = [
        "cmake",
        "-S",
        str(target),
        "-B",
        str(build),
        f"-DCMAKE_BUILD_TYPE={configuration}",
        "-DBUILD_TESTING=ON",
    ]
    if fluentqt_source is not None:
        configure.append(f"-DFLUENTQT_SOURCE_DIR={fluentqt_source.resolve()}")
    if cmake_prefix_path:
        configure.append(f"-DCMAKE_PREFIX_PATH={cmake_prefix_path}")
    if toolchain_file is not None:
        configure.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file.resolve()}")

    test_pattern = (
        "_(application_test|quality_report)$"
        if starter == "workbench"
        else "_summary_test$"
    )
    window_suffix = "ui_smoke" if starter == "workbench" else "panel_smoke"
    common_ctest = [
        "ctest",
        "--test-dir",
        str(build),
        "-C",
        configuration,
        "--output-on-failure",
    ]
    return [
        ("configure", configure, None, None),
        (
            "build",
            [
                "cmake",
                "--build",
                str(build),
                "--config",
                configuration,
                "--parallel",
                str(parallel),
            ],
            None,
            None,
        ),
        ("tests", common_ctest + ["-R", test_pattern], None, None),
        ("window", common_ctest + ["-R", f"_{window_suffix}$"], None, None),
    ]


def _python_commands(
    *, target: Path, starter: str
) -> list[tuple[str, list[str], Path | None, Mapping[str, str] | None]]:
    environment = _python_environment(target)
    module = "first_window_trial"
    entry = "main" if starter == "workbench" else "demo"
    return [
        (
            "tests",
            [sys.executable, "-m", "unittest", "discover", "-s", "tests"],
            target,
            environment,
        ),
        (
            "window",
            [
                sys.executable,
                "-m",
                f"{module}.app.{entry}",
                "--smoke-test",
            ],
            target,
            environment,
        ),
    ]


def run_trial(
    *,
    profile: str,
    starter: str,
    target: Path,
    fluentqt_source: Path | None = None,
    cmake_prefix_path: str | None = None,
    toolchain_file: Path | None = None,
    configuration: str = "Release",
    parallel: int = max(1, os.cpu_count() or 1),
    timeout_seconds: int = 600,
    kept_workspace: bool = True,
    runner: Runner = run_command,
) -> dict[str, object]:
    """Run one no-network onboarding trial and return its evidence report."""
    started = time.monotonic()
    recorded_at = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
    planned_ids = _planned_step_ids(profile)
    steps: list[TrialStep] = []
    blockers: list[str] = []
    doctor_report: dict[str, object] | None = None

    doctor_profile = "cpp" if profile == "cpp" else "python"
    doctor_command = [
        sys.executable,
        str(LAUNCHER),
        "doctor",
        "--profile",
        doctor_profile,
        "--format",
        "json",
        "--timeout-seconds",
        str(min(timeout_seconds, 120)),
    ]
    if profile == "cpp" and cmake_prefix_path:
        doctor_command += ["--cmake-prefix-path", cmake_prefix_path]
    if profile == "cpp" and toolchain_file is not None:
        doctor_command += ["--toolchain-file", str(toolchain_file.resolve())]

    doctor_step, doctor_output = _run_step(
        "doctor",
        doctor_command,
        cwd=None,
        environ=None,
        timeout_seconds=timeout_seconds,
        runner=runner,
    )
    steps.append(doctor_step)
    try:
        decoded = json.loads(doctor_output.output)
        if isinstance(decoded, dict):
            doctor_report = decoded
    except json.JSONDecodeError:
        doctor_report = None

    if doctor_step.status != "passed" or not doctor_report or not doctor_report.get("ready"):
        blockers.append(doctor_step.detail or "Environment preflight failed.")
        steps.extend(_skipped_steps(planned_ids[1:]))
        status = "blocked"
    else:
        language = "cpp" if profile == "cpp" else "pyside6"
        create_command = [
            sys.executable,
            str(LAUNCHER),
            "create",
            str(target),
            "--name",
            "First Window Trial",
            "--id",
            "first-window-trial",
            "--language",
            language,
            "--starter",
            starter,
            "--format",
            "json",
        ]
        create_step, _ = _run_step(
            "create",
            create_command,
            cwd=None,
            environ=None,
            timeout_seconds=timeout_seconds,
            runner=runner,
        )
        steps.append(create_step)
        if create_step.status != "passed":
            blockers.append(create_step.detail or "Starter creation failed.")
            steps.extend(_skipped_steps(planned_ids[2:]))
            status = "failed"
        else:
            commands = (
                _cpp_commands(
                    target=target,
                    starter=starter,
                    fluentqt_source=fluentqt_source,
                    cmake_prefix_path=cmake_prefix_path,
                    toolchain_file=toolchain_file,
                    configuration=configuration,
                    parallel=parallel,
                )
                if profile == "cpp"
                else _python_commands(target=target, starter=starter)
            )
            status = "passed"
            for index, (step_id, command, cwd, environ) in enumerate(commands):
                step, _ = _run_step(
                    step_id,
                    command,
                    cwd=cwd,
                    environ=environ,
                    timeout_seconds=timeout_seconds,
                    runner=runner,
                )
                steps.append(step)
                if step.status == "passed":
                    continue
                blockers.append(step.detail or f"{step_id} failed.")
                remaining = [value[0] for value in commands[index + 1 :]]
                steps.extend(_skipped_steps(remaining))
                status = "failed"
                break

    total_duration_ms = max(0, round((time.monotonic() - started) * 1000))
    window_step = next(step for step in steps if step.id == "window")
    return {
        "schema_version": 1,
        "trial_version": TRIAL_VERSION,
        "recorded_at": recorded_at,
        "profile": profile,
        "starter": starter,
        "status": status,
        "first_window_reached": window_step.status == "passed",
        "target": str(target.resolve()),
        "kept_workspace": kept_workspace,
        "total_duration_ms": total_duration_ms,
        "host": {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "python": platform.python_version(),
        },
        "doctor": doctor_report,
        "steps": [asdict(step) for step in steps],
        "blockers": blockers,
    }


def render_human(report: Mapping[str, object]) -> str:
    lines = [
        f"FluentQt first-window trial ({report['profile']}-{report['starter']})"
    ]
    raw_steps = report["steps"]
    assert isinstance(raw_steps, list)
    for raw in raw_steps:
        assert isinstance(raw, dict)
        duration = int(raw["duration_ms"]) / 1000
        lines.append(f"[{str(raw['status']).upper():7}] {raw['id']}  {duration:.2f}s")
    total = int(report["total_duration_ms"]) / 1000
    outcome = "Passed" if report["status"] == "passed" else str(report["status"]).title()
    lines.append(f"{outcome} in {total:.2f}s.")
    if report["first_window_reached"]:
        lines.append("The generated application reached its real window show path.")
    blockers = report["blockers"]
    assert isinstance(blockers, list)
    for blocker in blockers:
        lines.append(f"Blocker: {blocker}")
    return "\n".join(lines)


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=("cpp", "python"), default="cpp")
    parser.add_argument(
        "--starter", choices=("existing-qt", "workbench"), default="workbench"
    )
    parser.add_argument(
        "--target", type=Path, help="Keep the generated project at this new path."
    )
    parser.add_argument("--fluentqt-source", type=Path)
    parser.add_argument("--cmake-prefix-path")
    parser.add_argument("--toolchain-file", type=Path)
    parser.add_argument("--config", default="Release")
    parser.add_argument("--parallel", type=int, default=max(1, os.cpu_count() or 1))
    parser.add_argument("--timeout-seconds", type=int, default=600)
    parser.add_argument("--format", choices=("human", "json"), default="human")
    parser.add_argument("--output", type=Path, help="Write the JSON report to a file.")
    parser.add_argument("--version", action="version", version=TRIAL_VERSION)
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    if args.parallel < 1:
        raise SystemExit("--parallel must be positive")
    if args.timeout_seconds < 1:
        raise SystemExit("--timeout-seconds must be positive")
    if args.profile == "python" and (
        args.fluentqt_source or args.cmake_prefix_path or args.toolchain_file
    ):
        raise SystemExit("CMake path options are only valid for the cpp profile")

    if args.target is not None:
        report = run_trial(
            profile=args.profile,
            starter=args.starter,
            target=args.target,
            fluentqt_source=args.fluentqt_source,
            cmake_prefix_path=args.cmake_prefix_path,
            toolchain_file=args.toolchain_file,
            configuration=args.config,
            parallel=args.parallel,
            timeout_seconds=args.timeout_seconds,
            kept_workspace=True,
        )
    else:
        with tempfile.TemporaryDirectory(prefix="fluentqt-first-window-") as temporary:
            report = run_trial(
                profile=args.profile,
                starter=args.starter,
                target=Path(temporary) / "project",
                fluentqt_source=args.fluentqt_source,
                cmake_prefix_path=args.cmake_prefix_path,
                toolchain_file=args.toolchain_file,
                configuration=args.config,
                parallel=args.parallel,
                timeout_seconds=args.timeout_seconds,
                kept_workspace=False,
            )

    encoded = json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    print(encoded if args.format == "json" else render_human(report), end="")
    if args.format == "human":
        print()
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
