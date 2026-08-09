#!/usr/bin/env python3
"""Install exact release packages while package-index edges converge."""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from collections.abc import Callable, Sequence


class ReleaseInstallError(RuntimeError):
    """Raised when an exact release cannot be installed after all retries."""


def positive_integer(value: str) -> int:
    parsed = int(value)
    if parsed < 1:
        raise argparse.ArgumentTypeError("value must be at least 1")
    return parsed


def nonnegative_float(value: str) -> float:
    parsed = float(value)
    if parsed < 0:
        raise argparse.ArgumentTypeError("value must not be negative")
    return parsed


def exact_requirement(value: str) -> str:
    if "==" not in value:
        raise argparse.ArgumentTypeError(
            "release package requirements must pin an exact version with =="
        )
    return value


def build_command(
    python: str,
    packages: Sequence[str],
    *,
    index_url: str | None,
    no_deps: bool,
) -> list[str]:
    command = [
        python,
        "-m",
        "pip",
        "install",
        "--disable-pip-version-check",
        "--no-cache-dir",
        "--only-binary=:all:",
    ]
    if index_url:
        command.extend(("--index-url", index_url))
    if no_deps:
        command.append("--no-deps")
    command.extend(packages)
    return command


def install_with_retry(
    command: Sequence[str],
    attempts: int,
    delay_seconds: float,
    *,
    runner: Callable[..., subprocess.CompletedProcess] = subprocess.run,
    sleeper: Callable[[float], None] = time.sleep,
) -> None:
    last_exit_code = 1
    for attempt in range(1, attempts + 1):
        print(f"Package-index install attempt {attempt}/{attempts}.", flush=True)
        result = runner(list(command), check=False)
        last_exit_code = result.returncode
        if last_exit_code == 0:
            return
        if attempt < attempts:
            print(
                "Package-index edge is not installable yet; "
                f"retrying in {delay_seconds:g} seconds.",
                file=sys.stderr,
                flush=True,
            )
            sleeper(delay_seconds)
    raise ReleaseInstallError(
        f"pip install failed after {attempts} attempts "
        f"(last exit code {last_exit_code})"
    )


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--python", required=True, help="Target virtualenv Python")
    parser.add_argument("--index-url", help="PEP 503 package-index URL")
    parser.add_argument("--no-deps", action="store_true")
    parser.add_argument("--attempts", type=positive_integer, default=20)
    parser.add_argument("--delay-seconds", type=nonnegative_float, default=15)
    parser.add_argument(
        "--package",
        action="append",
        required=True,
        type=exact_requirement,
        dest="packages",
        help="Exact distribution requirement; repeat for each package",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    command = build_command(
        args.python,
        args.packages,
        index_url=args.index_url,
        no_deps=args.no_deps,
    )
    try:
        install_with_retry(command, args.attempts, args.delay_seconds)
    except ReleaseInstallError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
