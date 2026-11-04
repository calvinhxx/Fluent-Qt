#!/usr/bin/env python3
"""Select C++ CI jobs and apply the PR test scope to one Linux builder."""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path
from typing import Any

from classify_ci_changes import CppTestSelection


CATALOG = Path(__file__).resolve().parents[1] / "ci-cpp-matrix.json"


def select_matrix(
    catalog: dict[str, Any], mode: str, scope: str, labels: str, targets: str,
    candidate_owns_package: bool = False,
) -> dict[str, list[dict[str, Any]]]:
    if mode not in ("fast", "full"):
        raise ValueError(f"Unknown C++ validation tier: {mode}")
    groups = ()
    if scope == "selected":
        groups = tuple(labels.removeprefix("^(").removesuffix(")$").split("|"))
    if CppTestSelection(scope, groups).output_values() != (scope, labels, targets):
        raise ValueError("C++ test scope, labels and build targets must match the path classifier")

    jobs = []
    for scenario in catalog["scenarios"]:
        if scenario["mode"] != mode:
            continue
        if candidate_owns_package and scenario["id"] == "win-arm64-qt693-cross":
            continue
        job = dict(scenario)
        del job["mode"]
        if scope != "none" and job["id"] == f"linux-x64-qt62-{mode}":
            build_targets = job["build_targets"].split()
            if scope == "all":
                build_targets[build_targets.index(f"fluent_qt_ci_{mode}_tests")] = targets
            else:
                build_targets.extend(targets.split())
            job["build_targets"] = " ".join(build_targets)
            job["test_labels"] = f"({job['test_labels']}|{labels})"
        jobs.append(job)
    if not jobs:
        raise ValueError(f"Empty C++ matrix for {mode}")
    return {"include": jobs}


if __name__ == "__main__":
    try:
        matrix = select_matrix(
            json.loads(CATALOG.read_text(encoding="utf-8")),
            os.environ["VALIDATION_MODE"], os.environ["CPP_TEST_SCOPE"],
            os.environ["CPP_TEST_LABEL_REGEX"], os.environ["CPP_TEST_TARGETS"],
            os.environ["CANDIDATE_OWNS_WINDOWS_ARM64"] == "true",
        )
    except (KeyError, OSError, ValueError) as error:
        sys.exit(f"error: {error}")
    print(json.dumps(matrix, separators=(",", ":")))
