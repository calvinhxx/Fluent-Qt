# FluentQt Onboarding Tools

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../../docs/README.md) › [AI-assisted development](../../docs/ai/README.md) › Workflow

[Contents](../../docs/SUMMARY.md) · [AI-assisted development index](../../docs/ai/README.md) · [Add a FluentQt GUI to a project →](../../docs/ai/add-gui-to-project.md)
<!-- docs-nav:top:end -->

## Environment doctor

`fluentqt_doctor.py` performs a local, read-only preflight before a consumer
project is changed. It makes no network requests. The C++ profile configures a
small Qt Widgets project in a temporary directory and removes that directory
when the probe completes.

From a FluentQt checkout or source package:

```bash
python3 tools/onboarding/fluentqt_doctor.py --profile cpp
python3 tools/onboarding/fluentqt_doctor.py --profile python
```

If Qt is not on CMake's default search path:

```bash
python3 tools/onboarding/fluentqt_doctor.py \
  --profile cpp \
  --cmake-prefix-path /path/to/Qt/6.9.3/platform
```

The C++ profile also accepts `--toolchain-file`. When no prefix is supplied,
the tool checks `CMAKE_PREFIX_PATH`, `Qt6_DIR`/`Qt5_DIR`, and common `qmake` or
`qtpaths` commands before using CMake's default search.

Agents should use the stable JSON form:

```bash
python3 tools/onboarding/fluentqt_doctor.py --profile cpp --format json
```

The report follows [doctor-report.schema.json](doctor-report.schema.json). It
has `schema_version: 1`, a `ready` boolean, pass/warning/failure counts, and
ordered checks with repair hints. Exit code `0` means no blocking finding;
warnings do not block. Exit code `1` means at least one required capability is
missing.

The Python profile treats CPython 3.11–3.13 as published-wheel environments.
Python 3.10 is reported as source-build only.

## Project creator

`fluentqt create` writes a maintained starter into a new directory. It never
overwrites an existing target and never embeds the target's absolute path in
generated files.

```bash
python3 tools/onboarding/fluentqt create my-app \
  --language cpp --starter workbench
```

Choose `existing-qt` for a bounded panel that can be added to an existing
application. Choose `workbench` for an app with domain, application,
infrastructure, UI, tests, and CI boundaries. Both starters are available for
`cpp` and `pyside6`; C++ is the canonical implementation.

```bash
python3 tools/onboarding/fluentqt create my-panel \
  --language cpp --starter existing-qt --dry-run
python3 tools/onboarding/fluentqt create my-python-app \
  --language pyside6 --starter workbench --format json
```

Use `--name`, `--id`, and `--accent #RRGGBB` to set product identity. The JSON
report follows [create-report.schema.json](create-report.schema.json). Every
project includes `.fluentqt/architecture.json`, which can be checked with the
Skill's project-structure validator.

Both Workbench starters expose `--quality-report`. It lays out the generated
window, prints a versioned FluentQt Inspector report, and exits without changing
the interface.

## First-window trial

`fluentqt trial` runs the same `doctor` and `create` commands a new user sees,
then configures, builds, tests, and enters the generated application's real
window show path. It makes no network requests and removes its temporary project
unless `--target` is supplied.

From a checkout, measure the C++ Workbench against that source tree:

```bash
python3 tools/onboarding/fluentqt trial \
  --profile cpp \
  --starter workbench \
  --fluentqt-source . \
  --output first-window.json
```

Against an installed development package, omit `--fluentqt-source` and pass its
prefix with `--cmake-prefix-path` when CMake does not find it automatically.
For an installed Python wheel, use `--profile python`.

The report follows
[first-window-report.schema.json](first-window-report.schema.json). A passing
run proves that the starter reached `window.show()` through its packaged smoke
path; it is not a visual-quality score. Keep reports from clean machines to
measure completion rate and time-to-first-window instead of estimating them.

<!-- docs-nav:bottom:start -->
---
[Contents](../../docs/SUMMARY.md) · [AI-assisted development index](../../docs/ai/README.md) · [Add a FluentQt GUI to a project →](../../docs/ai/add-gui-to-project.md)
<!-- docs-nav:bottom:end -->
