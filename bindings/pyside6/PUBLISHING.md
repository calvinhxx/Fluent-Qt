# Python Publishing Runbook

This document is the release contract for the `FluentQt` and
`FluentQt-Gallery` Python distributions. It complements the general
[release governance](../../docs/development/release-governance.md), the
[wheel matrix](wheel-matrix.json), and the [manylinux policy](MANYLINUX.md).

M6 is not complete merely because all wheel lanes compile. Completion requires
one immutable CI bundle to pass TestPyPI, stable-tag, PyPI, attestation, and
clean public-install verification without rebuilding any wheel.

## Immutable release bundle

A successful full CI run creates one artifact named
`fluentqt-python-release-bundle`:

```text
python-release-bundle/
├── dist/                         # 17 FluentQt + 1 FluentQt-Gallery wheel
├── audits/                       # five manylinux audit reports
├── PYTHON_SHA256SUMS.txt
└── python-release-manifest.json
```

`.github/scripts/assemble-pyside-release-bundle.py` rejects compatibility-only
CPython 3.10 wheels, raw `linux_*` wheels, missing or extra matrix entries,
wrong package metadata, mismatched manylinux evidence, and non-identical
Gallery wheels. The 17 build lanes must produce a byte-identical Gallery wheel;
the bundle retains exactly one copy.

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
| `testpypi` | Selected branch `release/1.6.x` | Optional for the rehearsal |
| `pypi` | Protected tags matching `v*` | Required reviewer: repository maintainer |

For a single-maintainer repository, leave **Prevent self-review** disabled on
the `pypi` environment. Otherwise the only maintainer cannot approve the
production deployment.

Register four Trusted Publisher records, one for each distribution and index:

| Index | PyPI project | Owner | Repository | Workflow | Environment |
|---|---|---|---|---|---|
| TestPyPI | `FluentQt` | `calvinhxx` | `Fluent-Qt` | `python-release.yml` | `testpypi` |
| TestPyPI | `FluentQt-Gallery` | `calvinhxx` | `Fluent-Qt` | `python-release.yml` | `testpypi` |
| PyPI | `FluentQt` | `calvinhxx` | `Fluent-Qt` | `python-release.yml` | `pypi` |
| PyPI | `FluentQt-Gallery` | `calvinhxx` | `Fluent-Qt` | `python-release.yml` | `pypi` |

Use pending publishers when a project does not yet exist. Do not add a PyPI or
TestPyPI API token to repository, organization, or environment secrets. Only
the two upload jobs receive `id-token: write`; they do not checkout source or
execute repository scripts.

## Prepare a release candidate

1. Integrate the intended changes into `release/1.6.x`.
2. Keep the CMake, vcpkg, documentation, Python API manifest, core wheel, and
   Gallery wheel versions aligned at `1.6.0`.
3. Review `docs/releases/v1.6.0.md` and the maintainer changelog.
4. Run full CI on the untagged release commit:

   ```bash
   gh workflow run CI --ref release/1.6.x -f matrix=full
   ```

5. Require `Release ready` to pass. Download or inspect the canonical bundle
   and record the full-CI run ID, commit SHA, and manifest SHA-256:

   ```bash
   sha256sum python-release-manifest.json
   ```

Do not create `v1.6.0` until the TestPyPI stage below succeeds.

## TestPyPI rehearsal

Dispatch the release workflow from the exact release commit that produced the
successful full-CI bundle:

```bash
gh workflow run python-release.yml \
  --ref release/1.6.x \
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

An interrupted TestPyPI run is retried with the same command. Existing files
are skipped only after their hashes match the immutable manifest. A mismatch
requires a new version; package-index files are immutable.

## Stable tag and production publication

Once TestPyPI succeeds, create the annotated tag from the same commit:

```bash
git tag -a v1.6.0 -m "Release v1.6.0"
git push origin v1.6.0
```

Wait for the existing C++ Release workflow to publish a non-draft stable
GitHub Release and its desktop packages. Then dispatch production publication:

```bash
gh workflow run python-release.yml \
  --ref v1.6.0 \
  -f stage=pypi \
  -f recovery=false
```

Production preflight rejects a lightweight or prerelease tag, version drift,
a different TestPyPI/full-CI commit, an unpublished GitHub Release, an
incomplete TestPyPI file set, or an existing production version. The upload
job then pauses at the `pypi` environment for maintainer approval.

After approval, the official PyPI publish action uploads the same 18 files
through OIDC Trusted Publishing and creates attestations. Verification checks
the public JSON hashes, validates all 18 attestations against
`calvinhxx/Fluent-Qt`, and performs a normal clean-index installation of both
distributions followed by the UILib and Gallery smoke suites.

## Partial production recovery

Use recovery only when a production run uploaded some, but not all, files:

```bash
gh workflow run python-release.yml \
  --ref v1.6.0 \
  -f stage=pypi \
  -f recovery=true
```

Recovery first requires every existing PyPI file for `1.6.0` to be a subset of
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

Finally synchronize the tagged release commit to `main` following release
governance and delete the temporary `python-sup` branch. The Qt 6.2.4 /
CPython 3.10 lanes remain non-published compatibility gates.
