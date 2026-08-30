#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = (
    Path(__file__).resolve().parents[2]
    / ".agents/skills/build-fluentqt-gui/scripts/benchmark_run.py"
)
SPEC = importlib.util.spec_from_file_location("fluent_qt_benchmark_run", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class BenchmarkRunPathTest(unittest.TestCase):
    def test_portable_path_stays_relative_on_one_volume_and_round_trips(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            manifest_path = root / "runs" / "run.json"
            target = root / "workspace" / "artifact.json"

            recorded = MODULE._portable_path(target, manifest_path.parent)

            self.assertFalse(Path(recorded).is_absolute())
            self.assertEqual(
                MODULE._recorded_path(recorded, manifest_path), target.resolve()
            )

    def test_portable_path_uses_absolute_fallback_and_round_trips(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            manifest_path = root / "runs" / "run.json"
            target = root / "workspace" / "artifact.json"

            with mock.patch.object(
                MODULE.os.path,
                "relpath",
                side_effect=ValueError("path is on a different mount"),
            ):
                recorded = MODULE._portable_path(target, manifest_path.parent)

            self.assertEqual(recorded, target.resolve().as_posix())
            self.assertTrue(Path(recorded).is_absolute())
            self.assertEqual(
                MODULE._recorded_path(recorded, manifest_path), target.resolve()
            )


if __name__ == "__main__":
    unittest.main()
