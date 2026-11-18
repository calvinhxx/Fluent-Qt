# Language bindings

Optional language bindings built on top of the FluentQt C++ library. Binding
targets stay disabled by default and do not affect C++ Qt 5.15+ compatibility.

- [`pyside6`](pyside6/) provides the Qt 6.2+ Python binding generated with
  Shiboken6. See its [roadmap](pyside6/ROADMAP.md) for component coverage,
  lifetime handling, platform checks, and wheel status. The reusable `FluentQt`
  wheel and the standalone [`FluentQt-Gallery`](pyside6/gallery/) wheel are
  published separately and kept matched to the same CI release bundle in the
  [publishing runbook](pyside6/PUBLISHING.md).

Each binding keeps its generator config, package sources, examples, tests, and
packaging metadata in its own directory.
