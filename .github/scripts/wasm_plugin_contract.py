"""Exercise the compiled plugin consumer through real browser input."""

from __future__ import annotations

import json
import math
from pathlib import Path


def validate_plugin_presentation(initial: dict) -> None:
    presentation = initial.get("presentation", {})
    activation = presentation.get("activation", {})
    creation = presentation.get("creation", {})
    content = presentation.get("content", [])
    if (presentation.get("window") != [600, 500]
            or presentation.get("margins") != [24, 24, 24, 24]
            or presentation.get("spacing") != 12
            or activation.get("text") != "Activate extension"
            or creation.get("text") != "Create greeting"
            or activation.get("enabled") is not True
            or creation.get("enabled") is not False
            or presentation.get("status", {}).get("text") !=
            "Extension inactive; created greetings stay available"
            or activation.get("y") != creation.get("y")
            or activation.get("height") != creation.get("height")
            or activation.get("x") != 24 or len(content) != 2
            or abs(activation.get("width", 0) - creation.get("width", 0)) > 1
            or abs(activation.get("width", 0) + creation.get("width", 0) + 12 -
                   (content[0] - 48)) > 1):
        raise RuntimeError(f"Plugin example initial layout differs: {presentation}")


def validate_plugin_greeting_spacing(state: dict, count: int) -> list[float]:
    greetings = state.get("greetings", [])
    if len(greetings) != count:
        raise RuntimeError(f"Expected {count} visible greetings: {greetings}")
    for greeting in greetings:
        for key in ("x", "y", "width", "height"):
            value = greeting.get(key)
            if (isinstance(value, bool) or not isinstance(value, (int, float))
                    or not math.isfinite(value)):
                raise RuntimeError(f"Greeting has no visible geometry: {greeting}")
        if greeting["width"] <= 0 or greeting["height"] <= 0:
            raise RuntimeError(f"Greeting has no visible geometry: {greeting}")
    gaps = [second["y"] - first["y"] - first["height"]
            for first, second in zip(greetings, greetings[1:])]
    if any(gap != 12 for gap in gaps):
        raise RuntimeError(f"Greeting gaps must be 12 logical pixels; observed {gaps}: {greetings}")
    return gaps


