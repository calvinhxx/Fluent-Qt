"""Command-line entry point for the standalone PySide6 Gallery."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

import fluentqt
from PySide6.QtCore import (
    QCoreApplication,
    QElapsedTimer,
    QEvent,
    QPropertyAnimation,
    QSize,
    Qt,
    QTimer,
    qVersion,
)
from PySide6.QtGui import QColor, QGuiApplication, QImage, QPainter
from PySide6.QtWidgets import QApplication, QWidget

from .identity import APPLICATION_ID, APPLICATION_NAME, ORGANIZATION_NAME
from .single_instance import GallerySingleInstance, StartResult


_ROUTE_SETTLE_MS = 10
_NETWORK_SETTLE_POLL_MS = 50
_NETWORK_SETTLE_TIMEOUT_MS = 5000


def _parse_window_size(value: str) -> QSize:
    match = value.lower().split("x", 1)
    if len(match) != 2:
        raise argparse.ArgumentTypeError("window size must use WIDTHxHEIGHT")
    try:
        width, height = (int(part) for part in match)
    except ValueError as error:
        raise argparse.ArgumentTypeError("window size must use WIDTHxHEIGHT") from error
    if width < 1 or height < 1:
        raise argparse.ArgumentTypeError("window size must be positive")
    return QSize(width, height)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Browse and validate FluentQt's public PySide6 controls."
    )
    parser.add_argument(
        "--route",
        default="home",
        help="Open an exact C++ Gallery route id or a public Python type name.",
    )
    parser.add_argument(
        "--snapshot",
        type=Path,
        help="Save the selected Gallery route as a PNG and exit.",
    )
    parser.add_argument(
        "--snapshot-dir",
        type=Path,
        help="Save every native Gallery route as a parity PNG and exit.",
    )
    parser.add_argument(
        "--window-size",
        type=_parse_window_size,
        help="Set a deterministic WIDTHxHEIGHT for visual parity snapshots.",
    )
    parser.add_argument(
        "--theme",
        choices=("system", "light", "dark"),
        default="system",
        help="Force a theme for deterministic visual parity snapshots.",
    )
    parser.add_argument(
        "--report",
        type=Path,
        help="Write the catalog/runtime acceptance result as JSON.",
    )
    parser.add_argument(
        "--verify-catalog",
        action="store_true",
        help="Construct every live preview and reject coverage gaps.",
    )
    parser.add_argument(
        "--walk-routes",
        action="store_true",
        help="Build and navigate every home/category/component route.",
    )
    parser.add_argument(
        "--auto-close-ms",
        type=int,
        default=0,
        help="Automatically close an otherwise interactive Gallery.",
    )
    return parser.parse_args(argv)


def normalize_route(value: str) -> str:
    from .catalog import ENTRY_BY_NAME, ROUTE_BY_ID

    route = value.strip()
    if route in ROUTE_BY_ID:
        return route
    if route in ENTRY_BY_NAME:
        return ENTRY_BY_NAME[route].route_id
    # Accept the prototype Gallery's prefixed routes as a compatibility aid,
    # but normalize them to the native C++ route ids immediately.
    if route.startswith("category/") or route.startswith("component/"):
        candidate = route.split("/", 1)[1]
        if candidate in ROUTE_BY_ID:
            return candidate
        if candidate in ENTRY_BY_NAME:
            return ENTRY_BY_NAME[candidate].route_id
    return route or "home"


def runtime_catalog_errors() -> list[str]:
    from .catalog import ENTRIES, SUPPORT_TYPES

    errors = []
    covered_types = [entry.name for entry in ENTRIES] + sorted(SUPPORT_TYPES)
    for name in covered_types:
        value = getattr(fluentqt, name, None)
        if value is None:
            errors.append("Missing public runtime type: {0}".format(name))
        elif not isinstance(value, type):
            errors.append("Public runtime symbol is not a type: {0}".format(name))
    return errors


def report_payload(
    window: GalleryWindow,
    failures: list[str],
    visited_routes: int,
) -> dict[str, object]:
    from .catalog import CATEGORIES, ENTRIES, SUPPORT_TYPES
    from .native_samples import ported_sample_keys

    native_sample_count = len(ported_sample_keys())
    sample_count = sum(len(entry.samples) for entry in ENTRIES)
    return {
        "api_version": fluentqt.__api_version__,
        "binding_build_info": fluentqt.binding_build_info(),
        "category_count": len(CATEGORIES),
        "component_count": len(ENTRIES),
        "sample_count": sample_count,
        "native_equivalent_sample_count": native_sample_count,
        "fallback_sample_count": sample_count - native_sample_count,
        "support_type_count": len(SUPPORT_TYPES),
        "current_route": window.current_route,
        "failures": failures,
        "fluentqt_version": fluentqt.__version__,
        "platform_plugin": QApplication.platformName(),
        "qt_version": qVersion(),
        "route_count": len(window.all_route_ids()),
        "visited_routes": visited_routes,
    }


def save_report(path: Path, payload: dict[str, object]) -> None:
    output = path.expanduser().resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(payload, indent=2, sort_keys=True, default=str) + "\n",
        encoding="utf-8",
    )
    print("report: {0}".format(output))


def _normalize_snapshot_capture(
    captured: QImage,
    target_size: QSize,
) -> QImage:
    normalized = QImage(captured)
    # ``grab()`` stores physical pixels and tags them with the screen DPR. The
    # output PNG contract is a deterministic logical size, so treat those
    # physical pixels as an ordinary image before resampling/compositing.
    # Otherwise QPainter applies the DPR a second time and a Retina capture
    # occupies only the top-left quarter of the saved PNG.
    normalized.setDevicePixelRatio(1.0)
    if normalized.size() != target_size:
        normalized = normalized.scaled(
            target_size,
            Qt.IgnoreAspectRatio,
            Qt.SmoothTransformation,
        )
    normalized.setDevicePixelRatio(1.0)
    return normalized


def save_snapshot(
    window: GalleryWindow,
    path: Path,
    snapshot_size: QSize | None = None,
) -> None:
    output = path.expanduser().resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    # Snapshot parity must not depend on where the host pointer happened to be
    # when the native window appeared. Clear both custom leave-state and Qt's
    # style hover attribute immediately before rendering.
    for widget in (window, *window.findChildren(QWidget)):
        # underMouse() is derived from the desktop cursor and can already be
        # false while a Fluent control still retains its app-owned hover
        # state. Always deliver Leave and clear the Qt attribute so route
        # snapshots do not depend on the cursor's previous location.
        QCoreApplication.sendEvent(widget, QEvent(QEvent.Leave))
        widget.setAttribute(Qt.WA_UnderMouse, False)
    pixmap = window.grab()
    if pixmap.isNull():
        raise RuntimeError("Unable to capture Gallery snapshot: {0}".format(output))

    # QWidget.grab() cannot include the DWM/Mica compositor layer. On Windows
    # that leaves otherwise correct title/navigation pixels transparent, which
    # many PNG viewers display as black. Composite the widget render over the
    # same neutral fallback canvas used when a platform backdrop is unavailable.
    # Fully opaque page and Hero pixels remain unchanged.
    target_size = snapshot_size if snapshot_size is not None else window.size()
    captured = _normalize_snapshot_capture(pixmap.toImage(), target_size)
    image = QImage(captured.size(), QImage.Format_ARGB32_Premultiplied)
    background = (
        QColor("#202020")
        if fluentqt.current_theme() == fluentqt.Theme.Dark
        else QColor("#F3F3F3")
    )
    image.fill(background)
    painter = QPainter(image)
    painter.drawImage(0, 0, captured)
    painter.end()
    if not image.save(str(output), "PNG"):
        raise RuntimeError("Unable to save Gallery snapshot: {0}".format(output))
    print("snapshot: {0}".format(output))


def _reset_current_page_scroll(window: GalleryWindow) -> None:
    record = window._pages.get(window.current_route)
    if record is None:
        return
    page = record[1]
    if not hasattr(page, "verticalScrollBar"):
        return
    scroll_bar = page.verticalScrollBar()
    signals_blocked = scroll_bar.signalsBlocked()
    scroll_bar.blockSignals(True)
    try:
        scroll_bar.setValue(scroll_bar.minimum())
        QApplication.processEvents()
        scroll_bar.setValue(scroll_bar.minimum())
    finally:
        scroll_bar.blockSignals(signals_blocked)
    # ScrollView keeps the logical QScrollBar in its container and paints a
    # separate floating overlay bar above the viewport. Native parity captures
    # wait for the same 250 ms route-settle interval, so force that painted
    # instance into the C++ resting state when initial layout started its timer.
    overlay_bar = page.findChild(
        fluentqt.ScrollBar,
        "fluentScrollViewFloatingVerticalBar",
    )
    if overlay_bar is not None:
        for animation in overlay_bar.findChildren(QPropertyAnimation):
            if bytes(animation.propertyName()) == b"opacity":
                animation.stop()
        overlay_bar.setOpacity(0.0)


def _pending_gallery_network_requests(window: GalleryWindow) -> int:
    """Return pending sample-image requests on the visible Gallery route."""

    record = window._pages.get(window.current_route)
    if record is None:
        return 0
    page = record[1]
    widgets = [page] + page.findChildren(QWidget)
    return sum(
        max(0, int(getattr(widget, "_gallery_photo_network_pending", 0)))
        for widget in widgets
    )


def _freeze_current_page_animations(window: GalleryWindow) -> None:
    """Set continuously animated sample controls to a stable capture phase."""

    record = window._pages.get(window.current_route)
    if record is None:
        return
    page = record[1]
    for ring in page.findChildren(fluentqt.ProgressRing):
        ring.setAnimationEnabled(False)
    for shimmer in page.findChildren(fluentqt.Shimmer):
        shimmer.setAnimationEnabled(False)
        shimmer.setShimmerProgress(0.42)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    automated = bool(
        args.snapshot is not None
        or args.snapshot_dir is not None
        or args.report is not None
        or args.verify_catalog
        or args.walk_routes
    )
    QCoreApplication.setApplicationName(APPLICATION_NAME)
    QCoreApplication.setOrganizationName(ORGANIZATION_NAME)
    QCoreApplication.setApplicationVersion(fluentqt.__version__)
    fluentqt.prepare_high_dpi_application()
    app = QApplication.instance() or QApplication(sys.argv[:1])
    app.setProperty("fluentqtGalleryAutomated", automated)
    if sys.platform.startswith("linux"):
        QGuiApplication.setDesktopFileName(APPLICATION_ID)
    app.setQuitOnLastWindowClosed(False)
    single_instance = None
    activation_pending = False

    def remember_activation() -> None:
        nonlocal activation_pending
        activation_pending = True

    if not automated:
        single_instance = GallerySingleInstance(APPLICATION_ID, app)
        single_instance.activationRequested.connect(remember_activation)
        instance_result = single_instance.start()
        if instance_result == StartResult.ExistingInstanceNotified:
            return 0
        if instance_result == StartResult.Error:
            print(
                "FluentQt Gallery single-instance startup failed: {0}".format(
                    single_instance.error_string
                ),
                file=sys.stderr,
            )
            return 1

    # Import the Gallery catalog, pages, and sample builders only after the
    # per-user lock is acquired. A second launch now reaches the existing
    # instance handshake without paying the cost of constructing the complete
    # Gallery module graph first.
    from .application_controller import GalleryApplicationController
    from .catalog import ENTRIES
    from .settings import (
        CloseBehavior,
        NavigationStyle,
        ThemeMode,
        gallery_settings,
    )
    from .visual import app_icon
    from .window import GalleryWindow
    from .window_placement import GalleryWindowPlacement

    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
    app.setWindowIcon(app_icon())

    settings = gallery_settings()
    if automated:
        # Native visual tests construct GallerySettings without the production
        # app identity, so persisted user choices are intentionally absent.
        # Keep Python verification equally deterministic while leaving normal
        # interactive launches persistent.
        settings.navigation_style = NavigationStyle.Auto
        settings.window_effect = 1
        settings.close_behavior = CloseBehavior.Tray
        settings.apply_user_theme()
    if args.theme == "light":
        settings.theme_mode = ThemeMode.Light
        settings.apply_theme_mode()
    elif args.theme == "dark":
        settings.theme_mode = ThemeMode.Dark
        settings.apply_theme_mode()

    window = GalleryWindow(startup_visuals=not automated)
    placement = None
    application_controller = None
    restore_maximized = False
    if not automated:
        placement = GalleryWindowPlacement(window, settings, app)
        application_controller = GalleryApplicationController(
            window, settings, app
        )
        if single_instance is not None:
            single_instance.activationRequested.disconnect(remember_activation)
            single_instance.activationRequested.connect(
                application_controller.restore_window
            )
            if activation_pending:
                QTimer.singleShot(0, application_controller.restore_window)
        restore_maximized = placement.restore()
    if args.window_size is not None:
        if args.snapshot is not None or args.snapshot_dir is not None:
            window.setFixedSize(args.window_size)
        else:
            window.resize(args.window_size)
    if args.snapshot is not None or args.snapshot_dir is not None:
        window.setBackdropEffect(fluentqt.BackdropEffect.Solid)
    route = normalize_route(args.route)
    failures = runtime_catalog_errors()
    visited_routes = 0

    if restore_maximized and args.window_size is None:
        window.showMaximized()
    else:
        window.show()

    try:
        window.navigate(route, record_history=route != "home")
    except Exception as error:
        failures.append(
            "{0}: {1}: {2}".format(route, type(error).__name__, error)
        )

    if automated:
        final_settle_timer = QElapsedTimer()
        final_settle_timer.start()

        def finish_verification() -> None:
            if (
                args.snapshot is not None
                and _pending_gallery_network_requests(window) > 0
                and final_settle_timer.elapsed()
                < _NETWORK_SETTLE_TIMEOUT_MS
            ):
                QTimer.singleShot(
                    _NETWORK_SETTLE_POLL_MS, finish_verification
                )
                return
            try:
                payload = report_payload(window, failures, visited_routes)
                if args.snapshot is not None:
                    _freeze_current_page_animations(window)
                    _reset_current_page_scroll(window)
                    save_snapshot(window, args.snapshot, args.window_size)
                if args.report is not None:
                    save_report(args.report, payload)
                if failures:
                    for failure in failures:
                        print(
                            "gallery verification failed: {0}".format(failure),
                            file=sys.stderr,
                        )
                    app.exit(2)
                else:
                    print(
                        "FluentQt Python Gallery verified {0} components across {1} routes".format(
                            len(ENTRIES), visited_routes
                        )
                    )
                    app.exit(0)
            except Exception as error:
                print(
                    "gallery verification failed: {0}: {1}".format(
                        type(error).__name__, error
                    ),
                    file=sys.stderr,
                )
                app.exit(2)

        if args.walk_routes or args.snapshot_dir is not None:
            verification_routes = window.all_route_ids()
        elif args.verify_catalog:
            verification_routes = tuple(entry.route_id for entry in ENTRIES)
        else:
            verification_routes = ()
        route_iterator = iter(verification_routes)

        def visit_next_route() -> None:
            nonlocal visited_routes
            try:
                current_route = next(route_iterator)
            except StopIteration:
                try:
                    window.navigate(route, record_history=False, animated=False)
                except Exception as error:
                    failures.append(
                        "{0}: {1}: {2}".format(
                            route, type(error).__name__, error
                        )
                    )
                QTimer.singleShot(0, finish_verification)
                return

            failure = window.visit_route(
                current_route,
                process_events=False,
                # The C++ all-route visual check selects routes through the
                # normal navigation path, so every non-home snapshot exposes
                # the same title-bar back affordance.
                record_history=args.snapshot_dir is not None,
            )
            visited_routes += 1
            if failure is not None:
                failures.append(failure)

            if args.snapshot_dir is not None and failure is None:
                _freeze_current_page_animations(window)

            route_settle_timer = QElapsedTimer()
            route_settle_timer.start()

            def finish_route() -> None:
                if (
                    args.snapshot_dir is not None
                    and _pending_gallery_network_requests(window) > 0
                    and route_settle_timer.elapsed()
                    < _NETWORK_SETTLE_TIMEOUT_MS
                ):
                    QTimer.singleShot(
                        _NETWORK_SETTLE_POLL_MS, finish_route
                    )
                    return
                if args.snapshot_dir is not None and failure is None:
                    _reset_current_page_scroll(window)
                    suffix = (
                        "dark"
                        if fluentqt.current_theme() == fluentqt.Theme.Dark
                        else "light"
                    )
                    save_snapshot(
                        window,
                        args.snapshot_dir
                        / "parity-{0}-{1}.png".format(current_route, suffix),
                        args.window_size,
                    )
                QTimer.singleShot(0, visit_next_route)

            settle_ms = 500 if args.snapshot_dir is not None else _ROUTE_SETTLE_MS
            QTimer.singleShot(settle_ms, finish_route)

        def verify_routes() -> None:
            nonlocal visited_routes
            try:
                if (
                    args.walk_routes
                    or args.verify_catalog
                    or args.snapshot_dir is not None
                ):
                    QTimer.singleShot(0, visit_next_route)
                    return
                else:
                    visited_routes = 1
            except Exception as error:
                failures.append(
                    "{0}: {1}".format(type(error).__name__, error)
                )
            QTimer.singleShot(0, finish_verification)

        QTimer.singleShot(300, verify_routes)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    exit_code = app.exec()
    if placement is not None:
        placement.save_now()
    if single_instance is not None:
        single_instance.close()
    return exit_code


__all__ = [
    "main",
    "normalize_route",
    "parse_args",
    "report_payload",
    "runtime_catalog_errors",
]
