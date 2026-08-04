# PySide6 API compatibility policy

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

The repository currently has no deprecated PySide6 symbols. The empty
`deprecations` ledger is intentional and is validated so the first future
deprecation must adopt this contract.

## Gates

`tools/verify_api_policy.py` rejects version drift, malformed or duplicate
ledger entries, missing public version variables, deprecations of unknown
symbols, replacements that are not public, future deprecation dates, and
same-major removals. Stub generation verifies the version variables and the
rest of the manifest, while build-tree and clean-wheel tests require runtime,
native, metadata, and typing versions to agree.
