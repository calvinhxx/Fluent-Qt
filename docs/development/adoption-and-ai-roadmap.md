# Adoption and AI Delivery Roadmap

English | [简体中文](adoption-and-ai-roadmap.zh-CN.md)

## Purpose

FluentQt 1.7 closed the main component, accessibility, binding, WebAssembly,
and release gaps. The next constraint is adoption: a new user or agent must be
able to choose the library, reach a working window quickly, and keep the
result at Gallery-level quality.

This roadmap treats AI support as a delivery loop rather than a documentation
feature. Phases describe evidence levels, not a serial feature checklist. Work
follows the lean queue below.

## Product Boundary

FluentQt remains an MIT, native C++ Fluent component system for Qt Widgets,
with optional PySide6 and WebAssembly delivery. The primary scenarios are:

1. modernizing an existing Qt Widgets application;
2. adding a native desktop client to a CLI, TUI, service, data tool, or agent;
3. using PySide6 while retaining the same native component implementation.

This roadmap does not add another design language, a QML or mobile renderer,
application business logic, or a hosted source-code collection service.
Collection of project source, screenshots, or usage data remains opt-in.

## North-star and Supporting Measures

The north-star measure is **verified external repositories that reach a
working FluentQt window each month**. Stars and page views are discovery
signals, not successful adoption.

Supporting measures:

| Measure | Target before broad promotion |
|---|---:|
| Median trial time to first window after doctor passes | under 10 minutes |
| Quick Start completion | at least 80% |
| Agent build success on the benchmark set | at least 85% |
| Agent workflow completion | at least 80% |
| Blind visual review | every dimension at least 4/5 |
| Pairwise preference over an unassisted baseline | at least 70% |
| Independent successful integrations | at least 3 |
| Open blocker or major findings in accepted benchmark runs | 0 |

These are FluentQt operating gates, not industry benchmarks. A run counts only
when its repository, command, result, and review evidence are recorded.
The trial clock starts after Qt and FluentQt are available and the doctor can
pass; package acquisition and installation are tracked separately as
distribution evidence.

## Status

| Phase | Outcome | Status |
|---|---|---|
| 0 Baseline and contract | One living roadmap, measures, evidence rules, and non-goals | Complete |
| 1 First success | Diagnose, scaffold, build, and understand a minimal consumer | In progress |
| 2 AI quality loop | Inspect a built application and publish cross-agent judged runs | In progress |
| 3 Production evidence | Publish performance, accessibility, visual, and distribution evidence | Planned |
| 4 External adoption | Convert real integrations into cases, feedback, and contributors | Planned |

## Lean Execution Queue

Only three P0 items are active, with a work-in-progress limit of one engineering
change and one live acceptance effort:

1. run the next full CI and evaluate the `fluentqt trial` reports from its five
   selected C++ clean-consumer environments against the ten-minute/80% gate;
   Python wheel jobs also publish Linux, Windows, and macOS reports;
2. run the existing `agent-run-workspace` task once each in Codex, Claude Code,
   and Cursor; change the Skill from evidence before specifying another task;
3. help three external projects integrate and turn repeated failures into a
   doctor, starter, catalog, or component fix.

P1 contains only production evidence triggered by real integrations: startup
and memory, a large DataGrid model, WebAssembly first load, macOS VoiceOver, and
a concise ABI/Qt support policy. Four more agent tasks, advanced Inspector
alignment/wheel probes, NVDA/Orca, multi-platform pixel lanes, multiple package
registries, and the exploration queue are evidence-triggered later work. This
avoids building an expensive evaluation system before there are user samples.

## Phase 0: Baseline and Contract

Delivered:

- the 1.7 component and quality closeout remains the engineering baseline;
- the generated catalog records 69 components, 90 routes, and 205 samples;
- the portable Skill defines Codex, Claude Code, and Cursor benchmark runs;
- release, PyPI, TestPyPI, Gallery, and site events are evidence sources;
- this document defines the adoption gate and privacy boundary.

