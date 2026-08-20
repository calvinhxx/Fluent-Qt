# Contributing to FluentQt

Bug reports, focused fixes, documentation improvements, and component proposals
are welcome. Small, reproducible changes are easier to review and validate
across C++, PySide6, and WebAssembly.

By participating, you agree to follow the [Code of Conduct](CODE_OF_CONDUCT.md).
For installation and usage help, follow the routing in [SUPPORT.md](SUPPORT.md)
instead of opening a support Issue.

## Before opening an issue

- Search existing issues and confirm the behavior on a supported Qt version.
- Include the FluentQt version or commit, affected surface (C++, PySide6,
  WebAssembly, packaging, or docs), Qt/Python version, OS, architecture, and
  install route.
- Reduce bugs to the smallest reproducible project or Gallery route. Attach
  logs and screenshots when they clarify behavior; remove credentials,
  customer data, and private paths first.
- Describe the user outcome for feature requests. A component name without a
  concrete workflow is not enough to define a reusable API.

Use the repository's structured bug or feature form when it fits. Blank issues
remain available for focused documentation, packaging, and design discussions.

## Making a change

1. Read the [development workflow index](docs/development/README.md) and the
   closest architecture or component contract.
2. Keep one pull request focused. Preserve source compatibility unless a
   dedicated breaking migration has been accepted.
3. Define public state, ownership, signals, no-op behavior, accessibility, and
   tests before adding or renaming a public API.
4. Keep collection data, models, delegates, navigation, and business copy
   application-owned. Do not add a persistent widget per item to style a view.
5. Record whether a new public C++ surface is supported by PySide6 in the same
   release or intentionally C++-only. Do not leave accidental binding gaps.
6. When a visible component changes, update its source-aligned Gallery example,
   generated catalogs through their generators, and focused visual evidence.

Use Angular-style Conventional Commit subjects such as `feat(collections): ...`
or `fix(windowing): ...`; see [release governance](docs/development/release-governance.md).

## Validate the smallest relevant surface

Configure with a supported preset, then build in parallel. On macOS arm64:

```bash
cmake --preset vcpkg-osx
cmake --build --preset vcpkg-osx --target test_NAME --parallel
ctest --preset vcpkg-osx -L '^test_NAME$' --output-on-failure
```

Use the equivalent Windows or Linux preset from the README. Automated tests
skip interactive VisualCheck cases; follow the
[testing and visual review workflow](docs/development/testing-workflow.md) for
manual or snapshot runs. Changes that affect bindings or browser builds should
also follow the [PySide6](bindings/pyside6/README.md) or
[WebAssembly](docs/development/webassembly-workflow.md) workflow.

Before requesting review, run `git diff --check`, describe what you tested, and
call out any platform or surface you could not verify. Full cross-platform CI
is expected on the pull request; contributors do not need every toolchain on
one machine.
