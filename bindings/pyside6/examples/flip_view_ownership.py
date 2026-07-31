"""Interactive and snapshot acceptance example for FlipView ownership."""

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
        description="Open or snapshot the FluentQt FlipView example."
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


def create_page(title_text: str, policy_text: str, detail: str) -> QWidget:
    page = fluentqt.Card()
    page.setObjectName(title_text)
    layout = QVBoxLayout(page)
    layout.setContentsMargins(34, 30, 34, 30)
    layout.setSpacing(10)

    title = fluentqt.Label(title_text, page)
    title.setFluentTypography(fluentqt.FontRole.Title)
    layout.addWidget(title)

    policy = fluentqt.Label(policy_text, page)
    policy.setFluentTypography(fluentqt.FontRole.BodyStrong)
    layout.addWidget(policy)

    description = fluentqt.Label(detail, page)
    description.setWordWrap(True)
    layout.addWidget(description)
    layout.addStretch()
    return page


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 FlipView")
    window.resize(720, 540)

    content = QWidget()
    root = QVBoxLayout(content)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = fluentqt.Label("FlipView page ownership", content)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = fluentqt.Label(
        "The Python facade keeps native navigation and rendering while every "
        "hosted QWidget has an explicit release policy.",
        content,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(fluentqt.Divider(content))

    flip_view = fluentqt.FlipView(content)
    owned = create_page(
        "Owned page",
        "Deleted when released",
        "Use addOwnedPage() for content whose lifetime belongs entirely to "
        "the carousel.",
    )
    borrowed = create_page(
        "Borrowed page",
        "Detached when released",
        "Use addBorrowedPage() when the caller keeps the page and wants it "
        "returned as a parentless QWidget.",
    )
    reparented = create_page(
        "Reparented page",
        "Restored to its original parent",
        "Use addReparentedPage() when a page temporarily moves from another "
        "QWidget hierarchy into the carousel.",
    )
    reparented.setParent(content)

    flip_view.addOwnedPage(owned)
    flip_view.addBorrowedPage(borrowed)
    flip_view.addReparentedPage(reparented)
    root.addWidget(flip_view, 1)

    status = fluentqt.Label("Page 1 of 3 · Owned", content)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(status)

    actions = QHBoxLayout()
    previous = fluentqt.Button("Previous", content)
    next_button = fluentqt.Button("Next", content)
    next_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    actions.addStretch()
    actions.addWidget(previous)
    actions.addWidget(next_button)
    root.addLayout(actions)

    policy_names = ("Owned", "Borrowed", "Reparented")

    def update_state(index: int) -> None:
        status.setText(
            "Page {0} of {1} · {2}".format(
                index + 1,
                flip_view.pageCount(),
                policy_names[index],
            )
        )
        previous.setEnabled(index > 0)
        next_button.setEnabled(index + 1 < flip_view.pageCount())

    previous.clicked.connect(flip_view.goPrevious)
    next_button.clicked.connect(flip_view.goNext)
    flip_view.currentIndexChanged.connect(update_state)
    update_state(flip_view.currentIndex())

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
