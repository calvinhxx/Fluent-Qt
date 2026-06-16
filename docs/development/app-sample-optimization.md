# App Sample Optimization

Use this workflow when improving Gallery app examples for a component under
`app/view/widgets/samples/`.

The goal is to make each Gallery example a small, trustworthy explanation of one
component capability: the preview UI must demonstrate the same behavior that the
snippet shows, and the snippet must contain the meaningful properties, signals,
slots, and setup used by the preview.

## Entry Prompts

Single component:

```text
Follow docs/development/app-sample-optimization.md to optimize the <ComponentName> app examples.
```

Batch:

```text
Follow docs/development/app-sample-optimization.md to optimize these <Category> app examples:
<ComponentA>, <ComponentB>, <ComponentC>.
Handle each component independently and split commits when requested.
```

## Required Inputs

For each target component, inspect these sources before editing examples:

- Component API: `src/components/<category>/<Component>.h`
- Component implementation when behavior is not clear from the header:
  `src/components/<category>/<Component>.cpp`
- Component tests: `tests/components/<category>/Test<Component>.cpp`
- Current Gallery examples: `app/view/widgets/samples/<Category>Samples.cpp`
- Shared sample helpers: `app/view/widgets/samples/SampleBuilders.*`
- WinUI Gallery reference for the same or nearest equivalent component:
  `https://github.com/microsoft/WinUI-Gallery/tree/main`

Use the WinUI Gallery examples as semantic references, not as code to copy.
Map XAML/C# concepts to this repository's Qt API and local design conventions.

## Capability Matrix

Build a small matrix from the component tests before designing cards:

| Source | What to Extract |
| --- | --- |
| Default property tests | Required baseline example state |
| Property/signal tests | Independent properties that deserve cards |
| Input/keyboard/pointer tests | Interaction flows worth making visible |
| Layout/geometry tests | Size constraints, orientation, overflow, scroll range |
| VisualCheck tests | Existing visual states, theme behavior, disabled/empty states |
| WinUI Gallery examples | User-facing grouping and terminology |

Do not blindly create one card per test. Group closely related behavior when the
UI reads as one concept, but do not mix unrelated concepts in a single card.

## Card Contract

Each `GallerySample` card should satisfy all of these requirements:

- `id` is stable and names the demonstrated capability.
- `title` describes one clear behavior or property family.
- `description` explains why the behavior matters to a user.
- Preview UI demonstrates only the card's semantic focus plus minimal supporting
  state needed to make it understandable.
- Snippet includes the key setup used by the preview: meaningful properties,
  signal/slot connections, model/content wiring, and button actions.
- If the preview has interactive controls, the snippet names the same controls or
  equivalent variables and shows the same calls.
- If the snippet shows a behavior, the preview must make that behavior visible.
- Status labels are used when state changes are otherwise invisible, such as
  selected index, current route, offset, range, zoom factor, or selected item.

Avoid cards where the preview demonstrates several hidden features but the
snippet only says `auto* control = new Component(this);`.

## UI Guidelines

- Prefer existing Fluent components over raw Qt widgets when an equivalent
  exists.
- Reuse local sample helpers before adding new preview infrastructure.
- Use a rounded, token-driven surface when a complex preview needs a clear
  visual boundary.
- Keep dimensions stable with fixed sizes, `sizeHint()` synchronization, or
  responsive constraints so state changes do not reflow the card unexpectedly.
- Keep labels short and wrap status text when it can grow.
- Use multiple small cards instead of one large control panel when properties are
  conceptually independent.
- Do not add instructional text inside the preview that merely describes how to
  use the app. Use labels/status only when they represent live state.

## Snippet Alignment Checklist

Before finishing a component, check every card:

- Every visible toggle/button/selector has a corresponding snippet connection or
  property setup.
- Every non-default property that changes the preview's behavior appears in the
  snippet.
- Every snippet variable name matches the preview's role closely enough to be
  readable.
- The snippet does not include unrelated behavior that the preview does not show.
- The preview does not rely on hidden helper behavior that changes the component
  semantics without being represented in the snippet.

Small helper functions are acceptable in snippets when they keep the snippet
focused, but they must not hide the component API being demonstrated.

## Batch Strategy

When optimizing multiple components:

1. Work one component at a time.
2. Read the component API and tests before editing that component's app samples.
3. Keep helper additions local to the category sample file unless reuse is clear.
4. Build and run the focused test target for that component before moving on.
5. If commits are requested, split commits by component or by tightly related
   sample families.
6. Do not rewrite already accepted cards unless the current component requires a
   shared helper adjustment.

## Verification

At minimum, verify the Gallery target:

```bash
cmake --build --preset vcpkg-osx --target fluent_qt_gallery
```

Then run the focused component tests:

```bash
ctest --preset vcpkg-osx -L '^test_<component>$' --output-on-failure
```

If the component has important visual behavior that is not covered by automated
tests, use the VisualCheck binary or Gallery app for manual review. VisualCheck
tests are interactive by design and may be skipped in automated CTest runs.

## Final Report

Summarize:

- Which cards were added or reworked.
- How UT and WinUI Gallery references informed the grouping.
- Build and focused test results.
- Any dirty worktree state intentionally left untouched.
