# PySide6 API compatibility policy

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../../docs/README.md) › [Python bindings](README.md) › Compatibility and delivery

[Contents](../../docs/SUMMARY.md) · [Python bindings index](README.md) · [PySide6 manylinux build and audit policy →](MANYLINUX.md)
<!-- docs-nav:top:end -->

This policy applies to the public Python surface exported by `fluentqt` and
its documented category modules. The committed [`api-manifest.json`](api-manifest.json)
is the machine-readable public API ledger; generated Shiboken implementation
helpers, private names, and undocumented native mixins are not compatibility
contracts.

## Versions

- The root CMake `project(FluentQt VERSION MAJOR.MINOR.PATCH)` declaration is
  the single version source for the C++ library, Python package, and wheel.
- `fluentqt.__version__` is the full package version and must match both the
  wheel metadata and `binding_build_info()["fluentqt_version"]`.
- `fluentqt.__api_version__` is the `MAJOR.MINOR` public API line recorded as
  `api_version` in `api-manifest.json`.
- A patch release may fix behavior without removing or changing a documented
  Python call contract. A minor release may add backward-compatible API. A
  removal, rename, incompatible signature change, ownership-policy change, or
  packaging-contract break requires a new major version.
- The exact PySide6, Shiboken6, and Qt dependency versions in a wheel are a
  native-runtime compatibility constraint, not a separate FluentQt API
  version.

## Deprecations

1. Add an active entry to `api-manifest.json` before shipping a deprecation.
   Each entry names the public symbol, the first deprecating release, the next
   major release that may remove it, an optional replacement, and a reason.
2. Keep the old symbol behavior-compatible for the remainder of its major
   release line. A Python facade emits `DeprecationWarning` with `stacklevel=2`
   when the deprecated entry is used; native adapters must provide an
   equivalent Python-visible warning before forwarding to the replacement.
3. Document the replacement in the binding guide and release notes. A rename
   keeps the old spelling as a deprecated forwarding alias; it is never a
   silent removal.
4. Remove the symbol only in the declared later major release. Remove its
   active ledger entry in the same change and record the breaking change in
   the curated release notes.

### 1.7 Fluent-only reset

Version 1.7 is an explicitly approved one-time breaking cleanup made before the
binding reached broad adoption. It removes `DesignLanguage`, `StyleTheme`,
`current_design_language()`, `FluentWidget.design_language()`, and
`apply_style_theme()` instead of carrying no-op compatibility shells. Use
Light/Dark `Theme`, Fluent semantic tokens, `set_accent_color()`, and
`apply_user_theme()` instead. These removed symbols are absent from the 1.7 API
manifest rather than listed as active deprecations. Future removals follow the
major-version policy above.

## New C++ surface decisions

Every new installed C++ component or non-trivial public API records one of
these decisions before release:

- **Supported in Python:** the Shiboken export, API manifest, typing/runtime
  checks, ownership tests, and a Python example land in the same release slice
  as the public C++ API.
- **Intentionally C++-only:** the limitation and reason are documented in the
  release roadmap and binding coverage ledger, and generated guidance must not
  advertise a Python import.

Private prototypes may stay C++-only while an API is still changing. They are
not installed, exported from `<FluentQt/FluentQt.h>`, or represented as public
catalog entries. Do not publish a C++ API first and silently treat Python as an
unscheduled follow-up; either finish the selected surface or keep the feature
private/preview.

For item views, Python ownership follows Qt's model/view/delegate contracts.
Bindings must not introduce a Python-owned mirror of the model or a persistent
widget per item to compensate for missing native API.

`DataGrid` is supported in Python in the same slice as its C++ API. The facade
retains Python wrappers for the caller-supplied model, selection model, and
delegate while Qt remains the native object owner; replacement and garbage-
collection tests guard that boundary. Its Python Gallery routes use the same
three read-only, selection/column, and editing/validation scenarios as C++.

## Gates

`tools/verify_api_policy.py` rejects version drift, malformed or duplicate
ledger entries, missing public version variables, deprecations of unknown
symbols, replacements that are not public, future deprecation dates, and
same-major removals. Stub generation verifies the version variables and the
rest of the manifest, while build-tree and clean-wheel tests require runtime,
native, metadata, and typing versions to agree.

<!-- docs-nav:bottom:start -->
---
[Contents](../../docs/SUMMARY.md) · [Python bindings index](README.md) · [PySide6 manylinux build and audit policy →](MANYLINUX.md)
<!-- docs-nav:bottom:end -->
