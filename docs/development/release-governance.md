# Release Governance

Use this workflow when planning branches, commit messages, release tags,
changelog generation, and release automation.

This is a lightweight single-maintainer flow. The project does not use a
long-lived `develop` branch. Maintenance branches such as `release/1.0.x`
represent supported patch lines, not Git Flow release branches.

## Branches

- `main` is the default branch and records the latest promoted stable baseline.
  Keep it releasable, but do not use it for routine post-release development.
- `release/<major>.<minor>.x` branches are long-lived patch lines. The highest
  active version is the normal working branch for features, fixes, CI,
  packaging, documentation, and other maintenance. The current working branch
  is `release/1.3.x`.
- Commit routine single-maintainer changes directly to the latest long-lived
  release branch after appropriate validation. Do not create a short-lived
  `feat/*`, `fix/*`, `docs/*`, `ci/*`, or `chore/*` branch by default. Keeping
  the patch line linear makes tag-to-tag changelog review straightforward.
- Use older supported release branches only for deliberate backports to those
  patch lines. Do not mix new-line development into an older branch.
- Cut `vX.Y.Z` tags from the matching `release/X.Y.x` branch. After publishing
  the latest stable release, fast-forward or rebase-merge that released commit
  into `main`; create the next long-lived minor line from the updated `main`.
- Create a temporary branch only when explicitly needed for external review,
  contributor work, or risky isolation. Rebase-merge it to keep history linear,
  then delete it promptly.
- Do not create a permanent `develop` branch, and do not delete supported
  long-lived release branches.

## Commits

Use Angular-style Conventional Commits:

```text
<type>(<scope>): <summary>
```

The scope is optional. Keep the summary imperative, concise, and without a final
period.

Allowed commit types:

- `feat`: user-visible feature or new capability.
- `fix`: bug fix.
- `perf`: performance improvement.
- `refactor`: internal code change that does not alter behavior.
- `docs`: documentation-only change.
- `test`: tests or test infrastructure.
- `build`: CMake, packaging, dependencies, or build system change.
- `ci`: GitHub Actions and other CI automation.
- `style`: formatting-only change.
- `chore`: repository maintenance that does not fit another type.
- `revert`: revert a previous commit.

Suggested scopes include `components`, `gallery`, `foundation`, `windowing`,
`navigation`, `cmake`, `docs`, `ci`, and `release`.

Examples:

```text
feat(gallery): add platform design hero cards
fix(windowing): keep macOS traffic lights aligned
ci(release): add tagged release workflow
build(cmake): add release packaging preset
docs(release): document tag policy
```

For breaking changes, use either `!` in the header or a `BREAKING CHANGE:`
footer:

```text
feat(components)!: rename legacy namespace exports

BREAKING CHANGE: Consumers must include fluent component headers from
src/components.
```

Keep unrelated changes in separate commits so changelog generation can classify
them accurately.

## Versions

Use SemVer:

```text
MAJOR.MINOR.PATCH
```

- Increment `PATCH` for compatible bug fixes.
- Increment `MINOR` for new compatible functionality.
- Increment `MAJOR` for incompatible public API or packaging contract changes.
- While the project is `0.x`, incompatible public changes may use a `MINOR`
  bump, but the commit or release notes should still call out the break.

Until a dedicated version file exists, the root CMake project version is the
source of truth:

```cmake
project(FluentQT VERSION X.Y.Z LANGUAGES CXX C)
```

Release automation must verify that the tag version and CMake project version
match.

## Tags

Use annotated SemVer tags with a leading `v`:

```text
vX.Y.Z
vX.Y.Z-rc.N
```

Examples:

```bash
git tag -a v0.2.0 -m "Release v0.2.0"
git tag -a v0.2.0-rc.1 -m "Release v0.2.0-rc.1"
```

Rules:

- Stable releases use `vX.Y.Z`.
- Release candidates use `vX.Y.Z-rc.N`.
- Tags must point at a commit whose CMake project version matches the tag.
- Do not move or replace a public release tag. Publish a patch release instead.
- Prefer a `chore(release): vX.Y.Z` commit when the release updates version
  metadata, changelog files, or packaging metadata before tagging.

## Changelog

