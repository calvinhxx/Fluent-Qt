"""Verify that the packaged Gallery contract is a live native-source derivation."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    project_root = args.project_root.resolve()
    tools_dir = Path(__file__).resolve().parents[1] / "tools"
    sys.path.insert(0, str(tools_dir))
    from generate_gallery_contract import (
        _category_metadata,
        _route_sample_functions,
        generate_contract,
    )

    qualified_dispatch = _route_sample_functions(
        'if (routeId == QStringLiteral("tree-view"))\n'
        '    return detail::treeViewSamples();'
    )
    if qualified_dispatch != {"tree-view": "treeViewSamples"}:
        raise AssertionError(
            "qualified split-sample dispatch is not recognized: {0!r}".format(
                qualified_dispatch
            )
        )

    metadata = _category_metadata('''
        static const QVector<GalleryComponentCategory> catalog{
            {QStringLiteral("basic"), QStringLiteral("Basic"), QStringLiteral("basic"),
             Typography::Icons::Grid,
             {{QStringLiteral("button"), QStringLiteral("Button"), Typography::Icons::Add}}},
            {QStringLiteral("extension"), QStringLiteral("Extension"), QStringLiteral("extension"),
             Typography::Icons::Grid,
             {{QStringLiteral("extension-view"), QStringLiteral("ExtensionView"), Typography::Icons::Add}}, true}
        };
    ''')
    if list(metadata) != ["basic", "extension"] or metadata["basic"]["plugin"] \
            or not metadata["extension"]["plugin"]:
        raise AssertionError("native category order and plugin bool must drive the contract")

    generated = generate_contract(project_root)
    packaged = json.loads(args.contract.resolve().read_text(encoding="utf-8"))
    if generated != packaged:
        raise AssertionError(
            "packaged Python Gallery contract differs from current native C++ sources"
        )
    base_contract = generate_contract(project_root, include_cpp_only=False)
    cpp_only_ids = {"spatial-view", "spatial-item"}
    if cpp_only_ids & {c["id"] for c in base_contract["components"]}:
        raise AssertionError("The base Python Gallery must not require optional Spatial bindings")
    if any(route["id"] in ("plugins", "spatial", "spatial-view", "spatial-item")
           for route in base_contract["routes"]):
        raise AssertionError("The base Gallery must omit the empty Plugins subtree")
    cpp_spatial = [c for c in generated["components"] if c["id"] in cpp_only_ids]
    if len(cpp_spatial) != 2 or any(c["category_id"] != "spatial" for c in cpp_spatial):
        raise AssertionError("native Spatial components must share their plugin module")
    if sum(len(c["samples"]) for c in cpp_spatial) != 11:
        raise AssertionError("Spatial composition samples are missing")
    summary = generated["summary"]
    if summary != {
        "route_count": 111,
        "component_count": 87,
        "sample_count": 243,
    }:
        raise AssertionError("unexpected Gallery contract summary: {0!r}".format(summary))
    routes = {route["id"]: route for route in generated["routes"]}
    for route_id, parent, depth, group in (
        ("plugins", "", 0, "Plugins"), ("spatial", "plugins", 1, "Plugins"),
        ("spatial-view", "spatial", 2, "Spatial"), ("spatial-item", "spatial", 2, "Spatial"),
    ):
        route = routes[route_id]
        if (route["parent_id"], route["depth"], route["group"]) != (parent, depth, group):
            raise AssertionError("incorrect plugin navigation hierarchy: " + route_id)
    route_ids = [route["id"] for route in generated["routes"]]
    start = route_ids.index("plugins")
    if route_ids[start:start + 5] != ["plugins", "spatial", "spatial-view", "spatial-item", "all-controls"]:
        raise AssertionError("Plugins must precede Controls and keep its subtree together")
    native_categories = _category_metadata(
        (project_root / "app/model/GalleryComponentCatalog.cpp").read_text(encoding="utf-8")
    )
    if [category["id"] for category in generated["categories"]] != list(native_categories):
        raise AssertionError("Gallery category order must come from native metadata")
    if [category["id"] for category in generated["categories"] if category["plugin"]] != ["spatial"]:
        raise AssertionError("Spatial must be classified as a plugin module")
    print(
        "Verified Gallery contract: {route_count} routes, "
        "{component_count} components, {sample_count} samples".format(**summary)
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
