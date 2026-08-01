"""Interactive and snapshot acceptance example for Toast and ToolTip."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QPoint, QTimer, Qt
from PySide6.QtGui import QAction, QPainter
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt status-overlay example."
    )
    parser.add_argument(
        "--snapshot",
        type=Path,
        help="Show both surfaces, save a PNG, and exit.",
    )
    parser.add_argument(
        "--auto-close-ms",
        type=int,
        default=0,
        help="Automatically close the interactive window after this delay.",
    )
    return parser.parse_args()


def build_showcase() -> tuple[fluentqt.Window, fluentqt.ToolTip]:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 Status Overlays")
    window.resize(880, 580)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(36, 30, 36, 30)
    root.setSpacing(14)

    title = fluentqt.Label("Toast and ToolTip from Python", page)
    title.setFluentTypography(fluentqt.FontRole.TitleLarge)
    root.addWidget(title)

    summary = fluentqt.Label(
        "Toast stays inside the owning top-level window and can update or "
        "stack by key. ToolTip is a native tooltip surface owned by its "
        "target. Both keep borrowed Python dependencies without taking "
        "their QObject ownership.",
        page,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(fluentqt.Divider(page))

    stage = fluentqt.Card(page)
    stage.setMinimumHeight(330)
    stage_layout = QVBoxLayout(stage)
    stage_layout.setContentsMargins(28, 24, 28, 26)
    stage_layout.setSpacing(16)

    stage_title = fluentqt.Label("Native status surfaces", stage)
    stage_title.setFluentTypography(fluentqt.FontRole.Subtitle)
    stage_layout.addWidget(stage_title)

    stage_text = fluentqt.Label(
        "Hover the first button for a target-owned ToolTip. Use the other "
        "buttons to create a managed notification or update one in place.",
        stage,
    )
    stage_text.setWordWrap(True)
    stage_layout.addWidget(stage_text)
    stage_layout.addStretch()

    actions = QHBoxLayout()
    actions.setSpacing(12)
    tooltip_target = fluentqt.Button("Hover for ToolTip", stage)
    success_button = fluentqt.Button("Show success", stage)
    success_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    update_button = fluentqt.Button("Update sync status", stage)
    actions.addWidget(tooltip_target)
    actions.addStretch()
    actions.addWidget(update_button)
    actions.addWidget(success_button)
    stage_layout.addLayout(actions)
    root.addWidget(stage)

    status = fluentqt.Label("Status overlays ready", page)
    status.setAlignment(Qt.AlignCenter)
    root.addWidget(status)
    window.setContentWidget(page)

    tooltip = fluentqt.ToolTip.attach(
        tooltip_target,
        "This native tooltip is attached by PySide6.",
        fluentqt.ToolTip.Placement.Above,
    )
    tooltip.setAnimationEnabled(False)

    open_action = QAction("Open", window)
    sync_iteration = {"value": 0}

    def show_success() -> fluentqt.Toast:
        toast = fluentqt.Toast.showToast(
            tooltip_target,
            "The Python binding is ready.",
            severity=fluentqt.Toast.Severity.Success,
            durationMs=0,
            placement=fluentqt.Toast.Placement.TopEnd,
        )
        toast.setTitle("Saved")
        toast.setAction(open_action)
        toast.setPauseOnHoverEnabled(True)
        status.setText("Managed success Toast shown")
        return toast

    def update_sync() -> fluentqt.Toast:
        sync_iteration["value"] += 1
        toast = fluentqt.Toast.showOrUpdateToast(
            tooltip_target,
            "sync-status",
            "Synced revision {0}".format(sync_iteration["value"]),
            severity=fluentqt.Toast.Severity.Informational,
            durationMs=0,
            placement=fluentqt.Toast.Placement.BottomEnd,
        )
        toast.setTitle("Cloud sync")
        status.setText("Sync Toast updated in place")
        return toast

    success_button.clicked.connect(show_success)
    update_button.clicked.connect(update_sync)
    open_action.triggered.connect(lambda: status.setText("Toast action invoked"))

    window._status_tooltip = tooltip
    window._status_toasts = []
    window._show_snapshot_surfaces = lambda: (
        window._status_toasts.append(show_success()),
        window._status_toasts.append(update_sync()),
        tooltip.setVisible(True),
    )
    return window, tooltip


def save_composited_snapshot(
    window: fluentqt.Window,
    tooltip: fluentqt.ToolTip,
    snapshot_path: Path,
) -> bool:
    """Capture the same-window Toast plus the separate native ToolTip."""

    image = window.grab().toImage()
    tooltip_image = tooltip.grab().toImage()
    if image.isNull() or tooltip_image.isNull():
        return False

    window_origin = window.mapToGlobal(QPoint(0, 0))
    tooltip_origin = tooltip.mapToGlobal(QPoint(0, 0))
    painter = QPainter(image)
    painter.drawImage(tooltip_origin - window_origin, tooltip_image)
    painter.end()
    snapshot_path.parent.mkdir(parents=True, exist_ok=True)
    return image.save(str(snapshot_path), "PNG")


def main() -> int:
    args = parse_args()
    app = QApplication.instance() or QApplication(sys.argv)
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window, tooltip = build_showcase()
    window.show()

    if args.snapshot is not None:
        snapshot_path = args.snapshot.expanduser().resolve()

        def open_surfaces() -> None:
            window._show_snapshot_surfaces()

        def capture() -> None:
            if not window._status_toasts or not tooltip.isVisible():
                print("Status overlays did not open", file=sys.stderr)
                app.exit(2)
                return
            if not save_composited_snapshot(window, tooltip, snapshot_path):
                print(
                    "Unable to save snapshot: {0}".format(snapshot_path),
                    file=sys.stderr,
                )
                app.exit(2)
                return
            print("snapshot: {0}".format(snapshot_path))
            app.quit()

        QTimer.singleShot(80, open_surfaces)
        QTimer.singleShot(500, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
