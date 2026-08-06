# Language bindings

This directory contains optional language bindings built on top of the reusable
FluentQt C++ library. Binding targets stay disabled by default and do not change
the library's Qt 5.15+ compatibility.

- [`pyside6`](pyside6/) provides the Qt 6.2+ Python binding generated with
  Shiboken6. Its [roadmap](pyside6/ROADMAP.md) tracks component coverage,
  ownership work, native-platform validation, and wheel release readiness;
  a [Simplified Chinese version](pyside6/ROADMAP.zh-CN.md) is also available.
  The reusable `FluentQt` wheel and standalone pure-Python
  [`FluentQt-Gallery`](pyside6/gallery/) wheel are separate distributions.

Each binding owns its native generator input, language package sources,
examples, tests, and packaging metadata.
