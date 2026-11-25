"""Submit a validated release overlay from an isolated microsoft/vcpkg checkout."""

import argparse
import json
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

from package_vcpkg_overlay import STABLE_TAG, SOURCE_HASH


UPSTREAM = "microsoft/vcpkg"
FORK = "calvinhxx/vcpkg"
PORT = Path("ports/fluent-qt")
VERSIONS = Path("versions/f-/fluent-qt.json")
BASELINE = Path("versions/baseline.json")


def run(*args):
    result = subprocess.run(args, stdout=subprocess.PIPE, text=True)
    if result.returncode:
        print(result.stdout, file=sys.stderr)
        result.check_returncode()
    return result.stdout.strip()


def version(port):
    value = json.loads((port / "vcpkg.json").read_bytes())["version-semver"]
    return tuple(map(int, value.split(".")))


def same_archive(left, right):
    left_hash = SOURCE_HASH.search((left / "portfile.cmake").read_text())
    right_hash = SOURCE_HASH.search((right / "portfile.cmake").read_text())
    return left_hash and right_hash and left_hash[2].lower() == right_hash[2].lower()


def open_prs():
    # The first, manually submitted port predates the per-release branch names.
    query = ('.[] | select(.head.repo.full_name == "calvinhxx/vcpkg" and '
             '(.head.ref == "add-fluent-qt-1.12.3" or '
             '(.head.ref | startswith("release/fluent-qt-v")))) | '
             '{number, branch: .head.ref, url: .html_url, title, body} | @json')
    rows = run("gh", "api", "--paginate", "-X", "GET", f"repos/{UPSTREAM}/pulls",
               "-f", "state=open", "-f", "base=master", "-f", "per_page=100", "--jq", query)
    return [json.loads(row) for row in rows.splitlines()]


def submit(tag, overlay, dry_run):
    match = STABLE_TAG.fullmatch(tag)
    if not match:
        raise ValueError(f"Expected a stable release tag, got {tag!r}")
    wanted = version(overlay)
    if wanted != tuple(map(int, match.groups())):
        raise ValueError("Release tag and overlay manifest disagree")
    if not Path(".vcpkg-root").is_file() or not BASELINE.is_file() or run("git", "status", "--porcelain"):
        raise ValueError("Run the submitter in a clean vcpkg registry checkout")
    upstream_tip = run("git", "rev-parse", "HEAD")
    registered = PORT.is_dir()
    if registered and version(PORT) >= wanted:
        if version(PORT) == wanted and not same_archive(PORT, overlay):
            raise ValueError(f"Registry already contains a different archive for {tag}")
        print(f"The registry already contains {tag} or newer: https://github.com/{UPSTREAM}/tree/master/{PORT}")
        return

    prs = open_prs()
    if len(prs) > 1:
        raise ValueError("Multiple Fluent-Qt update PRs are open: " + ", ".join(p["url"] for p in prs))
    pr = next(iter(prs), None)
    branch = pr["branch"] if pr else f"release/fluent-qt-{tag}"
    fork_url = f"https://github.com/{FORK}.git"
    ref = f"refs/heads/{branch}"
    # Also recover a successful push followed by a failed PR creation.
    if run("git", "ls-remote", "--heads", fork_url, ref):
        if not pr:
            closed = run("gh", "api", "-X", "GET", f"repos/{UPSTREAM}/pulls", "-f", "state=closed",
                         "-f", f"head=calvinhxx:{branch}", "-f", "base=master", "--jq", ".[0].html_url // empty")
            if closed:
                raise ValueError(f"Review the previously closed PR before resubmitting: {closed}")
        run("git", "fetch", fork_url, ref)
        run("git", "checkout", "--detach", "FETCH_HEAD")
        pending = version(PORT)
        if pending > wanted:
            print(f"A newer release is already pending at {pr['url'] if pr else branch}.")
            return
        if pending == wanted and not same_archive(PORT, overlay):
            raise ValueError(f"The fork already contains a different archive for {tag}")
    else:
        if pr:
            raise ValueError(f"The head branch for {pr['url']} has disappeared")
        pending = None
        run("git", "checkout", "--detach", upstream_tip)

    title = f"[fluent-qt] {'Update to' if registered else 'Add version'} {tag[1:]}"
    if pending == wanted and pr and tag[1:] == pr["title"].split()[-1]:
        print(f"Already submitted: {pr['url']}")
        return

    if pending != wanted:
        if pending is not None:
            run("git", "merge", "--no-edit", upstream_tip)
        if PORT.exists():
            shutil.rmtree(PORT)
        shutil.copytree(overlay, PORT)
        run("git", "add", "--", PORT)
        run("git", "commit", "-m", title)
        # Keep all published versions. Supersede only the earlier, unmerged proposal.
        if registered:
            run("git", "restore", "--source", upstream_tip, "--", VERSIONS)
        elif VERSIONS.exists():
            VERSIONS.unlink()
        tool = str(Path("vcpkg").resolve())
        root = f"--vcpkg-root={Path.cwd()}"
        run(tool, "x-add-version", "fluent-qt", root)
        run("git", "add", "--", BASELINE, VERSIONS)
        run("git", "commit", "-m", "Update version database")
        run(tool, "x-ci-verify-versions", root)
    if dry_run:
        print(f"Dry run for {tag}: {FORK}:{branch} -> {UPSTREAM}:master")
        return

    run("git", "-c", "credential.helper=", "-c", "credential.helper=!gh auth git-credential",
        "push", fork_url, f"HEAD:{ref}")
    release = f"https://github.com/calvinhxx/Fluent-Qt/releases/tag/{tag}"
    body = (f"Packages the Qt Widgets SDK from [Fluent-Qt {tag}]({release}).\n\n"
            "Windows and Linux consumers built and ran in Debug and Release using the "
            f"[published overlay]({release.replace('/tag/', '/download/')}/fluent-qt-{tag}-vcpkg.zip). "
            "The version database was generated and verified with vcpkg.\n")
    if os.getenv("GITHUB_RUN_ID"):
        body += f"\n[Release checks](https://github.com/calvinhxx/Fluent-Qt/actions/runs/{os.environ['GITHUB_RUN_ID']})\n"
    if pr and "### New port checklist" in pr["body"]:
        body += "\n### New port checklist" + pr["body"].split("### New port checklist", 1)[1]
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8") as message:
        message.write(body)
        message.flush()
        args = ("--repo", UPSTREAM, "--title", title, "--body-file", message.name)
        if pr:
            print(run("gh", "pr", "edit", str(pr["number"]), *args))
            state = run("gh", "pr", "view", str(pr["number"]), "--repo", UPSTREAM,
                        "--json", "state", "--jq", ".state")
            if state != "OPEN":
                raise RuntimeError(f"{pr['url']} closed during submission. Rerun the job against the current registry.")
        else:
            print(run("gh", "pr", "create", "--base", "master", "--head", f"calvinhxx:{branch}", *args))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tag")
    parser.add_argument("overlay", type=Path)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    submit(args.tag, args.overlay.resolve(), args.dry_run)
