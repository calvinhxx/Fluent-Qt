#!/usr/bin/env python3
"""Validate atomic QMLPlus behavior and C++/Python source in a real browser."""

import argparse
from functools import partial
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import runpy
from threading import Thread

from playwright.sync_api import sync_playwright


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


def fingerprint(source):
    data = source.encode("utf-8")
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def run_variant(browser, base_url, output, name, theme, width, cpp, python):
    context = browser.new_context(viewport={"width": width, "height": 900},
                                  device_scale_factor=2, color_scheme=theme)
    page = context.new_page()
    errors = []
    page.on("pageerror", lambda error: errors.append(str(error)))
    sequence = 0

    def state():
        return page.evaluate("JSON.parse(document.documentElement.dataset.fluentQtQmlPlus || '{}')")

    def wait(condition):
        page.wait_for_function("""condition => {
            const s = JSON.parse(document.documentElement.dataset.fluentQtQmlPlus || '{}');
            if (s.state === 'fail') throw new Error(s.error);
            return new Function('s', 'return ' + condition)(s);
        }""", arg=condition, timeout=60000)
        return state()

    def command(action, target="", **kwargs):
        nonlocal sequence
        sequence += 1
        page.evaluate("request => document.documentElement.dataset.fluentQtQmlPlusCommand = JSON.stringify(request)",
                      {"action": action, "target": target, "sequence": sequence, **kwargs})
        return wait(f"s.sequence === {sequence}")

    def value(target, property_name, new_value):
        command("value", target, property=property_name, value=new_value)

    def expect(target, property_name, expected, *, epsilon=None):
        if epsilon is not None:
            return wait(f"Math.abs(s.values?.{target}?.{property_name} - {expected}) < {epsilon}")
        return wait(f"s.values?.{target}?.{property_name} === {json.dumps(expected)}")

    try:
        page.goto(base_url + "/app/index.html?route=foundation-qmlplus&wasm-smoke=qmlplus"
                  f"&host-theme={theme}&window-mode=maximized&render-scale=native",
                  wait_until="domcontentloaded")
        page.bring_to_front()
        initial = wait("s.ready && Object.keys(s.sources).length === 12 && s.geometryStable && !s.transitioning")
        assert set(initial["sources"]) == set(cpp) == set(python)
        for sample_id, sources in initial["sources"].items():
            assert sources["cpp"] == fingerprint(cpp[sample_id]), sample_id
            assert sources["python"] == fingerprint(python[sample_id]), sample_id
        command("show-source", "qmlplus-binding-one-way")
        wait("s.positions?.Python && s.positions?.Cpp && s.geometryStable && !s.transitioning")
        stage = page.locator("#qt-container").bounding_box()
        for language in ("Python", "Cpp", "Python"):
            snapshot = state()
            x, y = snapshot["positions"][language]
            if stage is None or not (0 <= x < stage["width"] and 0 <= y < stage["height"]):
                raise RuntimeError("Source language button is outside the visible canvas")
            page.mouse.click(stage["x"] + x, stage["y"] + y)
            page.wait_for_timeout(150)
            if state().get("language") != language.lower():
                snapshot = state()
                x, y = snapshot["positions"][language]
                page.mouse.click(stage["x"] + x, stage["y"] + y)
            selected = wait("s.language === '" + language.lower() + "'")
            source = python if language == "Python" else cpp
            assert selected["code"] == fingerprint(source["qmlplus-binding-one-way"])
        page.screenshot(path=str(output / f"qmlplus-{name}-source.png"))
        value("qmlplusOneWaySource", "value", 63)
        expect("qmlplusOneWayTarget", "value", 63)
        value("qmlplusTwoWayFirst", "isOn", True)
        expect("qmlplusTwoWaySecond", "isOn", True)
        value("qmlplusTwoWaySecond", "isOn", False)
        expect("qmlplusTwoWayFirst", "isOn", False)
        value("qmlplusComputedFirst", "value", 25)
        value("qmlplusComputedSecond", "value", 70)
        expect("qmlplusComputedResult", "text", "25 + 70 = 95")
        value("qmlplusPauseToggle", "isOn", True)
        value("qmlplusPauseSource", "value", 72)
        expect("qmlplusPauseTarget", "value", 40)
        value("qmlplusPauseToggle", "isOn", False)
        expect("qmlplusPauseTarget", "value", 72)
        for kind in ("Manual", "Conditional"):
            value(f"qmlplus{kind}Toggle", "isOn", True)
            expect(f"qmlplus{kind}Result", "text", "Active")
            value(f"qmlplus{kind}Toggle", "isOn", False)
            expect(f"qmlplus{kind}Result", "text", "Default")
        value("qmlplusInheritedToggle", "isOn", True)
        expect("qmlplusInheritedTarget", "enabled", False)
        expect("qmlplusInheritedTarget", "text", "Saving...")
        value("qmlplusInheritedToggle", "isOn", False)
        expect("qmlplusInheritedTarget", "enabled", True)
        value("qmlplusRestoreToggle", "isOn", True)
        value("qmlplusRestoreSource", "value", 72)
        expect("qmlplusRestoreTarget", "value", 100)
        value("qmlplusRestoreToggle", "isOn", False)
        expect("qmlplusRestoreTarget", "value", 72)
        value("qmlplusBehaviorSource", "value", 58)
        # ProgressBar suppresses writes below 0.001, matching the native Gallery test.
        expect("qmlplusBehaviorTarget", "value", 58, epsilon=0.001)
        value("qmlplusTransitionToggle", "isOn", True)
        expect("qmlplusTransitionTarget", "value", 80, epsilon=0.001)
        value("qmlplusTransitionToggle", "isOn", False)
        expect("qmlplusTransitionTarget", "value", 20, epsilon=0.001)
        value("qmlplusStateAnchorToggle", "isOn", True)
        wait("s.rectangles.qmlplusStateAnchorTarget[0] + s.rectangles.qmlplusStateAnchorTarget[2]"
             " === s.rectangles.qmlplusStateAnchorSurface[2] - 12")
        value("qmlplusStateAnchorToggle", "isOn", False)
        final = wait("s.rectangles.qmlplusStateAnchorTarget[0] === 12")
        centered = final["rectangles"]["qmlplusAnchorTarget"]
        surface = final["rectangles"]["qmlplusAnchorSurface"]
        assert abs(centered[0] + centered[2] / 2 - surface[2] / 2) <= 1
        assert abs(centered[1] + centered[3] / 2 - surface[3] / 2) <= 1
        assert not errors, errors
        command("finish")
        result = {"variant": name, "passed": True, "state": wait("s.state === 'pass'"),
                  "pageErrors": errors}
        (output / f"qmlplus-{name}.json").write_text(json.dumps(result, indent=2) + "\n")
        return result
    except Exception as error:
        (output / f"qmlplus-{name}-failure.json").write_text(json.dumps(
            {"error": str(error), "state": state(), "pageErrors": errors}, indent=2) + "\n")
        page.screenshot(path=str(output / f"qmlplus-{name}-failure.png"))
        raise
    finally:
        context.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("build/wasm"))
    parser.add_argument("--output", type=Path, default=Path("build/qmlplus-validation/wasm"))
    parser.add_argument("--headed", action="store_true")
    parser.add_argument("--channel", default=None)
    parser.add_argument("--variant", choices=("all", "light", "dark", "narrow"), default="all")
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[2]
    generator = runpy.run_path(str(project / "bindings/pyside6/gallery/tools/generate_gallery_contract.py"))
    contract = generator["generate_contract"](project)
    cpp = {s["id"]: s["cpp_snippet"] for s in contract["foundation_samples"]["foundation-qmlplus"]}
    catalog = json.loads((project / "app/data/gallery_python_snippets.json").read_text())
    python = {s["sample_id"]: s["source"] for s in catalog["samples"]
              if s["route_id"] == "foundation-qmlplus"}
    args.output.mkdir(parents=True, exist_ok=True)
    server = ThreadingHTTPServer(("127.0.0.1", 0), partial(QuietHandler, directory=str(args.root.resolve())))
    Thread(target=server.serve_forever, daemon=True).start()
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(headless=not args.headed, channel=args.channel)
            results = [run_variant(browser, f"http://127.0.0.1:{server.server_port}", args.output,
                                   name, theme, width, cpp, python)
                       for name, theme, width in (("light", "light", 1280),
                                                  ("dark", "dark", 1280),
                                                  ("narrow", "light", 620))
                       if args.variant in ("all", name)]
            browser.close()
        (args.output / "result.json").write_text(json.dumps(results, indent=2) + "\n")
    finally:
        server.shutdown()
        server.server_close()
    print("QMLPlus Gallery passed: twelve examples, C++/Python source, Light/Dark/narrow at DPR 2")


if __name__ == "__main__":
    main()
