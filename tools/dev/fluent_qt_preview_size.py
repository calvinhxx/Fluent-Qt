"""Window-size arguments shared by the native and Live Scene preview tools."""

from __future__ import annotations

import argparse


def parse_dimensions(value: str, minimum: tuple[int, int] = (520, 420)) -> tuple[int, int]:
    try:
        width, height = map(int, value.lower().split("x", 1))
    except ValueError as error:
        raise argparse.ArgumentTypeError("size must be WIDTHxHEIGHT") from error
    min_width, min_height = minimum
    if not min_width <= width <= 3840 or not min_height <= height <= 2160:
        raise argparse.ArgumentTypeError(f"size must be within {min_width}x{min_height} and 3840x2160")
    return width, height


def parse_size(value: str, minimum: tuple[int, int] = (520, 420)) -> str:
    width, height = parse_dimensions(value, minimum)
    return f"{width}x{height}"
