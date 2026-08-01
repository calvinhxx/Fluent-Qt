"""Interactive and snapshot acceptance example for DrawerView."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QTimer, Qt
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt DrawerView example."
    )
    parser.add_argument(
        "--snapshot",
        type=Path,
        help="Save the rendered window as a PNG and exit.",
    )
    parser.add_argument(
        "--auto-close-ms",
        type=int,
        default=0,
        help="Automatically close the interactive window after this delay.",
    )
    return parser.parse_args()


def create_drawer_content(
    drawer: fluentqt.DrawerView,
    ownership_name: str,
) -> QWidget:
    content = QWidget()
    layout = QVBoxLayout(content)
    layout.setContentsMargins(24, 24, 24, 24)
    layout.setSpacing(12)

    title = fluentqt.Label("Quick settings", content)
    title.setFluentTypography(fluentqt.FontRole.Title)
    layout.addWidget(title)

    policy = fluentqt.Label(
        "{0} content".format(ownership_name),
        content,
    )
    policy.setFluentTypography(fluentqt.FontRole.BodyStrong)
    layout.addWidget(policy)

    description = fluentqt.Label(
        "This is the native same-window overlay: no extra Qt window is "
        "created, and Python keeps the hosted QWidget wrapper alive.",
        content,
    )
    description.setWordWrap(True)
    layout.addWidget(description)

    for heading, detail in (
        ("Notifications", "Enabled for workspace updates"),
        ("Compact navigation", "Use the smaller side pane"),
        ("Cloud sync", "Last synchronized just now"),
    ):
        card = fluentqt.Card(content)
        card_layout = QVBoxLayout(card)
        card_layout.setContentsMargins(16, 12, 16, 12)
        card_layout.setSpacing(4)
        label = fluentqt.Label(heading, card)
        label.setFluentTypography(fluentqt.FontRole.BodyStrong)
        note = fluentqt.Label(detail, card)
        note.setFluentTypography(fluentqt.FontRole.Caption)
        card_layout.addWidget(label)
        card_layout.addWidget(note)
        layout.addWidget(card)

    layout.addStretch()
    close_button = fluentqt.Button("Done", content)
    close_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    close_button.clicked.connect(drawer.close)
    layout.addWidget(close_button, 0, Qt.AlignRight)
    return content


def build_showcase(snapshot_mode: bool) -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 DrawerView")
    window.resize(920, 580)

    page = QWidget()
    page_layout = QVBoxLayout(page)
    page_layout.setContentsMargins(36, 32, 36, 32)
    page_layout.setSpacing(16)

    title = fluentqt.Label("DrawerView from Python", page)
    title.setFluentTypography(fluentqt.FontRole.TitleLarge)
    page_layout.addWidget(title)

    subtitle = fluentqt.Label(
        "The Python facade preserves DrawerView's native overlay, close "
        "policy, animation, and explicit QWidget ownership semantics.",
        page,
    )
    subtitle.setWordWrap(True)
    page_layout.addWidget(subtitle)

    summary = fluentqt.Card(page)
    summary_layout = QVBoxLayout(summary)
    summary_layout.setContentsMargins(24, 22, 24, 22)
    summary_layout.setSpacing(10)
    summary_title = fluentqt.Label("Same-window overlay", summary)
    summary_title.setFluentTypography(fluentqt.FontRole.Subtitle)
    summary_text = fluentqt.Label(
        "The drawer attaches to this Window, places its dim scrim below the "
        "panel, and closes on outside press or Escape.",
        summary,
    )
    summary_text.setWordWrap(True)
    summary_layout.addWidget(summary_title)
    summary_layout.addWidget(summary_text)
    page_layout.addWidget(summary)

    policies = QHBoxLayout()
    policies.setSpacing(12)
    for name, detail in (
        ("Owned", "Deleted with the drawer"),
        ("Borrowed", "Detached on release"),
        ("Reparented", "Restored to its parent"),
    ):
        card = fluentqt.Card(page)
        card_layout = QVBoxLayout(card)
        card_layout.setContentsMargins(18, 16, 18, 16)
        card_layout.setSpacing(4)
        heading = fluentqt.Label(name, card)
        heading.setFluentTypography(fluentqt.FontRole.BodyStrong)
        note = fluentqt.Label(detail, card)
        note.setWordWrap(True)
        note.setFluentTypography(fluentqt.FontRole.Caption)
        card_layout.addWidget(heading)
        card_layout.addWidget(note)
        policies.addWidget(card, 1)
    page_layout.addLayout(policies)
    page_layout.addStretch()

    action_row = QHBoxLayout()
    status = fluentqt.Label("Owned content installed", page)
    open_button = fluentqt.Button("Open drawer", page)
    open_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    action_row.addWidget(status)
    action_row.addStretch()
    action_row.addWidget(open_button)
    page_layout.addLayout(action_row)

    window.setContentWidget(page)

    drawer = fluentqt.DrawerView(page)
    drawer.setEdge(fluentqt.DrawerView.DrawerEdge.Right)
    drawer.setDrawerLength(350)
    drawer.setAnimationEnabled(not snapshot_mode)
    drawer.setClosePolicy(
        fluentqt.DrawerView.CloseFlag.CloseOnPressOutside
        | fluentqt.DrawerView.CloseFlag.CloseOnEscape
    )
    drawer.setOwnedContentWidget(create_drawer_content(drawer, "Owned"))

    open_button.clicked.connect(drawer.open)
    drawer.opened.connect(lambda: status.setText("Drawer opened"))
    drawer.closed.connect(lambda: status.setText("Drawer closed"))

    # Keep the Python facade visible for interactive inspection and ensure the
    # snapshot captures the fully opened native geometry.
    window._drawer_view = drawer
    if snapshot_mode:
        QTimer.singleShot(0, drawer.open)
    return window


def main() -> int:
    args = parse_args()
    app = QApplication.instance() or QApplication(sys.argv)
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window = build_showcase(args.snapshot is not None)
    window.show()

    if args.snapshot is not None:
        snapshot_path = args.snapshot.expanduser().resolve()

        def capture() -> None:
            snapshot_path.parent.mkdir(parents=True, exist_ok=True)
            image = window.grab()
            if image.isNull() or not image.save(str(snapshot_path), "PNG"):
                print(
                    "Unable to save snapshot: {0}".format(snapshot_path),
                    file=sys.stderr,
                )
                app.exit(2)
                return
            print("snapshot: {0}".format(snapshot_path))
            app.quit()

        QTimer.singleShot(300, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
