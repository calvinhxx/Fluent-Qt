"""Pixel-shaped Foundation topic pages shared with the native Gallery design."""

from __future__ import annotations

from dataclasses import dataclass
import json
import re
import sys
from typing import Callable, Iterable

import fluentqt
import shiboken6
from PySide6.QtCore import (
    QCoreApplication,
    QEvent,
    QMargins,
    QPoint,
    QRect,
    QRectF,
    QSize,
    Qt,
    QTimer,
)
from PySide6.QtGui import QColor, QFont, QFontMetrics, QHelpEvent, QPainter, QPen
from PySide6.QtWidgets import (
    QAbstractScrollArea,
    QApplication,
    QHBoxLayout,
    QSizePolicy,
    QVBoxLayout,
    QWidget,
)

from .visual import (
    GalleryCodeBlock,
    _font_style_strategy,
    asset_path,
    css_color,
    gallery_colors,
)


_CARD_PADDING = 20
_GRID_SPACING = 12


def _show_gallery_toast(anchor: QWidget, message: str) -> None:
    toast = fluentqt.Toast.showToast(
        anchor,
        message,
        fluentqt.Toast.Severity.Success,
        1700,
        fluentqt.Toast.Placement.Top,
        QMargins(16, 50, 16, 16),
    )
    if toast is None:
        return
    toast.setObjectName("galleryToast")
    card = toast.findChild(QWidget, "fluentToastCard")
    if card is not None:
        card.setObjectName("galleryToastCard")
    icon = toast.findChild(QWidget, "fluentToastIcon")
    if icon is not None:
        icon.setObjectName("galleryToastIcon")


def _rgba(red: int, green: int, blue: int, alpha: int = 255) -> QColor:
    return QColor(red, green, blue, alpha)


def _parse_rgba(value: str) -> QColor:
    """Parse the StyleThemeCatalog #RRGGBB/#RRGGBBAA convention."""

    value = value.lstrip("#")
    if len(value) == 8:
        return QColor(
            int(value[0:2], 16),
            int(value[2:4], 16),
            int(value[4:6], 16),
            int(value[6:8], 16),
        )
    return QColor("#" + value)


def _theme_tokens() -> dict[str, QColor]:
    """Return the resolved built-in token palette used by the C++ topic page."""

    dark = fluentqt.current_theme() == fluentqt.Theme.Dark
    if dark:
        values: dict[str, QColor] = {
            "accentDefault": QColor("#60CDFF"),
            "accentSecondary": _rgba(96, 205, 255, 230),
            "accentTertiary": _rgba(96, 205, 255, 204),
            "accentDisabled": _rgba(255, 255, 255, 37),
            "controlDefault": _rgba(255, 255, 255, 15),
            "controlSecondary": _rgba(255, 255, 255, 23),
            "controlTertiary": _rgba(255, 255, 255, 10),
            "controlDisabled": _rgba(255, 255, 255, 6),
            "controlAltSecondary": _rgba(255, 255, 255, 15),
            "controlAltTertiary": _rgba(255, 255, 255, 20),
            "subtleTransparent": _rgba(255, 255, 255, 0),
            "subtleSecondary": _rgba(255, 255, 255, 15),
            "subtleTertiary": _rgba(255, 255, 255, 10),
            "strokeDefault": _rgba(255, 255, 255, 17),
            "strokeSecondary": _rgba(255, 255, 255, 23),
            "strokeStrong": _rgba(255, 255, 255, 138),
            "strokeCard": _rgba(255, 255, 255, 10),
            "strokeDivider": _rgba(255, 255, 255, 20),
            "strokeSurface": _rgba(255, 255, 255, 30),
            "strokeFocusOuter": _rgba(255, 255, 255, 230),
            "strokeFocusInner": _rgba(0, 0, 0, 230),
            "textPrimary": QColor("#FFFFFF"),
            "textSecondary": _rgba(255, 255, 255, 199),
            "textTertiary": _rgba(255, 255, 255, 138),
            "textDisabled": _rgba(255, 255, 255, 92),
            "textOnAccent": QColor("#000000"),
            "textAccentPrimary": QColor("#99EBFF"),
            "bgCanvas": QColor("#202020"),
            "bgLayer": QColor("#2C2C2C"),
            "bgLayerAlt": QColor("#3D3D3D"),
            "bgSolid": QColor("#1C1C1C"),
            "grey10": QColor("#FAF9F8"),
            "grey20": QColor("#F3F2F1"),
            "grey30": QColor("#EDEBE9"),
            "grey40": QColor("#E1DFDD"),
            "grey50": QColor("#D2D0CE"),
            "grey60": QColor("#C8C6C4"),
            "grey90": QColor("#A19F9D"),
            "grey130": QColor("#605E5C"),
            "grey160": QColor("#323130"),
            "grey190": QColor("#201F1E"),
            "systemCritical": QColor("#FF99A4"),
            "systemCriticalBg": QColor("#442726"),
            "systemCaution": QColor("#FCE100"),
            "systemCautionBg": QColor("#433519"),
            "systemInfo": QColor("#60CDFF"),
            "systemInfoBg": QColor("#1F3150"),
            "systemSuccess": QColor("#6CCB5F"),
            "systemSuccessBg": QColor("#1E3C1F"),
        }
        charts = (
            "#60CDFF", "#00BCF2", "#2B88D8", "#00AD56",
            "#107C10", "#004B50", "#FF8C00", "#F7630C",
            "#EA4300", "#E3008C", "#BF0077", "#C239B3",
        )
    else:
        values = {
            "accentDefault": QColor("#005FB8"),
            "accentSecondary": _rgba(0, 95, 184, 230),
            "accentTertiary": _rgba(0, 95, 184, 204),
            "accentDisabled": _rgba(0, 0, 0, 37),
            "controlDefault": QColor("#FFFFFF"),
            "controlSecondary": _rgba(249, 249, 249, 242),
            "controlTertiary": _rgba(249, 249, 249, 204),
            "controlDisabled": _rgba(249, 249, 249, 77),
            "controlAltSecondary": _rgba(0, 0, 0, 5),
            "controlAltTertiary": _rgba(0, 0, 0, 15),
            "subtleTransparent": _rgba(0, 0, 0, 0),
            "subtleSecondary": _rgba(0, 0, 0, 9),
            "subtleTertiary": _rgba(0, 0, 0, 6),
            "strokeDefault": _rgba(0, 0, 0, 12),
            "strokeSecondary": _rgba(0, 0, 0, 41),
            "strokeStrong": _rgba(0, 0, 0, 112),
            "strokeCard": _rgba(0, 0, 0, 13),
            "strokeDivider": _rgba(0, 0, 0, 20),
            "strokeSurface": QColor("#757575"),
            "strokeFocusOuter": _rgba(0, 0, 0, 230),
            "strokeFocusInner": QColor("#FFFFFF"),
            "textPrimary": _rgba(0, 0, 0, 230),
            "textSecondary": _rgba(0, 0, 0, 154),
            "textTertiary": _rgba(0, 0, 0, 112),
            "textDisabled": _rgba(0, 0, 0, 92),
            "textOnAccent": QColor("#FFFFFF"),
            "textAccentPrimary": QColor("#003E92"),
            "bgCanvas": QColor("#F3F3F3"),
            "bgLayer": QColor("#FFFFFF"),
            "bgLayerAlt": QColor("#F9F9F9"),
            "bgSolid": QColor("#EEEEEE"),
            "grey10": QColor("#FAF9F8"),
            "grey20": QColor("#F3F2F1"),
            "grey30": QColor("#EDEBE9"),
            "grey40": QColor("#E1DFDD"),
            "grey50": QColor("#D2D0CE"),
            "grey60": QColor("#C8C6C4"),
            "grey90": QColor("#A19F9D"),
            "grey130": QColor("#605E5C"),
            "grey160": QColor("#323130"),
            "grey190": QColor("#201F1E"),
            "systemCritical": QColor("#C42B1C"),
            "systemCriticalBg": QColor("#FDE7E9"),
            "systemCaution": QColor("#9D5D00"),
            "systemCautionBg": QColor("#FFF4CE"),
            "systemInfo": QColor("#015CDA"),
            "systemInfoBg": QColor("#F6F6F6"),
            "systemSuccess": QColor("#0F7B0F"),
            "systemSuccessBg": QColor("#DFF6DD"),
        }
        charts = (
            "#005FB8", "#00BCF2", "#2B88D8", "#004B50",
            "#00AD56", "#007833", "#881798", "#B4009E",
            "#E3008C", "#D83B01", "#EA4300", "#FF8C00",
        )

    language = str(fluentqt.current_design_language())
    if "Material" in language:
        overrides = (
            {
                "accentDefault": "#D0BCFF", "textOnAccent": "#381E72",
                "bgCanvas": "#141218", "bgLayer": "#1D1B20",
                "bgLayerAlt": "#211F26", "bgSolid": "#0F0D13",
                "textPrimary": "#E6E0E9", "textSecondary": "#CAC4D0",
                "textTertiary": "#938F99", "systemCritical": "#F2B8B5",
                "systemCriticalBg": "#8C1D18", "systemCaution": "#F5C518",
                "systemCautionBg": "#4A3C00", "systemInfo": "#D0BCFF",
                "systemInfoBg": "#4F378B", "systemSuccess": "#6DD58C",
                "systemSuccessBg": "#0A3818",
            }
            if dark
            else {
                "accentDefault": "#6750A4", "textOnAccent": "#FFFFFF",
                "bgCanvas": "#FEF7FF", "bgLayer": "#FFFFFF",
                "bgLayerAlt": "#F7F2FA", "bgSolid": "#F3EDF7",
                "textPrimary": "#1D1B20", "textSecondary": "#49454F",
                "textTertiary": "#79747E", "systemCritical": "#B3261E",
                "systemCriticalBg": "#F9DEDC", "systemCaution": "#7A5900",
                "systemCautionBg": "#FCEFC7", "systemInfo": "#6750A4",
                "systemInfoBg": "#EADDFF", "systemSuccess": "#146C2E",
                "systemSuccessBg": "#C4EED0",
            }
        )
        values.update({key: _parse_rgba(value) for key, value in overrides.items()})
    elif "Cupertino" in language:
        overrides = (
            {
                "accentDefault": "#0A84FF", "textOnAccent": "#FFFFFF",
                "bgCanvas": "#1E1E1E", "bgLayer": "#2C2C2E",
                "bgLayerAlt": "#3A3A3C", "bgSolid": "#161618",
                "textPrimary": "#FFFFFF", "textSecondary": "#FFFFFF8C",
                "textTertiary": "#FFFFFF40", "systemCritical": "#FF453A",
                "systemCriticalBg": "#3A1F1E", "systemCaution": "#FF9F0A",
                "systemCautionBg": "#3A2C12", "systemInfo": "#0A84FF",
                "systemInfoBg": "#16263A", "systemSuccess": "#30D158",
                "systemSuccessBg": "#15321E",
            }
            if dark
            else {
                "accentDefault": "#007AFF", "textOnAccent": "#FFFFFF",
                "bgCanvas": "#ECECEC", "bgLayer": "#FFFFFF",
                "bgLayerAlt": "#F5F5F7", "bgSolid": "#E3E3E3",
                "textPrimary": "#000000D9", "textSecondary": "#0000007F",
                "textTertiary": "#00000042", "systemCritical": "#FF3B30",
                "systemCriticalBg": "#FFE5E3", "systemCaution": "#FF9500",
                "systemCautionBg": "#FFF0DB", "systemInfo": "#007AFF",
                "systemInfoBg": "#E3F0FF", "systemSuccess": "#34C759",
                "systemSuccessBg": "#E3F9E5",
            }
        )
        values.update({key: _parse_rgba(value) for key, value in overrides.items()})

    active_accent = QColor(fluentqt.accent_color())
    if active_accent.isValid():
        values["accentDefault"] = active_accent
        secondary = QColor(active_accent)
        secondary.setAlpha(230)
        tertiary = QColor(active_accent)
        tertiary.setAlpha(204)
        values["accentSecondary"] = secondary
        values["accentTertiary"] = tertiary
    for index, color in enumerate(charts, 1):
        values["chart{0}".format(index)] = QColor(color)
    return values


