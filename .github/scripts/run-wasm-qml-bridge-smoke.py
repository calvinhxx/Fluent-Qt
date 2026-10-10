#!/usr/bin/env python3
"""Check QML bridge example and Gallery composition with real canvas input."""

from __future__ import annotations

import argparse
import base64
from functools import partial
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import math
from pathlib import Path
import re
from threading import Thread

from playwright.sync_api import sync_playwright


QML_SAMPLE_IDS = (
    "qml-view-load", "qml-view-theme", "qml-view-context", "qml-view-action",
    "qml-view-lifecycle", "qml-view-error",
)
QML_SAMPLE_ASSETS = dict(zip(QML_SAMPLE_IDS, (
    "Basic.qml", "Theme.qml", "Context.qml", "Action.qml", "Basic.qml", "Basic.qml",
)))


def source_fingerprint(source: str) -> dict:
    encoded = source.encode("utf-8")
    return {"sha256": hashlib.sha256(encoded).hexdigest(),
            "lines": source.count("\n") + (0 if not source or source.endswith("\n") else 1),
            "bytes": len(encoded)}


def validate_source_fingerprint(fingerprint: object, label: str, *, allow_empty=False) -> None:
    if (not isinstance(fingerprint, dict) or set(fingerprint) != {"sha256", "lines", "bytes"}
            or not isinstance(fingerprint.get("sha256"), str)
            or re.fullmatch(r"[0-9a-f]{64}", fingerprint["sha256"]) is None
            or any(type(fingerprint.get(field)) is not int or fingerprint[field] < 0
                   for field in ("lines", "bytes"))):
        raise RuntimeError(f"{label}: invalid source fingerprint: {fingerprint}")
    if fingerprint["lines"] == 0 or fingerprint["bytes"] == 0:
        if not allow_empty or fingerprint != source_fingerprint(""):
            raise RuntimeError(f"{label}: empty source fingerprint is invalid: {fingerprint}")


def validate_source_contract(contract: object, expected: dict | None = None) -> None:
    if not isinstance(contract, dict) or set(contract) != set(QML_SAMPLE_IDS):
        raise RuntimeError("QML source contract must contain exactly the six atomic sample IDs")
    for sample_id in QML_SAMPLE_IDS:
        sample = contract[sample_id]
        if not isinstance(sample, dict) or set(sample) != {"full", "key"}:
            raise RuntimeError(f"{sample_id}: missing Full example or Key usage fingerprint")
        for kind in ("full", "key"):
            fingerprint = sample[kind]
            validate_source_fingerprint(fingerprint, f"{sample_id} {kind}")
            if expected is not None and fingerprint != expected[sample_id][kind]:
                raise RuntimeError(f"{sample_id}: browser {kind} source differs from compiled native "
                                   f"Gallery source: {fingerprint}; expected {expected[sample_id][kind]}")
        if (sample["key"]["sha256"] == sample["full"]["sha256"]
                or sample["key"]["lines"] >= sample["full"]["lines"]
                or sample["key"]["bytes"] >= sample["full"]["bytes"]):
            raise RuntimeError(f"{sample_id}: Key usage must differ from and be shorter than Full example")


def validate_source_file_contract(contract: object, sources: dict,
                                  expected: dict | None = None) -> None:
    validate_source_contract(sources)
    if not isinstance(contract, dict) or set(contract) != set(QML_SAMPLE_IDS):
        raise RuntimeError("QML source file contract must contain exactly the six atomic sample IDs")
    for sample_id in QML_SAMPLE_IDS:
        files = contract[sample_id]
        names = ["main.cpp", QML_SAMPLE_ASSETS[sample_id], "qml.qrc", "CMakeLists.txt"]
        if not isinstance(files, list) or len(files) != len(names):
            raise RuntimeError(f"{sample_id}: Full example must expose four independent source files")
        for index, (file, name, language) in enumerate(zip(files, names, ("cpp", "qml", "xml", "cmake"))):
            if (not isinstance(file, dict) or set(file) != {"name", "language", "code", "usage"}
                    or file["name"] != name or file["language"] != language):
                raise RuntimeError(f"{sample_id}: source file has the wrong name or language: {file}")
            validate_source_fingerprint(file["code"], f"{sample_id}/{name} code")
            validate_source_fingerprint(file["usage"], f"{sample_id}/{name} usage", allow_empty=True)
            if index == 0:
                if file["code"] != sources[sample_id]["full"] or file["usage"] != sources[sample_id]["key"]:
                    raise RuntimeError(f"{sample_id}: main.cpp must use this card's actual Full/Key source")
            elif file["usage"] != source_fingerprint(""):
                raise RuntimeError(f"{sample_id}/{name}: only the host file has a Key usage excerpt")
        if expected is not None and files != expected[sample_id]:
            raise RuntimeError(f"{sample_id}: browser source files differ from the native Gallery: "
                               f"{files}; expected {expected[sample_id]}")


