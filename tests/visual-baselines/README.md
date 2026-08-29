# Visual baselines

The root PNGs are checked-in legacy representative evidence for the 1.7
Light/Dark/RTL visual gate. They predate digest-bound independent approval
metadata and do not record the Qt or OS version, display, or reviewer. This is
not a screenshot farm. Approved
multi-state GUI verification bundles, when added, live under `gui/` and keep
their image, capture report, sanitized provenance, and approval metadata
together.

## Contents

| File suffix | What it covers |
| --- | --- |
| `button-states-light-ltr.png` | Button Rest / Hover / Pressed / Focus / Disabled in Light, LTR |
| `button-states-dark-ltr.png` | Same Button row in Dark, LTR |
| `tree-view-rtl.png` | Compact TreeView in Light, RTL |

Filenames also include the test target, suite, and case so they stay 1:1 with
`captureVisualSnapshot()`.

## Approval host

Baselines in this directory were captured on **macOS arm64** with the
`vcpkg-osx` preset, Fusion style, bundled Fluent fonts, `QT_SCALE_FACTOR=1`,
and `QT_FONT_DPI=96`. Pixel compare is host-specific. Linux/Windows/offscreen
renders will not match these files.

## Update

```bash
python3 tools/dev/fluent_qt_build.py --preset vcpkg-osx --target test_visual_gate
VISUAL_SNAPSHOT=1 VISUAL_UPDATE_BASELINE=1 QT_SCALE_FACTOR=1 QT_FONT_DPI=96 \
  ./build/vcpkg-osx/tests/components/test_visual_gate
```

Then re-run the gate (see [Testing Workflow](../../docs/development/testing-workflow.md)).
The test skips compare/update outside this approval-host fingerprint, including
macOS x64, Linux, Windows, headless plugins, non-Fusion style, or different
scale/DPI environment values. `QWidget::render()` on
macOS may omit native TreeView chrome (chevrons); RTL text alignment is the
signal this gate checks.

## AI-assisted GUI bundles

Use the
[AI-assisted GUI verification workflow](../../docs/development/gui-verification-workflow.md)
for baseline directories under `gui/`. Each scenario bundle contains:

- `baseline.png` — the native-resolution approved pixels;
- `baseline-report.json` — an environment-fingerprint digest plus only the recipe-required named rectangles, minimal action results, and Inspector summary, without unrelated widgets, environment strings, artifact/action paths, action payloads, widget labels, individual findings, secrets, or display hardware identifiers;
- `source-evidence.json` — sanitized source-evidence, recipe, scenario, artifact, binary, and Git digests without absolute paths, commands, logs, or host timestamps;
- `baseline.json` — independent approver identity plus recipe-contract, image, report, and provenance hashes.

Create or supersede those bundles only with
`python3 tools/dev/fluent_qt_gui_verify.py approve`. The GUI runner binds the
complete acceptance policy and refuses an unapproved, stale-contract,
changed-fingerprint, or modified bundle before pixel comparison. Finalization
recomputes recipe, capture, baseline, binary, and comparator-report integrity.
The complete run `evidence.json` remains an ignored local or CI artifact; do
not copy it into a checked-in bundle because it contains host-specific paths,
commands, logs, timestamps, and unsanitized screen identity fields.
Approver/reviewer IDs are declared identities, not an
external identity proof; keep the human review record outside the generated
evidence when stronger provenance is required. The `VISUAL_UPDATE_BASELINE`
command above is only for the three representative root PNGs. GUI recipes should declare
`"path_base": "repository"` and use paths such as
`tests/visual-baselines/gui/<component>/<scenario>` so moving a recipe under
`build/` cannot redirect baseline approval.