def _radii() -> tuple[int, int]:
    language = str(fluentqt.current_design_language())
    if "Material" in language:
        return 8, 12
    if "Cupertino" in language:
        return 6, 10
    return 4, 8


def _paint_surface(painter: QPainter, rect: QRectF) -> None:
    colors = _theme_tokens()
    _control, overlay = _radii()
    painter.setPen(QPen(colors["strokeCard"], 1.0))
    painter.setBrush(colors["bgLayer"])
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), overlay, overlay)


def _section_heading(text: str, parent: QWidget) -> fluentqt.Label:
    label = fluentqt.Label(text, parent)
    label.setObjectName("galleryContentSectionHeader")
    label.setFluentTypography(fluentqt.FontRole.Subtitle)
    label.setWordWrap(True)
    label.setStyleSheet(
        "color: {0}; background: transparent;".format(
            css_color(gallery_colors().text_primary)
        )
    )
    return label


def _add_section_heading(
    layout: QVBoxLayout,
    text: str,
    parent: QWidget,
) -> fluentqt.Label:
    layout.addSpacing(8)
    label = _section_heading(text, parent)
    layout.addWidget(label)
    return label


def _secondary_caption(text: str, parent: QWidget) -> fluentqt.Label:
    label = fluentqt.Label(text, parent)
    label.setFluentTypography(fluentqt.FontRole.Caption)
    label.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
    label.setWordWrap(True)
    return label


class _SurfaceCard(QWidget):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)

    def paintEvent(self, event) -> None:
        del event
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        _paint_surface(painter, QRectF(self.rect()))


