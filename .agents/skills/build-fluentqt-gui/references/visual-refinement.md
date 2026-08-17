# Visual Refinement

Treat visual quality as an iterative engineering task. A compiling window and a
single screenshot are not a finished GUI.

Visual and engineering quality are co-equal acceptance gates. Use this
reference together with Performance and lifecycle; polished pixels do not
compensate for unbounded object creation, full-model rebuilds, blocked event
loops, or incorrect transient ownership.

## Contents

- Gallery-equivalent quality bar
- Coherent desktop density
- Comparable views and 100% perimeter review
- Backdrop, signature, hierarchy, geometry, interaction, and resilience passes
- Geometry, layer, and final acceptance gates

## Gallery-equivalent quality bar

Use the shipped FluentQt Gallery as the benchmark for finish, not as a layout
template. Product-specific navigation and density may differ, but the result
must still look and behave like it belongs to the same component library.

Choose comparison evidence in this order:

1. the same component in its Gallery sample;
2. the nearest Gallery application pattern or surface;
3. the component `VisualCheck` together with the relevant design tokens.

Compare the application and reference on the same platform, theme, display
scale, and font setup. Review these dimensions explicitly:

| Dimension | Gallery-equivalent evidence |
| --- | --- |
| Components | Public FluentQt controls keep their intended geometry and states |
| Color and material | Semantic tokens, layer roles, contrast, backdrop, and elevation are coherent |
| Typography and icons | Roles, weights, line heights, optical icon sizes, and labels are consistent |
| Spacing and shape | The 4 px rhythm, alignment, control heights, and role-based radii hold throughout |
| Interaction | Hover, pressed, focus, selected, disabled, loading, overlays, and motion feel complete |
| Resilience | Light/Dark, long content, scaling, normal/narrow/minimum widths remain intentional |

Do not require pixel identity: native chrome and product structure may differ.
Do require comparable completeness. An unexplained downgrade in a major
dimension blocks visual acceptance.

## Start with window material, then density

Before choosing row heights, decide whether the slice owns the top-level
window. If it does, install the [Premium shell](premium-shell.md) and compare
the first render with Gallery window chrome on the same platform and theme. If
it is embedded, preserve the host surface and record that constraint. Reject
the first render when an application-owned window is a flat Solid slab or every
host is painted with `bgCanvas` / `bgLayer`. Then finish the applicable
[Signature surface](signature-surface.md) before calling density done.

Then choose one density before composing the shell. Prefer compact desktop metrics
for tool, developer, data, and productivity applications; use a roomier scale
only when touch, accessibility, or content hierarchy requires it. Do not let
each region invent its own scale.

Use an exact component or Gallery metric when one exists. Otherwise start from
these logical-pixel values and change them only for a recorded reason:

| Region or element | Fluent desktop starting point |
| --- | --- |
| Title bar | 40–44 high; mixed chrome content in a shared 24-high slot |
| Compact icon/action | 16–18 icon inside a 24–28 action slot |
| Compact text control | 28–32 high |
| Normal text control | 32–36 high |
| Navigation/list row | 32–36 high |
| Panel inset | 12 compact or 16 normal |
| Related controls | 8 gap |
| Separate compact sections | 12–16 gap |
| Bottom action footer | 44–48 high with a centered compact action |

Keep typography restrained: normally use no more than one title role, one body
role, and one caption role in a surface. Do not enlarge every heading, make
every label strong, or use display typography to compensate for weak hierarchy.
Prefer token typography and icon sizes over ad hoc fonts and glyph scaling.

Build hierarchy with canvas/layer roles, spacing, and typography first. Add a
card or border only when it communicates an independent surface, selection, or
interaction. Avoid nested rounded rectangles, a card around every section,
multiple accent actions, decorative emoji, and a brand tint applied to every
surface.

## Establish comparable views

Create deterministic demo or fixture data that exercises realistic text,
status, progress, and optional panels without credentials or network access.
Capture the same views before and after each refinement round:

- the named Gallery, sample, or `VisualCheck` reference in a comparable setup;
- normal window, Light;
- normal window, Dark;
- narrow window near the responsive breakpoint;
- minimum supported window or the smallest intentionally supported layout;
- important overlay, error, loading, empty, or permission state when applicable.

Use the actual built application. A mockup can guide implementation but cannot
validate font rendering, component states, native chrome, event flow, or resize
behavior.

## Inspect the perimeter at 100%

Full-window screenshots reveal hierarchy but can hide small geometry defects.
Also inspect native-resolution crops of these high-risk regions:

- title bar: platform controls, leading identity, trailing status/actions, and
  shared optical center;
