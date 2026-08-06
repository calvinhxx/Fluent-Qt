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
    from generate_gallery_contract import generate_contract

    generated = generate_contract(project_root)
    packaged = json.loads(args.contract.resolve().read_text(encoding="utf-8"))
    if generated != packaged:
        raise AssertionError(
            "packaged Python Gallery contract differs from current native C++ sources"
        )
    summary = generated["summary"]
    if summary != {
        "route_count": 88,
        "component_count": 67,
        "sample_count": 199,
    }:
        raise AssertionError("unexpected Gallery contract summary: {0!r}".format(summary))
    print(
        "Verified Gallery contract: {route_count} routes, "
        "{component_count} components, {sample_count} samples".format(**summary)
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
