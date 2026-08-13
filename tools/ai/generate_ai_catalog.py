#!/usr/bin/env python3

"""Generate the public FluentQt catalog used by coding agents and tooling."""

from __future__ import annotations

import argparse
import ast
import difflib
import importlib.util
import json
from pathlib import Path
import re
import sys
from types import ModuleType
from typing import Iterable


CATEGORY_API = {
    "basic-input": {
        "cpp_header": "BasicInput.h",
        "cpp_namespace": "fluent::basicinput",
        "python_module": "basicinput",
        "source_directory": "basicinput",
        "sample_source": "BasicInputSamples.cpp",
    },
    "collections": {
        "cpp_header": "Collections.h",
        "cpp_namespace": "fluent::collections",
        "python_module": "collections",
        "source_directory": "collections",
        "sample_source": "CollectionsSamples.cpp",
    },
    "date-time": {
        "cpp_header": "DateTime.h",
        "cpp_namespace": "fluent::date_time",
        "python_module": "date_time",
        "source_directory": "date_time",
        "sample_source": "DateTimeSamples.cpp",
    },
    "dialogs-flyouts": {
        "cpp_header": "DialogsFlyouts.h",
        "cpp_namespace": "fluent::dialogs_flyouts",
        "python_module": "dialogs_flyouts",
        "source_directory": "dialogs_flyouts",
        "sample_source": "DialogsFlyoutsSamples.cpp",
    },
    "layout": {
        "cpp_header": "Layout.h",
        "cpp_namespace": "fluent::layout",
        "python_module": "layout",
        "source_directory": "layout",
        "sample_source": "LayoutSamples.cpp",
    },
    "menus-toolbars": {
        "cpp_header": "MenusToolbars.h",
        "cpp_namespace": "fluent::menus_toolbars",
        "python_module": "menus_toolbars",
        "source_directory": "menus_toolbars",
        "sample_source": "MenusToolbarsSamples.cpp",
    },
    "navigation": {
        "cpp_header": "Navigation.h",
        "cpp_namespace": "fluent::navigation",
        "python_module": "navigation",
        "source_directory": "navigation",
        "sample_source": "NavigationSamples.cpp",
    },
    "scrolling": {
        "cpp_header": "Scrolling.h",
        "cpp_namespace": "fluent::scrolling",
        "python_module": "scrolling",
        "source_directory": "scrolling",
        "sample_source": "ScrollingSamples.cpp",
    },
    "status-info": {
        "cpp_header": "StatusInfo.h",
        "cpp_namespace": "fluent::status_info",
        "python_module": "status_info",
        "source_directory": "status_info",
        "sample_source": "StatusInfoSamples.cpp",
    },
    "text-fields": {
        "cpp_header": "TextFields.h",
        "cpp_namespace": "fluent::textfields",
        "python_module": "textfields",
        "source_directory": "textfields",
        "sample_source": "TextFieldsSamples.cpp",
    },
    "windowing": {
        "cpp_header": "Windowing.h",
        "cpp_namespace": "fluent::windowing",
        "python_module": "windowing",
        "source_directory": "windowing",
        "sample_source": "WindowingSamples.cpp",
    },
    "foundation": {
        "cpp_header": "Foundation.h",
        "cpp_namespace": "fluent",
        "python_module": "foundation",
        "source_directory": "foundation",
        "sample_source": "FoundationSamples.cpp",
    },
}

TEST_SOURCE_OVERRIDES = {
    "title-bar": "TestWindow.cpp",
}

WORD_PATTERN = re.compile(r"[A-Za-z0-9][A-Za-z0-9+.-]*")
PROJECT_VERSION_PATTERN = re.compile(
    r"project\(FluentQt\s+VERSION\s+([0-9]+(?:\.[0-9]+)+)"
)
TEST_TARGET_PATTERN = re.compile(
    r"add_qt_test_module\(\s*([A-Za-z0-9_]+)\s+(Test[A-Za-z0-9_]+\.cpp)"
)
STOP_WORDS = {
    "and",
    "are",
    "for",
    "from",
    "into",
    "its",
    "the",
    "that",
    "this",
    "with",
}


