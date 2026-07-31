"""Interactive and snapshot acceptance example for SelectorBar and Pivot."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QStackedWidget,
    QVBoxLayout,
    QWidget,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the SelectorBar and Pivot example."
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


def make_page(
    parent: QWidget,
    title_text: str,
    body_text: str,
    metadata_text: str,
) -> fluentqt.Card:
    card = fluentqt.Card(parent)
    layout = QVBoxLayout(card)
    layout.setContentsMargins(22, 18, 22, 18)
    layout.setSpacing(8)

    title = fluentqt.Label(title_text, card)
    title.setFluentTypography(fluentqt.FontRole.Subtitle)
    layout.addWidget(title)

    body = fluentqt.Label(body_text, card)
    body.setWordWrap(True)
    layout.addWidget(body)

    metadata = fluentqt.Label(metadata_text, card)
    metadata.setFluentTypography(fluentqt.FontRole.Caption)
    metadata.setWordWrap(True)
    layout.addWidget(metadata)
    layout.addStretch()
    return card


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 navigation selectors")
    window.resize(900, 650)

    content = QWidget()
    root = QVBoxLayout(content)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = fluentqt.Label("SelectorBar + Pivot navigation", content)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = fluentqt.Label(
        "Python owns the page stack and route metadata. Native FluentQt "
        "controls provide selection, keyboard input, overflow, painting, "
        "and accessibility.",
        content,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(fluentqt.Divider(content))

    selector_label = fluentqt.Label("Application section", content)
    selector_label.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(selector_label)

    selector = fluentqt.SelectorBar(content)
    selector.setFixedHeight(44)
    selector_items = (
        fluentqt.SelectorBarItem(
            "Overview",
            "",
            True,
            True,
            {"route": "overview", "page": 0},
            "Overview section",
        ),
        fluentqt.SelectorBarItem(
            "Activity",
            "",
            True,
            True,
            {"route": "activity", "page": 1},
            "Activity section",
        ),
        fluentqt.SelectorBarItem(
            "Settings",
            "",
            True,
            True,
            {"route": "settings", "page": 2},
            "Settings section",
        ),
    )
    for item in selector_items:
        selector.addItem(item)
    root.addWidget(selector)

    pages = QStackedWidget(content)
    pages.setMinimumHeight(165)
    pages.addWidget(
        make_page(
            pages,
            "Overview",
            "A caller-owned QStackedWidget presents the page selected by "
            "the native SelectorBar.",
            "route='overview' · page=0",
        )
    )
    pages.addWidget(
        make_page(
            pages,
            "Recent activity",
            "The selectionChanged signal delivers both the index and a "
            "SelectorBarItem value to Python.",
            "route='activity' · page=1",
        )
    )
    pages.addWidget(
        make_page(
            pages,
            "Settings",
            "Pages remain ordinary PySide widgets; SelectorBar stores only "
            "navigation metadata and selection state.",
            "route='settings' · page=2",
        )
    )
    root.addWidget(pages)

    pivot_row = QHBoxLayout()
    pivot_row.setSpacing(12)

    mail_card = fluentqt.Card(content)
    mail_layout = QVBoxLayout(mail_card)
    mail_layout.setContentsMargins(18, 16, 18, 16)
    mail_layout.setSpacing(8)
    mail_title = fluentqt.Label("Message filter", mail_card)
    mail_title.setFluentTypography(fluentqt.FontRole.BodyStrong)
    mail_layout.addWidget(mail_title)

    pivot = fluentqt.Pivot(mail_card)
    pivot.setFixedHeight(44)
    for item in (
        fluentqt.PivotItem("All", "", True, {"filter": "all"}),
        fluentqt.PivotItem("Unread", "", True, {"filter": "unread"}),
        fluentqt.PivotItem("Flagged", "", True, {"filter": "flagged"}),
    ):
        pivot.addItem(item)
    mail_layout.addWidget(pivot)

    pivot_status = fluentqt.Label(mail_card)
    pivot_status.setWordWrap(True)
    mail_layout.addWidget(pivot_status)
    mail_layout.addStretch()
    pivot_row.addWidget(mail_card, 3)

    overflow_card = fluentqt.Card(content)
    overflow_layout = QVBoxLayout(overflow_card)
    overflow_layout.setContentsMargins(18, 16, 18, 16)
    overflow_layout.setSpacing(8)
    overflow_title = fluentqt.Label("Narrow overflow", overflow_card)
    overflow_title.setFluentTypography(fluentqt.FontRole.BodyStrong)
    overflow_layout.addWidget(overflow_title)

    narrow = fluentqt.SelectorBar(overflow_card)
    narrow.setOverflowBehavior(
        fluentqt.SelectorBar.OverflowBehavior.MoreButton
    )
    for name in (
        "Home",
        "Files",
        "Activity",
        "Reports",
        "People",
        "Settings",
    ):
        narrow.addItem(
            fluentqt.SelectorBarItem(
                name,
                "",
                True,
                True,
                {"route": name.lower()},
                "{0} section".format(name),
            )
        )
    narrow.setFixedHeight(44)
    overflow_layout.addWidget(narrow)
    overflow_note = fluentqt.Label(
        "MoreButton exposes the hidden item indexes without moving pages.",
        overflow_card,
    )
    overflow_note.setWordWrap(True)
    overflow_layout.addWidget(overflow_note)
    overflow_layout.addStretch()
    pivot_row.addWidget(overflow_card, 2)
    root.addLayout(pivot_row, 1)

    def select_page(index: int, item: fluentqt.SelectorBarItem) -> None:
        pages.setCurrentIndex(index)
        pages.currentWidget().setAccessibleName(item.accessibleName)

    def show_filter(index: int) -> None:
        item = pivot.itemAt(index)
        pivot_status.setText(
            "Python received PivotItem.data={0}. The filtered result view "
            "would stay caller-owned.".format(item.data)
        )

    selector.selectionChanged.connect(select_page)
    pivot.currentChanged.connect(show_filter)
    selector.setItemSelected(1, True)
    pivot.setSelectedIndex(1)
    select_page(selector.selectedIndex(), selector.selectedItem())
    show_filter(pivot.selectedIndex())

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

        QTimer.singleShot(300, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
