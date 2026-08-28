# Security Policy

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](docs/README.md) › [Community](docs/community/README.md) › Participation and policy

[← Support](SUPPORT.md) · [Contents](docs/SUMMARY.md) · [Community index](docs/community/README.md) · [Contributor Covenant Code of Conduct →](CODE_OF_CONDUCT.md)
<!-- docs-nav:top:end -->

## Supported versions

Security reports are assessed against the current `main` branch and the latest
stable release. Backports to older releases depend on severity,
reproducibility, and maintainer capacity; Fluent-Qt does not currently promise a
fixed support window for older versions.

## Report a vulnerability privately

Do not open a public Issue or Discussion for a suspected vulnerability.

If the repository Security tab offers **Report a vulnerability**, use that
private channel. Otherwise, email the maintainer using the public contact
address on the [GitHub profile](https://github.com/calvinhxx) with the subject
`[Fluent-Qt Security]`.

Include, when possible:

- the affected FluentQt version or commit and affected surface;
- operating system, architecture, Qt/Python version, and install route;
- a minimal reproduction or proof of concept using non-sensitive data;
- the expected impact and any known mitigations;
- whether the issue has been disclosed anywhere else.

Do not send live credentials, customer data, private signing material, or a
production-only exploit. Use synthetic data and the smallest reproduction that
demonstrates the risk.

Fluent-Qt is volunteer-maintained and cannot guarantee a response deadline.
The maintainer will acknowledge and triage reports as capacity allows and will
coordinate remediation and disclosure before public details are published.

## Scope

This policy covers the FluentQt C++ library, official PySide6 packages, Gallery
applications, release artifacts, and repository-owned build or release
automation. Vulnerabilities in Qt, compilers, package registries, or other
third-party dependencies should also be reported to the relevant upstream
project; please still notify Fluent-Qt privately when its integration increases
the impact.

<!-- docs-nav:bottom:start -->
---
[← Support](SUPPORT.md) · [Contents](docs/SUMMARY.md) · [Community index](docs/community/README.md) · [Contributor Covenant Code of Conduct →](CODE_OF_CONDUCT.md)
<!-- docs-nav:bottom:end -->
