#!/usr/bin/env python3
"""Launch FluentQt Live Scene with the checkout's matching PySide6 runtime."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path
import subprocess
import sys
from typing import Iterable


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_PACKAGE_ROOT = PROJECT_ROOT / "bindings" / "pyside6" / "gallery" / "src"
CATALOG_EXPORTER = Path(__file__).with_name("fluent_qt_catalog_scene.py")
LIVE_HOST = Path(__file__).with_name("fluent_qt_live_host.py")
PREFERRED_BUILD_NAMES = (
    "pyside6-local",
    "pyside6-6.9.3",
    "pyside6-6.11.1",
    "pyside6",
)


@dataclass(frozen=True)
class PySideRuntime:
    build_directory: Path
    python_executable: Path
    python_path: Path


def parse_size(value: str) -> str:
    pieces = value.lower().split("x", 1)
    if len(pieces) != 2:
        raise argparse.ArgumentTypeError("size must be WIDTHxHEIGHT")
    try:
        width, height = (int(piece) for piece in pieces)
    except ValueError as error:
        raise argparse.ArgumentTypeError("size must be WIDTHxHEIGHT") from error
    if not 520 <= width <= 3840 or not 420 <= height <= 2160:
        raise argparse.ArgumentTypeError(
            "size must be within 520x420 and 3840x2160"
        )
    return "{0}x{1}".format(width, height)


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    discovery = parser.add_mutually_exclusive_group()
    discovery.add_argument(
        "--list-routes",
        action="store_true",
        help="Print every catalog-backed Live Scene route as JSON.",
    )
    discovery.add_argument(
        "--list-samples",
        action="store_true",
        help="Print sample ids for --route as JSON.",
    )
    source = parser.add_mutually_exclusive_group()
    source.add_argument(
        "--scene",
        type=Path,
        help="Python scene exporting build(parent) -> QWidget.",
    )
    source.add_argument(
        "--route",
        help="Gallery component route; materializes its real PySide6 sample.",
    )
    parser.add_argument(
        "--sample",
        help="Gallery sample id; defaults to the route's first sample.",
    )
    parser.add_argument(
        "--fork-scene",
        nargs="?",
        const="",
        metavar="PATH",
        help=(
            "Create and preview a user-owned editable catalog scene. A bare "
            "flag writes under the selected build's preview/scenes directory."
        ),
    )
    parser.add_argument("--theme", choices=("light", "dark"), default="light")
    parser.add_argument("--size", type=parse_size, default="920x680")
    parser.add_argument("--rtl", action="store_true")
    parser.add_argument("--no-watch", action="store_true")
    parser.add_argument("--snapshot", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--settle-ms", type=int, default=220)
    parser.add_argument("--keep-open", action="store_true")
    parser.add_argument(
        "--build-dir",
        type=Path,
        help="Configured PySide6 build containing python/fluentqt.",
    )
    parser.add_argument(
        "--python",
        dest="python_executable",
        type=Path,
        help="Matching Python interpreter; otherwise read it from CMakeCache.",
    )
    args = parser.parse_args(list(argv) if argv is not None else None)
    if args.list_routes and (args.scene or args.route or args.sample):
        parser.error("--list-routes cannot be combined with a scene selection")
    if args.list_samples and not args.route:
        parser.error("--list-samples requires --route")
    if args.list_samples and (args.scene or args.sample):
        parser.error("--list-samples cannot be combined with --scene or --sample")
    if not args.list_routes and not args.list_samples and not (
        args.scene or args.route
    ):
        parser.error("one of --scene or --route is required")
    if args.sample and not args.route:
        parser.error("--sample requires --route")
    if args.fork_scene is not None and not args.route:
        parser.error("--fork-scene requires --route")
    if (args.list_routes or args.list_samples) and args.fork_scene is not None:
        parser.error("--fork-scene cannot be combined with catalog listing")
    if not 0 <= args.settle_ms <= 10000:
        parser.error("--settle-ms must be between 0 and 10000")
    return args


def _binding_extensions(python_path: Path) -> list[Path]:
    package = python_path / "fluentqt"
    extensions: list[Path] = []
    for pattern in ("_fluentqt*.so", "_fluentqt*.dylib", "_fluentqt*.pyd"):
        extensions.extend(package.glob(pattern))
    return sorted(path for path in extensions if path.is_file())


def candidate_build_directories(
    project_root: Path = PROJECT_ROOT,
) -> tuple[Path, ...]:
    build_root = project_root / "build"
    candidates: list[Path] = []
    seen: set[Path] = set()
    for name in PREFERRED_BUILD_NAMES:
        candidate = build_root / name
        if candidate not in seen:
            candidates.append(candidate)
            seen.add(candidate)
    if build_root.is_dir():
        discovered = sorted(
            (
                path
                for path in build_root.glob("pyside6*")
                if path.is_dir() and path not in seen
            ),
            key=lambda path: path.stat().st_mtime_ns,
            reverse=True,
        )
        candidates.extend(discovered)
    return tuple(candidates)


def resolve_build_directory(
    explicit: Path | None,
    *,
    project_root: Path = PROJECT_ROOT,
) -> Path:
    if explicit is not None:
        candidates = (explicit.expanduser().resolve(),)
    else:
        candidates = candidate_build_directories(project_root)
    for candidate in candidates:
        python_path = candidate / "python"
        if _binding_extensions(python_path):
            return candidate.resolve()
    searched = "\n  ".join(str(path) for path in candidates)
    raise FileNotFoundError(
        "No built FluentQt PySide6 extension was found. Searched:\n  {0}\n"
        "Configure and build the PySide6 bindings, or pass --build-dir.".format(
            searched or "(no pyside6 build directories)"
        )
    )


def cached_python_executable(build_directory: Path) -> Path | None:
    cache = build_directory / "CMakeCache.txt"
    if not cache.is_file():
        return None
    values: dict[str, str] = {}
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if "=" not in line or ":" not in line:
            continue
        key_and_type, value = line.split("=", 1)
        key = key_and_type.split(":", 1)[0]
        if key in {"Python_EXECUTABLE", "_Python_EXECUTABLE"}:
            values[key] = value
    for key in ("Python_EXECUTABLE", "_Python_EXECUTABLE"):
        value = values.get(key)
        if value:
            candidate = Path(value).expanduser()
            if candidate.is_file():
                # Keep a virtual-environment launcher path intact. Resolving
                # its symlink to the base interpreter loses pyvenv.cfg and
                # therefore the matching PySide6 site-packages.
                return candidate.absolute()
    return None


def resolve_python_executable(
    build_directory: Path,
    explicit: Path | None,
) -> Path:
    if explicit is not None:
        candidate = explicit.expanduser().absolute()
        if not candidate.is_file():
            raise FileNotFoundError(
                "Python interpreter does not exist: {0}".format(candidate)
            )
        return candidate
    cached = cached_python_executable(build_directory)
    if cached is not None:
        return cached
    raise FileNotFoundError(
        "CMakeCache.txt does not name a usable Python interpreter for {0}; "
        "pass --python.".format(build_directory)
    )


def resolve_runtime(
    build_directory: Path | None,
    python_executable: Path | None,
    *,
    project_root: Path = PROJECT_ROOT,
) -> PySideRuntime:
    if build_directory is not None:
        build = resolve_build_directory(
            build_directory, project_root=project_root
        )
        interpreter = resolve_python_executable(build, python_executable)
    else:
        build = None
        interpreter = None
        for candidate in candidate_build_directories(project_root):
            if not _binding_extensions(candidate / "python"):
                continue
            resolved_python = (
                resolve_python_executable(candidate, python_executable)
                if python_executable is not None
                else cached_python_executable(candidate)
            )
            if resolved_python is None:
                continue
            build = candidate.resolve()
            interpreter = resolved_python
            break
        if build is None or interpreter is None:
            raise FileNotFoundError(
                "No built FluentQt PySide6 extension has a usable matching "
                "Python interpreter. Pass --build-dir and --python."
            )
    return PySideRuntime(
        build_directory=build,
        python_executable=interpreter,
        python_path=(build / "python").resolve(),
    )


def load_catalog_contract(runtime: PySideRuntime) -> dict[str, object]:
    path = runtime.python_path / "fluentqt_gallery" / "contract.json"
    if not path.is_file():
        raise FileNotFoundError(
            "PySide6 Gallery contract is missing: {0}. Configure the selected "
            "build with FLUENT_QT_BUILD_PYSIDE6_GALLERY=ON and rebuild.".format(
                path
            )
        )
    try:
        contract = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RuntimeError(
            "Unable to read PySide6 Gallery contract {0}: {1}".format(path, error)
        ) from error
    if contract.get("schema_version") != 1 or not isinstance(
        contract.get("components"), list
    ):
        raise RuntimeError("Unsupported PySide6 Gallery contract: {0}".format(path))
    return contract


def catalog_routes_document(contract: dict[str, object]) -> dict[str, object]:
    routes = []
    for component in contract["components"]:
        samples = component.get("samples", [])
        if not samples:
            continue
        routes.append(
            {
                "id": component["id"],
                "title": component["title"],
                "samples": [sample["id"] for sample in samples],
            }
        )
    return {
        "schema_version": 1,
        "tool": "FluentQt Live Scene",
        "source": "C++ Gallery catalog via PySide6 contract",
        "routes": routes,
    }


def resolve_catalog_selection(
    contract: dict[str, object], route: str, sample: str | None
) -> tuple[dict[str, object], dict[str, object]]:
    component = next(
        (
            candidate
            for candidate in contract["components"]
            if candidate.get("id") == route
        ),
        None,
    )
    if component is None:
        raise ValueError("Unknown Gallery route: {0}".format(route))
    samples = component.get("samples", [])
    if not samples:
        raise ValueError("Gallery route has no previewable samples: {0}".format(route))
    selected_id = sample or samples[0]["id"]
    selected = next(
        (candidate for candidate in samples if candidate.get("id") == selected_id),
        None,
    )
    if selected is None:
        available = ", ".join(candidate["id"] for candidate in samples)
        raise ValueError(
            "Unknown Gallery sample {0}/{1}; available: {2}".format(
                route, selected_id, available
            )
        )
    return component, selected


def catalog_samples_document(
    contract: dict[str, object], route: str
) -> dict[str, object]:
    component, _sample = resolve_catalog_selection(contract, route, None)
    return {
        "schema_version": 1,
        "tool": "FluentQt Live Scene",
        "source": "C++ Gallery catalog via PySide6 contract",
        "route": route,
        "samples": [sample["id"] for sample in component["samples"]],
    }


def catalog_scene_paths(
    args: argparse.Namespace,
    runtime: PySideRuntime,
    component: dict[str, object],
    sample: dict[str, object],
) -> tuple[Path, str]:
    route_id = str(component["id"])
    sample_id = str(sample["id"])
    if args.fork_scene is None:
        scene = (
            runtime.build_directory
            / "preview"
            / "live-catalog"
            / route_id
            / "{0}.preview.py".format(sample_id)
        )
        mode = "managed"
    else:
        scene = (
            runtime.build_directory
            / "preview"
            / "scenes"
            / "{0}--{1}.preview.py".format(route_id, sample_id)
            if args.fork_scene == ""
            else Path(args.fork_scene).expanduser().resolve()
        )
        mode = "fork"
    scene = scene.resolve()
    return scene, mode


def materialize_catalog_scene(
    args: argparse.Namespace,
    runtime: PySideRuntime,
    component: dict[str, object],
    sample: dict[str, object],
) -> Path:
    scene, mode = catalog_scene_paths(args, runtime, component, sample)
    if mode == "fork" and scene.exists():
        if not scene.is_file():
            raise OSError("Catalog scene path is not a file: {0}".format(scene))
        print("[live-scene] reusing editable fork: {0}".format(scene), flush=True)
        return scene

    environment = runtime_environment(runtime, source_first=False)
    environment.setdefault("QT_QPA_PLATFORM", "offscreen")
    command = [
        str(runtime.python_executable),
        str(CATALOG_EXPORTER),
        "--route",
        str(component["id"]),
        "--sample",
        str(sample["id"]),
        "--output",
        str(scene),
    ]
    completed = subprocess.run(
        command,
        cwd=PROJECT_ROOT,
        env=environment,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            "Catalog scene export failed for {0}/{1}".format(
                component["id"], sample["id"]
            )
        )
    if mode == "fork":
        print(
            "[live-scene] edit this file and save; the window will update: {0}".format(
                scene
            ),
            flush=True,
        )
    else:
        print(
            "[live-scene] managed scene; regenerated on next launch "
            "(use --fork-scene to keep edits): {0}".format(scene),
            flush=True,
        )
    return scene


def host_arguments(args: argparse.Namespace) -> list[str]:
    if args.scene is None:
        raise ValueError("A materialized --scene is required before launch")
    command = [
        str(LIVE_HOST),
        "--scene",
        str(args.scene.expanduser().resolve()),
        "--theme",
        args.theme,
        "--size",
        args.size,
        "--settle-ms",
        str(args.settle_ms),
    ]
    if args.rtl:
        command.append("--rtl")
    if args.no_watch:
        command.append("--no-watch")
    if args.snapshot is not None:
        command.extend(
            ["--snapshot", str(args.snapshot.expanduser().resolve())]
        )
    if args.report is not None:
        command.extend(["--report", str(args.report.expanduser().resolve())])
    if args.keep_open:
        command.append("--keep-open")
    return command


def runtime_environment(
    runtime: PySideRuntime,
    *,
    base: dict[str, str] | None = None,
    source_package_root: Path = SOURCE_PACKAGE_ROOT,
    source_first: bool = True,
) -> dict[str, str]:
    environment = dict(os.environ if base is None else base)
    source_path = str(source_package_root.resolve())
    built_path = str(runtime.python_path)
    python_paths = (
        [source_path, built_path] if source_first else [built_path, source_path]
    )
    existing = environment.get("PYTHONPATH")
    if existing:
        python_paths.append(existing)
    environment["PYTHONPATH"] = os.pathsep.join(python_paths)
    environment["PYTHONDONTWRITEBYTECODE"] = "1"
    return environment


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        runtime = resolve_runtime(args.build_dir, args.python_executable)
        if args.list_routes or args.list_samples or args.route:
            contract = load_catalog_contract(runtime)
            if args.list_routes:
                print(
                    json.dumps(
                        catalog_routes_document(contract),
                        indent=2,
                        ensure_ascii=False,
                    )
                )
                return 0
            if args.list_samples:
                print(
                    json.dumps(
                        catalog_samples_document(contract, args.route),
                        indent=2,
                        ensure_ascii=False,
                    )
                )
                return 0
            component, sample = resolve_catalog_selection(
                contract, args.route, args.sample
            )
            args.sample = sample["id"]
            args.scene = materialize_catalog_scene(
                args, runtime, component, sample
            )
        environment = runtime_environment(runtime)
        command = [
            str(runtime.python_executable),
            *host_arguments(args),
        ]
        os.chdir(PROJECT_ROOT)
        os.execve(str(runtime.python_executable), command, environment)
    except (FileNotFoundError, OSError, RuntimeError, ValueError) as error:
        print("fluent_qt_live_preview: {0}".format(error), file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
