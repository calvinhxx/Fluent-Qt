"""Interactive and snapshot acceptance example for Fluent command surfaces."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QTimer
from PySide6.QtGui import QAction
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt command-surface example."
    )
    parser.add_argument(
        "--snapshot",
        type=Path,
        help="Open the same-window command flyout, save a PNG, and exit.",
    )
    parser.add_argument(
        "--auto-close-ms",
        type=int,
        default=0,
        help="Automatically close the interactive window after this delay.",
    )
    return parser.parse_args()


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 command surfaces")
    window.resize(900, 600)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(36, 28, 36, 30)
    root.setSpacing(14)

    title = fluentqt.Label("Command surfaces from Python", page)
    title.setFluentTypography(fluentqt.FontRole.TitleLarge)
    root.addWidget(title)

    summary = fluentqt.Label(
        "FluentMenuBar, CommandBar, and CommandBarFlyout keep their native "
        "painting and keyboard behavior. Python supplies shared QAction "
        "commands while the binding retains wrappers without taking QObject "
        "ownership.",
        page,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)

    menu_bar = fluentqt.FluentMenuBar(page)
    menu_bar.setFontStyle(fluentqt.FontRole.BodyStrong)
    for menu_name, item_names in (
        ("File", ("New", "Open", "Save")),
        ("Edit", ("Undo", "Redo")),
        ("View", ("Compact", "Comfortable")),
    ):
        menu = fluentqt.FluentMenu(menu_name, menu_bar)
        for item_name in item_names:
            menu.addAction(item_name)
        menu_bar.addMenu(menu)
    root.addWidget(menu_bar)
    root.addWidget(fluentqt.Divider(page))

    stage = fluentqt.Card(page)
    stage.setMinimumHeight(300)
    stage_layout = QVBoxLayout(stage)
    stage_layout.setContentsMargins(24, 22, 24, 24)
    stage_layout.setSpacing(16)

    stage_title = fluentqt.Label("Shared QAction command model", stage)
    stage_title.setFluentTypography(fluentqt.FontRole.Subtitle)
    stage_layout.addWidget(stage_title)

    command_bar = fluentqt.CommandBar(stage)
    command_bar.setLabelPosition(
        fluentqt.CommandBar.LabelPosition.Right
    )
    command_bar.setDynamicOverflowEnabled(True)
    command_bar.setBackgroundVisible(True)
    stage_layout.addWidget(command_bar)
    stage_layout.addWidget(fluentqt.Divider(stage))

    status = fluentqt.Label("Choose a command", stage)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    stage_layout.addWidget(status)
    stage_layout.addStretch()

    command_row = QHBoxLayout()
    command_row.addStretch()
    flyout_button = fluentqt.Button("Open command flyout", stage)
    flyout_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    command_row.addWidget(flyout_button)
    stage_layout.addLayout(command_row)
    root.addWidget(stage)

    actions = []
    for action_name in ("Cut", "Copy", "Paste", "Rename", "Share"):
        action = QAction(action_name, window)
        action.triggered.connect(
            lambda _checked=False, name=action_name: status.setText(
                "Command: {0}".format(name)
            )
        )
        command_bar.addPrimaryAction(action)
        actions.append(action)

    delete_action = QAction("Delete", window)
    delete_action.triggered.connect(
        lambda: status.setText("Command: Delete")
    )
    command_bar.addSecondaryAction(delete_action)
    actions.append(delete_action)

    command_flyout = fluentqt.CommandBarFlyout(window)
    command_flyout.setAnimationEnabled(False)
    command_flyout.setExitAnimationEnabled(False)
    command_flyout.setAlwaysExpanded(True)
    command_flyout.setShowMode(
        fluentqt.CommandBarFlyout.ShowMode.Standard
    )
    for action in actions[:3]:
        command_flyout.addPrimaryAction(action)
    for action in actions[3:]:
        command_flyout.addSecondaryAction(action)

    def open_flyout() -> None:
        status.setText("Command flyout opened")
        command_flyout.showAt(flyout_button)

    flyout_button.clicked.connect(open_flyout)
    window.setContentWidget(page)
    window._command_actions = tuple(actions)
    window._command_flyout = command_flyout
    window._open_command_flyout = open_flyout
    return window


def main() -> int:
    args = parse_args()
    app = QApplication.instance() or QApplication(sys.argv)
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window = build_showcase()
    window.show()

    if args.snapshot is not None:
        snapshot_path = args.snapshot.expanduser().resolve()

        def open_surface() -> None:
            window._open_command_flyout()

        def capture() -> None:
            flyout = window._command_flyout
            if not flyout.isOpen() or flyout.window() is not window:
                print(
                    "CommandBarFlyout did not open in the owning window",
                    file=sys.stderr,
                )
                app.exit(2)
                return
            snapshot_path.parent.mkdir(parents=True, exist_ok=True)
            if not window.grab().save(str(snapshot_path), "PNG"):
                print(
                    "Unable to save snapshot: {0}".format(snapshot_path),
                    file=sys.stderr,
                )
                app.exit(2)
                return
            print("snapshot: {0}".format(snapshot_path))
            app.quit()

        QTimer.singleShot(80, open_surface)
        QTimer.singleShot(500, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
