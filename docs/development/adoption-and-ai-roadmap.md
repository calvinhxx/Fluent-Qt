# AI Delivery Roadmap

## Purpose

FluentQt 1.7 closed the main component, accessibility, binding, WebAssembly,
and release gaps. This roadmap now answers only two questions: can a new user
or agent reach a working window quickly, and can an agent deliver a GUI that
meets a verifiable Gallery-level quality bar? The roadmap closes when Phase 2
passes.

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

## Acceptance Measures

The primary measure is whether an agent can complete the fixed benchmark's
build, workflow, and visual acceptance. Stars, page views, and unreviewed
screenshots do not count.

Supporting measures:

| Measure | Closeout target |
|---|---:|
| Median trial time to first window after doctor passes | under 10 minutes |
| Quick Start completion | at least 80% |
| Agent build success on the benchmark set | at least 85% |
| Agent workflow completion | at least 80% |
| Blind visual review | every dimension at least 4/5 |
| Same-package final-build cross-agent pairwise preference | winning result at least 70% |
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
| 1 First success | Diagnose, scaffold, build, and understand a minimal consumer | Complete |
| 2 AI quality loop | Inspect a built application and publish cross-agent judged runs | Complete |

## Lean Execution Queue

Phase 2 closeout is complete:

1. Codex and Cursor completed clean runs with the same package, source, and
   Qt/FluentQt prefix;
2. both terminal records are sealed and pass content-hash validation;
3. all thirteen automatable application scenes are covered, with the native
   IME candidate surface assigned to platform manual compatibility review;
4. five randomized X/Y final-build comparisons completed and the aggregate
   gate passed.

## Phase 0: Baseline and Contract

Delivered:

- the 1.7 component and quality closeout remains the engineering baseline;
- the generated catalog records 69 components, 90 routes, and 205 samples;
- the portable Skill defines Codex and Cursor benchmark runs;
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

Status: Complete. [Full CI #32654501221](https://github.com/calvinhxx/Fluent-Qt/actions/runs/32654501221)
produced passing reports in all five C++ clean-consumer environments across
Linux x64/ARM64, Qt 5/6, macOS, and Windows. Completion was 100% with a median
time to first window of 9.692 seconds. The three Linux, Windows, and macOS
Python wheel lanes also passed, with a 0.546-second median. All eight reports
reached the real window path with no blockers.

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
generated starter, so the evidence does not duplicate the FluentQt build.

Add `fluentqt create` only after the doctor output contract is stable. Generate
two maintained starters rather than a generic empty shell:

- an existing-Qt integration slice;
- a greenfield native workbench with app/application/domain/infrastructure/UI
  boundaries, Light/Dark identity, tests, and CI.

The generated project must build without editing absolute paths. The C++
starter is canonical; the Python starter mirrors its product structure without
copying C++ ownership details mechanically.

### 1C. Searchable public API

Status: Implementation and local acceptance are complete. Public Pages will be
published by the next normal promotion to `main`; it is no longer a gate for
this roadmap. On 2026-08-25 the public Chinese landing page returned
200 while `/Fluent-Qt/api/` still returned 404, confirming that the remaining
blocker is promotion from `release/1.7.x` into the Pages workflow that listens
only to `main`, not generation or site assembly. The generated API Explorer
covers all 69 catalog
components and 114 installed public headers, with search, category filters,
C++ / Python names, declaration links, focused tests, and Gallery routes. The
WebAssembly Gallery deep link compiled and opened the requested DataGrid route
locally. A full run on `release/1.7.x` produces the site artifact, but
production Pages is published only from `main`; local files and CI artifacts
are therefore not reported as a public launch. Generation is checked against
the installed-header allowlist and the existing AI catalog rather than a
second hand-maintained index.

Publish generated API reference for installed public headers and link every
catalog component to the relevant declaration, Gallery route, and focused
test. Keep the Quick Start task-oriented; do not turn the README into the API
manual.

Phase gate: after the doctor prerequisites are available, five clean consumer
environments run `fluentqt trial` for the relevant starter with a median
first-window-path time below ten minutes and at least 80% completion. The
automated smoke reaches the real `show()` path; it is not an aesthetic review.

The remote evidence for 1B exceeds this gate, and 1C generation, entry points,
and deep links pass locally. Phase 1 is therefore complete. Public deployment
is a normal post-merge site check.

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

Status: Complete. A v2 JSON Schema defines fourteen scenes across Gallery and
both Workbench starters. Thirteen automated scenes cover Light/Dark,
wide/narrow/minimum, empty, loading/error, long text, dense data, and scroll
boundaries. Empty, failure, and retry use real controller states in both the
C++ and PySide6 starters rather than schematic placeholders.

The fourteenth scene is a native IME compatibility check. The candidate window
is owned by the operating system and input method, cannot be produced by an
offscreen run, and would require taking the user's foreground input context to
automate. It therefore remains a per-platform manual compatibility check, not
an AI-quality roadmap gate. The benchmark keeps Qt's native editor and input
method event path; no uncaptured candidate surface is reported as passed.

### 2C. Cross-agent benchmark

Status: Complete. The final Codex and Cursor runs used source commit
`b150a551b8d465e31e418e1b2eaf5e79bbb7d28e`, the same pinned Qt/FluentQt
prefix, and Skill package SHA-256
`29384c19ec821ecd37d92d63d0004d4ee5111b8231c0ceb1931bf2ce463de249`.

| Run | Build and workflow | Inspector | Nine-dimension review |
|---|---|---:|---:|
| Codex, user-selected Concept A | 3/3 CTest, strict structure, and v4 evidence pass | 0 | 36/45, every dimension 4/5 |
| Cursor, final clean rerun | 6/6 CTest, strict structure, and v4 evidence pass | 0 | 36/45, every dimension 4/5 |

Both terminal manifests and every referenced artifact are content-hashed by
`benchmark_run.py seal` and pass `--require-current --require-pass`. Five
state-matched final-build pairs were reviewed under randomized X/Y labels.
After reveal, the Codex Concept A build was preferred in 5/5 comparisons, or
100%. This is a relative result for these two same-package final runs, not an
unassisted baseline or an industry benchmark.

The aggregate status is `pass`: build success 100%, workflow completion 100%,
minimum visual dimension 4/5, pairwise preference 100%, and zero open blocker
or major findings. The original 24/45 Cursor result remains historical evidence;
it drove the responsive Workbench shell, wide-layout gate, independent final
review, and sealed-artifact workflow rather than being overwritten.

No external model credential was used. The result validates the real
`QProcess` adapter, protocol mapping, streaming events, cancellation, retry,
and fixture-backed local workflow; it does not claim an external-provider call.

Phase gate: Passed on 2026-08-25.

## Closeout Boundary

This roadmap is closed. Phases 0, 1, and 2 are complete; the former Phases 3
and 4 were removed and are not release gates. The existing
[Production Evidence Baselines](production-evidence.md) and
[Compatibility Policy](compatibility-policy.md) remain maintained references.
Production Pages is a normal post-merge check, while the native IME candidate
surface remains a per-platform manual compatibility check. VoiceOver,
external-adoption counts, more platform pixel lanes, package registries,
Figma, Designer, and MCP become separate projects only when an explicit need
appears.
