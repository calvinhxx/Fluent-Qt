# Project architecture

Use this contract for a new FluentQt application, a full-profile GUI, or an
existing GUI whose shell owns unrelated workflow, process, settings, theme, and
presentation responsibilities. It applies to both C++ and PySide6. Adapt names
to the product vocabulary; preserve the dependency direction and ownership
boundaries.

## Select the smallest honest structure

Use the lite template only for a bounded utility with one finite surface and no
background process, network boundary, growing collection, persistence, or
transient lifetime. Use full for every other application.

Initialize the layout before production UI code:

```bash
python3 <skill-root>/scripts/init_project_structure.py \
  --project-root /path/to/app \
  --application "Product name" \
  --language cpp \
  --profile full \
  --source-root src \
  --tests-root tests
```

For an existing application, create the manifest and directories first, map
current files to their real roles, then move them in compilable slices. Do not
generate parallel placeholder classes or rename public integration contracts
merely to match the example names.

## Full C++ template

```text
src/
├── app/                 main, application bootstrap, dependency composition
├── domain/              domain values and policies; no Widgets or FluentQt
├── application/         use cases, controllers, immutable view state
├── infrastructure/      process, network, storage, settings implementations
└── ui/
    ├── shell/           top-level Window; composition and window events only
    ├── pages/           workflow-level views
    ├── components/      reusable application-specific visual pieces
    ├── models/          Qt item models and UI projection models
    ├── delegates/       item delegates and bounded custom painting
    └── theme/           semantic brand tokens and raw-widget bridges
tests/
├── domain/
├── application/
├── infrastructure/
└── ui/
```

Recommended target direction:

```text
app executable
  -> ui
  -> infrastructure
  -> application
  -> domain

ui -> application -> domain
infrastructure -> application/domain
domain -> standard library or QtCore value types only
```

Prefer separate CMake targets when the application is more than a small
utility:

- `<app>_domain`: no Qt Widgets or FluentQt;
- `<app>_application`: state transitions and use cases;
- `<app>_infrastructure`: concrete process, network, storage, and settings;
- `<app>_ui`: FluentQt views, models, delegates, and theme;
- `<app>`: `main` plus dependency assembly only.

Using one target temporarily is acceptable during migration, but directory and
include dependencies must still point inward. Do not expose UI headers through
the domain or application target.

## Full PySide6 template

Use the same roles as packages rather than flattening the application into a
single `main.py`:

```text
src/
├── app/
├── domain/
├── application/
├── infrastructure/
└── ui/
    ├── shell/
    ├── pages/
    ├── components/
    ├── models/
    ├── delegates/
    └── theme/
tests/
    └── <mirrored layers>
```

The domain and application packages must not import `QtWidgets` or FluentQt.
Use signals or explicit ports at the UI boundary; do not pass widgets into use
cases or infrastructure services.

## Responsibility contracts

### Shell

The top-level `Window` owns only:

- top-level chrome and material;
- construction of pages and persistent panes;
- window resize, close, activation, and platform events;
- wiring high-level actions to an application controller;
- switching responsive compositions.

It must not parse protocol events, launch child processes, validate settings,
construct demo transcripts, retain growing data, or paint collection rows.

### Application controller and view state

Put workflow transitions in an application controller or use case. Publish a
small view state whose fields describe what the UI needs, not pointers to
widgets. The controller decides start, stop, retry, selection, cancellation,
and stale-result policy. Views render state and emit intent.

### Infrastructure adapters

Process, network, filesystem, secure storage, and persistence live behind an
application-facing port. The adapter owns protocol framing and resource
lifetime. UI code may receive state or events but must not construct or start
the process directly.

### Views, models, and delegates

Split a view when it has an independent visual role, lifetime, responsive
behavior, or test surface. Use models and delegates for repeated items. A
private helper widget that only normalizes one layout detail can remain beside
its owning view; do not create a file for every five-line helper.

Theme modules own semantic colors, spacing, radius, typography, icon policy,
and raw-widget style bridges. Views consume semantic values and do not grow
their own literal palette.

## Source budgets and review triggers

The bundled defaults are review triggers, not permission to split a coherent
class mechanically:

- C++ shell implementation: 500 lines;
- PySide6 shell module: 400 lines;
- other C++ source file: 800 lines;
- other Python module: 650 lines;
- shell-owned fields: 48 for C++, 40 for PySide6.

Exceeding a budget requires extracting a real responsibility, not creating
`MainWindowPart2.cpp` around the same God object. A shell split across partial
implementation files still fails when it owns runtime, settings, domain, and
demo state.

Treat any of these as a mandatory architecture correction:

- UI constructs a `QProcess`, network client, database, or persistence engine;
- one window owns settings validation, protocol parsing, demo generation, and
  responsive painting together;
- domain/application imports `QtWidgets` or FluentQt;
- repeated rows are child widgets rather than a model/delegate;
- tests cannot instantiate the controller or adapter without a top-level
  window;
- a feature requires adding another unrelated pointer or flag to the shell.

## Architecture manifest

`.fluentqt/architecture.json` records the selected template, source and test
roots, layer paths, shell files, dependency rules, and budgets. After adding
the real shell, list its implementation and header relative to `source_root`:

```json
{
  "shell_files": [
    "ui/shell/MainWindow.cpp",
    "ui/shell/MainWindow.h"
  ]
}
```

Use `allowed_source_root_files` only for compatibility files that genuinely
cannot move. Do not use it to bless a flat tree.

Validate before visual acceptance:

```bash
python3 <skill-root>/scripts/validate_project_structure.py \
  --project-root /path/to/app --strict
```

The validator checks the declared layers, flat-root leakage, source and shell
budgets, shell member count, UI imports in domain/application, process
ownership in UI, and mirrored full-profile test directories. It is a coarse
gate; code review must still check dependency direction, ownership, naming,
cohesion, and behavior tests.

## Refactor an existing God window safely

Keep the final executable behavior stable and move one dependency seam at a
time:

1. record the current build, tests, screenshots, and public/runtime boundary;
2. move pure values, models, delegates, theme, and adapters without changing
   behavior;
3. extract independently testable panels and publish intent signals;
4. move workflow transitions to an application controller and expose view
   state;
5. reduce the shell to composition, window events, and responsive placement;
6. run the structure validator, build, focused tests, and the same visual
   evidence after every compilable slice.

Do not combine an architecture migration with a new visual direction unless
the user explicitly requests both. Preserve existing interfaces and state
semantics while restructuring.
