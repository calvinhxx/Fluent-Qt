# Architecture

> **Status:** Current index

[Documentation home](../README.md) · [Contents](../SUMMARY.md)

These contracts describe behavior that component implementations must preserve.
Use the development guides for commands and review steps; use this section for
state, ownership, rendering, and platform boundaries.

```mermaid
flowchart TD
    App[Application]
    Window[Window and native chrome]
    Components[FluentQt components]
    Overlay[Same-window overlays]
    Typography[Typography resolution]
    Inspector[Read-only Inspector]
    Platform[Qt and platform capabilities]

    App --> Window
    Window --> Components
    Components --> Overlay
    Components --> Typography
    Inspector -. observes .-> Window
    Inspector -. observes .-> Components
    Window --> Platform
```

| Contract | Use it when changing |
|---|---|
| [Overlay behavior](overlay-behavior.md) | Popup, Flyout, Dialog, TeachingTip, dropdown, drawer, or another same-window transient surface |
| [Window chrome](window-chrome.md) | Title bars, native move/resize, backdrops, hit testing, or platform window behavior |
| [Typography resolution](typography-resolution.md) | Fonts, inherited application typography, role resolution, or text scaling |
| [Inspector report](inspector-report.md) · [schema](inspector-report.schema.json) | Diagnostic rules, findings, generated workbenches, or report consumers |

Public compatibility rules are in the
[compatibility policy](../development/compatibility-policy.md). Visual choices
are governed by the [Fluent design contract](../design-languages/README.md).
