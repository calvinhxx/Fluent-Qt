"""Offline release history for the Gallery title-bar action."""

from collections import OrderedDict
from dataclasses import dataclass
import json
from math import ceil
from html import escape

from PySide6.QtCore import (
    QAbstractListModel, QCoreApplication, QDate, QEvent, QLocale, QModelIndex, QPoint, QPointF,
    QRect, QSize, Qt, QTimer, QUrl, QVersionNumber,
)
from PySide6.QtGui import QFontMetrics, QPainter, QStaticText, QTransform
from PySide6.QtWidgets import QHBoxLayout, QListView, QStyledItemDelegate, QVBoxLayout

import fluentqt

from .foundation_pages import _theme_snapshot
from .visual import asset_path, gallery_colors


ANNOUNCEMENT_ICON = "ic_fluent_megaphone_20_regular"


def _entries():
    path = asset_path("release-history.json")
    if not path.is_file():
        # A direct source run uses app/assets instead of a staged wheel.
        path = path.parent.parent.parent / "tools" / "site" / path.name
    try:
        versions = json.loads(path.read_text(encoding="utf-8"))["versions"]
    except (OSError, ValueError, KeyError):
        return
    installed = QVersionNumber.fromString(QCoreApplication.applicationVersion())
    versions.sort(key=lambda release: QVersionNumber.fromString(release["version"]), reverse=True)
    for release in versions:
        version = release["version"]
        number = QVersionNumber.fromString(version)
        bundled_candidate = release["status"] == "candidate" and number == installed
        if not installed.isNull() and number > installed:
            continue
        if release["status"] != "published" and not bundled_candidate:
            continue
        for group in release["groups"]:
            if group["scope"] != "gallery":
                continue
            summary = group["summary"]["en"]
            headline = group.get("headline", {}).get("en")
            paragraphs = [change["en"] for change in group["changes"]] or ([summary] if headline else [])
            date = QDate.fromString((release["published_at"] or "")[:10], Qt.DateFormat.ISODate)
            yield date, version, headline or summary, paragraphs


class _HistoryModel(QAbstractListModel):
    Date = Qt.UserRole + 1
    Version = Qt.UserRole + 2
    Paragraphs = Qt.UserRole + 3

    def __init__(self, parent):
        super().__init__(parent)
        self._rows = []
        locale = QLocale(QLocale.Language.English)
        for date, version, headline, paragraphs in _entries():
            date_text = locale.toString(date, "MMMM d, yyyy") if date.isValid() else "This version"
            self._rows.append((date_text, "v" + version, headline.replace("`", ""),
                               [text.replace("`", "") for text in paragraphs]))

    def rowCount(self, parent=QModelIndex()):
        return 0 if parent.isValid() else len(self._rows)

    def data(self, index, role=Qt.DisplayRole):
        if not index.isValid() or index.row() >= len(self._rows):
            return None
        date, version, headline, paragraphs = self._rows[index.row()]
        if role == Qt.DisplayRole:
            return headline
        if role == Qt.AccessibleTextRole:
            return ", ".join((date, version, headline))
        if role == Qt.AccessibleDescriptionRole:
            return "\n".join(paragraphs)
        if role == self.Date:
            return date
        if role == self.Version:
            return version
        if role == self.Paragraphs:
            return paragraphs
        return None


@dataclass
class _RowLayout:
    inset: int
    meta_top: int
    meta_height: int
    blocks: list
    height: int


