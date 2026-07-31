"""Interactive and snapshot acceptance example for Breadcrumb navigation."""

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
        description="Open or snapshot the FluentQt Breadcrumb example."
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


def breadcrumb_items() -> list[fluentqt.BreadcrumbItem]:
    names = ("Home", "Projects", "FluentQt", "Python", "Breadcrumb")
    return [
        fluentqt.BreadcrumbItem(
            name,
            {"route": index, "slug": name.lower()},
            True,
            "{0} location".format(name),
        )
        for index, name in enumerate(names)
    ]


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 Breadcrumb")
    window.resize(820, 430)

    content = QWidget()
    root = QVBoxLayout(content)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = fluentqt.Label("Breadcrumb metadata + native navigation", content)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = fluentqt.Label(
        "Python supplies mutable route metadata and signal handlers. "
        "FluentQt keeps overflow layout, keyboard navigation, RTL behavior, "
        "painting, and accessibility in the native widget.",
        content,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(fluentqt.Divider(content))

    full_path = breadcrumb_items()
    breadcrumb = fluentqt.Breadcrumb(content)
    breadcrumb.setBreadcrumbSize(
        fluentqt.Breadcrumb.BreadcrumbSize.Large
    )
    breadcrumb.setOverflowMode(fluentqt.Breadcrumb.OverflowMode.Middle)
    breadcrumb.setLargeFontRole(fluentqt.FontRole.Subtitle)
    breadcrumb.setFixedHeight(40)
    root.addWidget(breadcrumb)

    status = fluentqt.Label(content)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    status.setWordWrap(True)
    root.addWidget(status)

    actions = QHBoxLayout()
    reset_button = fluentqt.Button("Full path", content)
    python_button = fluentqt.Button("Open Python", content)
    python_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    actions.addStretch()
    actions.addWidget(reset_button)
    actions.addWidget(python_button)
    root.addLayout(actions)

    root.addWidget(fluentqt.Divider(content))

    overflow_row = QHBoxLayout()
    overflow_label = fluentqt.Label("Narrow middle overflow", content)
    overflow_label.setFluentTypography(fluentqt.FontRole.BodyStrong)
    overflow_row.addWidget(overflow_label)
    overflow_row.addStretch()
    narrow = fluentqt.Breadcrumb(content)
    narrow.setItems(full_path)
    narrow.setOverflowMode(fluentqt.Breadcrumb.OverflowMode.Middle)
    narrow.setFixedSize(330, 20)
    overflow_row.addWidget(narrow)
    root.addLayout(overflow_row)

    detail = fluentqt.Card(content)
    detail_layout = QVBoxLayout(detail)
    detail_layout.setContentsMargins(20, 18, 20, 18)
    detail_title = fluentqt.Label("Python route metadata", detail)
    detail_title.setFluentTypography(fluentqt.FontRole.Subtitle)
    detail_layout.addWidget(detail_title)
    detail_text = fluentqt.Label(detail)
    detail_text.setWordWrap(True)
    detail_layout.addWidget(detail_text)
    root.addWidget(detail, 1)

    def show_level(index: int) -> None:
        bounded = max(0, min(index, len(full_path) - 1))
        breadcrumb.setItems(full_path[: bounded + 1])
        item = full_path[bounded]
        status.setText(
            "Location {0} of {1}: {2}".format(
                bounded + 1,
                len(full_path),
                item.text,
            )
        )
        detail_text.setText(
            "The activated BreadcrumbItem arrived in Python with data={0} "
            "and accessibleName={1!r}.".format(
                item.data,
                item.accessibleName,
            )
        )

    breadcrumb.itemActivated.connect(
        lambda index, _item: show_level(index)
    )
    reset_button.clicked.connect(lambda: show_level(len(full_path) - 1))
    python_button.clicked.connect(lambda: show_level(3))
    show_level(len(full_path) - 1)

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