class TypographyRampCard(_SurfaceCard):
    ROWS = (
        (fluentqt.FontRole.Display, "Display", 68, 92, "SemiBold"),
        (fluentqt.FontRole.TitleLarge, "Title Large", 40, 52, "SemiBold"),
        (fluentqt.FontRole.Title, "Title", 28, 36, "SemiBold"),
        (fluentqt.FontRole.Subtitle, "Subtitle", 20, 28, "SemiBold"),
        (fluentqt.FontRole.BodyLargeStrong, "Body Large Strong", 18, 24, "SemiBold"),
        (fluentqt.FontRole.BodyLarge, "Body Large", 18, 24, "Regular"),
        (fluentqt.FontRole.BodyStrong, "Body Strong", 14, 20, "SemiBold"),
        (fluentqt.FontRole.Body, "Body", 14, 20, "Regular"),
        (fluentqt.FontRole.Caption, "Caption", 12, 16, "Regular"),
    )

    def _row_height(self, role: object, line_height: int) -> int:
        return max(line_height, QFontMetrics(fluentqt.font_for_role(role)).height())

    def _total_height(self) -> int:
        return _CARD_PADDING * 2 + sum(
            self._row_height(role, line_height)
            for role, _name, _size, line_height, _weight in self.ROWS
        ) + _GRID_SPACING * (len(self.ROWS) - 1)

    def sizeHint(self) -> QSize:
        return QSize(480, self._total_height())

    def minimumSizeHint(self) -> QSize:
        return QSize(0, self._total_height())

    def paintEvent(self, event) -> None:
        super().paintEvent(event)
        colors = _theme_tokens()
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)
        caption_font = fluentqt.font_for_role(fluentqt.FontRole.Caption)
        left = _CARD_PADDING
        right = self.width() - _CARD_PADDING
        metrics_width = 220
        y = _CARD_PADDING
        for index, (role, name, size, line_height, weight) in enumerate(self.ROWS):
            role_font = fluentqt.font_for_role(role)
            height = self._row_height(role, line_height)
            painter.setFont(role_font)
            painter.setPen(colors["textPrimary"])
            painter.drawText(
                QRect(left, y, right - left - metrics_width, height),
                Qt.AlignLeft | Qt.AlignVCenter,
                name,
            )
            painter.setFont(caption_font)
            painter.setPen(colors["textSecondary"])
            painter.drawText(
                QRect(right - metrics_width, y, metrics_width, height),
                Qt.AlignRight | Qt.AlignVCenter,
                "{0} / {1} · {2}".format(size, line_height, weight),
            )
            y += height
            if index != len(self.ROWS) - 1:
                painter.setPen(QPen(colors["strokeDivider"], 1.0))
                painter.drawLine(left, y + _GRID_SPACING // 2, right, y + _GRID_SPACING // 2)
                y += _GRID_SPACING


class FoundationTileGrid(QWidget):
    def __init__(self, token_names: Iterable[str], parent: QWidget) -> None:
        super().__init__(parent)
        self._tokens = tuple(token_names)
        self._hovered = -1
        self._last_columns = 0
        self.setObjectName("galleryFoundationSwatchGrid")
        self.setMouseTracking(True)
        self.setCursor(Qt.PointingHandCursor)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)

    def _columns(self, width: int | None = None) -> int:
        width = self.width() if width is None else width
        return max(1, min(4, (max(1, width) + _GRID_SPACING) // (200 + _GRID_SPACING)))

    def _grid_height(self, width: int | None = None) -> int:
        columns = self._columns(width)
        rows = (len(self._tokens) + columns - 1) // columns
        return rows * 112 + max(0, rows - 1) * _GRID_SPACING

    def hasHeightForWidth(self) -> bool:
        return True

    def heightForWidth(self, width: int) -> int:
        return self._grid_height(width)

    def sizeHint(self) -> QSize:
        width = self.width() if self.width() >= 200 else 480
        return QSize(width, self._grid_height(width))

    def minimumSizeHint(self) -> QSize:
        return QSize(0, self._grid_height(max(1, self.width())))

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        columns = self._columns()
        if columns != self._last_columns:
            self._last_columns = columns
            height = self._grid_height()
            self.setMinimumHeight(height)
            self.setMaximumHeight(height)
            self.updateGeometry()
        self.update()

    def _cell_rect(self, index: int) -> QRect:
        columns = self._columns()
        width = max(0, (self.width() - (columns - 1) * _GRID_SPACING) // columns)
        return QRect(
            (index % columns) * (width + _GRID_SPACING),
            (index // columns) * (112 + _GRID_SPACING),
            width,
            112,
        )

    def _index_at(self, point: QPoint) -> int:
        for index in range(len(self._tokens)):
            if self._cell_rect(index).contains(point):
                return index
        return -1

    def mouseMoveEvent(self, event) -> None:
        hovered = self._index_at(event.position().toPoint())
        if hovered != self._hovered:
            previous = self._hovered
            self._hovered = hovered
            if previous >= 0:
                self.update(self._cell_rect(previous))
            if hovered >= 0:
                self.update(self._cell_rect(hovered))
        super().mouseMoveEvent(event)

    def leaveEvent(self, event) -> None:
        previous = self._hovered
        self._hovered = -1
        if previous >= 0:
            self.update(self._cell_rect(previous))
        super().leaveEvent(event)

    @staticmethod
    def _hex(color: QColor) -> str:
        mode = QColor.HexRgb if color.alpha() == 255 else QColor.HexArgb
        return color.name(mode).upper()

    def mouseReleaseEvent(self, event) -> None:
        if event.button() == Qt.LeftButton:
            index = self._index_at(event.position().toPoint())
            if index >= 0:
                value = self._hex(_theme_tokens()[self._tokens[index]])
                QApplication.clipboard().setText(value)
                try:
                    _show_gallery_toast(self, "Copied " + value)
                except (RuntimeError, ValueError):
                    pass
        super().mouseReleaseEvent(event)

    def paintEvent(self, event) -> None:
        del event
        colors = _theme_tokens()
        _control, overlay = _radii()
        title_font = fluentqt.font_for_role(fluentqt.FontRole.BodyStrong)
        caption_font = fluentqt.font_for_role(fluentqt.FontRole.Caption)
        title_metrics = QFontMetrics(title_font)
        caption_metrics = QFontMetrics(caption_font)
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)
        for index, name in enumerate(self._tokens):
            cell = self._cell_rect(index)
            painter.setPen(QPen(colors["strokeCard"], 1.0))
            painter.setBrush(
                colors["subtleSecondary"] if index == self._hovered else colors["bgLayer"]
            )
            painter.drawRoundedRect(
                QRectF(cell).adjusted(0.5, 0.5, -0.5, -0.5), overlay, overlay
            )
            pad = 12
            chip = QRect(cell.left() + pad, cell.top() + pad, cell.width() - pad * 2, 40)
            painter.setPen(QPen(colors["strokeSurface"], 1.0))
            painter.setBrush(colors[name])
            control, _overlay = _radii()
            painter.drawRoundedRect(
                QRectF(chip).adjusted(0.5, 0.5, -0.5, -0.5), control, control
            )
            text_left = cell.left() + pad
            text_width = cell.width() - pad * 2
            text_y = chip.bottom() + 8
            painter.setFont(title_font)
            painter.setPen(colors["textPrimary"])
            painter.drawText(
                QRect(text_left, text_y, text_width, title_metrics.height()),
                Qt.AlignLeft | Qt.AlignVCenter,
                title_metrics.elidedText(name, Qt.ElideRight, text_width),
            )
            text_y += title_metrics.height() + 2
            painter.setFont(caption_font)
            painter.setPen(colors["textSecondary"])
            painter.drawText(
                QRect(text_left, text_y, text_width, caption_metrics.height()),
                Qt.AlignLeft | Qt.AlignVCenter,
                self._hex(colors[name]),
            )


class RadiusCard(_SurfaceCard):
    ITEMS = (("None", 0), ("Control", 4), ("Overlay", 8))
    HEIGHT = 88 + 12 + 20 + 4 + 16 + _CARD_PADDING * 2

    def sizeHint(self) -> QSize:
        return QSize(480, self.HEIGHT)

    def minimumSizeHint(self) -> QSize:
        return QSize(0, self.HEIGHT)

    def paintEvent(self, event) -> None:
        super().paintEvent(event)
        colors = _theme_tokens()
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)
        column_width = (self.width() - _CARD_PADDING * 2) // len(self.ITEMS)
        title_font = fluentqt.font_for_role(fluentqt.FontRole.BodyStrong)
        caption_font = fluentqt.font_for_role(fluentqt.FontRole.Caption)
        for index, (name, radius) in enumerate(self.ITEMS):
            center_x = _CARD_PADDING + index * column_width + column_width // 2
            tile = QRect(center_x - 44, _CARD_PADDING, 88, 88)
            painter.setPen(QPen(colors["strokeStrong"], 1.0))
            painter.setBrush(colors["subtleSecondary"])
            painter.drawRoundedRect(
                QRectF(tile).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius
            )
            painter.setFont(title_font)
            painter.setPen(colors["textPrimary"])
            painter.drawText(
                QRect(_CARD_PADDING + index * column_width, tile.bottom() + 12, column_width, 20),
                Qt.AlignHCenter,
                name,
            )
            painter.setFont(caption_font)
            painter.setPen(colors["textSecondary"])
            painter.drawText(
                QRect(_CARD_PADDING + index * column_width, tile.bottom() + 34, column_width, 16),
                Qt.AlignHCenter,
                "{0}px".format(radius),
            )


@dataclass(frozen=True)
class MeasureRow:
    name: str
    qualified_name: str
    value: int
    usage: str


class TokenMeasureCard(_SurfaceCard):
    ROW_HEIGHT = 48
    VERTICAL_PADDING = 12

    def __init__(self, rows: Iterable[MeasureRow], parent: QWidget) -> None:
        super().__init__(parent)
        self.rows = tuple(rows)

    def _height(self) -> int:
        return len(self.rows) * self.ROW_HEIGHT + self.VERTICAL_PADDING * 2

    def sizeHint(self) -> QSize:
        return QSize(560, self._height())

    def minimumSizeHint(self) -> QSize:
        return QSize(0, self._height())

    def paintEvent(self, event) -> None:
        super().paintEvent(event)
        colors = _theme_tokens()
        control, _overlay = _radii()
        body_font = fluentqt.font_for_role(fluentqt.FontRole.BodyStrong)
        caption_font = fluentqt.font_for_role(fluentqt.FontRole.Caption)
        caption_metrics = QFontMetrics(caption_font)
        name_column = min(230, max(140, self.width() // 3))
        value_column = 56
        bar_left = _CARD_PADDING + name_column
        usage_left = bar_left + 112
        usage_width = max(0, self.width() - usage_left - _CARD_PADDING)
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)
        y = self.VERTICAL_PADDING
        for index, row in enumerate(self.rows):
            painter.setFont(body_font)
            painter.setPen(colors["textPrimary"])
            painter.drawText(
                QRect(_CARD_PADDING, y + 3, name_column - 8, 20),
                Qt.AlignLeft | Qt.AlignVCenter,
                row.name,
            )
            painter.setFont(caption_font)
            painter.setPen(colors["textSecondary"])
            painter.drawText(
                QRect(_CARD_PADDING, y + 23, name_column - 8, 18),
                Qt.AlignLeft | Qt.AlignVCenter,
                row.qualified_name,
            )
            painter.drawText(
                QRect(bar_left, y + 3, value_column, 20),
                Qt.AlignLeft | Qt.AlignVCenter,
                "{0} px".format(row.value),
            )
            painter.setPen(Qt.NoPen)
            painter.setBrush(colors["accentDefault"])
            painter.drawRoundedRect(QRect(bar_left, y + 29, max(1, row.value), 8), control, control)
            if usage_width:
                painter.setPen(colors["textSecondary"])
                painter.drawText(
                    QRect(usage_left, y, usage_width, self.ROW_HEIGHT),
                    Qt.AlignLeft | Qt.AlignVCenter,
                    caption_metrics.elidedText(row.usage, Qt.ElideRight, usage_width),
                )
            y += self.ROW_HEIGHT
            if index != len(self.rows) - 1:
                painter.setPen(QPen(colors["strokeDivider"], 1.0))
                painter.drawLine(_CARD_PADDING, y, self.width() - _CARD_PADDING, y)


class QmlPlusDemoCard(_SurfaceCard):
    def __init__(self, parent: QWidget) -> None:
        super().__init__(parent)
        self.body = QVBoxLayout(self)
        self.body.setContentsMargins(_CARD_PADDING, _CARD_PADDING, _CARD_PADDING, _CARD_PADDING)
        self.body.setSpacing(12)


class _AnchorDemoBox(QWidget):
    def __init__(self, parent: QWidget) -> None:
        super().__init__(parent)
        self.setAttribute(Qt.WidgetAttribute.WA_StyledBackground, True)
        self.setMinimumHeight(140)
        self.setStyleSheet("border: 1px dashed rgba(128,128,128,0.45); border-radius: 6px;")
        self.centered = fluentqt.Button("Centered", self)
        self.centered.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
        self.pinned = fluentqt.Button("Top-right", self)
        self.pinned.setFluentStyle(fluentqt.Button.ButtonStyle.Standard)

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self.centered.adjustSize()
        self.pinned.adjustSize()
        self.centered.move(
            (self.width() - self.centered.width()) // 2,
            (self.height() - self.centered.height()) // 2,
        )
        self.pinned.move(self.width() - self.pinned.width() - 12, 12)


def _code(lines: Iterable[str], parent: QWidget, object_name: str) -> GalleryCodeBlock:
    block = GalleryCodeBlock("\n".join(lines), parent)
    block.setObjectName(object_name)
    return block


def _build_qmlplus(layout: QVBoxLayout, content: QWidget) -> tuple[QWidget, ...]:
    widgets: list[QWidget] = []

    _add_section_heading(layout, "Property binding", content)
    property_card = QmlPlusDemoCard(content)
    property_card.body.addWidget(
        _secondary_caption(
            'One-way · PropertyBinder::bind(slider, "value", bar, "value")',
            property_card,
        )
    )
    slider_row = QWidget(property_card)
    slider_layout = QHBoxLayout(slider_row)
    slider_layout.setContentsMargins(0, 0, 0, 0)
    slider_layout.setSpacing(12)
    slider = fluentqt.Slider(Qt.Horizontal, slider_row)
    slider.setRange(0, 100)
    slider.setValue(40)
    value_label = fluentqt.Label("40%", slider_row)
    value_label.setFluentTypography(fluentqt.FontRole.BodyStrong)
    value_label.setMinimumWidth(48)
    slider_layout.addWidget(slider, 1)
    slider_layout.addWidget(value_label)
    property_card.body.addWidget(slider_row)
    progress = fluentqt.ProgressBar(property_card)
    progress.setRange(0, 100)
    progress.setValue(40)
    property_card.body.addWidget(progress)
    slider.valueChanged.connect(progress.setValue)
    slider.valueChanged.connect(lambda value: value_label.setText("{0}%".format(value)))
    property_card.body.addSpacing(4)
    property_card.body.addWidget(
        _secondary_caption("Two-way · each switch's isOn mirrors the other", property_card)
    )
    switch_row = QWidget(property_card)
    switch_layout = QHBoxLayout(switch_row)
    switch_layout.setContentsMargins(0, 0, 0, 0)
    switch_layout.setSpacing(24)
    switch_a = fluentqt.ToggleSwitch(switch_row)
    switch_b = fluentqt.ToggleSwitch(switch_row)
    for control in (switch_a, switch_b):
        control.setOnContent("On")
        control.setOffContent("Off")
        switch_layout.addWidget(control)
    switch_layout.addStretch()
    switch_a.toggled.connect(switch_b.setIsOn)
    switch_b.toggled.connect(switch_a.setIsOn)
    property_card.body.addWidget(switch_row)
    layout.addWidget(property_card)
    property_code = _code(
        (
            "slider = fluentqt.Slider(Qt.Horizontal, parent)",
            "slider.setRange(0, 100)",
            "bar = fluentqt.ProgressBar(parent)",
            "bar.setRange(0, 100)",
            "",
            "# One-way: slider.value drives bar.value.",
            "slider.valueChanged.connect(bar.setValue)",
            "",
            "# Two-way: each switch mirrors the other.",
            "switch_a.toggled.connect(switch_b.setIsOn)",
            "switch_b.toggled.connect(switch_a.setIsOn)",
        ),
        content,
        "galleryFoundationQmlPlusPropertyCodeBlock",
    )
    layout.addWidget(property_code)
    widgets.extend((property_card, property_code))

    _add_section_heading(layout, "States", content)
    state_card = QmlPlusDemoCard(content)
    state_card.body.addWidget(
        _secondary_caption(
            'setState("active") applies a named bundle of property changes.',
            state_card,
        )
    )
    state_label = fluentqt.Label("Idle", state_card)
    state_label.setFluentTypography(fluentqt.FontRole.BodyLargeStrong)
    state_card.body.addWidget(state_label)
    state_progress = fluentqt.ProgressBar(state_card)
    state_progress.setRange(0, 100)
    state_progress.setValue(20)
    state_card.body.addWidget(state_progress)
    state_toggle = fluentqt.ToggleSwitch(state_card)
    state_toggle.setOnContent("Active")
    state_toggle.setOffContent("Idle")
    state_card.body.addWidget(state_toggle)

    def apply_state(active: bool) -> None:
        state_label.setText("Active — one state rewrote text + value" if active else "Idle")
        state_progress.setValue(90 if active else 20)

    state_toggle.toggled.connect(apply_state)
    layout.addWidget(state_card)
    state_code = _code(
        (
            "def apply_state(active):",
            '    label.setText("Active — one state, many props" if active else "Idle")',
            "    bar.setValue(90 if active else 20)",
            "",
            "toggle.toggled.connect(apply_state)",
        ),
        content,
        "galleryFoundationQmlPlusStateCodeBlock",
    )
    layout.addWidget(state_code)
    widgets.extend((state_card, state_code))

    _add_section_heading(layout, "Anchors", content)
    anchor_card = QmlPlusDemoCard(content)
    anchor_card.body.addWidget(
        _secondary_caption(
            "AnchorLayout pins children by edge — resize the window and watch them track.",
            anchor_card,
        )
    )
    anchor_box = _AnchorDemoBox(anchor_card)
    anchor_card.body.addWidget(anchor_box)
    layout.addWidget(anchor_card)
    anchor_code = _code(
        (
            "# Python widgets can apply the same center and top-right constraints.",
            "centered.move((box.width() - centered.width()) // 2,",
            "              (box.height() - centered.height()) // 2)",
            "pinned.move(box.width() - pinned.width() - 12, 12)",
        ),
        content,
        "galleryFoundationQmlPlusAnchorCodeBlock",
    )
    layout.addWidget(anchor_code)
    widgets.extend((anchor_card, anchor_code))
    return tuple(widgets)


_COLOR_GROUPS = (
    ("Text", ("textPrimary", "textSecondary", "textTertiary", "textDisabled", "textOnAccent", "textAccentPrimary")),
    ("Fill & accent", (
        "accentDefault", "accentSecondary", "accentTertiary", "accentDisabled",
        "controlDefault", "controlSecondary", "controlTertiary", "controlDisabled",
        "controlAltSecondary", "controlAltTertiary", "subtleTransparent",
        "subtleSecondary", "subtleTertiary",
    )),
    ("Background & layers", (
        "bgCanvas", "bgLayer", "bgLayerAlt", "bgSolid", "grey10", "grey20",
        "grey30", "grey40", "grey50", "grey60", "grey90", "grey130",
        "grey160", "grey190",
    )),
    ("Stroke", (
        "strokeDefault", "strokeSecondary", "strokeStrong", "strokeCard",
        "strokeDivider", "strokeSurface", "strokeFocusOuter", "strokeFocusInner",
    )),
    ("System", (
        "systemCritical", "systemCriticalBg", "systemCaution", "systemCautionBg",
        "systemInfo", "systemInfoBg", "systemSuccess", "systemSuccessBg",
    )),
    ("Charts", tuple("chart{0}".format(index) for index in range(1, 13))),
)


def _build_color(layout: QVBoxLayout, content: QWidget) -> tuple[QWidget, ...]:
    grids: list[QWidget] = []
    for header, tokens in _COLOR_GROUPS:
        _add_section_heading(layout, header, content)
        grid = FoundationTileGrid(tokens, content)
        grid.setObjectName("galleryFoundationColorGrid." + header.replace(" ", ""))
        layout.addWidget(grid)
        grids.append(grid)
    return tuple(grids)


def _build_typography(layout: QVBoxLayout, content: QWidget) -> tuple[QWidget, ...]:
    _add_section_heading(layout, "Type ramp", content)
    ramp = TypographyRampCard(content)
    ramp.setObjectName("galleryTypographyTypeRamp")
    layout.addWidget(ramp)
    _add_section_heading(layout, "Use semantic roles", content)
    code = _code(
        (
            'title = fluentqt.Label("Settings", parent)',
            "title.setFluentTypography(fluentqt.FontRole.Title)",
            "",
            "# Every role resolves to the bundled, static, hinted faces.",
            "body_font = fluentqt.font_for_role(fluentqt.FontRole.Body)",
            "heading_font = fluentqt.font_for_role(fluentqt.FontRole.Title)",
        ),
        content,
        "galleryFoundationTypographyCodeBlock",
    )
    layout.addWidget(code)
    return ramp, code


def _build_geometry(layout: QVBoxLayout, content: QWidget) -> tuple[QWidget, ...]:
    _add_section_heading(layout, "Corner radius", content)
    radius = RadiusCard(content)
    radius.setObjectName("galleryFoundationRadiusCard")
    layout.addWidget(radius)
    _add_section_heading(layout, "Stroke widths", content)
    strokes = TokenMeasureCard(
        (
            MeasureRow("Normal", "Spacing::Border::Normal", 1, "Default control and surface borders"),
            MeasureRow("Focused", "Spacing::Border::Focused", 2, "Keyboard focus indicator"),
        ),
        content,
    )
    strokes.setObjectName("galleryFoundationStrokeCard")
    layout.addWidget(strokes)
    _add_section_heading(layout, "Use geometry tokens", content)
    code = _code(
        (
            "painter.drawRoundedRect(control_rect, 4, 4)",
            "focus_pen = QPen(colors.stroke_focus_outer, 2)",
        ),
        content,
        "galleryFoundationGeometryCodeBlock",
    )
    layout.addWidget(code)
    return radius, strokes, code


_SPACING_GROUPS = (
    ("Spacing scale", (
        MeasureRow("XSmall", "Spacing::XSmall", 4, "Icon-to-text and compact inline gaps"),
        MeasureRow("Small", "Spacing::Small", 8, "Related controls"),
        MeasureRow("Medium", "Spacing::Medium", 12, "Default control padding"),
        MeasureRow("Standard", "Spacing::Standard", 16, "Card content and grouped regions"),
        MeasureRow("Large", "Spacing::Large", 24, "Sections and dialog content"),
        MeasureRow("XLarge", "Spacing::XLarge", 32, "Page-level whitespace"),
        MeasureRow("XXLarge", "Spacing::XXLarge", 48, "Large page separation"),
    )),
    ("Component padding", (
        MeasureRow("Control horizontal", "Spacing::Padding::ControlHorizontal", 12, "Generic controls"),
        MeasureRow("Control vertical", "Spacing::Padding::ControlVertical", 8, "Generic controls"),
        MeasureRow("ComboBox horizontal", "Spacing::Padding::ComboBoxHorizontal", 11, "ComboBox content"),
        MeasureRow("ComboBox vertical", "Spacing::Padding::ComboBoxVertical", 4, "ComboBox content"),
        MeasureRow("Text field horizontal", "Spacing::Padding::TextFieldHorizontal", 8, "LineEdit and text inputs"),
        MeasureRow("Text field vertical", "Spacing::Padding::TextFieldVertical", 4, "LineEdit and text inputs"),
        MeasureRow("List item horizontal", "Spacing::Padding::ListItemHorizontal", 12, "Collection rows"),
        MeasureRow("List item vertical", "Spacing::Padding::ListItemVertical", 8, "Collection rows"),
        MeasureRow("Card", "Spacing::Padding::Card", 16, "Card content"),
        MeasureRow("Dialog", "Spacing::Padding::Dialog", 24, "Dialog content"),
    )),
    ("Gaps", (
        MeasureRow("Tight", "Spacing::Gap::Tight", 4, "Icon and text"),
        MeasureRow("Normal", "Spacing::Gap::Normal", 8, "Controls in one group"),
        MeasureRow("Loose", "Spacing::Gap::Loose", 16, "Separate control groups"),
        MeasureRow("Section", "Spacing::Gap::Section", 24, "Page sections"),
    )),
    ("Control heights", (
        MeasureRow("Small", "Spacing::ControlHeight::Small", 24, "Compact layouts"),
        MeasureRow("Standard", "Spacing::ControlHeight::Standard", 32, "Default controls"),
        MeasureRow("Large", "Spacing::ControlHeight::Large", 40, "Spacious layouts"),
    )),
)


def _build_spacing(layout: QVBoxLayout, content: QWidget) -> tuple[QWidget, ...]:
    cards: list[QWidget] = []
    for header, rows in _SPACING_GROUPS:
        _add_section_heading(layout, header, content)
        card = TokenMeasureCard(rows, content)
        card.setObjectName("galleryFoundationSpacingCard." + header.replace(" ", ""))
        layout.addWidget(card)
        cards.append(card)
    return tuple(cards)


@dataclass(frozen=True)
class IconRecord:
    name: str
    display_name: str
    semantic_name: str
    semantic_tokens: tuple[str, ...]
    codepoint: int
    design_size: int


_ICON_PATTERN = re.compile(r"^(ic_fluent_.+)_([0-9]+)_regular$")
_COMPLETE_ICON_PATTERN = re.compile(
    r"(ic_fluent_[A-Za-z0-9_]+_[0-9]+_regular)\b", re.IGNORECASE
)


def _load_icon_catalog() -> tuple[IconRecord, ...]:
    path = asset_path("icon_catalog.json")
    if not path.is_file():
        return ()
    with path.open("r", encoding="utf-8") as stream:
        data = json.load(stream)
    records: list[IconRecord] = []
    for name, codepoint in data.items():
        match = _ICON_PATTERN.match(name)
        if match:
            semantic = match.group(1).removeprefix("ic_fluent_").replace("_", " ")
            size = int(match.group(2))
        else:
            semantic = name.replace("_", " ")
            size = 0
        display = " ".join(word[:1].upper() + word[1:] for word in semantic.split())
        normalized = " ".join(semantic.lower().split())
        records.append(
            IconRecord(
                name=name,
                display_name=display,
                semantic_name=normalized,
                semantic_tokens=tuple(normalized.split()),
                codepoint=int(codepoint),
                design_size=size,
            )
        )
    records.sort(key=lambda item: (item.display_name.casefold(), item.design_size))
    return tuple(records)


def _snap_icon_pixel_size(requested_pixel_size: int) -> int:
    """Mirror ``Typography::Icons::snapIconPixelSize`` exactly."""

    optical_sizes = (12, 16, 20, 24, 28, 32, 40, 48, 64)
    if requested_pixel_size <= 0:
        return 16
    if requested_pixel_size < 12:
        return requested_pixel_size
    return min(
        optical_sizes,
        key=lambda size: (abs(requested_pixel_size - size), -size),
    )


def _icon_font(pixel_size: int) -> QFont:
    """Build the same icon font used by ``Typography::Icons::font``."""

    font = QFont("FluentQt Icons")
    font.setPixelSize(_snap_icon_pixel_size(pixel_size))
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


def _catalog_glyphs_for_size(
    records: tuple[IconRecord, ...], design_size: int
) -> dict[int, str]:
    """Resolve every catalog codepoint like ``Icons::glyphForSize``."""

    snapped_size = _snap_icon_pixel_size(design_size)
    by_name = {record.name: record for record in records}
    variants: dict[str, list[IconRecord]] = {}
    for record in records:
        match = _ICON_PATTERN.match(record.name)
        if match:
            variants.setdefault(match.group(1), []).append(record)

    resolved: dict[int, str] = {}
    for record in records:
        fallback = chr(record.codepoint)
        match = _ICON_PATTERN.match(record.name)
        if not match or design_size <= 0:
            resolved[record.codepoint] = fallback
            continue

        family = match.group(1)
        exact = by_name.get(f"{family}_{snapped_size}_regular")
        if exact is not None:
            resolved[record.codepoint] = chr(exact.codepoint)
            continue

        candidates = variants.get(family, ())
        best = min(
            candidates,
            key=lambda candidate: (
                abs(candidate.design_size - snapped_size),
                0 if candidate.design_size >= snapped_size else 1,
                candidate.design_size,
            ),
            default=None,
        )
        resolved[record.codepoint] = (
            chr(best.codepoint) if best is not None else fallback
        )
    return resolved


_SEARCH_ALIASES = {
    "trash": ("delete",),
    "gear": ("settings",),
    "cog": ("settings",),
    "preferences": ("settings",),
    "close": ("dismiss",),
    "x": ("dismiss",),
    "duplicate": ("copy",),
    "bell": ("alert",),
    "notification": ("alert",),
    "email": ("mail",),
    "envelope": ("mail",),
    "photo": ("image",),
    "picture": ("image",),
    "user": ("person",),
    "profile": ("person",),
    "account": ("person",),
}


def _direct_score(record: IconRecord, term: str) -> int | None:
    compact = record.semantic_name.replace(" ", "")
    if record.semantic_name == term:
        return 0
    if compact == term:
        return 1
    if term in record.semantic_tokens:
        return 2
    if record.semantic_name.startswith(term) or compact.startswith(term):
        return 4
    if any(token.startswith(term) for token in record.semantic_tokens):
        return 6
    if len(term) >= 3 and term in record.semantic_name:
        return 10
    if len(term) >= 3 and term in record.name.lower():
        return 12
    return None


def _is_fuzzy_text_term(term: str) -> bool:
    return len(term) >= 3 and all(character.isalpha() for character in term)


def _maximum_edit_distance(length: int) -> int:
    if length <= 4:
        return 1
    if length <= 8:
        return 2
    return min(3, max(2, length // 4))


def _bounded_damerau_levenshtein(
    left: str,
    right: str,
    maximum_distance: int,
) -> int:
    if abs(len(left) - len(right)) > maximum_distance:
        return maximum_distance + 1
    if left == right:
        return 0

    previous_previous = [0] * (len(right) + 1)
    previous = list(range(len(right) + 1))
    current = [0] * (len(right) + 1)
    for row in range(1, len(left) + 1):
        current[0] = row
        for column in range(1, len(right) + 1):
            substitution = 0 if left[row - 1] == right[column - 1] else 1
            distance = min(
                previous[column] + 1,
                current[column - 1] + 1,
                previous[column - 1] + substitution,
            )
            if (
                row > 1
                and column > 1
                and left[row - 1] == right[column - 2]
                and left[row - 2] == right[column - 1]
            ):
                distance = min(
                    distance,
                    previous_previous[column - 2] + 1,
                )
            current[column] = distance
        previous_previous, previous, current = previous, current, previous_previous
    return previous[len(right)]


def _fuzzy_term_score(record: IconRecord, term: str) -> int | None:
    if not _is_fuzzy_text_term(term):
        return None

    threshold = _maximum_edit_distance(len(term))
    best_distance = threshold + 1
    best_length_difference = threshold + 1
    for token in record.semantic_tokens:
        if not _is_fuzzy_text_term(token):
            continue
        length_difference = abs(len(term) - len(token))
        if length_difference > threshold:
            continue
        distance = _bounded_damerau_levenshtein(term, token, threshold)
        if distance < best_distance or (
            distance == best_distance
            and length_difference < best_length_difference
        ):
            best_distance = distance
            best_length_difference = length_difference

    compact = record.semantic_name.replace(" ", "")
    if len(term) >= 5:
        length_difference = abs(len(term) - len(compact))
        if length_difference <= threshold:
            distance = _bounded_damerau_levenshtein(
                term,
                compact,
                threshold,
            )
            if distance < best_distance or (
                distance == best_distance
                and length_difference < best_length_difference
            ):
                best_distance = distance
                best_length_difference = length_difference

    if best_distance > threshold:
        return None
    return 100 + best_distance * 20 + best_length_difference


def _closest_score(
    record: IconRecord,
    terms: list[str],
    allow_fuzzy: bool,
) -> int | None:
    total = 0
    for term in terms:
        direct = _direct_score(record, term)
        if direct is not None:
            total += direct
            continue

        alias_score: int | None = None
        for alternative in _SEARCH_ALIASES.get(term, ()):
            alternative_score = _direct_score(record, alternative)
            if alternative_score is None:
                continue
            candidate = 60 + alternative_score
            alias_score = (
                candidate
                if alias_score is None
                else min(alias_score, candidate)
            )
        if alias_score is not None:
            total += alias_score
            continue

        if not allow_fuzzy:
            return None
        fuzzy_score = _fuzzy_term_score(record, term)
        if fuzzy_score is None:
            return None
        total += fuzzy_score
    return total


def _rank_icons(records: tuple[IconRecord, ...], query: str) -> tuple[list[int], bool]:
    exact_names = {match.lower() for match in _COMPLETE_ICON_PATTERN.findall(query)}
    if exact_names:
        return [index for index, record in enumerate(records) if record.name.lower() in exact_names], False
    normalized = re.sub(r"[^A-Za-z0-9_+]+", " ", query).lower().strip()
    terms = normalized.split()
    known_sizes = {record.design_size for record in records}
    sizes: set[int] = set()
    codepoints: set[int] = set()
    text_terms: list[str] = []
    for term in terms:
        if re.fullmatch(r"u\+[0-9a-f]{1,6}", term):
            codepoints.add(int(term[2:], 16))
        elif term.isdigit() and int(term) in known_sizes:
            sizes.add(int(term))
        else:
            text_terms.append(term)

    ranked: list[tuple[int, int]] = []
    for index, record in enumerate(records):
        if sizes and record.design_size not in sizes:
            continue
        if codepoints and record.codepoint not in codepoints:
            continue
        scores = [_direct_score(record, term) for term in text_terms]
        if all(score is not None for score in scores):
            ranked.append((sum(int(score) for score in scores), index))
    if ranked or not text_terms:
        ranked.sort()
        return [index for _score, index in ranked], False

    closest: list[tuple[int, int]] = []
    for allow_fuzzy in (False, True):
        for index, record in enumerate(records):
            if sizes and record.design_size not in sizes:
                continue
            if codepoints and record.codepoint not in codepoints:
                continue
            score = _closest_score(record, text_terms, allow_fuzzy)
            if score is not None:
                closest.append((score, index))
        if closest:
            break
    closest.sort()
    return [index for _score, index in closest], bool(closest)


class IconCatalogGrid(QWidget):
    MINIMUM_TILE_WIDTH = 44
    TILE_HEIGHT = 44
    TILE_GAP = 6
    MAXIMUM_COLUMNS = 24
    ITEMS_PER_PAGE = 216

    def __init__(self, browser: "GalleryIconBrowser", records: tuple[IconRecord, ...]) -> None:
        super().__init__(browser)
        self.browser = browser
        self.records = records
        self._large_icon_font = _icon_font(20)
        self._large_glyphs = _catalog_glyphs_for_size(records, 20)
        self.rows = list(range(len(records)))
        self.page_index = 0
        self.hovered_index = -1
        self.pressed_index = -1
        self.showing_closest = False
        self.setObjectName("galleryIconGrid")
        self.setAccessibleName("Fluent icon catalog page")
        self.setMouseTracking(True)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        self._tooltip_anchor = QWidget(self)
        self._tooltip_anchor.setObjectName("galleryIconHoverAnchor")
        self._tooltip_anchor.setAttribute(Qt.WA_TransparentForMouseEvents)
        self._tooltip_anchor.setAttribute(Qt.WA_NoSystemBackground)
        self._tooltip_anchor.hide()
        self._hover_tip = fluentqt.ToolTip.attach(
            self._tooltip_anchor,
            "",
            fluentqt.ToolTip.Placement.Above,
        )
        self._hover_tip.setObjectName("galleryIconHoverTip")
        self._hover_tip.setThemeSource(browser)
        self._hover_tip.setMargins(QMargins(12, 8, 12, 10))
        self._hover_timer = QTimer(self)
        self._hover_timer.setSingleShot(True)
        self._hover_timer.setInterval(320)
        self._hover_timer.timeout.connect(self._show_hover_tip)
        self._scroll_bar = None

    def page_count(self) -> int:
        return max(1, (len(self.rows) + self.ITEMS_PER_PAGE - 1) // self.ITEMS_PER_PAGE)

    def first_offset(self) -> int:
        return 0 if not self.rows else self.page_index * self.ITEMS_PER_PAGE

    def page_item_count(self) -> int:
        return max(0, min(self.ITEMS_PER_PAGE, len(self.rows) - self.first_offset()))

    def record_at(self, page_item_index: int) -> IconRecord | None:
        filtered_index = self.first_offset() + page_item_index
        if page_item_index < 0 or filtered_index < 0 or filtered_index >= len(self.rows):
            return None
        return self.records[self.rows[filtered_index]]

    def set_filter(self, query: str) -> None:
        self.rows, self.showing_closest = _rank_icons(self.records, query)
        self.page_index = 0
        self.hovered_index = -1
        self.pressed_index = -1
        self._hide_hover_tip()
        self.updateGeometry()
        self.browser.updateGeometry()
        self.update()

    def set_page(self, index: int) -> None:
        bounded = max(0, min(index, self.page_count() - 1))
        if bounded == self.page_index:
            return
        self.page_index = bounded
        self.hovered_index = -1
        self.pressed_index = -1
        self._hide_hover_tip()
        self.updateGeometry()
        self.browser.updateGeometry()
        self.update()

    def _columns(self, width: int | None = None) -> int:
        width = max(1, self.width() if width is None else width)
        return max(1, min(self.MAXIMUM_COLUMNS, (width + self.TILE_GAP) // (self.MINIMUM_TILE_WIDTH + self.TILE_GAP)))

    def _height_for_width(self, width: int) -> int:
        count = self.page_item_count()
        if not count:
            return 96
        columns = self._columns(width)
        rows = (count + columns - 1) // columns
        return rows * self.TILE_HEIGHT + max(0, rows - 1) * self.TILE_GAP

    def hasHeightForWidth(self) -> bool:
        return True

    def heightForWidth(self, width: int) -> int:
        return self._height_for_width(width)

    def sizeHint(self) -> QSize:
        width = self.width() if self.width() >= self.MINIMUM_TILE_WIDTH else 920
        return QSize(width, self._height_for_width(width))

    def minimumSizeHint(self) -> QSize:
        return QSize(self.MINIMUM_TILE_WIDTH, self.TILE_HEIGHT)

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        height = self._height_for_width(self.width())
        self.setMinimumHeight(height)
        self.setMaximumHeight(height)

    def _tile_rect(self, index: int) -> QRect:
        columns = self._columns()
        column = index % columns
        row = index // columns
        content_width = max(1, self.width()) - (columns - 1) * self.TILE_GAP
        base_width = content_width // columns
        remainder = content_width % columns
        left = column * (base_width + self.TILE_GAP) + min(column, remainder)
        width = base_width + (1 if column < remainder else 0)
        return QRect(left, row * (self.TILE_HEIGHT + self.TILE_GAP), width, self.TILE_HEIGHT)

    def _index_at(self, point: QPoint) -> int:
        for index in range(self.page_item_count()):
            if self._tile_rect(index).contains(point):
                return index
        return -1

    def paintEvent(self, event) -> None:
        colors = _theme_tokens()
        control, _overlay = _radii()
        painter = QPainter(self)
        painter.setClipRect(event.rect())
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)
        if not self.page_item_count():
            painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
            painter.setPen(colors["textSecondary"])
            painter.drawText(self.rect(), Qt.AlignCenter, "No icons match this search.")
            return
        item_count = self.page_item_count()
        columns = self._columns()
        row_step = self.TILE_HEIGHT + self.TILE_GAP
        first_row = max(0, event.rect().top() // row_step)
        last_row = min((item_count - 1) // columns, event.rect().bottom() // row_step)
        painter.setFont(self._large_icon_font)
        for row in range(first_row, last_row + 1):
            for column in range(columns):
                index = row * columns + column
                record = self.record_at(index)
                if record is None:
                    break
                tile = self._tile_rect(index)
                hovered = index == self.hovered_index
                pressed = index == self.pressed_index
                painter.setPen(
                    QPen(
                        colors["accentDefault"] if hovered else colors["strokeCard"],
                        1.0,
                    )
                )
                painter.setBrush(
                    colors["subtleTertiary"]
                    if pressed
                    else colors["subtleSecondary"] if hovered else colors["bgLayer"]
                )
                painter.drawRoundedRect(
                    QRectF(tile).adjusted(0.5, 0.5, -0.5, -0.5), control, control
                )
                painter.setPen(
                    colors["textAccentPrimary"] if hovered else colors["textPrimary"]
                )
                painter.drawText(
                    tile.adjusted(5, 5, -5, -5),
                    Qt.AlignCenter,
                    self._large_glyphs.get(record.codepoint, chr(record.codepoint)),
                )

    def mouseMoveEvent(self, event) -> None:
        hovered = self._index_at(event.position().toPoint())
        if hovered != self.hovered_index:
            previous = self.hovered_index
            self._hide_hover_tip()
            self.hovered_index = hovered
            if previous >= 0:
                self.update(self._tile_rect(previous).adjusted(-1, -1, 1, 1))
            if hovered >= 0:
                self.update(self._tile_rect(hovered).adjusted(-1, -1, 1, 1))
                self._hover_timer.start()
            self.setCursor(Qt.PointingHandCursor if hovered >= 0 else Qt.ArrowCursor)
        super().mouseMoveEvent(event)

    def mousePressEvent(self, event) -> None:
        self._hide_hover_tip()
        if event.button() == Qt.LeftButton:
            self.pressed_index = self._index_at(event.position().toPoint())
            if self.pressed_index >= 0:
                self.update(self._tile_rect(self.pressed_index))
        super().mousePressEvent(event)

    def mouseReleaseEvent(self, event) -> None:
        released = self._index_at(event.position().toPoint()) if event.button() == Qt.LeftButton else -1
        pressed = self.pressed_index
        self.pressed_index = -1
        if pressed >= 0:
            self.update(self._tile_rect(pressed))
        if released >= 0 and released == pressed:
            record = self.record_at(released)
            if record is not None:
                snippet = 'fluentqt.FontIcon("{0}")'.format(record.name)
                QApplication.clipboard().setText(snippet)
                try:
                    _show_gallery_toast(self, "Copied " + record.name)
                except (RuntimeError, ValueError):
                    pass
        super().mouseReleaseEvent(event)

    def leaveEvent(self, event) -> None:
        previous = self.hovered_index
        self.hovered_index = -1
        self.pressed_index = -1
        self.unsetCursor()
        self._hide_hover_tip()
        if previous >= 0:
            self.update(self._tile_rect(previous))
        super().leaveEvent(event)

    def wheelEvent(self, event) -> None:
        self._hide_hover_tip()
        event.ignore()

    def showEvent(self, event) -> None:
        super().showEvent(event)
        if self._scroll_bar is not None:
            return
        ancestor = self.parentWidget()
        while ancestor is not None:
            if isinstance(ancestor, QAbstractScrollArea):
                self._scroll_bar = ancestor.verticalScrollBar()
                self._scroll_bar.valueChanged.connect(self._hide_hover_tip)
                break
            ancestor = ancestor.parentWidget()

    def hideEvent(self, event) -> None:
        self._hide_hover_tip()
        super().hideEvent(event)

    def _show_hover_tip(self) -> None:
        if not (
            shiboken6.isValid(self._tooltip_anchor)
            and shiboken6.isValid(self._hover_tip)
        ):
            return
        record = self.record_at(self.hovered_index)
        if record is None:
            return
        self._tooltip_anchor.setGeometry(self._tile_rect(self.hovered_index))
        self._tooltip_anchor.show()
        self._tooltip_anchor.raise_()
        width = 6 if record.codepoint > 0xFFFF else 4
        self._hover_tip.setText(
            "{0}\n{1}\nU+{2:0{3}X}  ·  {4} px\n"
            "Click to copy lookup".format(
                record.display_name,
                record.name,
                record.codepoint,
                width,
                record.design_size,
            )
        )
        local = self._tooltip_anchor.rect().center()
        help_event = QHelpEvent(
            QEvent.ToolTip,
            local,
            self._tooltip_anchor.mapToGlobal(local),
        )
        QCoreApplication.sendEvent(self._tooltip_anchor, help_event)

    def _hide_hover_tip(self, *_unused: object) -> None:
        if shiboken6.isValid(self._hover_timer):
            self._hover_timer.stop()
        if shiboken6.isValid(self._hover_tip):
            self._hover_tip.hide()
        if shiboken6.isValid(self._tooltip_anchor):
            self._tooltip_anchor.hide()


class GalleryIconBrowser(QWidget):
    def __init__(self, parent: QWidget) -> None:
        super().__init__(parent)
        self.setObjectName("galleryIconBrowser")
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        self.records = _load_icon_catalog()
        body = QVBoxLayout(self)
        body.setContentsMargins(0, 0, 0, 0)
        body.setSpacing(12)
        guidance = _secondary_caption(
            "Search the complete Regular catalog by name, size, or U+ codepoint. Exact matches stay deterministic; typos fall back to the closest names. Hover for metadata and click to copy a lookup.",
            self,
        )
        body.addWidget(guidance)
        search_row = QHBoxLayout()
        search_row.setContentsMargins(0, 0, 0, 0)
        search_row.setSpacing(12)
        self.search = fluentqt.LineEdit(self)
        self.search.setObjectName("galleryIconSearch")
        self.search.setPlaceholderText(
            "Search name, 20, U+F109, or paste a lookup..."
        )
        self.search.setClearButtonEnabled(True)
        self.search.setMinimumWidth(240)
        self.count_label = _secondary_caption("", self)
        self.count_label.setObjectName("galleryIconCount")
        self.count_label.setWordWrap(False)
        search_row.addWidget(self.search, 1)
        search_row.addWidget(self.count_label, 0, Qt.AlignVCenter)
        body.addLayout(search_row)
        self.grid = IconCatalogGrid(self, self.records)
        body.addWidget(self.grid)
        self.pagination = QWidget(self)
        self.pagination.setObjectName("galleryIconPagination")
        pagination_layout = QHBoxLayout(self.pagination)
        pagination_layout.setContentsMargins(0, 0, 0, 0)
        pagination_layout.setSpacing(12)
        self.page_label = _secondary_caption("", self.pagination)
        self.page_label.setObjectName("galleryIconPageLabel")
        self.page_label.setWordWrap(False)
        self.pager = fluentqt.PipsPager(self.pagination)
        self.pager.setObjectName("galleryIconPager")
        self.pager.setMaxVisiblePips(7)
        visibility = fluentqt.PipsPager.PipsPagerButtonVisibility.Visible
        self.pager.setPreviousButtonVisibility(visibility)
        self.pager.setNextButtonVisibility(visibility)
        pagination_layout.addWidget(self.page_label, 0, Qt.AlignVCenter)
        pagination_layout.addStretch()
        pagination_layout.addWidget(self.pager, 0, Qt.AlignVCenter)
        body.addWidget(self.pagination)
        self.search.textChanged.connect(self._filter)
        self.pager.selectedPageIndexChanged.connect(self._select_page)
        self._update_labels()

    def refresh_theme(self) -> None:
        self.grid.update()
        # Global Fluent theme/style changes already update native controls.
        # Repaint the app-owned browser without calling protected C++ hooks:
        # ToolTip intentionally does not expose onThemeUpdated() in PySide6.
        for widget in (
            self.grid._hover_tip,
            self.count_label,
            self.page_label,
            self.pager,
        ):
            if shiboken6.isValid(widget):
                widget.update()

    def iconCount(self) -> int:
        return len(self.records)

    def visibleIconCount(self) -> int:
        return len(self.grid.rows)

    def showingClosestMatches(self) -> bool:
        return self.grid.showing_closest

    def _filter(self, query: str) -> None:
        self.grid.set_filter(query)
        self._update_labels()

    def _select_page(self, index: int) -> None:
        self.grid.set_page(index)
        self._update_labels()

    def _update_labels(self) -> None:
        visible = len(self.grid.rows)
        total = len(self.records)
        if self.grid.showing_closest:
            self.count_label.setText("Closest matches: {0:,} of {1:,}".format(visible, total))
        elif visible == total:
            self.count_label.setText("{0:,} icons".format(total))
        else:
            self.count_label.setText("{0:,} of {1:,}".format(visible, total))
        page_count = self.grid.page_count()
        self.pager.blockSignals(True)
        self.pager.setNumberOfPages(page_count)
        self.pager.setSelectedPageIndex(self.grid.page_index)
        self.pager.blockSignals(False)
        self.pagination.setVisible(page_count > 1)
        first = 0 if not visible else self.grid.first_offset() + 1
        last = self.grid.first_offset() + self.grid.page_item_count()
        self.page_label.setText(
            "{0:,}-{1:,} of {2:,}  ·  Page {3:,} of {4:,}".format(
                first, last, visible, self.grid.page_index + 1, page_count
            )
        )


def _build_iconography(layout: QVBoxLayout, content: QWidget) -> tuple[QWidget, ...]:
    _add_section_heading(layout, "Complete Regular catalog", content)
    browser = GalleryIconBrowser(content)
    layout.addWidget(browser)
    return (browser,)


def populate_foundation_topic_page(
    route_id: str,
    layout: QVBoxLayout,
    content: QWidget,
) -> tuple[QWidget, ...]:
    builders: dict[str, Callable[[QVBoxLayout, QWidget], tuple[QWidget, ...]]] = {
        "foundation-qmlplus": _build_qmlplus,
        "foundation-typography": _build_typography,
        "foundation-color": _build_color,
        "foundation-iconography": _build_iconography,
        "foundation-geometry": _build_geometry,
        "foundation-spacing": _build_spacing,
    }
    widgets = builders[route_id](layout, content)
    layout.addStretch()
    return widgets


__all__ = [
    "FoundationTileGrid",
    "GalleryIconBrowser",
    "MeasureRow",
    "RadiusCard",
    "TokenMeasureCard",
    "TypographyRampCard",
    "populate_foundation_topic_page",
]
