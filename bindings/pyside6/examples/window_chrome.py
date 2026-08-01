"""Interactive and native-platform acceptance for Fluent window chrome."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QEventLoop, QTimer, Qt, qVersion
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget
from shiboken6 import Shiboken


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or verify FluentQt's native Window and TitleBar."
    )
    parser.add_argument(
        "--snapshot",
        type=Path,
        help="Save the visible client surface as a PNG.",
    )
    parser.add_argument(
        "--report",
        type=Path,
        help="Write the resolved platform/backdrop contract as JSON.",
    )
    parser.add_argument(
        "--verify-native",
        action="store_true",
        help="Reject offscreen/minimal plugins and validate native chrome.",
    )
    parser.add_argument(
        "--require-platform-backdrop",
        action="store_true",
        help="Require Mica or Acrylic to use an OS/compositor backend.",
    )
    parser.add_argument(
        "--auto-close-ms",
        type=int,
        default=0,
        help="Automatically close the interactive window after this delay.",
    )
    return parser.parse_args()


def wait_for_events(duration_ms: int = 80) -> None:
    loop = QEventLoop()
    QTimer.singleShot(duration_ms, loop.quit)
    loop.exec()


def text_value(value) -> str:
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return str(value)


def enum_name(value) -> str:
    name = getattr(value, "name", None)
    return text_value(name) if name is not None else str(int(value))


def json_default(value) -> str:
    if isinstance(value, bytes):
        return text_value(value)
    raise TypeError(
        "Object of type {0} is not JSON serializable".format(
            type(value).__name__
        )
    )


def rect_report(rect) -> dict[str, int]:
    return {
        "x": rect.x(),
        "y": rect.y(),
        "width": rect.width(),
        "height": rect.height(),
    }


def state_report(state) -> dict[str, object]:
    return {
        "requested_effect": enum_name(state.requestedEffect),
        "effective_effect": enum_name(state.effectiveEffect),
        "backend": enum_name(state.backend),
        "fidelity": enum_name(state.fidelity),
        "surface_mode": enum_name(state.surfaceMode),
        "platform_applied": bool(state.platformApplied),
        "reason": text_value(state.reason),
    }


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setObjectName("FluentQtPySideWindowChrome")
    window.setWindowTitle("FluentQt PySide6 window chrome")
    window.resize(900, 600)
    window.setCustomWindowChromeEnabled(True)
    window.setChromeInteractive(True)
    window.setCaptionButtonToolTips(
        "Minimize",
        "Maximize",
        "Close",
        "Restore",
    )
    window.setCaptionButtonAccessibleNames(
        "Minimize window",
        "Maximize window",
        "Close window",
        "Restore window",
    )

    title_bar = window.titleBar()
    title_bar.setObjectName("FluentQtPySideTitleBar")
    title_bar.setTitleBarHeight(44)
    title_content = QWidget()
    title_layout = QHBoxLayout(title_content)
    title_layout.setContentsMargins(12, 0, 12, 0)
    title_layout.setSpacing(10)
    app_title = fluentqt.Label("FluentQt · Python", title_content)
    app_title.setFluentTypography(fluentqt.FontRole.BodyStrong)
    title_layout.addWidget(app_title)
    title_layout.addStretch()
    title_hint = fluentqt.Label("Native Window + TitleBar", title_content)
    title_hint.setFluentTypography(fluentqt.FontRole.Caption)
    title_layout.addWidget(title_hint)
    title_bar.setContentWidget(title_content)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(36, 30, 36, 34)
    root.setSpacing(16)

    heading = fluentqt.Label("Window chrome compatibility", page)
    heading.setFluentTypography(fluentqt.FontRole.TitleLarge)
    root.addWidget(heading)

    summary = fluentqt.Label(
        "Switch the requested backdrop while the native FluentQt window "
        "publishes the backend, fidelity, and surface mode actually in use.",
        page,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)

    controls = QHBoxLayout()
    effect_buttons = []
    for label, effect in (
        ("Solid", fluentqt.BackdropEffect.Solid),
        ("Mica", fluentqt.BackdropEffect.Mica),
        ("Acrylic", fluentqt.BackdropEffect.Acrylic),
    ):
        button = fluentqt.Button(label, page)
        controls.addWidget(button)
        effect_buttons.append((button, effect))
    controls.addStretch()
    root.addLayout(controls)

    surface = fluentqt.Card(page)
    surface.setMinimumHeight(260)
    surface_layout = QVBoxLayout(surface)
    surface_layout.setContentsMargins(24, 22, 24, 24)
    surface_layout.setSpacing(12)
    state_title = fluentqt.Label("Resolved backdrop", surface)
    state_title.setFluentTypography(fluentqt.FontRole.Subtitle)
    surface_layout.addWidget(state_title)
    state_label = fluentqt.Label("", surface)
    state_label.setWordWrap(True)
    surface_layout.addWidget(state_label)
    geometry_label = fluentqt.Label("", surface)
    geometry_label.setWordWrap(True)
    surface_layout.addWidget(geometry_label)
    surface_layout.addStretch()
    root.addWidget(surface)

    def refresh_state(*_args) -> None:
        state = window.backdropState()
        capabilities = window.backdropCapabilities()
        for button, effect in effect_buttons:
            button.setFluentStyle(
                fluentqt.Button.ButtonStyle.Accent
                if state.requestedEffect == effect
                else fluentqt.Button.ButtonStyle.Standard
            )
        state_label.setText(
            "Requested: {0}  ·  Effective: {1}\n"
            "Backend: {2}  ·  Fidelity: {3}  ·  Surface: {4}\n"
            "Provider: {5}  ·  Reason: {6}".format(
                enum_name(state.requestedEffect),
                enum_name(state.effectiveEffect),
                enum_name(state.backend),
                enum_name(state.fidelity),
                enum_name(state.surfaceMode),
                (
                    text_value(capabilities.provider)
                    if capabilities.provider
                    else "none"
                ),
                text_value(state.reason) if state.reason else "none",
            )
        )
        frame = window.chromeFrameRect()
        geometry_label.setText(
            "TitleBar: {0} px  ·  Insets: {1}/{2} px  ·  "
            "Chrome frame: {3}×{4}".format(
                title_bar.titleBarHeight(),
                title_bar.systemReservedLeadingWidth(),
                title_bar.systemReservedTrailingWidth(),
                frame.width(),
                frame.height(),
            )
        )

    for button, effect in effect_buttons:
        button.clicked.connect(
            lambda _checked=False, requested=effect: window.setBackdropEffect(
                requested
            )
        )
    window.backdropStateChanged.connect(refresh_state)
    window.backdropEffectChanged.connect(refresh_state)
    title_bar.chromeGeometryChanged.connect(refresh_state)
    window.setBackdropEffect(fluentqt.BackdropEffect.Mica)
    window.setContentWidget(page)
    refresh_state()

    window._window_chrome_title_content = title_content
    window._window_chrome_page = page
    window._window_chrome_state_label = state_label
    window._window_chrome_refresh_state = refresh_state
    return window


def validate_material_state(effect, state) -> None:
    if state.requestedEffect != effect or state.effectiveEffect != effect:
        raise AssertionError("Backdrop lost its requested/effective effect")
    if not state.reason:
        raise AssertionError("Backdrop state did not publish a diagnostic reason")

    if state.platformApplied:
        if state.backend in (
            fluentqt.BackdropBackend.Solid,
            fluentqt.BackdropBackend.PaintedMaterial,
        ):
            raise AssertionError("Platform-applied backdrop reports fallback backend")
        if state.fidelity not in (
            fluentqt.BackdropFidelity.Composited,
            fluentqt.BackdropFidelity.Native,
        ):
            raise AssertionError("Platform-applied backdrop reports wrong fidelity")
        if (
            state.surfaceMode
            != fluentqt.BackdropSurfaceMode.CompositedTransparent
        ):
            raise AssertionError("Platform backdrop is not composited transparent")
    else:
        if state.backend != fluentqt.BackdropBackend.PaintedMaterial:
            raise AssertionError("Rejected backdrop did not use painted fallback")
        if state.fidelity != fluentqt.BackdropFidelity.Emulated:
            raise AssertionError("Painted fallback did not report emulated fidelity")
        if state.surfaceMode != fluentqt.BackdropSurfaceMode.PaintedOpaque:
            raise AssertionError("Painted fallback is not opaque")


def verify_native_window(window: fluentqt.Window) -> dict[str, object]:
    platform_name = QApplication.platformName().lower()
    if "offscreen" in platform_name or "minimal" in platform_name:
        raise AssertionError(
            "Native verification requires cocoa, windows, xcb, or wayland; "
            "found {0}".format(platform_name)
        )
    expected_plugins = {
        "darwin": ("cocoa",),
        "win32": ("windows",),
        "linux": ("xcb", "wayland"),
    }
    if not platform_name.startswith(expected_plugins.get(sys.platform, ())):
        raise AssertionError(
            "Unexpected native platform plugin for {0}: {1}".format(
                sys.platform,
                platform_name,
            )
        )
    if not window.isVisible() or window.windowHandle() is None:
        raise AssertionError("Window did not create a visible native handle")
    if int(window.winId()) == 0:
        raise AssertionError("Window has a null native id")

    title_bar = window.titleBar()
    if not Shiboken.isValid(title_bar) or title_bar.window() is not window:
        raise AssertionError("Window TitleBar is not a valid Qt-owned child")
    if title_bar.contentWidget() is not window._window_chrome_title_content:
        raise AssertionError("TitleBar lost its Python-installed content")
    if window.contentWidget() is not window._window_chrome_page:
        raise AssertionError("Window lost its Python-installed content")
    if (
        not title_bar.isVisible()
        or title_bar.height() != title_bar.titleBarHeight()
    ):
        raise AssertionError("TitleBar is not visible at its configured height")
    if not window.customWindowChromeEnabled() or not window.isChromeInteractive():
        raise AssertionError("Custom interactive chrome was not enabled")

    original_size = window.size()
    window.resize(original_size.width() + 40, original_size.height() + 24)
    wait_for_events()
    if window.size() == original_size:
        raise AssertionError("Native window resize did not reach the QWidget")
    window.resize(original_size)
    window.prepareForNativeRestore()
    window.requestForegroundActivation()
    wait_for_events()

    states = {}
    platform_materials = []
    for effect in (
        fluentqt.BackdropEffect.Solid,
        fluentqt.BackdropEffect.Mica,
        fluentqt.BackdropEffect.Acrylic,
    ):
        window.setBackdropEffect(effect)
        window.reapplySystemBackdrop()
        wait_for_events(120)
        state = window.backdropState()
        if effect == fluentqt.BackdropEffect.Solid:
            if (
                state.requestedEffect != effect
                or state.backend != fluentqt.BackdropBackend.Solid
                or state.fidelity != fluentqt.BackdropFidelity.Solid
                or state.surfaceMode
                != fluentqt.BackdropSurfaceMode.SolidOpaque
                or state.platformApplied
            ):
                raise AssertionError("Native Solid backdrop contract is inconsistent")
        else:
            validate_material_state(effect, state)
            if state.platformApplied:
                platform_materials.append(enum_name(effect))
        states[enum_name(effect)] = state_report(state)

    # QWidget.grab() cannot composite an OS-owned transparent backdrop into the
    # client image. End native verification on Solid so an optional snapshot is
    # readable; the report above still proves every requested material state.
    window.setBackdropEffect(fluentqt.BackdropEffect.Solid)
    window.reapplySystemBackdrop()
    wait_for_events(120)
    window._window_chrome_refresh_state()
    wait_for_events(120)

    capabilities = window.backdropCapabilities()
    return {
        "platform": sys.platform,
        "platform_plugin": platform_name,
        "qt_version": qVersion(),
        "native_window_id": int(window.winId()),
        "custom_chrome": window.customWindowChromeEnabled(),
        "chrome_interactive": window.isChromeInteractive(),
        "chrome_frame": rect_report(window.chromeFrameRect()),
        "title_bar_geometry": rect_report(title_bar.geometry()),
        "title_bar_height": title_bar.titleBarHeight(),
        "system_reserved_leading_width": (
            title_bar.systemReservedLeadingWidth()
        ),
        "system_reserved_trailing_width": (
            title_bar.systemReservedTrailingWidth()
        ),
        "capabilities": {
            "alpha_surface_supported": bool(
                capabilities.alphaSurfaceSupported
            ),
            "native_mica": bool(capabilities.nativeMica),
            "native_acrylic": bool(capabilities.nativeAcrylic),
            "compositor_blur": bool(capabilities.compositorBlur),
            "provider": text_value(capabilities.provider),
        },
        "platform_materials": platform_materials,
        "states": states,
    }


def save_outputs(
    window: fluentqt.Window,
    snapshot: Path | None,
    report: Path | None,
    payload: dict[str, object],
) -> None:
    if snapshot is not None:
        snapshot_path = snapshot.expanduser().resolve()
        snapshot_path.parent.mkdir(parents=True, exist_ok=True)
        if not window.grab().save(str(snapshot_path), "PNG"):
            raise RuntimeError("Unable to save snapshot: {0}".format(snapshot_path))
        print("snapshot: {0}".format(snapshot_path))
    if report is not None:
        report_path = report.expanduser().resolve()
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(
            json.dumps(
                payload,
                default=json_default,
                indent=2,
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )
        print("report: {0}".format(report_path))


def main() -> int:
    args = parse_args()
    app = QApplication.instance() or QApplication(sys.argv)
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window = build_showcase()
    window.show()

    if args.verify_native or args.snapshot is not None or args.report is not None:
        def verify_and_exit() -> None:
            try:
                if args.verify_native:
                    payload = verify_native_window(window)
                else:
                    wait_for_events(120)
                    payload = {
                        "platform": sys.platform,
                        "platform_plugin": QApplication.platformName(),
                        "qt_version": qVersion(),
                        "state": state_report(window.backdropState()),
                    }
                if args.require_platform_backdrop and not payload.get(
                    "platform_materials"
                ):
                    raise AssertionError(
                        "No Mica/Acrylic platform backend was successfully applied"
                    )
                save_outputs(window, args.snapshot, args.report, payload)
                app.exit(0)
            except Exception as error:
                print(
                    "window chrome verification failed: {0}".format(error),
                    file=sys.stderr,
                )
                app.exit(2)

        QTimer.singleShot(500, verify_and_exit)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
