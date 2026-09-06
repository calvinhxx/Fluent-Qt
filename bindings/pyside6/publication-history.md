# Python publication history

> **Status:** Historical record

<!-- docs-nav:top:start -->
[Documentation](../../docs/README.md) › [Python bindings](README.md) › Historical records

[Contents](../../docs/SUMMARY.md) · [Python bindings index](README.md)
<!-- docs-nav:top:end -->

These records preserve the v1.6.0 and v1.6.1 publication evidence, counts, and
approval process used at the time. They do not define today's release order.
Use the [publishing runbook](PUBLISHING.md) and
[release governance](../../docs/development/release-governance.md) for current
commands and branch synchronization.

## v1.6.0 closure record

M6 closed with this immutable release chain:

- source commit: `e2523ded0d0ae664321b0f2d1d8dd59a1cf0be7c`;
- full CI: [run 31251091780](https://github.com/calvinhxx/Fluent-Qt/actions/runs/31251091780),
  producing 17 Core wheels, one Gallery wheel, and five Linux audit reports;
- release-manifest SHA-256:
  `b015b48abe1a43955530f2e5c6f0046c3c136a78f55694ed0981385155585f94`;
- TestPyPI: [run 31252283807](https://github.com/calvinhxx/Fluent-Qt/actions/runs/31252283807),
  with all 17+1 files hash-verified before tag creation in
  [FluentQt 1.6.0](https://test.pypi.org/project/FluentQt/1.6.0/) and
  [FluentQt-Gallery 1.6.0](https://test.pypi.org/project/FluentQt-Gallery/1.6.0/);
- annotated tag and non-draft GitHub Release:
  [`v1.6.0`](https://github.com/calvinhxx/Fluent-Qt/releases/tag/v1.6.0),
  published by [run 31252452593](https://github.com/calvinhxx/Fluent-Qt/actions/runs/31252452593);
- reviewer-approved PyPI Trusted Publishing:
  [run 31252873846](https://github.com/calvinhxx/Fluent-Qt/actions/runs/31252873846);
- public projects: [FluentQt 1.6.0](https://pypi.org/project/FluentQt/1.6.0/)
  and [FluentQt-Gallery 1.6.0](https://pypi.org/project/FluentQt-Gallery/1.6.0/).

The production workflow verified exact public-index file hashes, all 18
repository-bound attestations, and a clean Linux CPython 3.11 installation of
both distributions. An independent macOS ARM64 CPython 3.11 installation from
public PyPI also passed `pip check`, UILib wheel smoke, and standalone Gallery
wheel smoke. Synchronization to `main` remains a separate, explicit maintainer
action; the release workflow did not perform it.

## v1.6.1 standard publication record

The metadata-corrected standard release repeated the complete publication
contract rather than reusing or replacing the `1.6.0` files:

- source commit: `fd4ce4b4a05671b01fcb3e88da0015c9011f5240`;
- full CI: [run 31269181384](https://github.com/calvinhxx/Fluent-Qt/actions/runs/31269181384),
  with all 43 jobs successful and a new 17+1 wheel bundle;
- release-manifest SHA-256:
  `f766d5214a2073f0f59710e9c306187594060b48bd7540623d548405d1b729de`;
- TestPyPI: [run 31270655830](https://github.com/calvinhxx/Fluent-Qt/actions/runs/31270655830),
  completed on attempt 3 after package-index propagation and verified all 18
  immutable files;
- annotated tag and non-draft GitHub Release:
  [`v1.6.1`](https://github.com/calvinhxx/Fluent-Qt/releases/tag/v1.6.1),
  published by [run 31271042718](https://github.com/calvinhxx/Fluent-Qt/actions/runs/31271042718)
  with nine desktop packages, the source archive, and checksums;
- reviewer-approved PyPI Trusted Publishing:
  [run 31271530901](https://github.com/calvinhxx/Fluent-Qt/actions/runs/31271530901);
- public projects: [FluentQt 1.6.1](https://pypi.org/project/FluentQt/1.6.1/)
  and [FluentQt-Gallery 1.6.1](https://pypi.org/project/FluentQt-Gallery/1.6.1/).

Production verification matched all 18 public hashes, verified all 18
repository-bound attestations, and passed clean Linux CPython 3.11 installation
and wheel smoke. An independent macOS ARM64 installation from public PyPI also
passed `pip check`, UILib smoke, Gallery smoke, and the 67-component/88-route
catalog walk. The tagged commit is synchronized to both `main` and
`release/1.6.x`.

<!-- docs-nav:bottom:start -->
---
[Contents](../../docs/SUMMARY.md) · [Python bindings index](README.md)
<!-- docs-nav:bottom:end -->
