#!/usr/bin/env python3
"""Render visual-evidence JSON into a local independent-review board."""

from __future__ import annotations

import argparse
import base64
from collections import defaultdict
from html import escape
import json
import mimetypes
from pathlib import Path
import sys
from typing import Any
from urllib.parse import unquote, urlparse
from urllib.request import url2pathname


SCORE_LABELS = {
    "workflow_fit": "Workflow fit",
    "product_signature": "Product signature",
    "visual_hierarchy": "Visual hierarchy",
    "density_and_typography": "Density and typography",
    "theme_and_material": "Theme and material",
    "iconography": "Iconography",
    "surface_composition": "Surface composition",
    "responsive_quality": "Responsive quality",
    "state_and_interaction_polish": "State and interaction polish",
}


def resolve_local_path(raw_path: str, manifest_path: Path) -> Path | None:
    direct = Path(raw_path).expanduser()
    if direct.is_absolute():
        return direct.resolve()
    parsed = urlparse(raw_path)
    if parsed.scheme and parsed.scheme != "file":
        return None
    value = (
        url2pathname(unquote(parsed.path))
        if parsed.scheme == "file"
        else raw_path
    )
    path = Path(value).expanduser()
    if not path.is_absolute():
        path = manifest_path.parent / path
    return path.resolve()


def data_uri(path: Path) -> str:
    mime = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
    encoded = base64.b64encode(path.read_bytes()).decode("ascii")
    return f"data:{mime};base64,{encoded}"


def collect_evidence(data: dict[str, Any]) -> dict[str, set[str]]:
    usage: dict[str, set[str]] = defaultdict(set)
    review = data.get("review")
    if isinstance(review, dict):
        for raw_path in review.get("reference_images", []):
            if isinstance(raw_path, str) and raw_path:
                usage[raw_path].add("reference")
    for field in ("states", "regions", "dynamic_checks"):
        for entry in data.get(field, []):
            if not isinstance(entry, dict):
                continue
            for raw_path in entry.get("evidence", []):
                if isinstance(raw_path, str) and raw_path:
                    usage[raw_path].add(f"{field[:-1]}: {entry.get('id', '?')}")
    for entry in data.get("measurements", []):
        if not isinstance(entry, dict):
            continue
        raw_path = entry.get("evidence")
        if isinstance(raw_path, str) and raw_path:
            usage[raw_path].add(f"measurement: {entry.get('id', '?')}")
    return usage


def render_scores(review: dict[str, Any]) -> str:
    scores = review.get("scores") if isinstance(review.get("scores"), dict) else {}
    cards = []
    for score_id, label in SCORE_LABELS.items():
        entry = scores.get(score_id) if isinstance(scores.get(score_id), dict) else {}
        score = entry.get("score", "—")
        note = escape(str(entry.get("note", "Not reviewed")))
        cards.append(
            '<article class="score">'
            f'<div><span>{escape(label)}</span><strong>{escape(str(score))}/5</strong></div>'
            f'<p>{note}</p>'
            "</article>"
        )
    return "\n".join(cards)


def render_claims(data: dict[str, Any]) -> str:
    fields = (
        ("window_backdrop", "Backdrop"),
        ("surface_fill_policy", "Surface fill"),
        ("signature_finish", "Signature"),
        ("chrome_on_material", "Chrome"),
        ("sparse_canvas_treatment", "Sparse canvas"),
        ("primary_input_treatment", "Primary input"),
        ("visible_copy_register", "Visible copy"),
    )
    return "".join(
        f'<span class="claim"><b>{escape(label)}</b>{escape(str(data.get(key, "—")))}</span>'
        for key, label in fields
    )


def render_gallery(
    usage: dict[str, set[str]], manifest_path: Path, *, embed_images: bool
) -> tuple[str, list[str]]:
    cards = []
    missing = []
    for raw_path, labels in sorted(usage.items(), key=lambda item: item[0]):
        path = resolve_local_path(raw_path, manifest_path)
        badges = "".join(f"<span>{escape(label)}</span>" for label in sorted(labels))
        if path is None or not path.is_file():
            missing.append(raw_path)
            cards.append(
                '<article class="capture missing">'
                f"<div class=\"placeholder\">Missing: {escape(raw_path)}</div>"
                f"<div class=\"badges\">{badges}</div>"
                "</article>"
            )
            continue
        try:
            uri = data_uri(path) if embed_images else path.as_uri()
        except OSError:
            missing.append(raw_path)
            continue
        cards.append(
            '<article class="capture">'
            f'<a href="{uri}" target="_blank"><img src="{uri}" alt="{escape(path.name)}"></a>'
            f'<div class="filename">{escape(path.name)}</div>'
            f'<div class="badges">{badges}</div>'
            "</article>"
        )
    return "\n".join(cards), missing


def render_findings(review: dict[str, Any]) -> str:
    findings = review.get("findings")
    if not isinstance(findings, list) or not findings:
        return '<p class="empty">No reviewer findings recorded.</p>'
    rows = []
    for finding in findings:
        if not isinstance(finding, dict):
            continue
        rows.append(
            "<tr>"
            f"<td>{escape(str(finding.get('severity', '—')))}</td>"
            f"<td>{escape(str(finding.get('status', '—')))}</td>"
            f"<td>{escape(str(finding.get('summary', finding.get('id', '—'))))}</td>"
            "</tr>"
        )
    return (
        "<table><thead><tr><th>Severity</th><th>Status</th><th>Finding</th></tr></thead>"
        f"<tbody>{''.join(rows)}</tbody></table>"
    )


