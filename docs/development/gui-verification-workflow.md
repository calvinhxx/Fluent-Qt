# AI-assisted GUI verification

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Gallery and site

[← Gallery preview workflow](gallery-preview-workflow.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Gallery Control Images →](gallery-control-images.md)
<!-- docs-nav:top:end -->

Use this workflow when a visible change needs repeatable evidence across themes,
window sizes, interaction states, geometry, accessibility, and exact rendering.
The system produces deterministic evidence first, then requires a separate AI
or human reviewer for the final visual decision.

## What the gate proves

No single screenshot metric is treated as a visual verdict. A recipe combines
five independent layers:

| Layer | Evidence | Failure meaning |
|---|---|---|
| Coverage | Required scenario tags | A declared theme, width, state, or input path was never exercised |
| Behavior | Mouse/keyboard steps and property assertions | The intended interaction did not reach the named control or produce the expected state |
| Semantics | Inspector findings and named-widget geometry | Accessibility, clipping, focus, hit area, or layout contracts regressed |
| Rendering | Full-frame and high-risk-region pixel comparison | Color, rasterization, size, or painted geometry exceeded the approved policy |
| Judgment | Digest-bound independent review | Hierarchy, balance, typography, contrast, and polish were not accepted by a different reviewer |

The runner records `pass`, `fail`, `incomplete`, `human-required`, and
`review-required` separately. Missing baselines, a changed capture fingerprint,
a headless run when native desktop evidence is required, or an absent review
can never become `pass`.

## Start a recipe

Copy the checked-in example into an ignored build directory and change the
author id, selection, coverage tags, probes, actions, and review prompts:

```bash
mkdir -p build/gui-verification/recipes
cp tools/dev/gui-verification.example.json \
  build/gui-verification/recipes/my-change.json
```

The recipe is checked against the executable contract in
`tools/dev/fluent_qt_gui_verify.py`; editor tooling can also use
`tools/dev/gui-verification-recipe.schema.json`. `path_base` makes relative
paths explicit: use `repository` for checked-in baselines and repository-owned
action files, or `recipe` for a self-contained portable recipe directory.
Absolute paths ignore this setting. The checked-in example uses `repository`,
so copying it under `build/` does not change which baseline bundle it targets.
Each scenario must declare:

- a stable id, Light or Dark theme, logical viewport size, and coverage tags;
- a platform-specific or default baseline directory;
- named geometry probes with zero or explicit pixel tolerance;
- an Inspector budget and full-frame pixel policy;
- concrete prompts for the independent reviewer;
- optional interaction actions for focus, pointer, keyboard, or staged state.

Run all scenarios from the repository root:

```bash
python3 tools/dev/fluent_qt_gui_verify.py run \
  --recipe build/gui-verification/recipes/my-change.json \
  --preset vcpkg-osx
```

The default output is under `build/gui-verification/`. Pass `--output-dir` for
a stable artifact location. The runner builds the Gallery and visual comparator
unless `--no-build` is set.

## Read the first result

A first capture normally ends as `human-required` because no approved baseline
exists. This is expected only when every scenario's `pre_baseline_status` is
`pass`. Open these artifacts before approving anything:

- `evidence.json` — commands, binary hashes, Git state, checks, reports, and artifact hashes;
- `review.html` — actual, baseline, and diff images plus scenario prompts;
- `review-request.json` — immutable evidence digest and a review JSON template;
- `scenarios/<id>/capture.json` — environment, actions, named geometry, and Inspector evidence;
- `scenarios/<id>/actual.png` — native-resolution capture.

The report's environment fingerprint includes Qt, platform plugin, style,
device-pixel ratio, logical DPI, locale, font, operating system, CPU, and Qt
scale variables. Pixel comparison runs only when that fingerprint exactly
matches the approved baseline. Geometry remains in logical coordinates.
High-risk pixel regions default to logical coordinates and are converted to
device pixels with the recorded device-pixel ratio.

## Approve a baseline

The approver must inspect the first capture at native resolution and use an id
different from `recipe.author.id`:

```bash
python3 tools/dev/fluent_qt_gui_verify.py approve \
  --evidence build/gui-verification/<run>/evidence.json \
  --scenario light-normal-default \
  --approved-by reviewer-id \
  --approver-kind human \
  --approval-note "Checked typography, alignment, focus, and contrast at native resolution"
```

Approval writes `baseline.png`, `baseline-report.json`, and `baseline.json`.
The metadata binds the recipe/scenario/action contract plus the image and
report hashes to the approver. Existing bundles are immutable by default;
`--replace` is an explicit superseding
operation and requires another review. Do not generate or update baselines
automatically after a mismatch.

Use separate baseline paths per platform or platform/architecture when the
recipe needs multiple native renderers. A baseline from one environment is not
silently reused on another.

## Script interactions and states

The compiled preview accepts an action file directly:

```bash
python3 tools/dev/fluent_qt_preview.py \
  --route combobox \
  --sample combobox-editable \
  --actions path/to/actions.json \
  --snapshot build/preview/actual.png \
  --report build/preview/capture.json
```

The action contract is in `tools/dev/gallery-preview-actions.schema.json`.
Targets use stable `objectName` values. `descendant_class` can select one unique
internal editor beneath a stable compound control. `@focus` follows the deepest
focused editor. Supported actions include focus, click, mouse movement,
press/release, key, text entry, wait, and explicit state staging. `expect`
asserts observable Qt properties after the event; missing targets, ambiguous
targets, unknown properties, and failed expectations stop the script by
default.

Use event actions for behavior claims. `set_property` is marked as
`state-staging` in the report and is suitable for capturing a disabled or
selected appearance, not for proving that user input caused that state.

## Require an independent visual decision

After every baseline-backed deterministic check passes, `run` still reports
`review-required` and returns nonzero. Give `review-request.json`,
`evidence.json`, and `review.html` to a fresh AI context or a human reviewer.
The reviewer must open every declared artifact and write a document matching
`tools/dev/gui-verification-review.schema.json`.

Finalize only against the unchanged evidence file:

```bash
python3 tools/dev/fluent_qt_gui_verify.py finalize \
  --evidence build/gui-verification/<run>/evidence.json \
  --review path/to/independent-review.json
```

`finalize` is the only command that returns success for final acceptance. It
rejects a reviewer id equal to the evidence author, a stale evidence hash,
omitted scenarios, missing visual or interaction attestations, and a pass
verdict containing blocker or major findings.

## Boundaries

The action runner injects Qt input events into the real compiled Gallery
widgets. It is deterministic and exercises component event handlers, but it
does not replace platform automation for operating-system focus, input method,
window management, screen-reader, drag-and-drop, or animation timing. Keep a
small native VisualCheck/platform smoke lane for those risks.

Snapshot comparison also does not decide whether a new design is better. A
strict zero-pixel policy catches a one-pixel shift; a controlled tolerance can
handle known raster noise. In both cases, named geometry, Inspector contracts,
coverage, interaction assertions, and independent review remain mandatory parts
of the decision.

<!-- docs-nav:bottom:start -->
---
[← Gallery preview workflow](gallery-preview-workflow.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Gallery Control Images →](gallery-control-images.md)
<!-- docs-nav:bottom:end -->