- pane headers and navigation: equal insets, control heights, selected-row
  indicator/icon/text slots, and baseline rhythm;
- bottom edge: composer plus left/right pane footers, matching bottom inset and
  action centers;
- transient edge cases: focus cues, menus, flyouts, tooltips, IME preedit and
  candidate surfaces.

Check every window edge once even when it looks unimportant. Large stretches
often conceal an accidentally oversized footer or a button pinned to the wrong
edge.

## Review in passes

### Pass 0: material

- Does an application-owned window request Mica or Acrylic, or does an embedded
  surface correctly inherit and document its host-owned window?
- Are title-bar rest areas, split gaps, and list viewports unfilled?
- Are the composer and pane chrome on material rather than wrapped in cards?
- Do overlays use elevation from Fluent dialog/flyout/drawer components?

### Pass 1: signature surface

Read [Signature surface](signature-surface.md) and reject the first product
render when any of these appear:

- labeled log rows (`Request` / `Agent` or protocol names) as the timeline;
- a large opaque composer `Card` or filled `ComboBox` pane header on Mica;
- one or two bare rows with no intentional measure, hierarchy, or nearby
  context, or a short transcript vertically centered/bottom-anchored in a tall canvas;
- an empty state that is only placeholder text or blank material;
- missing user/assistant/tool/permission visual grammar when the primary
  object is a run or conversation.
- tool rows that are Standard-height white slabs, or next-turn text showing
  through a Mica `ListView` (viewport not erased; `sizeHint` ≠ paint).
- a composer whose rest height is `ControlHeight::Standard` instead of the
  Body font's `lineSpacing`.

### Pass 2: hierarchy

- Is the primary workflow obvious within a few seconds?
- Do canvas, sidebar, content, inspector, card, and overlay layers separate
  without excessive borders?
- Is there one clear primary action per region?
- Does the layout resemble the chosen reference in hierarchy and density rather
  than only in color?

### Pass 3: geometry and typography

- Check the 4 px spacing rhythm, aligned edges, balanced margins, and consistent
  control heights.
- For mixed-size title-bar and toolbar content, compare optical centers and text
  baselines inside one explicit common-height slot. Independent
  `AlignVCenter` flags do not prove that the composed row is visually aligned.
- Give side-by-side panes that end at the same window edge matching footer
  heights, bottom insets, and action centers. Do not let an unconstrained
  stretch silently become a different-sized footer in each pane.
- Check title/body/caption roles, line length, wrapping, elision, and long paths.
- Check radius by role: controls versus overlays.
- Remove decorative containers and labels that do not improve comprehension.

Measure the important geometry in logical pixels instead of judging only by
eye. Use the component's Gallery metric when it is more specific; otherwise use
these acceptance defaults:

| Contract | Default gate |
| --- | --- |
| Aligned edges or baselines | within 1 px |
| Icon or selection indicator to text | at least 8 px |
| Closely related content | 4 px rhythm |
| Controls in one group | 8 px |
| Separate rows or compact sections | at least 12 px |
| Panel edge inset | 12 or 16 px, consistent on the edge |
| Peer title-bar or footer centers | within 1 px |

Intentional 1 px strokes are not spacing. A painted hover, selected, or focus
background must not consume the gap between adjacent controls.

### Pass 4: interaction detail

- Check rest, hover, pressed, focused, selected, disabled, loading, and
  destructive states that apply.
- Verify icon meaning, optical size, tooltip/accessibility name for icon-only
  actions, and adequate hit targets.
- Verify keyboard order, default action, Escape/cancel, focus return, and modal
  ownership.
- Type with an input method when the product accepts text. Preedit text must
  replace the placeholder, the candidate surface must stay above the app, and
  commit/cancel must not leave stale glyphs.
- Select collection rows and inspect the indicator, icon, and text as separate
  layers. No indicator may touch or paint through text.

### Pass 5: resilience

- Resize across breakpoints instead of testing only two endpoints.
- Use long labels, paths, errors, collection items, and localized text where
  relevant.
- Check scaling and both themes for clipped text, stale palettes, and low
  contrast.
- Exercise cancellation, process teardown, window close, and repeated actions.

## Use available review tools

When desktop automation is available, open the application, switch themes,
resize it, and exercise controls in the live accessibility tree. Codex may use
Computer Use; other agents should use their equivalent UI automation. When only
screenshots are available, capture deterministic states and clearly report
that hover, focus, animation, or live interaction remains manually unverified.

Do not automate a different host application as a substitute for inspecting the
GUI being built.

### Live picture-in-picture detail loop

When Computer Use is available, drive the built application through its live
accessibility tree and keep evidence at two scales:

