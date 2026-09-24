# Product Reference Patterns

Study how these products connect the primary object, time model, signature
surface, panel lifetime, density, and interaction. Adapt those relationships
without copying a brand, screenshot, icon set, color palette, or exact geometry.

Sources were reviewed on 2026-08-12. Recheck the official source when a design
decision depends on behavior that may have changed.

## Contents

- Reference-synthesis protocol
- Fast selection matrix
- Multi-document command workbench
- Spatial authoring workbench
- Focused collaborative editor
- Nearby-device transfer flow
- Live production console
- Document knowledge workspace
- Acceptance gate

## Reference-synthesis protocol

Run this only after defining the target product signature.

1. Choose one **aligned reference** whose primary object and dominant time
   model match the target. Extract no more than three structural rules.
2. Choose one **contrast reference** with a meaningfully different surface or
   tempo. Use it to expose an alternative composition or a failure mode.
3. Record what transfers, what is rejected, and why. Explicitly exclude brand
   marks, proprietary assets, exact colors, and screenshot-level geometry.
4. Generate three concepts from the target evidence. At most one may preserve
   the aligned reference's complete region topology; the others must change the
   primary surface, panel lifetime, reading direction, or narrow transformation.
5. Select FluentQt components only after a concept wins. The component probes
   below are questions to investigate, not required inventories.

Use this compact record:

| Field | Required answer |
| --- | --- |
| Target signature | Primary object, time model, hero interaction, and signature surface |
| Aligned reference | Why its structural grammar fits; up to three transferable rules |
| Contrast reference | Which different grammar was tested and what it revealed |
| Rejected traits | Regions, density, or interactions that conflict with target evidence |
| Original synthesis | The target-specific composition and narrow-layout transformation |
| Excluded copying | Marks, assets, exact colors, text, and screenshot geometry not reused |

If no card fits, use a neutral Fluent baseline and state the gap. Do not force a
target into this library merely because a card looks polished.

## Fast selection matrix

| Reference | Primary object | Dominant time model | Signature grammar | Prefer when | Avoid when |
| --- | --- | --- | --- | --- | --- |
| VS Code | Editable documents in a project | Persistent edit/build/debug workspace | Dominant editor with switchable side views, auxiliary panel, tabs, and status | Peer documents and project context remain continuously useful | The task is a short guided flow or has no user-managed documents |
| Godot | Scene or spatial artifact | Stateful authoring and play-test loop | Central viewport, hierarchy, contextual tools, property inspection, collapsible diagnostics | Direct manipulation and selection drive secondary properties | The central object is not spatial or direct manipulation is incidental |
| Zed | Code/document panes with collaborators and agent threads | Focused editing with contextual collaboration | Restrained editor canvas, split panes, project tree, command-first transient tools | The canvas must stay quiet while advanced actions remain fast | Continuous monitoring requires many always-visible instruments |
| LocalSend | Payload plus nearby destination | Discover, choose, transfer, complete | Responsive destination collection, direct send action, explicit progress and recovery | The workflow is short, endpoint-oriented, and often touch-adjacent | Dense expert editing or many simultaneous persistent tools are required |
| OBS Studio | Live composed output | Continuous real-time operation | Preview/program canvas with scene/source collections, meters, and direct controls | Operators must monitor and adjust several live signals at once | The product is primarily asynchronous reading or document creation |
| AppFlowy | Hierarchical document or database view | Persistent knowledge editing and organization | Quiet document canvas, workspace tree, view switching, contextual block commands | Users create durable knowledge in several representations | The result is ephemeral or dominated by a live operational loop |
| Run-first coding agent (Codex-style) | Ordered agent run or conversation | Streaming transcript + integrated composer | Compact session rail, mixed-height turn timeline, one-line composer dock | The run/conversation is the primary object | Peer documents or an IDE editor must remain the dominant canvas |

## Multi-document command workbench

