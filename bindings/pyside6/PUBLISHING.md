# Python Publishing Runbook

This document is the release contract for the `FluentQt` and
`FluentQt-Gallery` Python distributions. It complements the general
[release governance](../../docs/development/release-governance.md), the
[wheel matrix](wheel-matrix.json), and the [manylinux policy](MANYLINUX.md).

M6 is not complete merely because all wheel lanes compile. Completion requires
one immutable CI bundle to pass TestPyPI, stable-tag, PyPI, attestation, and
clean public-install verification without rebuilding any wheel.

## Immutable release bundle

A successful full CI run with `python_release_bundle=true` creates one artifact
named `fluentqt-python-release-bundle`:

```text
python-release-bundle/
├── dist/                         # 17 FluentQt + 1 FluentQt-Gallery wheel
├── audits/                       # five manylinux audit reports
├── PYTHON_SHA256SUMS.txt
└── python-release-manifest.json
```

`.github/scripts/assemble-pyside-release-bundle.py` rejects compatibility-only
CPython 3.10 wheels, raw `linux_*` wheels, missing or extra matrix entries,
wrong package metadata, missing PyPI Markdown descriptions or project links,
mismatched manylinux evidence, and non-identical Gallery wheels. The 17 build
lanes must produce a byte-identical Gallery wheel; the bundle retains exactly
one copy.

The manifest records the project version, source commit, originating full-CI
run and attempt, every wheel hash, and every audit hash. TestPyPI and PyPI must
receive the files from this artifact. Never rebuild a release wheel after the
TestPyPI rehearsal.

## One-time Trusted Publishing setup

The top-level [Python Release workflow](../../.github/workflows/python-release.yml)
must exist on the default branch before GitHub can dispatch it for a release
branch or tag. It is the workflow identity registered with the package indexes;
the reusable `ci-python.yml` workflow is not a publisher.

Create these GitHub deployment environments:

| Environment | Deployment branch/tag policy | Approval |
|---|---|---|
| `testpypi` | Selected branches matching `release/*` | Optional for the rehearsal |
| `testpypi-gallery` | Selected branches matching `release/*` | Optional for the rehearsal |
| `pypi` | Protected tags matching `v*` | Explicit production workflow dispatch |
| `pypi-gallery` | Protected tags matching `v*` | Explicit production workflow dispatch |

For a single-maintainer repository, do not configure required reviewers on the
two production environments. The explicit `stage=pypi` workflow dispatch is
the production approval, while the environments still enforce the `v*` tag
boundary and package-scoped Trusted Publisher identities. Disable
administrator bypass on all four environments so that branch and tag rules
cannot be skipped. If release ownership later expands to multiple maintainers,
required reviewers can be restored as an additional separation-of-duties gate.

The `release/*` environment policy is intentionally reusable across release
lines. The workflow applies the narrower check dynamically: a project version
of `1.6.z` must run from `release/1.6.x`, `1.7.z` must run from
`release/1.7.x`, and so on. A mismatched release branch is rejected before any
publisher job receives an OIDC token.

The package-specific environments are intentional. PyPI rejects two pending
projects that use the same owner/repository/workflow/environment identity,
because that identity would be ambiguous when it creates a project for the
first time. After the projects exist, one publisher may technically authorize
multiple projects, but retaining separate identities keeps first publication
and later releases consistent and limits each short-lived token to one
distribution.

Register four Trusted Publisher records, one for each distribution and index:

| Index | PyPI project | Owner | Repository | Workflow | Environment |
|---|---|---|---|---|---|
| TestPyPI | `FluentQt` | `calvinhxx` | `Fluent-Qt` | `python-release.yml` | `testpypi` |
| TestPyPI | `FluentQt-Gallery` | `calvinhxx` | `Fluent-Qt` | `python-release.yml` | `testpypi-gallery` |
| PyPI | `FluentQt` | `calvinhxx` | `Fluent-Qt` | `python-release.yml` | `pypi` |
| PyPI | `FluentQt-Gallery` | `calvinhxx` | `Fluent-Qt` | `python-release.yml` | `pypi-gallery` |

If the `FluentQt` pending records were already registered with `testpypi` and
`pypi`, keep them. Add only the two Gallery environments and register the two
Gallery records with the `-gallery` environment names. Do not delete and
recreate a valid Core record.