1. a full-window view that preserves hierarchy and responsive context;
2. native-resolution crops of the current high-risk detail, assembled beside
   or over the full view as a compact picture-in-picture board.

After each interaction, refresh the accessibility state before the next click.
Use the loop to inspect the signature surface, selected navigation state,
drawer/dialog/flyout stacking, title-bar geometry, input edge, and theme
transition. The crop must identify its source state; never approve geometry from
an enlarged or stale crop. A picture-in-picture board is review evidence, not a
substitute for exercising the actual window.

## Record actionable findings

Use short issue/fix notes, for example:

| Severity | Observation | Fix | Recheck |
| --- | --- | --- | --- |
| High | Dark-mode document viewport remains white | Theme raw viewport palette | Both modes |
| Medium | Two accent buttons compete in header | Demote secondary action | Normal/narrow |
| Low | Caption is 2 px off adjacent baseline | Align layout margins | Normal |

Fix high-impact hierarchy, legibility, clipping, and interaction issues before
small cosmetic differences. Rebuild and inspect the same state after each fix;
do not rely on code inspection to predict the final pixels.

## Geometry and layer gate

Before acceptance, mark or measure at least one representative instance of
each repeated geometry: panel inset, header alignment, adjacent controls,
collection row content, label/value pair, section boundary, and composer/input
area. Then exercise selected, focused, disabled, typing/preedit, and transient
surface states that exist.

Acceptance is blocked by any of these:

- text, indicator, icon, placeholder, focus cue, or control background overlaps
  another element unintentionally;
- repeated rows use visibly different gaps or baselines without a semantic
  reason;
- peer title-bar groups or same-edge pane footers use different optical centers,
  heights, or bottom insets without a semantic reason;
- a popup, flyout, menu, tooltip, dialog, input-method candidate, or toast is
  clipped by or painted behind its owner;
- canvas, layer, alternate layer, card, and overlay roles produce an accidental
  nested rectangle or stale surface;
- only the default rest state was inspected.

Reject the first render immediately when any of these common low-fidelity
patterns appears:

- the window is Solid, or pane hosts are opaque-filled, without a recorded
  reason in the visual-evidence manifest;
- compact desktop controls, text, icons, cards, or headings are visibly
  oversized relative to their Gallery counterparts;
- `AlignVCenter` is treated as proof of optical alignment for mixed-size text,
  badges, avatars, or buttons;
- pane footers or title-bar groups that should be peers use different heights,
  baselines, bottom insets, or empty vertical space;
- indicator, icon, and label slots are improvised instead of measured;
- borders, cards, accent colors, font weights, or corner radii are repeated
  without semantic purpose;
- a scaled-down full-window screenshot is the only evidence used to approve
  small chrome, spacing, or layer details;
- the signature surface is a labeled log, a composer or ComboBox sticker on
  Mica, or two short rows in unused canvas.
- a transparent `ListView`/`TreeView` directly on composited Mica paints items
  without first erasing the viewport (`CompositionMode_Source`), or clears
  through an intentionally painted parent instead of preserving it; Gallery
  filled lists do not catch either path.
- a variable-height delegate whose `sizeHint` does not match painted height,
  or that paints outside `option.rect`.
- a `TextEdit` composer that keeps the default 32 px line slot.
- a transcript that paints `# heading` / `---` as raw Body, or caps a turn
  at 8 wrapped lines.
- a tool chip whose title is empty (`· done`) because snapshot `name` was
  not resolved from `tool_calls`, or that shows `[User denied tool execution]`.
- a new session row whose title/preview is a `cwd` path.

If automation cannot exercise native input methods, hover, or a platform
overlay, report that state as unverified. Do not convert missing coverage into a
pass.

## Visual acceptance gate

- Visual acceptance is independent of performance/lifecycle acceptance; both
  must pass.
- Every major surface or control family has a named comparison reference.
- The real application was compared beside that reference in a comparable
  theme and scale.
- No major Gallery-equivalent dimension has an unexplained downgrade.
- An application-owned window uses Mica or Acrylic unless a Solid reason is
  recorded; an embedded surface records and preserves its host-owned material.
- The signature surface is a finished product object, not a labeled log, opaque
  sticker chrome, or vacant mica canvas. See [Signature surface](signature-surface.md).
- Two theme modes and at least two widths were reviewed.
- Important visible states were reviewed or explicitly marked unverified.
- Geometry measurements satisfy the table above or cite the component-specific
  Gallery metric used instead.
- Selected rows, text preedit, and transient surfaces have no overlap, clipping,
  or stale-underlay defects.
- No raw widget breaks the palette.
- Text remains readable with realistic long content.
- Primary and secondary actions have stable hierarchy.
- The final report names concrete issues found and fixed, not only "looks good."