**Reference:** [Visual Studio Code repository](https://github.com/microsoft/vscode)
and [official interface guide](https://code.visualstudio.com/docs/editing/userinterface).

VS Code's interface guide describes a dominant editor with primary and secondary
side bars, an activity switcher, a movable panel, status, and side-by-side editor
groups. Users can configure these regions around the documents they are editing.
Do not reduce this reference to adding a left-hand rail; preserve configurable
region ownership around peer documents.

- Transfer: let the editable artifact own most space; keep navigation and
  output subordinate; persist the user's tabs, splits, and visibility choices.
- Density: compact chrome and rows are justified because the user repeatedly
  traverses files, symbols, diagnostics, and commands.
- Narrow behavior: preserve one focused editor; collapse or overlay auxiliary
  regions rather than squeezing every pane.
- Component probes: `TabView`, `SplitView`, `TreeView`, `CommandBar`,
  `CommandBarFlyout`, `AnnotatedScrollBar`, and compact status controls.
- Reject: a permanent activity rail when there are not several stable tool
  modes; a bottom panel that is empty outside debugging or output tasks.

## Spatial authoring workbench

**Reference:** [Godot repository](https://github.com/godotengine/godot) and
[official editor tour](https://docs.godotengine.org/en/stable/getting_started/introduction/first_look_at_the_editor.html).

Godot's central viewport has tools that follow the current selection, with side
docks for scenes, files, and properties. A bottom panel expands only for debugging,
animation, audio, and related tasks. The selected canvas object gives the
surrounding tools their context.

- Transfer: prioritize the authored scene; bind hierarchy selection, property
  inspection, and commands to the same selected object.
- Density: tool chrome can be compact, while property groups need clear label
  and value columns with reliable disclosure.
- Narrow behavior: keep the viewport viable; move the inspector to a drawer or
  switchable full-height page and fold diagnostics by default.
- Component probes: `SplitView`, `TreeView`, `SelectorBar` or `Pivot`,
  `CommandBar`, `Expander`, `Slider`, and `DrawerView`.
- Reject: using dock panels as decorative containers; showing an inspector when
  no selection-dependent properties exist.

## Focused collaborative editor

**Reference:** [Zed repository](https://github.com/zed-industries/zed),
[official project-panel guide](https://zed.dev/docs/project-panel), and
[official window/project model](https://zed.dev/docs/windows-and-projects).

Zed keeps the editor canvas restrained while exposing advanced actions in
context. Its documented features include a project tree, preview and permanent
tabs, preserved splits, project-scoped context, collaboration, and command-driven
navigation.

- Transfer: distinguish previews from committed work surfaces; keep project,
  collaboration, and agent context scoped to the active project.
- Density: reserve persistent chrome for high-frequency state; reveal rare
  commands through searchable or anchored transient surfaces.
- Narrow behavior: focus one pane and make supporting panels mutually
  exclusive rather than stacking all of them.
- Component probes: `TabView`, `TreeView`, `SplitView`, `CommandBarFlyout`,
  `Flyout`, `InfoBadge`, and compact `DrawerView` surfaces.
- Reject: equating “minimal” with missing state; hiding errors, collaboration
  identity, or unsaved state that users must continuously trust.

## Nearby-device transfer flow

**Reference:** [LocalSend repository](https://github.com/localsend/localsend)
and [official product flow](https://localsend.org/).

LocalSend lets users select content, choose a nearby device, and follow transfer
progress without an account. Finding and confirming the destination is the main
decision in this short workflow.

- Transfer: keep payload, destination, security/availability, progress, and
  completion in one understandable sequence.
- Density: discovered endpoints need generous target separation, but desktop
  controls and status rows should not inherit oversized mobile metrics.
- Narrow behavior: reflow the destination collection and keep progress/action
  feedback adjacent to the selected endpoint.
- Component probes: `GridView` or `ListView`, `CompoundButton`, `ProgressBar`,
  `InfoBar`, `TeachingTip`, and `ContentDialog` for blocking acceptance.
- Reject: permanent navigation for a three-step flow; card grids for dense logs
  or settings; silent discovery failure.

## Live production console

**Reference:** [OBS Studio repository](https://github.com/obsproject/obs-studio),
[official overview](https://obsproject.com/kb/obs-studio-overview), and
[audio mixer guide](https://obsproject.com/kb/audio-mixer-guide).

OBS places scene/source ordering, meters, controls, and status around live
composition. Operators can monitor the output continuously and act on its
current state. Studio mode separates the editable preview from live program
output.

- Transfer: keep the live output or monitored artifact dominant; show status
  where an operator can verify it without navigating away.
- Density: high information density is acceptable when every row drives live
  awareness or control; stable alignment matters more than decorative cards.
- Narrow behavior: preserve the preview and critical start/stop/status actions;
  collapse secondary collections into switchable panels.
- Component probes: `SplitView`, `ListView`, `Slider`, `ToggleButton`,
  `InfoBadge`, `CommandBar`, and explicit confirmation dialogs.
- Reject: burying live state in toasts; using identical treatment for safe,
  armed, active, muted, and destructive controls.

## Document knowledge workspace

**Reference:** [AppFlowy repository](https://github.com/AppFlowy-IO/AppFlowy)
and [official architecture overview](https://docs.appflowy.io/docs/documentation/software-contributions/architecture/frontend/frontend/codemap).

AppFlowy combines a persistent workspace hierarchy with text, grid, and board
views. Use this pattern to give a knowledge object a restrained editing canvas
and alternate structured representations.

- Transfer: let the document or database view dominate; keep hierarchy and
  view switching stable while block-level commands remain contextual.
- Density: use restrained typography hierarchy and whitespace in the canvas;
  keep trees, tables, and property menus compact and predictable.
- Narrow behavior: focus the active document; move the workspace tree to a
  drawer and keep the current view identity visible.
- Component probes: `TreeView`, `Breadcrumb`, `TabView`, `TextEdit`,
  `GridView`, `Pivot`, and contextual `Flyout` surfaces.
- Reject: turning every setting or result into a document block; permanently
  showing database controls for ordinary text documents.

## Coding-agent run workspace

**Reference:** Codex Desktop as a run-first hierarchy. Treat IDE/editor-hosted
agents as contrast references when the editor or artifact owns the primary
surface. Transfer hierarchy and density without copying marks, colors, wording,
or screenshot geometry.

This pattern uses a compact session rail, a transcript that **is** the product,
and an integrated composer dock. Prefer it when the identity is an ordered
run/conversation. Contrast with editor-hosted agents when peer documents must
stay dominant; do not force a run-first shell into an IDE panel.

- Transfer: left rail is workspace identity + session rows + one New action,
  with optional groups when the product has enough durable work to organize;
  grouping, retained history, and active execution capacity remain separate.
  Overflow work queues visibly instead of disabling creation because a runtime
  slot is busy. Keep the New action and contextual move/close operations in
  their owning collection region, and keep the footer inset (12 to 16 px), not
  glued to the window corner. Center
  canvas distinguishes user / assistant / tool / error / permission without
  protocol labels (`Request` / `Agent`). Tool calls are one- or two-line
  chips; status sits on the same row as the name. Composer rest height is
  one Body `lineSpacing` line, grows to ~6, and Stop replaces Send. Header
  chrome is one ~32 px slot (title + mode + status); theme lives in the
  window chrome, not the traffic-light corner.
- Density: 4 px rhythm, 12/16 pane inset, 8 px related gap. Messages have a
  readable measure; they are not sparse labels in a mica void.
- Narrow behavior: collapse the session rail; keep the transcript and
  composer.
- Component probes: `ListView` + mixed-height delegate, Subtle
  `DropDownButton`, `TextEdit` composer, permission `InfoBar`.
- Reject: Gallery-uniform 32 to 36 px rows as tool cards; bottom-anchoring a
  short transcript; wrapping the composer in a `Card`; treating handshake
  frames as turns; decorative wires between a selected session row and detail;
  repeating that row as an oversized empty-state card.

## Acceptance gate

- The aligned and contrast references were chosen from target evidence, not
  aesthetic preference alone.
- The synthesis names transferable and rejected traits; it does not say only
  “inspired by” a product.
- No proprietary asset, brand mark, exact color palette, product copy, or
  screenshot geometry is reused.
- At least two concepts depart from the aligned reference's complete region
  topology.
- Components were selected from the winning target-specific concept, not from
  a reference card's probe list.
- The final application remains distinguishable from both reference products
  when rendered without logo and accent color.
