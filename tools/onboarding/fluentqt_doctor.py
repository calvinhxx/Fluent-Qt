#!/usr/bin/env python3

"""Check whether the current machine can start a FluentQt consumer project."""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import importlib.metadata
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from typing import Callable, Iterable, Mapping, Sequence


DOCTOR_VERSION = "0.2.0"
MINIMUM_CMAKE = (3, 16, 0)
PUBLISHED_PYTHON_MINIMUM = (3, 11)
PUBLISHED_PYTHON_MAXIMUM = (3, 13)
SOURCE_PYTHON_MINIMUM = (3, 10)


@dataclass(frozen=True)
class CommandOutput:
    returncode: int
    output: str
    timed_out: bool = False


@dataclass(frozen=True)
class CheckResult:
    id: str
    status: str
    summary: str
    detail: str = ""
    hint: str = ""


Runner = Callable[[Sequence[str], int], CommandOutput]
Which = Callable[[str], str | None]
ModuleFinder = Callable[[str], object | None]
VersionReader = Callable[[str], str]


def run_command(command: Sequence[str], timeout_seconds: int) -> CommandOutput:
    """Run one local probe without invoking a shell or network command."""
    try:
        completed = subprocess.run(
            list(command),
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


def _version_tuple(value: str) -> tuple[int, int, int] | None:
    match = re.search(r"(\d+)\.(\d+)(?:\.(\d+))?", value)
    if match is None:
        return None
    return tuple(int(part or 0) for part in match.groups())


def _tail(value: str, line_count: int = 8) -> str:
    lines = [line.strip() for line in value.splitlines() if line.strip()]
    return "\n".join(lines[-line_count:])


def _find_command(command: str, which: Which) -> str | None:
    tokens = shlex.split(command, posix=os.name != "nt")
    if not tokens:
        return None
    candidate = Path(tokens[0]).expanduser()
    if candidate.is_absolute():
        return str(candidate) if candidate.exists() else None
    return which(tokens[0])


def check_cmake(which: Which, runner: Runner, timeout_seconds: int) -> tuple[CheckResult, str | None]:
    cmake = which("cmake")
    if cmake is None:
        return (
            CheckResult(
                "cmake",
                "fail",
                "CMake was not found.",
                hint="Install CMake 3.16 or newer and make it available on PATH.",
            ),
            None,
        )

    output = runner([cmake, "--version"], timeout_seconds)
    if output.timed_out:
        return (
            CheckResult(
                "cmake",
                "fail",
                "CMake version check timed out.",
                detail=output.output,
                hint=(
                    "Retry with a larger --timeout-seconds value and verify that "
                    "the selected CMake command starts normally."
                ),
            ),
            None,
        )
    version = _version_tuple(output.output)
    if output.returncode != 0 or version is None:
        return (
            CheckResult(
                "cmake",
                "fail",
                "CMake could not be queried.",
                detail=_tail(output.output),
                hint="Run `cmake --version` and repair the selected CMake installation.",
            ),
            None,
        )
    if version < MINIMUM_CMAKE:
        return (
            CheckResult(
                "cmake",
                "fail",
                f"CMake {'.'.join(map(str, version))} is too old.",
                hint="Install CMake 3.16 or newer.",
            ),
            None,
        )
    return (
        CheckResult(
            "cmake",
            "pass",
            f"CMake {'.'.join(map(str, version))}",
            detail=cmake,
        ),
        cmake,
    )


def check_compiler(environ: Mapping[str, str], which: Which) -> CheckResult:
    configured = environ.get("CXX", "").strip()
    if configured:
        compiler = _find_command(configured, which)
        if compiler is not None:
            return CheckResult(
                "cxx_compiler",
                "pass",
                f"C++ compiler from CXX: {configured}",
                detail=compiler,
            )
        return CheckResult(
            "cxx_compiler",
            "fail",
            f"CXX points to an unavailable command: {configured}",
            hint="Fix CXX or remove it so CMake can select the platform compiler.",
        )

    candidates = (
        ("cl.exe", "Microsoft C++"),
        ("clang++", "Clang"),
        ("g++", "GCC"),
        ("c++", "C++"),
    )
    for command, label in candidates:
        compiler = which(command)
        if compiler is not None:
            return CheckResult(
                "cxx_compiler",
                "pass",
                f"{label} compiler found",
                detail=compiler,
            )
    return CheckResult(
        "cxx_compiler",
        "warn",
        "No standalone C++ compiler command was found on PATH.",
        hint=(
            "The CMake probe will still check its selected generator. If it also "
            "fails, install Xcode Command Line Tools, Visual Studio Build Tools, "
            "or a supported GCC/Clang toolchain."
        ),
    )


def discover_qt_prefix(
    explicit_prefix: str | None,
    environ: Mapping[str, str],
    which: Which,
    runner: Runner,
    timeout_seconds: int,
) -> tuple[str | None, str]:
    if explicit_prefix:
        return explicit_prefix, "--cmake-prefix-path"
    if environ.get("CMAKE_PREFIX_PATH"):
        return environ["CMAKE_PREFIX_PATH"], "CMAKE_PREFIX_PATH"

    for variable in ("Qt6_DIR", "Qt5_DIR"):
        configured = environ.get(variable)
        if configured:
            path = Path(configured).expanduser()
            parents = path.parents
            prefix = parents[2] if len(parents) > 2 else path
            return str(prefix), variable

    for name in ("qmake6", "qmake-qt6", "qmake", "qtpaths6", "qtpaths"):
        command = which(name)
        if command is None:
            continue
        query = [command, "-query", "QT_INSTALL_PREFIX"]
        if name.startswith("qtpaths"):
            query = [command, "--query", "QT_INSTALL_PREFIX"]
        output = runner(query, timeout_seconds)
        prefix = output.output.strip().splitlines()[-1] if output.output.strip() else ""
        if output.returncode == 0 and prefix and Path(prefix).expanduser().exists():
            return str(Path(prefix).expanduser()), name
    return None, "CMake default search"


def check_qt_widgets(
    *,
    cmake: str | None,
    prefix: str | None,
    prefix_source: str,
    toolchain_file: str | None,
    probe_source: Path,
    runner: Runner,
    timeout_seconds: int,
) -> CheckResult:
    if cmake is None:
        return CheckResult(
            "qt_widgets",
            "fail",
            "Qt Widgets discovery was skipped because CMake is unavailable.",
        )
    if toolchain_file and not Path(toolchain_file).expanduser().is_file():
        return CheckResult(
            "qt_widgets",
            "fail",
            "The requested CMake toolchain file does not exist.",
            detail=str(Path(toolchain_file).expanduser()),
            hint="Pass an existing file with --toolchain-file or omit the option.",
        )
    if not (probe_source / "CMakeLists.txt").is_file():
        return CheckResult(
            "qt_widgets",
            "fail",
            "The bundled Qt Widgets probe is missing.",
            detail=str(probe_source),
        )

    with tempfile.TemporaryDirectory(prefix="fluentqt-doctor-") as temporary:
        command = [
            cmake,
            "-S",
            str(probe_source),
            "-B",
            str(Path(temporary) / "build"),
        ]
        if prefix:
            command.append(f"-DCMAKE_PREFIX_PATH={prefix}")
        if toolchain_file:
            command.append(
                f"-DCMAKE_TOOLCHAIN_FILE={Path(toolchain_file).expanduser().resolve()}"
            )
        output = runner(command, timeout_seconds)

    if output.timed_out:
        retry_timeout = max(120, timeout_seconds * 2)
        return CheckResult(
            "qt_widgets",
            "fail",
            "The Qt Widgets CMake probe timed out.",
            detail=output.output,
            hint=(
                f"Retry with --timeout-seconds {retry_timeout}. If it still "
                "times out, run the reported CMake configure step directly and "
                "check the selected toolchain or Qt package provider."
            ),
        )
    if output.returncode != 0:
        return CheckResult(
            "qt_widgets",
            "fail",
            "CMake could not find a supported Qt Widgets installation.",
            detail=_tail(output.output),
            hint=(
                "Install Qt 5.15+ or 6.2+, then pass its prefix with "
                "--cmake-prefix-path or configure CMAKE_PREFIX_PATH."
            ),
        )

    fields: dict[str, str] = {}
    for key in ("QT_MAJOR", "QT_VERSION", "QT_DIR", "CXX_COMPILER"):
        match = re.search(rf"FLUENTQT_DOCTOR_{key}=([^\r\n]+)", output.output)
        if match:
            fields[key] = match.group(1).strip()
    version = fields.get("QT_VERSION", "unknown version")
    major = fields.get("QT_MAJOR", "?")
    detail_parts = [f"discovery: {prefix_source}"]
    if fields.get("QT_DIR"):
        detail_parts.append(f"Qt directory: {fields['QT_DIR']}")
    if fields.get("CXX_COMPILER"):
        detail_parts.append(f"compiler: {fields['CXX_COMPILER']}")
    parsed_version = _version_tuple(version)
    supported = (
        major == "5" and parsed_version is not None and parsed_version >= (5, 15, 0)
    ) or (
        major == "6" and parsed_version is not None and parsed_version >= (6, 2, 0)
    )
    if not supported:
        return CheckResult(
            "qt_widgets",
            "fail",
            f"Qt {major} Widgets {version} is outside the supported range.",
            detail="\n".join(detail_parts),
            hint="Use Qt 5.15+ or Qt 6.2+.",
        )
    return CheckResult(
        "qt_widgets",
        "pass",
        f"Qt {major} Widgets {version}",
        detail="\n".join(detail_parts),
    )


def _read_distribution_version(distribution: str) -> str:
    return importlib.metadata.version(distribution)


def check_python_version(
    version: tuple[int, int, int],
) -> CheckResult:
    short = version[:2]
    display = ".".join(map(str, version))
    if PUBLISHED_PYTHON_MINIMUM <= short <= PUBLISHED_PYTHON_MAXIMUM:
        return CheckResult(
            "python_runtime",
            "pass",
            f"Python {display} is covered by published FluentQt wheels.",
        )
    if short == SOURCE_PYTHON_MINIMUM:
        return CheckResult(
            "python_runtime",
            "warn",
            f"Python {display} is source-build only.",
            hint="Use Python 3.11–3.13 for a published wheel.",
        )
    return CheckResult(
        "python_runtime",
        "fail",
        f"Python {display} is outside the supported range.",
        hint="Use Python 3.11–3.13, or Python 3.10 for a source build.",
    )


def check_python_package(
    *,
    check_id: str,
    module: str,
    distribution: str,
    module_finder: ModuleFinder,
    version_reader: VersionReader,
) -> CheckResult:
    try:
        specification = module_finder(module)
    except (ImportError, ModuleNotFoundError, ValueError):
        specification = None
    if specification is None:
        return CheckResult(
            check_id,
            "fail",
            f"Python module `{module}` is not installed.",
            hint="Run `python -m pip install FluentQt` in the selected environment.",
        )
    try:
        version = version_reader(distribution)
    except importlib.metadata.PackageNotFoundError:
        version = "unknown version"
    return CheckResult(
        check_id,
        "pass",
        f"{distribution} {version}",
    )


def run_doctor(
    *,
    profile: str,
    cmake_prefix_path: str | None = None,
    toolchain_file: str | None = None,
    timeout_seconds: int = 60,
    environ: Mapping[str, str] | None = None,
    which: Which = shutil.which,
    runner: Runner = run_command,
    module_finder: ModuleFinder = importlib.util.find_spec,
    version_reader: VersionReader = _read_distribution_version,
    python_version: tuple[int, int, int] | None = None,
    probe_source: Path | None = None,
) -> dict[str, object]:
    environment = os.environ if environ is None else environ
    checks: list[CheckResult]
    if profile == "cpp":
        cmake_result, cmake = check_cmake(which, runner, timeout_seconds)
        prefix, prefix_source = discover_qt_prefix(
            cmake_prefix_path,
            environment,
            which,
            runner,
            timeout_seconds,
        )
        checks = [
            cmake_result,
            check_compiler(environment, which),
            check_qt_widgets(
                cmake=cmake,
                prefix=prefix,
                prefix_source=prefix_source,
                toolchain_file=toolchain_file,
                probe_source=(
                    probe_source
                    if probe_source is not None
                    else Path(__file__).resolve().parent / "qt_widgets_probe"
                ),
                runner=runner,
                timeout_seconds=timeout_seconds,
            ),
        ]
    elif profile == "python":
        active_version = python_version or (
            sys.version_info.major,
            sys.version_info.minor,
            sys.version_info.micro,
        )
        checks = [
            check_python_version(active_version),
            check_python_package(
                check_id="pyside6",
                module="PySide6",
                distribution="PySide6",
                module_finder=module_finder,
                version_reader=version_reader,
            ),
            check_python_package(
                check_id="fluentqt",
                module="fluentqt",
                distribution="FluentQt",
                module_finder=module_finder,
                version_reader=version_reader,
            ),
        ]
    else:
        raise ValueError(f"Unsupported profile: {profile}")

    failures = sum(check.status == "fail" for check in checks)
    warnings = sum(check.status == "warn" for check in checks)
    return {
        "schema_version": 1,
        "doctor_version": DOCTOR_VERSION,
        "profile": profile,
        "ready": failures == 0,
        "host": {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
        },
        "summary": {
            "passed": sum(check.status == "pass" for check in checks),
            "warnings": warnings,
            "failures": failures,
        },
        "checks": [asdict(check) for check in checks],
    }


def render_human(report: Mapping[str, object]) -> str:
    profile = str(report["profile"])
    lines = [f"FluentQt doctor ({profile})"]
    checks = report["checks"]
    assert isinstance(checks, list)
    for raw in checks:
        assert isinstance(raw, dict)
        status = str(raw["status"]).upper().ljust(4)
        lines.append(f"[{status}] {raw['summary']}")
        detail = str(raw.get("detail", ""))
        hint = str(raw.get("hint", ""))
        for detail_line in detail.splitlines():
            lines.append(f"       {detail_line}")
        if hint:
            lines.append(f"       Fix: {hint}")
    summary = report["summary"]
    assert isinstance(summary, dict)
    if report["ready"]:
        outcome = "Ready"
        if summary["warnings"]:
            outcome += " with warnings"
    else:
        outcome = "Blocked"
    lines.append(
        f"{outcome}: {summary['passed']} passed, "
        f"{summary['warnings']} warnings, {summary['failures']} failures."
    )
    return "\n".join(lines)


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--profile",
        choices=("cpp", "python"),
        default="cpp",
        help="Consumer environment to check (default: cpp).",
    )
    parser.add_argument(
        "--format",
        choices=("human", "json"),
        default="human",
        help="Output format (default: human).",
    )
    parser.add_argument(
        "--cmake-prefix-path",
        help="Qt installation prefix passed to the CMake consumer probe.",
    )
    parser.add_argument(
        "--toolchain-file",
        help="Optional CMake toolchain file passed to the C++ probe.",
    )
    parser.add_argument(
        "--timeout-seconds",
        type=int,
        default=60,
        help="Timeout for each local command (default: 60).",
    )
    parser.add_argument("--version", action="version", version=DOCTOR_VERSION)
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    if args.timeout_seconds < 1:
        raise SystemExit("--timeout-seconds must be positive")
    report = run_doctor(
        profile=args.profile,
        cmake_prefix_path=args.cmake_prefix_path,
        toolchain_file=args.toolchain_file,
        timeout_seconds=args.timeout_seconds,
    )
    if args.format == "json":
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(render_human(report))
    return 0 if report["ready"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