Use pending publishers when a project does not yet exist. Do not add a PyPI or
TestPyPI API token to repository, organization, or environment secrets. Only
the two matrix upload job definitions receive `id-token: write`; each expands
to package-scoped Core and Gallery jobs. They do not checkout source or execute
repository scripts. Both jobs download subsets of the same verified 18-wheel
candidate: Core receives 17 wheels and Gallery receives one.

## Prepare a release candidate

1. Integrate the intended changes into the matching `release/X.Y.x` branch.
2. Keep the CMake, vcpkg, documentation, Python API manifest, core wheel, and
   Gallery wheel versions aligned at the intended `X.Y.Z` version.
3. Review `docs/releases/vX.Y.Z.md` and the maintainer changelog.
4. Run full CI with the Python release bundle enabled on the untagged commit:

   ```bash
   gh workflow run CI --ref release/X.Y.x -f matrix=full -f python_release_bundle=true
   ```

5. Require `Release ready` to pass. Download or inspect the canonical bundle
   and record the full-CI run ID, commit SHA, and manifest SHA-256:

   ```bash
   sha256sum python-release-manifest.json
   ```

Do not create `vX.Y.Z` until the TestPyPI stage below succeeds.

## TestPyPI rehearsal

Dispatch the release workflow from the exact release commit that produced the
successful full-CI bundle:

```bash
gh workflow run python-release.yml \
  --ref release/X.Y.x \
  -f stage=testpypi \
  -f recovery=false
```

The workflow locates the successful `Release ready` run for that commit,
downloads its bundle, verifies all local hashes, rejects any production PyPI
files for the version, and compares pre-existing TestPyPI files with the
manifest before using `skip-existing`.

After upload it waits for both TestPyPI JSON records to contain the exact
17+1 file set, then uses Linux x64 with CPython 3.11 to:

- install PySide6-Essentials and Shiboken6 6.9.3 from production PyPI;
- install both FluentQt distributions from TestPyPI with `--no-deps`;
- run `pip check`, version/import/UILib smoke, and Gallery offscreen smoke.

Package-index JSON and Simple API edges can converge at different times. The
exact-version install therefore uses bounded, cache-free retries after the
hash gate passes; this handles propagation delay without rebuilding or
replacing any wheel.

An interrupted TestPyPI run is retried with the same command. Existing files
are skipped only after their hashes match the immutable manifest. A mismatch
requires a new version; package-index files are immutable.

## Stable tag and production publication

Once TestPyPI succeeds, create the annotated tag from the same commit:

```bash
git tag -a vX.Y.Z -m "Release vX.Y.Z"
git push origin vX.Y.Z
```

Wait for the existing C++ Release workflow to publish a non-draft stable
GitHub Release and its desktop packages. Then dispatch production publication:

```bash
gh workflow run python-release.yml \
  --ref vX.Y.Z \
  -f stage=pypi \
  -f recovery=false
```

Production preflight rejects a lightweight or prerelease tag, version drift,
a different TestPyPI/full-CI commit, an unpublished GitHub Release, an
incomplete TestPyPI file set, or an existing production version. The manual
workflow dispatch is the production authorization; after preflight, both
package-scoped upload jobs proceed without a second deployment approval.

The official PyPI publish action uploads the same 18 files
through OIDC Trusted Publishing and creates attestations. Verification checks
the public JSON hashes, validates all 18 attestations against
`calvinhxx/Fluent-Qt`, and performs a normal clean-index installation of both
distributions followed by the UILib and Gallery smoke suites.

## Partial production recovery

Use recovery only when a production run uploaded some, but not all, files:

```bash
gh workflow run python-release.yml \
  --ref vX.Y.Z \
  -f stage=pypi \
  -f recovery=true
```

Recovery first requires every existing PyPI file for `X.Y.Z` to be a subset of
the manifest with an identical SHA-256. Only then may the publish action skip
existing files. Never use recovery to replace a file, upload a rebuilt wheel,
or bypass TestPyPI.

## M6 closure evidence

Record the following in both roadmaps before marking M6 complete:

- full-CI run ID and source commit;
- TestPyPI workflow run ID;
- production PyPI workflow run ID;
- `FluentQt` and `FluentQt-Gallery` PyPI project URLs;
- SHA-256 of `python-release-manifest.json`;
- successful public-index clean-install and attestation verification.

After reviewing the final release content, the repository maintainer explicitly
authorizes and performs synchronization of the tagged release commit to `main`
following release governance. The Qt 6.2.4 / CPython 3.10 lanes remain
non-published compatibility gates.

### v1.6.0 closure record

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

### v1.6.1 standard publication record

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
