"""Interactive and snapshot acceptance example for StackView navigation."""

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
        description="Open or snapshot the FluentQt StackView example."
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


def create_page(number: int, detail: str) -> QWidget:
    page = fluentqt.Card()
    page.setObjectName("Page {0}".format(number))
    layout = QVBoxLayout(page)
    layout.setContentsMargins(28, 28, 28, 28)
    layout.setSpacing(10)
    title = fluentqt.Label("Page {0}".format(number), page)
    title.setFluentTypography(fluentqt.FontRole.Subtitle)
    layout.addWidget(title)
    description = fluentqt.Label(detail, page)
    description.setWordWrap(True)
    layout.addWidget(description)
    layout.addStretch()
    return page


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 StackView")
    window.resize(680, 500)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = fluentqt.Label("StackView navigation facade", page)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = fluentqt.Label(
        "Push, replace, and pop keep the native transition and signal "
        "behavior while each Python page has an explicit lifetime policy.",
        page,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(fluentqt.Divider(page))

    stack = fluentqt.StackView(page)
    stack.setTransitionDuration(180)
    stack.pushOwnedItem(
        create_page(
            1,
            "This initial page is Owned and is deleted when it leaves the "
            "navigation stack.",
        )
    )
    root.addWidget(stack, 1)

    status = fluentqt.Label("Page 1 · depth 1", page)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(status)

    actions = QHBoxLayout()
    push_button = fluentqt.Button("Push page", page)
    push_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    replace_button = fluentqt.Button("Replace", page)
    pop_button = fluentqt.Button("Pop", page)
    actions.addStretch()
    actions.addWidget(pop_button)
    actions.addWidget(replace_button)
    actions.addWidget(push_button)
    root.addLayout(actions)

    next_number = {"value": 2}

    def update_state() -> None:
        current = stack.currentItem()
        name = current.objectName() if current is not None else "None"
        status.setText("{0} · depth {1}".format(name, stack.depth()))
        pop_button.setEnabled(stack.canPop() and not stack.busy())
        replace_button.setEnabled(stack.depth() > 0 and not stack.busy())
        push_button.setEnabled(not stack.busy())

    def create_next(detail: str) -> QWidget:
        number = next_number["value"]
        next_number["value"] += 1
        return create_page(number, detail)

    def push_page() -> None:
        stack.pushOwnedItem(
            create_next("Owned page pushed from the Python facade.")
        )

    def replace_page() -> None:
        stack.replaceOwnedItem(
            create_next("Owned replacement for the current page.")
        )

    push_button.clicked.connect(push_page)
    replace_button.clicked.connect(replace_page)
    pop_button.clicked.connect(stack.pop)
    stack.depthChanged.connect(update_state)
    stack.currentItemChanged.connect(update_state)
    stack.busyChanged.connect(update_state)
    update_state()

    window.setContentWidget(page)
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
