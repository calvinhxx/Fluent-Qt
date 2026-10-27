#!/usr/bin/env python3
"""Capture real Gallery widgets for the static component documentation."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import sys
import zlib

from component_inventory import ROOT, CONFIG, config

ASSETS = ROOT / "site/assets/component-previews"
MANIFEST = ASSETS / "manifest.json"
PLACEHOLDER_BACKGROUND = {"light": "#f9f9f9", "dark": "#3d3d3d"}
CAPTURE_ENVIRONMENT = (
    "QT_VERSION", "QT_QPA_PLATFORM", "QT_SCALE_FACTOR", "QT_SCREEN_SCALE_FACTORS",
    "QT_FONT_DPI", "QT_AUTO_SCREEN_SCALE_FACTOR", "QT_ENABLE_HIGHDPI_SCALING",
    "QT_SCALE_FACTOR_ROUNDING_POLICY", "QT_QUICK_BACKEND", "QSG_RHI_BACKEND",
    "QT_OPENGL", "LIBGL_ALWAYS_SOFTWARE", "LANG", "LC_ALL",
)


def capture_settings():
    # DISPLAY is intentionally excluded: Xvfb chooses a fresh display per run.
    return {"platform": sys.platform, "architecture": platform.machine(),
            "environment": {name: os.environ.get(name, "") for name in CAPTURE_ENVIRONMENT}}


def preview_path(component_id, preview_id, theme):
    return f"assets/component-previews/{component_id}/{preview_id}-{theme}.png"


def capture_inputs():
    paths = {
        "tools/site/generate_component_previews.py",
        "CMakeLists.txt", "CMakePresets.json", "resources.qrc",
        ".github/scripts/install-linux-dependencies.sh",
    }
    # The native target also depends on bootstrap, headers, support and resources
    # outside samples/. Keep the owning trees instead of a partial .cpp allowlist.
    for directory in ("src", "include", "res", "cmake", "uilib", "plugin", "support/logging", "app"):
        paths.update(str(p.relative_to(ROOT)) for p in (ROOT / directory).rglob("*")
                     if p.is_file() and not any(part.startswith(".") or part == "__pycache__"
                                                for part in p.relative_to(ROOT).parts))
    return sorted(paths)


def capture_digest(include_recipes=True):
    digest = hashlib.sha256()
    digest.update(json.dumps(capture_settings(), sort_keys=True).encode() + b"\0")
    for path in capture_inputs():
        digest.update(path.encode() + b"\0" + (ROOT / path).read_bytes())
    if include_recipes:
        recipes = [{"id": c["id"], "previews": [{k: v for k, v in p.items() if k not in ("name", "alt")}
                    for p in c["previews"]]} for c in config()]
        digest.update(json.dumps(recipes, sort_keys=True).encode())
    return digest.hexdigest()


def png_size(path):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"Not a PNG: {path}")
    return struct.unpack(">II", data[16:24])


def png_background(path):
    """Read the opaque capture margin without adding an imaging dependency."""
    data = path.read_bytes()
    if data[24] != 8 or data[25] not in (2, 6):
        raise ValueError(f"Expected an 8-bit RGB/RGBA native capture: {path}")
    compressed = bytearray()
    offset = 8
    while offset < len(data):
        length = struct.unpack(">I", data[offset:offset + 4])[0]
        if data[offset + 4:offset + 8] == b"IDAT":
            compressed.extend(data[offset + 8:offset + 8 + length])
        offset += length + 12
    # Every PNG filter predicts zero for the very first pixel.
    scanline = zlib.decompress(compressed)
    return "#" + scanline[1:4].hex()


def validated_manifest():
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if data.get("capture_settings") != capture_settings() or data.get("input_digest") != capture_digest():
        raise ValueError("Component previews are stale; run tools/site/generate_component_previews.py")
    expected = {(c["id"], p["id"], theme) for c in config() for p in c["previews"] for theme in ("light", "dark")}
    found = set()
    for item in data["images"]:
        key = (item["component"], item["state"], item["theme"])
        if key in found:
            raise ValueError(f"Duplicate preview: {key}")
        found.add(key)
        path = ROOT / "site" / item["path"]
        if not path.resolve().is_relative_to(ASSETS.resolve()):
            raise ValueError("Preview path escapes its asset directory")
        if hashlib.sha256(path.read_bytes()).hexdigest() != item["sha256"]:
            raise ValueError(f"Preview image does not match its capture: {path}")
        if list(png_size(path)) != item["pixel_size"]:
            raise ValueError(f"Preview dimensions changed: {path}")
    if found != expected:
        raise ValueError(f"Preview coverage differs: {found ^ expected}")
    return data


def available_previews():
    """Return local captures for draft documents, with placeholders where none fit.

    Images whose bytes still match the manifest are reused even when their inputs
    changed, so editing prose never needs a native Gallery build. Publication
    uses validated_manifest(), which accepts neither stale nor missing images.
    """
    try:
        data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        data = {}
    usable = {}
    for item in data.get("images", []):
        try:
            path = ROOT / "site" / item["path"]
            if (path.resolve().is_relative_to(ASSETS.resolve())
                    and hashlib.sha256(path.read_bytes()).hexdigest() == item["sha256"]):
                usable[(item["component"], item["state"], item["theme"])] = item
        except (OSError, KeyError, TypeError):
            continue
    images, missing = [], 0
    for component in config():
        for preview in component["previews"]:
            for theme in ("light", "dark"):
                key = (component["id"], preview["id"], theme)
                if key not in usable:
                    missing += 1
                images.append(usable.get(key) or {
                    "component": key[0], "state": key[1], "theme": theme, "path": preview_path(*key),
                    "sha256": "", "logical_size": [480, 120], "background": PLACEHOLDER_BACKGROUND[theme]})
    stale = bool(usable) and data.get("input_digest") != capture_digest()
    notice = (f"Draft previews: {len(images) - missing} reused{' from stale inputs' if stale else ''}, "
              f"{missing} placeholders. Publication recaptures them." if stale or missing else "")
    return {"images": images, "notice": notice}


def run_capture(command, log, attempts=2):
    """Run one native capture, retrying once because Xvfb and GL start-up can stall."""
    for attempt in range(1, attempts + 1):
        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=60, cwd=ROOT)
        except subprocess.TimeoutExpired:
            log.write_text("Capture timed out after 60 seconds\n", encoding="utf-8")
        else:
            log.write_text(result.stdout + result.stderr, encoding="utf-8")
            if result.returncode == 0:
                return True
        if attempt < attempts:
            print(f"Retrying capture {attempt + 1}/{attempts}: {log.stem}", flush=True)
    return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", default="vcpkg-osx")
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--executable", type=Path, help="Use an already built native Gallery (CI or a custom build directory).")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if args.check:
        data = validated_manifest()
        print(f"Verified {len(data['images'])} native component previews")
        return
    if not args.no_build and not args.executable:
        subprocess.run([sys.executable, str(ROOT / "tools/dev/fluent_qt_build.py"), "--preset", args.preset,
                        "--target", "fluent_qt_gallery"], check=True, cwd=ROOT)
    sys.path.insert(0, str(ROOT / "tools/dev"))
    from fluent_qt_preview import resolve_gallery_executable
    executable = args.executable.resolve() if args.executable else resolve_gallery_executable(ROOT / "build" / args.preset)
    binary_digest = hashlib.sha256(executable.read_bytes()).hexdigest()
    output = ROOT / "build/component-docs"
    output.mkdir(parents=True, exist_ok=True)
    staging = output / ("capture-" + capture_digest(False)[:12] + "-" + binary_digest[:12])
    staging.mkdir(exist_ok=True)
    images = []
    failures = []
    input_digest = capture_digest()
    for component in config():
        for preview in component["previews"]:
            for theme in ("light", "dark"):
                key = f"{component['id']}-{preview['id']}-{theme}"
                png, report = staging / f"{key}.png", staging / f"{key}.json"
                size = preview.get("capture_size", "1400x1200" if component.get("snippet_only") else "920x460")
                recipe = hashlib.sha256(json.dumps({"preview": preview, "theme": theme, "size": size}, sort_keys=True).encode()).hexdigest()
                stamp = staging / f"{key}.recipe"
                command = [str(executable), "--preview", "--route", component["id"], "--sample", preview["sample"],
                           "--theme", theme, "--size", size, "--settle-ms", "650",
                           "--capture-target", "gallerySamplePreviewWidget", "--capture-padding", "16",
                           "--snapshot", str(png), "--report", str(report)]
                if preview.get("actions"):
                    actions = staging / f"{key}-actions.json"
                    actions.write_text(json.dumps({"schema_version": 1, "steps": preview["actions"]}), encoding="utf-8")
                    command += ["--actions", str(actions)]
                if not (png.exists() and report.exists() and stamp.exists() and stamp.read_text() == recipe):
                    log = staging / f"{key}.log"
                    if not run_capture(command, log):
                        failures.append(key)
                        print(f"Capture failed: {key}; see {log}", flush=True)
                        continue
                data = json.loads(report.read_text())
                environment = data["environment"]
                if data["status"] != "ok" or environment["platform_plugin"] in ("offscreen", "minimal", "vnc"):
                    raise ValueError(f"A successful native capture is required: {key}")
                expected_qt = os.environ.get("QT_VERSION")
                if expected_qt and environment["qt_version"] != expected_qt:
                    raise ValueError(f"Expected Qt {expected_qt}, captured {environment['qt_version']}: {key}")
                width, height = png_size(png)
                dpr = environment["device_pixel_ratio"]
                if data["artifacts"]["snapshot"]["target"] != "gallerySamplePreviewWidget":
                    raise ValueError(f"Capture must contain the real sample: {key}")
                stamp.write_text(recipe)
                images.append({"component": component["id"], "state": preview["id"], "sample": preview["sample"],
                               "theme": theme, "path": preview_path(component["id"], preview["id"], theme),
                               "sha256": hashlib.sha256(png.read_bytes()).hexdigest(), "pixel_size": [width, height],
                               "logical_size": [round(width / dpr), round(height / dpr)], "dpr": dpr,
                               "background": png_background(png),
                               "qt_version": environment["qt_version"], "platform": environment["platform_plugin"],
                               "font": environment["font"], "locale": environment["locale"],
                               "interaction_checked": bool(preview.get("actions"))})
                print(f"Captured {key}: {width}x{height}", flush=True)
    if failures:
        raise RuntimeError(f"Native captures failed ({len(failures)}): {', '.join(failures)}; successful captures are cached in {staging}")
    if input_digest != capture_digest():
        raise RuntimeError("Capture inputs changed during generation")
    if binary_digest != hashlib.sha256(executable.read_bytes()).hexdigest():
        raise RuntimeError("Gallery executable changed during capture; rerun after the build finishes")
    for item in images:
        destination = ROOT / "site" / item["path"]
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(staging / f"{item['component']}-{item['state']}-{item['theme']}.png", destination)
    manifest = {"schema_version": 2, "input_digest": input_digest, "capture_settings": capture_settings(),
                "source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                "source_worktree_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT).strip()),
                "binary_sha256": binary_digest,
                "capture_tool": "FluentQt native Gallery preview", "images": images}
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    validated_manifest()


if __name__ == "__main__":
    main()
