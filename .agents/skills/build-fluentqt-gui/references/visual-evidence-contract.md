# Visual Evidence Contract

Use this contract to prevent a visually plausible default screenshot from
hiding clipping, density, alignment, and dynamic-layout defects. Keep the
manifest task-local unless it is useful project documentation.

## Contents

- Build identity and evidence rules
- Contract version and legacy migration
- Mandatory state and region coverage
- Painted geometry checks
- Dynamic convergence checks
- Independent review
- Severity and acceptance
- Manifest shape

## Identify the reviewed build

Record the application, exact executable or package, build configuration,
platform, display scale, theme, capture time, and selected `lite` or `full`
profile. Rebuild before the final review. Do not mix captures from different
binaries in one passing manifest.

New manifests use `contract_version: 4`, identify the implementation
`author_id`, and point to the validated machine-readable `design_brief`. Record
window material as first-class evidence, not as an optional note:

For full new-GUI or redesign work, the referenced design brief must pass its
default approved-stage validation. `CONCEPTS READY`, a pending/rejected human
decision, or a schematic-only board cannot seed final visual evidence.

| Field | Allowed values | Rule |
| --- | --- | --- |
| `window_backdrop` | `mica`, `acrylic`, `solid`, `host-owned` | Required. `solid` and `host-owned` also require `window_backdrop_reason`. |
| `surface_fill_policy` | `reveal-material`, `opaque-hosts`, `inherit-host` | Required. `opaque-hosts` and `inherit-host` also require `surface_fill_reason`. |
| `signature_finish` | `product`, `wireframe` | Required. `wireframe` never passes. |
| `chrome_on_material` | `quiet`, `filled-stickers` | Required. `filled-stickers` never passes. |
| `sparse_canvas_treatment` | `composed`, `dead-space` | Required. `dead-space` never passes. |
| `primary_input_treatment` | `integrated-dock`, `independent-card`, `none` | Required. `independent-card` also requires `primary_input_reason`. |
| `visible_copy_register` | `user-facing`, `developer-labeled` | Required. `developer-labeled` never passes. |

`host-owned` means an embedded GUI correctly leaves the top-level chrome and
backdrop under host control. It is not permission to create a nested window;
the reason names the host and constraint. Pair it with `inherit-host` when
child fills follow the host contract, or `opaque-hosts` when the host requires
an opaque plugin surface. `inherit-host` is invalid for an application-owned
window.

`reveal-material` means content hosts, split panes, and collection viewports
leave unused pixels unfilled so the window material shows through. Capture
harnesses must not switch the window to Solid merely to make screenshots
flatter. If Solid or opaque hosts are required, the reason must name the host,
capture, or accessibility constraint.

`product` means the primary object is a designed surface, not a labeled log.
`quiet` means pane chrome and the composer are Subtle or integrated, not opaque
stickers on Mica. `composed` means 0/1/2/many items each have an intentional
layout. See [Signature surface](signature-surface.md).

Every evidence path must identify a full-window capture or a native-resolution
detail crop. Label picture-in-picture crops with their source state and region.
Do not use a scaled crop to measure logical pixels. The validator resolves
relative evidence paths from the manifest directory and requires the reviewed
build and every passing evidence file to exist. A non-empty invented path does
not count as evidence.

## Version the manifest

The current contract is version 4. A manifest without `contract_version` is
treated as legacy v1; versions 2 and 3 remain readable so existing task-local
evidence does not fail solely because the validator was upgraded. Version 3
keeps its original seven review dimensions and may reference its version 2
design brief. The validator prints a migration warning. Use `--require-current`
for new work so a legacy manifest cannot be mistaken for current visual
acceptance.

Do not create new v1/v2/v3 evidence. Initialize v4 from a current validated
design brief, inspect the final build, render the review board, and populate the required
fields rather than mechanically copying pass values. Explicit unsupported
versions fail validation.

## Choose evidence breadth proportionally

Use `lite` only for a bounded single surface or a focused correction with no
new collection, asynchronous work, transient lifetime, custom theme bridge, or
application-shell decision. Use `full` for new GUIs and whenever any of those
conditions appears or is uncertain.

Both profiles require the real rebuilt application, Light/Dark, normal/narrow,
painted geometry measurements, long text, focus/disabled review, close
behavior, and an inspect-fix-recheck loop. Lite removes inapplicable matrix
ceremony; it does not relax the visual-quality gate.

| Profile | Required state ids | Required region ids | Dynamic checks |
| --- | --- | --- | --- |
| `lite` | `normal-light`, `normal-dark`, `narrow`, `minimum`, `long-localized-content`, `selected-focus-disabled` | `titlebar`, `primary-viewport`, `footer-or-primary-input` | None unless the work must be reclassified as full |
| `full` | Every state listed below | Every region listed below | Every dynamic check listed below |

An applicable full-profile id may be included in a lite manifest. Never omit an
applicable state merely because the selected profile does not mechanically
require its id.

## Cover mandatory states