def render_html(
    data: dict[str, Any], manifest_path: Path, *, embed_images: bool
) -> str:
    review = data.get("review") if isinstance(data.get("review"), dict) else {}
    gallery, missing = render_gallery(
        collect_evidence(data), manifest_path, embed_images=embed_images
    )
    verdict = escape(str(review.get("verdict", "unverified")))
    reviewer = escape(str(review.get("reviewer_id", "not assigned")))
    missing_note = (
        '<div class="alert">Missing evidence: '
        + escape(", ".join(missing))
        + "</div>"
        if missing
        else ""
    )
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{escape(str(data.get('application', 'FluentQt')))} visual review</title>
<style>
:root {{ color-scheme: light dark; font-family: Inter, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; }}
* {{ box-sizing: border-box; }}
body {{ margin: 0; background: #eaf0f6; color: #111a23; }}
main {{ width: min(1540px, calc(100% - 40px)); margin: 0 auto; padding: 34px 0 64px; }}
header {{ display: grid; grid-template-columns: 1fr auto; gap: 24px; align-items: end; margin-bottom: 24px; }}
.eyebrow {{ color: #167f9e; font-size: 12px; font-weight: 750; letter-spacing: .12em; text-transform: uppercase; }}
h1 {{ margin: 8px 0 6px; font-size: clamp(28px, 4vw, 52px); letter-spacing: -.035em; }}
.meta {{ color: #5d6b7a; }}
.verdict {{ min-width: 180px; padding: 14px 18px; border: 1px solid #b8c7d6; border-radius: 14px; background: #fff; }}
.verdict strong {{ display: block; margin-top: 4px; font-size: 22px; }}
.claims {{ display: flex; flex-wrap: wrap; gap: 8px; margin: 18px 0 28px; }}
.claim {{ display: inline-flex; gap: 8px; padding: 8px 11px; border: 1px solid #c4d1dd; border-radius: 999px; background: rgba(255,255,255,.72); font-size: 12px; }}
section {{ margin-top: 32px; }}
h2 {{ margin: 0 0 14px; font-size: 22px; }}
.scores {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(230px, 1fr)); gap: 12px; }}
.score {{ min-height: 132px; padding: 16px; border: 1px solid #c4d1dd; border-radius: 14px; background: #fff; }}
.score div {{ display: flex; justify-content: space-between; gap: 12px; }}
.score strong {{ color: #087fa2; font-size: 20px; }}
.score p {{ margin: 14px 0 0; color: #5d6b7a; font-size: 13px; line-height: 1.45; }}
.gallery {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(390px, 1fr)); gap: 16px; }}
.capture {{ overflow: hidden; border: 1px solid #b8c7d6; border-radius: 14px; background: #fff; }}
.capture img {{ display: block; width: 100%; max-height: 680px; object-fit: contain; background: #dfe7ee; }}
.filename {{ padding: 12px 14px 4px; font-size: 13px; font-weight: 700; }}
.badges {{ display: flex; flex-wrap: wrap; gap: 6px; padding: 8px 14px 14px; }}
.badges span {{ padding: 4px 7px; border-radius: 6px; background: #e5f5f9; color: #076d89; font-size: 11px; }}
.placeholder {{ min-height: 220px; display: grid; place-items: center; color: #a23b34; }}
.alert {{ margin: 16px 0; padding: 12px 14px; border-radius: 10px; background: #ffe2df; color: #8c2b25; }}
table {{ width: 100%; border-collapse: collapse; overflow: hidden; border-radius: 12px; background: #fff; }}
th, td {{ padding: 12px 14px; border-bottom: 1px solid #d9e1e8; text-align: left; }}
.empty {{ padding: 18px; border: 1px dashed #aab8c5; border-radius: 12px; color: #5d6b7a; }}
@media (prefers-color-scheme: dark) {{
  body {{ background: #171b20; color: #f4f7fa; }}
  .meta, .score p, .empty {{ color: #aeb9c4; }}
  .verdict, .score, .capture, table {{ background: #22282f; border-color: #3a4651; }}
  .claim {{ background: #22282f; border-color: #3a4651; }}
  .capture img {{ background: #11161a; }}
  .badges span {{ background: #123d48; color: #8fe3f5; }}
  th, td {{ border-color: #3a4651; }}
}}
</style>
</head>
<body><main>
<header><div><div class="eyebrow">Independent visual review</div>
<h1>{escape(str(data.get('application', 'Application')))}</h1>
<div class="meta">{escape(str(data.get('platform', 'platform not recorded')))} · profile {escape(str(data.get('profile', '—')))}</div>
</div><div class="verdict"><span>Reviewer: {reviewer}</span><strong>{verdict}</strong></div></header>
<div class="claims">{render_claims(data)}</div>
{missing_note}
<section><h2>Judged dimensions</h2><div class="scores">{render_scores(review)}</div></section>
<section><h2>References and final-state evidence</h2><div class="gallery">{gallery}</div></section>
<section><h2>Findings</h2>{render_findings(review)}</section>
</main></body></html>"""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--embed-images",
        action="store_true",
        help="Embed full-resolution evidence for a portable but potentially large HTML file.",
    )
    args = parser.parse_args()
    try:
        data = json.loads(args.manifest.read_text(encoding="utf-8"))
        review = data.get("review") if isinstance(data.get("review"), dict) else {}
        declared = review.get("review_board")
        output = args.output
        if output is None and isinstance(declared, str) and declared:
            output = resolve_local_path(declared, args.manifest)
        output = (output or args.manifest.parent / "visual-review.html").resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(
            render_html(data, args.manifest, embed_images=args.embed_images),
            encoding="utf-8",
        )
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: could not render visual review: {exc}", file=sys.stderr)
        return 1
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
