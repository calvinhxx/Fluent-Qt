# AI-friendly FluentQt

This directory is the stable entry point for coding agents that need to add a
FluentQt desktop GUI to an existing or new project. The workflow does not assume
that the target already has a CLI or TUI. A library, service, plugin host,
file-processing repository, data tool, or greenfield application can all use
the same discovery and integration contract.

## What AI-friendly means here

FluentQt v1 provides five layers:

1. `llms.txt` gives agents a small, predictable repository entry point.
2. `generated/fluentqt-ai-catalog.json` exposes versioned component, C++,
   Python, sample, test, and integration facts without requiring a full source
   scan.
3. `query_ai_catalog.py` returns only the entries relevant to one intent or
   component, keeping model context focused.
4. `add-gui-to-project.md` defines a project-shape-independent implementation
   workflow, while `project-analysis.schema.json` defines its optional machine
   record.
5. `build-fluentqt-gui` packages that workflow using the open Agent Skills
   format, and CI rejects stale generated facts or broken platform routing.

The catalog is evidence, not a replacement for repository inspection. An agent
must still verify the target project's public interfaces, ownership, event loop,
threading, cancellation, persistence, and test contracts before choosing an
integration pattern.

## Quick start

List application and integration patterns:

```bash
python3 tools/ai/query_ai_catalog.py --pattern service-client
python3 tools/ai/query_ai_catalog.py --pattern greenfield
```

Find controls by user intent, or inspect one exact component:

```bash
python3 tools/ai/query_ai_catalog.py --search "expandable hierarchy"
python3 tools/ai/query_ai_catalog.py --component tree-view
python3 tools/ai/query_ai_catalog.py --guide navigation
```

Use `--json` when a tool needs structured output. Then follow
[Adding a GUI to any project](add-gui-to-project.md) from discovery through
validation.

## Cross-agent Skill

[`../../.agents/skills/build-fluentqt-gui`](../../.agents/skills/build-fluentqt-gui)
is the only source. It follows the
[Agent Skills specification](https://agentskills.io/specification) and contains
its instructions, references, scripts, and catalog snapshot. There is no
second Claude-, Cursor-, or Copilot-specific copy in this repository.

Build the installable archive with:

```bash
python3 tools/ai/package_fluentqt_skill.py \
  --project-root . \
  --output-dir dist
```

The archive has one top-level `build-fluentqt-gui/` directory. Extract that
same directory into the Skills location supported by Codex, Claude Code,
Cursor, GitHub Copilot, or another compatible agent. Agents that discover
`.agents/skills/` can use the repository copy directly. `agents/openai.yaml`
only supplies optional Codex UI metadata. In Codex, mention
`$build-fluentqt-gui`; other agents use their normal Skill invocation.

The bundled `scripts/query_catalog.py` reads
`assets/fluentqt-ai-catalog.json`, so the installed Skill does not need a
FluentQt source checkout or a guessed sibling path. `--project-root` and
`--catalog` provide explicit overrides when working against another checkout.

The Skill has `lite` and `full` routes, but both keep the same visual and
engineering bar. Gallery remains the finish benchmark. Integration records
also expose `window_ownership`; `host-owned` forbids a second application
window, while `caller-decides` requires repository evidence.

Validate the portable structure and deterministic catalog behavior with:

```bash
python3 tools/ai/validate_ai_assets.py --project-root .
python3 tools/ai/evaluate_ai_catalog.py --project-root .
```

For a behavioral smoke test, open a fresh session in each target agent and ask:

```text
Use build-fluentqt-gui to analyze <target-project>. It may have a CLI, TUI,
service, library API, existing GUI, or no interface. Do not edit yet; select an
integration pattern from repository evidence and propose one validated vertical
slice.
```

The result should identify the target's actual project shape, avoid assuming a
TUI or CLI, cite the selected integration boundary, preserve existing entry
points, select public FluentQt APIs, and include build, test, responsiveness,
and visual validation. Compare those fields across agents rather than expecting
identical prose.

## Sources of truth

The generated catalogs derive component identity, route descriptions, C++
samples, Python API names, installed headers, and focused tests from canonical
repository files. Human-authored decision semantics live in `guidance.json`.
Do not hand-edit either `generated/fluentqt-ai-catalog.json` or the Skill copy
under `assets/`; the generator updates and checks both.

In a full checkout, after changing a component, sample, binding, test, or AI
guidance entry, run:

```bash
python3 tools/ai/generate_ai_catalog.py --project-root .
python3 tools/ai/evaluate_ai_catalog.py --project-root .
python3 tools/ai/validate_ai_assets.py --project-root .
```

The catalog uses
[JSON Schema draft 2020-12](fluentqt-ai-catalog.schema.json). Catalog schema v2
adds required integration `window_ownership`; v1 consumers should migrate
before reading newly generated catalogs. The optional project-analysis record
uses [its own schema](project-analysis.schema.json).

## Roadmap

| Milestone | Outcome | v1 status |
| --- | --- | --- |
| M0: discoverability | `llms.txt`, canonical index, supported-version facts | Implemented |
| M1: machine contract | Generated catalog, JSON Schemas, intent query | Implemented |
| M2: general workflow | Evidence-based integration for existing and greenfield projects | Implemented |
| M3: reusable Skill | One self-contained Agent Skill shared by compatible agents | Implemented |
| M4: drift prevention | CI validation, catalog synchronization, and source-package delivery | Implemented |
| M5: measured quality | Eighteen project shapes, sixteen retrieval regressions, and five cross-pattern composition gates | Baseline implemented |
| M6: distribution | Deterministic versioned Skill archive | Implemented |

The Skill remains the distribution unit. A plugin is only warranted later if
FluentQt needs MCP servers, apps, or several related Skills.

The deterministic cases in `evals/scenarios.json` cover projects with no
interface as well as library, CLI, TUI, service, and plugin boundaries. They
measure catalog consistency, retrieval, component-set differentiation, and a
negative guard against generic chat/sidebar shell convergence—not end-to-end
model or visual quality. Live agent tasks and judged implementation outcomes
remain a later evaluation layer.

## Compatibility boundary

- C++ consumers use C++17, Qt Widgets 5.15+ or 6.2+, and
  `FluentQt::FluentQt`.
- Python consumers use Python 3.10+, PySide6/Shiboken6 6.2+, and the `FluentQt`
  package.
- Prefer public installed headers or exported Python modules. Source-private
  classes found during repository scanning are not supported integration APIs.
- Preserve existing CLI, TUI, service, or library entry points unless the user
  explicitly asks to replace them.
