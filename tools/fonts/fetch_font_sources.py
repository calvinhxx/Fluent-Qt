#!/usr/bin/env python3
"""Restore pinned font-generation inputs; networking requires --download."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import tempfile
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = Path(__file__).with_name("source-assets.json")
# text/icons feed the desktop resources; web-fallback feeds only the WASM CJK subset.
ROLES = {"text", "icons", "web-fallback"}


def load_assets(path: Path = MANIFEST) -> dict[str, dict]:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1:
        raise ValueError("Unsupported font source manifest")
    assets = {}
    for asset in manifest["assets"]:
        relative = asset["path"]
        parts = PurePosixPath(relative)
        if (parts.is_absolute() or ".." in parts.parts or "\\" in relative
                or not relative.startswith(("third_party/fonts/", "third_party/icons/"))
                or relative in assets):
            raise ValueError(f"Invalid or duplicate font source path: {relative}")
        if (not re.fullmatch(r"[0-9a-f]{64}", asset["sha256"])
                or not re.fullmatch(r"[0-9a-f]{40}", asset["git_blob"])
                or not isinstance(asset["bytes"], int) or asset["bytes"] <= 0
                or not asset["url"].startswith("https://")
                or asset.get("role") not in ROLES):
            raise ValueError(f"Invalid font source metadata: {relative}")
        assets[relative] = asset
    if not assets:
        raise ValueError("Font source manifest is empty")
    return assets


def verified(data: bytes, asset: dict) -> bool:
    return len(data) == asset["bytes"] and hashlib.sha256(data).hexdigest() == asset["sha256"]


class FontSources:
    def __init__(self, root: Path = ROOT, cache_dir: Path | None = None,
                 assets: dict[str, dict] | None = None):
        self.root = root
        configured = os.environ.get("FLUENTQT_FONT_SOURCE_CACHE")
        self.cache = Path(cache_dir or configured or root / "build/font-sources").resolve()
        self.assets = load_assets() if assets is None else assets

    def path(self, relative: str) -> Path:
        if relative not in self.assets:
            raise ValueError(f"Unknown font source: {relative}")
        return self.cache / relative

    def paths(self, *roles: str) -> list[str]:
        return [relative for relative, asset in self.assets.items() if asset["role"] in roles]

    def _git(self, *arguments: str) -> bytes | None:
        environment = dict(os.environ, GIT_NO_LAZY_FETCH="1", GIT_TERMINAL_PROMPT="0")
        try:
            result = subprocess.run(["git", "-C", str(self.root), *arguments],
                                    capture_output=True, env=environment, timeout=60)
        except (OSError, subprocess.TimeoutExpired):
            return None
        return result.stdout if result.returncode == 0 else None

    def prepare(self, paths: list[str] | None = None, *, download: bool = False) -> list[Path]:
        selected = list(self.assets) if paths is None else paths
        available = None
        outputs = []
        for relative in selected:
            destination = self.path(relative)
            asset = self.assets[relative]
            if destination.is_file() and verified(destination.read_bytes(), asset):
                outputs.append(destination)
                continue
            # --batch-all-objects enumerates only local objects, including in a
            # partial clone. Do not ask cat-file for a missing promisor object:
            # older Git versions may fetch it even with GIT_NO_LAZY_FETCH set.
            if available is None:
                inventory = self._git("cat-file", "--batch-all-objects", "--unordered",
                                      "--batch-check=%(objectname)")
                available = set((inventory or b"").splitlines())
            data = None
            if asset["git_blob"].encode() in available:
                candidate = self._git("cat-file", "blob", asset["git_blob"])
                if candidate is not None and verified(candidate, asset):
                    data = candidate
            if data is None and download:
                with urlopen(asset["url"], timeout=60) as response:
                    candidate = response.read(asset["bytes"] + 1)
                if not verified(candidate, asset):
                    raise ValueError(f"Font source download failed SHA-256/size verification: {relative}")
                data = candidate
            if data is None:
                raise FileNotFoundError(
                    f"Missing verified font input: {relative}. Run "
                    "python3 tools/fonts/fetch_font_sources.py --download, or provide "
                    "a verified cache through FLUENTQT_FONT_SOURCE_CACHE."
                )
            destination.parent.mkdir(parents=True, exist_ok=True)
            temporary = None
            try:
                with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as stream:
                    temporary = Path(stream.name)
                    stream.write(data)
                os.replace(temporary, destination)
            finally:
                if temporary is not None:
                    temporary.unlink(missing_ok=True)
            outputs.append(destination)
        return outputs


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--download", action="store_true", help="Allow HTTPS when a verified local input is unavailable")
    parser.add_argument("--cache-dir", type=Path, help="Override the cache location (also FLUENTQT_FONT_SOURCE_CACHE)")
    args = parser.parse_args()
    try:
        paths = FontSources(cache_dir=args.cache_dir).prepare(download=args.download)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    for path in paths:
        print(path)


if __name__ == "__main__":
    main()