class _HistoryDelegate(QStyledItemDelegate):
    def __init__(self, view):
        super().__init__(view)
        self._view = view
        self._rows = OrderedDict()
        self._heights = {}
        self._viewport_key = None
        self._fonts = ()
        self.refresh_theme()

    def refresh_theme(self):
        snapshot = _theme_snapshot(self._view)
        self._colors = snapshot["colors"]
        self._radius = snapshot["radius"]["control"]
        fonts = tuple(fluentqt.font_for_role(role) for role in (
            fluentqt.FontRole.Caption, fluentqt.FontRole.BodyLargeStrong, fluentqt.FontRole.Body,
        ))
        if self._fonts == fonts:
            return False
        self._fonts = fonts
        self._viewport_key = None
        return True

    def _sync_viewport(self):
        key = (self._view.viewport().width(), self._view.devicePixelRatioF())
        if key != self._viewport_key:
            self._viewport_key = key
            self._rows.clear()
            self._heights.clear()

    def _layout(self, index):
        number = index.row()
        if number in self._rows:
            self._rows.move_to_end(number)
            return self._rows[number]

        width = self._viewport_key[0]
        compact = width < 558
        inset = 22 if compact else 28
        meta_top = (16 if compact else 20) if number == 0 else 14
        caption, title, body = self._fonts
        meta_height = QFontMetrics(caption).height() + 6
        bottom = meta_top + meta_height
        blocks = []

        def append(text, text_format, font):
            nonlocal bottom
            block = QStaticText(text)
            block.setTextFormat(text_format)
            block.setTextWidth(max(1, width - 2 * inset))
            block.prepare(QTransform(), font)
            top = bottom + 9
            blocks.append((top, block))
            bottom = top + ceil(block.size().height())

        append(index.data(), Qt.PlainText, title)
        for paragraph in index.data(_HistoryModel.Paragraphs):
            append(f"<p style='margin:0; line-height:125%'>{escape(paragraph)}</p>", Qt.RichText, body)
        height = bottom + (20 if number + 1 == index.model().rowCount() else 21)
        layout = _RowLayout(inset, meta_top, meta_height, blocks, height)
        self._heights[number] = height
        self._rows[number] = layout
        if len(self._rows) > 32:
            self._rows.popitem(last=False)
        return layout

    def sizeHint(self, option, index):
        self._sync_viewport()
        height = self._heights.get(index.row())
        if height is None:
            height = self._layout(index).height
        return QSize(self._viewport_key[0], height)

    def paint(self, painter, option, index):
        self._sync_viewport()
        row = self._layout(index)
        width = option.rect.width() - 2 * row.inset
        colors = self._colors
        caption, title, body = self._fonts
        painter.save()
        painter.setClipRect(option.rect, Qt.IntersectClip)
        painter.translate(option.rect.topLeft() + QPoint(row.inset, 0))
        if index.row():
            painter.fillRect(0, 0, width, 1, colors["strokeDivider"])
        painter.setFont(caption)
        version = index.data(_HistoryModel.Version)
        badge_width = QFontMetrics(caption).horizontalAdvance(version) + 18
        badge = QRect(width - badge_width, row.meta_top, badge_width, row.meta_height)
        painter.setPen(colors["textSecondary"])
        painter.drawText(QRect(0, row.meta_top, badge.left(), row.meta_height),
                         Qt.AlignLeft | Qt.AlignVCenter, index.data(_HistoryModel.Date))
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setPen(Qt.NoPen)
        painter.setBrush(colors["bgCanvas"])
        painter.drawRoundedRect(badge, self._radius, self._radius)
        painter.setPen(colors["textSecondary"])
        painter.drawText(badge, Qt.AlignCenter, version)
        for i, (top, block) in enumerate(row.blocks):
            painter.setFont(title if i == 0 else body)
            painter.setPen(colors["textPrimary"] if i == 0 else colors["textSecondary"])
            painter.drawStaticText(QPointF(0, top), block)
        painter.restore()


class _HistoryView(fluentqt.ListView):
    def __init__(self, parent):
        super().__init__(parent)
        self.setObjectName("GalleryWhatsNew.History")
        self.setAccessibleName("Gallery release history")
        self.setSpacing(0)
        self.setBorderVisible(False)
        self.setSelectionMode(fluentqt.SelectionMode.None_)
        self.setSelectionIndicatorVisible(False)
        self.setResizeMode(QListView.ResizeMode.Adjust)
        self._delegate = _HistoryDelegate(self)
        self.setItemDelegate(self._delegate)
        self.setModel(_HistoryModel(self))
        if not self.model().rowCount():
            self.setAccessibleDescription("No Gallery updates for this version.")

    def onThemeUpdated(self):
        super().onThemeUpdated()
        delegate = getattr(self, "_delegate", None)
        if delegate and delegate.refresh_theme():
            self.scheduleDelayedItemsLayout()

    def paintEvent(self, event):
        super().paintEvent(event)
        if self.model() is not None and not self.model().rowCount():
            viewport = self.viewport()
            compact = viewport.width() < 558
            with QPainter(viewport) as painter:
                painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
                painter.setPen(_theme_snapshot(self)["colors"]["textSecondary"])
                painter.drawText(viewport.rect().adjusted(22 if compact else 28, 16 if compact else 20, 0, 0),
                                 Qt.AlignLeft | Qt.AlignTop, self.accessibleDescription())

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if event.size().width() != event.oldSize().width():
            self.scheduleDelayedItemsLayout()


