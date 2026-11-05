"""Verify benchmark isolation, comparison order, and timing interpretation."""

import importlib.util
import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location("benchmark_ci", Path(__file__).with_name("benchmark-ci-builds.py"))
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class BenchmarkTest(unittest.TestCase):
    def test_comparisons_bracket_order_and_do_not_fake_a_warm_build(self):
        for lane in ("linux-sanitizers", "windows-targeted"):
            variants = MODULE.variants(lane)
            self.assertEqual([v["version"] for v in variants], ["baseline", "candidate", "candidate", "baseline"])
            self.assertTrue(all(v["cache"] is False for v in variants))
        self.assertEqual([v["pch"] for v in MODULE.variants("python-pch")], [False, True, True, False])
        self.assertEqual([v["cache"] for v in MODULE.variants("intel-cache")], [False, "cold", "warm", False])

    def test_local_checkout_is_refused_before_any_mutation(self):
        with patch.dict(os.environ, {"GITHUB_ACTIONS": "false"}), patch.object(MODULE.subprocess, "run") as run:
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                MODULE.main(["--lane", "wasm"])
            run.assert_not_called()

    def test_ninja_log_keeps_overlapping_edge_timings(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / ".ninja_log"
            path.write_text("# ninja log v5\n0\t200\t10\ta.cpp.o\thash-a\n10\t100\t20\tb.cpp.o\thash-b\n200\t500\t30\tapp.js\thash-c\n")
            rows = MODULE.ninja_timings(path)
            self.assertEqual([r["duration_ms"] for r in rows], [200, 90, 300])
            self.assertEqual(rows[-1]["start_ms"], 200)

    def make_reply(self, build, artifact):
        reply = build / ".cmake/api/v1/reply"
        reply.mkdir(parents=True)
        (reply / "codemodel-v2-test.json").write_text(json.dumps({"configurations": [
            {"name": "Debug", "targets": [{"jsonFile": "target.json"}]}]}))
        (reply / "target.json").write_text(json.dumps({"type": "EXECUTABLE", "artifacts": [{"path": artifact}]}))

    def test_relink_only_selects_existing_outputs_inside_build(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary)
            artifact = build / "test.exe"
            artifact.touch()
            self.make_reply(build, "test.exe")
            self.assertEqual(MODULE.existing_link_outputs(build), [artifact.resolve()])
            self.assertTrue(artifact.exists(), "discovery must be read-only")

    def test_relink_refuses_an_output_outside_its_disposable_build(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary) / "build"
            self.make_reply(build, "../precious.exe")
            with self.assertRaisesRegex(ValueError, "outside"):
                MODULE.existing_link_outputs(build)

    def test_object_snapshot_detects_recompilation(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary)
            obj = build / "module.obj"
            obj.write_bytes(b"first")
            before = MODULE.object_snapshot(build)
            obj.write_bytes(b"changed object")
            self.assertNotEqual(before, MODULE.object_snapshot(build))

    @unittest.skipUnless(shutil.which("cmake"), "CMake is needed for the real relink fixture")
    def test_real_cmake_relink_preserves_compiled_objects(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            query = build / ".cmake/api/v1/query"
            query.mkdir(parents=True)
            (query / "codemodel-v2").touch()
            (root / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.16)\nproject(Probe LANGUAGES CXX)\n"
                "add_executable(probe main.cpp)\n")
            (root / "main.cpp").write_text("int main() { return 0; }\n")
            commands = (["cmake", "-S", str(root), "-B", str(build)],
                        ["cmake", "--build", str(build), "--config", "Debug"])
            for command in commands:
                subprocess.run(command, check=True, capture_output=True)
            objects = MODULE.object_snapshot(build)
            self.assertTrue(objects)
            outputs = MODULE.existing_link_outputs(build)
            for output in outputs:
                output.unlink()
            subprocess.run(commands[-1], check=True, capture_output=True)
            self.assertEqual(objects, MODULE.object_snapshot(build))
            self.assertTrue(all(output.exists() for output in outputs))


if __name__ == "__main__":
    unittest.main()
