# Component Selection

Choose controls from user intent, state semantics, and layout ownership. Visual
similarity alone is not enough.

Select the product signature and information architecture first. Components
implement that composition; they must not decide it by accident.

If a product reference was used, treat its component probes as questions, not a
shopping list. Select from the target-specific winning concept and re-query the
FluentQt catalog; do not inherit a reference product's widget inventory.

## Contents

- Decision table and selection order
- Semantic opportunity scan
- Shell, grouping, action, and feedback choices
- Raw Qt exception rule
- Component acceptance gate

## Produce a decision table

Before implementation, record the important choices:

| Intent | Required state/behavior | Chosen component | Rejected alternative | Evidence |
| --- | --- | --- | --- | --- |
| Example: run one primary operation | async, disabled while running | `Button` Accent | `ToggleButton` | `--guide actions`, public sample |

Include shell/navigation, primary input, primary action, result presentation,
status/progress, optional detail, and overlays. Small self-evident labels and
dividers do not need individual rows.

## Run a semantic opportunity scan

After the interface concept is selected, query the relevant selection guides
and classify plausible component families as **must use**, **conditional**, or
**not applicable**. Include the semantic trigger for every must-use or
conditional family. For example, a temporary inspectable side surface can make
`DrawerView` conditional; it becomes must-use only when the workflow actually
contains such details.

There is no minimum component count. Do not perform component bingo. A control
that does not own a required behavior, state, lifetime, or data shape is a
liability even when it would make the screen look different. Diversity should
come from the product's primary object, time model, and signature surface, then
from the controls that naturally express them.

## Decide in this order

1. **Semantics:** action, persistent setting, selection, navigation, disclosure,
   progress, feedback, or content.
2. **Lifetime and scope:** persistent, modal, anchored transient, temporary side
   panel, or top-level window.
3. **Data shape:** scalar, short choice list, long collection, hierarchy,
   document, or stream; include expected cardinality and whether it is bounded.
4. **Interaction:** keyboard focus, selection, reordering, cancellation,
   validation, and destructive behavior.
5. **Density:** prominent task surface, normal content, compact toolbar, or
   icon-only chrome.

Choose the component's supported size/style variant before forcing geometry.
Use compact variants for title bars, toolbars, navigation footers, and dense
developer/productivity shells. Do not enlarge a component merely to fill an
underspecified layout; fix the surface hierarchy and spacing instead.

Query the matching catalog guide, then inspect each shortlisted component:

```bash
python3 tools/ai/query_ai_catalog.py --guide actions --json
python3 tools/ai/query_ai_catalog.py --guide layout-surfaces --json
python3 tools/ai/query_ai_catalog.py --component button --json
```

Query one decision or closely related intent per call. Use a broad natural
language query only to form an initial shortlist; do not expect one search to
decide an entire screen.

Verify the public header/import, supported sample, and focused test. Catalog
retrieval is evidence gathering, not permission to use every returned control.

For collection and model/view controls, inspect the code that builds the live
sample, not only the displayed snippet. Record the model roles, delegate,
selection-indicator owner, row height, icon size, and any proxy model. Ordinary
`ListView` text/icon rows may use its built-in delegate; richer rows need an
explicit delegate whose indicator and content insets match the reference.
Variable-height custom delegates **must clip** and keep `sizeHint == paint`;
Gallery's default 32–36 px uniform rows hide overlap. Tool/step rows are
compact Caption chips (name · status on one line), not Standard-sized cards.
A backgroundless `ListView`/`TreeView` that directly reveals composited Mica
must erase its viewport (`CompositionMode_Source`)—filled Gallery lists never
exercise that path. A backgroundless `GridView` uses the same public
`backgroundVisible` contract. When any of these views sits on an intentionally
painted parent instead of directly on composited material, set
`fluentPreserveParentSurface` on both the view and viewport and set
`Qt::WA_NoSystemBackground` on the viewport; remove all three together if the
view returns to direct material. Keep a background when the view is itself the
bounded surface.
A `TextEdit` used as a composer must `setLineHeight` from the text font's
`lineSpacing()`, not `ControlHeight::Standard` (32).
For long or growing data, also record paging/windowing, incremental update,
cache, and editor-materialization policies. `ScrollView` does not virtualize a
layout of child widgets; `ListView` loses virtualization if every index receives
a persistent widget. Follow [Performance and lifecycle](performance-lifecycle.md).

