"""Check Qt accessibility proxy lifetime while the real Drawer is recreated."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Callable


DRAWER_PROXY_SELECTOR = '[id*="Gallery.Drawer.Effects"]'
DRAWER_PROXY_TYPES = {
    "Modal": ("INPUT", "checkbox"),
    "DimEnabled": ("INPUT", "checkbox"),
    "CloseOutside": ("INPUT", "checkbox"),
    "Blur": ("INPUT", "range"),
    "Dim": ("INPUT", "range"),
    # Qt creates an ordinary HTML button, whose reflected default type is submit.
    "Close": ("BUTTON", "submit"),
}
DRAWER_LIFECYCLE_CYCLES = 5

DRAWER_SNAPSHOT_SCRIPT = """selector => {
    const root = document.querySelector('#qt-shadow-container').shadowRoot;
    return [...root.querySelectorAll(selector)].map(node => ({
        id: node.id, tag: node.tagName, type: node.type || null,
        hidden: node.getAttribute('aria-hidden')
    }));
}"""


def validate_drawer_proxy_snapshot(nodes: list[dict], *, opened: bool) -> None:
    """Reject stale hidden nodes, duplicates, and reused proxy types."""
    if not isinstance(nodes, list) or any(not isinstance(node, dict) for node in nodes):
        raise RuntimeError("Drawer proxy snapshot is not a node list")
    if not opened:
        if nodes:
            raise RuntimeError(f"Closed Drawer retains {len(nodes)} accessibility proxies")
        return
    for suffix, expected in DRAWER_PROXY_TYPES.items():
        matches = [node for node in nodes
                   if str(node.get("id", "")).endswith(f".Gallery.Drawer.{suffix}")]
        if len(matches) != 1:
            raise RuntimeError(f"Drawer {suffix} requires exactly one proxy, observed {len(matches)}")
        node = matches[0]
        observed = node.get("tag"), node.get("type")
        if observed != expected or node.get("hidden") != "false":
            raise RuntimeError(
                f"Drawer {suffix} proxy must be visible {expected}, observed "
                f"{observed} aria-hidden={node.get('hidden')!r}"
            )


def run_drawer_lifecycle_contract(
    page: object, opener: object, click: Callable[[object], None], artifact_dir: Path
) -> dict:
    from playwright.sync_api import expect

    artifact_dir.mkdir(parents=True, exist_ok=True)
    snapshots = []
    result = {"status": "running", "cycles": DRAWER_LIFECYCLE_CYCLES,
              "completed_cycles": 0, "input": "browser mouse and keyboard"}
    proxies = page.locator(DRAWER_PROXY_SELECTOR)

    def snapshot(cycle: int, opened: bool) -> None:
        nodes = page.evaluate(DRAWER_SNAPSHOT_SCRIPT, DRAWER_PROXY_SELECTOR)
        snapshots.append({"cycle": cycle, "opened": opened, "nodes": nodes})
        validate_drawer_proxy_snapshot(nodes, opened=opened)

    try:
        # The preceding close must complete, including QObject deletion, before
        # another instance is created. Include hidden and construction-time nodes.
        expect(proxies).to_have_count(0, timeout=30_000)
        snapshot(0, False)
        for cycle in range(1, DRAWER_LIFECYCLE_CYCLES + 1):
            click(opener)
            for suffix in DRAWER_PROXY_TYPES:
                target = page.locator(
                    f'[id$=".Gallery.Drawer.{suffix}"][aria-hidden="false"]'
                )
                # Playwright strict resolution fails if stale visible proxies
                # share an identity; never select the first duplicate.
                expect(target).to_be_visible(timeout=30_000)
            snapshot(cycle, True)
            page.keyboard.press("Escape")
            # Observe real closure/deferred destruction, not an arbitrary delay.
            expect(proxies).to_have_count(0, timeout=30_000)
            snapshot(cycle, False)
            result["completed_cycles"] = cycle
        result["status"] = "pass"
        return result
    except Exception as error:
        result.update(status="fail", error=str(error))
        raise
    finally:
        (artifact_dir / "result.json").write_text(
            json.dumps(result, indent=2) + "\n", encoding="utf-8"
        )
        (artifact_dir / "snapshots.json").write_text(
            json.dumps(snapshots, indent=2) + "\n", encoding="utf-8"
        )
