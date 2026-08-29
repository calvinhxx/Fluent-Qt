# Development documentation

> **Status:** Current index

[Documentation home](../README.md) · [Contents](../SUMMARY.md)

This is the entry point for contributors and maintainers. Choose a task below;
do not read the directory as one long manual. Architecture contracts, current
workflows, accepted component decisions, and historical evidence are separated
so a dated roadmap cannot be mistaken for current guidance.

## Find the right document

| Task | Read first | Then verify with |
|---|---|---|
| Choose or close a cross-cutting maintenance phase | [Technical debt roadmap](technical-debt-roadmap.md) | The phase's checked-in exit condition and CI/CTest evidence |
| Add or change a public component | [Component API conventions](component-api-conventions.md) | [Compatibility policy](compatibility-policy.md), [installed-header allowlist](../../cmake/FluentQtInstallHeaders.cmake) |
| Add a visible component | [Accessibility contract](accessibility-contract.md) | [Accessibility inventory](accessibility-inventory.md), [visual review](visual-review.md) |
| Add or edit a Gallery sample | [App sample optimization](app-sample-optimization.md) | [Live Scene and native preview](gallery-preview-workflow.md), [AI-assisted GUI verification](gui-verification-workflow.md) |
| Add a Gallery card image | [Gallery control images](gallery-control-images.md) | qrc registration and Gallery build |
| Build the repository locally | [Build workflow](build-workflow.md) | The selected job count printed by the adaptive wrapper |
| Write or update a test | [Testing workflow](testing-workflow.md) | [Qt component test conventions](qt-component-test-conventions.md) |
| Diagnose behavior | [Logging workflow](logging-workflow.md) | Focused component tests |
| Change Linux or WebAssembly support | [Linux](linux-workflow.md) or [WebAssembly](webassembly-workflow.md) | The matching preset and CI lane |
| Package desktop artifacts | [Packaging workflow](packaging-workflow.md) | Package smoke tests |
| Prepare a release | [Release governance](release-governance.md) | [Release notes](../releases/README.md) and compatibility review |

## Current guides

### API, compatibility, and writing

- [Component API conventions](component-api-conventions.md)
- [Compatibility policy](compatibility-policy.md) · [简体中文](compatibility-policy.zh-CN.md)
- [Source comment style](comment-style.md)
- [Documentation style](documentation-style.md)

### Build, tests, and diagnostics

- [Build workflow](build-workflow.md)
- [Testing workflow](testing-workflow.md)
- [Qt component test conventions](qt-component-test-conventions.md)
- [Logging workflow](logging-workflow.md)
- [Visual review](visual-review.md)
- [App visual geometry verification](app-visual-geometry-verification.md)
- [High-DPI workflow](high-dpi-workflow.md)
- [Linux workflow](linux-workflow.md)
- [WebAssembly workflow](webassembly-workflow.md)

### Gallery and project site

- [App sample optimization](app-sample-optimization.md)
- [Gallery Live Scene and native preview](gallery-preview-workflow.md)
- [AI-assisted GUI verification](gui-verification-workflow.md)
- [Gallery control images](gallery-control-images.md)
- [Tooltip usage](tooltip-usage.md)
- [Project site workflow](site-workflow.md)

### Packaging and release

- [Packaging workflow](packaging-workflow.md)
- [Release governance](release-governance.md)

## Living references

These files are maintained with the implementation. When prose and a generated
inventory disagree, the generated inventory wins.

- [Accessibility contract](accessibility-contract.md)
- [Accessibility inventory](accessibility-inventory.md) ·
  [machine-readable data](accessibility-inventory.json)
- [Visual evidence inventory](visual-evidence-inventory.json) — machine-checked
  high-risk families, exact evidence locators, manual boundaries, and open gaps
- [Technical debt roadmap](technical-debt-roadmap.md) — current cross-cutting
  maintenance phases and exit conditions
- [Component contract baseline](component-contract-baseline.md) — historical
  Phase 0/1 evidence plus a clearly marked current addendum
- [Production evidence baselines](production-evidence.md) — dated measurements,
  not performance promises

## Accepted component contracts

The filenames retain `proposal` for stable external links. Their status blocks
state whether the contract is implemented.

- [Field](field-api-proposal.md)
- [DataGrid](datagrid-api-proposal.md)
- [MultiSelectComboBox](multi-select-combobox-api-proposal.md)
- [EditingCommandRouter](editing-command-router-proposal.md)
- [CommandBar](command-bar-proposal.md)

## Architecture contracts

Use the [architecture index](../architecture/README.md) for runtime ownership,
overlay, typography, window chrome, and Inspector contracts.

## Python bindings

Use the [PySide6 binding index](../../bindings/pyside6/README.md) for package
usage, source builds, compatibility, manylinux, and publishing guidance.

## Historical records

These documents preserve decisions or acceptance evidence from a dated project
state. Follow their links to current contracts; do not copy their counts into
new guidance.

- [FluentQt 1.7 delivery record](release-1.7-roadmap.md)
- [AI delivery record](adoption-and-ai-roadmap.md)
- [WebAssembly delivery record](webassembly-roadmap.md)
- [System capability delivery record](system-capability-roadmap.md)
- [Component API audit](component-api-audit.md)
- [Release notes](../releases/README.md)

## Documentation maintenance

Keep links stable where practical. If a document changes role, update its
status and this index before moving it. Do not duplicate workflow rules in
agent files; link here instead. The former project-specific workflow skills
were removed so these guides remain usable after a repository rename or move.