## Common shell choices

- Use `NavigationView` for several fixed top-level product areas.
- Use `TabView` for user-managed peer documents or work surfaces.
- Use `Pivot` or `SelectorBar` for a small set of nearby peer views.
- Use `StackView` for push/pop task flow.
- Use `Breadcrumb` for a real navigable hierarchy, not decoration.
- Use a focused custom sidebar only when it expresses task/session structure
  that the public navigation components cannot model cleanly.

## Grouping and secondary surfaces

- Prefer spacing and typography before adding a `Card`.
- Use `Card` when the surface has a distinct fill, boundary, selection, or
  independent interaction. Do not wrap the composer or pane header in a `Card`
  to fake hierarchy on Mica.
- Use `Divider` for a lightweight region boundary.
- Use `Expander` for one disclosure and `Accordion` for coordinated sections.
- Use `SplitView` when users need persistent resizable regions.
- Use `DrawerView` for a temporary edge panel; use `ContentDialog` for a
  blocking same-window decision; use `Flyout` for anchored contextual content.

## Actions and feedback

- Accent `Button`: one primary commit action in a region.
- Standard `Button`: ordinary visible action.
- Subtle `Button`: window chrome, compact toolbar, or low-emphasis action.
- `DropDownButton` Subtle: pane-header switchers on window material.
- `ComboBox`: forms and settings. Do not use its filled bezel as a Mica pane
  title.
- `ToggleButton`: an action surface that must communicate on/off state.
- `ToggleSwitch`: a persistent setting that takes effect immediately.
- `InfoBar`: persistent inline status or validation with optional action.
- `Toast`: brief confirmation that does not require resolution.
- `ProgressBar`: comparable or precise progress; `ProgressRing`: compact
  activity; `Shimmer`: geometry-preserving initial load.

Destructive behavior must be explicit in wording and interaction state; do not
make every dangerous action permanently red if the component supports a
critical hover/confirmation contract.

## Raw Qt exception rule

Use a raw Qt widget only when no public FluentQt component expresses the needed
contract or a platform/native behavior is essential. Document the gap, keep the
widget behind a small adapter/subclass, and provide Fluent typography, palette,
focus, spacing, and Light/Dark behavior. Do not replace a missing reusable
component with duplicated custom paint code inside an application without
first evaluating composition.

## Component acceptance gate

- Every major control has one clear semantic job.
- The shell follows the selected product concept rather than the first catalog
  pattern returned.
- The opportunity scan contains no decorative component justified only by
  variety, novelty, or quota.
- No page contains competing accent actions.
- Collection controls match hierarchy and selection needs.
- Temporary content uses the correct modal/modeless/anchored lifetime.
- One-shot surfaces are created on demand and destroyed after close; any cached
  temporary surface is lazy and justified by state or measured reuse cost.
- Unbounded collections use model/view virtualization plus a separate paging,
  retention, and incremental-update contract.
- Visible raw Qt exceptions are documented and theme-aware.
- The implemented API matches the verified public sample or header.
- Component size variants match the region's declared density; peer controls do
  not drift into unrelated heights or icon scales.
- Collection rows keep their selection indicator, icon, and text in separate
  measured slots in rest, selected, and hover states.
- Variable-height delegates clip to the row and report a `sizeHint` equal to
  painted height; tool chips hug Caption metrics rather than Standard control
  height.