Phase gate: the roadmap and measures are reviewable without presenting a
repository inventory as proof of end-to-end AI quality.

## Phase 1: First Success

### 1A. Environment doctor

Status: Complete. The C++ and Python profiles, versioned JSON Schema, source
archive coverage, development-package install, unit tests, and native CI
preflight are in place.

Deliver a standard-library-only, read-only preflight tool for C++ and Python.
It must provide concise human output, stable JSON output for agents, actionable
repair hints, and a non-zero exit status only for blocking findings.

Acceptance:

- C++ checks CMake 3.16+, a compiler, and the same Qt Widgets discovery path
  used by a consumer CMake project;
- Python distinguishes published-wheel support from source-only versions and
  verifies PySide6 plus FluentQt imports;
- the tool performs no network requests and writes only to a temporary probe
  directory;
- unit tests cover ready and blocked paths without requiring Qt;
- the source archive contains the tool.

### 1B. Project creation

Status: Complete locally; cross-platform evidence collection is wired and the
next push will confirm it.
The portable command, four maintained C++ / PySide6 starters, architecture
manifests, versioned report contract, source/development-package delivery, and
CI acceptance jobs are implemented. Both C++ starters build against an
installed FluentQt package and pass application plus offscreen UI smoke tests;
the Python starters compile and execute their application tests. The standalone
Skill archive also carries `doctor`, `create`, and all four starters.
`fluentqt trial` now combines preflight, creation, build, tests, and the real
window show path in one versioned report. Fast CI selects three representative
C++ lanes; full CI selects exactly five across Linux x64/ARM64, Qt 5/6, macOS,
and Windows. The Linux, Windows, and macOS wheel jobs publish the same Python
report. These jobs install existing build outputs and compile only the small
generated starter, so the evidence does not duplicate the FluentQt build. The
remaining gap is a passing remote run and its measured reports, not another
scaffolding feature.

Add `fluentqt create` only after the doctor output contract is stable. Generate
two maintained starters rather than a generic empty shell:

- an existing-Qt integration slice;
- a greenfield native workbench with app/application/domain/infrastructure/UI
  boundaries, Light/Dark identity, tests, and CI.

The generated project must build without editing absolute paths. The C++
starter is canonical; the Python starter mirrors its product structure without
copying C++ ownership details mechanically.

### 1C. Searchable public API

Status: Local acceptance complete; Pages publication remains for the next
remote run. The generated API Explorer covers all 69 catalog components and
114 installed public headers, with search, category filters, C++ / Python
names, declaration links, focused tests, and Gallery routes. The WebAssembly
Gallery deep link compiled and opened the requested DataGrid route locally.
Generation is checked against the installed-header allowlist and the existing
AI catalog rather than a second hand-maintained index.

Publish generated API reference for installed public headers and link every
catalog component to the relevant declaration, Gallery route, and focused
test. Keep the Quick Start task-oriented; do not turn the README into the API
manual.

Phase gate: after the doctor prerequisites are available, five clean consumer
environments run `fluentqt trial` for the relevant starter with a median
first-window-path time below ten minutes and at least 80% completion. The
automated smoke reaches the real `show()` path; it is not an aesthetic review.

## Phase 2: AI Quality Loop

### 2A. FluentQt Inspector

Status: Public v1 complete locally. The installed C++ API and
`fluentqt.inspect_widget(...)` now share one read-only native implementation
and versioned JSON report. Generated C++ and PySide6 Workbench projects expose
the same report through `--quality-report`; both freshly generated command-line
paths now pass real runtime tests. Rules cover clipped text, accessibility names, desktop hit areas, focus
reachability, duplicate semantic actions, nested scroll boundaries, and an
opt-in 4 px layout-grid check. Gallery scene review removed false positives for
structural scrollers, selectable static text, focus proxies, and components
with an explicit scroll-chaining contract. Seven Gallery scenes and three scenes
from each generated C++ and PySide6 Workbench now pass with zero findings.
Internal finding objects remain private so rule evolution does not expand the
public compatibility surface. Application evidence no longer depends on
Gallery alone. Baseline/optical-alignment and runtime wheel-boundary probes
remain deferred until they have low-noise component semantics.

