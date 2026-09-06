# Add a FluentQt GUI to a project

> **Status:** Current guide
>
> **Audience:** Application developers and coding agents

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [AI-assisted development](README.md) › Workflow

[← FluentQt Onboarding Tools](../../tools/onboarding/README.md) · [Contents](../SUMMARY.md) · [AI-assisted development index](README.md)
<!-- docs-nav:top:end -->

Use this guide to choose the integration boundary and first complete workflow.
Use the [FluentQt Skill](../../.agents/skills/build-fluentqt-gui/SKILL.md) for
agent execution, design selection, implementation references, and acceptance
tools. For a bug fix, start from the existing interface and its owning tests;
a new application plan is unnecessary.

## Find reusable behavior

Inspect the relevant application APIs, build metadata, entry points, and tests.
A CLI or TUI can demonstrate behavior without being the architecture a GUI
should copy.

| Evidence | Decision it informs |
|---|---|
| Domain and application APIs | Which behavior can be reused without widgets |
| Process, network, persistence, and authentication calls | Where an adapter must isolate external I/O |
| Progress, cancellation, retry, and shutdown paths | Which states and lifetime rules the GUI must preserve |
| Existing frontends and host lifecycle | Which entry points and window ownership remain intact |
| Supported toolchains and packages | How to build and deliver the new interface |

Keep a task-local analysis only when it helps retain these decisions. Tools
that exchange structured analysis can use
[project-analysis.schema.json](project-analysis.schema.json).

## Choose the boundary

Query a candidate pattern from the repository root:

```bash
python3 tools/ai/query_ai_catalog.py --pattern direct-library --json
```

| Pattern | Use it when | Preserve |
|---|---|---|
| `direct-library` | Stable behavior is callable in-process | Thread affinity, ownership, cancellation |
| `service-api` | A service already owns the operation | Transport types outside widgets |
| `structured-process` | An executable is the reusable surface | Structured I/O and stable errors |
| `plugin-extension` | The host supports embedded frontends | Host event loop, ABI/API, lifecycle, unload |
| `extract-core` | Behavior is trapped in another interface | A small, tested UI-independent service |
| `greenfield` | No application layer exists | Use cases and state defined before the view |

Apply the pattern's `window_ownership` field. A host-owned integration returns
an embedded surface rather than creating another application window or event
loop. When the boundary is uncertain, test a narrow adapter first. Preserve
existing CLI, TUI, service, plugin, and library entry points unless replacement
is requested.

## Deliver one complete workflow

Name the input, invoked operation, result, and applicable loading, empty,
failure, retry, cancellation, and cleanup paths. Build that workflow before
secondary navigation.

```mermaid
flowchart LR
    View[FluentQt view]
    State[View state / controller]
    Adapter[Integration adapter]
    App[Application / domain]

    View -- user intent --> State
    State --> Adapter --> App
    App -- result / event --> Adapter --> State
    State -- render state --> View
```

Application behavior stays outside signal handlers. Keep blocking work off
the GUI thread and marshal results through Qt signals or queued calls. Use
models and delegates for long or growing collections, with an explicit
retention or pagination boundary. Create one-shot surfaces on demand and
verify cleanup.

For a new GUI or major redesign, follow the Skill's
[design workflow](../../.agents/skills/build-fluentqt-gui/SKILL.md#new-or-redesigned-interfaces):
compare three concepts using the same real content, record a human selection,
then implement that direction. Increased engineering risk alone does not
require new concepts for an otherwise unchanged interface.

Select components using public headers or Python imports, Gallery examples,
and focused tests. The [AI tools index](README.md) provides setup, catalog
queries, and Inspector commands. Application patterns suggest components;
they are not mandatory screen templates.

## Verify the boundary and interface

| Layer | Evidence |
|---|---|
| Preserved interfaces | Existing behavior and regression tests |
| Adapter and view state | Applicable success, failure, cancellation, and teardown tests |
| Build and delivery | Supported build; package smoke when packaging is in scope |
| Responsiveness | Non-blocking work, bounded collections, stable scroll behavior, transient cleanup |
| Visible changes | Real Light/Dark and normal/narrow review; relevant long text, focus, input, and resize states |
| New or redesigned application | Final-build comparison with the selected concept, Inspector, architecture validation, and independent visual review |

Fix findings and review the rebuilt result. Keep engineering results, visual
acceptance, and unverified platform boundaries separate. The
[consumer visual contract](../../.agents/skills/build-fluentqt-gui/references/visual-evidence-contract.md)
defines formal application evidence; FluentQt library changes use
[repository maintainer gates](../../.agents/skills/build-fluentqt-gui/references/fluentqt-maintainer-gates.md).

<!-- docs-nav:bottom:start -->
---
[← FluentQt Onboarding Tools](../../tools/onboarding/README.md) · [Contents](../SUMMARY.md) · [AI-assisted development index](README.md)
<!-- docs-nav:bottom:end -->