def validate_python_source_files(contract: object) -> None:
    rows = json.loads((Path(__file__).resolve().parents[2] /
                       "app/data/gallery_python_snippets.json").read_text("utf-8"))["samples"]
    expected = {row["sample_id"]: row["source_files"] for row in rows
                if row["route_id"] == "qml-view"}
    if not isinstance(contract, dict) or set(contract) != set(QML_SAMPLE_IDS):
        raise RuntimeError("Browser QML cards must expose all six Python host file bundles")
    for sample_id, files in expected.items():
        fingerprints = [{"name": file["name"], "language": file["language"],
                         "code": source_fingerprint(file["code"]),
                         "usage": source_fingerprint(file.get("usage_snippet", ""))}
                        for file in files]
        if contract[sample_id] != fingerprints:
            raise RuntimeError(f"{sample_id}: browser Python files are missing or incomplete")


def validate_source_selection(selection: object, file: dict, mode: str) -> None:
    if (not isinstance(selection, dict) or selection.get("currentFileName") != file["name"]
            or selection.get("sourceLanguage") != file["language"]
            or selection.get("sourceMode") != mode
            or selection.get("codeFingerprint") != file["usage" if mode == "key" else "code"]):
        raise RuntimeError(f"Selected source does not match {file['name']} in {mode} mode: {selection}")


def _read_native_source_case(path: Path) -> dict:
    report = json.loads(path.read_text(encoding="utf-8"))
    if (not isinstance(report, dict) or report.get("failures") != 0
            or report.get("errors") != 0 or not isinstance(report.get("testsuites"), list)):
        raise RuntimeError("Native source contract requires a successful GTest JSON report")
    cases = []
    for suite in report["testsuites"]:
        if not isinstance(suite, dict) or suite.get("name") != "GalleryQmlBridgeTest":
            continue
        for case in suite.get("testsuite", []):
            if (isinstance(case, dict)
                    and case.get("name") == "Contract_AtomicSourcesStayDistinct"):
                cases.append(case)
    if (len(cases) != 1 or cases[0].get("status") != "RUN"
            or cases[0].get("result") != "COMPLETED" or cases[0].get("failures")):
        raise RuntimeError("Native source contract requires one completed "
                           "GalleryQmlBridgeTest.Contract_AtomicSourcesStayDistinct case")
    return cases[0]


def read_native_source_contract(path: Path) -> dict:
    case = _read_native_source_case(path)
    fields = {f"{sample_id}-{kind}" for sample_id in QML_SAMPLE_IDS for kind in ("full", "key")}
    actual_fields = {field for field in case
                     if field.startswith("qml-view-") and field.endswith(("-full", "-key"))}
    if actual_fields != fields:
        raise RuntimeError("Native GTest source properties do not cover exactly the six QML samples")
    contract = {}
    for sample_id in QML_SAMPLE_IDS:
        sources = {kind: case[f"{sample_id}-{kind}"] for kind in ("full", "key")}
        if any(not isinstance(source, str) for source in sources.values()):
            raise RuntimeError(f"{sample_id}: native source properties must contain source text")
        contract[sample_id] = {kind: source_fingerprint(source) for kind, source in sources.items()}
    validate_source_contract(contract)
    return contract


def read_native_source_file_contract(path: Path, sources: dict) -> dict:
    case = _read_native_source_case(path)
    expected_fields = {f"{sample_id}-files" for sample_id in QML_SAMPLE_IDS}
    actual_fields = {field for field in case if field.startswith("qml-view-") and field.endswith("-files")}
    if actual_fields != expected_fields:
        raise RuntimeError("Native GTest file properties must cover exactly the six QML samples")
    contract = {}
    for sample_id in QML_SAMPLE_IDS:
        encoded = case[f"{sample_id}-files"]
        if not isinstance(encoded, str):
            raise RuntimeError(f"{sample_id}: native files must contain a JSON source file array")
        try:
            files = json.loads(encoded)
        except (ValueError, TypeError) as error:
            raise RuntimeError(f"{sample_id}: native source file JSON is invalid") from error
        if not isinstance(files, list) or any(
            not isinstance(file, dict) or set(file) != {"name", "language", "code", "usage_snippet"}
            or any(not isinstance(file[field], str) for field in file)
            for file in files
        ):
            raise RuntimeError(f"{sample_id}: native source files must contain complete source text")
        contract[sample_id] = [
            {"name": file["name"], "language": file["language"],
             "code": source_fingerprint(file["code"]), "usage": source_fingerprint(file["usage_snippet"])}
            for file in files
        ]
    validate_source_file_contract(contract, sources)
    return contract


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *_args: object) -> None:
        pass


