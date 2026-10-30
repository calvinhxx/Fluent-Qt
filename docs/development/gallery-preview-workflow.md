# Gallery preview workflow

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Gallery and site

[← App Sample Optimization](app-sample-optimization.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [AI-assisted GUI verification →](gui-verification-workflow.md)
<!-- docs-nav:top:end -->

Use the Python Live Scene while adjusting a focused Gallery surface. It keeps
one process and window alive and replaces only the scene widget after a
successful save. Use Native Verify when the result has been moved into C++ and
must be checked against the compiled Gallery sample.

| Capability | Entry point | Current scope |
|---|---|---|
| Inspect, screenshot, click, and type into the running UI | `--session-dir` plus the preview client or MCP adapter | C++ Gallery preview and Python Live Scene |
| Update the preview after saving code | C++ `--watch`; Python file watching is enabled by default | C++ incremental build and process restart; Python scene replacement in the same window |
| Change preview data without rebuilding | `set-data` / `preview_set_data` | Exposed scalar text, values, selection, and control state |

## Watch the three capabilities together

Run the guided settings-page demonstration:

```bash
python3 tools/dev/fluent_qt_settings_demo.py
```

Use `--build-dir` and `--python` if runtime discovery does not select the desired
PySide6 build. The demo opens a real Qt settings window and drives the stdio MCP
adapter. It reads controls, types a workspace name, switches the theme, and
checks that disabling notifications disables their dependent controls. It then
saves a sidebar-width change, verifies that the same process and typed name
survive the reload, and stages long text and read-only states without editing
source. Finally, it resets the staged data and leaves the window open.

The editable scene, screenshots, MCP call log, and assertions are written under
`build/preview/settings-demo/`. When `ffmpeg` is available, captured frames from
that window become `settings-demo.mp4`. Choose another `--output` directory to
run a second instance. The checked-in scene template is
`tools/dev/scenes/settings_page.preview.py`; layout edits during replay affect
only the generated copy. The sample keeps preferences in memory and displays
test notifications in its own window; it does not integrate workspace or update
services.

`--content-only` on the Live Scene launcher hides the development toolbar and
file path for presentation. File watching, the reload shortcut, error feedback,
Inspector reports, and the optional command session remain available.

## Select, edit, and compare a region

Open the same settings page with an optional editing panel:

```bash
python3 tools/dev/fluent_qt_settings_editor.py
```

Pass `--build-dir` and `--python` to select a matching runtime. The launcher
creates `build/preview/settings-editor/settings.preview.py` on first use and
preserves that copy on later launches. Use another `--output` directory for an
independent session. The checked-in scene template is never edited by replay.

1. Choose **Pick region** (`点选区域`), then click a notification row. This click
   selects the registered group and does not toggle its controls. `F8` toggles
   picking, `Esc` cancels it, and the region combo box provides a keyboard path.
2. Adjust row padding, heading emphasis, or the semantic description color.
3. Choose **Preview changes** (`预览修改`). The tool patches registered literal
   values in the scene source and reloads it in the same window.
4. Choose **Compare before and after** (`查看前后对比`) to inspect actual region
   screenshots at the same scale, or copy the exact code diff.
5. Keep or revert the pending edit. A kept edit can also be undone. Source
   revisions are checked before replacement; external edits are never
   intentionally overwritten by an undo. End a conflicted comparison to keep
   the current file without writing it.

Each transaction stores source snapshots and actual before/after images under
the session's `edits/` directory. Pending review survives reopening. History is
local development data and currently retained until its output directory is
removed. The panel has no model-provider connection: manual controls and an
external AI client both use the same registered edit contract. Copying a
selection supplies its source locations, allowed fields, and current revision.

### Register a scene without coupling it to the editor UI

`--edit` on `fluent_qt_live_preview.py` enables this module for other Python
scenes. Register only author-controlled regions and literal parameters:

```python
from fluent_qt_preview_editor import Field, register_region

PREVIEW_EDITS = {"details": {"padding": 16}}

def build(parent):
    root = QWidget(parent)
    layout = QVBoxLayout(root)
    padding = PREVIEW_EDITS["details"]["padding"]
    layout.setContentsMargins(padding, padding, padding, padding)
    register_region(root, root, "details", "Details", [
        Field("padding", "Padding", "int", minimum=4, maximum=32),
    ])
    return root
```

The dictionary must be one UTF-8 module-level `PREVIEW_EDITS = {...}` literal.
Integer ranges, boolean fields, and enumerated string choices are supported.
The tool changes only the selected literal spans, preserving surrounding code,
comments, and line endings. It does not infer arbitrary Python or C++ setters.
An optional region `reveal` callback can switch an application-owned page when
the user explicitly selects a region from the combo box or AI tools.

Scenes may export `after_preview_state_restore(root)` to reconcile derived
presentation after the existing signal-blocked state restoration. The settings
scene uses it to synchronize navigation highlighting and dependent notification
controls. It runs before a candidate replaces the visible scene; a failure
keeps the previous scene. It should not persist settings or trigger external
application actions.

The independent package is `tools/dev/fluent_qt_preview_editor/`:

- `registry.py` declares regions and fields without importing Qt.
- `source.py` and `history.py` own literal patches, revision checks, and recovery.
- `controller.py` connects those operations to the borrowed Live Scene host.
- `picker.py` and `panel.py` provide the optional Fluent editing interface.
- `protocol.py` describes the same operations to MCP clients without loading Qt.

This version supports registered Python scenes only. The Widgets library,
installed public API, normal Gallery launch, and C++ preview authoring remain
independent of this package.

Catalog-backed `--route` / `--sample` commands require a built PySide6 Gallery
contract. Configure the selected bindings build with
`FLUENT_QT_BUILD_PYSIDE6_GALLERY=ON`; see the
[PySide6 Gallery build guide](../../bindings/pyside6/gallery/README.md).

## Start from any catalog sample

List the available route and sample ids:

```bash
python3 tools/dev/fluent_qt_live_preview.py --list-routes
python3 tools/dev/fluent_qt_live_preview.py --list-samples --route button
```

Create an editable scene from a real catalog sample:

```bash
python3 tools/dev/fluent_qt_live_preview.py \
  --route button \
  --sample button-styles \
  --fork-scene \
  --theme dark
```

The launcher prints the generated `.preview.py` path. Edit that file and save;
the open window updates without restarting. A fork is never overwritten.
Without `--fork-scene`, the launcher creates a disposable managed scene for
inspection and regenerates it on the next launch.

On each save the host:

1. builds the new scene under a hidden parent;
2. restores interaction changes for controls with stable `objectName` values;
3. swaps the scene widget only after construction succeeds;
4. keeps the last usable scene visible if the new file fails.

The Reload button handles editors that do not emit a normal save event. Light
and Dark can be switched in the same window.

## Scene contract

A scene is a trusted local Python file that exports
`build(parent) -> QWidget`. `SCENE_TITLE` is optional.

```python
import fluentqt
from PySide6.QtWidgets import QHBoxLayout, QWidget

SCENE_TITLE = "Button styles"


def build(parent):
    root = QWidget(parent)
    layout = QHBoxLayout(root)
    button = fluentqt.Button("Save", root)
    button.setObjectName("saveAction")
    button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    layout.addWidget(button)
    return root
```

Open a hand-written scene with:

```bash
python3 tools/dev/fluent_qt_live_preview.py --scene path/to/example.preview.py
```

Scenes execute with the launcher's filesystem and network permissions. This is
a repository development tool, not a sandbox.

## Operate a running preview

Start a C++ preview with a persistent local session:

```bash
python3 tools/dev/fluent_qt_preview.py \
  --route toggle-switch --sample toggle-switch-scale \
  --session-dir build/preview/session --watch
```

Leave that command running. In another terminal, inspect the real controls,
switch the toggle off without compiling, then click it and capture the result:

```bash
python3 tools/dev/fluent_qt_preview_client.py \
  --session-dir build/preview/session inspect
python3 tools/dev/fluent_qt_preview_client.py \
  --session-dir build/preview/session set-data \
  --target scaledToggleSwitch --property isOn --value false
python3 tools/dev/fluent_qt_preview_client.py \
  --session-dir build/preview/session action \
  --target scaledToggleSwitch --action click
python3 tools/dev/fluent_qt_preview_client.py \
  --session-dir build/preview/session screenshot --output build/preview/current.png
```

`data` lists editable fields. `reset-data` restores the values staged with
`set-data`; it does not undo clicks, application callbacks, or external effects.
The response reports `restored`, `failed`, `pending`, and per-field `errors`.
Restoration is checked against the actual values after all setters have run.
Failed values on surviving targets remain available for a later retry; successful
values are removed from the buffer. A partial failure is an error to CLI/MCP
clients. Destroyed controls expire with their IDs and cannot be restored.
Targets may be a unique `objectName`, an `id` returned by `inspect`, or `@root`.
Anonymous controls also receive IDs. Inspect again after a C++ restart or a
Python scene reload: old IDs are rejected. Duplicate names are rejected too.

The property list covers text and placeholder text, checked/on states, numeric
values, selected indices, visibility, enabled/read-only states, and indeterminate
progress. Each field reports its Qt type and enum choices when available. Values
go through the real widget setters and their signals; source files are not
rewritten. Password text is omitted from editable data. Application table/list
models and arbitrary business objects are not exposed by this first version.

For Python, add the same flag to the existing Live Scene launcher:

```bash
python3 tools/dev/fluent_qt_live_preview.py \
  --route button --sample button-styles --fork-scene \
  --session-dir build/preview/python-session
```

Point the client at `build/preview/python-session`. Imported Gallery helpers use
the checkout's Python source. The launcher separately supplies the selected
build's generated `contract.json` through the child-only
`FLUENTQT_LIVE_PREVIEW_CONTRACT` environment variable. Packaged Gallery keeps its
colocated contract by default. An invalid explicit contract path fails rather
than falling back to another build.

Restart the preview after editing an imported helper; no rebuild is required for
that Python edit. Automatic reload watches the selected scene file, not imported
modules. Catalog export still uses the selected built sample snapshot, and an
existing fork keeps its own copied source. If discovery selects a different
build, pass `--build-dir` and, if needed, its matching `--python` interpreter.

### Connect an AI client through MCP

Start the preview first. Configure a stdio MCP server in the AI client with
these fields, substituting absolute paths for this checkout and session:

```json
{
  "command": "python3",
  "args": [
    "/absolute/Fluent-QT/tools/dev/fluent_qt_preview_mcp.py",
    "--session-dir",
    "/absolute/Fluent-QT/build/preview/session"
  ]
}
```

The adapter exposes `preview_inspect`, `preview_screenshot`, `preview_click`,
`preview_type_text`, `preview_key`, `preview_set_data`, and `preview_reset_data`.
For example, ask the connected AI client: "Inspect the preview, turn the switch
off, click it once, then show a screenshot." Screenshots are MCP image content;
tool failures report errors rather than claiming the operation succeeded.
Both hosts reject an interaction if its target disappears, including deferred
deletion while processing input. No release event is sent after a click's press
has deleted its target.

For a Python scene launched with `--edit`, the same adapter also exposes
`preview_editor_inspect`, `preview_editor_select`, `preview_editor_apply`,
`preview_editor_compare`, `preview_editor_accept`, `preview_editor_revert`,
`preview_editor_undo`, and `preview_editor_dismiss`. Inspect the current
selection and source revision before applying registered field changes.
Comparisons return two actual PNG images plus the source diff. Source edits
replace the scene and expire old widget IDs, so inspect the new tree before
performing subsequent widget actions. Editor requests on a host without
`--edit` fail explicitly.

The transport is newline-delimited JSON-RPC over stdio using the
[MCP transport](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports)
and [tool contracts](https://modelcontextprotocol.io/specification/2025-11-25/server/tools).
The adapter attaches to the selected session; it does not launch the UI or change
the AI client's configuration. Local request/response files connect it to the
GUI thread. One host owns each session directory. Enable sessions only for
trusted development previews, using a directory private to your user account.

### Refresh C++ after saving

`--watch` monitors the Gallery/library source trees and invokes the adaptive
incremental build. A failed build leaves the old preview running. A successful
build restarts the child preview and restores changed scalar values on controls
whose unique `objectName` and class still match. Anonymous controls, focus,
scroll position, application models, and external effects are not restored by
the C++ watcher. Python retains its existing named-control state restoration.

Use repeatable `--watch-path path/to/file-or-directory` arguments to narrow
the watched files. The watcher scans once per second by default, using directory
entry metadata and skipping `.git`, `__pycache__`, `node_modules`, and
`CMakeFiles`. Atomic replacements are detected even when size and modification
time are preserved. Set `--watch-interval 0.5` for a shorter interval; the valid
range is 0.2–60 seconds. Scanning frequency is separate from build-process polling.

Each startup allows 60 seconds by default; `--ready-timeout 120` extends that
limit (valid range: 1–600 seconds). Readiness must belong to the launched process,
and an early process exit is reported immediately. A directory lock permits one
watcher per session. The host's own lock rejects an already running preview;
a slow command reply is not interpreted as an available directory.

With `--no-build`, only the initial build is skipped;
subsequent saves still build. Build progress stays in the terminal, and
`watch.json` in the session directory records `ready`, `building`,
`build_failed`, or `stopped`. Closing the preview or interrupting the watcher
stops its owned child processes. Startup failure or timeout stops the new child;
restart failures retain an error in the watcher report. The previous process
has already exited at this point, so startup failure does not restore it.

## Capture evidence

Both preview layers accept `--snapshot`, `--report`, `--theme`, `--size`, and
`--rtl`. A Live report records reload status and the FluentQt Inspector result:

```bash
QT_QPA_PLATFORM=offscreen \
python3 tools/dev/fluent_qt_live_preview.py \
  --route button --sample button-styles \
  --theme light --size 920x680 \
  --snapshot build/preview/live.png \
  --report build/preview/live.json
```

Inspector findings are review prompts. They are not automatically treated as a
CI failure by this low-level preview command. The
[AI-assisted GUI verification](gui-verification-workflow.md) runner applies an
explicit Inspector budget and treats violations as deterministic failures.

## Verify the compiled C++ sample

After applying the accepted layout and properties to C++, render the real
SampleCard in an isolated native host:

```bash
python3 tools/dev/fluent_qt_preview.py \
  --route button \
  --sample button-styles \
  --theme dark \
  --size 920x680
```

The wrapper builds `fluent_qt_gallery` in parallel unless `--no-build` is
passed. Normal Gallery startup is unchanged when `--preview` is absent.

The native report also records the Qt/platform/font/scale fingerprint and a
logical geometry inventory for visible named widgets. Report schema 2 keeps
those signals separate from the Inspector findings.

To exercise a state before capture, pass a versioned action script:

```bash
python3 tools/dev/fluent_qt_preview.py \
  --route combobox \
  --sample combobox-editable \
  --actions path/to/actions.json \
  --snapshot build/preview/native.png \
  --report build/preview/native.json
```

The action schema is `tools/dev/gallery-preview-actions.schema.json`. Steps
target stable `objectName` values, send Qt mouse or keyboard events, and can
assert observable properties. A failed action or assertion makes the preview
exit nonzero while still writing the report for diagnosis.
An explicit `focus` action requests activation of the target's top-level window
before setting widget focus. Native hosts may refuse activation; keep
`has_focus` assertions when a capture requires keyboard focus.

For documentation imagery, add `--capture-target <objectName>` to capture one
uniquely named, visible widget instead of the entire preview window. The target
must fit its visible ancestors. `--capture-padding` adds 0–64 logical pixels
around it (16 by default); the PNG keeps the native device pixel ratio. Missing,
ambiguous or clipped targets fail instead of silently capturing another region.
The default full-window capture is unchanged. Action assertions can observe a
CheckBox's integer `checkState` as well as ordinary Qt properties.

For a side-by-side review of an edited fork and the compiled sample:

```bash
python3 tools/dev/fluent_qt_compare.py \
  --route button \
  --sample button-styles \
  --scene /path/printed/by-live-scene.preview.py \
  --theme dark \
  --size 920x680
```

The command writes `comparison.html`, both PNGs, and a small JSON manifest under
`build/preview/compare/`. It checks that both runs used the same selection,
theme, direction, and size. `ready-for-review` means those capture conditions
match; it is not a visual verdict or pixel-equality gate.

For a multi-state gate with approved baselines, native-resolution pixel and
region comparison, geometry, Inspector budgets, interaction assertions, and a
digest-bound independent review, use the
[AI-assisted GUI verification workflow](gui-verification-workflow.md).

## Boundary

Sessions are currently repository preview tools. They are not an installed
runtime debugging API for arbitrary downstream applications, and the file-based
session transport is not available in WebAssembly. The public Inspector API
remains read-only. Injected Qt events demonstrate tool behavior; they do not
establish native OS input, IME, accessibility, or cross-platform visual approval.
The C++ screenshot path includes its existing OpenGL-child composition; Python
uses `QWidget.grab()`. Cross-host screenshot parity for 3D content is not claimed.

The Python scene is an authoring aid, not a second production implementation.
C++ components and Gallery samples remain canonical. After Native Verify, run
the component's focused unit, accessibility, geometry, and VisualCheck gates as
usual. Full CTest, packaging, WebAssembly, and cross-platform checks remain at
the normal CI boundary.

The workflow deliberately does not reload native code in process. FluentQt and
the Gallery sample factories are linked into the executable, so safe arbitrary
plugin unloading would add lifetime and ABI complexity without improving this
authoring loop.

<!-- docs-nav:bottom:start -->
---
[← App Sample Optimization](app-sample-optimization.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [AI-assisted GUI verification →](gui-verification-workflow.md)
<!-- docs-nav:bottom:end -->