def run_plugin_extension_contract(
    browser: object, base_url: str, output: Path | None = None,
    entry_path: str = "/app/plugin-extension/index.html",
) -> dict:
    context = browser.new_context(
        viewport={"width": 900, "height": 700}, device_scale_factor=2
    )
    page = context.new_page()
    errors: list[str] = []
    page.on("pageerror", lambda error: errors.append(str(error)))
    phases: list[dict] = []

    def snapshot() -> dict:
        return page.evaluate(
            "JSON.parse(document.documentElement.dataset.fluentQtPluginState)"
        )

    def expect(phase: str, **values: object) -> dict:
        page.wait_for_function(
            """expected => {
                const state = JSON.parse(
                    document.documentElement.dataset.fluentQtPluginState || '{}');
                return Object.entries(expected).every(([key, value]) => state[key] === value);
            }""",
            arg=values,
            timeout=10_000,
        )
        state = snapshot()
        if state["alive"] >= 2:
            page.wait_for_function("""count => {
                const state = JSON.parse(document.documentElement.dataset.fluentQtPluginState);
                return state.greetings.length === count && state.greetings.every(
                    greeting => greeting.width > 0 && greeting.height > 0);
            }""", arg=state["alive"], timeout=10_000, polling=100)
            state = snapshot()
            state["greetingGaps"] = validate_plugin_greeting_spacing(state, state["alive"])
        phases.append({"phase": phase, **state})
        return state

    def retained(before: dict, after: dict) -> None:
        records = lambda state: [(greeting["name"], greeting["text"])
                                 for greeting in state["greetings"]]
        if records(before) != records(after):
            raise RuntimeError("The plugin lifecycle replaced or changed a delivered widget")

    def click(name: str, greeting: int | None = None) -> None:
        if greeting is None:
            predicate = """name => {
                const state = JSON.parse(document.documentElement.dataset.fluentQtPluginState);
                const rect = state[name];
                return rect && rect.width > 0 && rect.height > 0;
            }"""
            page.wait_for_function(predicate, arg=name, polling=100)
        else:
            predicate = """index => {
                const state = JSON.parse(document.documentElement.dataset.fluentQtPluginState);
                const rect = state.greetings[index];
                return rect && rect.width > 0 && rect.height > 0;
            }"""
            page.wait_for_function(predicate, arg=greeting, polling=100)
        state = snapshot()
        rect = state[name] if greeting is None else state["greetings"][greeting]
        stage = page.locator("#qt-container").bounding_box()
        page.mouse.click(
            stage["x"] + rect["x"] + rect["width"] / 2,
            stage["y"] + rect["y"] + rect["height"] / 2,
        )

    try:
        url = base_url + "/" + entry_path.lstrip("/")
        response = page.goto(url, wait_until="domcontentloaded")
        if response is None or not response.ok:
            raise RuntimeError(f"Plugin example request failed: {url}")
        page.wait_for_function(
            """() => document.documentElement.dataset.fluentQtLoaded === 'true' &&
                document.documentElement.dataset.fluentQtPluginState &&
                typeof window.fluentQtPluginRuntime?._fluentQtPluginRunContract === 'function'""",
            timeout=60_000,
        )
        initial = expect("registered", registered=True, active=False, created=0, alive=0, activations=0)
        validate_plugin_presentation(initial)
        if output is not None:
            output.mkdir(parents=True, exist_ok=True)
            page.screenshot(path=str(output / "plugin-initial.png"), full_page=True)
        result = page.evaluate("window.fluentQtPluginRuntime._fluentQtPluginRunContract()")
        contract = page.evaluate(
            "JSON.parse(document.documentElement.dataset.fluentQtPluginContract)"
        )
        checks = (
            "missingBeforeHooks", "cycleBeforeHooks", "dependencyOrder", "reverseCleanup",
            "rollbackKeepsExisting", "activationExceptionCaught", "factoryExceptionCaught",
        )
        if result != 1 or any(contract.get(check) is not True for check in checks):
            raise RuntimeError(f"Native WebAssembly plugin contract failed: {contract}")
        click("activateButton")
        expect("activated", active=True, activations=1, deactivations=0)
        click("createButton")
        first = expect("created", active=True, created=1, alive=1)
        if output is not None:
            page.screenshot(path=str(output / "plugin-created.png"), full_page=True)
        click("activateButton")
        stopped = expect("deactivated", active=False, deactivations=1, created=1, alive=1)
        if output is not None:
            page.screenshot(path=str(output / "plugin-deactivated.png"), full_page=True)
        if stopped["greetings"][0]["name"] != first["greetings"][0]["name"]:
            raise RuntimeError("Deactivation replaced the caller-owned widget")
        click("greetings", greeting=0)
        clicked = expect("retained-widget-clicked", active=False, greetingClicks=1, alive=1)
        if clicked["greetings"][0]["text"] != "Hello, you!":
            raise RuntimeError("The retained Fluent button stopped accepting browser input")
        rejected = page.evaluate("window.fluentQtPluginRuntime._fluentQtPluginCreateGreeting()")
        rejection = expect("inactive-factory-rejected", active=False, rejectedFactories=1, alive=1)
        if rejected != 0 or not rejection["error"]:
            raise RuntimeError("The native manager accepted an inactive widget factory")
        click("activateButton")
        resumed = expect("reactivated", active=True, activations=2, created=1, alive=1)
        if resumed["greetings"][0]["text"] != "Hello, you!" or resumed["error"]:
            raise RuntimeError("Reactivation did not preserve the delivered widget's state")
        click("createButton")
        second = expect("second-widget-created", active=True, created=2, alive=2)
        if second["greetings"][0]["name"] == second["greetings"][1]["name"]:
            raise RuntimeError("The plugin factory returned the same widget twice")
        if output is not None:
            page.screenshot(path=str(output / "plugin-two.png"), full_page=True)
        click("activateButton")
        stopped_two = expect("stopped-with-two", active=False, activations=2, deactivations=2,
                             created=2, alive=2)
        retained(second, stopped_two)
        click("activateButton")
        resumed_two = expect("reactivated-with-two", active=True, activations=3, deactivations=2,
                             created=2, alive=2)
        retained(second, resumed_two)
        for count, phase in ((3, "third-widget-created"), (4, "fourth-widget-created")):
            click("createButton")
            four = expect(phase, active=True, created=count, alive=count)
        if len({greeting["name"] for greeting in four["greetings"]}) != 4:
            raise RuntimeError("The plugin factory reused a delivered widget")
        if output is not None:
            page.screenshot(path=str(output / "plugin-four.png"), full_page=True)
        click("activateButton")
        stopped_four = expect("stopped-with-four", active=False, activations=3, deactivations=3,
                              created=4, alive=4)
        retained(four, stopped_four)
        click("activateButton")
        resumed_four = expect("reactivated-with-four", active=True, activations=4, deactivations=3,
                              created=4, alive=4)
        retained(four, resumed_four)
        click("activateButton")
        final = expect("stopped-again", active=False, activations=4, deactivations=4,
                       created=4, alive=4)
        retained(four, final)
        if errors:
            raise RuntimeError("Plugin example browser errors: " + " | ".join(errors))
        report = {"nativeContract": contract, "phases": phases, "browser": browser.version,
                  "deviceScaleFactor": 2}
        if output is not None:
            output.mkdir(parents=True, exist_ok=True)
            (output / "plugin-lifecycle.json").write_text(json.dumps(report, indent=2) + "\n")
            page.screenshot(path=str(output / "plugin-extension.png"), full_page=True)
            for name in ("plugin-failure.json", "plugin-failure.png"):
                (output / name).unlink(missing_ok=True)
        print("Plugin WebAssembly contract passed: dependency graph, rollback, exceptions, "
              "physical button input, inactive factory rejection, retained widgets, reactivation, "
              "and 12px gaps for two and four greetings")
        return report
    except Exception:
        if output is not None:
            output.mkdir(parents=True, exist_ok=True)
            page.screenshot(path=str(output / "plugin-failure.png"), full_page=True)
            diagnostics = {"phases": phases, "errors": errors, "state": page.evaluate(
                "document.documentElement.dataset.fluentQtPluginState || ''")}
            (output / "plugin-failure.json").write_text(json.dumps(diagnostics, indent=2) + "\n")
        raise
    finally:
        context.close()
