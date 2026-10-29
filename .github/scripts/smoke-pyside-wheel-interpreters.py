#!/usr/bin/env python3
"""Smoke-test one stable-ABI FluentQt wheel on each additional CPython."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Any, Sequence


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CATALOG = ROOT / "bindings" / "pyside6" / "wheel-matrix.json"
GALLERY_TAG = "py3-none-any"
# Match the installed-wheel steps: nothing from the build tree or the runner
# image may satisfy Python, Qt, or PySide6 imports.
REMOVED_ENVIRONMENT = (
    "PYTHONHOME",
    "PYTHONPATH",
    "LD_LIBRARY_PATH",
    "DYLD_LIBRARY_PATH",
    "QT_PLUGIN_PATH",
    "QT_QPA_PLATFORM_PLUGIN_PATH",
)
INTERPRETER_PROBE = (
    "import sys; "
    "print(sys.implementation.name, '{0}.{1}'.format(*sys.version_info[:2]))"
)


class SmokeError(RuntimeError):
    """Raised when the wheel or interpreter set violates the release lane."""


def load_scenario(catalog_path: Path, scenario_id: str) -> dict[str, Any]:
    try:
        catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise SmokeError(f"cannot read wheel matrix {catalog_path}: {error}") from error
    scenarios = catalog.get("scenarios") if isinstance(catalog, dict) else None
    if not isinstance(scenarios, list):
        raise SmokeError("wheel matrix must contain a scenarios array")
    matches = [
        scenario
        for scenario in scenarios
        if isinstance(scenario, dict) and scenario.get("id") == scenario_id
    ]
    if len(matches) != 1:
        raise SmokeError(f"wheel matrix must define scenario {scenario_id!r} once")
    scenario = matches[0]
    if scenario.get("release") is not True or scenario.get("stable_abi") is not True:
        raise SmokeError(f"scenario {scenario_id!r} is not a stable-ABI release lane")
    versions = scenario.get("test_python_versions")
    if not isinstance(versions, list) or not all(
        isinstance(version, str) for version in versions
    ):
        raise SmokeError(f"scenario {scenario_id!r} has no test_python_versions")
    return scenario


def single_wheel(directory: Path, prefix: str, suffix: str) -> Path:
    if not directory.is_dir():
        raise SmokeError(f"wheel directory does not exist: {directory}")
    candidates = sorted(directory.glob(f"{prefix}-*.whl"))
    if len(candidates) != 1 or not candidates[0].name.endswith(f"-{suffix}.whl"):
        raise SmokeError(
            f"expected one {prefix} wheel tagged {suffix} in {directory}, "
            f"found {[path.name for path in candidates]}"
        )
    return candidates[0]


def parse_interpreters(values: Sequence[str]) -> dict[str, Path]:
    interpreters: dict[str, Path] = {}
    for value in values:
        version, separator, path = value.partition("=")
        if not separator or not version:
            raise SmokeError(f"interpreter must be VERSION=PATH: {value!r}")
        if not path:
            # A skipped setup-python step leaves its python-path output empty.
            continue
        if version in interpreters:
            raise SmokeError(f"duplicate interpreter for CPython {version}")
        interpreters[version] = Path(path)
    return interpreters


def required_versions(scenario: dict[str, Any]) -> list[str]:
    """Return interpreters not already covered by the build-interpreter steps."""

    build_version = scenario.get("python_version")
    versions = list(scenario["test_python_versions"])
    if not versions or versions[0] != build_version:
        raise SmokeError(
            f"scenario {scenario.get('id')!r} must build with its oldest "
            f"tested CPython, found {build_version!r} for {versions}"
        )
    return versions[1:]


def venv_python(venv: Path) -> Path:
    if os.name == "nt":
        return venv / "Scripts" / "python.exe"
    return venv / "bin" / "python"


def clean_environment(base: dict[str, str], version: str) -> dict[str, str]:
    environment = {
        name: value for name, value in base.items() if name not in REMOVED_ENVIRONMENT
    }
    environment["QT_QPA_PLATFORM"] = "offscreen"
    environment["FLUENTQT_EXPECTED_VERSION"] = version
    return environment


def run(command: Sequence[str | os.PathLike[str]], **kwargs: Any) -> str:
    completed = subprocess.run(
        [str(part) for part in command],
        check=True,
        stdout=subprocess.PIPE,
        universal_newlines=True,
        **kwargs,
    )
    output = completed.stdout
    if output:
        sys.stdout.write(output)
        sys.stdout.flush()
    return output.strip()


def probe_interpreter(python: Path, version: str) -> None:
    lines = run([python, "-c", INTERPRETER_PROBE]).splitlines()
    actual = lines[-1].strip() if lines else ""
    if actual != f"cpython {version}":
        raise SmokeError(f"{python} reports {actual!r}, expected 'cpython {version}'")


def smoke_interpreter(
    *,
    version: str,
    python: Path,
    core_wheel: Path,
    gallery_wheel: Path,
    work_dir: Path,
    project_root: Path,
) -> None:
    probe_interpreter(python, version)
    venv = work_dir / ("cp" + version.replace(".", ""))
    if venv.exists():
        shutil.rmtree(venv)
    run([python, "-m", "venv", venv])
    clean_python = venv_python(venv)
    run(
        [
            clean_python,
            "-m",
            "pip",
            "install",
            "--disable-pip-version-check",
            core_wheel,
            gallery_wheel,
        ]
    )
    installed_version = run(
        [
            clean_python,
            "-c",
            "from importlib.metadata import version; print(version('FluentQt'))",
        ]
    ).splitlines()[-1]
    environment = clean_environment(dict(os.environ), installed_version)
    run(
        [clean_python, project_root / "bindings/pyside6/tests/test_wheel_smoke.py"],
        cwd=project_root,
        env=environment,
    )
    run(
        [
            clean_python,
            project_root / "bindings/pyside6/gallery/tests/test_gallery_wheel_smoke.py",
            "--project-root",
            project_root,
        ],
        cwd=project_root,
        env=environment,
    )
    run([clean_python, "-m", "pip", "check"], env=environment)
    print(
        f"CPython {version}: {core_wheel.name} passed the core and Gallery "
        "installed-wheel smoke"
    )


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", required=True, help="wheel-matrix scenario id")
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    parser.add_argument(
        "--wheel-dir",
        type=Path,
        required=True,
        help="directory holding the publishable FluentQt wheel",
    )
    parser.add_argument("--gallery-wheel-dir", type=Path, required=True)
    parser.add_argument(
        "--interpreter",
        action="append",
        default=[],
        metavar="VERSION=PATH",
        help="CPython executable for one additional test version; empty paths are ignored",
    )
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--project-root", type=Path, default=ROOT)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        scenario = load_scenario(args.catalog, args.scenario)
        core_wheel = single_wheel(
            args.wheel_dir, "fluentqt", str(scenario.get("publish_wheel_suffix", ""))
        )
        gallery_wheel = single_wheel(
            args.gallery_wheel_dir, "fluentqt_gallery", GALLERY_TAG
        )
        versions = required_versions(scenario)
        interpreters = parse_interpreters(args.interpreter)
        if sorted(interpreters) != sorted(versions):
            raise SmokeError(
                f"scenario {args.scenario} requires interpreters for {versions}, "
                f"received {sorted(interpreters)}"
            )
        args.work_dir.mkdir(parents=True, exist_ok=True)
        for version in versions:
            smoke_interpreter(
                version=version,
                python=interpreters[version],
                core_wheel=core_wheel.resolve(),
                gallery_wheel=gallery_wheel.resolve(),
                work_dir=args.work_dir.resolve(),
                project_root=args.project_root.resolve(),
            )
    except (SmokeError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as error:
        print(f"error: command failed with exit code {error.returncode}: {error.cmd}", file=sys.stderr)
        return error.returncode or 1
    covered = scenario["python_version"]
    print(
        f"Verified {core_wheel.name} on CPython {', '.join(versions) or 'none'} "
        f"in addition to build interpreter {covered}."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
