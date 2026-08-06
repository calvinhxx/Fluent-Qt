"""Standalone app visuals matching the native C++ Gallery shell."""

from __future__ import annotations

from dataclasses import dataclass
import html
import io
import json
import keyword
import math
from pathlib import Path
import re
import sys
import token
import tokenize
from typing import Callable, Iterable

import fluentqt
from PySide6.QtCore import (
    QEasingCurve,
    QEvent,
    QModelIndex,
    QMargins,
    QObject,
    QPersistentModelIndex,
    QPoint,
    QPointF,
    QPropertyAnimation,
    QRect,
    QRectF,
    QSize,
    QSizeF,
    Qt,
    QTimer,
    QUrl,
    QVariantAnimation,
    Signal,
)
from PySide6.QtGui import (
    QBrush,
    QColor,
    QDesktopServices,
    QFont,
    QFontDatabase,
    QFontMetrics,
    QIcon,
    QImage,
    QLinearGradient,
    QPainter,
    QPainterPath,
    QPalette,
    QPen,
    QPixmap,
    QRadialGradient,
    QStandardItem,
    QStandardItemModel,
)
from PySide6.QtWidgets import (
    QAbstractItemView,
    QApplication,
    QFrame,
    QGridLayout,
    QGraphicsOpacityEffect,
    QHBoxLayout,
    QLabel,
    QScrollArea,
    QSizePolicy,
    QStyle,
    QStyledItemDelegate,
    QVBoxLayout,
    QWidget,
)


@dataclass(frozen=True)
class GalleryColors:
    text_primary: QColor
    text_secondary: QColor
    layer: QColor
    layer_alt: QColor
    subtle: QColor
    subtle_tertiary: QColor
    stroke: QColor
    accent: QColor


def gallery_colors() -> GalleryColors:
    """Return the app palette used by the native Gallery-owned surfaces."""
    # Resolve the same live StyleThemeCatalog palette used by C++ so Gallery-
    # owned paint code follows Fluent, Material, and macOS style changes too.
    # Import lazily because the Foundation topic module also reuses visual.py.
    from .foundation_pages import _theme_tokens

    tokens = _theme_tokens()
    return GalleryColors(
        text_primary=QColor(tokens["textPrimary"]),
        text_secondary=QColor(tokens["textSecondary"]),
        layer=QColor(tokens["bgLayer"]),
        layer_alt=QColor(tokens["bgLayerAlt"]),
        subtle=QColor(tokens["subtleSecondary"]),
        subtle_tertiary=QColor(tokens["subtleTertiary"]),
        stroke=QColor(tokens["strokeCard"]),
        accent=QColor(tokens["accentDefault"]),
    )


def css_color(color: QColor) -> str:
    return "rgba({0}, {1}, {2}, {3})".format(
        color.red(), color.green(), color.blue(), color.alpha()
    )


def _single_shot(
    delay_ms: int,
    context: QObject,
    callback: Callable[[], None],
) -> QTimer:
    """Schedule a context-owned callback on every supported PySide6."""

    timer = QTimer(context)
    timer.setSingleShot(True)
    timer.timeout.connect(callback)
    timer.timeout.connect(timer.deleteLater)
    timer.start(max(0, int(delay_ms)))
    return timer


def _font_style_strategy(
    *strategies: QFont.StyleStrategy,
) -> QFont.StyleStrategy:
    """Combine QFont flags without leaking an ``int`` on PySide6 6.2."""

    combined = 0
    for strategy in strategies:
        combined |= int(getattr(strategy, "value", strategy))
    return QFont.StyleStrategy(combined)


def _qround(value: float) -> int:
    return (
        math.floor(value + 0.5)
        if value >= 0.0
        else math.ceil(value - 0.5)
    )


def _normalized_device_pixel_ratio(value: float) -> float:
    """Return the same minimum DPR used by the native Gallery."""

    return max(1.0, float(value))


def _primary_screen_device_pixel_ratio() -> float:
    screen = QApplication.primaryScreen()
    return _normalized_device_pixel_ratio(
        screen.devicePixelRatio() if screen is not None else 1.0
    )


def asset_root() -> Path:
    """Resolve packaged assets, with a direct-source checkout fallback."""

    packaged = Path(__file__).resolve().with_name("assets")
    if packaged.is_dir():
        return packaged
    for parent in Path(__file__).resolve().parents:
        checkout = parent / "app" / "assets"
        if checkout.is_dir():
            return checkout
    return packaged


def asset_path(*parts: str) -> Path:
    return asset_root().joinpath(*parts)


def _macos_dock_icon_pixmap(source: QPixmap) -> QPixmap:
    """Match the native Gallery's macOS Dock icon visual footprint."""

    if source.isNull():
        return source
    canvas_size = 256
    content_fraction = 0.88
    inner_size = _qround(canvas_size * content_fraction)
    offset = (canvas_size - inner_size) // 2
    padded = QPixmap(canvas_size, canvas_size)
    padded.fill(Qt.GlobalColor.transparent)
    painter = QPainter(padded)
    painter.setRenderHint(QPainter.SmoothPixmapTransform)
    painter.drawPixmap(
        QRect(offset, offset, inner_size, inner_size),
        source,
        source.rect(),
    )
    painter.end()
    return padded


def app_icon() -> QIcon:
    icon_path = str(asset_path("app-icon.png"))
    if sys.platform != "darwin":
        return QIcon(icon_path)
    source = QPixmap(icon_path)
    if source.isNull():
        return QIcon(icon_path)
    return QIcon(_macos_dock_icon_pixmap(source))


def app_icon_pixmap(
    size: int,
    device_pixel_ratio: float = 1.0,
) -> QPixmap:
    """Return a DPR-tagged app icon with ``size`` logical pixels."""

    logical_size = max(1, int(size))
    dpr = _normalized_device_pixel_ratio(device_pixel_ratio)
    physical_size = max(1, _qround(logical_size * dpr))
    source = QPixmap(str(asset_path("app-icon.png")))
    if source.isNull():
        return source
    scaled = source.scaled(
        physical_size,
        physical_size,
        Qt.KeepAspectRatio,
        Qt.SmoothTransformation,
    )
    scaled.setDevicePixelRatio(dpr)
    return scaled


def _draw_pixmap_in_logical_rect(
    painter: QPainter,
    logical_rect: QRectF | QRect,
    source: QPixmap,
) -> None:
    """Mirror ``fluentDrawPixmapInLogicalRect`` for Gallery-owned images."""

    rect = QRectF(logical_rect)
    if rect.isEmpty() or source.isNull():
        return
    device = painter.device()
    dpr = max(
        1.0,
        float(device.devicePixelRatioF()) if device is not None else 1.0,
    )
    target = QSize(
        max(1, _qround(rect.width() * dpr)),
        max(1, _qround(rect.height() * dpr)),
    )
    scaled = QPixmap(source)
    if scaled.size() != target:
        scaled = source.scaled(
            target,
            Qt.KeepAspectRatio,
            Qt.SmoothTransformation,
        )
    scaled.setDevicePixelRatio(dpr)
    logical_size = QSizeF(scaled.size()) / dpr
    top_left = QPointF(
        rect.x() + (rect.width() - logical_size.width()) * 0.5,
        rect.y() + (rect.height() - logical_size.height()) * 0.5,
    )
    painter.drawPixmap(top_left, scaled)


ROUTE_ICON_NAMES = {
    "home": chr(0xE80F),
    "foundation": chr(0xE80A),
    "all-controls": chr(0xE890),
    "basic-input": chr(0xE73E),
    "collections": chr(0xE80A),
    "date-time": chr(0xE787),
    "dialogs-flyouts": chr(0xE8BD),
    "layout": chr(0xE8E4),
    "menus-toolbars": chr(0xE74E),
    "navigation": chr(0xE700),
    "scrolling": chr(0xE74B),
    "status-info": chr(0xE946),
    "text-fields": chr(0xE70F),
    "windowing": chr(0xE73F),
    "settings": chr(0xE713),
}


def route_icon_name(route_id: str) -> str:
    return ROUTE_ICON_NAMES.get(route_id, "ic_fluent_square_20_regular")


def control_image_path(category_id: str, title: str) -> Path:
    file_title = {"QML+": "QMLPlus"}.get(title, title)
    candidate = asset_path("control_images", category_id, file_title + ".png")
    if candidate.is_file():
        return candidate
    return asset_path("control_images", "Placeholder.png")


class _GalleryIconTile(QWidget):
    """Exact app-owned ``GalleryIconTile`` paint path."""

    def __init__(
        self,
        pixmap: QPixmap,
        glyph: str,
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self._pixmap = QPixmap(pixmap)
        self._glyph = glyph
        self.setObjectName("galleryIconTile")
        self.setFixedSize(40, 40)
        self.setAttribute(Qt.WA_TransparentForMouseEvents)

    def paintEvent(self, event) -> None:
        del event
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)
        if self._glyph:
            colors = gallery_colors()
            painter.setPen(Qt.NoPen)
            painter.setBrush(colors.subtle)
            painter.drawRoundedRect(self.rect(), 4.0, 4.0)
            _draw_native_font_icon(
                painter,
                self.rect(),
                self._glyph,
                20,
                colors.text_primary,
            )
        elif not self._pixmap.isNull():
            _draw_pixmap_in_logical_rect(painter, self.rect(), self._pixmap)


