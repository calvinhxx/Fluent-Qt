"""Interactive and snapshot acceptance example for AutoSuggestBox."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QTimer, Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication, QVBoxLayout, QWidget

from fluentqt.layout import Card, Divider
from fluentqt.textfields import AutoSuggestBox, Label
from fluentqt.windowing import Window


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt AutoSuggestBox example."
    )
    parser.add_argument(
        "--snapshot",
        type=Path,
        help="Open the suggestion flyout, save the window as PNG, and exit.",
    )
    parser.add_argument(
        "--auto-close-ms",
        type=int,
        default=0,
        help="Automatically close the interactive window after this delay.",
    )
    return parser.parse_args()


def build_showcase() -> tuple[Window, AutoSuggestBox]:
    window = Window()
    window.setWindowTitle("FluentQt PySide6 AutoSuggestBox")
    window.resize(820, 620)

    content = QWidget()
    root = QVBoxLayout(content)
    root.setContentsMargins(36, 30, 36, 30)
    root.setSpacing(14)

    title = Label("Native suggestions from Python", content)
    title.setFluentTypography(fluentqt.FontRole.TitleLarge)
    root.addWidget(title)

    summary = Label(
        "Python supplies a QStringList and receives typed signals. The input, "
        "keyboard preview, Fluent painting, and same-window suggestion Flyout "
        "remain in the C++ component.",
        content,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(Divider(content))

    card = Card(content)
    card.setMinimumHeight(300)
    card_layout = QVBoxLayout(card)
    card_layout.setContentsMargins(24, 22, 24, 22)
    card_layout.setSpacing(14)

    search = AutoSuggestBox(card)
    search.setHeader("Search destinations")
    search.setPlaceholderText("Type a city")
    search.setSuggestions(
        [
            "Amsterdam",
            "Athens",
            "Auckland",
            "Austin",
        ]
    )
    search.setFixedWidth(420)
    card_layout.addWidget(search, 0, Qt.AlignHCenter)
    card_layout.addStretch()

    status = Label("Type to open suggestions", card)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    status.setAlignment(Qt.AlignCenter)
    card_layout.addWidget(status)

    reason_names = {
        AutoSuggestBox.TextChangeReason.UserInput: "user input",
        AutoSuggestBox.TextChangeReason.ProgrammaticChange: "preview",
        AutoSuggestBox.TextChangeReason.SuggestionChosen: "chosen",
    }

    def on_text_changed(text: str, reason: object) -> None:
        status.setText(
            "Text: {0}  |  Reason: {1}".format(
                text or "(empty)",
                reason_names.get(reason, "unknown"),
            )
        )

    def on_query_submitted(text: str, chosen: object) -> None:
        status.setText(
            "Submitted: {0}  |  Suggestion: {1}".format(
                text,
                chosen or "(typed query)",
            )
        )

    search.textChangedWithReason.connect(on_text_changed)
    search.querySubmitted.connect(on_query_submitted)
    root.addWidget(card)

    hint = Label(
        "Use Up/Down to preview, Enter to submit, or Escape/outside press "
        "to dismiss without moving focus away from the editor.",
        content,
    )
    hint.setWordWrap(True)
    hint.setAlignment(Qt.AlignCenter)
    root.addWidget(hint)
    root.addStretch()

    window.setContentWidget(content)
    return window, search


def main() -> int:
    args = parse_args()
    app = QApplication.instance() or QApplication(sys.argv)
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window, search = build_showcase()
    window.show()

    if args.snapshot is not None:
        snapshot_path = args.snapshot.expanduser().resolve()

        def open_suggestions() -> None:
            search.setFocus()
            QTest.keyClicks(search, "a")

        def capture() -> None:
            if not search.isSuggestionListOpen():
                print("Suggestion Flyout did not open", file=sys.stderr)
                app.exit(2)
                return
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

        QTimer.singleShot(80, open_suggestions)
        QTimer.singleShot(400, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
