"""Bounded JSON and PNG readers, artifact digests, and atomic writes."""

from __future__ import annotations

from pathlib import Path
from typing import Any
import hashlib
import json
import os
import re
import struct
import tempfile
import zlib

from .common import MAX_JSON_INTEGER_DIGITS, MAX_JSON_NESTING_DEPTH, VerificationError


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def png_dimensions(path: Path) -> tuple[int, int] | None:
    try:
        payload = path.read_bytes()
    except OSError:
        return None
    if len(payload) < 8 or payload[:8] != b"\x89PNG\r\n\x1a\n":
        return None

    offset = 8
    ihdr: bytes | None = None
    idat_chunks: list[bytes] = []
    seen_plte = False
    seen_idat = False
    idat_closed = False
    seen_iend = False
    while offset < len(payload):
        if offset + 12 > len(payload):
            return None
        length = struct.unpack(">I", payload[offset : offset + 4])[0]
        chunk_end = offset + 12 + length
        if chunk_end > len(payload):
            return None
        chunk_type = payload[offset + 4 : offset + 8]
        if (
            re.fullmatch(rb"[A-Za-z]{4}", chunk_type) is None
            or not 65 <= chunk_type[2] <= 90
        ):
            return None
        chunk_payload = payload[offset + 8 : offset + 8 + length]
        recorded_crc = struct.unpack(">I", payload[offset + 8 + length : chunk_end])[0]
        computed_crc = zlib.crc32(chunk_type)
        computed_crc = zlib.crc32(chunk_payload, computed_crc) & 0xFFFFFFFF
        if recorded_crc != computed_crc:
            return None
        if ihdr is None and chunk_type != b"IHDR":
            return None
        if chunk_type == b"IHDR":
            if ihdr is not None or length != 13 or offset != 8:
                return None
            ihdr = chunk_payload
        elif chunk_type == b"PLTE":
            if seen_idat or seen_plte or not length or length % 3 or length > 768:
                return None
            seen_plte = True
        elif chunk_type == b"IDAT":
            if ihdr is None or idat_closed or seen_iend:
                return None
            seen_idat = True
            idat_chunks.append(chunk_payload)
        elif chunk_type == b"IEND":
            if length or not seen_idat or seen_iend:
                return None
            seen_iend = True
            offset = chunk_end
            break
        else:
            if seen_idat:
                idat_closed = True
            if chunk_type and 65 <= chunk_type[0] <= 90:
                return None
        offset = chunk_end

    if ihdr is None or not seen_iend or offset != len(payload):
        return None
    width, height, bit_depth, color_type, compression, filtering, interlace = (
        struct.unpack(">IIBBBBB", ihdr)
    )
    valid_depths = {
        0: {1, 2, 4, 8, 16},
        2: {8, 16},
        3: {1, 2, 4, 8},
        4: {8, 16},
        6: {8, 16},
    }
    if (
        width <= 0
        or height <= 0
        or width > 32768
        or height > 32768
        or color_type not in valid_depths
        or bit_depth not in valid_depths[color_type]
        or compression != 0
        or filtering != 0
        or interlace not in {0, 1}
        or (color_type == 3 and not seen_plte)
        or (color_type in {0, 4} and seen_plte)
    ):
        return None

    samples = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color_type]

    def row_size(row_width: int) -> int:
        return 1 + ((row_width * samples * bit_depth + 7) // 8)

    scanline_sizes: list[int] = []
    if interlace == 0:
        scanline_sizes = [row_size(width)] * height
    else:
        for start_x, start_y, step_x, step_y in (
            (0, 0, 8, 8),
            (4, 0, 8, 8),
            (0, 4, 4, 8),
            (2, 0, 4, 4),
            (0, 2, 2, 4),
            (1, 0, 2, 2),
            (0, 1, 1, 2),
        ):
            pass_width = max(0, (width - start_x + step_x - 1) // step_x)
            pass_height = max(0, (height - start_y + step_y - 1) // step_y)
            if pass_width:
                scanline_sizes.extend([row_size(pass_width)] * pass_height)
    expected_bytes = sum(scanline_sizes)
    if expected_bytes <= 0 or expected_bytes > 256 * 1024 * 1024:
        return None

    filter_offsets: list[int] = []
    scanline_offset = 0
    for size in scanline_sizes:
        filter_offsets.append(scanline_offset)
        scanline_offset += size
    filter_index = 0
    decoded_bytes = 0

    def consume(decoded: bytes) -> bool:
        nonlocal decoded_bytes, filter_index
        if decoded_bytes + len(decoded) > expected_bytes:
            return False
        end = decoded_bytes + len(decoded)
        while filter_index < len(filter_offsets) and filter_offsets[filter_index] < end:
            position = filter_offsets[filter_index] - decoded_bytes
            if position >= 0 and decoded[position] > 4:
                return False
            filter_index += 1
        decoded_bytes = end
        return True

    try:
        decoder = zlib.decompressobj()
        for compressed in idat_chunks:
            remaining = compressed
            while remaining:
                decoded = decoder.decompress(remaining, 1024 * 1024)
                remaining = decoder.unconsumed_tail
                if not consume(decoded):
                    return None
                if not decoded and not remaining:
                    break
        if not consume(decoder.flush()):
            return None
    except zlib.error:
        return None
    if (
        not decoder.eof
        or decoder.unused_data
        or decoded_bytes != expected_bytes
        or filter_index != len(filter_offsets)
    ):
        return None
    return width, height


def canonical_json(value: object) -> bytes:
    return json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")


def parse_json_integer(raw: str) -> int:
    digits = raw[1:] if raw.startswith("-") else raw
    if len(digits) > MAX_JSON_INTEGER_DIGITS:
        raise ValueError(
            f"JSON integer exceeds {MAX_JSON_INTEGER_DIGITS} digits"
        )
    return int(raw)


def validate_json_nesting(value: object) -> None:
    pending: list[tuple[object, int]] = [(value, 1)]
    while pending:
        current, depth = pending.pop()
        if depth > MAX_JSON_NESTING_DEPTH:
            raise ValueError(
                f"JSON nesting exceeds {MAX_JSON_NESTING_DEPTH} levels"
            )
        if isinstance(current, dict):
            pending.extend((item, depth + 1) for item in current.values())
        elif isinstance(current, list):
            pending.extend((item, depth + 1) for item in current)


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(
            path.read_text(encoding="utf-8"), parse_int=parse_json_integer
        )
        validate_json_nesting(value)
    except OSError as error:
        raise VerificationError(f"Could not read JSON {path}: {error}") from error
    except (ValueError, RecursionError) as error:
        raise VerificationError(f"Could not parse JSON {path}: {error}") from error
    if not isinstance(value, dict):
        raise VerificationError(f"JSON root must be an object: {path}")
    return value


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            dir=path.parent,
            prefix=f".{path.name}.",
            suffix=".tmp",
            delete=False,
        ) as stream:
            json.dump(value, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
            temporary = Path(stream.name)
        temporary.replace(path)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def resolved_path(raw: str | os.PathLike[str], base: Path) -> Path:
    path = Path(raw).expanduser()
    return (base / path).resolve() if not path.is_absolute() else path.resolve()


def path_is_within(path: Path, directory: Path) -> bool:
    try:
        path.resolve().relative_to(directory.resolve())
    except ValueError:
        return False
    return True


def paths_overlap(first: Path, second: Path) -> bool:
    return path_is_within(first, second) or path_is_within(second, first)