class GalleryEntryCard(QFrame):
    """Native-style icon, title, and description route card."""

    activated = Signal(str)

    def __init__(
        self,
        route_id: str,
        title: str,
        description: str,
        image_path: Path | None = None,
        icon_name: str = "",
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self._route_id = route_id
        self._entry_title = title
        self._entry_description = description
        self._entry_icon_name = icon_name or route_icon_name(route_id)
        self._entry_pixmap = (
            QPixmap(str(image_path))
            if image_path is not None and image_path.is_file()
            else QPixmap()
        )
        self.setObjectName("galleryEntryCard")
        self.setProperty("galleryTargetRouteId", route_id)
        self.setFrameShape(QFrame.NoFrame)
        self.setCursor(Qt.PointingHandCursor)
        self.setMinimumHeight(86)

        layout = QHBoxLayout(self)
        layout.setContentsMargins(16, 16, 16, 16)
        layout.setSpacing(16)

        tile = _GalleryIconTile(
            self._entry_pixmap,
            "" if not self._entry_pixmap.isNull() else self._entry_icon_name,
            self,
        )
        layout.addWidget(tile, 0, Qt.AlignTop)
        self._icon = tile

        text_column = QVBoxLayout()
        text_column.setContentsMargins(0, 0, 0, 0)
        text_column.setSpacing(3)
        title_label = fluentqt.Label(title, self)
        title_label.setObjectName("galleryEntryCardTitle")
        title_label.setProperty("galleryTargetRouteId", route_id)
        title_label.setFluentTypography(fluentqt.FontRole.BodyStrong)
        text_column.addWidget(title_label)
        description_label = None
        if description:
            description_label = fluentqt.Label(description, self)
            description_label.setObjectName("galleryEntryCardDescription")
            description_label.setFluentTypography(fluentqt.FontRole.Caption)
            description_label.setWordWrap(True)
            text_column.addWidget(description_label)
        text_column.addStretch()
        layout.addLayout(text_column, 1)

        self._title_label = title_label
        self._description_label = description_label
        self.refresh_theme()

    def mouseReleaseEvent(self, event) -> None:
        if event.button() == Qt.LeftButton and self.rect().contains(event.pos()):
            self.activated.emit(self._route_id)
        super().mouseReleaseEvent(event)

    def refresh_theme(self) -> None:
        colors = gallery_colors()
        self.setStyleSheet(
            "#galleryEntryCard {{ background: {0}; border: 1px solid {1}; "
            "border-radius: 8px; }}"
            "#galleryEntryCard:hover {{ background: {2}; }}".format(
                css_color(colors.layer),
                css_color(colors.stroke),
                css_color(colors.subtle),
            )
        )
        self._title_label.setStyleSheet(
            "color: {0}; background: transparent;".format(
                css_color(colors.text_primary)
            )
        )
        if self._description_label is not None:
            self._description_label.setStyleSheet(
                "color: {0}; background: transparent;".format(
                    css_color(colors.text_secondary)
                )
            )
        self._icon.update()
        self.update()


class GalleryEntryGrid(QWidget):
    """Direct paint-port of the C++ GalleryEntryGrid."""

    GRID_SPACING = 12
    MIN_CARD_HEIGHT = 86
    CARD_PADDING = 16
    ICON_SIZE = 40
    ICON_TEXT_GAP = 16
    TITLE_DESC_GAP = 3
    MIN_CARD_WIDTH = 240
    MAX_COLUMNS = 4

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("galleryEntryGrid")
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        self.setMouseTracking(True)
        self.setCursor(Qt.PointingHandCursor)
        self._cards: list[GalleryEntryCard] = []
        self._row_heights: list[int] = []
        self._row_tops: list[int] = []
        self._hovered_index = -1
        self._last_columns = 0
        self._last_column_width = 0

    @property
    def cards(self) -> tuple[GalleryEntryCard, ...]:
        return tuple(self._cards)

    @property
    def columns(self) -> int:
        return self._column_count()

    def set_cards(self, cards: Iterable[GalleryEntryCard]) -> None:
        self._cards = list(cards)
        for card in self._cards:
            card.setParent(self)
            card.hide()
        self._hovered_index = -1
        self._recalculate_row_layout()
        self.updateGeometry()
        self.update()

    def _column_count(self) -> int:
        return max(
            1,
            min(
                self.MAX_COLUMNS,
                (self.width() + self.GRID_SPACING)
                // (self.MIN_CARD_WIDTH + self.GRID_SPACING),
            ),
        )

    def _row_count(self) -> int:
        if not self._cards:
            return 0
        columns = self._column_count()
        return (len(self._cards) + columns - 1) // columns

    def _column_width(self) -> int:
        columns = self._column_count()
        return max(
            0,
            (self.width() - (columns - 1) * self.GRID_SPACING) // columns,
        )

    def _grid_height(self) -> int:
        rows = self._row_count()
        if rows == 0:
            return 0
        if len(self._row_heights) == rows and len(self._row_tops) == rows:
            return self._row_tops[-1] + self._row_heights[-1]
        return (
            rows * self.MIN_CARD_HEIGHT
            + (rows - 1) * self.GRID_SPACING
        )

    def _card_rect(self, index: int) -> QRect:
        columns = self._column_count()
        row = index // columns
        column = index % columns
        width = self._column_width()
        x = column * (width + self.GRID_SPACING)
        y = (
            self._row_tops[row]
            if row < len(self._row_tops)
            else row * (self.MIN_CARD_HEIGHT + self.GRID_SPACING)
        )
        height = (
            self._row_heights[row]
            if row < len(self._row_heights)
            else self.MIN_CARD_HEIGHT
        )
        return QRect(x, y, width, height)

    def _card_index_at(self, pos) -> int:
        width = self._column_width()
        if width <= 0 or pos.y() < 0:
            return -1
        columns = self._column_count()
        column = pos.x() // (width + self.GRID_SPACING)
        if column < 0 or column >= columns:
            return -1
        row = -1
        for candidate, top in enumerate(self._row_tops):
            if pos.y() < top:
                break
            if pos.y() < top + self._row_heights[candidate]:
                row = candidate
                break
        index = row * columns + column
        if row < 0 or index < 0 or index >= len(self._cards):
            return -1
        return index if self._card_rect(index).contains(pos) else -1

    def _recalculate_row_layout(self) -> bool:
        rows = self._row_count()
        heights = [self.MIN_CARD_HEIGHT] * rows
        text_width = (
            self._column_width()
            - 2 * self.CARD_PADDING
            - self.ICON_SIZE
            - self.ICON_TEXT_GAP
        )
        if text_width > 0:
            title_metrics = QFontMetrics(
                fluentqt.font_for_role(fluentqt.FontRole.BodyStrong)
            )
            description_metrics = QFontMetrics(
                fluentqt.font_for_role(fluentqt.FontRole.Caption)
            )
            columns = self._column_count()
            for index, card in enumerate(self._cards):
                text_height = title_metrics.height()
                if card._entry_description:
                    bounds = description_metrics.boundingRect(
                        QRect(0, 0, text_width, 16777215),
                        Qt.AlignLeft | Qt.AlignTop | Qt.TextWordWrap,
                        card._entry_description,
                    )
                    text_height += self.TITLE_DESC_GAP + bounds.height()
                row = index // columns
                heights[row] = max(
                    heights[row],
                    2 * self.CARD_PADDING
                    + max(self.ICON_SIZE, text_height),
                )
        tops: list[int] = []
        next_top = 0
        for height in heights:
            tops.append(next_top)
            next_top += height + self.GRID_SPACING
        if heights == self._row_heights and tops == self._row_tops:
            return False
        self._row_heights = heights
        self._row_tops = tops
        return True

    def sizeHint(self) -> QSize:
        return QSize(480, self._grid_height())

    def minimumSizeHint(self) -> QSize:
        return QSize(0, self._grid_height())

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        columns = self._column_count()
        column_width = self._column_width()
        if columns != self._last_columns or column_width != self._last_column_width:
            self._last_columns = columns
            self._last_column_width = column_width
            self._recalculate_row_layout()
            self.updateGeometry()
        self.update()

    def refresh_theme(self) -> None:
        if self._recalculate_row_layout():
            self.updateGeometry()
        self.update()

    def leaveEvent(self, event) -> None:
        self._set_hovered_index(-1)
        super().leaveEvent(event)

    def _set_hovered_index(self, index: int) -> None:
        if self._hovered_index == index:
            return
        previous = self._hovered_index
        self._hovered_index = index
        if 0 <= previous < len(self._cards):
            self.update(self._card_rect(previous))
        if 0 <= index < len(self._cards):
            self.update(self._card_rect(index))

    def mouseMoveEvent(self, event) -> None:
        self._set_hovered_index(self._card_index_at(event.pos()))
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event) -> None:
        if event.button() == Qt.LeftButton:
            index = self._card_index_at(event.pos())
            if index >= 0:
                self._cards[index].activated.emit(self._cards[index]._route_id)
        super().mouseReleaseEvent(event)

    def paintEvent(self, event) -> None:
        if not self._cards:
            return
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)
        painter.setRenderHint(QPainter.SmoothPixmapTransform)
        colors = gallery_colors()
        title_font = fluentqt.font_for_role(fluentqt.FontRole.BodyStrong)
        description_font = fluentqt.font_for_role(fluentqt.FontRole.Caption)
        title_metrics = QFontMetrics(title_font)
        exposed = event.rect()
        for index, card in enumerate(self._cards):
            rect = self._card_rect(index)
            if rect.bottom() < exposed.top():
                continue
            if rect.top() > exposed.bottom():
                break

            painter.setPen(QPen(colors.stroke, 1.0))
            painter.setBrush(
                colors.subtle if index == self._hovered_index else colors.layer
            )
            painter.drawRoundedRect(
                QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5),
                8.0,
                8.0,
            )

            icon_rect = QRect(
                rect.left() + self.CARD_PADDING,
                rect.top() + self.CARD_PADDING,
                self.ICON_SIZE,
                self.ICON_SIZE,
            )
            if card._entry_pixmap.isNull():
                painter.setPen(Qt.NoPen)
                painter.setBrush(colors.subtle)
                painter.drawRoundedRect(icon_rect, 4.0, 4.0)
                _draw_native_font_icon(
                    painter,
                    icon_rect,
                    card._entry_icon_name,
                    20,
                    colors.text_primary,
                )
            else:
                _draw_pixmap_in_logical_rect(
                    painter,
                    icon_rect,
                    card._entry_pixmap,
                )

            text_left = icon_rect.right() + 1 + self.ICON_TEXT_GAP
            text_width = rect.right() - self.CARD_PADDING - text_left
            if text_width <= 0:
                continue
            title_y = rect.top() + self.CARD_PADDING
            painter.setFont(title_font)
            painter.setPen(colors.text_primary)
            painter.drawText(
                QRect(text_left, title_y, text_width, title_metrics.height()),
                Qt.AlignLeft | Qt.AlignVCenter,
                title_metrics.elidedText(
                    card._entry_title,
                    Qt.ElideRight,
                    text_width,
                ),
            )
            if card._entry_description:
                description_y = (
                    title_y + title_metrics.height() + self.TITLE_DESC_GAP
                )
                description_bottom = rect.bottom() - self.CARD_PADDING
                painter.setFont(description_font)
                painter.setPen(colors.text_secondary)
                painter.drawText(
                    QRect(
                        text_left,
                        description_y,
                        text_width,
                        max(0, description_bottom - description_y + 1),
                    ),
                    Qt.AlignLeft | Qt.AlignTop | Qt.TextWordWrap,
                    card._entry_description,
                )


_ACRYLIC_NOISE_TILE: QImage | None = None


def _qt_seeded_bytes(seed: int, count: int) -> list[int]:
    """Reproduce QRandomGenerator(seed).bounded(256) from the C++ build.

    PySide's one-value constructor seeds ``std::mt19937`` directly, while Qt's
    C++ constructor first expands the value through ``std::seed_seq``.  The
    Gallery texture must use the latter sequence or every dithered pixel drifts.
    """

    mask = 0xFFFFFFFF
    size = 624
    values = [0x8B8B8B8B] * size
    seeds = [seed & mask]
    t = 11
    p = (size - t) // 2
    q = p + t

    def xor27(value: int) -> int:
        value &= mask
        return value ^ (value >> 27)

    for index in range(size):
        first = (
            1664525
            * xor27(
                values[index % size]
                ^ values[(index + p) % size]
                ^ values[(index - 1) % size]
            )
        ) & mask
        if index == 0:
            offset = len(seeds)
        elif index <= len(seeds):
            offset = index % size + seeds[index - 1]
        else:
            offset = index % size
        second = (first + offset) & mask
        values[(index + p) % size] = (
            values[(index + p) % size] + first
        ) & mask
        values[(index + q) % size] = (
            values[(index + q) % size] + second
        ) & mask
        values[index % size] = second

    for index in range(size, size * 2):
        summed = (
            values[index % size]
            + values[(index + p) % size]
            + values[(index - 1) % size]
        ) & mask
        third = (1566083941 * xor27(summed)) & mask
        fourth = (third - index % size) & mask
        values[(index + p) % size] = (
            values[(index + p) % size] ^ third
        ) & mask
        values[(index + q) % size] = (
            values[(index + q) % size] ^ fourth
        ) & mask
        values[index % size] = fourth

    result: list[int] = []
    state_index = size
    while len(result) < count:
        if state_index == size:
            for index in range(size):
                joined = (values[index] & 0x80000000) | (
                    values[(index + 1) % size] & 0x7FFFFFFF
                )
                values[index] = (
                    values[(index + 397) % size]
                    ^ (joined >> 1)
                    ^ (0x9908B0DF if joined & 1 else 0)
                ) & mask
            state_index = 0
        value = values[state_index]
        state_index += 1
        value ^= value >> 11
        value ^= (value << 7) & 0x9D2C5680
        value ^= (value << 15) & 0xEFC60000
        value ^= value >> 18
        result.append((value & mask) >> 24)
    return result


def _acrylic_noise_tile() -> QImage:
    global _ACRYLIC_NOISE_TILE
    if _ACRYLIC_NOISE_TILE is not None:
        return _ACRYLIC_NOISE_TILE
    image = QImage(96, 96, QImage.Format_ARGB32)
    values = iter(_qt_seeded_bytes(0xACE71C5E, 96 * 96))
    pixels = image.bits().cast("I")
    stride = image.bytesPerLine() // 4
    # Write native QRgb words directly. Besides matching the C++ path, this
    # avoids thousands of Python-to-C++ setter calls on Linux ARM64.
    for y in range(96):
        for x in range(96):
            value = next(values)
            pixels[y * stride + x] = 0xFF000000 | value * 0x010101
    _ACRYLIC_NOISE_TILE = image
    return image


_HERO_LINK_PIXMAP_CACHE: dict[tuple[str, int, int, int], QPixmap] = {}


def _tint_github_mark(image: QImage, tint: QColor) -> None:
    tint_rgb = (tint.red() << 16) | (tint.green() << 8) | tint.blue()
    pixels = image.bits().cast("I")
    stride = image.bytesPerLine() // 4
    width = image.width()
    height = image.height()
    # Keep the entire hot loop on the native pixel buffer. Calling a wrapped
    # void setter once per pixel can exhaust Py_None references with the
    # PySide 6.9 Linux ARM64 wheel before a 560 px icon is processed.
    for y in range(height):
        row = y * stride
        for x in range(width):
            source = int(pixels[row + x])
            luminance = (
                ((source >> 16) & 0xFF) * 11
                + ((source >> 8) & 0xFF) * 16
                + (source & 0xFF) * 5
            ) // 32
            alpha = (255 - luminance) * ((source >> 24) & 0xFF) // 255
            pixels[row + x] = (alpha << 24) | tint_rgb


def _hero_link_pixmap(
    image_name: str,
    size: int,
    tint: QColor,
    device_pixel_ratio: float = 1.0,
) -> QPixmap:
    logical_size = max(1, int(size))
    dpr = _normalized_device_pixel_ratio(device_pixel_ratio)
    physical_size = max(1, _qround(logical_size * dpr))
    key = (
        image_name,
        logical_size,
        int(tint.rgba()),
        _qround(dpr * 1000.0),
    )
    cached = _HERO_LINK_PIXMAP_CACHE.get(key)
    if cached is not None:
        return cached
    image = QImage(str(asset_path("home_header_tiles", image_name)))
    if image.isNull():
        return QPixmap()
    image = image.convertToFormat(QImage.Format_ARGB32)
    if image_name == "GitHub-Mark.png":
        _tint_github_mark(image, tint)
    alpha_image = image.convertToFormat(QImage.Format_RGBA8888)
    raw = bytes(alpha_image.constBits())
    stride = alpha_image.bytesPerLine()
    left, top = image.width(), image.height()
    right = bottom = -1
    for y in range(image.height()):
        alpha_row = raw[y * stride + 3 : y * stride + image.width() * 4 : 4]
        for x, alpha in enumerate(alpha_row):
            if alpha <= 8:
                continue
            left = min(left, x)
            top = min(top, y)
            right = max(right, x)
            bottom = max(bottom, y)
    if right >= left and bottom >= top:
        visible = QRect(left, top, right - left + 1, bottom - top + 1)
        if visible.size() != image.size():
            image = image.copy(visible)
    image = image.scaled(
        physical_size,
        physical_size,
        Qt.KeepAspectRatio,
        Qt.SmoothTransformation,
    )
    pixmap = QPixmap.fromImage(image)
    pixmap.setDevicePixelRatio(dpr)
    _HERO_LINK_PIXMAP_CACHE[key] = pixmap
    return pixmap