def _load_module(name: str, path: Path) -> ModuleType:
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load Python module from {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _gallery_contract(project_root: Path) -> dict[str, object]:
    generator_path = (
        project_root
        / "bindings"
        / "pyside6"
        / "gallery"
        / "tools"
        / "generate_gallery_contract.py"
    )
    module = _load_module("_fluentqt_gallery_contract", generator_path)
    return module.generate_contract(project_root)


def _project_version(project_root: Path) -> str:
    contents = (project_root / "CMakeLists.txt").read_text(encoding="utf-8")
    match = PROJECT_VERSION_PATTERN.search(contents)
    if not match:
        raise ValueError("Could not read FluentQt version from CMakeLists.txt")
    return match.group(1)


def _test_targets(project_root: Path) -> dict[str, dict[str, str]]:
    tests_root = project_root / "tests" / "components"
    targets: dict[str, dict[str, str]] = {}
    for cmake_path in sorted(tests_root.rglob("CMakeLists.txt")):
        contents = cmake_path.read_text(encoding="utf-8")
        for target, source_name in TEST_TARGET_PATTERN.findall(contents):
            source_path = cmake_path.parent / source_name
            if not source_path.is_file():
                raise ValueError(
                    f"Test target {target} references missing source {source_path}"
                )
            if source_name in targets:
                raise ValueError(f"Duplicate component test source: {source_name}")
            targets[source_name] = {
                "target": target,
                "source": source_path.relative_to(project_root).as_posix(),
                "ctest_label": target,
            }
    return targets


def _guidance(project_root: Path) -> dict[str, object]:
    path = project_root / "docs" / "ai" / "guidance.json"
    guidance = json.loads(path.read_text(encoding="utf-8"))
    if guidance.get("schema_version") != 2:
        raise ValueError("docs/ai/guidance.json schema_version must be 2")
    return guidance


def _python_exports(module_path: Path) -> set[str]:
    tree = ast.parse(module_path.read_text(encoding="utf-8"), filename=str(module_path))
    for node in tree.body:
        if not isinstance(node, ast.Assign):
            continue
        if not any(
            isinstance(target, ast.Name) and target.id == "__all__"
            for target in node.targets
        ):
            continue
        exports = ast.literal_eval(node.value)
        if not isinstance(exports, list) or not all(
            isinstance(item, str) for item in exports
        ):
            break
        return set(exports)
    raise ValueError(f"Could not read __all__ from Python module {module_path}")


def _validate_guidance(
    guidance: dict[str, object], component_ids: set[str]
) -> dict[str, list[str]]:
    capability_map = {component_id: [] for component_id in component_ids}
    pattern_ids: set[str] = set()
    for key in ("integration_patterns", "application_patterns", "selection_guides"):
        values = guidance.get(key)
        if not isinstance(values, list) or not values:
            raise ValueError(f"guidance.{key} must be a non-empty array")
        for value in values:
            identifier = value.get("id")
            namespaced_id = f"{key}:{identifier}"
            if not isinstance(identifier, str) or not identifier:
                raise ValueError(f"guidance.{key} contains an invalid id")
            if namespaced_id in pattern_ids:
                raise ValueError(f"Duplicate guidance id: {identifier}")
            pattern_ids.add(namespaced_id)

    integration_ids = {
        pattern["id"] for pattern in guidance["integration_patterns"]
    }
    allowed_window_ownership = {
        "application-owned",
        "host-owned",
        "caller-decides",
    }
    for pattern in guidance["integration_patterns"]:
        if pattern.get("window_ownership") not in allowed_window_ownership:
            raise ValueError(
                f"Integration pattern {pattern['id']} has invalid window_ownership"
            )
    for pattern in guidance["application_patterns"]:
        unknown_integrations = set(pattern["preferred_integrations"]) - integration_ids
        if unknown_integrations:
            raise ValueError(
                f"Application pattern {pattern['id']} references unknown integrations: "
                + ", ".join(sorted(unknown_integrations))
            )
        unknown_components = set(pattern["components"]) - component_ids
        if unknown_components:
            raise ValueError(
                f"Application pattern {pattern['id']} references unknown components: "
                + ", ".join(sorted(unknown_components))
            )

    for guide in guidance["selection_guides"]:
        candidates = guide.get("candidates")
        if not isinstance(candidates, list) or not candidates:
            raise ValueError(f"Selection guide {guide['id']} has no candidates")
        seen: set[str] = set()
        for candidate in candidates:
            component_id = candidate.get("component_id")
            if component_id not in component_ids:
                raise ValueError(
                    f"Selection guide {guide['id']} references unknown component "
                    f"{component_id}"
                )
            if component_id in seen:
                raise ValueError(
                    f"Selection guide {guide['id']} repeats {component_id}"
                )
            for field in ("choose_when", "avoid_when"):
                if not candidate.get(field):
                    raise ValueError(
                        f"Selection guide {guide['id']} candidate {component_id} "
                        f"is missing {field}"
                    )
            seen.add(component_id)
            capability_map[component_id].append(guide["id"])

    uncovered = sorted(
        component_id
        for component_id, guide_ids in capability_map.items()
        if not guide_ids
    )
    if uncovered:
        raise ValueError(
            "Components missing from selection guidance: " + ", ".join(uncovered)
        )
    return capability_map


def _search_terms(
    component: dict[str, object], category_title: str, capability_ids: list[str]
) -> list[str]:
    values = [
        component["id"],
        component["title"],
        component["api_type"],
        component["description"],
        category_title,
        *capability_ids,
    ]
    for sample in component["samples"]:
        values.extend((sample["id"], sample["title"], sample["description"]))
    words = {
        word.lower()
        for value in values
        for word in WORD_PATTERN.findall(str(value))
        if len(word) > 2 and word.lower() not in STOP_WORDS
    }
    return sorted(words)


def generate_catalog(project_root: Path) -> dict[str, object]:
    project_root = project_root.resolve()
    gallery = _gallery_contract(project_root)
    guidance = _guidance(project_root)
    component_ids = {component["id"] for component in gallery["components"]}
    capability_map = _validate_guidance(guidance, component_ids)
    test_targets = _test_targets(project_root)
    category_titles = {
        category["id"]: category["title"] for category in gallery["categories"]
    }
    python_exports: dict[str, set[str]] = {}
    for category_id, category_api in CATEGORY_API.items():
        public_header = (
            project_root / "include" / "FluentQt" / category_api["cpp_header"]
        )
        if not public_header.is_file():
            raise ValueError(
                f"Missing installed category header for {category_id}: {public_header}"
            )
        module_path = (
            project_root
            / "bindings"
            / "pyside6"
            / "src"
            / "fluentqt"
            / f"{category_api['python_module']}.py"
        )
        if not module_path.is_file():
            raise ValueError(
                f"Missing public Python module for {category_id}: {module_path}"
            )
        python_exports[category_id] = _python_exports(module_path)

    api_manifest = json.loads(
        (project_root / "bindings" / "pyside6" / "api-manifest.json").read_text(
            encoding="utf-8"
        )
    )
    components = []
    for component in gallery["components"]:
        category_id = component["category_id"]
        category_api = CATEGORY_API.get(category_id)
        if category_api is None:
            raise ValueError(f"Missing API mapping for category {category_id}")
        if component["api_type"] not in python_exports[category_id]:
            raise ValueError(
                f"Python module fluentqt.{category_api['python_module']} does not "
                f"export {component['api_type']} for {component['id']}"
            )

        declaration = (
            project_root
            / "src"
            / "components"
            / category_api["source_directory"]
            / f"{component['title']}.h"
        )
        if not declaration.is_file():
            raise ValueError(
                f"Missing declaration header for {component['id']}: {declaration}"
            )
        test_source_name = TEST_SOURCE_OVERRIDES.get(
            component["id"], f"Test{component['title']}.cpp"
        )
        test = test_targets.get(test_source_name)
        if test is None:
            raise ValueError(
                f"Missing focused test mapping for {component['id']}: "
                f"{test_source_name}"
            )

        control_image = (
            project_root
            / "app"
            / "assets"
            / "control_images"
            / category_id
            / f"{component['title']}.png"
        )
        sample_source = (
            Path("app")
            / "view"
            / "widgets"
            / "samples"
            / category_api["sample_source"]
        ).as_posix()
        capability_ids = sorted(capability_map[component["id"]])
        components.append(
            {
                "id": component["id"],
                "title": component["title"],
                "category_id": category_id,
                "description": component["description"],
                "capabilities": capability_ids,
                "search_terms": _search_terms(
                    component, category_titles[category_id], capability_ids
                ),
                "cpp": {
                    "public_header": f"<FluentQt/{category_api['cpp_header']}>",
                    "declaration": declaration.relative_to(project_root).as_posix(),
                    "qualified_type": (
                        f"{category_api['cpp_namespace']}::{component['api_type']}"
                    ),
                    "cmake_target": "FluentQt::FluentQt",
                },
                "python": {
                    "package": "FluentQt",
                    "module": f"fluentqt.{category_api['python_module']}",
                    "type": component["api_type"],
                    "import_statement": (
                        f"from fluentqt.{category_api['python_module']} "
                        f"import {component['api_type']}"
                    ),
                },
                "tests": [test],
                "gallery": {
                    "route_id": component["id"],
                    "sample_source": sample_source,
                    "control_image": (
                        control_image.relative_to(project_root).as_posix()
                        if control_image.is_file()
                        else None
                    ),
                },
                "samples": component["samples"],
            }
        )

    categories = []
    for category in gallery["categories"]:
        category_api = CATEGORY_API[category["id"]]
        categories.append(
            {
                **category,
                "cpp_header": f"<FluentQt/{category_api['cpp_header']}>",
                "python_module": f"fluentqt.{category_api['python_module']}",
            }
        )

    return {
        "schema_version": 2,
        "project": {
            "name": "FluentQt",
            "version": _project_version(project_root),
            "api_version": api_manifest["api_version"],
            "cpp_standard": "C++17",
            "qt_versions": ["5.15+", "6.2+"],
            "python_versions": ["3.10+"],
            "pyside_versions": ["6.2+"],
            "cpp_target": "FluentQt::FluentQt",
            "python_package": "FluentQt",
        },
        "canonical_sources": [
            {
                "path": "app/model/GalleryComponentCatalog.cpp",
                "role": "Component identity, category, and public integration facts",
            },
            {
                "path": "app/model/GalleryContentCatalog.cpp",
                "role": "Route descriptions",
            },
            {
                "path": "app/view/widgets/samples",
                "role": "Executable previews and concise C++ teaching snippets",
            },
            {
                "path": "bindings/pyside6/api-manifest.json",
                "role": "Versioned Python API surface",
            },
            {
                "path": "cmake/FluentQtInstallHeaders.cmake",
                "role": "Installed public-header allowlist",
            },
            {
                "path": "docs/ai/guidance.json",
                "role": "Application, integration, and component-selection semantics",
            },
        ],
        "integration_patterns": guidance["integration_patterns"],
        "application_patterns": guidance["application_patterns"],
        "selection_guides": guidance["selection_guides"],
        "categories": categories,
        "components": components,
        "routes": gallery["routes"],
        "binding_support_types": gallery["binding_support_types"],
        "summary": {
            **gallery["summary"],
            "integration_pattern_count": len(guidance["integration_patterns"]),
            "application_pattern_count": len(guidance["application_patterns"]),
            "selection_guide_count": len(guidance["selection_guides"]),
            "guided_component_count": len(capability_map),
        },
    }


def serialized_catalog(catalog: dict[str, object]) -> str:
    return json.dumps(catalog, ensure_ascii=False, indent=2) + "\n"


def write_catalog(catalog: dict[str, object], output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    serialized = serialized_catalog(catalog)
    if output.is_file() and output.read_text(encoding="utf-8") == serialized:
        return
    output.write_text(serialized, encoding="utf-8", newline="\n")


def check_catalog(catalog: dict[str, object], output: Path) -> bool:
    expected = serialized_catalog(catalog)
    actual = output.read_text(encoding="utf-8") if output.is_file() else ""
    if actual == expected:
        return True
    diff = difflib.unified_diff(
        actual.splitlines(),
        expected.splitlines(),
        fromfile=str(output),
        tofile="generated catalog",
        lineterm="",
    )
    print("AI catalog is stale; regenerate it with generate_ai_catalog.py.")
    for index, line in enumerate(diff):
        if index >= 120:
            print("... diff truncated ...")
            break
        print(line)
    return False


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    default_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=default_root)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--check", action="store_true")
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    project_root = args.project_root.resolve()
    output = (
        args.output.resolve()
        if args.output is not None
        else project_root
        / "docs"
        / "ai"
        / "generated"
        / "fluentqt-ai-catalog.json"
    )
    catalog = generate_catalog(project_root)
    if args.check:
        return 0 if check_catalog(catalog, output) else 1
    write_catalog(catalog, output)
    print(
        "Generated FluentQt AI catalog: "
        "{component_count} components, {sample_count} samples, "
        "{application_pattern_count} application patterns".format(
            **catalog["summary"]
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
