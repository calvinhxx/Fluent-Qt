"""Interactive and snapshot acceptance example for ListView model interop."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QAbstractListModel, QModelIndex, QRectF, QSize, Qt, QTimer
from PySide6.QtGui import QColor, QPainter
from PySide6.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QStyle,
    QStyledItemDelegate,
    QVBoxLayout,
    QWidget,
)


DETAIL_ROLE = Qt.UserRole + 1
STATUS_ROLE = Qt.UserRole + 2

INITIAL_TASKS = [
    {
        "title": "Audit the Python model boundary",
        "detail": "QAbstractListModel · persistent indexes",
        "status": "Done",
    },
    {
        "title": "Verify caller-owned delegates",
        "detail": "QStyledItemDelegate · virtual paint",
        "status": "In progress",
    },
    {
        "title": "Exercise selection updates",
        "detail": "Insert · remove · reset",
        "status": "Queued",
    },
    {
        "title": "Run clean-wheel smoke",
        "detail": "One matching Qt runtime",
        "status": "Queued",
    },
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt ListView model example."
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


class TaskModel(QAbstractListModel):
    """A pure-Python model consumed directly by the native C++ ListView."""

    def __init__(self, tasks=None):
        super().__init__()
        self._tasks = [dict(task) for task in (tasks or INITIAL_TASKS)]
        self._next_number = 1

    def rowCount(self, parent=QModelIndex()):
        return 0 if parent.isValid() else len(self._tasks)

    def data(self, index, role=Qt.DisplayRole):
        if not index.isValid() or not 0 <= index.row() < len(self._tasks):
            return None
        task = self._tasks[index.row()]
        if role == Qt.DisplayRole:
            return task["title"]
        if role == DETAIL_ROLE:
            return task["detail"]
        if role == STATUS_ROLE:
            return task["status"]
        return None

    def add_task(self):
        row = len(self._tasks)
        self.beginInsertRows(QModelIndex(), row, row)
        self._tasks.append(
            {
                "title": "Python task {0}".format(self._next_number),
                "detail": "Inserted through beginInsertRows()",
                "status": "Queued",
            }
        )
        self._next_number += 1
        self.endInsertRows()
        return row

    def advance_task(self, row):
        if not 0 <= row < len(self._tasks):
            return False
        states = ("Queued", "In progress", "Done")
        task = self._tasks[row]
        task["status"] = states[(states.index(task["status"]) + 1) % len(states)]
        index = self.index(row, 0)
        self.dataChanged.emit(index, index, [STATUS_ROLE])
        return True

    def remove_task(self, row):
        if not 0 <= row < len(self._tasks):
            return False
        self.beginRemoveRows(QModelIndex(), row, row)
        self._tasks.pop(row)
        self.endRemoveRows()
        return True

    def reset_tasks(self):
        self.beginResetModel()
        self._tasks = [dict(task) for task in INITIAL_TASKS]
        self._next_number = 1
        self.endResetModel()


class TaskDelegate(QStyledItemDelegate):
    """Python virtual paint/sizeHint implementation for native list rows."""

    def sizeHint(self, option, index):
        del option, index
        return QSize(520, 62)

    def paint(self, painter, option, index):
        painter.save()
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)
        painter.setClipRect(option.rect)

        dark = fluentqt.theme_uses_dark_appearance(fluentqt.current_theme())
        accent = QColor(fluentqt.accent_color())
        text_primary = QColor("#f5f5f5" if dark else "#1b1b1b")
        text_secondary = QColor("#c8c8c8" if dark else "#666666")
        row_rect = QRectF(option.rect).adjusted(3.0, 2.0, -3.0, -2.0)

        selected = bool(option.state & QStyle.State_Selected)
        hovered = bool(option.state & QStyle.State_MouseOver)
        if selected or hovered:
            fill = QColor(accent if selected else text_primary)
            fill.setAlpha(38 if selected else 14)
            painter.setPen(Qt.NoPen)
            painter.setBrush(fill)
            painter.drawRoundedRect(row_rect, 6.0, 6.0)

        status = index.data(STATUS_ROLE) or "Queued"
        status_color = {
            "Done": QColor("#107c10"),
            "In progress": accent,
            "Queued": QColor("#737373"),
        }[status]
        badge_font = fluentqt.font_for_role(fluentqt.FontRole.Caption)
        painter.setFont(badge_font)
        badge_width = painter.fontMetrics().horizontalAdvance(status) + 20
        badge_rect = QRectF(
            row_rect.right() - badge_width - 14,
            row_rect.center().y() - 13,
            badge_width,
            26,
        )
        badge_fill = QColor(status_color)
        badge_fill.setAlpha(32 if dark else 22)
        painter.setPen(Qt.NoPen)
        painter.setBrush(badge_fill)
        painter.drawRoundedRect(badge_rect, 13.0, 13.0)
        painter.setPen(status_color)
        painter.drawText(badge_rect, Qt.AlignCenter, status)

        text_right = badge_rect.left() - 16
        title_rect = QRectF(
            row_rect.left() + 18,
            row_rect.top() + 9,
            text_right - row_rect.left() - 18,
            22,
        )
        detail_rect = QRectF(
            title_rect.left(),
            title_rect.bottom() + 1,
            title_rect.width(),
            20,
        )

        painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.BodyStrong))
        painter.setPen(text_primary)
        title = painter.fontMetrics().elidedText(
            str(index.data(Qt.DisplayRole)),
            Qt.ElideRight,
            int(title_rect.width()),
        )
        painter.drawText(title_rect, Qt.AlignLeft | Qt.AlignVCenter, title)

        painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.Caption))
        painter.setPen(text_secondary)
        detail = painter.fontMetrics().elidedText(
            str(index.data(DETAIL_ROLE)),
            Qt.ElideRight,
            int(detail_rect.width()),
        )
        painter.drawText(detail_rect, Qt.AlignLeft | Qt.AlignVCenter, detail)
        painter.restore()


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 ListView")
    window.resize(760, 610)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = fluentqt.Label("Native ListView + Python model", page)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = fluentqt.Label(
        "The C++ Fluent view consumes a parentless Python QAbstractListModel "
        "and QStyledItemDelegate. Use the actions below to drive real model "
        "insert, dataChanged, remove, and reset notifications.",
        page,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(fluentqt.Divider(page))

    model = TaskModel()
    delegate = TaskDelegate()
    view = fluentqt.ListView(page, selectionMode=fluentqt.SelectionMode.Single)
    view.setModel(model)
    view.setItemDelegate(delegate)
    view.setUniformItemSizes(True)
    view.setSpacing(2)
    view.setHeaderText("Python work queue")
    view.setFooterText("Model and delegate remain caller-owned")
    view.setPlaceholderText("No Python rows")
    view.setSelectedIndicatorAnimationEnabled(False)
    view.setSelectedIndex(1)
    root.addWidget(view, 1)

    status = fluentqt.Label(page)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(status)

    actions = QHBoxLayout()
    reset_button = fluentqt.Button("Reset model", page)
    remove_button = fluentqt.Button("Remove selected", page)
    advance_button = fluentqt.Button("Advance status", page)
    add_button = fluentqt.Button("Insert task", page)
    add_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    actions.addStretch()
    actions.addWidget(reset_button)
    actions.addWidget(remove_button)
    actions.addWidget(advance_button)
    actions.addWidget(add_button)
    root.addLayout(actions)

    def update_status(*_args):
        row = view.selectedIndex()
        count = model.rowCount()
        view.setHeaderText(
            "Python work queue · {0} row{1}".format(
                count,
                "" if count == 1 else "s",
            )
        )
        if row < 0:
            status.setText("No row selected")
            return
        status.setText(
            "Row {0} selected · {1}".format(
                row + 1,
                model.data(model.index(row, 0), STATUS_ROLE),
            )
        )

    def add_task():
        row = model.add_task()
        view.setSelectedIndex(row)
        update_status()

    def advance_task():
        if model.advance_task(view.selectedIndex()):
            update_status()

    def remove_task():
        row = view.selectedIndex()
        if not model.remove_task(row):
            return
        if model.rowCount() > 0:
            view.setSelectedIndex(min(row, model.rowCount() - 1))
        update_status()

    def reset_tasks():
        model.reset_tasks()
        view.setSelectedIndex(1)
        update_status()

    add_button.clicked.connect(add_task)
    advance_button.clicked.connect(advance_task)
    remove_button.clicked.connect(remove_task)
    reset_button.clicked.connect(reset_tasks)
    view.itemClicked.connect(update_status)
    view.selectionModel().currentChanged.connect(update_status)
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

        QTimer.singleShot(250, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