class GalleryWhatsNew(fluentqt.Dialog):
    def __init__(self, trigger):
        super().__init__(trigger.window())
        self._host = trigger.window()
        self.setObjectName("GalleryWhatsNew")
        self.setAccessibleName("What’s new")
        self.setThemeSource(trigger)
        self.setSmokeEnabled(True)
        self.setBackdropBlurRadius(0)
        self.setDimOpacity(0.27)
        self.setDragEnabled(False)
        content = QVBoxLayout(self)
        content.setContentsMargins(1, 1, 1, 1)
        content.setSpacing(0)
        self._header = QHBoxLayout()
        self._header.setSpacing(12)
        tile = fluentqt.Card(self)
        tile.setBorderVisible(False)
        tile.setAppearance(fluentqt.Card.Appearance.Canvas)
        tile.setFixedSize(32, 32)
        icon_layout = QHBoxLayout(tile)
        icon_layout.setContentsMargins(8, 8, 8, 8)
        icon = fluentqt.FontIcon(ANNOUNCEMENT_ICON, tile)
        icon.setColor(gallery_colors().accent)
        icon_layout.addWidget(icon)
        self._header.addWidget(tile)
        title = fluentqt.Label("What’s new", self)
        title.setFluentTypography(fluentqt.FontRole.Subtitle)
        font = title.font()
        font.setPixelSize(24)
        title.setFont(font)
        self._header.addWidget(title, 1)
        close = fluentqt.Button("", self)
        close.setObjectName("GalleryWhatsNew.Close")
        close.setAccessibleName("Close release history")
        close.setToolTip(close.accessibleName())
        close.setFluentStyle(fluentqt.Button.ButtonStyle.Subtle)
        close.setFluentLayout(fluentqt.Button.ButtonLayout.IconOnly)
        close.setIconGlyph(fluentqt.Icons.ChromeClose, 16)
        close.setFixedSize(32, 32)
        close.clicked.connect(self.reject)
        self._header.addWidget(close)
        content.addLayout(self._header)
        content.addWidget(fluentqt.Divider(self))
        content.addWidget(_HistoryView(self), 1)
        content.addWidget(fluentqt.Divider(self))
        footer = QHBoxLayout()
        footer.setContentsMargins(18, 5, 16, 5)
        history = fluentqt.HyperlinkButton("Full release history", self)
        history.setUrl(QUrl("https://calvinhxx.github.io/Fluent-Qt/api/releases/"))
        history.setFluentLayout(fluentqt.Button.ButtonLayout.IconAfter)
        history.setIconGlyph("ic_fluent_arrow_up_right_16_regular", 12)
        footer.addWidget(history)
        footer.addStretch()
        content.addLayout(footer)
        self._host.installEventFilter(self)

        def finish_opening():
            self._fit_window()
            close.setFocus(Qt.FocusReason.OtherFocusReason)

        self.opened.connect(finish_opening)
        self.finished.connect(lambda: trigger.setFocus(Qt.FocusReason.OtherFocusReason))
        self._fit_window()

    def eventFilter(self, watched, event):
        surface_changed = (
            event.type() == QEvent.Type.DynamicPropertyChange
            and event.propertyName() == b"fluentOverlaySurfaceRect"
        )
        if watched is self._host and (event.type() == QEvent.Type.Resize or surface_changed) and self.isVisible():
            QTimer.singleShot(0, self, self._fit_window)
        return super().eventFilter(watched, event)

    def showEvent(self, event):
        super().showEvent(event)
        self._fit_window()

    def _fit_window(self):
        surface = self._host.property("fluentOverlaySurfaceRect")
        if not isinstance(surface, QRect) or surface.isEmpty():
            surface = self._host.rect()
        bar = self._host.titleBar()
        top = bar.mapTo(self._host, QPoint(0, bar.height())).y() if bar.isVisible() else surface.top()
        width = max(1, min(560, surface.width() - 32))
        height = max(1, min(526, surface.bottom() + 1 - top - 28))
        compact = width < 560
        header_margins = (20, 13, 14, 13) if compact else (26, 18, 18, 18)
        self._header.setContentsMargins(*header_margins)
        x = surface.x() + (surface.width() - width) // 2
        y = max(top + 12, surface.y() + (surface.height() - height) // 2)
        shadow = self.shadowSize()
        self.setGeometry(QRect(x, y, width, height).adjusted(-shadow, -shadow, shadow, shadow))
