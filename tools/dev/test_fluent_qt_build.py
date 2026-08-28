#!/usr/bin/env python3

from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import importlib.util
from io import StringIO
from pathlib import Path
import sys
import unittest
from unittest import mock


SCRIPT = Path(__file__).with_name("fluent_qt_build.py")
SPEC = importlib.util.spec_from_file_location("fluent_qt_build", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class FluentQtBuildToolTest(unittest.TestCase):
    def test_high_resource_host_uses_all_available_cpus(self):
        resources = MODULE.ResourceSnapshot(
            logical_cpus=32,
            affinity_cpus=32,
            quota_cpus=None,
            available_memory_bytes=64 * 1024**3,
        )

        decision = MODULE.choose_parallel_jobs(resources)

        self.assertEqual(decision.jobs, 32)
        self.assertGreaterEqual(decision.memory_limit, 32)

    def test_memory_pressure_bounds_cpu_parallelism(self):
        resources = MODULE.ResourceSnapshot(
            logical_cpus=24,
            affinity_cpus=16,
            quota_cpus=None,
            available_memory_bytes=10 * 1024**3,
        )

        decision = MODULE.choose_parallel_jobs(resources)

        self.assertEqual(decision.cpu_limit, 16)
        self.assertEqual(decision.memory_limit, 6)
        self.assertEqual(decision.jobs, 6)

    def test_affinity_and_cgroup_quota_constrain_cpu_limit(self):
        resources = MODULE.ResourceSnapshot(
            logical_cpus=32,
            affinity_cpus=12,
            quota_cpus=6,
            available_memory_bytes=None,
        )

        decision = MODULE.choose_parallel_jobs(resources)

        self.assertEqual(decision.jobs, 6)
        self.assertIsNone(decision.memory_limit)

    def test_low_memory_host_keeps_one_build_job(self):
        resources = MODULE.ResourceSnapshot(
            logical_cpus=16,
            affinity_cpus=16,
            quota_cpus=None,
            available_memory_bytes=512 * 1024**2,
        )

        decision = MODULE.choose_parallel_jobs(resources)

        self.assertEqual(decision.jobs, 1)
        self.assertEqual(decision.memory_limit, 1)

    def test_linux_detection_uses_process_and_cgroup_limits(self):
        with (
            mock.patch.object(MODULE.os, "cpu_count", return_value=32),
            mock.patch.object(MODULE, "_affinity_cpu_count", return_value=12),
            mock.patch.object(MODULE, "_cgroup_cpu_limit", return_value=6),
            mock.patch.object(
                MODULE, "_host_available_memory", return_value=24 * 1024**3
            ),
            mock.patch.object(
                MODULE, "_cgroup_memory_available", return_value=10 * 1024**3
            ),
        ):
            resources = MODULE.detect_resources("Linux")

        self.assertEqual(resources.cpu_limit, 6)
        self.assertEqual(resources.available_memory_bytes, 10 * 1024**3)

    def test_windows_detection_skips_linux_cgroup_probes(self):
        with (
            mock.patch.object(MODULE.os, "cpu_count", return_value=24),
            mock.patch.object(MODULE, "_affinity_cpu_count", return_value=None),
            mock.patch.object(
                MODULE, "_host_available_memory", return_value=48 * 1024**3
            ),
            mock.patch.object(MODULE, "_cgroup_cpu_limit") as cpu_probe,
            mock.patch.object(MODULE, "_cgroup_memory_available") as memory_probe,
        ):
            resources = MODULE.detect_resources("Windows")

        self.assertEqual(resources.cpu_limit, 24)
        self.assertEqual(resources.available_memory_bytes, 48 * 1024**3)
        cpu_probe.assert_not_called()
        memory_probe.assert_not_called()

    def test_cpu_quota_rounds_fractional_capacity_up(self):
        self.assertEqual(MODULE.parse_cpu_max("150000 100000"), 2)
        self.assertEqual(MODULE.parse_cpu_max("50000 100000"), 1)
        self.assertIsNone(MODULE.parse_cpu_max("max 100000"))
        self.assertIsNone(MODULE.parse_cpu_max("invalid"))

    def test_linux_available_memory_prefers_memavailable(self):
        value = """\
MemTotal:       16384000 kB
MemFree:         1000000 kB
MemAvailable:    8000000 kB
Buffers:          200000 kB
Cached:          3000000 kB
"""
        self.assertEqual(
            MODULE.parse_linux_mem_available(value), 8000000 * 1024
        )

    def test_vm_stat_counts_reclaimable_pages(self):
        value = """\
Mach Virtual Memory Statistics: (page size of 16384 bytes)
Pages free:                               1000.
Pages active:                             9000.
Pages inactive:                           2000.
Pages speculative:                         500.
Pages wired down:                         4000.
"""
        self.assertEqual(MODULE.parse_vm_stat(value), 3500 * 16384)

    def test_command_line_override_wins_over_environment(self):
        jobs, reason = MODULE.requested_jobs(
            "12",
            {
                "FLUENTQT_BUILD_JOBS": "8",
                "CMAKE_BUILD_PARALLEL_LEVEL": "4",
            },
        )
        self.assertEqual(jobs, 12)
        self.assertEqual(reason, "--jobs override")

    def test_project_environment_precedes_cmake_environment(self):
        jobs, reason = MODULE.requested_jobs(
            None,
            {
                "FLUENTQT_BUILD_JOBS": "10",
                "CMAKE_BUILD_PARALLEL_LEVEL": "4",
            },
        )
        self.assertEqual(jobs, 10)
        self.assertEqual(reason, "FLUENTQT_BUILD_JOBS override")

    def test_parallel_flag_is_inserted_before_native_arguments(self):
        command = MODULE.create_build_command(
            ["--preset", "vcpkg-osx", "--target", "FluentQt", "--", "-v"],
            8,
        )
        self.assertEqual(
            command,
            [
                "cmake",
                "--build",
                "--preset",
                "vcpkg-osx",
                "--target",
                "FluentQt",
                "--parallel",
                "8",
                "--",
                "-v",
            ],
        )

    def test_rejects_conflicting_cmake_parallel_argument(self):
        with self.assertRaisesRegex(ValueError, "--jobs"):
            MODULE.create_build_command(
                ["--preset", "vcpkg-osx", "--parallel", "8"], 6
            )

    def test_rejects_conflicting_native_parallel_argument(self):
        with self.assertRaisesRegex(ValueError, "--jobs"):
            MODULE.create_build_command(
                ["--preset", "vcpkg-osx", "--", "-j8"], 6
            )

    def test_keyboard_interrupt_exits_without_traceback(self):
        resources = MODULE.ResourceSnapshot(
            logical_cpus=8,
            affinity_cpus=8,
            quota_cpus=None,
            available_memory_bytes=8 * 1024**3,
        )
        stdout = StringIO()
        stderr = StringIO()
        with (
            mock.patch.object(MODULE, "detect_resources", return_value=resources),
            mock.patch.object(MODULE.subprocess, "run", side_effect=KeyboardInterrupt),
            redirect_stdout(stdout),
            redirect_stderr(stderr),
        ):
            exit_code = MODULE.main(
                ["--jobs", "2", "--preset", "vcpkg-osx", "--target", "FluentQt"]
            )

        self.assertEqual(exit_code, 130)
        self.assertIn("interrupted by user", stderr.getvalue())
        self.assertNotIn("Traceback", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
