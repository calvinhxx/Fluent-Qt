"""Interactive and snapshot acceptance example for TreeView model interop."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QRectF, QSize, Qt, QTimer
from PySide6.QtGui import QColor, QPainter, QPen, QStandardItem, QStandardItemModel
from PySide6.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QStyle,
    QStyledItemDelegate,
    QVBoxLayout,
    QWidget,
)


KIND_ROLE = Qt.UserRole + 1
DETAIL_ROLE = Qt.UserRole + 2


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt TreeView model example."
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


def node(text, kind, detail, children=()):
    item = QStandardItem(text)
    item.setData(kind, KIND_ROLE)
    item.setData(detail, DETAIL_ROLE)
    for child in children:
        item.appendRow(child)
    return item


def populate_model(model):
    model.clear()
    model.appendRow(
        node(
            "Design system",
            "workspace",
            "12 components",
            (
                node(
                    "Components",
                    "folder",
                    "In progress",
                    (
                        node("Button states", "file", "Reviewed"),
                        node("TreeView binding", "file", "Active"),
                    ),
                ),
                node(
                    "Design tokens",
                    "folder",
                    "Synchronized",
                    (
                        node("Color roles", "file", "42 tokens"),
                        node("Typography", "file", "9 roles"),
                    ),
                ),
            ),
        )
    )
    model.appendRow(
        node(
            "Platform validation",
            "workspace",
            "3 environments",
            (
                node("macOS · Qt 6.9.3", "platform", "Local"),
                node("Windows · Qt 6.2.4", "platform", "CI"),
                node("Linux · Qt 6.2.4", "platform", "CI"),
            ),
        )
    )
    model.appendRow(
        node(
            "Release package",
            "workspace",
            "Ready",
            (
                node("Wheel smoke", "file", "Passed"),
                node("API manifest", "file", "Tracked"),
            ),
        )
    )


class WorkspaceTreeDelegate(QStyledItemDelegate):
    """Python delegate for the native TreeView rows."""

    def sizeHint(self, option, index):
        del option, index
        return QSize(280, 42)

    def paint(self, painter, option, index):
        painter.save()
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)

        dark = fluentqt.current_theme() == fluentqt.Theme.Dark
        accent = QColor(fluentqt.accent_color())
        primary = QColor("#f5f5f5" if dark else "#1b1b1b")
        secondary = QColor("#c8c8c8" if dark else "#666666")
        row = QRectF(option.rect).adjusted(3.0, 2.0, -3.0, -2.0)
        selected = bool(option.state & QStyle.State_Selected)
        hovered = bool(option.state & QStyle.State_MouseOver)

        if selected or hovered:
            fill = QColor(accent if selected else primary)
            fill.setAlpha(30 if selected else 13)
            painter.setPen(Qt.NoPen)
            painter.setBrush(fill)
            painter.drawRoundedRect(row, 6.0, 6.0)

        kind = str(index.data(KIND_ROLE) or "file")
        icon_color = {
            "workspace": accent,
            "folder": QColor("#d29a34"),
            "platform": QColor("#3a9c7a"),
            "file": QColor("#6787c7"),
        }.get(kind, accent)
        icon_rect = QRectF(row.left() + 9.0, row.top() + 9.0, 18.0, 18.0)
        painter.setPen(QPen(icon_color.darker(115), 1.0))
        painter.setBrush(icon_color)
        painter.drawRoundedRect(icon_rect, 4.0, 4.0)
        painter.setPen(QPen(QColor("#ffffff"), 1.4))
        painter.drawLine(
            icon_rect.left() + 5.0,
            icon_rect.center().y(),
            icon_rect.right() - 5.0,
            icon_rect.center().y(),
        )

        text_left = icon_rect.right() + 10.0
        detail = str(index.data(DETAIL_ROLE) or "")
        detail_width = min(
            110.0,
            painter.fontMetrics().horizontalAdvance(detail) + 18.0,
        )
        title_rect = QRectF(
            text_left,
            row.top(),
            max(20.0, row.right() - text_left - detail_width - 10.0),
            row.height(),
        )
        painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
        painter.setPen(primary)
        painter.drawText(
            title_rect,
            Qt.AlignLeft | Qt.AlignVCenter,
            painter.fontMetrics().elidedText(
                str(index.data(Qt.DisplayRole)),
                Qt.ElideRight,
                int(title_rect.width()),
            ),
        )

        detail_rect = QRectF(
            row.right() - detail_width,
            row.top(),
            detail_width,
            row.height(),
        )
        painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.Caption))
        painter.setPen(secondary)
        painter.drawText(
            detail_rect,
            Qt.AlignRight | Qt.AlignVCenter,
            painter.fontMetrics().elidedText(
                detail,
                Qt.ElideRight,
                int(detail_rect.width()),
            ),
        )
        # TreeView paints the Fluent accent pill when its overlay indicator is
        # enabled, so this delegate deliberately does not draw a second bar.
        painter.restore()


def node_count(model, parent_index=None):
    if parent_index is None:
        parent_index = model.index(-1, -1)
    count = 0
    for row in range(model.rowCount(parent_index)):
        index = model.index(row, 0, parent_index)
        count += 1 + node_count(model, index)
    return count


def index_path(index):
    parts = []
    while index.isValid():
        parts.append(str(index.data(Qt.DisplayRole)))
        index = index.parent()
    return " / ".join(reversed(parts))


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 TreeView")
    window.resize(720, 650)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = fluentqt.Label("Native TreeView + Python hierarchy", page)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = fluentqt.Label(
        "A PySide QStandardItemModel and Python delegate drive native "
        "hierarchy, selection motion, expansion, and file-manager-style "
        "reordering.",
        page,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(fluentqt.Divider(page))

    model = QStandardItemModel()
    populate_model(model)
    delegate = WorkspaceTreeDelegate()
    view = fluentqt.TreeView(
        page,
        selectionMode=fluentqt.SelectionMode.Single,
        headerText="Python workspace",
        placeholderText="No hierarchy nodes",
    )
    view.setModel(model)
    view.setItemDelegate(delegate)
    view.setSelectionIndicatorVisible(True)
    view.setSelectionIndicatorInset(7.0)
    view.setCanReorderItems(True)
    view.expandAll()
    initial_index = model.index(1, 0, model.index(0, 0))
    view.setSelectedItem(initial_index)
    root.addWidget(view, 1)

    status = fluentqt.Label(page)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(status)

    actions = QHBoxLayout()
    collapse_button = fluentqt.Button("Collapse all", page)
    expand_button = fluentqt.Button("Expand all", page)
    add_button = fluentqt.Button("Add child", page)
    add_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    actions.addStretch()
    actions.addWidget(collapse_button)
    actions.addWidget(expand_button)
    actions.addWidget(add_button)
    root.addLayout(actions)

    next_child = {"value": 1}

    def update_status(*_args):
        current = view.selectedItem()
        status.setText(
            "{0} nodes · {1}".format(
                node_count(model),
                index_path(current) if current.isValid() else "No selection",
            )
        )

    def add_child():
        current = view.selectedItem()
        parent_item = (
            model.itemFromIndex(current)
            if current.isValid()
            else model.invisibleRootItem()
        )
        number = next_child["value"]
        next_child["value"] += 1
        inserted = node(
            "Python child {0}".format(number),
            "file",
            "Inserted",
        )
        parent_item.appendRow(inserted)
        if current.isValid():
            view.setExpanded(current, True)
        view.setSelectedItem(inserted.index())
        update_status()

    collapse_button.clicked.connect(view.collapseAll)
    expand_button.clicked.connect(view.expandAll)
    add_button.clicked.connect(add_child)
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
