"""Render captured artifacts and checks for independent review."""

from __future__ import annotations

from html import escape
from pathlib import Path
from typing import Mapping


def html_uri(path: object) -> str:
    if not isinstance(path, str) or not path:
        return ""
    candidate = Path(path)
    return candidate.resolve().as_uri() if candidate.exists() else ""


def review_html(evidence: Mapping[str, object]) -> str:
    cards: list[str] = []
    scenarios = evidence.get("scenarios") if isinstance(evidence.get("scenarios"), list) else []
    for raw in scenarios:
        if not isinstance(raw, dict):
            continue
        artifacts = raw.get("artifacts") if isinstance(raw.get("artifacts"), dict) else {}
        images: list[str] = []
        for label, key in (("Actual", "actual"), ("Approved baseline", "baseline")):
            uri = html_uri(artifacts.get(key))
            if uri:
                images.append(
                    f'<figure><figcaption>{escape(label)}</figcaption><img src="{escape(uri)}" alt="{escape(label)}"></figure>'
                )
        diffs = artifacts.get("diffs") if isinstance(artifacts.get("diffs"), list) else []
        for index, diff in enumerate(diffs):
            uri = html_uri(diff)
            if uri:
                images.append(
                    f'<figure><figcaption>Diff {index + 1}</figcaption><img src="{escape(uri)}" alt="Diff"></figure>'
                )
        failed_checks = [
            item
            for item in raw.get("checks", [])
            if isinstance(item, dict) and item.get("status") not in {"pass", "not-applicable"}
        ]
        check_items = "".join(
            f'<li><code>{escape(str(item.get("id")))}</code> — {escape(str(item.get("status")))}: {escape(str(item.get("message")))}</li>'
            for item in failed_checks
        ) or "<li>All deterministic checks passed.</li>"
        prompts = "".join(f"<li>{escape(str(prompt))}</li>" for prompt in raw.get("review", []))
        cards.append(
            f'''<section>
<h2>{escape(str(raw.get("id")))} <span class="status {escape(str(raw.get("status")))}">{escape(str(raw.get("status")))}</span></h2>
<div class="images">{"".join(images)}</div>
<h3>Deterministic gates</h3><ul>{check_items}</ul>
<h3>Independent review prompts</h3><ul>{prompts}</ul>
</section>'''
        )
    return f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>FluentQt GUI verification</title>
<style>
body{{font:14px system-ui;margin:24px;background:#111318;color:#f4f6fa}}h1{{font-size:22px}}section{{margin:20px 0;padding:16px;border:1px solid #3a3f49;border-radius:10px;background:#191c22}}h2{{margin-top:0}}h3{{font-size:14px}}.images{{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:12px}}figure{{margin:0;background:#fff;border-radius:6px;overflow:hidden}}figcaption{{padding:8px;background:#2a2f38;color:#fff}}img{{display:block;width:100%;height:auto;image-rendering:auto}}.status{{font-size:12px;padding:3px 7px;border-radius:999px;background:#404754}}.pass{{background:#176b42}}.fail{{background:#a73333}}.human-required,.review-required,.incomplete{{background:#8a641d}}code{{color:#9fd2ff}}li{{margin:5px 0}}
</style></head><body><h1>FluentQt GUI verification</h1><p>Deterministic status: <strong>{escape(str(evidence.get("deterministic_status")))}</strong>. Final visual acceptance requires a separate reviewer whose identity differs from the author.</p>{"".join(cards)}</body></html>'''
