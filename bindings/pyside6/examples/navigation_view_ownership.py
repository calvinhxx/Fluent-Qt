"""Interactive and snapshot acceptance example for NavigationView."""

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
        description="Open or snapshot the FluentQt NavigationView example."
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


def make_page(title_text: str, policy_text: str, detail: str) -> fluentqt.Card:
    page = fluentqt.Card()
    layout = QVBoxLayout(page)
    layout.setContentsMargins(32, 28, 32, 28)
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
    window.setWindowTitle("FluentQt PySide6 NavigationView")
    window.resize(960, 620)

    nav = fluentqt.NavigationView()
    nav.setAnimationEnabled(False)
    nav.setDisplayMode(fluentqt.NavigationView.DisplayMode.Left)
    nav.setExpandedPaneWidth(260)

    header = fluentqt.Card()
    header.setMinimumSize(240, 64)
    header_layout = QHBoxLayout(header)
    header_layout.setContentsMargins(18, 10, 14, 10)
    product = fluentqt.Label("FluentQt", header)
    product.setFluentTypography(fluentqt.FontRole.Subtitle)
    mode_button = fluentqt.Button("Top mode", header)
    header_layout.addWidget(product)
    header_layout.addStretch()
    header_layout.addWidget(mode_button)
    nav.setOwnedHeaderChromeWidget(header)

    navigation_card = fluentqt.Card()
    navigation_card.setMinimumWidth(240)
    navigation_layout = QVBoxLayout(navigation_card)
    navigation_layout.setContentsMargins(14, 14, 14, 14)
    navigation_layout.setSpacing(8)
    section = fluentqt.Label("Python routes", navigation_card)
    section.setFluentTypography(fluentqt.FontRole.BodyStrong)
    navigation_layout.addWidget(section)

    page_buttons = []
    for text in ("Overview", "Workspace", "Settings"):
        button = fluentqt.Button(text, navigation_card)
        button.setFixedHeight(40)
        navigation_layout.addWidget(button)
        page_buttons.append(button)
    navigation_layout.addStretch()
    nav.setBorrowedMainChromeWidget(navigation_card)

    footer_parent = QWidget()
    footer = fluentqt.Card(footer_parent)
    footer.setMinimumSize(240, 82)
    footer_layout = QVBoxLayout(footer)
    footer_layout.setContentsMargins(16, 10, 16, 10)
    status = fluentqt.Label("Overview · Owned page", footer)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    policy_note = fluentqt.Label(
        "Chrome: Owned / Borrowed / Reparented",
        footer,
    )
    policy_note.setFluentTypography(fluentqt.FontRole.Caption)
    policy_note.setWordWrap(True)
    footer_layout.addWidget(status)
    footer_layout.addWidget(policy_note)
    nav.setReparentedFooterChromeWidget(footer)

    host = nav.contentHost()
    pages = [
        make_page(
            "Overview",
            "Owned page",
            "The native StackContentHost deletes this page when its "
            "ownership-aware release path runs.",
        ),
        make_page(
            "Workspace",
            "Borrowed page",
            "Python retains this QWidget while NavigationView supplies "
            "geometry, theme painting, and page transitions.",
        ),
        make_page(
            "Settings",
            "Reparented page",
            "When released, this page returns to the QWidget parent it had "
            "before StackContentHost adopted it.",
        ),
    ]
    page_restore_parent = QWidget()
    pages[2].setParent(page_restore_parent)
    host.addOwnedPage(pages[0])
    host.addBorrowedPage(pages[1])
    host.addReparentedPage(pages[2])

    policies = ("Owned", "Borrowed", "Reparented")

    def select_page(index: int) -> None:
        host.setCurrentIndex(index, 1, False)
        status.setText(
            "{0} · {1} page".format(
                page_buttons[index].text(),
                policies[index],
            )
        )
        for button_index, button in enumerate(page_buttons):
            style = (
                fluentqt.Button.ButtonStyle.Accent
                if button_index == index
                else fluentqt.Button.ButtonStyle.Standard
            )
            button.setFluentStyle(style)

    for index, button in enumerate(page_buttons):
        button.clicked.connect(
            lambda checked=False, target=index: select_page(target)
        )

    def toggle_mode() -> None:
        top = nav.displayMode() == fluentqt.NavigationView.DisplayMode.Top
        nav.setDisplayMode(
            fluentqt.NavigationView.DisplayMode.Left
            if top
            else fluentqt.NavigationView.DisplayMode.Top
        )
        mode_button.setText("Top mode" if top else "Left mode")

    mode_button.clicked.connect(toggle_mode)
    select_page(0)
    window.setContentWidget(nav)

    # Keep restoration-only containers explicit for interactive inspection.
    window._navigation_restore_parents = (
        footer_parent,
        page_restore_parent,
    )
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