class GallerySplashScreen(QWidget):
    """Startup overlay matching the native Gallery splash specification."""

    _LOGO_SIZE = 96
    _SPINNER_SIZE = 32
    _SPINNER_CENTER_OFFSET = 144
    _CAPTION_GAP = 12
    _CAPTION_HEIGHT = 20

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("gallerySplashScreen")
        self._logo = QPixmap()
        self._progress_text = ""
        self._dismissing = False
        self._fade: QPropertyAnimation | None = None
        self._spinner = fluentqt.ProgressRing(self)
        self._spinner.setObjectName("gallerySplashSpinner")
        self._spinner.setFixedSize(self._SPINNER_SIZE, self._SPINNER_SIZE)
        self._spinner.setIsIndeterminate(True)
        self._spinner.setIsActive(True)
        if parent is not None:
            parent.installEventFilter(self)
            self.setGeometry(parent.rect())
        self.raise_()
        self._layout_content()
        self.refresh_display_scale()

    def set_progress(self, done: int, total: int) -> None:
        next_text = (
            "{0}%".format(max(0, min(done * 100 // total, 100)))
            if total > 0
            else ""
        )
        if self._progress_text == next_text:
            return
        self._progress_text = next_text
        self.update()

    def dismiss(self) -> None:
        if self._dismissing:
            return
        self._dismissing = True
        self._spinner.setIsActive(False)
        effect = QGraphicsOpacityEffect(self)
        self.setGraphicsEffect(effect)
        fade = QPropertyAnimation(effect, b"opacity", self)
        fade.setStartValue(1.0)
        fade.setEndValue(0.0)
        fade.setDuration(250)
        fade.setEasingCurve(QEasingCurve.OutCubic)
        fade.finished.connect(self.deleteLater)
        self._fade = fade
        fade.start()

    def refresh_theme(self) -> None:
        self.update()

    def refresh_display_scale(self) -> None:
        self._logo = app_icon_pixmap(
            self._LOGO_SIZE,
            self.devicePixelRatioF(),
        )
        self.update()

    def showEvent(self, event) -> None:
        super().showEvent(event)
        self.refresh_display_scale()

    def eventFilter(self, watched, event) -> bool:
        if (
            watched is self.parentWidget()
            and event.type() == QEvent.Type.Resize
        ):
            self.setGeometry(self.parentWidget().rect())
            self.raise_()
        return super().eventFilter(watched, event)

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self._layout_content()

    def _layout_content(self) -> None:
        center_x = self.width() // 2
        center_y = self.height() // 2
        self._spinner.move(
            center_x - self._SPINNER_SIZE // 2,
            center_y
            + self._SPINNER_CENTER_OFFSET
            - self._SPINNER_SIZE // 2,
        )

    def paintEvent(self, event) -> None:
        del event
        from .foundation_pages import _theme_tokens

        tokens = _theme_tokens()
        painter = QPainter(self)
        painter.setRenderHint(QPainter.SmoothPixmapTransform)
        painter.fillRect(self.rect(), tokens["bgCanvas"])
        if not self._logo.isNull():
            logo_rect = QRect(
                self.width() // 2 - self._LOGO_SIZE // 2,
                self.height() // 2 - self._LOGO_SIZE // 2,
                self._LOGO_SIZE,
                self._LOGO_SIZE,
            )
            painter.drawPixmap(logo_rect, self._logo)
        if self._progress_text:
            caption_top = (
                self.height() // 2
                + self._SPINNER_CENTER_OFFSET
                + self._SPINNER_SIZE // 2
                + self._CAPTION_GAP
            )
            painter.setFont(
                fluentqt.font_for_role(fluentqt.FontRole.Caption)
            )
            painter.setPen(tokens["textSecondary"])
            painter.drawText(
                QRect(0, caption_top, self.width(), self._CAPTION_HEIGHT),
                Qt.AlignHCenter | Qt.AlignTop,
                self._progress_text,
            )


class GalleryPageSkeleton(QWidget):
    """Single full-page shimmer matching GalleryPageSkeleton in C++."""

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("galleryPageSkeleton")
        self.setAttribute(Qt.WA_TranslucentBackground)
        self._progress = 0.0
        self._animation = QVariantAnimation(self)
        self._animation.setDuration(1400)
        self._animation.setStartValue(0.0)
        self._animation.setEndValue(1.0)
        self._animation.setLoopCount(-1)
        self._animation.setEasingCurve(QEasingCurve.Linear)
        self._animation.valueChanged.connect(self._set_progress)

    def _set_progress(self, value: object) -> None:
        self._progress = float(value)
        self.update()

    @staticmethod
    def _blend(
        foreground: QColor, background: QColor, amount: float
    ) -> QColor:
        inverse = 1.0 - amount
        return QColor(
            _qround(foreground.red() * amount + background.red() * inverse),
            _qround(
                foreground.green() * amount
                + background.green() * inverse
            ),
            _qround(
                foreground.blue() * amount + background.blue() * inverse
            ),
            _qround(
                foreground.alpha() * amount
                + background.alpha() * inverse
            ),
        )

    def showEvent(self, event) -> None:
        super().showEvent(event)
        self._animation.start()

    def hideEvent(self, event) -> None:
        super().hideEvent(event)
        self._animation.stop()

    def paintEvent(self, event) -> None:
        del event
        content = QRectF(self.rect()).adjusted(24, 34, -24, -48)
        if content.isEmpty():
            return
        y = content.top()
        elements = [
            QRectF(content.left(), y, min(320.0, content.width()), 40.0)
        ]
        y += 56.0
        elements.append(
            QRectF(content.left(), y, min(460.0, content.width()), 24.0)
        )
        y += 52.0
        for _unused in range(3):
            elements.append(
                QRectF(content.left(), y, content.width(), 132.0)
            )
            y += 140.0

        combined = QPainterPath()
        bounds = QRectF()
        for rect in elements:
            combined.addRoundedRect(rect, 4.0, 4.0)
            bounds = QRectF(rect) if bounds.isNull() else bounds.united(rect)

        from .foundation_pages import _theme_tokens

        colors = _theme_tokens()
        canvas = QColor(colors["bgCanvas"])
        dark = canvas.lightness() < 96
        base = self._blend(
            QColor(Qt.white) if dark else QColor(Qt.black),
            canvas,
            0.12 if dark else 0.075,
        )
        highlight = QColor(255, 255, 255, 68 if dark else 218)
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setPen(Qt.NoPen)
        painter.setBrush(base)
        painter.drawPath(combined)

        sweep_width = max(56.0, bounds.width() * 0.42)
        sweep_x = (
            bounds.left()
            - sweep_width
            + (bounds.width() + sweep_width * 2.0) * self._progress
        )
        gradient = QLinearGradient(
            QPointF(sweep_x - sweep_width, bounds.center().y()),
            QPointF(sweep_x + sweep_width, bounds.center().y()),
        )
        transparent = QColor(highlight)
        transparent.setAlpha(0)
        gradient.setColorAt(0.0, transparent)
        gradient.setColorAt(0.48, highlight)
        gradient.setColorAt(1.0, transparent)
        painter.setClipPath(combined)
        painter.fillRect(
            bounds.adjusted(-sweep_width, 0.0, sweep_width, 0.0),
            gradient,
        )
        painter.setClipping(False)
        painter.setBrush(Qt.NoBrush)
        painter.setPen(QPen(colors["strokeDefault"], 1.0))
        painter.drawPath(combined)


def _draw_elided_wrapped_text(
    painter: QPainter,
    rect: QRect,
    text: str,
    font: QFont,
    color: QColor,
    max_lines: int,
) -> None:
    if rect.isEmpty() or not text or max_lines <= 0:
        return
    metrics = QFontMetrics(font)
    line_height = max(1, metrics.lineSpacing() - 1)
    available_lines = min(max_lines, max(1, rect.height() // line_height))
    words = [word for word in text.split(" ") if word]
    lines: list[str] = []
    current = ""
    for index, word in enumerate(words):
        candidate = word if not current else current + " " + word
        if metrics.horizontalAdvance(candidate) <= rect.width() or not current:
            current = candidate
            continue
        lines.append(current)
        current = word
        if len(lines) == available_lines - 1:
            tail = " ".join(words[index:])
            lines.append(metrics.elidedText(tail, Qt.ElideRight, rect.width()))
            current = ""
            break
    if current and len(lines) < available_lines:
        lines.append(metrics.elidedText(current, Qt.ElideRight, rect.width()))

    painter.save()
    painter.setFont(font)
    painter.setPen(color)
    for index, line in enumerate(lines):
        painter.drawText(
            QRect(
                rect.left(),
                rect.top() + index * line_height,
                rect.width(),
                line_height,
            ),
            Qt.AlignLeft | Qt.AlignVCenter,
            line,
        )
    painter.restore()


class GalleryHeroLinkCard(QWidget):
    """One translucent external-link card over the home hero artwork."""

    def __init__(
        self,
        title: str,
        description: str,
        url: str,
        image_name: str,
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self._url = QUrl(url)
        self._hovered = False
        self._pressed = False
        self._image_name = image_name
        self._title_text = title
        self._description_text = description
        self.setObjectName("galleryHomeHeroLinkCard")
        self.setFixedSize(214, 182)
        self.setCursor(Qt.PointingHandCursor)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self._image_pixmap = QPixmap()
        self.refresh_theme()

    def enterEvent(self, event) -> None:
        self._hovered = True
        self.refresh_theme()
        super().enterEvent(event)

    def leaveEvent(self, event) -> None:
        self._hovered = False
        self._pressed = False
        self.refresh_theme()
        super().leaveEvent(event)

    def mousePressEvent(self, event) -> None:
        if event.button() == Qt.LeftButton:
            self._pressed = True
            self.update()
        super().mousePressEvent(event)

    def mouseReleaseEvent(self, event) -> None:
        self._pressed = False
        self.update()
        if event.button() == Qt.LeftButton and self.rect().contains(event.pos()):
            QDesktopServices.openUrl(self._url)
        super().mouseReleaseEvent(event)

    def paintEvent(self, event) -> None:
        del event
        colors = gallery_colors()
        dark = fluentqt.current_theme() == fluentqt.Theme.Dark
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)
        painter.setRenderHint(QPainter.SmoothPixmapTransform)
        active = self._hovered or self._pressed
        lift = 2.0 if active else 0.0
        card = QRectF(8, 8, 198, 150).adjusted(0.5, 0.5, -0.5, -0.5)
        card.translate(0, -lift)
        path = QPainterPath()
        path.addRoundedRect(card, 8.0, 8.0)

        painter.save()
        shadow_clip = QPainterPath()
        shadow_clip.addRect(QRectF(self.rect()).adjusted(-40, -40, 40, 60))
        shadow_clip = shadow_clip.subtracted(path)
        painter.setClipPath(shadow_clip)
        shadow_layers = 14
        reach = 13.0 if active else 9.0
        y_bias = 5.0 if active else 3.0
        peak_alpha = (13 if active else 9) if dark else (6 if active else 4)
        for index in range(shadow_layers):
            fraction = (index + 1.0) / shadow_layers
            grow = reach * fraction
            shadow = QColor(
                0,
                0,
                0,
                round(peak_alpha * (1.0 - fraction)),
            )
            shadow_path = QPainterPath()
            shadow_path.addRoundedRect(
                card.adjusted(
                    -grow,
                    -grow * 0.5 + y_bias,
                    grow,
                    grow + y_bias,
                ),
                8.0 + grow,
                8.0 + grow,
            )
            painter.fillPath(shadow_path, shadow)
        painter.restore()

        surface = QColor(colors.layer_alt if dark else colors.layer)
        surface.setAlpha(112 if dark else 132)
        painter.fillPath(path, surface)
        painter.save()
        painter.setClipPath(path)
        painter.setOpacity(0.05 if dark else 0.035)
        painter.setBrushOrigin(QPointF(-float(self.x()), 0.0))
        painter.fillRect(card, QBrush(_acrylic_noise_tile()))
        painter.restore()
        border = QColor(colors.accent if active else colors.stroke)
        if active:
            border.setAlpha(150 if dark else 120)
        painter.setPen(QPen(border, 1.0))
        painter.setBrush(Qt.NoBrush)
        painter.drawPath(path)

        content_left = _qround(card.left()) + 18
        content_width = _qround(card.width()) - 36
        icon_rect = QRectF(content_left, card.top() + 18, 32, 32)
        if not self._image_pixmap.isNull():
            logical = QSizeF(self._image_pixmap.size()) / max(
                1.0,
                self._image_pixmap.devicePixelRatioF(),
            )
            top_left = QPointF(
                icon_rect.center().x() - logical.width() / 2.0,
                icon_rect.center().y() - logical.height() / 2.0,
            )
            painter.drawPixmap(top_left, self._image_pixmap)

        text_y = _qround(icon_rect.bottom()) + 14
        title_font = fluentqt.font_for_role(fluentqt.FontRole.BodyStrong)
        description_font = fluentqt.font_for_role(fluentqt.FontRole.Caption)
        title_metrics = QFontMetrics(title_font)
        painter.setFont(title_font)
        painter.setPen(colors.text_primary)
        painter.drawText(
            QRect(content_left, text_y, content_width, title_metrics.height()),
            Qt.AlignLeft | Qt.AlignVCenter,
            title_metrics.elidedText(
                self._title_text,
                Qt.ElideRight,
                content_width,
            ),
        )
        text_y += title_metrics.height() + 4
        _draw_elided_wrapped_text(
            painter,
            QRect(
                content_left,
                text_y,
                content_width,
                _qround(card.bottom()) - 18 - text_y,
            ),
            self._description_text,
            description_font,
            colors.text_secondary,
            3,
        )

        external_rect = QRect(
            _qround(card.right()) - 18 - 16,
            _qround(card.top()) + 18,
            16,
            16,
        )
        _draw_native_font_icon(
            painter,
            external_rect,
            "ic_fluent_open_16_regular",
            16,
            colors.accent if active else colors.text_secondary,
        )

    def refresh_theme(self) -> None:
        colors = gallery_colors()
        self._image_pixmap = _hero_link_pixmap(
            self._image_name,
            32,
            colors.text_primary,
            self.devicePixelRatioF(),
        )
        self.update()

    def refresh_display_scale(self) -> None:
        self.refresh_theme()

    def showEvent(self, event) -> None:
        super().showEvent(event)
        self.refresh_display_scale()


class GalleryHomeHero(QWidget):
    """Full-width native-style gradient hero and external link strip."""

    HEIGHT = 390

    _LINKS = (
        (
            "Design",
            "Guidelines and toolkits for Fluent design.",
            "https://aka.ms/WinUI/3.0-figma-toolkit",
            "Header-WindowsDesign.png",
        ),
        (
            "macOS 27 Community",
            "Community Figma design kit for macOS 27.",
            "https://www.figma.com/community/file/1651309434229735362",
            "Header-macOS27.png",
        ),
        (
            "Material 3 Design Kit",
            "Community Figma kit for Material 3.",
            "https://www.figma.com/community/file/1035203688168086460",
            "Header-Material3.png",
        ),
        (
            "WinUI Gallery",
            "WinUI Gallery source on GitHub.",
            "https://github.com/microsoft/WinUI-Gallery",
            "GitHub-Mark.png",
        ),
        (
            "Fluent UI",
            "Fluent controls and patterns for the web.",
            "https://developer.microsoft.com/en-us/fluentui#/controls/web",
            "Header-Toolkit.png",
        ),
        (
            "FluentQt",
            "FluentQt UI component library source on GitHub.",
            "https://github.com/calvinhxx/Fluent-Qt",
            "Header-WinUI.png",
        ),
        (
            "Qt Quick Controls",
            "Qt Quick Controls reference on doc.qt.io.",
            "https://doc.qt.io/qt-6/qtquickcontrols-index.html",
            "Qt-Logo.png",
        ),
    )

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("galleryHomeHero")
        self.setFixedHeight(self.HEIGHT)
        self.setAttribute(Qt.WA_TranslucentBackground)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(24, 24, 24, 202)
        layout.setSpacing(0)
        icon = QLabel(self)
        icon.setObjectName("galleryHomeHeroIcon")
        icon.setFixedSize(56, 56)
        layout.addWidget(icon)
        layout.addSpacing(12)
        title = fluentqt.Label("Fluent-Qt Gallery", self)
        title.setObjectName("galleryHomeHeroTitle")
        title.setFluentTypography(fluentqt.FontRole.TitleLarge)
        layout.addWidget(title)
        layout.addSpacing(4)
        tagline = fluentqt.Label(
            "Interactive documentation for FluentQt: browse components, run live examples, and inspect focused API usage.",
            self,
        )
        tagline.setObjectName("galleryHomeHeroTagline")
        tagline.setFluentTypography(fluentqt.FontRole.Body)
        tagline.setWordWrap(True)
        layout.addWidget(tagline)
        layout.addStretch()

        strip = QScrollArea(self)
        strip.setObjectName("galleryHomeHeroLinksView")
        strip.setFrameShape(QFrame.NoFrame)
        strip.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        strip.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        strip.setWidgetResizable(False)
        strip.setStyleSheet(
            "QScrollArea#galleryHomeHeroLinksView { background: transparent; border: none; }"
            "QScrollArea#galleryHomeHeroLinksView > QWidget > QWidget { background: transparent; }"
        )
        container = QWidget()
        container.setObjectName("galleryHomeHeroLinkContainer")
        cards_layout = QHBoxLayout(container)
        cards_layout.setContentsMargins(0, 0, 0, 0)
        cards_layout.setSpacing(0)
        cards = []
        for link in self._LINKS:
            card = GalleryHeroLinkCard(*link, parent=container)
            cards_layout.addWidget(card)
            cards.append(card)
        container.setFixedSize(
            len(cards) * 214,
            182,
        )
        strip.setWidget(container)
        back = fluentqt.Button("", strip)
        back.setObjectName("galleryHomeHeroScrollButton")
        back.setFluentStyle(fluentqt.Button.ButtonStyle.Standard)
        back.setFluentSize(fluentqt.Button.ButtonSize.Small)
        back.setFluentLayout(fluentqt.Button.ButtonLayout.IconOnly)
        back.setIconGlyph("\uEDD9", 16)
        back.setContentOpacity(0.62)
        back.setFixedSize(16, 38)
        back.setFocusPolicy(Qt.NoFocus)
        back.setCursor(Qt.PointingHandCursor)
        fluentqt.ToolTip.attach(back, "Scroll left")

        forward = fluentqt.Button("", strip)
        forward.setObjectName("galleryHomeHeroScrollButton")
        forward.setFluentStyle(fluentqt.Button.ButtonStyle.Standard)
        forward.setFluentSize(fluentqt.Button.ButtonSize.Small)
        forward.setFluentLayout(fluentqt.Button.ButtonLayout.IconOnly)
        forward.setIconGlyph("\uEDDA", 16)
        forward.setContentOpacity(0.62)
        forward.setFixedSize(16, 38)
        forward.setFocusPolicy(Qt.NoFocus)
        forward.setCursor(Qt.PointingHandCursor)
        fluentqt.ToolTip.attach(forward, "Scroll right")

        scroll_bar = strip.horizontalScrollBar()
        back.clicked.connect(lambda: self._scroll_links(-1))
        forward.clicked.connect(lambda: self._scroll_links(1))
        scroll_bar.rangeChanged.connect(
            lambda _minimum, _maximum: self._update_scroll_buttons()
        )
        scroll_bar.valueChanged.connect(
            lambda _value: self._update_scroll_buttons()
        )
        back.hide()
        forward.hide()
        back.raise_()
        forward.raise_()
        strip.raise_()
        self._strip = strip
        self._back_button = back
        self._forward_button = forward
        self._cards = tuple(cards)
        self._icon = icon
        self._title = title
        self._tagline = tagline
        self.refresh_display_scale()
        self.refresh_theme()

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self._strip.setGeometry(16, 200, max(0, self.width() - 40), 182)
        self._update_scroll_buttons()

    def showEvent(self, event) -> None:
        super().showEvent(event)
        self.refresh_display_scale()
        self._update_scroll_buttons()

    def refresh_display_scale(self) -> None:
        self._icon.setPixmap(
            app_icon_pixmap(56, self.devicePixelRatioF())
        )

    def _scroll_links(self, direction: int) -> None:
        scroll_bar = self._strip.horizontalScrollBar()
        delta = max(1, self._strip.viewport().width() - 198)
        scroll_bar.setValue(
            max(
                scroll_bar.minimum(),
                min(
                    scroll_bar.maximum(),
                    scroll_bar.value() + direction * delta,
                ),
            )
        )
        self._update_scroll_buttons()

    def _update_scroll_buttons(self) -> None:
        if not hasattr(self, "_back_button"):
            return
        y = max(0, (self._strip.height() - 38) // 2 - 8)
        self._back_button.setGeometry(8, y, 16, 38)
        self._forward_button.setGeometry(
            self._strip.width() - 8 - 16,
            y,
            16,
            38,
        )
        scroll_bar = self._strip.horizontalScrollBar()
        can_scroll = scroll_bar.maximum() > scroll_bar.minimum()
        self._back_button.setVisible(
            can_scroll and scroll_bar.value() > scroll_bar.minimum()
        )
        self._forward_button.setVisible(
            can_scroll and scroll_bar.value() < scroll_bar.maximum()
        )
        self._back_button.raise_()
        self._forward_button.raise_()

    def paintEvent(self, event) -> None:
        del event
        dpr = max(1.0, self.devicePixelRatioF())
        artwork = QImage(
            max(1, round(self.width() * dpr)),
            max(1, round(self.height() * dpr)),
            QImage.Format_ARGB32_Premultiplied,
        )
        artwork.setDevicePixelRatio(dpr)
        artwork.fill(Qt.transparent)
        painter = QPainter(artwork)
        painter.setRenderHint(QPainter.Antialiasing)
        dark = fluentqt.current_theme() == fluentqt.Theme.Dark
        banner = QRectF(0, 0, self.width(), self.height())
        clip = QPainterPath()
        radius = 8.0
        clip.moveTo(banner.left() + radius, banner.top())
        clip.lineTo(banner.right(), banner.top())
        clip.lineTo(banner.right(), banner.bottom())
        clip.lineTo(banner.left(), banner.bottom())
        clip.lineTo(banner.left(), banner.top() + radius)
        clip.arcTo(
            QRectF(
                banner.left(),
                banner.top(),
                radius * 2.0,
                radius * 2.0,
            ),
            180,
            -90,
        )
        clip.closeSubpath()
        painter.setClipPath(clip)
        gradient = QLinearGradient(banner.topLeft(), banner.bottomRight())
        if dark:
            for stop, value in (
                (0.00, "#16223A"),
                (0.22, "#1A2440"),
                (0.40, "#1F2444"),
                (0.56, "#242447"),
                (0.70, "#29234A"),
                (0.84, "#2F2348"),
                (1.00, "#352240"),
            ):
                gradient.setColorAt(stop, QColor(value))
        else:
            for stop, value in (
                (0.00, "#D9E9F8"),
                (0.24, "#DEE5F6"),
                (0.46, "#E3E1F4"),
                (0.66, "#E9E2F0"),
                (0.84, "#EFE5EE"),
                (1.00, "#F4E8EA"),
            ):
                gradient.setColorAt(stop, QColor(value))
        painter.fillRect(banner, gradient)

        def bloom(center: QPointF, radius: float, tint: QColor) -> None:
            radial = QRadialGradient(center, radius)
            radial.setColorAt(0.0, tint)
            mid = QColor(tint)
            mid.setAlpha(tint.alpha() // 3)
            radial.setColorAt(0.5, mid)
            edge = QColor(tint)
            edge.setAlpha(0)
            radial.setColorAt(1.0, edge)
            painter.fillRect(banner, radial)

        width = banner.width()
        height = banner.height()
        bloom(
            QPointF(width * 0.86, -height * 0.30),
            height * 1.5,
            QColor(86, 150, 232, 82) if dark else QColor(255, 255, 255, 210),
        )
        bloom(
            QPointF(width * 0.94, height * 1.10),
            height * 1.3,
            QColor(150, 96, 206, 76) if dark else QColor(214, 180, 236, 128),
        )
        bloom(
            QPointF(width * 0.62, height * 1.15),
            height,
            QColor(196, 116, 168, 46) if dark else QColor(247, 210, 214, 102),
        )
        painter.save()
        painter.setOpacity(0.05 if dark else 0.045)
        painter.setCompositionMode(QPainter.CompositionMode_Overlay)
        painter.fillRect(banner, QBrush(_acrylic_noise_tile()))
        painter.restore()

        backdrop_getter = getattr(self.window(), "backdropEffect", None)
        backdrop = backdrop_getter() if callable(backdrop_getter) else None
        material_backdrop = backdrop in (
            fluentqt.BackdropEffect.Mica,
            fluentqt.BackdropEffect.Acrylic,
        )
        if material_backdrop:
            fade_top = 1.0 - 184.0 / max(1.0, banner.height())
            veil = 0.90 if dark else 0.92
            mask = QLinearGradient(banner.topLeft(), banner.bottomLeft())

            def alpha(opacity: float) -> QColor:
                return QColor(
                    0,
                    0,
                    0,
                    max(0, min(255, round(opacity * 255))),
                )

            mask.setColorAt(0.0, alpha(veil))
            mask.setColorAt(fade_top, alpha(veil))
            mask.setColorAt(
                fade_top + (1.0 - fade_top) * 0.30,
                alpha(veil * 0.74),
            )
            mask.setColorAt(
                fade_top + (1.0 - fade_top) * 0.55,
                alpha(veil * 0.46),
            )
            mask.setColorAt(
                fade_top + (1.0 - fade_top) * 0.78,
                alpha(veil * 0.18),
            )
            mask.setColorAt(1.0, alpha(0.0))
            painter.setCompositionMode(QPainter.CompositionMode_DestinationIn)
            painter.fillRect(banner, mask)
            painter.setCompositionMode(QPainter.CompositionMode_SourceOver)
        else:
            colors = gallery_colors()
            fade_rect = QRectF(
                banner.left(),
                banner.bottom() - 184.0,
                banner.width(),
                184.0,
            )
            clear = QColor(colors.layer_alt)
            clear.setAlpha(0)
            soft = QColor(colors.layer_alt)
            soft.setAlpha(34 if dark else 42)
            lower = QColor(colors.layer_alt)
            lower.setAlpha(74 if dark else 92)
            medium = QColor(colors.layer_alt)
            medium.setAlpha(122 if dark else 148)
            fade = QLinearGradient(fade_rect.topLeft(), fade_rect.bottomLeft())
            fade.setColorAt(0.00, clear)
            fade.setColorAt(0.30, soft)
            fade.setColorAt(0.55, lower)
            fade.setColorAt(0.78, medium)
            fade.setColorAt(1.00, colors.layer_alt)
            painter.fillRect(fade_rect, fade)
        painter.end()

        target = QPainter(self)
        target.drawImage(QPointF(0, 0), artwork)
        target.end()

    def refresh_theme(self) -> None:
        colors = gallery_colors()
        self._title.setStyleSheet(
            "color: {0}; background: transparent;".format(
                css_color(colors.text_primary)
            )
        )
        self._tagline.setStyleSheet(
            "color: {0}; background: transparent;".format(
                css_color(colors.text_secondary)
            )
        )
        for card in self._cards:
            card.refresh_theme()
        self.update()


_REFERENCE_MODULES = {
    "basic-input": "basicinput",
    "collections": "collections",
    "date-time": "date_time",
    "dialogs-flyouts": "dialogs_flyouts",
    "foundation": "foundation",
    "layout": "layout",
    "menus-toolbars": "menus_toolbars",
    "navigation": "navigation",
    "scrolling": "scrolling",
    "status-info": "status_info",
    "text-fields": "textfields",
    "windowing": "windowing",
}


class GalleryReferenceCard(QFrame):
    """Python import reference with the native Gallery card geometry."""

    def __init__(
        self,
        api_type: str,
        category_id: str,
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self.setObjectName("galleryComponentReferenceCard")
        self.setFrameShape(QFrame.NoFrame)
        self.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        layout = QGridLayout(self)
        layout.setContentsMargins(18, 16, 18, 16)
        layout.setHorizontalSpacing(20)
        layout.setVerticalSpacing(8)
        layout.setColumnStretch(1, 1)
        self._keys = []
        self._values = []
        module = _REFERENCE_MODULES[category_id]
        for row, (name, value, object_name) in enumerate(
            (
                (
                    "Import",
                    "import fluentqt",
                    "galleryComponentReferenceImport",
                ),
                (
                    "Type",
                    "fluentqt.{0}".format(api_type),
                    "galleryComponentReferenceType",
                ),
                (
                    "Module",
                    "fluentqt.{0}".format(module),
                    "galleryComponentReferenceModule",
                ),
            )
        ):
            key = fluentqt.Label(name, self)
            key.setObjectName("galleryComponentReferenceKey")
            key.setFluentTypography(fluentqt.FontRole.Body)
            key.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Preferred)
            value_label = fluentqt.Label(value, self)
            value_label.setObjectName(object_name)
            value_label.setFluentTypography(fluentqt.FontRole.BodyStrong)
            value_label.setWordWrap(True)
            value_label.setTextInteractionFlags(
                Qt.TextSelectableByMouse | Qt.TextSelectableByKeyboard
            )
            value_label.setSizePolicy(
                QSizePolicy.Expanding, QSizePolicy.Preferred
            )
            fixed = QFontDatabase.systemFont(QFontDatabase.FixedFont)
            fixed.setPixelSize(
                fluentqt.font_for_role(fluentqt.FontRole.Body).pixelSize()
            )
            value_label.setFont(fixed)
            layout.addWidget(key, row, 0, Qt.AlignTop)
            layout.addWidget(value_label, row, 1)
            self._keys.append(key)
            self._values.append(value_label)
        self.refresh_theme()

    def refresh_theme(self) -> None:
        colors = gallery_colors()
        self.setStyleSheet(
            "#galleryComponentReferenceCard {{ background: {0}; "
            "border: 1px solid {1}; border-radius: 8px; }}".format(
                css_color(colors.layer), css_color(colors.stroke)
            )
        )
        for label in self._keys:
            label.setStyleSheet(
                "color: {0}; background: transparent;".format(
                    css_color(colors.text_secondary)
                )
            )
        self.update()
        for label in self._values:
            label.setStyleSheet(
                "color: {0}; background: transparent;".format(
                    css_color(colors.text_primary)
                )
            )


class GalleryCodeBlock(fluentqt.Expander):
    """Source-code expander matching the native GalleryCodeBlock structure."""

    def __init__(self, code: str, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._code = code
        # Source extraction keeps a terminal line break for stable formatting,
        # but a rich-text QLabel renders it as an additional empty code line.
        # Keep the original text for copying and omit serialization-only line
        # endings from the visual block, matching the native C++ Gallery.
        self._display_code = code.rstrip("\r\n")
        self._highlighted = False
        self.setObjectName("galleryCodeBlock")
        self.setAppearance(fluentqt.Card.Appearance.LayerAlt)
        self.setHeaderText("Source code")
        header = self.findChild(QWidget, "fluentExpanderHeader")
        if header is not None:
            header.setObjectName("galleryCodeBlockHeader")
        caption = self.findChild(
            fluentqt.Label, "fluentExpanderHeaderText"
        )
        if caption is not None:
            caption.setObjectName("galleryCodeBlockCaption")
        clip = self.findChild(QWidget, "fluentExpanderClip")
        if clip is not None:
            clip.setObjectName("galleryCodeBlockContent")

        content = QWidget()
        content.setObjectName("galleryCodeBlockContentInner")
        content.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        content.setMinimumWidth(0)
        layout = QVBoxLayout(content)
        layout.setContentsMargins(16, 12, 14, 16)
        layout.setSpacing(10)
        top = QHBoxLayout()
        top.setContentsMargins(0, 0, 0, 0)
        top.setSpacing(8)
        language_column = QVBoxLayout()
        language_column.setContentsMargins(0, 0, 0, 0)
        language_column.setSpacing(4)
        language = fluentqt.Label("Python", content)
        language.setObjectName("galleryCodeBlockLang")
        language.setFluentTypography(fluentqt.FontRole.Caption)
        language.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        underline = QWidget(content)
        underline.setObjectName("galleryCodeBlockLangUnderline")
        underline.setFixedSize(38, 3)
        language_column.addWidget(language, 0, Qt.AlignLeft)
        language_column.addWidget(underline, 0, Qt.AlignLeft)
        copy = fluentqt.Button("", content)
        copy.setObjectName("galleryCodeBlockCopyButton")
        copy.setFluentStyle(fluentqt.Button.ButtonStyle.Subtle)
        copy.setFluentSize(fluentqt.Button.ButtonSize.Small)
        copy.setFluentLayout(fluentqt.Button.ButtonLayout.IconOnly)
        copy.setIconGlyph("\ue8c8", 16)
        copy.setFixedSize(28, 28)
        copy.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        fluentqt.ToolTip.attach(copy, "Copy")
        copy.clicked.connect(self._copy_source)
        top.addLayout(language_column)
        top.addStretch()
        top.addWidget(copy, 0, Qt.AlignTop)

        code_label = fluentqt.Label("", content)
        code_label.setObjectName("galleryCodeBlockText")
        code_label.setTextFormat(Qt.TextFormat.RichText)
        code_label.setTextInteractionFlags(Qt.TextSelectableByMouse)
        code_label.setAlignment(Qt.AlignLeft | Qt.AlignTop)
        code_label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        code_label.setWordWrap(True)
        code_label.setMinimumWidth(0)
        code_label.setSizePolicy(QSizePolicy.Ignored, QSizePolicy.Preferred)
        fixed = QFontDatabase.systemFont(QFontDatabase.FixedFont)
        fixed.setPixelSize(14)
        code_label.setFont(fixed)
        layout.addLayout(top)
        layout.addWidget(code_label)
        self.setOwnedContentWidget(content)
        self._language = language
        self._underline = underline
        self._copy_button = copy
        self._code_label = code_label
        self.expansionTransitionStarted.connect(
            self._on_expansion_transition_started
        )
        self.refresh_theme()

    @staticmethod
    def _escape_code(text: str, line_state: list[bool] | None = None) -> str:
        """Preserve indentation while leaving inter-token spaces wrappable."""

        state = line_state if line_state is not None else [True]
        escaped: list[str] = []
        index = 0
        while index < len(text):
            current = text[index]
            if current in "\r\n":
                if (
                    current == "\r"
                    and index + 1 < len(text)
                    and text[index + 1] == "\n"
                ):
                    index += 1
                escaped.append("<br/>")
                state[0] = True
                index += 1
                continue
            if current in " \t":
                width = 0
                while index < len(text) and text[index] in " \t":
                    width += 4 if text[index] == "\t" else 1
                    index += 1
                if state[0]:
                    escaped.append("&nbsp;" * width)
                else:
                    escaped.append("&nbsp;" * max(0, width - 1) + " ")
                continue
            escaped.append(html.escape(current, quote=True))
            state[0] = False
            index += 1
        return "".join(escaped)

    @staticmethod
    def _token_span(color: str, escaped_text: str) -> str:
        return '<span style="color:{0};">{1}</span>'.format(
            color, escaped_text
        )

    @classmethod
    def _highlight_python_to_html(cls, code: str, dark: bool) -> str:
        palette = (
            {
                "text": "#D4D4D4",
                "keyword": "#569CD6",
                "type": "#4EC9B0",
                "function": "#DCDCAA",
                "string": "#CE9178",
                "comment": "#6A9955",
                "number": "#B5CEA8",
            }
            if dark
            else {
                "text": "#1F1F1F",
                "keyword": "#0000FF",
                "type": "#267F99",
                "function": "#795E26",
                "string": "#A31515",
                "comment": "#008000",
                "number": "#098658",
            }
        )
        lines = code.splitlines(keepends=True)
        if not lines or not lines[-1].endswith(("\n", "\r")):
            lines.append("")
        offsets = [0]
        for line in lines:
            offsets.append(offsets[-1] + len(line))

        def absolute(position: tuple[int, int]) -> int:
            row, column = position
            return offsets[min(max(row - 1, 0), len(offsets) - 1)] + column

        try:
            tokens = list(tokenize.generate_tokens(io.StringIO(code).readline))
        except (IndentationError, tokenize.TokenError):
            body = cls._escape_code(code)
            return cls._token_span(palette["text"], body) if body else ""

        ignored = {
            token.INDENT,
            token.DEDENT,
            token.NL,
            token.NEWLINE,
            token.ENDMARKER,
        }
        result: list[str] = []
        cursor = 0
        line_state = [True]
        for index, current in enumerate(tokens):
            start = absolute(current.start)
            end = absolute(current.end)
            if start > cursor:
                result.append(
                    cls._escape_code(code[cursor:start], line_state)
                )
            text = code[start:end]
            color: str | None = None
            if current.type == token.NAME:
                if keyword.iskeyword(current.string):
                    color = palette["keyword"]
                else:
                    following = None
                    for candidate in tokens[index + 1 :]:
                        if candidate.type not in ignored:
                            following = candidate
                            break
                    if following is not None and following.string == "(":
                        color = palette["function"]
                    elif current.string[:1].isupper():
                        color = palette["type"]
            elif current.type == token.STRING:
                color = palette["string"]
            elif current.type == token.COMMENT:
                color = palette["comment"]
            elif current.type == token.NUMBER:
                color = palette["number"]
            escaped_text = cls._escape_code(text, line_state)
            # Commas are semantic argument boundaries. For qualified names,
            # keep ``fluentqt.Type`` together and expose a fallback break only
            # after the second (or later) dot. This prevents fragments such as
            # a bare ``fluentqt.`` while still allowing a very narrow window to
            # wrap ``fluentqt.Type.NestedEnum.Value`` without horizontal spill.
            dotted_depth = 0
            if current.type == token.OP and current.string == ".":
                candidate_index = index
                while candidate_index >= 0:
                    candidate = tokens[candidate_index]
                    if candidate.type == token.NAME:
                        candidate_index -= 1
                        continue
                    if candidate.type == token.OP and candidate.string == ".":
                        dotted_depth += 1
                        candidate_index -= 1
                        continue
                    break
            if (
                current.type == token.OP
                and (
                    current.string == ","
                    or (current.string == "." and dotted_depth >= 2)
                )
            ):
                escaped_text += "&#8203;"
            result.append(
                cls._token_span(color, escaped_text)
                if color is not None
                else escaped_text
            )
            cursor = max(cursor, end)
        if cursor < len(code):
            result.append(cls._escape_code(code[cursor:], line_state))
        return '<span style="color:{0};">{1}</span>'.format(
            palette["text"], "".join(result)
        )

    def _apply_highlighted_code(self) -> None:
        self._code_label.setText(
            self._highlight_python_to_html(
                self._display_code,
                fluentqt.current_theme() == fluentqt.Theme.Dark,
            )
        )
        self._highlighted = True

    def _on_expansion_transition_started(self, expanding: bool) -> None:
        if expanding and not self._highlighted:
            self._apply_highlighted_code()

    def _copy_source(self) -> None:
        QApplication.clipboard().setText(self._code)
        toast = fluentqt.Toast.showToast(
            self,
            "Copied to clipboard",
            fluentqt.Toast.Severity.Success,
            1700,
            fluentqt.Toast.Placement.Top,
            QMargins(16, 50, 16, 16),
        )
        if toast is not None:
            toast.setObjectName("galleryToast")
            card = toast.findChild(QWidget, "fluentToastCard")
            if card is not None:
                card.setObjectName("galleryToastCard")
            icon = toast.findChild(QWidget, "fluentToastIcon")
            if icon is not None:
                icon.setObjectName("galleryToastIcon")
        self._copy_button.setIconGlyph("\ue73e", 16)
        _single_shot(1300, self, self._restore_copy_icon)

    def _restore_copy_icon(self) -> None:
        self._copy_button.setIconGlyph("\ue8c8", 16)

    def refresh_theme(self) -> None:
        colors = gallery_colors()
        self._underline.setStyleSheet(
            "background: {0}; border-radius: 1px;".format(
                css_color(colors.accent)
            )
        )
        self._language.setStyleSheet(
            "color: {0}; background: transparent;".format(
                css_color(colors.text_secondary)
            )
        )
        self._code_label.setStyleSheet(
            "color: {0}; background: transparent; selection-background-color: {1};".format(
                css_color(colors.text_primary), css_color(colors.accent)
            )
        )
        if self._highlighted:
            self._apply_highlighted_code()


_ROUTE_ID_ROLE = int(Qt.UserRole) + 1
_KIND_ROLE = _ROUTE_ID_ROLE + 1
_PARENT_ROUTE_ID_ROLE = _ROUTE_ID_ROLE + 2
_ICON_GLYPH_ROLE = _ROUTE_ID_ROLE + 3
_INDICATOR_INSET_ROLE = _ROUTE_ID_ROLE + 4

_SECTION_HEADER = 0
_ROOT_ROUTE = 1
_CATEGORY_ROUTE = 2
_COMPONENT_ROUTE = 3
_FOOTER_ROUTE = 4

_NAV_ICON_CACHE: dict[
    tuple[str, int, int, int, int, int, int, int], QPixmap
] = {}


def _navigation_icon_pixmap(
    name: str,
    size: int,
    color: QColor,
    rotation: int = 0,
    device_pixel_ratio: float = 1.0,
) -> QPixmap:
    """Render through FluentQt's FontIcon so the Python shell uses native glyphs."""

    dpr = max(1.0, float(device_pixel_ratio))
    key = (
        name,
        size,
        color.red(),
        color.green(),
        color.blue(),
        int(fluentqt.theme_revision()),
        int(rotation),
        _qround(dpr * 1000.0),
    )
    cached = _NAV_ICON_CACHE.get(key)
    if cached is not None:
        return cached
    icon = fluentqt.FontIcon(name)
    icon.setIconSize(size)
    icon.setColor(color)
    icon.setRotation(rotation)
    icon.setFixedSize(size, size)
    icon.setAttribute(Qt.WA_DontShowOnScreen, True)
    icon.setAttribute(Qt.WA_TranslucentBackground, True)
    icon.setAutoFillBackground(False)
    palette = icon.palette()
    palette.setColor(QPalette.Window, Qt.transparent)
    palette.setColor(QPalette.Base, Qt.transparent)
    icon.setPalette(palette)
    icon.setStyleSheet("background: transparent; border: none;")
    physical_size = max(1, _qround(size * dpr))
    pixmap = QPixmap(physical_size, physical_size)
    pixmap.setDevicePixelRatio(dpr)
    pixmap.fill(Qt.transparent)
    icon.render(pixmap)
    icon.deleteLater()
    _NAV_ICON_CACHE[key] = pixmap
    return pixmap


def gallery_font_icon_pixmap(
    name: str,
    size: int,
    color: QColor,
    device_pixel_ratio: float | None = None,
) -> QPixmap:
    """Render a catalog icon through FluentQt's native FontIcon implementation."""

    dpr = (
        _primary_screen_device_pixel_ratio()
        if device_pixel_ratio is None
        else _normalized_device_pixel_ratio(device_pixel_ratio)
    )
    return _navigation_icon_pixmap(name, size, color, 0, dpr)


_ICON_VARIANT_PATTERN = re.compile(r"^(ic_fluent_.+)_([0-9]+)_regular$")
_DIRECT_ICON_CATALOG: dict[str, int] | None = None
_DIRECT_ICON_ALIASES: dict[int, str] | None = None


def _direct_icon_catalog() -> dict[str, int]:
    global _DIRECT_ICON_CATALOG
    if _DIRECT_ICON_CATALOG is None:
        path = asset_path("icon_catalog.json")
        if path.is_file():
            with path.open("r", encoding="utf-8") as stream:
                _DIRECT_ICON_CATALOG = {
                    str(name): int(codepoint)
                    for name, codepoint in json.load(stream).items()
                }
        else:
            _DIRECT_ICON_CATALOG = {}
    return _DIRECT_ICON_CATALOG


def _direct_icon_aliases() -> dict[int, str]:
    global _DIRECT_ICON_ALIASES
    if _DIRECT_ICON_ALIASES is None:
        path = asset_path("icon_aliases.json")
        if path.is_file():
            with path.open("r", encoding="utf-8") as stream:
                _DIRECT_ICON_ALIASES = {
                    int(codepoint, 16): str(target)
                    for codepoint, target in json.load(stream).items()
                }
        else:
            _DIRECT_ICON_ALIASES = {}
    return _DIRECT_ICON_ALIASES


def _snap_direct_icon_size(requested_size: int) -> int:
    optical_sizes = (12, 16, 20, 24, 28, 32, 40, 48, 64)
    if requested_size <= 0:
        return 16
    if requested_size < 12:
        return requested_size
    return min(
        optical_sizes,
        key=lambda candidate: (abs(requested_size - candidate), -candidate),
    )


def _direct_icon_glyph(name: str, requested_size: int) -> str:
    catalog = _direct_icon_catalog()
    fallback = chr(catalog[name]) if name in catalog else name
    source_name = name if name in catalog else ""
    if not source_name and len(name) == 1:
        codepoint = ord(name)
        source_name = _direct_icon_aliases().get(codepoint, "")
        if not source_name:
            source_name = next(
                (
                    candidate_name
                    for candidate_name, candidate_codepoint in catalog.items()
                    if candidate_codepoint == codepoint
                ),
                "",
            )
    match = _ICON_VARIANT_PATTERN.match(source_name)
    if match:
        family = match.group(1)
        snapped = _snap_direct_icon_size(requested_size)
        exact_name = "{0}_{1}_regular".format(family, snapped)
        if exact_name in catalog:
            return chr(catalog[exact_name])
        candidates = []
        for candidate_name, codepoint in catalog.items():
            candidate_match = _ICON_VARIANT_PATTERN.match(candidate_name)
            if candidate_match and candidate_match.group(1) == family:
                candidate_size = int(candidate_match.group(2))
                candidates.append(
                    (
                        abs(candidate_size - snapped),
                        -candidate_size,
                        codepoint,
                    )
                )
        if candidates:
            return chr(min(candidates)[2])
    return fallback


def _direct_icon_font(pixel_size: int) -> QFont:
    font = QFont("FluentQt Icons")
    font.setPixelSize(_snap_direct_icon_size(pixel_size))
    font.setHintingPreference(
        QFont.HintingPreference.PreferNoHinting
        if sys.platform == "win32"
        else QFont.HintingPreference.PreferVerticalHinting
    )
    font.setStyleStrategy(
        _font_style_strategy(
            QFont.StyleStrategy.PreferQuality,
            QFont.StyleStrategy.PreferAntialias,
            QFont.StyleStrategy.NoSubpixelAntialias,
        )
    )
    return font


def _draw_native_font_icon(
    painter: QPainter,
    target_rect: QRectF | QRect,
    name: str,
    size: int,
    color: QColor,
    rotation: float = 0.0,
    resolve_optical: bool = True,
) -> None:
    """Mirror ``Typography::Icons::paintGlyph`` on the destination painter."""

    rect = QRectF(target_rect)
    if rect.isEmpty() or not name or size <= 0:
        return
    device = painter.device()
    dpr = max(
        1.0,
        float(device.devicePixelRatioF()) if device is not None else 1.0,
    )
    aligned_rect = QRectF(
        QPointF(
            _qround(rect.left() * dpr) / dpr,
            _qround(rect.top() * dpr) / dpr,
        ),
        QPointF(
            _qround(rect.right() * dpr) / dpr,
            _qround(rect.bottom() * dpr) / dpr,
        ),
    )
    painter.save()
    if abs(rotation) > 0.0001:
        painter.translate(aligned_rect.center())
        painter.rotate(rotation)
        painter.translate(-aligned_rect.center())
    painter.setFont(_direct_icon_font(size))
    painter.setPen(color)
    painter.drawText(
        aligned_rect,
        Qt.AlignmentFlag.AlignCenter,
        _direct_icon_glyph(name, size) if resolve_optical else name,
    )
    painter.restore()


class _GalleryNavigationDelegate(QStyledItemDelegate):
    """Direct Python port of GalleryNavigationDelegate's stable paint state."""

    @staticmethod
    def _property(widget: QWidget | None, name: str, fallback: object) -> object:
        current = widget
        while current is not None:
            value = current.property(name)
            if value is not None:
                return value
            current = current.parentWidget()
        return fallback

    @classmethod
    def _compact(cls, widget: QWidget | None) -> bool:
        return bool(cls._property(widget, "galleryCompact", False))

    @classmethod
    def _compact_progress(cls, widget: QWidget | None) -> float:
        fallback = 1.0 if cls._compact(widget) else 0.0
        return max(
            0.0,
            min(
                1.0,
                float(
                    cls._property(
                        widget,
                        "galleryCompactVisualProgress",
                        fallback,
                    )
                ),
            ),
        )

    @classmethod
    def _settings_rotation(cls, widget: QWidget | None) -> float:
        return float(
            cls._property(widget, "gallerySettingsIconRotation", 0.0)
        )

    def sizeHint(self, option, index: QModelIndex) -> QSize:
        if int(index.data(_KIND_ROLE) or 0) == _SECTION_HEADER:
            progress = self._compact_progress(option.widget)
            return QSize(1, _qround(32.0 * (1.0 - progress)))
        return QSize(1, 36)

    def paint(self, painter: QPainter, option, index: QModelIndex) -> None:
        if not index.isValid():
            return
        painter.save()
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)
        colors = gallery_colors()
        kind = int(index.data(_KIND_ROLE) or 0)
        text = str(index.data(Qt.DisplayRole) or "")
        view = option.widget
        compact = self._compact(view)
        compact_progress = self._compact_progress(view)
        expanded_opacity = max(0.0, min(1.0, 1.0 - compact_progress))

        if kind == _SECTION_HEADER:
            if expanded_opacity > 0.01:
                painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.Caption))
                painter.setPen(colors.text_secondary)
                painter.setOpacity(expanded_opacity)
                painter.drawText(
                    option.rect.adjusted(
                        _qround(16.0 - 6.0 * compact_progress), 6, -8, 0
                    ),
                    Qt.AlignLeft | Qt.AlignVCenter,
                    text,
                )
            painter.restore()
            return

        viewport_width = (
            view.viewport().width()
            if isinstance(view, QAbstractItemView) and view.viewport() is not None
            else option.rect.width()
        )
        fully_compact = compact and compact_progress >= 0.999
        available_width = 48 if fully_compact else viewport_width
        right_inset = 4 if compact else 12
        background = QRectF(
            4,
            option.rect.top() + 2,
            max(0, available_width - 4 - right_inset),
            option.rect.height() - 4,
        )
        selected = bool(option.state & QStyle.State_Selected)
        hovered = bool(option.state & QStyle.State_MouseOver)
        pressed = bool(option.state & QStyle.State_Sunken) and hovered
        if selected or hovered or pressed:
            fill = colors.subtle_tertiary if pressed else colors.subtle
            painter.setPen(Qt.NoPen)
            painter.setBrush(fill)
            from .foundation_pages import _radii

            control_radius, _overlay_radius = _radii()
            painter.drawRoundedRect(
                background, control_radius, control_radius
            )

        has_children = bool(index.model() and index.model().hasChildren(index))
        chevron_rect = QRectF()
        chevron_opacity = 0.0 if compact else expanded_opacity
        if has_children and chevron_opacity > 0.01:
            chevron_rect = QRectF(
                background.right() - 6 - 28,
                background.top(),
                28,
                background.height(),
            )
            painter.save()
            painter.setOpacity(chevron_opacity)
            rotation_progress = 0.0
            if isinstance(view, fluentqt.TreeView):
                rotation_progress = float(view.chevronRotation(index))
            elif view is not None and bool(view.isExpanded(index)):
                rotation_progress = 1.0
            _draw_native_font_icon(
                painter,
                chevron_rect,
                chr(0xE972),
                16,
                colors.text_secondary,
                180.0 * max(0.0, min(1.0, rotation_progress)),
            )
            painter.restore()

        icon_name = str(index.data(_ICON_GLYPH_ROLE) or "")
        route_id = str(index.data(_ROUTE_ID_ROLE) or "")
        content_left = background.left() + 12
        compact_icon_left = max(0.0, (48.0 - 20.0) / 2.0)
        icon_left = content_left + (
            compact_icon_left - content_left
        ) * compact_progress
        text_x = (
            background.left() + 43
            if kind == _COMPONENT_ROUTE
            else content_left
        )
        if icon_name:
            icon_color = colors.text_primary if selected else colors.text_secondary
            icon_rect = QRectF(
                icon_left, background.top(), 20, background.height()
            )
            rotation = (
                self._settings_rotation(view)
                if route_id == "settings"
                else 0.0
            )
            _draw_native_font_icon(
                painter,
                icon_rect,
                icon_name,
                16,
                icon_color,
                rotation,
            )
            text_x = background.left() + 43
        elif kind != _COMPONENT_ROUTE:
            text_x = background.left() + 43

        if expanded_opacity > 0.01:
            font = fluentqt.font_for_role(fluentqt.FontRole.Body)
            font.setPixelSize(14)
            painter.setFont(font)
            painter.setPen(colors.text_primary)
            text_right = (
                chevron_rect.left() - 8
                if has_children and not compact
                else background.right() - 8
            )
            text_rect = QRectF(
                text_x - 6.0 * compact_progress,
                background.top(),
                max(0, text_right - text_x),
                background.height(),
            )
            elided = painter.fontMetrics().elidedText(
                text, Qt.ElideRight, _qround(text_rect.width())
            )
            painter.setOpacity(expanded_opacity)
            painter.drawText(text_rect, Qt.AlignLeft | Qt.AlignVCenter, elided)
        painter.restore()


def _configure_navigation_tree(tree: fluentqt.TreeView) -> None:
    tree.setBorderVisible(False)
    tree.setBackgroundVisible(False)
    tree.setHorizontalFluentScrollBarEnabled(False)
    tree.setOverscrollEnabled(False)
    tree.setIndentation(0)
    tree.setIndicatorMotionAnimationEnabled(True)
    tree.setSelectionIndicatorVisible(True)
    tree.setSelectionIndicatorInset(7.0)
    tree.setSelectionIndicatorHeight(14.0)
    tree.setSelectionMode(fluentqt.SelectionMode.Single)
    tree.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
    bar = tree.verticalFluentScrollBar()
    if bar is not None:
        bar.setThickness(5)


def _navigation_item(
    title: str,
    route_id: str,
    kind: int,
    parent_id: str = "",
    icon_name: str = "",
) -> QStandardItem:
    item = QStandardItem(title)
    item.setEditable(False)
    item.setData(route_id, _ROUTE_ID_ROLE)
    item.setData(kind, _KIND_ROLE)
    item.setData(parent_id, _PARENT_ROUTE_ID_ROLE)
    item.setData(icon_name if kind != _COMPONENT_ROUTE else "", _ICON_GLYPH_ROLE)
    item.setToolTip("" if kind == _SECTION_HEADER else title)
    if kind == _COMPONENT_ROUTE:
        item.setData(4 + 43 - 3 - 8, _INDICATOR_INSET_ROLE)
    item.setFlags(
        Qt.ItemIsEnabled
        if kind == _SECTION_HEADER
        else Qt.ItemIsEnabled | Qt.ItemIsSelectable
    )
    return item


def _show_compact_navigation_tooltip(
    owner: QWidget,
    index: QModelIndex,
) -> None:
    tree = owner._tree
    if (
        not owner._compact
        or owner._compact_visual_progress < 0.999
        or not index.isValid()
    ):
        return
    kind = int(index.data(_KIND_ROLE) or 0)
    text = str(index.data(Qt.ToolTipRole) or "")
    row_rect = tree.visualRect(index)
    if (
        kind == _SECTION_HEADER
        or not text
        or row_rect.isEmpty()
        or not tree.viewport().rect().intersects(row_rect)
    ):
        _hide_compact_navigation_tooltip(owner)
        return
    if owner._compact_tooltip is None:
        owner._compact_tooltip = fluentqt.ToolTip(owner)
        owner._compact_tooltip.setObjectName(
            "galleryCompactNavigationToolTip"
        )
        owner._compact_tooltip.setAnimationEnabled(True)
    tooltip = owner._compact_tooltip
    tooltip.setText(text)
    owner._compact_tooltip_index = QPersistentModelIndex(index)
    shadow = tooltip.shadowMargin()
    anchor = tree.viewport().mapToGlobal(
        QPoint(row_rect.center().x(), row_rect.top())
    )
    tooltip.move(
        anchor.x() - tooltip.width() // 2,
        anchor.y() - 4 - tooltip.height() + shadow,
    )
    tooltip.show()
    tooltip.raise_()


def _hide_compact_navigation_tooltip(owner: QWidget) -> None:
    owner._compact_tooltip_index = QPersistentModelIndex()
    if owner._compact_tooltip is not None:
        owner._compact_tooltip.hide()


def _filter_compact_navigation_tooltip(
    owner: QWidget,
    watched: object,
    event: QEvent,
) -> bool:
    tree = owner._tree
    if watched is not tree.viewport():
        return False
    event_type = event.type()
    if event_type == QEvent.Type.ToolTip:
        index = tree.indexAt(event.pos())
        if (
            owner._compact
            and owner._compact_visual_progress >= 0.999
            and index.isValid()
        ):
            _show_compact_navigation_tooltip(owner, index)
        else:
            _hide_compact_navigation_tooltip(owner)
        return True
    if (
        event_type == QEvent.Type.MouseMove
        and owner._compact_tooltip is not None
        and owner._compact_tooltip.isVisible()
        and tree.indexAt(event.position().toPoint())
        != owner._compact_tooltip_index
    ):
        _hide_compact_navigation_tooltip(owner)
    elif event_type in (
        QEvent.Type.Leave,
        QEvent.Type.Hide,
        QEvent.Type.MouseButtonPress,
        QEvent.Type.Wheel,
        QEvent.Type.Resize,
    ):
        _hide_compact_navigation_tooltip(owner)
    return False


def _navigation_key_moves_current_item(event: QEvent) -> bool:
    if event.type() != QEvent.Type.KeyPress:
        return False
    modifiers = event.modifiers() & ~Qt.KeyboardModifier.KeypadModifier
    return modifiers == Qt.KeyboardModifier.NoModifier and event.key() in (
        Qt.Key.Key_Up,
        Qt.Key.Key_Down,
        Qt.Key.Key_Home,
        Qt.Key.Key_End,
        Qt.Key.Key_PageUp,
        Qt.Key.Key_PageDown,
    )


class GalleryNavigationPane(QWidget):
    """The same TreeView/model/delegate navigation structure as the C++ shell."""

    routeActivated = Signal(str)

    def __init__(
        self,
        categories: Iterable[tuple[str, str, tuple[tuple[str, str], ...]]],
        foundation_children: tuple[tuple[str, str], ...],
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self.setObjectName("galleryMainNavigationPane")
        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)

        tree = fluentqt.TreeView(self)
        tree.setObjectName("galleryMainNavigationTreeView")
        _configure_navigation_tree(tree)
        model = QStandardItemModel(tree)
        route_items: dict[str, QStandardItem] = {}

        def append_root(item: QStandardItem) -> None:
            model.appendRow(item)
            route_id = str(item.data(_ROUTE_ID_ROLE) or "")
            if route_id:
                route_items[route_id] = item

        append_root(
            _navigation_item(
                "Home", "home", _ROOT_ROUTE, icon_name=route_icon_name("home")
            )
        )
        foundation = _navigation_item(
            "Foundation",
            "foundation",
            _CATEGORY_ROUTE,
            icon_name=route_icon_name("foundation"),
        )
        append_root(foundation)
        for child_id, child_title in foundation_children:
            child = _navigation_item(
                child_title,
                child_id,
                _COMPONENT_ROUTE,
                parent_id="foundation",
            )
            foundation.appendRow(child)
            route_items[child_id] = child

        append_root(_navigation_item("Controls", "", _SECTION_HEADER))
        append_root(
            _navigation_item(
                "All",
                "all-controls",
                _ROOT_ROUTE,
                parent_id="controls",
                icon_name=route_icon_name("all-controls"),
            )
        )
        for category_id, title, child_routes in categories:
            category = _navigation_item(
                title,
                category_id,
                _CATEGORY_ROUTE,
                parent_id="controls",
                icon_name=route_icon_name(category_id),
            )
            append_root(category)
            for child_id, child_title in child_routes:
                child = _navigation_item(
                    child_title,
                    child_id,
                    _COMPONENT_ROUTE,
                    parent_id=category_id,
                )
                category.appendRow(child)
                route_items[child_id] = child

        tree.setModel(model)
        delegate = _GalleryNavigationDelegate(tree)
        tree.setItemDelegate(delegate)
        tree.collapseAll()
        tree.itemPressed.connect(self._activate_index)
        outer.addWidget(tree)
        self._tree = tree
        self._model = model
        self._delegate = delegate
        self._route_items = route_items
        self._selected_route_id = ""
        self._surface_visible = False
        self._compact_tooltip: fluentqt.ToolTip | None = None
        self._compact_tooltip_index = QPersistentModelIndex()
        self._compact_flyout: fluentqt.Popup | None = None
        self._compact_flyout_panel: QWidget | None = None
        self._compact_flyout_anchor: QWidget | None = None
        self._compact = False
        self._compact_visual_progress = 0.0
        self._compact_visual_animation = QVariantAnimation(self)
        self._compact_visual_animation.setObjectName(
            "galleryCompactVisualAnimation"
        )
        self._compact_visual_animation.valueChanged.connect(
            self._set_compact_visual_progress
        )
        self._compact_visual_animation.finished.connect(
            self._finish_compact_visual_transition
        )
        tree.installEventFilter(self)
        tree.viewport().installEventFilter(self)
        tree.setProperty("fluentPreserveParentSurface", False)
        tree.viewport().setProperty("fluentPreserveParentSurface", False)
        self.set_compact(False)

    def _activate_index(
        self,
        index: QModelIndex,
        pointer_activation: bool = True,
    ) -> None:
        route_id = str(index.data(_ROUTE_ID_ROLE) or "")
        if not route_id:
            return
        if not pointer_activation and route_id == self._selected_route_id:
            return
        self._selected_route_id = route_id
        self.sync_selected(route_id)
        has_children = self._model.hasChildren(index)
        if pointer_activation:
            compact_rail = self._compact and not self._surface_visible
            if compact_rail and has_children:
                self._show_compact_flyout(index)
            elif has_children:
                self._tree.toggleExpanded(index)
            elif compact_rail:
                self._close_compact_flyout()
        self.routeActivated.emit(route_id)

    def event(self, event: QEvent) -> bool:
        if event.type() == QEvent.Type.DynamicPropertyChange and (
            bytes(event.propertyName()) == b"fluentNavPaneFloating"
        ):
            self._set_surface_visible(
                bool(self.property("fluentNavPaneFloating"))
            )
        return super().event(event)

    def eventFilter(self, watched, event: QEvent) -> bool:
        if watched is self._tree and _navigation_key_moves_current_item(event):
            _single_shot(
                0,
                self,
                lambda: self._activate_index(
                    self._tree.currentIndex(), False
                ),
            )
        if _filter_compact_navigation_tooltip(self, watched, event):
            return True
        return super().eventFilter(watched, event)

    def _set_surface_visible(self, visible: bool) -> None:
        visible = bool(visible)
        if self._surface_visible == visible:
            return
        self._surface_visible = visible
        self.setAttribute(Qt.WA_NoSystemBackground, visible)
        self.setAttribute(Qt.WA_TranslucentBackground, visible)
        self._tree.setBackgroundVisible(False)
        self._tree.setProperty("fluentPreserveParentSurface", visible)
        viewport = self._tree.viewport()
        viewport.setProperty("fluentPreserveParentSurface", visible)
        viewport.setAttribute(Qt.WA_NoSystemBackground, visible)
        viewport.update()
        self.update()

    def sync_selected(self, route_id: str) -> None:
        self._selected_route_id = route_id
        item = self._route_items.get(route_id)
        if item is None:
            self._tree.clearSelection()
            self._tree.setCurrentIndex(QModelIndex())
            return
        index = item.index()
        visual_index = (
            index.parent()
            if self._compact and index.parent().isValid()
            else index
        )
        indicator_inset = visual_index.data(_INDICATOR_INSET_ROLE)
        self._tree.setSelectionIndicatorInset(
            float(indicator_inset) if indicator_inset is not None else 7.0
        )
        parent = index.parent()
        while parent.isValid():
            if not self._compact:
                self._tree.expand(parent)
            parent = parent.parent()
        vertical = self._tree.verticalFluentScrollBar()
        horizontal = self._tree.horizontalFluentScrollBar()
        vertical_blocked = vertical.signalsBlocked() if vertical is not None else False
        horizontal_blocked = (
            horizontal.signalsBlocked() if horizontal is not None else False
        )
        if vertical is not None:
            vertical.blockSignals(True)
        if horizontal is not None:
            horizontal.blockSignals(True)
        try:
            self._tree.setSelectedItem(visual_index)
            self._tree.scrollTo(visual_index, QAbstractItemView.EnsureVisible)
        finally:
            if vertical is not None:
                vertical.blockSignals(vertical_blocked)
            if horizontal is not None:
                horizontal.blockSignals(horizontal_blocked)

    def set_compact(self, compact: bool) -> None:
        compact = bool(compact)
        _hide_compact_navigation_tooltip(self)
        if self._compact == compact:
            self._sync_compact_visual_properties()
            self._update_compact_row_visibility()
            return
        self._compact = compact
        self._sync_compact_visual_properties()
        if self._compact:
            self._tree.collapseAll()
        else:
            self._close_compact_flyout(False)
        self._update_compact_row_visibility()
        self._start_compact_visual_transition()
        if self._selected_route_id:
            self.sync_selected(self._selected_route_id)

    def _sync_compact_visual_properties(self) -> None:
        for widget in (self._tree, self._tree.viewport()):
            widget.setProperty("galleryCompact", self._compact)
            widget.setProperty(
                "galleryCompactVisualProgress",
                self._compact_visual_progress,
            )

    def _set_compact_visual_progress(self, value: object) -> None:
        progress = max(0.0, min(1.0, float(value)))
        if abs(self._compact_visual_progress - progress) <= 0.0001:
            return
        self._compact_visual_progress = progress
        self._sync_compact_visual_properties()
        self._update_compact_row_visibility()
        self._tree.doItemsLayout()
        self._tree.viewport().update()

    def _update_compact_row_visibility(self) -> None:
        hide_headers = (
            self._compact and self._compact_visual_progress >= 0.999
        )
        for row in range(self._model.rowCount()):
            index = self._model.index(row, 0)
            self._tree.setRowHidden(
                row,
                QModelIndex(),
                hide_headers
                and int(index.data(_KIND_ROLE) or 0) == _SECTION_HEADER,
            )

    def _start_compact_visual_transition(self) -> None:
        end_value = 1.0 if self._compact else 0.0
        if abs(self._compact_visual_progress - end_value) <= 0.0001:
            self._set_compact_visual_progress(end_value)
            self._update_compact_row_visibility()
            return
        self._compact_visual_animation.stop()
        if not self.isVisible() or not self.window().isVisible():
            self._compact_visual_progress = end_value
            self._sync_compact_visual_properties()
            self._update_compact_row_visibility()
            self._tree.doItemsLayout()
            self._tree.viewport().update()
            return
        self._compact_visual_animation.setDuration(250)
        self._compact_visual_animation.setEasingCurve(QEasingCurve.OutCubic)
        self._compact_visual_animation.setStartValue(
            self._compact_visual_progress
        )
        self._compact_visual_animation.setEndValue(end_value)
        self._compact_visual_animation.start()

    def _finish_compact_visual_transition(self) -> None:
        self._set_compact_visual_progress(1.0 if self._compact else 0.0)
        self._update_compact_row_visibility()

    def _show_compact_flyout(self, index: QModelIndex) -> None:
        if (
            not self._compact
            or self._surface_visible
            or not index.isValid()
            or not self._model.hasChildren(index)
        ):
            return
        visual_rect = self._tree.visualRect(index)
        if visual_rect.isEmpty():
            return
        self._close_compact_flyout(False)

        if self._compact_flyout_anchor is None:
            self._compact_flyout_anchor = QWidget(self._tree.viewport())
            self._compact_flyout_anchor.setObjectName(
                "galleryCompactNavigationFlyoutAnchor"
            )
            self._compact_flyout_anchor.setAttribute(
                Qt.WA_TransparentForMouseEvents
            )
        anchor = self._compact_flyout_anchor
        anchor.setGeometry(0, visual_rect.top(), 48, visual_rect.height())
        anchor.show()

        popup = fluentqt.Popup(self)
        popup.setObjectName("galleryCompactNavigationFlyout")
        popup.setAnimationEnabled(True)
        popup.setClosePolicy(
            fluentqt.Popup.ClosePolicy(
                fluentqt.Popup.CloseFlag.CloseOnPressOutside
                | fluentqt.Popup.CloseFlag.CloseOnEscape
            )
        )
        popup.setLightDismissConsumesPress(True)
        popup.addLightDismissPassthrough(self)

        panel = QWidget(popup)
        panel.setObjectName("galleryCompactNavigationFlyoutPanel")
        panel.setAttribute(Qt.WA_NoSystemBackground)
        panel_layout = QVBoxLayout(panel)
        panel_layout.setContentsMargins(0, 0, 0, 0)
        panel_layout.setSpacing(2)
        rows: list[_GalleryCompactFlyoutRow] = []
        for row_index in range(self._model.rowCount(index)):
            child = self._model.index(row_index, 0, index)
            child_route = str(child.data(_ROUTE_ID_ROLE) or "")
            if not child_route:
                continue
            row = _GalleryCompactFlyoutRow(
                child_route,
                str(child.data(Qt.DisplayRole) or ""),
                child_route == self._selected_route_id,
                panel,
            )
            row.activated.connect(self._activate_compact_child)
            panel_layout.addWidget(row)
            rows.append(row)
        if not rows:
            popup.deleteLater()
            return

        content_width = max(row.sizeHint().width() for row in rows)
        content_height = len(rows) * 36 + max(0, len(rows) - 1) * 2
        host = self.window()
        surface = host.property("fluentOverlaySurfaceRect")
        if not isinstance(surface, QRect):
            surface = host.rect()
        else:
            surface = surface.intersected(host.rect())
            if surface.isEmpty():
                surface = host.rect()
        anchor_top_left = anchor.mapTo(host, QPoint(0, 0))
        tree_top = self._tree.mapTo(host, QPoint(0, 0)).y()
        safe_top = max(surface.top() + 12, tree_top + 12)
        safe_bottom = surface.bottom() + 1 - 12
        max_visible_height = max(36, safe_bottom - safe_top)
        card_width = content_width + 6
        card_height = min(content_height + 8, max_visible_height)
        preferred_top = anchor_top_left.y() - 4
        card_top = max(
            safe_top,
            min(preferred_top, max(safe_top, safe_bottom - card_height)),
        )
        card_left = anchor_top_left.x() + anchor.width() + 8

        popup.resize(card_width + 16, card_height + 16)
        panel.setGeometry(
            11,
            12,
            content_width,
            content_height,
        )
        panel.show()
        popup.setPosition(host, QPoint(card_left, card_top))

        self._compact_flyout = popup
        self._compact_flyout_panel = panel

        def clear_popup(*_unused: object) -> None:
            if self._compact_flyout is popup:
                self._compact_flyout = None
                self._compact_flyout_panel = None

        popup.destroyed.connect(clear_popup)
        popup.open()
        end_position = popup.pos()
        entrance = QPropertyAnimation(popup, b"pos", popup)
        entrance.setObjectName(
            "galleryCompactNavigationFlyoutEntranceAnimation"
        )
        entrance.setDuration(100)
        entrance.setEasingCurve(QEasingCurve.OutCubic)
        entrance.setStartValue(end_position - QPoint(8, 0))
        entrance.setEndValue(end_position)
        popup.move(entrance.startValue())
        entrance.start(QPropertyAnimation.DeleteWhenStopped)

    def _activate_compact_child(self, route_id: str) -> None:
        self._selected_route_id = route_id
        self.sync_selected(route_id)
        self.routeActivated.emit(route_id)
        _single_shot(0, self, self._close_compact_flyout)

    def _close_compact_flyout(self, animated: bool = True) -> None:
        popup = self._compact_flyout
        if popup is None:
            return
        self._compact_flyout = None
        self._compact_flyout_panel = None
        if animated and popup.isVisible():
            popup.closed.connect(popup.deleteLater)
            popup.close()
        else:
            popup.hide()
            popup.deleteLater()
        if self._compact_flyout_anchor is not None:
            self._compact_flyout_anchor.hide()

    def refresh_theme(self) -> None:
        _NAV_ICON_CACHE.clear()
        self._tree.viewport().update()


class GalleryNavigationFooter(QWidget):
    """Footer TreeView using the same Settings item and divider as C++."""

    routeActivated = Signal(str)

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("galleryFooterNavigationPane")
        self.setMinimumHeight(45)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        divider = fluentqt.Divider(self)
        divider.setObjectName("galleryFooterNavigationDivider")
        divider.setLeadingInset(16)
        divider.setTrailingInset(16)
        divider.setFixedHeight(1)
        layout.addWidget(divider)
        self._divider = divider

        tree = fluentqt.TreeView(self)
        tree.setObjectName("galleryFooterNavigationTreeView")
        _configure_navigation_tree(tree)
        model = QStandardItemModel(tree)
        item = _navigation_item(
            "Settings",
            "settings",
            _FOOTER_ROUTE,
            icon_name=route_icon_name("settings"),
        )
        model.appendRow(item)
        tree.setModel(model)
        delegate = _GalleryNavigationDelegate(tree)
        tree.setItemDelegate(delegate)
        tree.itemPressed.connect(self._activate_index)
        layout.addWidget(tree)
        self._tree = tree
        self._model = model
        self._item = item
        self._delegate = delegate
        self._selected_route_id = ""
        self._surface_visible = False
        self._compact_tooltip: fluentqt.ToolTip | None = None
        self._compact_tooltip_index = QPersistentModelIndex()
        self._compact = False
        self._compact_visual_progress = 0.0
        self._compact_visual_animation = QVariantAnimation(self)
        self._compact_visual_animation.setObjectName(
            "galleryFooterCompactVisualAnimation"
        )
        self._compact_visual_animation.valueChanged.connect(
            self._set_compact_visual_progress
        )
        self._compact_visual_animation.finished.connect(
            self._finish_compact_visual_transition
        )
        self._settings_icon_rotation = 0.0
        self._settings_rotation_animation = QVariantAnimation(self)
        self._settings_rotation_animation.setObjectName(
            "gallerySettingsIconRotationAnimation"
        )
        self._settings_rotation_animation.valueChanged.connect(
            self._set_settings_icon_rotation
        )
        self._settings_rotation_animation.finished.connect(
            lambda: self._set_settings_icon_rotation(0.0)
        )
        tree.installEventFilter(self)
        tree.viewport().installEventFilter(self)
        tree.setProperty("fluentPreserveParentSurface", False)
        tree.viewport().setProperty("fluentPreserveParentSurface", False)
        self._sync_compact_visual_properties()
        self._update_divider_palette()

    def _activate_index(
        self,
        index: QModelIndex,
        pointer_activation: bool = True,
    ) -> None:
        route_id = str(index.data(_ROUTE_ID_ROLE) or "")
        if not route_id:
            return
        if not pointer_activation and route_id == self._selected_route_id:
            return
        if route_id == "settings":
            self._start_settings_icon_rotation()
        self._selected_route_id = route_id
        self.sync_selected(route_id)
        self.routeActivated.emit(route_id)

    def event(self, event: QEvent) -> bool:
        if event.type() == QEvent.Type.DynamicPropertyChange and (
            bytes(event.propertyName()) == b"fluentNavPaneFloating"
        ):
            self._set_surface_visible(
                bool(self.property("fluentNavPaneFloating"))
            )
        return super().event(event)

    def eventFilter(self, watched, event: QEvent) -> bool:
        if watched is self._tree and _navigation_key_moves_current_item(event):
            _single_shot(
                0,
                self,
                lambda: self._activate_index(
                    self._tree.currentIndex(), False
                ),
            )
        if _filter_compact_navigation_tooltip(self, watched, event):
            return True
        return super().eventFilter(watched, event)

    def _set_surface_visible(self, visible: bool) -> None:
        visible = bool(visible)
        if self._surface_visible == visible:
            return
        self._surface_visible = visible
        self.setAttribute(Qt.WA_NoSystemBackground, visible)
        self.setAttribute(Qt.WA_TranslucentBackground, visible)
        self._tree.setBackgroundVisible(False)
        self._tree.setProperty("fluentPreserveParentSurface", visible)
        viewport = self._tree.viewport()
        viewport.setProperty("fluentPreserveParentSurface", visible)
        viewport.setAttribute(Qt.WA_NoSystemBackground, visible)
        viewport.update()
        self._update_divider_palette()
        self.update()

    def _set_settings_icon_rotation(self, value: object) -> None:
        self._settings_icon_rotation = float(value) % 360.0
        self._tree.setProperty(
            "gallerySettingsIconRotation", self._settings_icon_rotation
        )
        self._tree.viewport().setProperty(
            "gallerySettingsIconRotation", self._settings_icon_rotation
        )
        self._tree.viewport().update()

    def _start_settings_icon_rotation(self) -> None:
        if not self.isVisible() or not self.window().isVisible():
            self._set_settings_icon_rotation(0.0)
            return
        self._settings_rotation_animation.stop()
        self._settings_rotation_animation.setDuration(400)
        self._settings_rotation_animation.setEasingCurve(
            QEasingCurve.OutCubic
        )
        self._settings_rotation_animation.setStartValue(
            self._settings_icon_rotation
        )
        self._settings_rotation_animation.setEndValue(
            self._settings_icon_rotation + 359.99
        )
        self._settings_rotation_animation.start()

    def sync_selected(self, route_id: str) -> None:
        self._selected_route_id = route_id
        if route_id == "settings":
            self._tree.setSelectedItem(self._item.index())
        else:
            self._tree.clearSelection()
            self._tree.setCurrentIndex(QModelIndex())

    def set_compact(self, compact: bool) -> None:
        compact = bool(compact)
        _hide_compact_navigation_tooltip(self)
        if self._compact == compact:
            self._sync_compact_visual_properties()
            return
        self._compact = compact
        self._sync_compact_visual_properties()
        self._start_compact_visual_transition()

    def _sync_compact_visual_properties(self) -> None:
        for widget in (self._tree, self._tree.viewport()):
            widget.setProperty("galleryCompact", self._compact)
            widget.setProperty(
                "galleryCompactVisualProgress",
                self._compact_visual_progress,
            )

    def _set_compact_visual_progress(self, value: object) -> None:
        progress = max(0.0, min(1.0, float(value)))
        if abs(self._compact_visual_progress - progress) <= 0.0001:
            return
        self._compact_visual_progress = progress
        self._sync_compact_visual_properties()
        self._tree.doItemsLayout()
        self._tree.viewport().update()

    def _start_compact_visual_transition(self) -> None:
        end_value = 1.0 if self._compact else 0.0
        if abs(self._compact_visual_progress - end_value) <= 0.0001:
            self._set_compact_visual_progress(end_value)
            return
        self._compact_visual_animation.stop()
        if not self.isVisible() or not self.window().isVisible():
            self._compact_visual_progress = end_value
            self._sync_compact_visual_properties()
            self._tree.doItemsLayout()
            self._tree.viewport().update()
            return
        self._compact_visual_animation.setDuration(250)
        self._compact_visual_animation.setEasingCurve(QEasingCurve.OutCubic)
        self._compact_visual_animation.setStartValue(
            self._compact_visual_progress
        )
        self._compact_visual_animation.setEndValue(end_value)
        self._compact_visual_animation.start()

    def _finish_compact_visual_transition(self) -> None:
        self._set_compact_visual_progress(1.0 if self._compact else 0.0)

    def _update_divider_palette(self) -> None:
        from .foundation_pages import _theme_tokens

        color = QColor(_theme_tokens()["strokeDivider"])
        color.setAlphaF(color.alphaF() * 0.4)
        self._divider.setColor(color)

    def refresh_theme(self) -> None:
        _NAV_ICON_CACHE.clear()
        self._update_divider_palette()
        self._tree.viewport().update()


class _GalleryCompactFlyoutRow(QWidget):
    """Self-painted child row used by the native top-navigation flyout."""

    activated = Signal(str)

    def __init__(
        self,
        route_id: str,
        text: str,
        selected: bool,
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self.setObjectName(
            "galleryCompactNavigationFlyoutRow_{0}".format(route_id)
        )
        self._route_id = route_id
        self._text = text
        self._selected = bool(selected)
        self._hovered = False
        self._pressed = False
        self.setMouseTracking(True)
        self.setFocusPolicy(Qt.NoFocus)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        self.setFixedHeight(36)

    def sizeHint(self) -> QSize:
        font = fluentqt.font_for_role(fluentqt.FontRole.Body)
        return QSize(max(160, QFontMetrics(font).horizontalAdvance(self._text) + 28), 36)

    def paintEvent(self, event) -> None:
        del event
        from .foundation_pages import _theme_tokens

        tokens = _theme_tokens()
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)
        background = QColor(Qt.transparent)
        if self._pressed:
            background = QColor(tokens["subtleTertiary"])
        elif self._selected or self._hovered:
            background = QColor(tokens["subtleSecondary"])
        if background.alpha() > 0:
            painter.setPen(Qt.NoPen)
            painter.setBrush(background)
            from .foundation_pages import _radii

            control_radius, _overlay_radius = _radii()
            painter.drawRoundedRect(
                QRectF(self.rect().adjusted(4, 2, -4, -2)),
                control_radius,
                control_radius,
            )
        font = fluentqt.font_for_role(fluentqt.FontRole.Body)
        font.setPixelSize(14)
        painter.setFont(font)
        painter.setPen(QColor(tokens["textPrimary"]))
        text_rect = QRect(14, 0, max(0, self.width() - 24), self.height())
        painter.drawText(
            text_rect,
            Qt.AlignLeft | Qt.AlignVCenter | Qt.TextSingleLine,
            painter.fontMetrics().elidedText(
                self._text, Qt.ElideRight, text_rect.width()
            ),
        )

    def enterEvent(self, event) -> None:
        super().enterEvent(event)
        self._hovered = True
        self.update()

    def leaveEvent(self, event) -> None:
        super().leaveEvent(event)
        self._hovered = False
        self._pressed = False
        self.update()

    def mousePressEvent(self, event) -> None:
        if event.button() == Qt.LeftButton:
            self._pressed = True
            self.update()
            event.accept()
            return
        super().mousePressEvent(event)

    def mouseReleaseEvent(self, event) -> None:
        activate = (
            self._pressed
            and event.button() == Qt.LeftButton
            and self.rect().contains(event.position().toPoint())
        )
        self._pressed = False
        self.update()
        if activate:
            self.activated.emit(self._route_id)
            event.accept()
            return
        super().mouseReleaseEvent(event)


class GalleryTopNavigationPane(QWidget):
    """Icon-only 48 px top navigation chrome matching the C++ Gallery."""

    routeActivated = Signal(str)

    def __init__(
        self,
        items: Iterable[
            tuple[
                str,
                str,
                str,
                tuple[tuple[str, str], ...],
            ]
        ],
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        self.setMinimumHeight(48)
        layout = QHBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(4)
        self._buttons: dict[str, fluentqt.Button] = {}
        self._children: dict[str, tuple[tuple[str, str], ...]] = {}
        self._parents: dict[str, str] = {}
        self._selected_route_id = ""
        self._child_flyout: fluentqt.Popup | None = None
        self._child_flyout_panel: QWidget | None = None

        for route_id, title, icon_name, children in items:
            button = fluentqt.Button("", self)
            button.setObjectName(
                "galleryTopNavigationButton_{0}".format(route_id)
            )
            button.setAccessibleName(title)
            fluentqt.ToolTip.attach(
                button, title, fluentqt.ToolTip.Placement.Above
            )
            button.setFluentLayout(fluentqt.Button.ButtonLayout.IconOnly)
            button.setFluentSize(fluentqt.Button.ButtonSize.Small)
            button.setFluentStyle(fluentqt.Button.ButtonStyle.Subtle)
            button.setIconGlyph(icon_name, 16)
            button.setFixedSize(32, 32)
            button.clicked.connect(
                lambda _checked=False, rid=route_id, source=button: (
                    self._activate_route(rid, source)
                )
            )
            self._buttons[route_id] = button
            self._children[route_id] = tuple(children)
            for child_id, _child_title in children:
                self._parents[child_id] = route_id
            layout.addWidget(button)

    def sizeHint(self) -> QSize:
        count = len(self._buttons)
        return QSize(16 + count * 32 + max(0, count - 1) * 4, 48)

    def sync_selected(self, route_id: str) -> None:
        if self._selected_route_id == route_id:
            return
        self._close_child_flyout(False)
        self._selected_route_id = route_id
        visual_route = route_id
        while visual_route not in self._buttons and visual_route in self._parents:
            visual_route = self._parents[visual_route]
        for button_route, button in self._buttons.items():
            button.setFluentStyle(
                fluentqt.Button.ButtonStyle.Standard
                if button_route == visual_route
                else fluentqt.Button.ButtonStyle.Subtle
            )

    def _activate_route(self, route_id: str, button: fluentqt.Button) -> None:
        if route_id == "settings":
            self._start_settings_icon_rotation(button)
        self.sync_selected(route_id)
        self.routeActivated.emit(route_id)
        if self._children.get(route_id):
            self._show_child_flyout(route_id, button)
        else:
            self._close_child_flyout()

    def _show_child_flyout(
        self, route_id: str, anchor: fluentqt.Button
    ) -> None:
        children = self._children.get(route_id, ())
        host = self.window()
        if not children or host is None:
            return
        self._close_child_flyout(False)
        popup = fluentqt.Popup(self)
        popup.setObjectName("galleryTopNavigationFlyout")
        popup.setAnimationEnabled(True)
        popup.setExitAnimationEnabled(False)
        popup.setClosePolicy(
            fluentqt.Popup.ClosePolicy(
                fluentqt.Popup.CloseFlag.CloseOnPressOutside
                | fluentqt.Popup.CloseFlag.CloseOnEscape
            )
        )
        popup.setLightDismissConsumesPress(True)

        panel = QWidget(popup)
        panel.setObjectName("galleryTopNavigationFlyoutPanel")
        panel.setAttribute(Qt.WA_NoSystemBackground)
        panel_layout = QVBoxLayout(panel)
        panel_layout.setContentsMargins(0, 0, 0, 0)
        panel_layout.setSpacing(2)
        rows = []
        for child_id, child_title in children:
            row = _GalleryCompactFlyoutRow(
                child_id,
                child_title,
                child_id == self._selected_route_id,
                panel,
            )
            row.activated.connect(self._activate_child_route)
            panel_layout.addWidget(row)
            rows.append(row)

        content_width = max(row.sizeHint().width() for row in rows)
        content_height = len(rows) * 36 + max(0, len(rows) - 1) * 2
        card_size = QSize(content_width + 6, content_height + 8)
        popup.resize(card_size.width() + 16, card_size.height() + 16)
        panel.setGeometry(11, 12, content_width, content_height)
        panel.show()

        anchor_top_left = anchor.mapTo(host, QPoint(0, 0))
        card_top_left = QPoint(
            anchor_top_left.x(),
            anchor_top_left.y() + anchor.height() + 8,
        )
        min_x = 12
        min_y = 12
        max_x = max(min_x, host.width() - card_size.width() - 12)
        max_y = max(min_y, host.height() - card_size.height() - 12)
        card_top_left.setX(max(min_x, min(card_top_left.x(), max_x)))
        card_top_left.setY(max(min_y, min(card_top_left.y(), max_y)))
        popup.setPosition(host, card_top_left)

        self._child_flyout = popup
        self._child_flyout_panel = panel

        def clear_popup(*_unused: object) -> None:
            if self._child_flyout is popup:
                self._child_flyout = None
                self._child_flyout_panel = None

        popup.destroyed.connect(clear_popup)
        popup.open()
        end_position = popup.pos()
        entrance = QPropertyAnimation(popup, b"pos", popup)
        entrance.setObjectName(
            "galleryTopNavigationFlyoutEntranceAnimation"
        )
        entrance.setDuration(100)
        entrance.setEasingCurve(QEasingCurve.OutCubic)
        entrance.setStartValue(end_position - QPoint(0, 8))
        entrance.setEndValue(end_position)
        popup.move(entrance.startValue())
        entrance.start(QPropertyAnimation.DeleteWhenStopped)

    def _activate_child_route(self, route_id: str) -> None:
        self._close_child_flyout(False)
        self.sync_selected(route_id)
        self.routeActivated.emit(route_id)

    def _close_child_flyout(self, animated: bool = True) -> None:
        popup = self._child_flyout
        if popup is None:
            return
        self._child_flyout = None
        self._child_flyout_panel = None
        if animated and popup.isVisible():
            popup.closed.connect(popup.deleteLater)
            popup.close()
        else:
            popup.hide()
            popup.deleteLater()

    @staticmethod
    def _start_settings_icon_rotation(button: fluentqt.Button) -> None:
        animation = getattr(button, "_gallery_settings_rotation", None)
        if animation is None:
            animation = QPropertyAnimation(button, b"iconRotation", button)
            animation.setObjectName(
                "galleryTopSettingsIconRotationAnimation"
            )
            animation.finished.connect(lambda: button.setIconRotation(0.0))
            button._gallery_settings_rotation = animation
        animation.stop()
        start = float(button.iconRotation())
        animation.setDuration(400)
        animation.setEasingCurve(QEasingCurve.OutCubic)
        animation.setStartValue(start)
        animation.setEndValue(start + 359.99)
        animation.start()


def refresh_gallery_visuals(
    root: QWidget, *, visible_only: bool = False
) -> None:
    """Refresh app-owned palette helpers after a Fluent theme switch."""

    seen: set[int] = set()
    widgets = [root] + root.findChildren(QWidget)
    for widget in widgets:
        if id(widget) in seen:
            continue
        seen.add(id(widget))
        if visible_only and widget is not root and not widget.isVisibleTo(root):
            continue
        refresh = getattr(widget, "refresh_theme", None)
        if callable(refresh):
            refresh()


def refresh_gallery_display_scale(
    root: QWidget, *, visible_only: bool = True
) -> None:
    """Rebuild app-owned raster assets after a screen/DPR transition."""

    seen: set[int] = set()
    widgets = [root] + root.findChildren(QWidget)
    for widget in widgets:
        if id(widget) in seen:
            continue
        seen.add(id(widget))
        if visible_only and widget is not root and not widget.isVisibleTo(root):
            continue
        refresh = getattr(widget, "refresh_display_scale", None)
        if callable(refresh):
            refresh()


__all__ = [
    "GalleryCodeBlock",
    "GalleryEntryCard",
    "GalleryEntryGrid",
    "GalleryHomeHero",
    "GalleryNavigationFooter",
    "GalleryNavigationPane",
    "GalleryPageSkeleton",
    "GalleryTopNavigationPane",
    "GalleryReferenceCard",
    "GallerySplashScreen",
    "app_icon",
    "app_icon_pixmap",
    "asset_path",
    "control_image_path",
    "refresh_gallery_display_scale",
    "refresh_gallery_visuals",
    "route_icon_name",
]
