"""Smoke-test the standalone FluentQt Gallery wheel in a clean environment."""

from importlib import metadata, util
import os
from pathlib import Path
import sys

import fluentqt
import fluentqt_gallery
from fluentqt_gallery.catalog import CATEGORIES, ENTRIES, ROUTES, SUPPORT_TYPES
from fluentqt_gallery.native_samples import ported_sample_keys


def require_installed_below_prefix(path):
    prefix = Path(sys.prefix).resolve()
    resolved = Path(path).resolve()
    try:
        common = os.path.commonpath((str(prefix), str(resolved)))
    except ValueError:
        common = ""
    if os.path.normcase(common) != os.path.normcase(str(prefix)):
        raise AssertionError(
            "Expected {0} below clean environment {1}".format(resolved, prefix)
        )


def main():
    expected_version = os.environ["FLUENTQT_EXPECTED_VERSION"]
    if metadata.version("FluentQt") != expected_version:
        raise AssertionError("Installed FluentQt wheel has the wrong version")
    if metadata.version("FluentQt-Gallery") != expected_version:
        raise AssertionError("Installed Gallery wheel has the wrong version")
    expected_requirement = "FluentQt (=={0})".format(expected_version)
    if expected_requirement not in (metadata.requires("FluentQt-Gallery") or ()):
        raise AssertionError("Gallery wheel does not pin the matching FluentQt")
    if fluentqt.__version__ != expected_version:
        raise AssertionError("Gallery dependency and native UILib versions differ")
    if util.find_spec("fluentqt.gallery") is not None:
        raise AssertionError("Gallery leaked back into the fluentqt namespace")

    package_dir = Path(fluentqt_gallery.__file__).resolve().parent
    require_installed_below_prefix(package_dir)
    native_files = tuple(
        path
        for path in package_dir.rglob("*")
        if path.suffix.lower() in {".so", ".pyd", ".dylib"}
    )
    if native_files:
        raise AssertionError("Standalone Gallery wheel contains native binaries")
    required_assets = (
        package_dir / "assets" / "app-icon.png",
        package_dir / "assets" / "icon_aliases.json",
        package_dir / "assets" / "icon_catalog.json",
        package_dir / "assets" / "control_images" / "Placeholder.png",
        package_dir
        / "assets"
        / "home_header_tiles"
        / "Header-WindowsDesign.png",
        package_dir / "contract.json",
    )
    for asset in required_assets:
        if not asset.is_file():
            raise AssertionError("Standalone Gallery asset is missing: {0}".format(asset))
    if len(tuple((package_dir / "assets" / "control_images").rglob("*.png"))) != 75:
        raise AssertionError("Standalone Gallery has the wrong control-image count")
    if len(tuple((package_dir / "assets" / "home_header_tiles").glob("*.png"))) != 7:
        raise AssertionError("Standalone Gallery has the wrong Home-tile count")

    sample_count = sum(len(entry.samples) for entry in ENTRIES)
    if (
        len(CATEGORIES) != 12
        or len(ENTRIES) != 68
        or len(ROUTES) != 89
        or len(SUPPORT_TYPES) != 20
        or sample_count != 202
        or len(ported_sample_keys()) != 202
    ):
        raise AssertionError("Standalone Gallery catalog has wrong coverage")

    fluentqt.prepare_high_dpi_application()
    from PySide6.QtCore import QCoreApplication, QEvent
    from PySide6.QtWidgets import QApplication
    from fluentqt_gallery.window import GalleryWindow

    app = QApplication.instance() or QApplication([])
    app.setProperty("fluentqtGalleryAutomated", True)
    if not fluentqt.initialize_resources():
        raise AssertionError("FluentQt resources could not be initialized")
    window = GalleryWindow()
    window.show()
    QApplication.processEvents()
    failures = window.visit_all_routes()
    if failures:
        raise AssertionError(
            "Standalone Gallery route failures: {0}".format("; ".join(failures))
        )
    if len(window.all_route_ids()) != 89:
        raise AssertionError("Standalone Gallery has wrong route coverage")
    window.navigate_component("button")
    if window.current_route != "button":
        raise AssertionError("Standalone Gallery could not navigate")
    window.close()
    window.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    app.processEvents()
    print(
        "FluentQt Gallery {0} standalone wheel smoke passed".format(
            expected_version
        ),
        flush=True,
    )


if __name__ == "__main__":
    main()
