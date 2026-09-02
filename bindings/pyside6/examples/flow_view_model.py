"""Interactive and snapshot acceptance example for FlowView model interop."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import (
    QAbstractListModel,
    QItemSelectionModel,
    QModelIndex,
    QRectF,
    QSize,
    Qt,
    QTimer,
)
from PySide6.QtGui import QColor, QPainter, QPen
from PySide6.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QStyle,
    QStyledItemDelegate,
    QVBoxLayout,
    QWidget,
)


DETAIL_ROLE = int(Qt.UserRole) + 1
COLOR_ROLE = int(Qt.UserRole) + 2
CARD_SIZE_ROLE = int(Qt.UserRole) + 3

INITIAL_CARDS = (
    ("Adaptive cards", "Python size role · 208 × 88", "#6750a4", QSize(208, 88)),
    ("Model updates", "insert / remove / reset", "#0067c0", QSize(176, 88)),
    ("Native wrapping", "C++ geometry engine", "#0f7b6c", QSize(196, 104)),
    ("Delegate virtuals", "Python paint + sizeHint", "#b146c2", QSize(224, 104)),
    ("Selection", "Qt item-selection model", "#ca5010", QSize(164, 82)),
    ("Qt 6.2+", "one stable binding contract", "#3a7d44", QSize(212, 82)),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt FlowView model example."
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


class FlowCardModel(QAbstractListModel):
    """Caller-owned Python model consumed by the native FlowView."""

    def __init__(self):
        super().__init__()
        self._cards = list(INITIAL_CARDS)

    def rowCount(self, parent=QModelIndex()):
        return 0 if parent.isValid() else len(self._cards)

    def data(self, index, role=Qt.DisplayRole):
        if not index.isValid() or not 0 <= index.row() < len(self._cards):
            return None
        title, detail, color, size = self._cards[index.row()]
        if role == Qt.DisplayRole:
            return title
        if role == DETAIL_ROLE:
            return detail
        if role == COLOR_ROLE:
            return QColor(color)
        if role == CARD_SIZE_ROLE:
            return size
        return None

    def flags(self, index):
        if not index.isValid():
            return Qt.NoItemFlags
        return Qt.ItemIsEnabled | Qt.ItemIsSelectable

    def appendCard(self, number):
        row = len(self._cards)
        self.beginInsertRows(QModelIndex(), row, row)
        self._cards.append(
            (
                "Python row {0}".format(number),
                "insertRows notification",
                "#4d69be",
                QSize(184 + (number % 2) * 24, 88),
            )
        )
        self.endInsertRows()

    def removeRows(self, row, count, parent=QModelIndex()):
        if parent.isValid() or count <= 0:
            return False
        last = row + count - 1
        if row < 0 or last >= len(self._cards):
            return False
        self.beginRemoveRows(QModelIndex(), row, last)
        del self._cards[row:last + 1]
        self.endRemoveRows()
        return True

    def resetCards(self):
        self.beginResetModel()
        self._cards = list(INITIAL_CARDS)
        self.endResetModel()


class FlowCardDelegate(QStyledItemDelegate):
    """Python virtual paint implementation for variable native flow items."""

    def sizeHint(self, option, index):
        del option
        size = index.data(CARD_SIZE_ROLE)
        return size if isinstance(size, QSize) else QSize(180, 88)

    def paint(self, painter, option, index):
        painter.save()
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)

        dark = fluentqt.theme_uses_dark_appearance(fluentqt.current_theme())
        selected = bool(option.state & QStyle.State_Selected)
        hovered = bool(option.state & QStyle.State_MouseOver)
        accent = QColor(index.data(COLOR_ROLE))
        surface = QColor("#303030" if dark else "#ffffff")
        text_primary = QColor("#f5f5f5" if dark else "#1b1b1b")
        text_secondary = QColor("#c7c7c7" if dark else "#616161")
        outline = QColor("#5b5b5b" if dark else "#dedede")

        card = QRectF(option.rect).adjusted(3.0, 3.0, -3.0, -3.0)
        if selected:
            selected_surface = QColor(accent)
            selected_surface.setAlpha(42 if dark else 24)
            painter.setBrush(selected_surface)
            painter.setPen(QPen(accent, 2.0))
        else:
            if hovered:
                surface = surface.lighter(108) if dark else surface.darker(102)
            painter.setBrush(surface)
            painter.setPen(QPen(outline, 1.0))
        painter.drawRoundedRect(card, 9.0, 9.0)

        accent_rect = QRectF(
            card.left() + 10.0,
            card.top() + 11.0,
            5.0,
            card.height() - 22.0,
        )
        painter.setPen(Qt.NoPen)
        painter.setBrush(accent)
        painter.drawRoundedRect(accent_rect, 2.5, 2.5)

        text_left = accent_rect.right() + 11.0
        text_width = card.right() - text_left - 11.0
        title_rect = QRectF(text_left, card.top() + 12.0, text_width, 22.0)
        detail_rect = QRectF(
            text_left,
            title_rect.bottom() + 4.0,
            text_width,
            max(20.0, card.bottom() - title_rect.bottom() - 12.0),
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
            detail_rect,
            Qt.AlignLeft | Qt.AlignTop | Qt.TextWordWrap,
            str(index.data(DETAIL_ROLE)),
        )
        painter.restore()


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 FlowView")
    window.resize(820, 650)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = fluentqt.Label("Native FlowView + Python model", page)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = fluentqt.Label(
        "A Python QAbstractListModel supplies each card's QSize while the "
        "native C++ FlowView performs wrapping, hit testing, scrolling, and "
        "selection. Painting is dispatched back to a Python delegate.",
        page,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(fluentqt.Divider(page))

    model = FlowCardModel()
    delegate = FlowCardDelegate()
    view = fluentqt.FlowView(
        page,
        selectionMode=fluentqt.SelectionMode.Multiple,
    )
    view.setModel(model)
    view.setItemDelegate(delegate)
    view.setItemSizeRole(CARD_SIZE_ROLE)
    view.setDefaultItemSize(QSize(180, 88))
    view.setMinimumItemSize(QSize(140, 72))
    view.setMaximumItemSize(QSize(240, 112))
    view.setHorizontalSpacing(8)
    view.setVerticalSpacing(8)
    view.setHeaderText("Adaptive Python cards")
    view.setPlaceholderText("No cards in the Python model")
    view.setCanReorderItems(False)
    for row in (0, 3):
        view.selectionModel().select(
            model.index(row, 0),
            QItemSelectionModel.Select,
        )
    root.addWidget(view, 1)

    status = fluentqt.Label(page)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(status)

    actions = QHBoxLayout()
    reset_button = fluentqt.Button("Reset model", page)
    remove_button = fluentqt.Button("Remove selected", page)
    add_button = fluentqt.Button("Insert Python row", page)
    add_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    actions.addStretch()
    actions.addWidget(reset_button)
    actions.addWidget(remove_button)
    actions.addWidget(add_button)
    root.addLayout(actions)

    next_row = {"value": 1}

    def update_status(*_args):
        selected = view.selectedRows()
        status.setText(
            "{0} adaptive cards · selected {1}".format(
                model.rowCount(),
                ", ".join(str(row + 1) for row in selected) or "none",
            )
        )

    def add_card():
        number = next_row["value"]
        next_row["value"] += 1
        model.appendCard(number)
        view.setSelectedIndex(model.rowCount() - 1)

    def remove_selected():
        for row in sorted(view.selectedRows(), reverse=True):
            model.removeRows(row, 1)
        if model.rowCount() > 0:
            view.setSelectedIndex(0)

    def reset_cards():
        model.resetCards()
        view.setSelectionMode(fluentqt.SelectionMode.Multiple)
        view.setSelectedIndex(0)

    add_button.clicked.connect(add_card)
    remove_button.clicked.connect(remove_selected)
    reset_button.clicked.connect(reset_cards)
    view.itemClicked.connect(update_status)
    view.selectionModel().selectionChanged.connect(update_status)
    model.rowsInserted.connect(update_status)
    model.rowsRemoved.connect(update_status)
    model.modelReset.connect(update_status)
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