Extend the existing debug-overlay foundation into an opt-in application
inspector. Its first contract should report machine-readable evidence for:

- clipped or elided text without an accessible full value;
- focus order, accessible names, and minimum hit areas;
- 4 px spacing and baseline/alignment outliers;
- duplicate visible entries bound to the same semantic action;
- scroll ownership and nested-wheel boundary behavior.

The Inspector reports evidence; it does not silently rewrite application UI.

### 2B. Application scenes

Status: Automated coverage complete; manual IME acceptance remains. A v2 JSON
Schema defines fourteen scenes across Gallery and both Workbench starters:
thirteen automated Light/Dark, wide/narrow/minimum, empty, loading/error,
long-text, dense-data, and scroll-boundary checks plus one manual IME review.
Both starters expose an application-owned empty workspace with a real retry
path, so empty coverage is no longer deferred.

Define reusable scene manifests for Light, Dark, narrow, minimum, empty,
loading, error, long text, IME, dense data, and scroll boundaries. Run them on
the built application, not on schematic mockups.

### 2C. Cross-agent benchmark

Status: In progress. The first `agent-run-workspace` task now has a fixed prompt,
a portable run-record Schema, and one tool that initializes, validates, and
summarizes Codex, Claude Code, and Cursor runs using the same Skill hash. It
records commits, commands, artifacts, Inspector and visual evidence, blockers,
and independent nine-dimension review. A summary without human pairwise results
remains `awaiting-preference`. Live runs and blind preference review remain
open. The other four repository tasks are no longer a current gate; add a
second task only if the first exposes a materially different integration shape.

Run the same repository task through Codex, Claude Code, and Cursor using the
same versioned Skill. Publish prompts, commits, commands, screenshots,
failures, nine-dimension blind scores, and pairwise preference. Deterministic
catalog tests remain retrieval checks and must not be reported as model or
visual quality.

Phase gate: the build, workflow, visual, preference, and blocker thresholds in
this document pass. Use the run traces to remove or defer Skill instructions
that do not affect results.

## Phase 3: Production Evidence

P1 deliverables:

- repeatable startup, memory, DataGrid large-model, and WebAssembly first-load
  measurements;
- macOS VoiceOver first; add NVDA and Orca when Windows/Linux have a real user
  or maintainer for those runs;
- a concise ABI, deprecation, and supported-Qt policy;
- add perceptual visual lanes, package-size/scrolling studies, or another
  package registry only after a real regression or integration demands it;
  aesthetic changes still require human approval.

Phase gate: results are versioned, reproducible, and linked from releases.

## Phase 4: External Adoption

Work from evidence outward. Start the first three external integrations now;
do not wait for every Phase 3 item:

1. support three independent integrations and record the failed steps;
2. convert recurring failures into doctor, starter, catalog, or component
   fixes;
3. publish migration, large-data, and cross-agent benchmark case studies;
4. add a verified “Built with FluentQt” surface;
5. use scoped issues and component ownership to grow beyond one maintainer;
6. expand promotion only after the Quick Start and integration gates pass.

Phase gate: at least ten external repositories have reached a working window,
three integrations are independently verified, and at least three people have
made useful code or documentation contributions.

## Exploration Queue

These items require evidence from Phases 1–4 before they become commitments:

- Qt Designer custom-widget integration;
- Figma variables/components and semantic-token export;
- docking-workbench recipes for established Qt docking libraries;
- an optional MCP surface for live catalog queries, Gallery launch, builds,
  diagnostics, and screenshot review.

Static guidance, templates, and reference assets stay in the portable Skill.
An MCP or plugin is justified only for live state or executable operations.