Full profile exercises these state classes. Lite uses the subset above plus any
state the surface can actually reach. Use `not-applicable` only with a concrete
reason:

| State id | Minimum evidence |
| --- | --- |
| `normal-light` | Primary workflow at the normal supported size |
| `normal-dark` | Same workflow and content in Dark mode |
| `narrow` | Layout near a real responsive breakpoint |
| `minimum` | Smallest intentionally supported layout |
| `empty` | Empty collection or result with wrapped explanatory text |
| `collection-density` | 0, 1, 2, and 8+ rows or the product-equivalent sparse/dense states |
| `long-localized-content` | Long path/label/error plus CJK or another relevant locale |
| `input-single-line` | Rest, focused, and populated primary input |
| `input-max-lines` | Maximum visible lines plus one overflowing line when multiline input exists |
| `scroll-end` | Final item fully readable at the bottom of every primary scroll surface |
| `async-settled` | Streaming, delayed document layout, or model update after geometry settles |
| `selected-focus-disabled` | Selected collection row, keyboard focus, and disabled action |
| `transient-surface` | Drawer, flyout, dialog, menu, tooltip, toast, or a reason none exists |
| `ime-preedit` | Native input-method preedit/candidate surface or an explicit unverified boundary |

Do not replace real persisted/live content with a demo when the defect depends
on real cardinality, asynchronous delivery, or stored text. Use sanitized data
when capture privacy requires it.

## Cover mandatory regions

Full profile reviews each present region independently at 100%. Lite reviews
its required regions plus every additional region touched by the change:

- title bar and window controls;
- primary navigation, collection, or object list;
- primary viewport or signature surface;
- pane headers and nearby status metadata;
- footer, command surface, or primary input;
- scroll viewport boundaries and terminal content;
- transient and overlay surfaces.

For a region that does not exist, record `not-applicable` and name the product
structure that replaces it.

## Measure painted geometry

Layout rectangles are insufficient when borders, focus rings, shadows, text,
or icons paint inside or outside them. Measure representative painted bounds:

- visible row height and row-to-row cadence;
- actual control stroke bounds, not only widget allocation;
- text baselines and mixed-size optical centers;
- group gaps, section gaps, and edge insets;
- selection indicator, icon, and text slots;
- viewport boundary and the last readable content pixel;
- typography role, font size, and line height for each repeated text style.

Use the Gallery metric when available. Otherwise use the defaults in
`visual-refinement.md`. Record the expected range, actual value, status, and
evidence path. An accidental 1 px stroke does not count as a gap.

Declare every primary scroll viewport as one of:

- `seamless`: content deliberately continues the parent layer;
- `divider`: a lightweight boundary separates regions;
- `panel`: fill and/or border communicates an independent surface.

The declaration must remain legible in both themes and at the terminal scroll
position. Adding a border does not fix clipped content.

## Verify dynamic convergence

Dynamic text and collection surfaces pass only when all of these hold:

1. Apply the final width and content.
2. Allow queued document/model/layout work to run.
3. Sample content size, viewport size, and scroll range.
4. Repeat after the next settled update.
5. Require both samples to agree before capture.
6. Verify wrapped text owns enough height, the last item is fully readable,
   and scroll-follow occurs after the final height change.

For multiline input, bind line height to the selected typography role unless a
component-specific Gallery metric proves otherwise. Grow by visual line until
the declared maximum, then let the editor own overflow scrolling. Test CJK,
mixed Latin/CJK, emoji, pasted long lines, and IME when applicable.

## Require independent visual review

The implementation agent may collect evidence and render the local review
board, but it may not grant final visual acceptance. Give the raw design
brief, review board, and final build to a human or a fresh independent agent.
Do not include the implementation agent's diagnosis, preferred verdict, or
intended fixes in that review task.

Concept selection and final-build review are separate decisions. The human
art-direction owner chooses which high-fidelity comp to build; this reviewer
judges whether the actual application reached that direction across states and
constraints. One person may perform both human roles, but neither role may be
claimed by the implementation agent.

Contract v4 records different `author_id` and `reviewer_id` values plus one of
`human` or `independent-agent` as `reviewer_kind`. A full review includes at
least one local reference image captured at a comparable platform, theme,
scale, and typography setup. The reviewer assigns evidence-backed 1–5 scores
for:

- workflow fit;
- product signature;
- visual hierarchy;
- density and typography;
- theme and material;
- iconography;
- surface composition;
- responsive quality;
- state and interaction polish.

`iconography` judges family coherence, provenance, optical sizing/alignment,
semantic color, state variants, and icon-only clarity. `surface_composition`
judges whether material, panes, cards, dividers, borders, radius, and empty
space form a purposeful layer hierarchy instead of a stack of opaque stickers.

Every score must be at least 4 to pass. A score is a judgment, not a replacement
for concrete findings. Record each issue with severity and status; any open
blocker or major finding fails the contract. Generate the board with
`scripts/render_visual_review.py` so the reviewer sees declared claims,
references, final-state captures, score notes, and findings together.
The default board links full-resolution local images to keep the HTML small.
Use `--embed-images` only when a single portable file is required.

