"""Interactive and snapshot acceptance example for Fluent menu buttons."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QPoint, QTimer, Qt
from PySide6.QtGui import QPainter, QPixmap
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt menu-button example."
    )
    parser.add_argument(
        "--snapshot",
        type=Path,
        help="Save the rendered window and an opened FluentMenu as PNG.",
    )
    parser.add_argument(
        "--auto-close-ms",
        type=int,
        default=0,
        help="Automatically close the interactive window after this delay.",
    )
    return parser.parse_args()


def add_menu_items(
    menu: fluentqt.FluentMenu,
    labels: tuple[str, ...],
    report,
) -> None:
    for label in labels:
        item = fluentqt.FluentMenuItem(label, menu)
        item.triggered.connect(
            lambda _checked=False, text=label: report("Menu: {0}".format(text))
        )
        menu.addAction(item)


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 menu buttons")
    window.resize(780, 470)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(36, 32, 36, 32)
    root.setSpacing(16)

    title = fluentqt.Label("Native Fluent menu buttons", page)
    title.setFluentTypography(fluentqt.FontRole.TitleLarge)
    root.addWidget(title)

    summary = fluentqt.Label(
        "Python supplies commands and handles signals. DropDownButton, "
        "SplitButton, ToggleSplitButton, and FluentMenu keep their native "
        "Fluent painting, hit testing, keyboard behavior, and lifetime rules.",
        page,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)

    card = fluentqt.Card(page)
    card_layout = QVBoxLayout(card)
    card_layout.setContentsMargins(24, 22, 24, 22)
    card_layout.setSpacing(16)

    status = fluentqt.Label("Choose a command", card)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)

    def report(message: str) -> None:
        status.setText(message)

    button_row = QHBoxLayout()
    button_row.setSpacing(14)

    drop_down = fluentqt.DropDownButton("Export", card)
    drop_down.setMinimumWidth(180)
    drop_menu = fluentqt.FluentMenu("Export format", drop_down)
    add_menu_items(drop_menu, ("PDF document", "PNG image", "Plain text"), report)
    drop_down.setMenu(drop_menu)
    button_row.addWidget(drop_down)

    split = fluentqt.SplitButton("Save", card)
    split.setMinimumWidth(180)
    split.clicked.connect(lambda: report("Primary: Save"))
    split_menu = fluentqt.FluentMenu("Save options", split)
    add_menu_items(split_menu, ("Save as...", "Save a copy", "Auto save"), report)
    split.setMenu(split_menu)
    button_row.addWidget(split)

    toggle = fluentqt.ToggleSplitButton("Pin", card)
    toggle.setMinimumWidth(180)
    toggle.toggled.connect(
        lambda checked: report(
            "Primary: {0}".format("Pinned" if checked else "Unpinned")
        )
    )
    toggle_menu = fluentqt.FluentMenu("Pin duration", toggle)
    add_menu_items(toggle_menu, ("For one hour", "Until tomorrow", "Always"), report)
    toggle.setMenu(toggle_menu)
    button_row.addWidget(toggle)

    card_layout.addLayout(button_row)
    card_layout.addWidget(fluentqt.Divider(card))
    card_layout.addWidget(status)
    root.addWidget(card)

    note = fluentqt.Label(
        "The menu remains caller-owned. The binding retains its Python "
        "wrapper while installed and releases it on replacement or setMenu(None).",
        page,
    )
    note.setWordWrap(True)
    note.setFluentTypography(fluentqt.FontRole.Caption)
    root.addWidget(note)
    root.addStretch()
    window.setContentWidget(page)

    window._snapshot_button = drop_down
    window._snapshot_menu = drop_menu
    window._menus = (drop_menu, split_menu, toggle_menu)
    return window


def save_snapshot(window: fluentqt.Window, path: Path) -> bool:
    window_image = window.grab()
    menu_image = window._snapshot_menu.grab()
    if window_image.isNull() or menu_image.isNull():
        return False

    ratio = window_image.devicePixelRatio()
    menu_position = window._snapshot_button.mapTo(
        window,
        QPoint(0, window._snapshot_button.height()),
    )
    canvas_width = max(
        window.width(),
        menu_position.x() + window._snapshot_menu.width(),
    )
    canvas_height = max(
        window.height(),
        menu_position.y() + window._snapshot_menu.height(),
    )
    canvas = QPixmap(
        max(1, round(canvas_width * ratio)),
        max(1, round(canvas_height * ratio)),
    )
    canvas.setDevicePixelRatio(ratio)
    canvas.fill(Qt.transparent)
    painter = QPainter(canvas)
    painter.drawPixmap(QPoint(0, 0), window_image)
    painter.drawPixmap(menu_position, menu_image)
    painter.end()
    return canvas.save(str(path), "PNG")


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

        def open_menu() -> None:
            button = window._snapshot_button
            window._snapshot_menu.popup(
                button.mapToGlobal(QPoint(0, button.height()))
            )

        def capture() -> None:
            snapshot_path.parent.mkdir(parents=True, exist_ok=True)
            if not save_snapshot(window, snapshot_path):
                print(
                    "Unable to save snapshot: {0}".format(snapshot_path),
                    file=sys.stderr,
                )
                app.exit(2)
                return
            print("snapshot: {0}".format(snapshot_path))
            window._snapshot_menu.close()
            app.quit()

        QTimer.singleShot(0, open_menu)
        QTimer.singleShot(300, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
