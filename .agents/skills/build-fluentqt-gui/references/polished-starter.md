# Maintained workbench starter

Use the bundled `workbench` starter for a new standalone C++ or PySide6
application. It is a production-shaped starting point, not a product concept.

## What the starter guarantees

- a thin composition-root window;
- separate reusable shell and replaceable product page;
- a centered wide-screen stage with a readable maximum width;
- material revealed between a navigation rail and the primary surface;
- a primary page that owns the available height instead of leaving unrelated
  header and footer islands around a dead middle field;
- a compact layout that removes the rail before the primary workflow becomes
  cramped;
- Light/Dark theme wiring, semantic accent setup, tests, CI, and architecture
  boundaries.

The native files are `ui/components/WorkbenchShell.*`,
`ui/pages/WorkspacePage.*`, and `ui/shell/MainWindow.*`. PySide6 mirrors the
same responsibilities.

## Replace the sample, keep the invariants

Replace the workspace fixture, visible copy, page composition, identity assets,
and semantic palette with target-project evidence. Do not ship the starter's
sample page under a different product name.

Keep these invariants until an approved design direction gives a concrete
reason to change them:

1. `MainWindow` composes dependencies and routes intent; it does not accumulate
   page construction, transport, persistence, and workflow state.
2. The primary page is one visible product object that grows with the window.
3. Wide screens constrain reading measure; narrow screens remove conditional
   chrome instead of shrinking every control.
4. Material gaps, cards, borders, and radius express hierarchy. Do not add an
   opaque full-window backing surface or wrap every row in a card.
5. One action has one visible owner. Responsive alternatives are mutually
   exclusive.

## Review the first replacement

Build the real application and inspect at approximately 1080x720, 1440x900,
and the supported minimum size in Light and Dark. Reject the replacement if the
primary workflow occupies only a shallow strip, if a large quiet region has no
role, or if the result is identifiable only by its logo or accent color.