## Classify findings and block acceptance

| Severity | Examples | Acceptance |
| --- | --- | --- |
| `blocker` | clipping, overlap, unreadable last row, stale overlay, unstable height | Must fix |
| `major` | wrong density, inconsistent peer controls, broken hierarchy, weak viewport boundary | Must fix unless user explicitly scopes it out |
| `minor` | small non-repeating optical polish issue | May remain only when recorded |

Do not downgrade a repeated defect because each occurrence is small. A gap,
baseline, or row-height inconsistency repeated across the main workflow is
`major`.

Visual acceptance requires:

- every mandatory state and present region is `pass`;
- every `not-applicable` entry includes a reason;
- no mandatory entry is `unverified`;
- all measurements pass or cite a component-specific exception;
- all dynamic checks pass;
- no open blocker or major issue remains;
- all evidence points to the final reviewed build;
- the design brief passes `scripts/validate_design_brief.py`;
- the independent reviewer differs from the implementation author;
- all nine review dimensions score at least 4/5 with evidence;
- the review board and, for full, comparable local reference images exist.

## Use the manifest shape

Create JSON with this minimum shape and validate it with the bundled script:

```json
{
  "contract_version": 4,
  "application": "example-app",
  "author_id": "implementation-agent-1",
  "design_brief": "/tmp/example-app/design-brief.json",
  "reviewed_build": "/absolute/path/to/executable",
  "platform": "macOS arm64, scale 2x",
  "profile": "full",
  "window_backdrop": "mica",
  "surface_fill_policy": "reveal-material",
  "signature_finish": "product",
  "chrome_on_material": "quiet",
  "sparse_canvas_treatment": "composed",
  "primary_input_treatment": "integrated-dock",
  "visible_copy_register": "user-facing",
  "states": [
    {"id": "normal-light", "status": "pass", "evidence": ["/tmp/normal-light.png"]}
  ],
  "regions": [
    {"id": "titlebar", "status": "pass", "evidence": ["/tmp/titlebar.png"]}
  ],
  "measurements": [
    {"id": "primary-row-height", "expected": "32-36", "actual": "36", "status": "pass", "evidence": "/tmp/list.png"}
  ],
  "dynamic_checks": [
    {"id": "wrapped-text-height", "status": "pass", "evidence": ["/tmp/empty.png"]},
    {"id": "multiline-input", "status": "pass", "evidence": ["/tmp/input.png"]},
    {"id": "async-scroll-end", "status": "pass", "evidence": ["/tmp/scroll-end.png"]}
  ],
  "issues": [],
  "review": {
    "reviewer_kind": "independent-agent",
    "reviewer_id": "visual-reviewer-1",
    "reviewed_at": "2026-08-18T12:00:00Z",
    "verdict": "pass",
    "review_board": "/tmp/example-app/visual-review.html",
    "reference_images": ["/tmp/example-app/gallery-reference.png"],
    "scores": {
      "workflow_fit": {"score": 4, "note": "The primary workflow is immediately visible.", "evidence": ["/tmp/example-app/normal-light.png"]},
      "product_signature": {"score": 4, "note": "The primary object is recognizable without logo or accent.", "evidence": ["/tmp/example-app/normal-light.png"]},
      "visual_hierarchy": {"score": 4, "note": "The signature surface dominates supporting chrome.", "evidence": ["/tmp/example-app/normal-dark.png"]},
      "density_and_typography": {"score": 4, "note": "Repeated rows and type follow recorded metrics.", "evidence": ["/tmp/example-app/normal-light.png"]},
      "theme_and_material": {"score": 4, "note": "Both themes preserve contrast and reveal Mica.", "evidence": ["/tmp/example-app/normal-light.png", "/tmp/example-app/normal-dark.png"]},
      "iconography": {"score": 4, "note": "One licensed family keeps peer actions optically aligned across states.", "evidence": ["/tmp/example-app/states.png"]},
      "surface_composition": {"score": 4, "note": "Material, panes, dividers, and bounded surfaces form a deliberate hierarchy.", "evidence": ["/tmp/example-app/normal-dark.png"]},
      "responsive_quality": {"score": 4, "note": "Narrow layout preserves the primary object.", "evidence": ["/tmp/example-app/narrow.png"]},
      "state_and_interaction_polish": {"score": 4, "note": "Focus, disabled, transient, and terminal states are finished.", "evidence": ["/tmp/example-app/states.png"]}
    },
    "findings": []
  }
}
```

For v4, the validator enforces profile-specific bookkeeping, known ids, local
file existence, the window-material and signature fields, a validated design
brief, and independent review metadata. Never convert script success alone into
a claim that the pixels, interaction, or product hierarchy are good; the named
reviewer owns that judgment. A manifest with `window_backdrop: "solid"` or
`"host-owned"` and no reason fails. A manifest that declares `wireframe`,
`filled-stickers`, `dead-space`, or `developer-labeled` is a failed product
surface.
