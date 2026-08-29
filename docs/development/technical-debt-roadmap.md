# Technical debt roadmap

> **Status:** Living reference

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › API, policy, and writing

[← Component API Conventions](component-api-conventions.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Compatibility Policy →](compatibility-policy.md)
<!-- docs-nav:top:end -->

This is the active queue for cross-cutting maintenance work that is too broad
for one component issue. Historical delivery records explain what shipped;
this document tracks only debt that still has an owner, an exit condition, and
repository evidence.

## Status model

| State | Meaning |
|---|---|
| Active | The phase is being enforced incrementally, but at least one named exit gate is still open |
| Next | Highest-value phase that can start without a new product decision |
| Planned | Scoped work with an exit condition, ordered after the current phase |
| Deferred | Intentionally waits for a compatibility boundary or external capability |
| Complete | Exit condition is enforced by current repository evidence |

A phase is not complete because code was edited once. Its regression boundary
must be represented by a checked-in contract, automated gate, or an explicitly
owned native-platform acceptance record.

## Current phases

| Phase | State | Scope | Exit condition |
|---|---|---|---|
| TD-1 — Living debt ownership | Complete | Replace closed historical roadmaps as the current maintenance queue | This page is linked from the development index and names evidence and exit conditions for every active phase |
| TD-2 — Public component API drift gate | Complete | Turn the component API audit rules into a repository-wide static contract | Installed component headers, catalog mappings, property accessors, notify gaps, and legacy boolean readers are checked in CTest and the reusable C++ CI planning gate |
| TD-3 — High-risk visual regression rollout | Active | Expand deterministic geometry and approved pixel evidence beyond the representative baseline set | The machine-checked inventory covers the canonical catalog, every high-risk state has deterministic or explicitly human-required evidence, and at least one high-risk scenario has a digest-bound independently reviewed GUI bundle |
| TD-4 — Native platform acceptance | Planned | Keep screen reader, IME, window management, drag-and-drop, animation timing, and physical touch claims separate from injected Qt events | Risk-based Windows, macOS, and Linux records name the tested release artifact, OS/runtime, workflow, result, and unresolved platform limitation |
| TD-5 — Major-version compatibility cleanup | Deferred | Retire compatibility aliases and legacy ownership/removal surfaces that remain necessary in 1.x | A 2.0 migration inventory links each removal to its replacement, C++ and Python policy entries, migration guidance, and release tests |

## TD-2 contract

The current gate is
`tools/quality/validate_component_api.py`. Its machine-readable policy is
`component-api-policy.json` beside this document.

The validator treats the installed-header allowlist and generated API catalog
as canonical inputs. It rejects:

- catalog components whose declaration or umbrella header is not installed;
- missing component declarations or focused test sources;
- malformed `Q_PROPERTY` declarations or accessors that are not declared;
- a new writable property without `NOTIFY` unless the exact existing property
  is classified in the policy; and
- a new noun-style boolean reader unless the exact compatibility surface is
  classified in the policy.

Policy entries are a freeze list, not approval for more exceptions. Removing a
legacy exception is progress; adding one requires an API review and a concrete
compatibility reason.

## TD-3 execution order

1. Add a machine-readable visual-evidence inventory keyed by the canonical
   component catalog and state/risk identifiers.
2. Cover overlay placement and dismissal, menu/flyout layering, collection
   selection and scrolling, window chrome/material, and responsive navigation
   before low-risk static controls.
3. Prefer named geometry and scripted interaction assertions for behavior.
   Add approved pixel regions only where raster output is the contract.
4. Keep host fingerprints and independent approval mandatory. A missing
   baseline remains `human-required`; it must not be silently regenerated.

The current enforcement files are
`visual-evidence-inventory.json` and
`tools/quality/validate_visual_evidence_inventory.py`. The inventory derives
high-risk families from the canonical component catalog, tracks the remaining
catalog complement, verifies exact focused test cases, and rejects artificial
native-platform claims from injected Qt events. Manual surfaces stay
`manual-required`; their presence is not a review result.

Each automated record declares `execution: ci` or
`execution: registered-only`. The validator derives that value from the CMake
fast/full target lists and the independent contract lane, so a locally
registered test cannot be presented as continuously executed evidence. It also
requires every cited automated or manual test source to belong to a test target
and rejects multiline `TEST(...)` declarations that the current
`gtest_add_tests` scanner cannot register. The C++ matrix validator locks one
real build-and-test lane for each `ci_fast`, `ci_full`, and `contract` label,
including the target, label selector, and interactive/local exclusion policy.
The workflow-boundary validator separately locks the Test step to
`matrix.test`, `ctest`, the matrix include/exclude labels, and
`--no-tests=error`. It also binds pagination and the trusted event file count
to the one active pull-request classifier command, rejecting comment, echo,
and no-op decoys. The plan and final gate jobs cannot continue on error, and
the final gate rejects missing or non-boolean classification outputs before it
interprets skipped module results. Pull-request classification consumes
current and previous rename paths, checks the API file count against the event
total, and enables all build matrices when GitHub's 3000-file response cap or
another mismatch makes documentation-only classification incomplete. Overlay
state is split into `open` and `placement`; an open/close assertion cannot
claim placement unless the cited test also checks geometry.

The validator also distinguishes the legacy representative root PNGs from
future approved GUI bundles. The root images are approval-host-only evidence
without digest-bound independent metadata. Automated CTest runs skip
interactive cases, so a green CTest result is not visual approval. Python
Gallery and WebAssembly checks are authoring or delivery smoke lanes, not C++
desktop pixel certification. New bundle approval also requires the capture's
recorded kernel and CPU architecture to match the platform/architecture route
selected by the Python verification host.

TD-3 remains Active because the inventory's machine-owned `open_gaps` still
includes registered-only automated evidence, the first approved high-risk
bundle, source-to-binary provenance, runtime-specific baseline routing, and
externally authenticated reviewer identity. Run the full static contract and
its adversarial fixtures with:

```bash
python3 tools/quality/validate_visual_evidence_inventory.py \
  --project-root . --self-test
```

## TD-4 acceptance boundary

Automated Qt events remain the default regression lane. Native platform checks
are required only when a change can affect an operating-system-owned surface or
service. VoiceOver is the macOS screen-reader lane; equivalent Windows and
Linux checks must name the actual accessibility backend rather than claiming
coverage from `QAccessibleInterface` tests alone.

A desktop QPA plugin is only a deterministic capture prerequisite. In
particular, Xvfb plus `xcb` is not native acceptance for physical input,
compositor material, accessibility services, IME, or window management.

## Update rule

Update this page in the same change that moves a phase. Record the enforcing
file or command, not a duplicated test count. New component features still
belong in accepted API contracts or release notes; this roadmap is for
cross-cutting maintenance debt only.

<!-- docs-nav:bottom:start -->
---
[← Component API Conventions](component-api-conventions.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Compatibility Policy →](compatibility-policy.md)
<!-- docs-nav:bottom:end -->
