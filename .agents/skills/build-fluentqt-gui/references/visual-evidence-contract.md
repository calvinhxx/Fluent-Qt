# Visual Evidence Contract

Use this contract to prevent a visually plausible default screenshot from
hiding clipping, density, alignment, and dynamic-layout defects. Keep the
manifest task-local unless it is useful project documentation.

## Contents

- Build identity and evidence rules
- Mandatory state and region coverage
- Painted geometry checks
- Dynamic convergence checks
- Severity and acceptance
- Manifest shape

## Identify the reviewed build

Record the application, exact executable or package, build configuration,
platform, display scale, theme, capture time, and selected `lite` or `full`
profile. Rebuild before the final review. Do not mix captures from different
binaries in one passing manifest.

Every evidence path must identify a full-window capture or a native-resolution
detail crop. Label picture-in-picture crops with their source state and region.
Do not use a scaled crop to measure logical pixels. The validator resolves
relative evidence paths from the manifest directory and requires the reviewed
build and every passing evidence file to exist. A non-empty invented path does
not count as evidence.

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
- all evidence points to the final reviewed build.

## Use the manifest shape

Create JSON with this minimum shape and validate it with the bundled script:

```json
{
  "application": "example-app",
  "reviewed_build": "/absolute/path/to/executable",
  "platform": "macOS arm64, scale 2x",
  "profile": "full",
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
  "issues": []
}
```

The validator enforces profile-specific bookkeeping, known ids, and local file
existence only. Never convert its success into a claim that the pixels,
interaction, or product hierarchy are good.