def run_gallery_contract(browser: object, base_url: str, output: Path,
                         name: str, theme: str, width: int, height: int,
                         expected_source_contract: dict | None = None,
                         expected_source_file_contract: dict | None = None) -> dict:
    context = browser.new_context(viewport={"width": width, "height": height},
                                  device_scale_factor=2, color_scheme=theme)
    page = context.new_page()
    errors: list[str] = []
    console_errors: list[str] = []
    qt_platform_warnings: list[str] = []
    expected_qml_warnings: list[str] = []
    expecting_missing = False
    frames: dict[str, dict] = {}
    sequence = 0
    page.on("pageerror", lambda error: errors.append(str(error)))

    def record_console(message: object) -> None:
        if message.type != "error":
            return
        text = message.text
        # Qt WASM emits this once for a shared Qt context, even when both Qt
        # wrappers use one actual WebGL context. Qt warnings reach console.error.
        # https://github.com/qt/qtbase/blob/v6.9.3/src/plugins/platforms/wasm/qwasmopenglcontext.cpp
        sharing_warning = "The functionality for sharing OpenGL contexts is limited, see documentation"
        if "[fluentqt] [warn]" in text and text.endswith(sharing_warning):
            qt_platform_warnings.append(text)
        elif (expecting_missing and "[fluentqt] [warn]" in text
              and "qrc:/app/qml-bridge/Missing.qml" in text
              and ("No such file or directory" in text or "File not found" in text)):
            expected_qml_warnings.append(text)
        else:
            console_errors.append(text)

    page.on("console", record_console)

    def state() -> dict:
        return page.evaluate(
            "JSON.parse(document.documentElement.dataset.fluentQtQmlGallery || '{}')")

    def wait_for(condition: str, timeout: int = 20_000) -> dict:
        page.wait_for_function(f"""() => {{
            const s = JSON.parse(document.documentElement.dataset.fluentQtQmlGallery || '{{}}');
            if (s.state === 'fail') throw new Error(s.error);
            if (s.availabilityPending === false && s.spatialAvailable === false
                    && s.spatialUnavailableReason) {{
                throw new Error('Gallery requires hardware 3D for this probe: '
                    + s.spatialUnavailableReason);
            }}
            return {condition};
        }}""", timeout=timeout)
        return state()

    def command(action: str, target: str = "") -> dict:
        nonlocal sequence
        sequence += 1
        page.evaluate("""request => {
            document.documentElement.dataset.fluentQtQmlGalleryCommand = JSON.stringify(request);
        }""", {"action": action, "target": target, "sequence": sequence})
        return wait_for(f"s.commandSequence === {sequence}")

    def reveal(target: str) -> dict:
        command("reveal", target)
        texture = " && s.textureId > 0 && s.presenting" if target == "advance" else ""
        return wait_for(f"s.positions?.{target}?.visible === true{texture} && !s.transitioning")

    def canvas_point(target: str, snapshot: dict, *, allow_clipped: bool = False) -> tuple[float, float]:
        position = snapshot.get("positions", {}).get(target, {})
        point = position.get("point")
        if (not allow_clipped and not position.get("visible") or not position.get("enabled")
                or not isinstance(point, list) or len(point) != 2
                or any(not isinstance(value, (int, float)) or isinstance(value, bool)
                       or not math.isfinite(value) for value in point)):
            raise RuntimeError(f"{name}: invalid {target} canvas position: {snapshot}")
        stage = page.locator("#qt-container").bounding_box()
        if stage is None:
            raise RuntimeError(f"{name}: Gallery has no visible Qt canvas")
        x, y = stage["x"] + point[0], stage["y"] + point[1]
        if not (0 <= x < width and 0 <= y < height
                and stage["x"] <= x < stage["x"] + stage["width"]
                and stage["y"] <= y < stage["y"] + stage["height"]):
            raise RuntimeError(f"{name}: {target} canvas point {(x, y)} is outside "
                               f"the Qt canvas/browser viewport {width}x{height}: {snapshot}")
        return x, y

    def click(target: str, snapshot: dict) -> None:
        page.mouse.click(*canvas_point(target, snapshot))

    def identities(snapshot: dict) -> dict:
        return {key: (value.get("rootIdentity"), value.get("engineIdentity"))
                for key, value in snapshot.get("views", {}).items()}

    def expect_texture(snapshot: dict, *, require_texture: bool = True) -> None:
        size = snapshot.get("textureSize", [])
        stats = snapshot.get("textureStatistics", {})
        views = snapshot.get("views", {})
        if (snapshot.get("route") != "qml-view" or not snapshot.get("pageVisible")
                or not snapshot.get("initial3d") or not snapshot.get("spatialRequested")
                or not snapshot.get("spatialAvailable") or not snapshot.get("presenting")
                or snapshot.get("transitioning") or snapshot.get("directCompositionRequired")
                or snapshot.get("pageDirectCompositionRequired")
                or not snapshot.get("navigationEffect") or not snapshot.get("contentEffect")
                or snapshot.get("qmlErrors") != [] or snapshot.get("dpr") != 2
                or snapshot.get("themeDark") != (theme == "dark")
                or not snapshot.get("ready") or not snapshot.get("active")
                or not snapshot.get("rootPresent") or not snapshot.get("textureEnabled")
                or snapshot.get("viewDirectCompositionRequired")
                or snapshot.get("visibleQuickWidgets") != 0
                or set(views) != {"load", "theme", "context", "action", "lifecycle", "error"}
                or snapshot.get("quickWidgets") != sum(v["quickWidgets"] for v in views.values())):
            raise RuntimeError(f"{name}: atomic QML scenes did not join Spatial composition: {snapshot}")
        for key in ("load", "theme", "context", "action"):
            scene = views[key]
            if not (scene.get("ready") and scene.get("rootPresent")
                    and scene.get("textureEnabled") and scene.get("rootIdentity")
                    and scene.get("engineIdentity")):
                raise RuntimeError(f"{name}: independent {key} scene is not live: {snapshot}")
        if require_texture and (not snapshot.get("textureId") or not snapshot.get("textureRevision")
                                or len(size) != 2 or min(size) <= 0
                                or stats.get("renderedFrames", 0) <= 0
                                or stats.get("allocatedBytes", 0) <= 0):
            raise RuntimeError(f"{name}: visible Action scene has no rendered GPU texture: {snapshot}")

    def expect_flat(snapshot: dict, original: dict) -> None:
        if (snapshot.get("spatialRequested") or snapshot.get("presenting")
                or snapshot.get("transitioning") or snapshot.get("textureEnabled")
                or snapshot.get("visibleQuickWidgets") != 6
                or snapshot.get("navigationEffect") or snapshot.get("contentEffect")
                or not snapshot.get("ready") or identities(snapshot) != identities(original)):
            raise RuntimeError(f"{name}: 2D handoff replaced an atomic scene: {snapshot}")

    def expect_unchanged_scenes(snapshot: dict, original: dict, changed: str) -> None:
        for key, identity in identities(original).items():
            if key != changed and identities(snapshot).get(key) != identity:
                raise RuntimeError(f"{name}: {changed} changed the independent {key} scene: {snapshot}")

    def expect_pixels(snapshot: dict, label: str, *, clipped: bool = False) -> None:
        samples = [{"point": canvas_point("accentSample", snapshot, allow_clipped=clipped),
                    "rgb": snapshot["accentRgb"]}]
        if clipped:
            if snapshot["positions"]["accentSample"].get("visible"):
                raise RuntimeError(f"{name}: accent pixel must be clipped: {snapshot}")
        else:
            samples += [
                {"point": canvas_point("backgroundSample", snapshot),
                 "rgb": snapshot["backgroundRgb"]},
                {"point": canvas_point("progressSample", snapshot),
                 "rgb": snapshot["progressRgb"]},
            ]
        encoded = base64.b64encode(page.screenshot(
            path=str(output / f"gallery-{name}-{label}.png"))).decode("ascii")
        measured = page.evaluate("""async ({encoded, samples, viewportWidth}) => {
            const bytes = Uint8Array.from(atob(encoded), c => c.charCodeAt(0));
            const bitmap = await createImageBitmap(new Blob([bytes], {type: 'image/png'}));
            const canvas = new OffscreenCanvas(bitmap.width, bitmap.height);
            const context = canvas.getContext('2d');
            context.drawImage(bitmap, 0, 0);
            const scale = bitmap.width / viewportWidth;
            return samples.map(sample => {
                const x = Math.round(sample.point[0] * scale);
                const y = Math.round(sample.point[1] * scale);
                const rgba = Array.from(context.getImageData(x, y, 1, 1).data);
                return {rgba, expected: sample.rgb,
                    maximumDifference: Math.max(...sample.rgb.map((v, i) => Math.abs(v-rgba[i])))};
            });
        }""", {"encoded": encoded, "samples": samples, "viewportWidth": width})
        if clipped and measured[0]["maximumDifference"] <= 12:
            raise RuntimeError(f"{name}: clipped QML accent escaped the scroll viewport: {measured}")
        if not clipped and any(sample["maximumDifference"] > 12 for sample in measured):
            raise RuntimeError(f"{name}: projected QML pixels differ from the actual scene: {measured}")
        frames[label + "Pixels"] = {"samples": measured, "textureRevision": snapshot["textureRevision"]}

    def expect_values(snapshot: dict, title: str, progress: int) -> None:
        if (snapshot.get("widgetTitle") != title or snapshot.get("qmlTitle") != title
                or snapshot.get("widgetProgress") != progress
                or snapshot.get("qmlProgress") != progress):
            raise RuntimeError(f"{name}: independent Widgets/QML bindings differ: {snapshot}")

    def type_title(text: str) -> dict:
        click("title", reveal("title"))
        page.keyboard.press("ControlOrMeta+A")
        page.keyboard.type(text, delay=12)
        return wait_for(f"s.qmlTitle === {json.dumps(text)} && "
                        f"s.widgetTitle === {json.dumps(text)}")

    try:
        response = page.goto(
            f"{base_url}/app/index.html?route=button&wasm-smoke=qml-bridge"
            f"&host-theme={theme}&render-scale=native&window-mode=maximized",
            wait_until="domcontentloaded")
        if response is None or not response.ok:
            raise RuntimeError(f"{name}: Gallery entry did not load")
        initial = wait_for("s.initial3d && s.route === 'button' && s.pageVisible"
                           " && s.spatialAvailable && !s.availabilityPending"
                           " && s.spatialRequested && s.presenting && !s.transitioning"
                           " && !s.directCompositionRequired", timeout=60_000)
        frames["initial3d"] = initial
        command("open-qml")
        frames["qmlPageBeforeReveal"] = wait_for(
            "s.route === 'qml-view' && s.ready && s.presenting && s.textureEnabled"
            " && !s.transitioning && s.quickWidgets === 6"
            " && Object.values(s.views || {}).length === 6"
            " && Object.values(s.views).every(v => v.ready && v.rootPresent)")
        source_contract = frames["qmlPageBeforeReveal"].get("sourceContract")
        validate_source_contract(source_contract, expected_source_contract)
        source_file_contract = frames["qmlPageBeforeReveal"].get("sourceFileContract")
        validate_source_file_contract(source_file_contract, source_contract, expected_source_file_contract)
        python_file_contract = frames["qmlPageBeforeReveal"].get("pythonSourceFileContract")
        validate_python_source_files(python_file_contract)
        action_files = source_file_contract["qml-view-action"]
        validate_source_selection(frames["qmlPageBeforeReveal"].get("sourceSelection"),
                                  action_files[0], "key")
        page.wait_for_timeout(400)
        ready = reveal("advance")
        expect_texture(ready)
        frames["qmlPage"] = ready
        page.screenshot(path=str(output / f"gallery-{name}-initial.png"))

        shown = reveal("advance")
        # Exercise real scrolling as well as native ensureVisible. Qt geometry
        # is observation only; it does not invoke the QML action or button slots.
        if shown.get("scrollValue", 0) > 0:
            page.mouse.move(*canvas_point("scrollViewport", shown))
            page.mouse.wheel(0, -120)
            wait_for(f"s.scrollValue < {shown['scrollValue']}")
            frames["physicallyScrolled"] = state()
            shown = reveal("advance")
        expect_texture(shown)
        expect_pixels(shown, "projected-scene")
        progress = shown["widgetProgress"]
        original_title = shown["widgetTitle"]
        if progress > 90:
            raise RuntimeError(f"{name}: initial progress cannot demonstrate +10: {shown}")
        frames["previewVisible"] = shown
        page.wait_for_timeout(250)
        page.screenshot(path=str(output / f"gallery-{name}-scrolled.png"))
        click("advance", state())
        advanced = wait_for(f"s.widgetProgress === {progress + 10}"
                            f" && s.qmlProgress === {progress + 10}")
        page.wait_for_timeout(250)
        advanced = state()
        expect_texture(advanced)
        expect_values(advanced, original_title, progress + 10)
        frames["qmlPhysicalClick"] = advanced
        expect_pixels(advanced, "updated-progress")
        click("source", reveal("source"))
        frames["expandedSourceGeometry"] = wait_for(
            "s.sourceExpanded === true && s.geometryStable === true && !s.transitioning",
            timeout=3000)
        command("source-file", "Action.qml")
        qml_source = wait_for(
            "s.sourceSelection?.currentFileName === 'Action.qml' && s.sourceExpanded"
            " && s.geometryStable && !s.transitioning", timeout=3000)
        validate_source_selection(qml_source.get("sourceSelection"), action_files[1], "file")
        if identities(qml_source) != identities(advanced):
            raise RuntimeError(f"{name}: selecting QML source replaced a live scene: {qml_source}")
        frames["qmlSourceFile"] = qml_source
        command("source-file", "main.cpp")
        cpp_source = wait_for(
            "s.sourceSelection?.currentFileName === 'main.cpp' && s.sourceExpanded"
            " && s.geometryStable && !s.transitioning", timeout=3000)
        validate_source_selection(cpp_source.get("sourceSelection"), action_files[0], "key")
        if identities(cpp_source) != identities(advanced):
            raise RuntimeError(f"{name}: restoring C++ source replaced a live scene: {cpp_source}")
        frames["restoredCppSourceFile"] = cpp_source
        click("sourcePython", reveal("sourcePython"))
        python_source = wait_for(
            "s.sourceSelection?.currentFileName === 'main.py' && s.sourceContentFits"
            " && s.geometryStable && !s.transitioning", timeout=3000)
        python_files = python_file_contract["qml-view-action"]
        validate_source_selection(python_source.get("sourceSelection"), python_files[0], "key")
        command("source-detail", "full")
        full_python = wait_for(
            "s.sourceSelection?.sourceMode === 'full' && s.sourceContentFits"
            " && s.geometryStable && !s.transitioning", timeout=3000)
        validate_source_selection(full_python.get("sourceSelection"), python_files[0], "full")
        if identities(full_python) != identities(advanced):
            raise RuntimeError(f"{name}: switching host source replaced a QML scene")
        frames["fullPythonSourceFile"] = full_python
        command("source-file", "Action.qml")
        python_qml = wait_for(
            "s.sourceSelection?.currentFileName === 'Action.qml' && s.sourceContentFits"
            " && s.geometryStable && !s.transitioning", timeout=3000)
        validate_source_selection(python_qml.get("sourceSelection"), python_files[1], "file")
        frames["pythonQmlSourceFile"] = python_qml
        reveal("sourcePython")
        page.screenshot(path=str(output / f"gallery-{name}-python-qml-source.png"))
        click("sourceCpp", reveal("sourceCpp"))
        command("source-detail", "key")
        wait_for("s.sourceSelection?.currentFileName === 'main.cpp' && s.sourceContentFits"
                 " && s.geometryStable && !s.transitioning", timeout=3000)
        command("clip-advance")
        clipped = wait_for("s.positions?.advance?.visible === false && s.presenting"
                           " && s.positions?.accentSample?.visible === false"
                           " && s.textureId > 0 && !s.transitioning && s.geometryStable === true",
                           timeout=3000)
        page.mouse.click(*canvas_point("advance", clipped, allow_clipped=True))
        page.wait_for_timeout(150)
        clipped = state()
        expect_texture(clipped)
        expect_values(clipped, original_title, progress + 10)
        frames["clippedPhysicalClickIgnored"] = clipped
        expect_pixels(clipped, "clipped", clipped=True)
        click("source", reveal("source"))
        wait_for("s.sourceExpanded === false && s.geometryStable === true", timeout=3000)
        reveal("advance")
        # Mode handoff must retain the live root/engine; the core fixture also checks QML local state.
        for handoff in range(2):
            command("mode-2d")
            flat = wait_for("!s.spatialRequested && !s.presenting && !s.transitioning"
                            " && !s.textureEnabled && s.visibleQuickWidgets === 6")
            expect_flat(flat, advanced)
            expect_values(flat, original_title, progress + 10)
            frames[f"flatHandoff{handoff}"] = flat
            command("mode-3d")
            restored_scene = wait_for("s.spatialRequested && s.presenting && !s.transitioning"
                                      " && s.textureEnabled && s.textureId > 0"
                                      " && s.visibleQuickWidgets === 0")
            expect_texture(restored_scene)
            if identities(restored_scene) != identities(advanced):
                raise RuntimeError(f"{name}: 3D handoff recreated the QML scene: {restored_scene}")
            frames[f"textureHandoff{handoff}"] = restored_scene
        shown = reveal("advance")
        page.wait_for_timeout(400)
        expect_pixels(state(), "after-handoffs")
        static = state()
        page.mouse.move(width / 2, 16)
        page.wait_for_timeout(300)
        reused = state()
        if (reused.get("textureRevision") != static.get("textureRevision")
                or reused.get("textureStatistics", {}).get("renderedFrames")
                != static.get("textureStatistics", {}).get("renderedFrames")):
            raise RuntimeError(f"{name}: perspective motion re-rendered static QML: {static} -> {reused}")
        frames["staticTextureReused"] = reused

        title = "Browser shared title"
        type_title(title)
        wait_for(f"s.qmlTitle === {json.dumps(title)}")
        edited = reveal("advance")
        expect_texture(edited)
        expect_values(edited, title, progress + 10)
        frames["widgetsKeyboardEdit"] = edited

        before_lifecycle = state()
        click("deactivate", reveal("deactivate"))
        unloaded = wait_for("s.views?.lifecycle?.status === 0 && !s.views.lifecycle.active"
                            " && !s.views.lifecycle.rootPresent && s.quickWidgets === 5"
                            " && s.presenting && !s.transitioning")
        expect_texture(unloaded, require_texture=False)
        expect_values(unloaded, title, progress + 10)
        if (unloaded["views"]["lifecycle"].get("source")
                or unloaded["views"]["lifecycle"].get("textureId")):
            raise RuntimeError(f"{name}: lifecycle scene retained stopped resources: {unloaded}")
        expect_unchanged_scenes(unloaded, before_lifecycle, "lifecycle")
        frames["lifecycleStoppedIndependently"] = unloaded
        title = "Other cards stay editable"
        edited_while_stopped = type_title(title)
        expect_values(edited_while_stopped, title, progress + 10)
        if edited_while_stopped["views"]["lifecycle"]["rootPresent"]:
            raise RuntimeError(f"{name}: a Context edit restarted the Lifecycle scene")
        frames["independentContextEdit"] = edited_while_stopped

        click("reload", reveal("reload"))
        wait_for("s.views?.lifecycle?.ready && s.views.lifecycle.active"
                 " && s.quickWidgets === 6 && s.presenting && !s.transitioning")
        reloaded = reveal("advance")
        expect_texture(reloaded)
        expect_values(reloaded, title, progress + 10)
        expect_unchanged_scenes(reloaded, before_lifecycle, "lifecycle")
        frames["lifecycleReloadedIndependently"] = reloaded

        rapid_reload = reveal("reload")
        # Browser input reaches Qt asynchronously. A previously published Ready
        # snapshot does not prove that the queued clicks have all been handled.
        ready_count = rapid_reload["views"]["lifecycle"]["readyCount"]
        for _ in range(5):
            click("reload", rapid_reload)
        rapid_reloaded = wait_for(
            "s.views?.lifecycle?.ready && s.views.lifecycle.active"
            f" && s.views.lifecycle.readyCount === {ready_count + 5}"
            " && s.quickWidgets === 6 && s.presenting && !s.transitioning"
            " && s.geometryStable", timeout=5000)
        expect_unchanged_scenes(rapid_reloaded, before_lifecycle, "lifecycle")
        expect_values(rapid_reloaded, title, progress + 10)
        frames["rapidLifecycleReload"] = rapid_reloaded

        before_error = state()
        expecting_missing = True
        click("missing", reveal("missing"))
        failed_scene = wait_for("s.views?.error?.status === 3 && !s.views.error.rootPresent"
                                " && s.views.error.lastError && s.presenting && !s.transitioning")
        expect_texture(failed_scene, require_texture=False)
        expect_unchanged_scenes(failed_scene, before_error, "error")
        frames["loadErrorIsolated"] = failed_scene
        click("restore", reveal("restore"))
        recovered = wait_for("s.views?.error?.ready && !s.views.error.lastError"
                             " && s.quickWidgets === 6 && s.presenting && !s.transitioning")
        expecting_missing = False
        expect_unchanged_scenes(recovered, before_error, "error")
        frames["loadErrorRecovered"] = recovered
        reloaded = reveal("advance")
        expect_texture(reloaded)
        cached_identity = identities(reloaded)

        command("leave-qml")
        restored = wait_for("s.route === 'button' && s.pageVisible && s.spatialRequested"
                            " && s.presenting && !s.transitioning && !s.directCompositionRequired")
        if restored.get("cachedReadyViews") != 6 or restored.get("allQuickWidgets") != 6:
            raise RuntimeError(f"{name}: leaving lost the cached QML scene: {restored}")
        frames["restored3dWithCachedQml"] = restored
        command("return-qml")
        wait_for("s.route === 'qml-view' && s.ready && s.presenting && s.textureEnabled"
                 " && !s.transitioning && s.quickWidgets === 6")
        page.wait_for_timeout(400)
        final = reveal("advance")
        expect_texture(final)
        expect_values(final, title, progress + 10)
        if identities(final) != cached_identity:
            raise RuntimeError(f"{name}: cached navigation replaced the live QML scene: {final}")
        frames["returnedQmlPage"] = final
        expect_pixels(final, "returned-projection")
        page.wait_for_timeout(400)
        page.screenshot(path=str(output / f"gallery-{name}.png"))
        if errors or console_errors:
            raise RuntimeError(f"{name}: browser errors: {errors}; console: {console_errors}")
        command("finish")
        wait_for("s.state === 'pass'")
        result = {"kind": "gallery", "variant": name, "passed": True,
                  "browser": browser.version, "deviceScaleFactor": 2,
                  "viewport": {"width": width, "height": height}, "frames": frames,
                  "sourceContract": source_contract,
                  "sourceFileContract": source_file_contract,
                  "nativeSourceContractMatched": expected_source_contract is not None,
                  "nativeSourceFileContractMatched": expected_source_file_contract is not None,
                  "pageErrors": errors, "consoleErrors": console_errors,
                  "qtPlatformWarnings": qt_platform_warnings, "expectedQmlWarnings": expected_qml_warnings}
        (output / f"gallery-{name}.json").write_text(
            json.dumps(result, indent=2) + "\n", encoding="utf-8")
        return result
    except Exception as error:
        (output / f"gallery-{name}-failure.json").write_text(json.dumps({
            "kind": "gallery", "variant": name, "passed": False, "error": str(error),
            "state": state(), "frames": frames, "pageErrors": errors,
            "expectedSourceContract": expected_source_contract,
            "expectedSourceFileContract": expected_source_file_contract,
            "consoleErrors": console_errors, "qtPlatformWarnings": qt_platform_warnings, "expectedQmlWarnings": expected_qml_warnings,
        }, indent=2) + "\n", encoding="utf-8")
        page.screenshot(path=str(output / f"gallery-{name}-failure.png"))
        raise
    finally:
        context.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("build/wasm"))
    parser.add_argument("--output", type=Path, default=Path("build/qmlbridge-validation/wasm"))
    parser.add_argument("--headed", action="store_true",
                        help="Show the browser window; use for the hardware 3D Gallery gate.")
    parser.add_argument("--channel", default=None,
                        help="For example chrome; default is bundled Chromium.")
    parser.add_argument("--scope", choices=("all", "gallery", "example"), default="all",
                        help="Default checks both Gallery and the shared standalone example.")
    parser.add_argument("--source-contract", type=Path,
                        help="Compare Gallery C++ Key/Full and all source files with the GTest JSON report from "
                             "GalleryQmlBridgeTest.Contract_AtomicSourcesStayDistinct.")
    parser.add_argument("--viewport-width", type=int, default=1280,
                        help="Gallery Light/Dark viewport width; Gallery narrow is 620 pixels.")
    parser.add_argument("--viewport-height", type=int, default=900,
                        help="Gallery Light/Dark viewport height; narrow remains 860 pixels.")
    args = parser.parse_args()
    if args.source_contract is not None and args.scope == "example":
        parser.error("--source-contract requires --scope gallery or all")
    expected_source_contract = (read_native_source_contract(args.source_contract.resolve())
                                if args.source_contract is not None else None)
    expected_source_file_contract = (
        read_native_source_file_contract(args.source_contract.resolve(), expected_source_contract)
        if args.source_contract is not None else None)
    root = args.root.resolve()
    entry = "app/qml-bridge/index.html"
    if not (root / entry).is_file():
        entry = "examples/qml_bridge/index.html"
    if args.scope != "gallery":
        for filename in ("index.html", "fluentqt_qml_bridge.js", "fluentqt_qml_bridge.wasm"):
            if not (root / Path(entry).parent / filename).is_file():
                raise RuntimeError(f"Missing browser artifact: {filename}")
    if args.scope != "example" and not (root / "app/index.html").is_file():
        raise RuntimeError("Missing Gallery browser artifact: app/index.html")
    if args.viewport_width <= 0 or args.viewport_height <= 0:
        parser.error("Viewport dimensions must be positive")
    args.output.mkdir(parents=True, exist_ok=True)
    server = ThreadingHTTPServer(("127.0.0.1", 0), partial(QuietHandler, directory=str(root)))
    Thread(target=server.serve_forever, daemon=True).start()
    results = []
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(headless=not args.headed, channel=args.channel)
            variants = (
                ("light", "light", 1100, 850),
                ("dark", "dark", 1100, 850),
                ("narrow", "light", 420, 860),
            ) if args.scope != "gallery" else ()
            for name, theme, width, height in variants:
                page = browser.new_page(viewport={"width": width, "height": height},
                                        device_scale_factor=2)
                errors: list[str] = []
                page.on("pageerror", lambda error: errors.append(str(error)))
                page.goto(f"http://127.0.0.1:{server.server_port}/{entry}?smoke-test&theme={theme}")
                page.wait_for_function("""() => {
                    const state = JSON.parse(document.documentElement.dataset.fluentQtQmlBridgeState || '{}');
                    return state.smokePassed !== undefined;
                }""", timeout=60_000)
                state = json.loads(page.locator("html").get_attribute("data-fluent-qt-qml-bridge-state"))
                if (not state.get("smokePassed") or not state.get("ready")
                        or state.get("error") or state.get("progress") != 35
                        or state.get("widgetProgress") != 35
                        or state.get("title") != "Updated from QML"
                        or state.get("widgetTitle") != "Updated from QML" or errors):
                    raise RuntimeError(f"{name}: {state}, browser errors: {errors}")
                # Let the final bounded Fluent transition settle before capture.
                page.wait_for_timeout(400)
                page.screenshot(path=str(args.output / f"{name}.png"))
                results.append({"kind": "example", "variant": name, "state": state,
                                "pageErrors": errors})
                page.close()
            if args.scope != "example":
                for name, theme, width, height in (
                    ("light", "light", args.viewport_width, args.viewport_height),
                    ("dark", "dark", args.viewport_width, args.viewport_height),
                    ("narrow", "light", 620, 860),
                ):
                    results.append(run_gallery_contract(
                        browser, f"http://127.0.0.1:{server.server_port}", args.output,
                        name, theme, width, height, expected_source_contract,
                        expected_source_file_contract))
            browser.close()
    finally:
        server.shutdown()
        server.server_close()
    (args.output / "result.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    if expected_source_contract is not None:
        print("Gallery C++ source parity passed: six distinct Key/Full pairs and four-file examples match the native "
              f"GTest report byte for byte ({args.source_contract.resolve()})")
    if args.scope == "example":
        print("QML bridge example lifecycle probe passed: shared model, theme, unload/reload, "
              "Light/Dark/narrow at DPR 2; Gallery composition was not checked")
    elif args.scope == "gallery":
        print("QML bridge Gallery probe passed: physical canvas input and hardware "
              "3D QML texture, retained-scene 2D/3D handoffs, Light/Dark/narrow at DPR 2")
    else:
        print("QML bridge example lifecycle and Gallery probes passed: shared model, theme, "
              "unload/reload, physical canvas input and hardware 3D QML texture, retained-scene 2D/3D handoffs, "
              "Light/Dark/narrow at DPR 2")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
