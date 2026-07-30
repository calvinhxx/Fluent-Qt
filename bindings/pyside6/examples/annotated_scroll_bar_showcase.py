"""Interactive and snapshot acceptance example for AnnotatedScrollBar."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QTimer, Qt
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget


SECTIONS = (
    ("Overview", "Project summary and current status"),
    ("Activity", "Recent events from the native scroll surface"),
    ("Messages", "Conversation history and unread messages"),
    ("Files", "Documents shared with this workspace"),
    ("Members", "People and access settings"),
    ("Automation", "Rules that run in the background"),
    ("Security", "Authentication and audit settings"),
    ("About", "Version and runtime information"),
)
CARD_HEIGHT = 110
CARD_SPACING = 10
CONTENT_MARGIN = 16


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt AnnotatedScrollBar example."
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


def build_scroll_content() -> tuple[QWidget, list[object]]:
    content = QWidget()
    content.setFixedSize(
        370,
        CONTENT_MARGIN * 2
        + len(SECTIONS) * CARD_HEIGHT
        + (len(SECTIONS) - 1) * CARD_SPACING,
    )
    layout = QVBoxLayout(content)
    layout.setContentsMargins(
        CONTENT_MARGIN,
        CONTENT_MARGIN,
        CONTENT_MARGIN,
        CONTENT_MARGIN,
    )
    layout.setSpacing(CARD_SPACING)

    labels = []
    for index, (name, description) in enumerate(SECTIONS):
        card = fluentqt.Card(content)
        card.setFixedHeight(CARD_HEIGHT)
        card_layout = QVBoxLayout(card)
        card_layout.setContentsMargins(18, 14, 18, 14)
        card_layout.setSpacing(6)

        heading = fluentqt.Label(name, card)
        heading.setFluentTypography(fluentqt.FontRole.Subtitle)
        card_layout.addWidget(heading)

        detail = fluentqt.Label(description, card)
        detail.setWordWrap(True)
        card_layout.addWidget(detail)
        card_layout.addStretch()
        layout.addWidget(card)

        labels.append(
            fluentqt.AnnotatedScrollBarLabel(
                name,
                CONTENT_MARGIN + index * (CARD_HEIGHT + CARD_SPACING),
                description,
            )
        )

    return content, labels


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 AnnotatedScrollBar")
    window.resize(690, 680)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = fluentqt.Label("Native AnnotatedScrollBar", page)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = fluentqt.Label(
        "Python supplies label value types and signal handlers. The native "
        "control filters labels, renders static detail text, and mirrors a "
        "borrowed ScrollView in both directions.",
        page,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(fluentqt.Divider(page))

    row = QHBoxLayout()
    row.setSpacing(12)

    scroll_view = fluentqt.ScrollView(page)
    scroll_view.setFixedSize(420, 420)
    scroll_view.setHorizontalScrollBarVisibility(
        fluentqt.ScrollView.ScrollBarVisibility.Hidden
    )
    scroll_view.setVerticalScrollBarVisibility(
        fluentqt.ScrollView.ScrollBarVisibility.Hidden
    )
    content, labels = build_scroll_content()
    scroll_view.setContentWidget(content)

    bar = fluentqt.AnnotatedScrollBar(page)
    bar.setFixedSize(150, 420)
    bar.setPreferredSize(bar.size())
    bar.setMinimumBarSize(bar.size())
    bar.setLabelColumnWidth(82)
    bar.setMinimumLabelSpacing(42)
    bar.setLabels(labels)
    bar.connectToScrollView(scroll_view)

    row.addWidget(scroll_view)
    row.addWidget(bar)
    root.addLayout(row)

    status = fluentqt.Label("Section: Overview · offset 0", page)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(status)

    def section_for_offset(offset: int) -> str:
        active = labels[0]
        for label in labels:
            if label.offset > offset:
                break
            active = label
        return active.text

    def update_status(_horizontal: int, vertical: int) -> None:
        status.setText(
            "Section: {0} · offset {1}".format(
                section_for_offset(vertical),
                vertical,
            )
        )

    scroll_view.scrollPositionChanged.connect(update_status)
    bar.labelActivated.connect(
        lambda offset, text: status.setText(
            "Activated: {0} · offset {1}".format(text, offset)
        )
    )

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
