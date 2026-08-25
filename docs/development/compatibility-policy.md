# Compatibility Policy

[简体中文](compatibility-policy.zh-CN.md)

This policy applies from FluentQt 1.7 onward. It keeps the public contract
predictable without promising binary compatibility that Qt toolchains cannot
provide.

## Public API

- Patch and minor releases remain source compatible within major version 1.
- New overloads, properties, signals, and components may be added compatibly.
- Removing or changing an established public API requires the next major
  release. The Fluent-only cleanup in 1.7 was the documented final exception
  before this policy.
- Installed headers listed by `cmake/FluentQtInstallHeaders.cmake`, exported
  CMake targets, and documented PySide6 names are public. Private headers,
  Gallery internals, tests, and generated implementation details are not.

## Binary ABI

FluentQt does not promise a stable C++ binary ABI across releases, compilers,
standard libraries, build modes, or Qt versions. Rebuild FluentQt and the
consumer with the same Qt/toolchain when upgrading. Release packages and
PySide6 wheels are matched to their documented platform, architecture, Qt,
Python, and Shiboken runtimes.

## Deprecation

- A planned public removal first gains a compiler/runtime deprecation that
  names the replacement and is documented in release notes.
- Deprecated APIs remain available for at least one minor release and are
  tested until removal in the next major release.
- A security issue, undefined behavior, or an upstream Qt break may require a
  faster change; the release notes must state the reason and migration.

## Supported Qt lines

- Native C++: C++17 with Qt Widgets 5.15+ or Qt 6.2+.
- CI samples Qt 5.15.2 and Qt 6.2 compatibility alongside the current Qt 6
  release line. Platform/architecture package coverage is listed in the
  repository README and release assets.
- PySide6 source compatibility starts at Qt/PySide/Shiboken 6.2.4; published
  wheels use the matched runtimes listed in the
  [binding policy](../../bindings/pyside6/API_COMPATIBILITY.md).
- WebAssembly is a separate, pinned browser toolchain: Qt 6.9.3
  `wasm_singlethread` with Emscripten 3.1.70.

Raising a minimum Qt, C++, CMake, or Python version is an incompatible build
contract change and follows the major-release process unless an upstream
security or distribution constraint makes that impossible.
