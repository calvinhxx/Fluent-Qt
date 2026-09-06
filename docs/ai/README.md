# AI-assisted GUI development

> **Status:** Current guide

[Documentation home](../README.md) · [Contents](../SUMMARY.md)

Use the [portable FluentQt Skill](../../.agents/skills/build-fluentqt-gui/SKILL.md)
to create, integrate, redesign, or repair a C++ or PySide6 interface. State the
application outcome; the Skill routes to the relevant implementation and
validation workflow.

## Choose an entry point

| Task | Start here |
|---|---|
| Build a new app or add a GUI to an existing project | [Integration guide](add-gui-to-project.md), then the Skill's new-interface workflow |
| Fix an existing application | The Skill's existing-interface workflow and the owning code/tests |
| Maintain FluentQt components, Gallery, or bindings | Repository `AGENTS.md` and [maintainer gates](../../.agents/skills/build-fluentqt-gui/references/fluentqt-maintainer-gates.md) |
| Edit docs, catalogs, or Skill guidance | [Documentation style](../development/documentation-style.md) and asset checks below |

New interfaces and major redesigns need a human design selection. A focused
fix preserves the accepted design and verifies the affected behavior; async
or collection work adds engineering checks without reopening visual direction.

## Set up a consumer project

For a new environment, run the read-only preflight from a FluentQt checkout or
packaged Skill:

```bash
python3 tools/onboarding/fluentqt doctor --profile cpp
python3 tools/onboarding/fluentqt create my-app --language cpp --starter workbench
```

Use `--profile python` and `--language pyside6` for PySide6. The `workbench`
starter owns its application window; `existing-qt` returns a panel in a
host-owned Qt application. See the [onboarding reference](../../tools/onboarding/README.md)
for toolchain discovery, dry runs, JSON reports, and first-window trials.
Reuse a working environment for ordinary changes.

## Find a component

The [API Explorer](https://calvinhxx.github.io/Fluent-Qt/api/) is the browsable
reference. Catalog queries return a small structured result:

```bash
python3 tools/ai/query_ai_catalog.py --search "expandable hierarchy"
python3 tools/ai/query_ai_catalog.py --component tree-view
python3 tools/ai/query_ai_catalog.py --pattern direct-library --json
```

Confirm candidates against their public headers or Python imports, Gallery
examples, and tests. A catalog result does not establish the target project's
ownership, event loop, cancellation, or persistence behavior.

## Inspect the result

Generated Workbench applications expose Inspector through `--quality-report`.
Other C++ applications include `<FluentQt/Diagnostics.h>` and call
`fluent::diagnostics::Inspector::report(rootWidget)`; Python applications use
`fluentqt.inspect_widget(root_widget)`.

Inspector is read-only. Its [contract](../architecture/inspector-report.md)
defines the layout, text, input, scrolling, and accessibility rules. Findings
need review; a zero-finding report does not replace inspection of the real UI.
The Skill owns consumer acceptance; the
[repository verifier](../development/gui-verification-workflow.md) owns named
FluentQt component and Gallery scenarios.

## Package and maintain the Skill

Agents that discover `.agents/skills/` can use the repository copy directly.
Other compatible agents can install the same archive, including its catalog,
onboarding tools, starters, references, and validators:

```bash
python3 tools/ai/package_fluentqt_skill.py --project-root . --output-dir dist
```

Keep one canonical Skill rather than per-agent copies. Package checks verify
portability and asset contracts; cross-agent quality claims require the
[benchmark](../../.agents/skills/build-fluentqt-gui/references/cross-agent-benchmark.md).

| Information | Source of truth |
|---|---|
| Components, routes, samples, names, and tests | [Generated AI catalog](generated/fluentqt-ai-catalog.json) |
| Selection and integration guidance | [guidance.json](guidance.json) |
| Installed public API | [API catalog](../../site/api/catalog.json) |
| Built-application review scenes | [Application scene manifest](evals/application-scenes.json) |
| Agent workflow and consumer acceptance | [build-fluentqt-gui](../../.agents/skills/build-fluentqt-gui/SKILL.md) |
| Optional project analysis exchange | [Project analysis schema](project-analysis.schema.json) |

Do not hand-edit generated catalogs or the Skill snapshot. Regenerate after
component, sample, binding, test, or guidance changes; validate after Skill or
AI documentation changes:

```bash
python3 tools/ai/generate_ai_catalog.py --project-root .
python3 tools/ai/evaluate_ai_catalog.py --project-root .
python3 tools/ai/validate_ai_assets.py --project-root .
```

The [AI delivery record](../development/adoption-and-ai-roadmap.md) preserves
historical milestones and benchmark evidence.

## Compatibility and safety

C++ consumers use C++17, Qt Widgets 5.15+ or 6.2+, and `FluentQt::FluentQt`.
Python consumers use Python 3.10+, PySide6/Shiboken6 6.2+, and the `FluentQt`
package; published binaries follow the [wheel matrix](../../bindings/pyside6/wheel-matrix.json).
Use exported APIs and preserve existing entry points unless replacement is
requested. Doctor and Inspector run locally and read-only; project source,
screenshots, and usage data are not collected automatically.
