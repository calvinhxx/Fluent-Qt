#!/usr/bin/env python3

"""Build FluentQt with host-aware, memory-bounded parallelism."""

from __future__ import annotations

import argparse
import ctypes
from dataclasses import dataclass
import math
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import sys
from typing import Iterable, Mapping


PROJECT_ROOT = Path(__file__).resolve().parents[2]
GIB = 1024**3
MEMORY_PER_JOB = 1536 * 1024**2
MINIMUM_RESERVE = GIB
RESERVE_FRACTION = 0.10


@dataclass(frozen=True)
class ResourceSnapshot:
    logical_cpus: int
    affinity_cpus: int | None
    quota_cpus: int | None
    available_memory_bytes: int | None

    @property
    def cpu_limit(self) -> int:
        limits = [self.logical_cpus, self.affinity_cpus, self.quota_cpus]
        return max(1, min(value for value in limits if value is not None))


@dataclass(frozen=True)
class ParallelDecision:
    jobs: int
    cpu_limit: int
    memory_limit: int | None
    available_memory_bytes: int | None
    reserve_memory_bytes: int | None


def _read(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8").strip()
    except (OSError, UnicodeError):
        return None


def parse_cpu_max(value: str) -> int | None:
    fields = value.split()
    if len(fields) != 2 or fields[0] == "max":
        return None
    try:
        quota, period = map(int, fields)
    except ValueError:
        return None
    if quota <= 0 or period <= 0:
        return None
    return max(1, math.ceil(quota / period))


def parse_linux_mem_available(value: str) -> int | None:
    entries = {
        match.group(1): int(match.group(2)) * 1024
        for line in value.splitlines()
        if (match := re.match(r"^([^:]+):\s+(\d+)\s+kB$", line.strip()))
    }
    if "MemAvailable" in entries:
        return entries["MemAvailable"]
    fallback = sum(entries.get(key, 0) for key in ("MemFree", "Buffers", "Cached"))
    return fallback or None


def parse_vm_stat(value: str) -> int | None:
    size_match = re.search(r"page size of (\d+) bytes", value)
    if not size_match:
        return None
    pages = {
        match.group(1): int(match.group(2))
        for line in value.splitlines()
        if (match := re.match(r"^([^:]+):\s+(\d+)\.?$", line.strip()))
    }
    reclaimable = sum(
        pages.get(key, 0)
        for key in ("Pages free", "Pages inactive", "Pages speculative")
    )
    return reclaimable * int(size_match.group(1)) if reclaimable else None


def _unified_cgroup_directories() -> list[Path]:
    root = Path("/sys/fs/cgroup")
    relative = "/"
    membership = _read(Path("/proc/self/cgroup"))
    if membership:
        for line in membership.splitlines():
            fields = line.split(":", 2)
            if len(fields) == 3 and not fields[1]:
                relative = fields[2]
                break

    current = root / relative.lstrip("/")
    directories: list[Path] = []
    while current == root or root in current.parents:
        directories.append(current)
        if current == root:
            break
        current = current.parent
    if root not in directories:
        directories.append(root)
    return list(dict.fromkeys(directories))


def _cgroup_cpu_limit() -> int | None:
    limits = [
        limit
        for directory in _unified_cgroup_directories()
        if (value := _read(directory / "cpu.max"))
        if (limit := parse_cpu_max(value)) is not None
    ]
    for root in (
        Path("/sys/fs/cgroup/cpu"),
        Path("/sys/fs/cgroup/cpu,cpuacct"),
    ):
        quota = _read(root / "cpu.cfs_quota_us")
        period = _read(root / "cpu.cfs_period_us")
        if quota and period and (limit := parse_cpu_max(f"{quota} {period}")):
            limits.append(limit)
    return min(limits) if limits else None


def _finite_memory_limit(value: str | None) -> int | None:
    if not value or value == "max":
        return None
    try:
        limit = int(value)
    except ValueError:
        return None
    return limit if 0 < limit < (1 << 60) else None


def _remaining_memory(directory: Path, maximum: str, current: str) -> int | None:
    limit = _finite_memory_limit(_read(directory / maximum))
    usage = _read(directory / current)
    if limit is None or usage is None:
        return None
    try:
        return max(0, limit - int(usage))
    except ValueError:
        return None


def _cgroup_memory_available() -> int | None:
    limits = [
        remaining
        for directory in _unified_cgroup_directories()
        if (
            remaining := _remaining_memory(
                directory, "memory.max", "memory.current"
            )
        )
        is not None
    ]
    legacy = _remaining_memory(
        Path("/sys/fs/cgroup/memory"),
        "memory.limit_in_bytes",
        "memory.usage_in_bytes",
    )
    if legacy is not None:
        limits.append(legacy)
    return min(limits) if limits else None


def _affinity_cpu_count() -> int | None:
    getter = getattr(os, "sched_getaffinity", None)
    if getter is None:
        return None
    try:
        return len(getter(0)) or None
    except OSError:
        return None


def _windows_available_memory() -> int | None:
    class MemoryStatus(ctypes.Structure):
        _fields_ = [
            ("length", ctypes.c_ulong),
            ("memory_load", ctypes.c_ulong),
            ("total_physical", ctypes.c_ulonglong),
            ("available_physical", ctypes.c_ulonglong),
            ("total_page_file", ctypes.c_ulonglong),
            ("available_page_file", ctypes.c_ulonglong),
            ("total_virtual", ctypes.c_ulonglong),
            ("available_virtual", ctypes.c_ulonglong),
            ("available_extended_virtual", ctypes.c_ulonglong),
        ]

    try:
        status = MemoryStatus(length=ctypes.sizeof(MemoryStatus))
        if ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
            return int(status.available_physical)
    except (AttributeError, OSError):
        pass
    return None


def _host_available_memory(system_name: str) -> int | None:
    if system_name == "linux":
        value = _read(Path("/proc/meminfo"))
        return parse_linux_mem_available(value) if value else None
    if system_name == "darwin":
        try:
            result = subprocess.run(
                ["vm_stat"], check=False, capture_output=True, text=True
            )
        except OSError:
            return None
        return parse_vm_stat(result.stdout) if result.returncode == 0 else None
    return _windows_available_memory() if system_name == "windows" else None


def detect_resources(system: str | None = None) -> ResourceSnapshot:
    system_name = (system or platform.system()).lower()
    memory_limits = [_host_available_memory(system_name)]
    if system_name == "linux":
        memory_limits.append(_cgroup_memory_available())
    known_memory = [value for value in memory_limits if value is not None]
    return ResourceSnapshot(
        logical_cpus=max(1, os.cpu_count() or 1),
        affinity_cpus=_affinity_cpu_count(),
        quota_cpus=_cgroup_cpu_limit() if system_name == "linux" else None,
        available_memory_bytes=min(known_memory) if known_memory else None,
    )


def choose_parallel_jobs(resources: ResourceSnapshot) -> ParallelDecision:
    memory_limit = None
    reserve = None
    if resources.available_memory_bytes is not None:
        reserve = max(
            MINIMUM_RESERVE,
            math.ceil(resources.available_memory_bytes * RESERVE_FRACTION),
        )
        memory_limit = max(
            1, (max(0, resources.available_memory_bytes - reserve)) // MEMORY_PER_JOB
        )
    jobs = min(resources.cpu_limit, memory_limit or resources.cpu_limit)
    return ParallelDecision(
        jobs=max(1, jobs),
        cpu_limit=resources.cpu_limit,
        memory_limit=memory_limit,
        available_memory_bytes=resources.available_memory_bytes,
        reserve_memory_bytes=reserve,
    )


def requested_jobs(
    command_line_value: str | None, environment: Mapping[str, str]
) -> tuple[int | None, str]:
    if command_line_value is not None:
        value, source = command_line_value, "--jobs"
    elif environment.get("FLUENTQT_BUILD_JOBS"):
        value, source = environment["FLUENTQT_BUILD_JOBS"], "FLUENTQT_BUILD_JOBS"
    elif environment.get("CMAKE_BUILD_PARALLEL_LEVEL"):
        value = environment["CMAKE_BUILD_PARALLEL_LEVEL"]
        source = "CMAKE_BUILD_PARALLEL_LEVEL"
    else:
        return None, "automatic resource detection"

    if value.lower() == "auto":
        return None, f"automatic detection requested by {source}"
    try:
        jobs = int(value)
    except ValueError as error:
        raise ValueError(f"{source} must be 'auto' or a positive integer") from error
    if jobs <= 0:
        raise ValueError(f"{source} must be 'auto' or a positive integer")
    return jobs, f"{source} override"


def create_build_command(arguments: list[str], jobs: int) -> list[str]:
    insertion = arguments.index("--") if "--" in arguments else len(arguments)
    for value in arguments:
        if (
            value in {"--parallel", "-j"}
            or value.startswith("--parallel=")
            or re.match(r"^-j\d+$", value)
        ):
            raise ValueError("pass the parallel override as --jobs N, not --parallel")
    return [
        "cmake",
        "--build",
        *arguments[:insertion],
        "--parallel",
        str(jobs),
        *arguments[insertion:],
    ]


def describe_decision(
    decision: ParallelDecision, resources: ResourceSnapshot, reason: str
) -> str:
    if reason.endswith("override"):
        return f"{decision.jobs} ({reason})"
    details = [f"CPU limit {decision.cpu_limit}"]
    if resources.affinity_cpus is not None:
        details.append(f"affinity {resources.affinity_cpus}")
    if resources.quota_cpus is not None:
        details.append(f"cgroup quota {resources.quota_cpus}")
    if decision.memory_limit is None:
        details.append("available memory unknown")
    else:
        assert decision.available_memory_bytes is not None
        assert decision.reserve_memory_bytes is not None
        details.append(
            f"memory limit {decision.memory_limit} from "
            f"{decision.available_memory_bytes / GIB:.1f} GiB available, "
            f"{decision.reserve_memory_bytes / GIB:.1f} GiB reserved, "
            f"{MEMORY_PER_JOB / GIB:.1f} GiB/job"
        )
    return f"{decision.jobs} ({'; '.join(details)})"


def parse_args(
    argv: Iterable[str] | None = None,
) -> tuple[argparse.Namespace, list[str], argparse.ArgumentParser]:
    parser = argparse.ArgumentParser(
        description=__doc__,
        usage="%(prog)s [--jobs N|auto] [--print-jobs] <cmake --build arguments>",
        epilog=(
            "example: python3 tools/dev/fluent_qt_build.py --preset "
            "vcpkg-osx --target fluent_qt_gallery"
        ),
        allow_abbrev=False,
    )
    parser.add_argument("--jobs", metavar="N|auto")
    parser.add_argument("--print-jobs", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    options, build_arguments = parser.parse_known_args(
        list(argv) if argv is not None else None
    )
    return options, build_arguments, parser


def main(argv: Iterable[str] | None = None) -> int:
    options, build_arguments, parser = parse_args(argv)
    try:
        override, reason = requested_jobs(options.jobs, os.environ)
    except ValueError as error:
        parser.error(str(error))
    resources = detect_resources()
    decision = choose_parallel_jobs(resources)
    if override is not None:
        decision = ParallelDecision(
            override,
            decision.cpu_limit,
            decision.memory_limit,
            decision.available_memory_bytes,
            decision.reserve_memory_bytes,
        )

    if options.print_jobs:
        print(decision.jobs)
        return 0
    if not build_arguments:
        parser.error("provide a build directory or --preset")
    try:
        command = create_build_command(build_arguments, decision.jobs)
    except ValueError as error:
        parser.error(str(error))

    print(
        "[FluentQt build] parallel jobs: "
        + describe_decision(decision, resources, reason),
        flush=True,
    )
    print("[FluentQt build] " + shlex.join(command), flush=True)
    if options.dry_run:
        return 0
    try:
        return subprocess.run(command, cwd=PROJECT_ROOT, check=False).returncode
    except KeyboardInterrupt:
        print("[FluentQt build] interrupted by user.", file=sys.stderr)
        return 130
    except OSError as error:
        print(f"[FluentQt build] failed to start CMake: {error}", file=sys.stderr)
        return 127


if __name__ == "__main__":
    raise SystemExit(main())
