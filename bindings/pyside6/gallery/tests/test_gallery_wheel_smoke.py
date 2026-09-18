"""Smoke-test the standalone FluentQt Gallery wheel in a clean environment."""

import argparse
from importlib import metadata, util
import os
from pathlib import Path
import sys

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


def verify_control_images(package_dir, recorded_files):
    """Validate the installed wheel's own RECORD without a source checkout."""
    prefix = ("fluentqt_gallery", "assets", "control_images")
    expected = {
        Path(*path.parts[len(prefix):])
        for path in (recorded_files or ())
        if path.parts[:len(prefix)] == prefix and path.suffix == ".png"
    }
    image_root = package_dir / "assets/control_images"
    actual = {path.relative_to(image_root) for path in image_root.rglob("*.png")}
    if not expected or actual != expected:
        raise AssertionError(
            "Installed Gallery images differ from wheel RECORD: missing={0}, extra={1}".format(
                sorted(expected - actual), sorted(actual - expected)
            )
        )


def native_contract(project_root):
    generator_path = project_root / "bindings/pyside6/gallery/tools/generate_gallery_contract.py"
    if not generator_path.is_file():
        raise AssertionError("--project-root does not contain the Gallery contract generator")
    spec = util.spec_from_file_location("gallery_smoke_contract", generator_path)
    generator = util.module_from_spec(spec)
    sys.modules[spec.name] = generator
    spec.loader.exec_module(generator)
    return generator.generate_contract(project_root)


def verify_source_contract(project_root, package_dir, contract):
    """Optional CI check against the exact source checkout being packaged."""
    if contract != native_contract(project_root):
        raise AssertionError("Installed Gallery contract differs from native sources")
    source_root = project_root / "app/assets/control_images"
    image_root = package_dir / "assets/control_images"
    expected = {path.relative_to(source_root) for path in source_root.rglob("*.png")}
    actual = {path.relative_to(image_root) for path in image_root.rglob("*.png")}
    if not expected or actual != expected:
        raise AssertionError(
            "Gallery images differ from native sources: missing={0}, extra={1}".format(
                sorted(expected - actual), sorted(actual - expected)
            )
        )


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path,
                        help="Also compare installed data with this source checkout (required in CI).")
    args = parser.parse_args(argv)
    import fluentqt
    import fluentqt_gallery
    from fluentqt_gallery.catalog import CONTRACT, CATEGORIES, ENTRIES, ROUTES, SUPPORT_TYPES
    from fluentqt_gallery.native_samples import ported_sample_keys

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
    if args.project_root is not None:
        verify_source_contract(args.project_root.resolve(), package_dir, CONTRACT)
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
    verify_control_images(package_dir, metadata.files("FluentQt-Gallery"))
    expected_home_tiles = {
        "GitHub-Mark.png",
        "Header-Toolkit.png",
        "Header-WindowsDesign.png",
        "Header-WinUI.png",
        "Qt-Logo.png",
    }
    actual_home_tiles = {
        path.name
        for path in (package_dir / "assets" / "home_header_tiles").glob("*.png")
    }
    if actual_home_tiles != expected_home_tiles:
        raise AssertionError(
            "Standalone Gallery has the wrong Home tiles: {0}".format(
                sorted(actual_home_tiles)
            )
        )

    sample_count = sum(len(entry.samples) for entry in ENTRIES)
    if (
        len(CATEGORIES) != len(CONTRACT["categories"])
        or len(ENTRIES) != CONTRACT["summary"]["component_count"]
        or len(ROUTES) != CONTRACT["summary"]["route_count"]
        or set(SUPPORT_TYPES) != set(CONTRACT["binding_support_types"])
        or sample_count != CONTRACT["summary"]["sample_count"]
        or set(ported_sample_keys()) != {
            (entry.route_id, sample.id) for entry in ENTRIES for sample in entry.samples
        }
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
    route_count = len(window.all_route_ids())
    if route_count != len(ROUTES):
        raise AssertionError(
            "Standalone Gallery route coverage: expected {0}, found {1}".format(
                len(ROUTES), route_count
            )
        )
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