Use Conventional Commits between release tags as the auditable maintainer
changelog. Do not publish that commit list directly as public release notes.
Public notes need an editorial pass that groups iterative commits into a few
user-visible outcomes and explains why the release matters.

Store reviewed public notes at `docs/releases/vX.Y.Z.md` before creating the
tag. Start with a short release theme, then describe capabilities and fixes in
user language. Avoid commit scopes, implementation-only terminology, repeated
entries for the same problem, and generic maintenance summaries. Link to the
full comparison when commit-level detail is useful.

Maintainer changelog review can still group commits as follows:

| Commit type | Changelog section |
| --- | --- |
| `feat` | Features |
| `fix` | Bug Fixes |
| `perf` | Performance |
| `build`, `ci` | Build & CI |
| `docs` | Documentation |
| `refactor` | Refactoring |
| `test` | Tests |
| `chore`, `style` | Maintenance or omitted |

Breaking changes must be called out in a dedicated section even when the project
is still below `1.0.0`.

Use the generator for both reviewed public notes and maintainer changelog
review:

```bash
python scripts/release/generate_changelog.py --from v1.0.0 --to HEAD
python scripts/release/generate_changelog.py --from v1.0.0 --to HEAD --audience maintainer
python scripts/release/generate_changelog.py --tag v1.1.0 --require-curated --output release-notes.md
```

While developing the current patch line, review only its unreleased range:

```bash
python scripts/release/generate_changelog.py --from v1.3.0 --to release/1.3.x --audience maintainer --check
```

`--tag` resolves the previous release tag automatically when `--from` is not
provided. For public output, the generator automatically discovers
`docs/releases/<tag>.md`. `--require-curated` fails when that reviewed file is
missing, which is mandatory in the GitHub Release workflow. Without it, the
script may produce a commit-derived draft for local review, but that draft must
not be published unchanged.

The generator skips merge commits and `chore(release): vX.Y.Z` release-marker
commits. Use `--audience maintainer` for the detailed commit-by-commit view with
stable section ordering and short SHAs for traceability.

Use `--check` before tagging when you want to fail on commits that cannot be
classified by the Conventional Commit rules:

```bash
python scripts/release/generate_changelog.py --from v1.0.0 --to HEAD --check
```

The GitHub Release workflow publishes notes from this generator with
`--notes-file` instead of GitHub's default generated notes.

## Release Checklist

Before creating a stable tag:

1. Confirm `main` contains the intended release commits.
2. Confirm the worktree is clean.
3. Confirm the CMake project version matches the intended tag.
4. Run the supported CI build/test matrix for the release, normally
   `gh workflow run CI --ref <branch-or-tag> -f matrix=full` or an equivalent
   local host full validation. The GitHub Release workflow requires the tagged
   commit to have a successful `Release ready` check produced by this full
   matrix unless `require_ci=false` is selected manually.
   If the change touches CMake, tests, Qt compatibility, platform behavior, or
   component input/windowing behavior, include the Ubuntu 22.04 Linux validation
   covered in [Linux Workflow](linux-workflow.md).
5. Add `docs/releases/vX.Y.Z.md`, preview it with `--require-curated`, and review
   the maintainer changelog from the previous release tag.
6. Create an annotated tag.
7. Build and attach release artifacts.
8. Publish the GitHub Release notes, installers, and one aggregate
   `SHA256SUMS.txt`.

Later automation may perform these steps, but the rules above remain the
contract that CI, changelog, and packaging workflows should enforce.

## Release Package Sets

- `standard` is the default stable release package set. It publishes the
  nine supported release package lanes:
  - Qt 5.15 Windows x64 installer.
  - Qt 5.15 macOS x64 DMG.
  - Qt 5.15 Ubuntu 22.04 x64 DEB.
  - Qt 6.2 Windows x64 installer.
  - Qt 6.2 macOS x64 DMG.
  - Qt 6.2 macOS arm64 DMG.
  - Qt 6.2 Ubuntu 22.04 x64 DEB.
  - Qt 6.2 Ubuntu 22.04 arm64 DEB.
  - Qt 6.9.3 Windows arm64 installer.
- `smoke` runs only the macOS x64 and Windows x64 package lanes without
  publishing and is intended for manual release workflow validation.

The package catalog lives in `.github/package-matrix.json`; `standard` and
`smoke` are selected from that shared source of truth.
