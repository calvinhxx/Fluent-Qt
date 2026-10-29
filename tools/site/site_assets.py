"""Map stable website URLs to their canonical, tracked artwork."""

from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]
COPIED_ASSETS = {
    "assets/app-icon.png": "app/assets/app-icon.png",
    "assets/gallery-home.jpg": "docs/assets/readme/gallery-home.jpg",
    "assets/gallery/gallery-hero-real.png": "docs/assets/readme/hero.png",
}


def source_path(relative: str, site_root: Path) -> Path:
    canonical = COPIED_ASSETS.get(relative)
    return site_root.parent / canonical if canonical else site_root / relative


def copy_assets(destination: Path, root: Path = ROOT) -> None:
    for relative, canonical in COPIED_ASSETS.items():
        source = root / canonical
        if source.is_symlink():
            raise ValueError(f"Canonical website artwork must not be a symlink: {source}")
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
