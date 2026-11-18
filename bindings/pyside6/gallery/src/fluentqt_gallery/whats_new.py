"""Offline release history for the Gallery title-bar action."""

import json
from html import escape

from PySide6.QtCore import (
    QCoreApplication, QDate, QEvent, QLocale, QPoint, QRect, Qt, QTimer, QUrl, QVersionNumber,
)
from PySide6.QtWidgets import QHBoxLayout, QVBoxLayout

import fluentqt

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


def _label(text, role, parent, secondary=False):
    label = fluentqt.Label(text.replace("`", ""), parent)
    label.setTextFormat(Qt.TextFormat.PlainText)
    label.setFluentTypography(role)
    if secondary:
        label.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
    return label


def _populate_history(entries, page):
    for date, version, headline, paragraphs in _entries():
        if entries.count():
            entries.addSpacing(12)
            entries.addWidget(fluentqt.Divider(page))
            entries.addSpacing(4)
        meta = QHBoxLayout()
        date_text = QLocale(QLocale.Language.English).toString(date, "MMMM d, yyyy") if date.isValid() else "This version"
        meta.addWidget(_label(date_text, fluentqt.FontRole.Caption, page, True))
        meta.addStretch()
        badge = fluentqt.Card(page)
        badge.setAppearance(fluentqt.Card.Appearance.Canvas)
        badge.setBorderVisible(False)
        badge_layout = QHBoxLayout(badge)
        badge_layout.setContentsMargins(9, 3, 9, 3)
        badge_layout.addWidget(_label("v" + version, fluentqt.FontRole.Caption, badge, True))
        meta.addWidget(badge)
        entries.addLayout(meta)
        title = _label(headline, fluentqt.FontRole.BodyLargeStrong, page)
        title.setWordWrap(True)
        entries.addWidget(title)
        for paragraph in paragraphs:
            body = _label(paragraph, fluentqt.FontRole.Body, page, True)
            body.setTextFormat(Qt.TextFormat.RichText)
            body.setText(f"<p style='margin:0; line-height:125%'>{escape(body.text())}</p>")
            body.setWordWrap(True)
            entries.addWidget(body)
    if not entries.count():
        entries.addWidget(_label("No Gallery updates for this version.", fluentqt.FontRole.Body, page, True))
    entries.addStretch()


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
        title = _label("What’s new", fluentqt.FontRole.Subtitle, self)
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
        scroll = fluentqt.ScrollView(self)
        scroll.setObjectName("GalleryWhatsNew.History")
        scroll.setAccessibleName("Gallery release history")
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarVisibility(fluentqt.ScrollView.ScrollBarVisibility.Hidden)
        page = fluentqt.Card(scroll)
        page.setBorderVisible(False)
        self._entries = QVBoxLayout(page)
        self._entries.setSpacing(9)
        _populate_history(self._entries, page)
        scroll.setWidget(page)
        page.setAutoFillBackground(False)
        content.addWidget(scroll, 1)
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
            scroll.viewport().update()
            self.update()
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
        entry_margins = (22, 16, 22, 20) if compact else (28, 20, 28, 20)
        self._header.setContentsMargins(*header_margins)
        self._entries.setContentsMargins(*entry_margins)
        x = surface.x() + (surface.width() - width) // 2
        y = max(top + 12, surface.y() + (surface.height() - height) // 2)
        shadow = self.shadowSize()
        self.setGeometry(QRect(x, y, width, height).adjusted(-shadow, -shadow, shadow, shadow))
