"""Interactive and snapshot acceptance example for TabView composition."""

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
        description="Open or snapshot the FluentQt TabView example."
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


def create_page(title: str, detail: str) -> QWidget:
    page = fluentqt.Card()
    page.setObjectName(title)
    layout = QVBoxLayout(page)
    layout.setContentsMargins(28, 28, 28, 28)
    layout.setSpacing(10)

    heading = fluentqt.Label(title, page)
    heading.setFluentTypography(fluentqt.FontRole.Subtitle)
    layout.addWidget(heading)

    description = fluentqt.Label(detail, page)
    description.setWordWrap(True)
    layout.addWidget(description)
    layout.addStretch()
    return page


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 TabView")
    window.resize(760, 520)

    content = QWidget()
    root = QVBoxLayout(content)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = fluentqt.Label("TabView metadata + Python page host", content)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = fluentqt.Label(
        "The native TabView owns tab metadata, selection, close, reorder, "
        "RTL, and keyboard behavior. A regular PySide QStackedWidget owns "
        "the corresponding application pages.",
        content,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(fluentqt.Divider(content))

    tabs = fluentqt.TabView(content)
    tabs.setFixedHeight(40)
    tabs.setTabWidthMode(fluentqt.TabView.TabWidthMode.SizeToContent)
    tabs.setCloseButtonOverlayMode(
        fluentqt.TabView.CloseButtonOverlayMode.Always
    )
    tabs.setTabReorderEnabled(True)
    root.addWidget(tabs)

    host = QStackedWidget(content)
    root.addWidget(host, 1)

    status = fluentqt.Label(content)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(status)

    actions = QHBoxLayout()
    move_button = fluentqt.Button("Move selected", content)
    close_button = fluentqt.Button("Close selected", content)
    add_button = fluentqt.Button("Add document", content)
    add_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    actions.addStretch()
    actions.addWidget(move_button)
    actions.addWidget(close_button)
    actions.addWidget(add_button)
    root.addLayout(actions)

    next_number = {"value": 1}

    def update_state(index: int | None = None) -> None:
        del index
        selected = tabs.selectedIndex()
        if selected < 0:
            status.setText("No document selected")
        else:
            item = tabs.tabAt(selected)
            status.setText(
                "{0} · tab {1} of {2} · metadata {3}".format(
                    item.text,
                    selected + 1,
                    tabs.tabCount(),
                    item.data,
                )
            )
        close_button.setEnabled(selected >= 0)
        move_button.setEnabled(tabs.tabCount() > 1 and selected >= 0)

    def add_document(select: bool = True) -> None:
        number = next_number["value"]
        next_number["value"] += 1
        item = fluentqt.TabViewItem(
            "Document {0}".format(number),
            "",
            True,
            True,
            {"document_id": number},
            "Document {0} tab".format(number),
        )
        index = tabs.addTab(item)
        host.insertWidget(
            index,
            create_page(
                item.text,
                "This page is ordinary Python-owned application content. "
                "The native tab keeps only its value metadata.",
            ),
        )
        if select:
            tabs.setSelectedIndex(index)
        host.setCurrentIndex(tabs.selectedIndex())
        update_state()

    def close_document(index: int) -> None:
        if index < 0 or index >= tabs.tabCount():
            return
        page = host.widget(index)
        if not tabs.closeTab(index):
            return
        host.removeWidget(page)
        page.deleteLater()
        host.setCurrentIndex(tabs.selectedIndex())
        update_state()

    def move_page(start: int, end: int) -> None:
        page = host.widget(start)
        current_page = host.currentWidget()
        host.removeWidget(page)
        host.insertWidget(end, page)
        if current_page is not None:
            host.setCurrentWidget(current_page)
        update_state()

    def move_selected() -> None:
        selected = tabs.selectedIndex()
        if selected < 0 or tabs.tabCount() < 2:
            return
        tabs.moveTab(selected, (selected + 1) % tabs.tabCount())

    tabs.currentChanged.connect(host.setCurrentIndex)
    tabs.currentChanged.connect(update_state)
    tabs.tabMoved.connect(move_page)
    tabs.tabCloseRequested.connect(close_document)
    tabs.addTabRequested.connect(add_document)
    add_button.clicked.connect(lambda: add_document())
    close_button.clicked.connect(
        lambda: close_document(tabs.selectedIndex())
    )
    move_button.clicked.connect(move_selected)

    add_document()
    add_document()
    add_document()
    tabs.setSelectedIndex(1)
    host.setCurrentIndex(1)
    update_state()

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
