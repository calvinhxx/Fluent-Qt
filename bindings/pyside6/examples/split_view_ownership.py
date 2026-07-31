"""Interactive and snapshot acceptance example for SplitView ownership."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt SplitView example."
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


def create_pane(title_text: str, policy_text: str, detail: str) -> QWidget:
    pane = fluentqt.Card()
    pane.setObjectName(title_text)
    layout = QVBoxLayout(pane)
    layout.setContentsMargins(18, 20, 18, 20)
    layout.setSpacing(8)

    title = fluentqt.Label(title_text, pane)
    title.setFluentTypography(fluentqt.FontRole.Subtitle)
    layout.addWidget(title)

    policy = fluentqt.Label(policy_text, pane)
    policy.setFluentTypography(fluentqt.FontRole.BodyStrong)
    layout.addWidget(policy)

    description = fluentqt.Label(detail, pane)
    description.setWordWrap(True)
    layout.addWidget(description)
    layout.addStretch()
    return pane


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 SplitView")
    window.resize(900, 520)

    content = QWidget()
    root = QVBoxLayout(content)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = fluentqt.Label("SplitView pane ownership", content)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = fluentqt.Label(
        "Drag either native handle to resize the panes. Each Python QWidget "
        "also carries an explicit release policy.",
        content,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(fluentqt.Divider(content))

    split_view = fluentqt.SplitView(content)
    split_view.setHandleWidth(8)
    split_view.setHandleVisualThickness(2)

    owned = create_pane(
        "Navigation",
        "Owned pane",
        "Deleted by SplitView when removed.",
    )
    borrowed = create_pane(
        "Workspace",
        "Borrowed fill pane",
        "Detached as a parentless QWidget when released.",
    )
    reparented = create_pane(
        "Inspector",
        "Reparented pane",
        "Restored to its original QWidget parent when released.",
    )
    reparented.setParent(content)

    split_view.addOwnedPane(
        owned,
        fluentqt.SplitViewPaneOptions(120, 170, 240),
    )
    split_view.addBorrowedPane(
        borrowed,
        fluentqt.SplitViewPaneOptions(220, 390, 720, True),
    )
    split_view.addReparentedPane(
        reparented,
        fluentqt.SplitViewPaneOptions(140, 190, 280),
    )
    root.addWidget(split_view, 1)

    status_row = QHBoxLayout()
    status = fluentqt.Label(
        "3 panes · Owned / Borrowed fill / Reparented",
        content,
    )
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    reset = fluentqt.Button("Reset sizes", content)
    reset.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    status_row.addWidget(status)
    status_row.addStretch()
    status_row.addWidget(reset)
    root.addLayout(status_row)

    def reset_sizes() -> None:
        split_view.setPanePreferredSize(0, 170)
        split_view.setPanePreferredSize(1, 390)
        split_view.setPanePreferredSize(2, 190)
        split_view.setFillPaneIndex(1)

    reset.clicked.connect(reset_sizes)
    window.setContentWidget(content)
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

        QTimer.singleShot(250, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
