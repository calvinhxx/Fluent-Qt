## Context

The project already has automated Qt/GTest coverage and interactive VisualCheck tests. For application-level visual issues such as gallery title bar alignment and icon sizing, the current debug loop still depends on launching a window, capturing a screenshot, and interpreting pixels manually or with AI. That loop is useful for final review but inefficient for layout correctness because most expected behavior can be represented as widget geometry: rectangles, centers, sizes, spacing, device-pixel-aware pixmap sizes, and stable widget identity.

This change introduces a structured verification layer that complements, rather than replaces, VisualCheck. The main consumers are app/gallery tests and future debugging sessions where a text geometry dump is faster and more reliable than screenshot interpretation. Reusable components under `src/components/` are considered stable enough to keep their existing component-level tests unless a specific component visual bug later needs geometry evidence.

## Goals / Non-Goals

**Goals:**
- Make visually important app-owned widgets discoverable through stable object names.
- Provide reusable test helpers for geometry assertions with small pixel tolerances.
- Provide an opt-in geometry dump that prints a readable widget tree or selected named widgets.
- Shift layout debugging toward deterministic evidence before screenshot review.
- Document when to use geometry tests, geometry dumps, screenshots, and manual VisualCheck.

**Non-Goals:**
- Replace manual visual review for subjective polish, density, rhythm, or brand fidelity.
- Introduce image recognition, OCR, or external screenshot analysis dependencies.
- Require reusable components to adopt app-level geometry naming rules.
- Freeze private implementation details that are not visually relevant.
- Add a new visual regression image baseline system in this change.

## Decisions

### Stable object names are the primary lookup contract

Visually important app-owned widgets SHOULD receive names such as `GalleryTitleBar.BackButton` or `GalleryTitleBar.SearchBox`. Tests and dumps can then locate widgets through Qt's object tree without exposing implementation pointers.

Alternative considered: expose test-only accessors for each widget. That creates extra public or friend-level API pressure and scales poorly. Object names are already native to Qt, easy to inspect, and can remain scoped to app visual/test contracts.

### Geometry helpers assert contracts, not screenshots

Reusable helpers should compare rectangles, centers, sizes, spacing, and containment with a small tolerance, normally 1 px unless a test has a device-pixel-specific reason to use another value. This directly catches the class of bugs that screenshots currently reveal late: controls not vertically centered, icons rendered too large, text/button content not aligned, or anchor constraints drifting.

Alternative considered: keep screenshot-only verification and ask AI to inspect captures. That is slower, harder to automate reliably, and produces weaker failure output than a geometry diff.

### Geometry dump is opt-in diagnostic output

The dump should be enabled by an environment variable or explicit test helper call so normal CTest output stays quiet. Output should include stable object name, class name, local rectangle, global rectangle when available, center, size hint, visibility, enabled state, device pixel ratio, and selected pixmap/icon logical size when known.

Alternative considered: always print geometry during tests. That would add noise to every run and make real failures harder to read.

### Screenshots remain a final review artifact

Screenshots and VisualCheck remain useful after geometry contracts pass, especially for typography, color, material, motion, and subjective visual balance. The expected workflow is geometry first, screenshot second, human final judgment when polish matters.

Alternative considered: remove screenshot review from this area. That would miss rendering issues that geometry cannot express, such as wrong color, blurry icon source, font weight, or poor perceived balance despite correct bounds.

## Risks / Trade-offs

- Stable object names can become stale when UI is refactored -> Keep names limited to visually important contracts and update tests alongside intentional UI changes.
- Overly strict geometry tests can block legitimate design iteration -> Use named constants, token-derived expectations where possible, and 1 px tolerance for cross-platform rounding.
- Geometry cannot prove subjective visual quality -> Keep VisualCheck and screenshots as complementary review tools.
- Dumping entire widget trees can be noisy -> Default to named widgets or scoped subtrees, with full tree output only when explicitly requested.
- Private child widgets may differ by Qt version or platform -> Prefer app-owned widgets for stable names and keep component or platform/native child assertions out of scope unless a specific visual bug requires them.

## Migration Plan

1. Add test support helpers for named-widget lookup, geometry comparison, and optional dump output.
2. Add stable object names to gallery title bar controls.
3. Convert current gallery title bar layout checks from screenshot interpretation to geometry contract tests.
4. Add documentation to the visual review/testing workflow explaining the geometry-first loop.
5. Keep existing VisualCheck tests unchanged unless a specific test benefits from adding geometry dump output.
