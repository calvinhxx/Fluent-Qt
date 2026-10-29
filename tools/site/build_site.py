#!/usr/bin/env python3
"""Generate the website from source and stage a verifiable Pages artifact.

Generated pages, homepage files and native previews are not tracked in Git; CI
writes them only inside its checkout/artifact. Catalog snapshots are refreshed
from source before validation. Native previews are reused only when their
complete input digest matches.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from generate_component_previews import ROOT, capture_digest, validated_manifest
from site_assets import COPIED_ASSETS, copy_assets

RECEIPT = "site-build-info.json"


def run(*arguments: str) -> None:
    subprocess.run([sys.executable, *map(str, arguments)], cwd=ROOT, check=True)


def prepare_catalogs() -> None:
    """Refresh derived inputs before quality gates or preview recipe discovery."""
    run("tools/ai/generate_ai_catalog.py", "--project-root", str(ROOT))
    run("tools/site/generate_api_reference.py", "--catalog-only")
    run("tools/ai/generate_ai_catalog.py", "--project-root", str(ROOT), "--check")
    run("tools/site/generate_api_reference.py", "--check")


def current_previews(gallery: Path | None = None) -> bool:
    try:
        manifest = validated_manifest()
        if gallery is not None:
            return manifest.get("binary_sha256") == hashlib.sha256(gallery.read_bytes()).hexdigest()
    except (OSError, ValueError, KeyError):
        return False
    return True


def checksums(folder: Path) -> dict[str, str]:
    result = {}
    for path in sorted(folder.rglob("*")):
        if path.is_symlink():
            raise ValueError(f"Site artifacts must not contain symlinks: {path}")
        if path.is_file() and path != folder / RECEIPT:
            result[path.relative_to(folder).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def verify(folder: Path, commit: str) -> None:
    receipt = json.loads((folder / RECEIPT).read_text(encoding="utf-8"))
    if receipt.get("schema_version") != 1 or receipt.get("commit") != commit:
        raise ValueError("Site artifact does not belong to the requested commit")
    if not receipt.get("files") or receipt["files"] != checksums(folder):
        raise ValueError("Site artifact contents differ from the validated build")
    for name in ("index.html", "zh-CN/index.html", "api/index.html", "api/catalog.json",
                 "api/button/index.html", "zh-CN/api/button/index.html", "sitemap.xml",
                 "assets/component-previews/manifest.json", *COPIED_ASSETS):
        if name not in receipt["files"]:
            raise ValueError(f"Incomplete site artifact: {name}")


def stage(output: Path, commit: str) -> None:
    source = (ROOT / "site").resolve()
    output = output.resolve()
    if output == ROOT.resolve() or output in ROOT.resolve().parents or output == source or source in output.parents:
        raise ValueError("The artifact directory must be separate from the source site")
    if output.exists() and not (output / RECEIPT).is_file():
        raise ValueError("Refusing to replace a directory without a site build receipt")
    output.parent.mkdir(parents=True, exist_ok=True)
    # Never deploy ignored local servers, build products or old generated routes.
    from generate_api_reference import generate_reference
    from component_docs import collect, generated_outputs
    previews = validated_manifest()
    pages = generated_outputs(collect(generate_reference(), previews))
    expected_images = {item["path"] for item in previews["images"]}
    expected_images.add("assets/component-previews/manifest.json")
    with tempfile.TemporaryDirectory(prefix="site-stage-", dir=output.parent) as temporary:
        staged = Path(temporary) / "site"
        for path in source.rglob("*"):
            relative = path.relative_to(source).as_posix()
            if relative in COPIED_ASSETS:
                continue
            if relative == "gallery" or relative.startswith("gallery/"):
                continue
            if path.is_symlink():
                raise ValueError(f"Unexpected symlink in website source: {path}")
            if not path.is_file() or path.name == ".DS_Store":
                continue
            if relative.startswith(("api/", "zh-CN/api/")) and path.suffix == ".html" and relative not in pages:
                continue
            if relative.startswith("assets/component-previews/") and relative not in expected_images:
                continue
            if relative == RECEIPT:
                continue
            destination = staged / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, destination)
        copy_assets(staged, ROOT)
        receipt = {"schema_version": 1, "commit": commit,
                   "preview_input_digest": previews["input_digest"], "files": checksums(staged)}
        (staged / RECEIPT).write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
        verify(staged, commit)
        if output.exists():
            shutil.rmtree(output)
        shutil.move(str(staged), output)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--prepare", action="store_true", help="Generate and validate current catalog inputs without Qt")
    mode.add_argument("--preview-key", action="store_true", help="Print the exact native preview cache key")
    mode.add_argument("--verify", type=Path, help="Verify a downloaded site artifact")
    parser.add_argument("--gallery", type=Path, help="Current native Gallery executable, needed for stale captures")
    parser.add_argument("--output", type=Path, default=ROOT / "build/site-pages")
    parser.add_argument("--commit", help="Expected checkout commit (required when verifying)")
    args = parser.parse_args()
    if args.prepare:
        prepare_catalogs()
        return
    if args.preview_key:
        print(capture_digest())
        return
    if args.verify:
        if not args.commit:
            parser.error("--verify requires --commit")
        verify(args.verify, args.commit)
        return
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    if args.commit and args.commit != commit:
        raise ValueError("Checkout does not match the requested site commit")
    prepare_catalogs()
    run("tools/site/generate_localized_site.py")
    if not current_previews(args.gallery):
        if not args.gallery:
            parser.error("Preview inputs changed; build the native Gallery and pass --gallery")
        run("tools/site/generate_component_previews.py", "--executable", str(args.gallery.resolve()))
    run("tools/site/generate_api_reference.py", "--require-current-previews")
    stage(args.output, commit)
    print(f"Generated website and component documentation: {args.output}")


if __name__ == "__main__":
    main()
