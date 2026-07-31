"""Interactive and snapshot acceptance example for GridView model interop."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QItemSelectionModel, QRectF, QSize, Qt, QTimer
from PySide6.QtGui import (
    QColor,
    QLinearGradient,
    QPainter,
    QPen,
    QStandardItem,
    QStandardItemModel,
)
from PySide6.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QStyle,
    QStyledItemDelegate,
    QVBoxLayout,
    QWidget,
)


SUBTITLE_ROLE = Qt.UserRole + 1
START_COLOR_ROLE = Qt.UserRole + 2
END_COLOR_ROLE = Qt.UserRole + 3

INITIAL_CARDS = (
    ("Aurora", "Design review", "#6257d5", "#54b9d1"),
    ("Canyon", "Reference set", "#d56847", "#f2b95e"),
    ("Forest", "Token study", "#277d63", "#7fba64"),
    ("Harbor", "Layout pass", "#26758f", "#67c4c0"),
    ("Orchid", "Color system", "#8c4b91", "#d477a4"),
    ("Slate", "Interaction notes", "#526277", "#8799aa"),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt GridView model example."
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


def append_card(model, title, subtitle, start_color, end_color):
    item = QStandardItem(title)
    item.setData(subtitle, SUBTITLE_ROLE)
    item.setData(QColor(start_color), START_COLOR_ROLE)
    item.setData(QColor(end_color), END_COLOR_ROLE)
    model.appendRow(item)


def populate_model(model):
    model.clear()
    for card in INITIAL_CARDS:
        append_card(model, *card)


class PhotoCardDelegate(QStyledItemDelegate):
    """Python virtual paint implementation for native GridView cells."""

    def sizeHint(self, option, index):
        del option, index
        return QSize(156, 116)

    def paint(self, painter, option, index):
        painter.save()
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)

        dark = fluentqt.current_theme() == fluentqt.Theme.Dark
        accent = QColor(fluentqt.accent_color())
        text_primary = QColor("#f5f5f5" if dark else "#1b1b1b")
        text_secondary = QColor("#d0d0d0" if dark else "#666666")
        surface = QColor("#292929" if dark else "#ffffff")
        outline = QColor("#575757" if dark else "#d8d8d8")

        card = QRectF(option.rect).adjusted(3.0, 3.0, -3.0, -3.0)
        painter.setPen(QPen(outline, 1.0))
        painter.setBrush(surface)
        painter.drawRoundedRect(card, 8.0, 8.0)

        image_rect = QRectF(
            card.left() + 1.0,
            card.top() + 1.0,
            card.width() - 2.0,
            67.0,
        )
        gradient = QLinearGradient(image_rect.topLeft(), image_rect.bottomRight())
        gradient.setColorAt(0.0, QColor(index.data(START_COLOR_ROLE)))
        gradient.setColorAt(1.0, QColor(index.data(END_COLOR_ROLE)))
        painter.setPen(Qt.NoPen)
        painter.setBrush(gradient)
        painter.drawRoundedRect(image_rect, 7.0, 7.0)
        painter.fillRect(
            QRectF(
                image_rect.left(),
                image_rect.bottom() - 7.0,
                image_rect.width(),
                8.0,
            ),
            gradient,
        )

        selected = bool(option.state & QStyle.State_Selected)
        hovered = bool(option.state & QStyle.State_MouseOver)
        if selected:
            painter.setPen(QPen(accent, 2.0))
            painter.setBrush(Qt.NoBrush)
            painter.drawRoundedRect(card.adjusted(1.0, 1.0, -1.0, -1.0), 7.0, 7.0)
            check_rect = QRectF(card.right() - 25.0, card.top() + 8.0, 17.0, 17.0)
            painter.setPen(Qt.NoPen)
            painter.setBrush(accent)
            painter.drawEllipse(check_rect)
            painter.setPen(QPen(QColor("#ffffff"), 1.8))
            painter.drawLine(
                check_rect.left() + 4.0,
                check_rect.center().y(),
                check_rect.center().x() - 1.0,
                check_rect.bottom() - 4.0,
            )
            painter.drawLine(
                check_rect.center().x() - 1.0,
                check_rect.bottom() - 4.0,
                check_rect.right() - 3.5,
                check_rect.top() + 4.0,
            )
        elif hovered:
            hover_fill = QColor(text_primary)
            hover_fill.setAlpha(14)
            painter.setPen(Qt.NoPen)
            painter.setBrush(hover_fill)
            painter.drawRoundedRect(card, 8.0, 8.0)

        title_rect = QRectF(
            card.left() + 11.0,
            image_rect.bottom() + 6.0,
            card.width() - 22.0,
            19.0,
        )
        subtitle_rect = QRectF(
            title_rect.left(),
            title_rect.bottom(),
            title_rect.width(),
            17.0,
        )
        painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.BodyStrong))
        painter.setPen(text_primary)
        painter.drawText(
            title_rect,
            Qt.AlignLeft | Qt.AlignVCenter,
            painter.fontMetrics().elidedText(
                str(index.data(Qt.DisplayRole)),
                Qt.ElideRight,
                int(title_rect.width()),
            ),
        )
        painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.Caption))
        painter.setPen(text_secondary)
        painter.drawText(
            subtitle_rect,
            Qt.AlignLeft | Qt.AlignVCenter,
            painter.fontMetrics().elidedText(
                str(index.data(SUBTITLE_ROLE)),
                Qt.ElideRight,
                int(subtitle_rect.width()),
            ),
        )
        painter.restore()


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 GridView")
    window.resize(680, 640)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = fluentqt.Label("Native GridView + Python model", page)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = fluentqt.Label(
        "A PySide QStandardItemModel and Python delegate drive the native "
        "Fluent grid. Select several cards, then drag one of the selected "
        "cards to reorder the group.",
        page,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(fluentqt.Divider(page))

    model = QStandardItemModel()
    populate_model(model)
    delegate = PhotoCardDelegate()
    view = fluentqt.GridView(
        page,
        selectionMode=fluentqt.SelectionMode.Multiple,
    )
    view.setModel(model)
    view.setItemDelegate(delegate)
    view.setCellSize(QSize(156, 116))
    view.setHorizontalSpacing(10)
    view.setVerticalSpacing(10)
    view.setMaxColumns(3)
    view.setHeaderText("Python collections")
    view.setPlaceholderText("No cards")
    view.setCanReorderItems(True)
    for row in (0, 2):
        view.selectionModel().select(
            model.index(row, 0),
            QItemSelectionModel.Select,
        )
    root.addWidget(view, 1)

    status = fluentqt.Label(page)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(status)

    actions = QHBoxLayout()
    reset_button = fluentqt.Button("Reset cards", page)
    remove_button = fluentqt.Button("Remove selected", page)
    add_button = fluentqt.Button("Add card", page)
    add_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    actions.addStretch()
    actions.addWidget(reset_button)
    actions.addWidget(remove_button)
    actions.addWidget(add_button)
    root.addLayout(actions)

    next_card = {"value": 1}

    def update_status(*_args):
        selected = view.selectedRows()
        status.setText(
            "{0} cards · selected {1}".format(
                model.rowCount(),
                ", ".join(str(row + 1) for row in selected) or "none",
            )
        )

    def add_card():
        number = next_card["value"]
        next_card["value"] += 1
        append_card(
            model,
            "Python {0}".format(number),
            "Inserted row",
            "#4d69be",
            "#8d74d8",
        )
        view.setSelectedIndex(model.rowCount() - 1)
        update_status()

    def remove_selected():
        rows = sorted(view.selectedRows(), reverse=True)
        for row in rows:
            model.removeRow(row)
        if model.rowCount() > 0:
            view.setSelectedIndex(min(rows[-1] if rows else 0, model.rowCount() - 1))
        update_status()

    def reset_cards():
        populate_model(model)
        view.setSelectedIndex(0)
        update_status()

    add_button.clicked.connect(add_card)
    remove_button.clicked.connect(remove_selected)
    reset_button.clicked.connect(reset_cards)
    view.itemClicked.connect(update_status)
    view.itemReordered.connect(update_status)
    view.selectionModel().selectionChanged.connect(update_status)
    model.rowsInserted.connect(update_status)
    model.rowsRemoved.connect(update_status)
    update_status()

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

        QTimer.singleShot(300, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
