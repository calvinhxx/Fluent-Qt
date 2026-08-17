"""Standalone Gallery ports for native collection controls."""

from __future__ import annotations

from textwrap import dedent

from .native_samples import register_source_samples


def _script(body: str, imports: str = "") -> str:
    prefix = "import fluentqt\n"
    if imports:
        prefix += imports.strip() + "\n"
    return prefix + "\n" + dedent(body).strip() + "\n"


class _SourceHelper(str):
    """Dedent the sample body appended to a reusable source helper."""

    def __add__(self, other: str) -> str:
        return super().__add__(dedent(other))


_WIDGETS = "from PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget"
_MODEL_IMPORTS = (
    "from PySide6.QtCore import (QEvent, QItemSelectionModel, QMargins, "
    "QPersistentModelIndex, QRectF, QSize, QSizeF, Qt, QUrl, QUrlQuery)\n"
    "from PySide6.QtGui import (QColor, QFont, QFontMetricsF, QGuiApplication, "
    "QLinearGradient, QPainter, QPainterPath, QPen, QPixmap, QStandardItem, "
    "QStandardItemModel)\n"
    "from PySide6.QtNetwork import (QNetworkAccessManager, QNetworkReply, "
    "QNetworkRequest)\n"
    "from PySide6.QtWidgets import (QApplication, QHBoxLayout, QStyle, "
    "QStyledItemDelegate, QSizePolicy, QVBoxLayout, QWidget)\n"
    "from fluentqt_gallery.foundation_pages import (_theme_snapshot, _theme_tokens)"
)

_PAGE_HELPER = _SourceHelper(dedent(
    """
    def make_page(title, detail):
        page = QWidget()
        page_layout = QVBoxLayout(page)
        page_layout.setContentsMargins(18, 16, 18, 16)
        page_layout.addWidget(fluentqt.Label(title, page))
        description = fluentqt.Label(detail, page)
        description.setWordWrap(True)
        page_layout.addWidget(description)
        page_layout.addStretch()
        return page
    """
))

_GRADIENT_PANE_HELPER = _SourceHelper(dedent(
    """
    class GradientPane(QWidget):
        def __init__(self, caption, from_color, to_color, parent=None):
            super().__init__(parent)
            self._caption = caption
            self._from = QColor(from_color)
            self._to = QColor(to_color)
            self.setSizePolicy(
                QSizePolicy.Policy.Expanding,
                QSizePolicy.Policy.Expanding,
            )
            self.setMinimumSize(64, 48)

        def paintEvent(self, event):
            del event
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
            surface = QRectF(self.rect()).adjusted(0.5, 0.5, -0.5, -0.5)
            gradient = QLinearGradient(surface.topLeft(), surface.bottomRight())
            gradient.setColorAt(0.0, self._from)
            gradient.setColorAt(1.0, self._to)
            path = QPainterPath()
            path.addRoundedRect(surface, 8.0, 8.0)
            painter.fillPath(path, gradient)

            font = QFont()
            font.setPixelSize(15)
            font.setWeight(QFont.Weight.DemiBold)
            painter.setFont(font)
            painter.setPen(QColor(255, 255, 255, 235))
            painter.drawText(surface, Qt.AlignmentFlag.AlignCenter, self._caption)


    def make_gradient_pane(caption, from_color, to_color, parent=None):
        return GradientPane(caption, from_color, to_color, parent)
    """
))

_GRADIENT_PHOTO_HELPER = _SourceHelper(dedent(
    """
    def make_canvas(size):
        screen = QGuiApplication.primaryScreen()
        dpr = max(1.0, float(screen.devicePixelRatio()) if screen else 1.0)
        pixel_size = QSize(
            max(1, int(size.width() * dpr + 0.5)),
            max(1, int(size.height() * dpr + 0.5)),
        )
        pixmap = QPixmap(pixel_size)
        pixmap.setDevicePixelRatio(dpr)
        pixmap.fill(Qt.GlobalColor.transparent)
        return pixmap


    def make_gradient_photo(size, from_color, to_color, caption):
        pixmap = make_canvas(size)
        painter = QPainter(pixmap)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
        surface = QRectF(0.0, 0.0, size.width(), size.height())
        gradient = QLinearGradient(surface.topLeft(), surface.bottomRight())
        gradient.setColorAt(0.0, QColor(from_color))
        gradient.setColorAt(1.0, QColor(to_color))
        path = QPainterPath()
        path.addRoundedRect(surface, 8.0, 8.0)
        painter.fillPath(path, gradient)
        if caption:
            font = QFont()
            font.setPixelSize(15)
            font.setWeight(QFont.Weight.DemiBold)
            painter.setFont(font)
            painter.setPen(QColor(255, 255, 255, 230))
            painter.drawText(
                surface.adjusted(16.0, 12.0, -16.0, -12.0),
                Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignBottom,
                caption,
            )
        painter.end()
        return pixmap
    """
))

_GRADIENT_IMPORTS = (
    "from PySide6.QtCore import QRectF, Qt\n"
    "from PySide6.QtGui import QColor, QFont, QLinearGradient, QPainter, QPainterPath\n"
    "from PySide6.QtWidgets import QHBoxLayout, QSizePolicy, QVBoxLayout, QWidget"
)
_GRADIENT_PHOTO_IMPORTS = (
    "from PySide6.QtCore import QRectF, QSize, Qt\n"
    "from PySide6.QtGui import (QColor, QFont, QGuiApplication, QLinearGradient, "
    "QPainter, QPainterPath, QPixmap)\n"
    "from PySide6.QtWidgets import QLabel"
)

_FLAT_MODEL_HELPER = _SourceHelper(dedent(
    """
    def make_model(labels, parent=None):
        model = QStandardItemModel(parent)
        for text in labels:
            item = QStandardItem(text)
            item.setEditable(False)
            model.appendRow(item)
        return model
    """
))

_DATA_GRID_IMPORTS = (
    "from PySide6.QtCore import (QAbstractTableModel, QItemSelectionModel, "
    "QModelIndex, QRectF, Qt)\n"
    "from PySide6.QtGui import QPen, QStandardItem, QStandardItemModel\n"
    "from PySide6.QtWidgets import QAbstractItemView, QStyledItemDelegate\n"
    "from fluentqt_gallery.foundation_pages import _theme_tokens"
)

_DATA_GRID_MODEL_HELPER = _SourceHelper(dedent(
    """
    class LargeDataGridModel(QAbstractTableModel):
        def rowCount(self, parent=QModelIndex()):
            return 0 if parent.isValid() else 100_000

        def columnCount(self, parent=QModelIndex()):
            return 0 if parent.isValid() else 5

        def data(self, index, role=Qt.ItemDataRole.DisplayRole):
            if not index.isValid():
                return None
            if (
                role == Qt.ItemDataRole.TextAlignmentRole
                and index.column() >= 3
            ):
                return Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter
            if role != Qt.ItemDataRole.DisplayRole:
                return None
            row = index.row() + 1
            values = (
                "Build #{0:06d}".format(row),
                "macOS-{0:02d}".format(row % 24 + 1),
                ("Passed", "Running", "Queued")[row % 3],
                "{0}m {1:02d}s".format(row % 9 + 1, row % 60),
                str(row % 12),
            )
            return values[index.column()]

        def headerData(self, section, orientation, role):
            if role != Qt.ItemDataRole.DisplayRole:
                return None
            if orientation == Qt.Orientation.Vertical:
                return section + 1
            headers = ("Build", "Runner", "Status", "Duration", "Artifacts")
            return headers[section] if 0 <= section < len(headers) else None

        def flags(self, index):
            if not index.isValid():
                return Qt.ItemFlag.NoItemFlags
            return Qt.ItemFlag.ItemIsEnabled | Qt.ItemFlag.ItemIsSelectable


    def make_project_data_grid_model(parent, editable):
        model = QStandardItemModel(parent)
        model.setHorizontalHeaderLabels(
            ("Project", "Owner", "Status", "Priority")
        )
        projects = (
            "Aurora", "Beacon", "Canvas", "Delta",
            "Ember", "Fjord", "Grove", "Harbor",
        )
        owners = ("Maya", "Noah", "Priya", "Riley")
        for row, project in enumerate(projects):
            items = [
                QStandardItem(project),
                QStandardItem(owners[row % len(owners)]),
                QStandardItem("Review" if row % 3 == 0 else "Active"),
                QStandardItem(str(row % 4 + 1)),
            ]
            for item in items:
                item.setEditable(editable)
            items[-1].setTextAlignment(
                Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter
            )
            model.appendRow(items)
        return model
    """
))

_DATA_GRID_VALIDATION_HELPER = _SourceHelper(dedent(
    """
    DATA_GRID_VALIDATION_MESSAGE_ROLE = int(Qt.ItemDataRole.UserRole) + 420


    class ValidatingDataGridModel(QStandardItemModel):
        def setData(self, index, value, role=Qt.ItemDataRole.EditRole):
            if (
                role == Qt.ItemDataRole.EditRole
                and len(str(value).strip()) < 3
            ):
                QStandardItemModel.setData(
                    self,
                    index,
                    "Use at least 3 characters",
                    DATA_GRID_VALIDATION_MESSAGE_ROLE,
                )
                return False
            if role == Qt.ItemDataRole.EditRole:
                QStandardItemModel.setData(
                    self,
                    index,
                    None,
                    DATA_GRID_VALIDATION_MESSAGE_ROLE,
                )
            return QStandardItemModel.setData(self, index, value, role)


    class DataGridValidationDelegate(QStyledItemDelegate):
        def __init__(self, grid):
            super().__init__(grid)
            self._base_delegate = grid.itemDelegate()

        def paint(self, painter, option, index):
            if self._base_delegate is not None:
                self._base_delegate.paint(painter, option, index)
            else:
                super().paint(painter, option, index)
            if not index.data(DATA_GRID_VALIDATION_MESSAGE_ROLE):
                return
            painter.save()
            painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.setPen(QPen(_theme_tokens(self)["systemCritical"], 1.5))
            painter.drawRoundedRect(
                QRectF(option.rect).adjusted(1.5, 1.5, -1.5, -1.5),
                3.0,
                3.0,
            )
            painter.restore()
    """
))

_PHOTO_MODEL_HELPER = _SourceHelper(dedent(
    """
    PHOTO_IMAGE_ROLE = int(Qt.ItemDataRole.UserRole) + 701
    PHOTO_SUBTITLE_ROLE = int(Qt.ItemDataRole.UserRole) + 702
    PHOTO_FROM_ROLE = int(Qt.ItemDataRole.UserRole) + 703
    PHOTO_TO_ROLE = int(Qt.ItemDataRole.UserRole) + 704


    class PhotoDelegate(QStyledItemDelegate):
        def __init__(self, parent=None, fixed_size=None, multiple=False):
            super().__init__(parent)
            self._grid_view = (
                parent if isinstance(parent, fluentqt.GridView) else None
            )
            self._fixed_size = fixed_size
            self._multiple = multiple

        def sizeHint(self, option, index):
            del option
            if self._grid_view is not None:
                return self._grid_view.gridSize()
            if self._fixed_size is not None:
                return self._fixed_size
            value = index.data(Qt.ItemDataRole.SizeHintRole)
            return value if isinstance(value, QSize) else QSize(160, 118)

        def paint(self, painter, option, index):
            if not index.isValid():
                return
            painter.save()
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform)
            painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
            snapshot = _theme_snapshot(self)
            colors = snapshot["colors"]
            selected = bool(option.state & QStyle.StateFlag.State_Selected)
            hovered = bool(option.state & QStyle.StateFlag.State_MouseOver)
            enabled = bool(option.state & QStyle.StateFlag.State_Enabled)

            card = QRectF(option.rect).adjusted(2.0, 2.0, -2.0, -2.0)
            clip = QPainterPath()
            clip.addRoundedRect(card, 4.0, 4.0)
            painter.fillPath(clip, colors["bgLayerAlt"])

            pixmap = index.data(PHOTO_IMAGE_ROLE)
            if isinstance(pixmap, QPixmap) and not pixmap.isNull():
                painter.setClipPath(clip)
                source_dpr = max(1.0e-6, pixmap.devicePixelRatioF())
                source_size = QSizeF(pixmap.size()) / source_dpr
                scale = max(
                    card.width() / source_size.width(),
                    card.height() / source_size.height(),
                )
                visible = QSizeF(card.width() / scale, card.height() / scale)
                crop = QRectF(
                    (source_size.width() - visible.width()) * 0.5 * source_dpr,
                    (source_size.height() - visible.height()) * 0.5 * source_dpr,
                    visible.width() * source_dpr,
                    visible.height() * source_dpr,
                )
                painter.drawPixmap(card, pixmap, crop)
                if hovered:
                    painter.fillRect(card, QColor(255, 255, 255, 24))

                title = str(index.data(Qt.ItemDataRole.DisplayRole) or "")
                subtitle = str(index.data(PHOTO_SUBTITLE_ROLE) or "")
                title_font = QFont(option.font)
                title_font.setWeight(QFont.Weight.DemiBold)
                subtitle_font = QFont(option.font)
                if subtitle_font.pixelSize() > 0:
                    subtitle_font.setPixelSize(max(11, subtitle_font.pixelSize() - 2))
                elif subtitle_font.pointSizeF() > 0.0:
                    subtitle_font.setPointSizeF(max(8.0, subtitle_font.pointSizeF() - 1.0))
                title_metrics = QFontMetricsF(title_font)
                subtitle_metrics = QFontMetricsF(subtitle_font)
                gap = 0.0 if not subtitle else 1.0
                text_height = title_metrics.height()
                if subtitle:
                    text_height += subtitle_metrics.height() + gap
                bar_height = min(card.height(), max(48.0, text_height + 10.0))
                label_bar = QRectF(
                    card.left(), card.bottom() - bar_height, card.width(), bar_height
                )
                scrim = QLinearGradient(label_bar.topLeft(), label_bar.bottomLeft())
                scrim.setColorAt(0.0, QColor(0, 0, 0, 20))
                scrim.setColorAt(1.0, QColor(0, 0, 0, 150))
                painter.fillRect(label_bar, scrim)

                text_top = label_bar.top() + (label_bar.height() - text_height) / 2.0
                title_rect = QRectF(
                    label_bar.left() + 10.0,
                    text_top,
                    max(0.0, label_bar.width() - 20.0),
                    title_metrics.height(),
                )
                painter.setFont(title_font)
                painter.setPen(QColor(255, 255, 255, 240))
                painter.drawText(
                    title_rect,
                    Qt.AlignmentFlag.AlignLeft
                    | Qt.AlignmentFlag.AlignVCenter
                    | Qt.TextFlag.TextSingleLine,
                    title_metrics.elidedText(
                        title, Qt.TextElideMode.ElideRight, round(title_rect.width())
                    ),
                )
                if subtitle:
                    subtitle_rect = QRectF(
                        title_rect.left(),
                        title_rect.bottom() + gap,
                        title_rect.width(),
                        subtitle_metrics.height(),
                    )
                    painter.setFont(subtitle_font)
                    painter.setPen(QColor(255, 255, 255, 205))
                    painter.drawText(
                        subtitle_rect,
                        Qt.AlignmentFlag.AlignLeft
                        | Qt.AlignmentFlag.AlignVCenter
                        | Qt.TextFlag.TextSingleLine,
                        subtitle_metrics.elidedText(
                            subtitle,
                            Qt.TextElideMode.ElideRight,
                            round(subtitle_rect.width()),
                        ),
                    )
                painter.setClipping(False)

            painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.setPen(
                QPen(
                    colors["accentDefault"] if selected else colors["strokeDefault"],
                    2.0 if selected else 1.0,
                )
            )
            painter.drawPath(clip)
            if self._grid_view is None and hovered and not selected:
                painter.setPen(QPen(colors["subtleSecondary"], 1.0))
                painter.drawPath(clip)
            if self._multiple:
                check = QRectF(card.right() - 29.0, card.top() + 7.0, 22.0, 22.0)
                if selected and enabled:
                    painter.setPen(Qt.PenStyle.NoPen)
                    painter.setBrush(colors["accentDefault"])
                    painter.drawEllipse(check)
                    check_font = QFont("FluentQt Icons")
                    check_font.setPixelSize(12)
                    painter.setFont(check_font)
                    painter.setPen(QColor("white"))
                    painter.drawText(check, Qt.AlignmentFlag.AlignCenter, "\ue73e")
                else:
                    painter.setPen(QPen(QColor(255, 255, 255, 200), 1.5))
                    painter.setBrush(QColor(0, 0, 0, 60))
                    painter.drawEllipse(check.adjusted(0.75, 0.75, -0.75, -0.75))
            painter.restore()


    def make_gradient_pixmap(size, from_color, to_color):
        screen = QGuiApplication.primaryScreen()
        dpr = max(1.0, screen.devicePixelRatio() if screen is not None else 1.0)
        physical_size = QSize(
            max(1, int(size.width() * dpr + 0.5)),
            max(1, int(size.height() * dpr + 0.5)),
        )
        pixmap = QPixmap(physical_size)
        pixmap.setDevicePixelRatio(dpr)
        pixmap.fill(Qt.GlobalColor.transparent)
        painter = QPainter(pixmap)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
        gradient = QLinearGradient(0.0, 0.0, size.width(), size.height())
        gradient.setColorAt(0.0, QColor(from_color))
        gradient.setColorAt(1.0, QColor(to_color))
        path = QPainterPath()
        path.addRoundedRect(
            QRectF(0.0, 0.0, float(size.width()), float(size.height())),
            8.0,
            8.0,
        )
        painter.fillPath(path, gradient)
        painter.end()
        return pixmap


    def photo_url(photo, row):
        image_ids = (
            "photo-1500530855697-b586d89ba3ee",
            "photo-1470770841072-f978cf4d019e",
            "photo-1441974231531-c6227db76b6e",
            "photo-1470252649378-9c29740c9fa8",
            "photo-1506744038136-46273834b3fb",
            "photo-1472214103451-9374bd1c798e",
            "photo-1469474968028-56623f02e42e",
            "photo-1511818966892-d7d671e672a2",
            "photo-1497366754035-f200968a6e72",
        )
        size = photo[4]
        url = QUrl("https://images.unsplash.com/" + image_ids[row % len(image_ids)])
        query = QUrlQuery()
        query.addQueryItem("fm", "jpg")
        query.addQueryItem("fit", "crop")
        query.addQueryItem("w", str(size.width() * 2))
        query.addQueryItem("h", str(size.height() * 2))
        query.addQueryItem("q", "82")
        url.setQuery(query)
        return url


    def load_photo_network_images(model, photos, owner):
        manager = QNetworkAccessManager(owner)
        owner._gallery_photo_network_manager = manager
        owner._gallery_photo_network_pending = 0
        owner._gallery_photo_network_replies = []
        application = QApplication.instance()
        if (
            application is not None
            and application.property("fluentqtGalleryAutomated")
        ):
            return
        owner._gallery_photo_network_pending = len(photos)
        for row, photo in enumerate(photos):
            index = QPersistentModelIndex(model.index(row, 0))
            request = QNetworkRequest(photo_url(photo, row))
            request.setAttribute(
                QNetworkRequest.Attribute.RedirectPolicyAttribute,
                QNetworkRequest.RedirectPolicy.NoLessSafeRedirectPolicy,
            )
            request.setTransferTimeout(10000)
            request.setHeader(
                QNetworkRequest.KnownHeaders.UserAgentHeader,
                "Fluent-Qt Gallery/1.0",
            )
            reply = manager.get(request)
            owner._gallery_photo_network_replies.append(reply)

            def finished(current_reply=reply, current_index=index):
                try:
                    if (
                        current_reply.error()
                        != QNetworkReply.NetworkError.NoError
                        or not current_index.isValid()
                    ):
                        return
                    pixmap = QPixmap()
                    if pixmap.loadFromData(current_reply.readAll()):
                        model.setData(current_index, pixmap, PHOTO_IMAGE_ROLE)
                finally:
                    owner._gallery_photo_network_pending = max(
                        0, owner._gallery_photo_network_pending - 1
                    )
                    if current_reply in owner._gallery_photo_network_replies:
                        owner._gallery_photo_network_replies.remove(
                            current_reply
                        )
                    current_reply.deleteLater()

            reply.finished.connect(finished)


    def make_photo_model(photos, parent=None, load_network=False):
        photos = tuple(photos)
        model = QStandardItemModel(parent)
        for title, subtitle, from_color, to_color, size in photos:
            item = QStandardItem(title)
            item.setEditable(False)
            item.setData(subtitle, PHOTO_SUBTITLE_ROLE)
            item.setData(QColor(from_color), PHOTO_FROM_ROLE)
            item.setData(QColor(to_color), PHOTO_TO_ROLE)
            item.setData(size, Qt.ItemDataRole.SizeHintRole)
            item.setData(
                make_gradient_pixmap(
                    QSize(size.width() * 2, size.height() * 2),
                    from_color,
                    to_color,
                ),
                PHOTO_IMAGE_ROLE,
            )
            model.appendRow(item)
        if load_network and parent is not None:
            load_photo_network_images(model, photos, parent)
        return model
    """
))

_TREE_MODEL_HELPER = _SourceHelper(dedent(
    """
    TREE_ICON_GLYPH_ROLE = int(Qt.ItemDataRole.UserRole) + 720
    TREE_ICON_COLOR_ROLE = int(Qt.ItemDataRole.UserRole) + 721


    def make_tree_item(text, glyph, color):
        item = QStandardItem(text)
        item.setEditable(False)
        item.setData(glyph, TREE_ICON_GLYPH_ROLE)
        item.setData(QColor(color), TREE_ICON_COLOR_ROLE)
        return item


    def make_tree_model(parent=None):
        model = QStandardItemModel(parent)
        def folder(text):
            return make_tree_item(text, "\ue838", "#CA8A1A")

        def file(text):
            return make_tree_item(text, "\ue8a5", "#528BC4")

        work = folder("Work documents")
        work.appendRow(file("Proposal.docx"))
        work.appendRow(file("Budget.xlsx"))
        archive = folder("Archive")
        archive.appendRow(file("Q1-review.pdf"))
        archive.appendRow(file("Q2-review.pdf"))
        work.appendRow(archive)
        model.appendRow(work)

        photos = folder("Photos")
        photos.appendRow(file("Trip.png"))
        photos.appendRow(file("Family.png"))
        model.appendRow(photos)

        music = folder("Music")
        music.appendRow(file("Playlist.m3u"))
        model.appendRow(music)
        return model
    """
))

_TREE_DELEGATE_HELPER = _SourceHelper(dedent(
    """
    class TreeRowDelegate(QStyledItemDelegate):
        def __init__(self, view, row_height=36):
            super().__init__(view)
            self._view = view
            self._row_height = row_height
            self._check_box_visible = False

        def setCheckBoxVisible(self, visible):
            self._check_box_visible = bool(visible)

        def sizeHint(self, option, index):
            del option, index
            return QSize(0, self._row_height)

        def _background_rect(self, option):
            viewport_width = self._view.viewport().width()
            top = option.rect.top() + 2.0
            height = option.rect.height() - 4.0
            if option.direction == Qt.LayoutDirection.RightToLeft:
                right = option.rect.x() + option.rect.width() - 2.0
                return QRectF(2.0, top, max(0.0, right - 2.0), height)
            left = option.rect.left() + 2.0
            return QRectF(
                left,
                top,
                max(0.0, viewport_width - 2.0 - left),
                height,
            )

        def _check_box_rect(self, option):
            if not self._check_box_visible:
                return QRectF()
            background = self._background_rect(option)
            rtl = option.direction == Qt.LayoutDirection.RightToLeft
            leading = option.rect.x() + option.rect.width() if rtl else option.rect.x()
            x = leading - 12.0 - 22.0 if rtl else leading + 12.0
            return QRectF(x, background.top(), 22.0, background.height())

        def paint(self, painter, option, index):
            if not index.isValid():
                return
            snapshot = _theme_snapshot(self)
            colors = snapshot["colors"]
            painter.save()
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
            selected = bool(option.state & QStyle.StateFlag.State_Selected)
            enabled = bool(option.state & QStyle.StateFlag.State_Enabled)
            hovered = bool(option.state & QStyle.StateFlag.State_MouseOver)
            pressed = bool(option.state & QStyle.StateFlag.State_Sunken) and hovered
            background = QColor(Qt.GlobalColor.transparent)
            if enabled:
                if pressed:
                    background = colors["subtleTertiary"]
                elif selected or hovered:
                    background = colors["subtleSecondary"]

            background_rect = self._background_rect(option)
            if background.alpha() > 0:
                path = QPainterPath()
                path.addRoundedRect(background_rect, 4.0, 4.0)
                painter.setPen(Qt.PenStyle.NoPen)
                painter.setBrush(background)
                painter.drawPath(path)

            text_color = colors["textPrimary"]
            if not enabled:
                text_color = colors["textDisabled"]

            if (
                selected
                and enabled
                and not self._check_box_visible
                and not self._view.selectionIndicatorVisible()
            ):
                progress = max(
                    0.0,
                    min(
                        float(self._view.selectedIndicatorProgress(index)),
                        1.0,
                    ),
                )
                motion_active = self._view.isIndicatorMotionActiveForIndex(
                    index
                )
                direction = (
                    self._view.indicatorMotionDirection()
                    if motion_active
                    else fluentqt.TreeView.IndicatorVerticalDirection.None_
                )
                hierarchy = (
                    self._view.indicatorHierarchyTransition()
                    if motion_active
                    else fluentqt.TreeView.IndicatorHierarchyTransition.None_
                )
                indicator_width = 3.0
                full_height = 16.0
                indicator_height = full_height * (0.35 + 0.65 * progress)
                rtl = option.direction == Qt.LayoutDirection.RightToLeft
                settled_x = (
                    option.rect.x()
                    + option.rect.width()
                    - 4.0
                    - indicator_width
                    if rtl
                    else option.rect.left() + 4.0
                )
                settled_y = background_rect.center().y() - full_height / 2.0
                remaining = 1.0 - progress
                indicator_x = settled_x
                if (
                    hierarchy
                    == fluentqt.TreeView.IndicatorHierarchyTransition.Inward
                ):
                    indicator_x += (-1.0 if rtl else 1.0) * remaining * 4.0
                elif (
                    hierarchy
                    == fluentqt.TreeView.IndicatorHierarchyTransition.Outward
                ):
                    indicator_x += (1.0 if rtl else -1.0) * remaining * 3.0
                indicator_y = (
                    background_rect.center().y() - indicator_height / 2.0
                )
                if (
                    direction
                    == fluentqt.TreeView.IndicatorVerticalDirection.Down
                ):
                    indicator_y = settled_y - remaining * 6.0
                elif (
                    direction
                    == fluentqt.TreeView.IndicatorVerticalDirection.Up
                ):
                    indicator_y = (
                        settled_y
                        + full_height
                        - indicator_height
                        + remaining * 6.0
                    )
                indicator = QRectF(
                    indicator_x,
                    indicator_y,
                    indicator_width,
                    indicator_height,
                )
                painter.setPen(Qt.PenStyle.NoPen)
                accent = QColor(colors["accentDefault"])
                accent.setAlphaF(accent.alphaF() * progress)
                painter.setBrush(accent)
                painter.drawRoundedRect(indicator, 1.5, 1.5)

            rtl = option.direction == Qt.LayoutDirection.RightToLeft
            cursor_x = (
                option.rect.x() + option.rect.width() - 12.0
                if rtl
                else option.rect.x() + 12.0
            )

            def take_leading_rect(width):
                nonlocal cursor_x
                x = cursor_x - width if rtl else cursor_x
                rect = QRectF(x, background_rect.top(), width, background_rect.height())
                cursor_x += (-1.0 if rtl else 1.0) * (width + 4.0)
                return rect

            if self._check_box_visible:
                check_area = take_leading_rect(22.0)
                state = index.data(Qt.ItemDataRole.CheckStateRole)
                if state is None:
                    state = Qt.CheckState.Unchecked
                state = Qt.CheckState(state)
                box = QRectF(
                    check_area.center().x() - 9.0,
                    check_area.center().y() - 9.0,
                    18.0,
                    18.0,
                )
                box_path = QPainterPath()
                box_path.addRoundedRect(box, 3.0, 3.0)
                if state in (
                    Qt.CheckState.Checked,
                    Qt.CheckState.PartiallyChecked,
                ):
                    painter.setPen(Qt.PenStyle.NoPen)
                    painter.setBrush(colors["accentDefault"])
                    painter.drawPath(box_path)
                    check_font = QFont("FluentQt Icons")
                    check_font.setPixelSize(12)
                    painter.setFont(check_font)
                    painter.setPen(Qt.GlobalColor.white)
                    painter.drawText(
                        box,
                        Qt.AlignmentFlag.AlignCenter,
                        "\ue73e"
                        if state == Qt.CheckState.Checked
                        else "\ue738",
                    )
                else:
                    painter.setPen(QPen(colors["strokeDefault"], 1.5))
                    painter.setBrush(Qt.BrushStyle.NoBrush)
                    painter.drawPath(box_path)

            chevron_rect = take_leading_rect(20.0)
            if index.model().hasChildren(index):
                icon_font = QFont("FluentQt Icons")
                icon_font.setPixelSize(12)
                painter.setFont(icon_font)
                painter.setPen(text_color)
                painter.save()
                painter.translate(chevron_rect.center())
                rotation = self._view.chevronRotation(index)
                painter.rotate((-1.0 if rtl else 1.0) * rotation * 90.0)
                painter.translate(-chevron_rect.center())
                painter.drawText(
                    chevron_rect,
                    Qt.AlignmentFlag.AlignCenter,
                    "\ue973" if rtl else "\ue974",
                )
                painter.restore()

            glyph = str(index.data(TREE_ICON_GLYPH_ROLE) or "")
            if glyph:
                glyph_color = text_color
                color_value = index.data(TREE_ICON_COLOR_ROLE)
                if isinstance(color_value, QColor) and color_value.isValid():
                    glyph_color = color_value
                icon_font = QFont("FluentQt Icons")
                icon_font.setPixelSize(16)
                painter.setFont(icon_font)
                painter.setPen(glyph_color)
                painter.drawText(
                    take_leading_rect(22.0),
                    Qt.AlignmentFlag.AlignCenter,
                    glyph,
                )

            if rtl:
                text_rect = QRectF(
                    background_rect.left() + 8.0,
                    background_rect.top(),
                    max(0.0, cursor_x - background_rect.left() - 8.0),
                    background_rect.height(),
                )
            else:
                text_rect = QRectF(
                    cursor_x,
                    background_rect.top(),
                    max(0.0, background_rect.right() - cursor_x - 8.0),
                    background_rect.height(),
                )
            painter.setPen(text_color)
            painter.setFont(option.font)
            text = str(index.data(Qt.ItemDataRole.DisplayRole) or "")
            painter.drawText(
                text_rect,
                (Qt.AlignmentFlag.AlignRight if rtl else Qt.AlignmentFlag.AlignLeft)
                | Qt.AlignmentFlag.AlignVCenter,
                painter.fontMetrics().elidedText(
                    text,
                    Qt.TextElideMode.ElideLeft if rtl else Qt.TextElideMode.ElideRight,
                    int(text_rect.width()),
                ),
            )
            painter.restore()

        def editorEvent(self, event, model, option, index):
            if event.type() == QEvent.Type.MouseButtonPress and self._check_box_visible:
                if self._check_box_rect(option).contains(event.position()):
                    return True
            if event.type() == QEvent.Type.MouseButtonRelease:
                position = event.position()
                if self._check_box_visible and self._check_box_rect(option).contains(position):
                    current = index.data(Qt.ItemDataRole.CheckStateRole)
                    current = (
                        Qt.CheckState(current)
                        if current is not None
                        else Qt.CheckState.Unchecked
                    )
                    next_state = (
                        Qt.CheckState.Unchecked
                        if current == Qt.CheckState.Checked
                        else Qt.CheckState.Checked
                    )
                    model.setData(index, next_state, Qt.ItemDataRole.CheckStateRole)

                    def cascade(parent_index):
                        for row in range(model.rowCount(parent_index)):
                            child = model.index(row, 0, parent_index)
                            model.setData(
                                child,
                                next_state,
                                Qt.ItemDataRole.CheckStateRole,
                            )
                            cascade(child)

                    def roll_up(child_index):
                        parent_index = child_index.parent()
                        if not parent_index.isValid():
                            return
                        states = [
                            Qt.CheckState(
                                model.index(row, 0, parent_index).data(
                                    Qt.ItemDataRole.CheckStateRole
                                )
                            )
                            for row in range(model.rowCount(parent_index))
                        ]
                        if all(state == Qt.CheckState.Checked for state in states):
                            parent_state = Qt.CheckState.Checked
                        elif all(state == Qt.CheckState.Unchecked for state in states):
                            parent_state = Qt.CheckState.Unchecked
                        else:
                            parent_state = Qt.CheckState.PartiallyChecked
                        model.setData(
                            parent_index,
                            parent_state,
                            Qt.ItemDataRole.CheckStateRole,
                        )
                        roll_up(parent_index)

                    cascade(index)
                    roll_up(index)
                    return True
                if (
                    model.hasChildren(index)
                    and self._background_rect(option).contains(position)
                ):
                    self._view.toggleExpanded(index)
                    return True
            return super().editorEvent(event, model, option, index)
    """
))

_DRAWER_HELPER = _SourceHelper(dedent(
    """
    class SampleSurface(QWidget):
        def __init__(self, parent=None):
            super().__init__(parent)
            self.setAutoFillBackground(False)

        def paintEvent(self, event):
            del event
            colors = _theme_tokens(self)
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            surface = QRectF(self.rect()).adjusted(0.5, 0.5, -0.5, -0.5)
            path = QPainterPath()
            path.addRoundedRect(surface, 8.0, 8.0)
            painter.fillPath(path, colors["bgLayerAlt"])
            painter.setPen(QPen(colors["strokeDefault"], 1.0))
            painter.drawPath(path)


    class DrawerGradientPane(QWidget):
        def __init__(self, caption, from_color, to_color, edge, parent=None):
            super().__init__(parent)
            self._caption = caption
            self._from = QColor(from_color)
            self._to = QColor(to_color)
            self._edge = edge
            self.setAutoFillBackground(False)
            self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, True)
            self.setSizePolicy(
                QSizePolicy.Policy.Expanding,
                QSizePolicy.Policy.Expanding,
            )

        def set_edge(self, edge):
            if self._edge != edge:
                self._edge = edge
                self.update()

        def _surface_path(self, rect, radius=8.0):
            left, top = rect.left(), rect.top()
            right, bottom = rect.right(), rect.bottom()
            Edge = fluentqt.DrawerView.DrawerEdge
            round_tl = self._edge in (Edge.Right, Edge.Bottom)
            round_tr = self._edge in (Edge.Left, Edge.Bottom)
            round_br = self._edge in (Edge.Left, Edge.Top)
            round_bl = self._edge in (Edge.Right, Edge.Top)
            path = QPainterPath()
            path.moveTo(left + radius if round_tl else left, top)
            path.lineTo(right - radius if round_tr else right, top)
            if round_tr:
                path.quadTo(right, top, right, top + radius)
            path.lineTo(right, bottom - radius if round_br else bottom)
            if round_br:
                path.quadTo(right, bottom, right - radius, bottom)
            path.lineTo(left + radius if round_bl else left, bottom)
            if round_bl:
                path.quadTo(left, bottom, left, bottom - radius)
            path.lineTo(left, top + radius if round_tl else top)
            if round_tl:
                path.quadTo(left, top, left + radius, top)
            path.closeSubpath()
            return path

        def paintEvent(self, event):
            del event
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
            surface = QRectF(self.rect())
            gradient = QLinearGradient(surface.topLeft(), surface.bottomRight())
            gradient.setColorAt(0.0, self._from)
            gradient.setColorAt(1.0, self._to)
            painter.setClipPath(self._surface_path(surface))
            painter.fillRect(self.rect(), gradient)
            painter.setClipping(False)
            font = QFont()
            font.setPixelSize(15)
            font.setWeight(QFont.Weight.DemiBold)
            painter.setFont(font)
            painter.setPen(QColor(255, 255, 255, 235))
            painter.drawText(
                surface, Qt.AlignmentFlag.AlignCenter, self._caption
            )


    def horizontal_group(parent, spacing=8):
        group = QWidget(parent)
        layout = QHBoxLayout(group)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(spacing)
        layout.setAlignment(
            Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
        )
        return group


    def make_status_label(parent, text):
        label = fluentqt.Label(text, parent)
        label.setFluentTypography(fluentqt.FontRole.Body)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        return label


    class DrawerNavigationItem(QWidget):
        def __init__(self, text, glyph, icon_color, selected=False, parent=None):
            super().__init__(parent)
            self._text = text
            self._glyph = glyph
            self._icon_color = QColor(icon_color)
            self._selected = selected
            self._hovered = False
            self._pressed = False
            self.on_activated = None
            self.setCursor(Qt.CursorShape.PointingHandCursor)
            self.setMouseTracking(True)
            self.setSizePolicy(
                QSizePolicy.Policy.Expanding,
                QSizePolicy.Policy.Fixed,
            )

        def sizeHint(self):
            return QSize(210, 40)

        def minimumSizeHint(self):
            return QSize(160, 36)

        def set_selected(self, selected):
            if self._selected != selected:
                self._selected = selected
                self.update()

        def enterEvent(self, event):
            super().enterEvent(event)
            self._hovered = True
            self.update()

        def leaveEvent(self, event):
            super().leaveEvent(event)
            self._hovered = False
            self._pressed = False
            self.update()

        def mousePressEvent(self, event):
            if event.button() == Qt.MouseButton.LeftButton:
                self._pressed = True
                self.update()
            super().mousePressEvent(event)

        def mouseReleaseEvent(self, event):
            activate = (
                self._pressed
                and event.button() == Qt.MouseButton.LeftButton
                and self.rect().contains(event.position().toPoint())
            )
            self._pressed = False
            self.update()
            if activate and self.on_activated is not None:
                self.on_activated()
            super().mouseReleaseEvent(event)

        def paintEvent(self, event):
            del event
            colors = _theme_tokens(self)
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
            row_rect = QRectF(self.rect()).adjusted(0.0, 2.0, 0.0, -2.0)
            if self._pressed or self._hovered or self._selected:
                background = (
                    colors["subtleTertiary"]
                    if self._pressed
                    else colors["subtleSecondary"]
                )
                path = QPainterPath()
                path.addRoundedRect(row_rect, 6.0, 6.0)
                painter.fillPath(path, background)
            if self._selected:
                indicator = QRectF(
                    row_rect.left() + 5.0,
                    row_rect.center().y() - 7.0,
                    3.0,
                    14.0,
                )
                painter.setPen(Qt.PenStyle.NoPen)
                painter.setBrush(colors["accentDefault"])
                painter.drawRoundedRect(indicator, 1.5, 1.5)
            icon_rect = QRectF(
                row_rect.left() + 26.0,
                row_rect.center().y() - 14.0,
                28.0,
                28.0,
            )
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(self._icon_color)
            painter.drawRoundedRect(icon_rect, 7.0, 7.0)
            icon_font = QFont("FluentQt Icons")
            icon_font.setPixelSize(16)
            painter.setFont(icon_font)
            painter.setPen(Qt.GlobalColor.white)
            painter.drawText(
                icon_rect, Qt.AlignmentFlag.AlignCenter, self._glyph
            )
            painter.setFont(
                fluentqt.font_for_role(
                    fluentqt.FontRole.BodyStrong
                    if self._selected
                    else fluentqt.FontRole.Body
                )
            )
            painter.setPen(
                colors["textPrimary"]
                if self.isEnabled()
                else colors["textDisabled"]
            )
            text_rect = QRectF(
                icon_rect.right() + 14.0,
                row_rect.top(),
                row_rect.right() - icon_rect.right() - 22.0,
                row_rect.height(),
            )
            painter.drawText(
                text_rect,
                Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter,
                painter.fontMetrics().elidedText(
                    self._text,
                    Qt.TextElideMode.ElideRight,
                    round(text_rect.width()),
                ),
            )
    """
))


register_source_samples(
    "drawer-view",
    ("DrawerView",),
    {
        "drawer-view-basic": (
            "host",
            _script(
                _DRAWER_HELPER
                + """
                host = QWidget(globals().get("gallery_parent"))
                host.setFixedSize(420, 240)
                host.setStyleSheet(
                    "background: rgba(128, 128, 128, 18); border-radius: 8px;"
                )

                drawer = fluentqt.DrawerView(host)
                drawer.setEdge(fluentqt.DrawerView.DrawerEdge.Right)
                drawer.setDrawerLength(260)
                panel = QWidget()
                panel_layout = QVBoxLayout(panel)
                panel_layout.setContentsMargins(14, 18, 14, 18)
                panel_layout.setSpacing(6)
                title = fluentqt.Label("Navigation", panel)
                title.setFluentTypography(fluentqt.FontRole.BodyStrong)
                title.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                panel_layout.addWidget(title)
                rows = (
                    ("Home", "\ue80f", "#0078D4"),
                    ("Music", "\ue8d6", "#038387"),
                    ("Downloads", "\ue896", "#CA5010"),
                    ("Settings", "\ue713", "#8764B8"),
                )
                nav_items = []
                for index, (text, glyph, color) in enumerate(rows):
                    item = DrawerNavigationItem(
                        text, glyph, color, index == 0, panel
                    )
                    nav_items.append(item)
                    panel_layout.addWidget(item)
                for item in nav_items:
                    item.on_activated = (
                        lambda selected=item: [
                            nav_item.set_selected(nav_item is selected)
                            for nav_item in nav_items
                        ]
                    )
                panel_layout.addStretch(1)
                drawer.setOwnedContentWidget(panel)

                open_button = fluentqt.Button("Open drawer", host)
                open_button.setFluentStyle(
                    fluentqt.Button.ButtonStyle.Accent
                )
                open_button.move(18, 18)
                open_button.clicked.connect(drawer.open)
                """,
                _MODEL_IMPORTS,
            ),
        ),
        "drawer-view-edges": (
            "host",
            _script(
                _DRAWER_HELPER + """
                host = SampleSurface(globals().get("gallery_parent"))
                host.setFixedSize(460, 238)
                layout = QVBoxLayout(host)
                layout.setContentsMargins(18, 18, 18, 18)
                layout.setSpacing(12)
                controls = horizontal_group(host, 8)
                status = make_status_label(host, "Edge: Left, length: 170")

                drawer = fluentqt.DrawerView(host)
                drawer.setDrawerLength(170)
                drawer.setModal(False)
                drawer.setDim(False)
                panel = DrawerGradientPane(
                    "Drawer content",
                    "#1E6FD9",
                    "#6FD1F2",
                    fluentqt.DrawerView.DrawerEdge.Left,
                    drawer,
                )
                drawer.setOwnedContentWidget(panel)

                def open_from(edge, name):
                    drawer.setEdge(edge)
                    panel.set_edge(edge)
                    status.setText("Edge: {0}, length: 170".format(name))
                    drawer.open()

                for name, edge in (
                    ("Left", fluentqt.DrawerView.DrawerEdge.Left),
                    ("Right", fluentqt.DrawerView.DrawerEdge.Right),
                    ("Top", fluentqt.DrawerView.DrawerEdge.Top),
                    ("Bottom", fluentqt.DrawerView.DrawerEdge.Bottom),
                ):
                    button = fluentqt.Button(name, controls)
                    button.clicked.connect(
                        lambda _checked=False, value=edge, text=name: open_from(value, text)
                    )
                    controls.layout().addWidget(button)
                layout.addWidget(controls)
                layout.addWidget(status)
                layout.addStretch(1)
                """,
                _MODEL_IMPORTS,
            ),
        ),
        "drawer-view-close-policy": (
            "host",
            _script(
                _DRAWER_HELPER + """
                host = SampleSurface(globals().get("gallery_parent"))
                host.setFixedSize(460, 238)
                layout = QVBoxLayout(host)
                layout.setContentsMargins(18, 18, 18, 18)
                layout.setSpacing(12)
                buttons = horizontal_group(host, 8)
                open_button = fluentqt.Button(
                    "Open persistent drawer", buttons
                )
                open_button.setFluentStyle(
                    fluentqt.Button.ButtonStyle.Accent
                )
                buttons.layout().addWidget(open_button)
                status = make_status_label(host, "Closed")
                layout.addWidget(buttons)
                layout.addWidget(status)
                layout.addStretch(1)

                drawer = fluentqt.DrawerView(host)
                drawer.setEdge(fluentqt.DrawerView.DrawerEdge.Left)
                drawer.setAvailableMargins(
                    drawer_title_bar_avoidance_margins()
                )
                drawer.setDrawerLength(224)
                drawer.setModal(False)
                drawer.setDim(False)
                drawer.setClosePolicy(fluentqt.DrawerView.ClosePolicy(
                    fluentqt.DrawerView.NoAutoClose
                ))
                panel = QWidget()
                panel_layout = QVBoxLayout(panel)
                panel_layout.setContentsMargins(16, 18, 16, 18)
                panel_layout.setSpacing(10)
                title = fluentqt.Label("Persistent panel", panel)
                title.setFluentTypography(fluentqt.FontRole.BodyStrong)
                title.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                panel_layout.addWidget(title)
                panel_layout.addStretch(1)
                close_button = fluentqt.Button("Close", panel)
                panel_layout.addWidget(close_button)
                drawer.setOwnedContentWidget(panel)
                open_button.clicked.connect(drawer.open)
                close_button.clicked.connect(drawer.close)
                drawer.opened.connect(
                    lambda: status.setText("Open: outside click does not dismiss")
                )
                drawer.closed.connect(lambda: status.setText("Closed"))
                """,
                "from fluentqt_gallery.metrics import "
                "drawer_title_bar_avoidance_margins\n"
                + _MODEL_IMPORTS,
            ),
        ),
        "drawer-view-interactive-drag": (
            "host",
            _script(
                _DRAWER_HELPER + """
                host = SampleSurface(globals().get("gallery_parent"))
                host.setFixedSize(460, 238)
                layout = QVBoxLayout(host)
                layout.setContentsMargins(18, 18, 18, 18)
                layout.setSpacing(12)
                controls = horizontal_group(host, 8)
                open_button = fluentqt.Button("Open", controls)
                open_button.setFluentStyle(
                    fluentqt.Button.ButtonStyle.Accent
                )
                close_button = fluentqt.Button("Close", controls)
                controls.layout().addWidget(open_button)
                controls.layout().addWidget(close_button)
                layout.addWidget(controls)
                layout.addWidget(
                    make_status_label(
                        host, "Interactive: true, dragMargin: 36"
                    )
                )
                layout.addStretch(1)

                drawer = fluentqt.DrawerView(host)
                drawer.setInteractive(True)
                drawer.setDragMargin(36)
                drawer.setAnimationEnabled(True)
                drawer.setEdge(fluentqt.DrawerView.DrawerEdge.Left)
                drawer.setDrawerLength(210)
                drawer.setModal(False)
                drawer.setDim(False)
                panel = DrawerGradientPane(
                    "Drag surface",
                    "#2F9E44",
                    "#A9E34B",
                    fluentqt.DrawerView.DrawerEdge.Left,
                    drawer,
                )
                drawer.setOwnedContentWidget(panel)
                open_button.clicked.connect(drawer.open)
                close_button.clicked.connect(drawer.close)
                """,
                _MODEL_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "flip-view",
    ("FlipView",),
    {
        "flip-view-basic": (
            "flip_view",
            _script(
                _GRADIENT_PHOTO_HELPER
                + """
                flip_view = fluentqt.FlipView(globals().get("gallery_parent"))
                flip_view.setFixedSize(420, 220)
                flip_view.setShowPageIndicator(True)
                page_size = QSize(420, 220)
                for caption, from_color, to_color in (
                    ("Sunrise", "#F7975B", "#F2C94C"),
                    ("Ocean", "#1E6FD9", "#6FD1F2"),
                    ("Forest", "#2F9E44", "#A9E34B"),
                ):
                    page = QLabel(flip_view)
                    page.setPixmap(
                        make_gradient_photo(
                            page_size, from_color, to_color, caption
                        )
                    )
                    page.setAlignment(Qt.AlignmentFlag.AlignCenter)
                    flip_view.addOwnedPage(page)
                """,
                _GRADIENT_PHOTO_IMPORTS,
            ),
        ),
        "flip-view-vertical": (
            "flip_view",
            _script(
                _GRADIENT_PANE_HELPER
                + """
                flip_view = fluentqt.FlipView(globals().get("gallery_parent"))
                flip_view.setFixedSize(300, 240)
                flip_view.setOrientation(Qt.Orientation.Vertical)
                flip_view.setShowPageIndicator(True)
                for index, colors in enumerate((
                    ("#8764B8", "#C26FB8"),
                    ("#038387", "#6FD1F2"),
                    ("#CA5010", "#F2C94C"),
                ), 1):
                    page = make_gradient_pane(
                        "Vertical {0}".format(index), colors[0], colors[1]
                    )
                    page.setFixedSize(300, 240)
                    flip_view.addOwnedPage(page)
                """,
                _GRADIENT_IMPORTS,
            ),
        ),
        "flip-view-external-navigation": (
            "root",
            _script(
                _GRADIENT_PANE_HELPER
                + """
                root = QWidget()
                layout = QVBoxLayout(root)
                flip_view = fluentqt.FlipView(root)
                flip_view.setFixedSize(360, 168)
                flip_view.setShowNavigationButtons(False)
                flip_view.setShowPageIndicator(False)
                for index, colors in enumerate((
                    ("#1E6FD9", "#6FD1F2"),
                    ("#2F9E44", "#A9E34B"),
                    ("#F7975B", "#F2C94C"),
                ), 1):
                    page = make_gradient_pane(
                        "Page {0}".format(index), colors[0], colors[1]
                    )
                    page.setFixedSize(360, 168)
                    flip_view.addOwnedPage(page)
                controls = QWidget(root)
                controls_layout = QHBoxLayout(controls)
                controls_layout.setContentsMargins(0, 0, 0, 0)
                controls_layout.setSpacing(8)
                controls_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                previous = fluentqt.Button("Previous", controls)
                next_button = fluentqt.Button("Next", controls)
                next_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
                status = fluentqt.Label("Current page: 1", controls)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)

                def update_status(index, label=status):
                    label.setText("Current page: {0}".format(index + 1))

                previous.clicked.connect(flip_view.goPrevious)
                next_button.clicked.connect(flip_view.goNext)
                flip_view.currentIndexChanged.connect(update_status)
                controls_layout.addWidget(previous)
                controls_layout.addWidget(next_button)
                controls_layout.addWidget(status)
                layout.addWidget(flip_view)
                layout.addWidget(controls)
                """,
                _GRADIENT_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "data-grid",
    ("DataGrid",),
    {
        "data-grid-large-read-only": (
            "grid",
            _script(
                _DATA_GRID_MODEL_HELPER
                + """
                grid = fluentqt.DataGrid(globals().get("gallery_parent"))
                grid.setFixedSize(680, 252)
                grid.setModel(LargeDataGridModel(grid))
                grid.setScrollChainingEnabled(True)
                grid.setSelectionBehavior(
                    QAbstractItemView.SelectionBehavior.SelectRows
                )
                grid.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
                grid.horizontalHeader().setStretchLastSection(True)
                grid.setColumnWidth(0, 150)
                grid.setColumnWidth(1, 120)
                """,
                _DATA_GRID_IMPORTS,
            ),
        ),
        "data-grid-column-selection": (
            "grid",
            _script(
                _DATA_GRID_MODEL_HELPER
                + """
                grid = fluentqt.DataGrid(globals().get("gallery_parent"))
                grid.setFixedSize(680, 252)
                model = make_project_data_grid_model(grid, False)
                grid.setModel(model)
                grid.setScrollChainingEnabled(True)
                grid.setSelectionMode(fluentqt.SelectionMode.Extended)
                grid.setSelectionBehavior(
                    QAbstractItemView.SelectionBehavior.SelectRows
                )
                grid.setSortingEnabled(True)
                grid.horizontalHeader().setSectionsMovable(True)
                grid.horizontalHeader().setStretchLastSection(True)
                grid.setColumnWidth(0, 170)
                grid.setColumnWidth(1, 130)
                """,
                _DATA_GRID_IMPORTS,
            ),
        ),
        "data-grid-edit-validation": (
            "grid",
            _script(
                _DATA_GRID_VALIDATION_HELPER
                + """
                grid = fluentqt.DataGrid(globals().get("gallery_parent"))
                grid.setFixedSize(680, 224)
                model = ValidatingDataGridModel(grid)
                model.setHorizontalHeaderLabels(("Setting", "Value", "Scope"))
                for row in (
                    ("Channel", "stable", "Workspace"),
                    ("Region", "eu-west", "Account"),
                    ("Mode", "No", "Session"),
                ):
                    model.appendRow([QStandardItem(value) for value in row])
                QStandardItemModel.setData(
                    model,
                    model.index(2, 1),
                    "Use at least 3 characters",
                    DATA_GRID_VALIDATION_MESSAGE_ROLE,
                )
                grid.setModel(model)
                grid.setScrollChainingEnabled(True)
                grid.setItemDelegate(DataGridValidationDelegate(grid))
                grid.setEditTriggers(
                    QAbstractItemView.EditTrigger.DoubleClicked
                    | QAbstractItemView.EditTrigger.EditKeyPressed
                )
                grid.horizontalHeader().setStretchLastSection(True)
                grid.setColumnWidth(0, 180)
                grid.setColumnWidth(1, 220)
                grid.setCurrentIndex(model.index(2, 1))
                """,
                _DATA_GRID_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "flow-view",
    ("FlowView",),
    {
        "flow-view-basic": (
            "flow_view",
            _script(
                _PHOTO_MODEL_HELPER
                + """
                flow_view = fluentqt.FlowView(globals().get("gallery_parent"))
                flow_view.setFixedSize(540, 282)
                flow_view.setSelectionMode(fluentqt.SelectionMode.Single)
                flow_view.setDefaultItemSize(QSize(160, 118))
                flow_view.setMinimumItemSize(QSize(140, 100))
                flow_view.setMaximumItemSize(QSize(180, 128))
                flow_view.setHorizontalSpacing(10)
                flow_view.setVerticalSpacing(10)
                flow_view.setContentMargins(QMargins(8, 8, 8, 8))
                photo_delegate = PhotoDelegate(flow_view)
                flow_view.setItemDelegate(photo_delegate)
                model = make_photo_model((
                    ("Atrium", "architecture", "#378BC4", "#9AD9EA", QSize(160, 118)),
                    ("Harbor", "travel", "#1E6FD9", "#67D0D6", QSize(160, 118)),
                    ("Canyon", "landscape", "#C25B2B", "#F2B84B", QSize(160, 118)),
                    ("Studio", "workspace", "#7A5FC9", "#D794E6", QSize(160, 118)),
                    ("Garden", "nature", "#2F9E44", "#A9E34B", QSize(160, 118)),
                    ("Dawn", "morning", "#D96C75", "#F2C96B", QSize(160, 118)),
                    ("Transit", "city", "#375BA8", "#58B7E8", QSize(160, 118)),
                    ("Market", "street", "#91434A", "#E09552", QSize(160, 118)),
                    ("Cabin", "retreat", "#5B7535", "#CEA85B", QSize(160, 118)),
                ), flow_view, True)
                flow_view.setModel(model)
                flow_view.setSelectedIndex(0)
                """,
                _MODEL_IMPORTS,
            ),
        ),
        "flow-view-reorder": (
            "flow_view",
            _script(
                _PHOTO_MODEL_HELPER
                + """
                flow_view = fluentqt.FlowView(globals().get("gallery_parent"))
                flow_view.setFixedSize(540, 318)
                flow_view.setCanReorderItems(True)
                flow_view.setDefaultItemSize(QSize(150, 110))
                flow_view.setMinimumItemSize(QSize(112, 92))
                flow_view.setHorizontalSpacing(10)
                flow_view.setVerticalSpacing(10)
                flow_view.setContentMargins(QMargins(8, 8, 8, 8))
                photo_delegate = PhotoDelegate(flow_view)
                flow_view.setItemDelegate(photo_delegate)
                model = make_photo_model((
                    ("Loft", "interior", "#5D7FB8", "#C9DBF2", QSize(148, 104)),
                    ("Ridge", "wide", "#24748F", "#8CCFA5", QSize(210, 118)),
                    ("Cafe", "street", "#A95B45", "#E9B87A", QSize(132, 132)),
                    ("Mist", "forest", "#3C7552", "#AFC98E", QSize(172, 148)),
                    ("Gallery", "exhibit", "#666A86", "#D9D7EA", QSize(122, 104)),
                    ("Canal", "water", "#2D73A3", "#9AD8E5", QSize(190, 126)),
                    ("Trail", "nature", "#70832F", "#E2C458", QSize(156, 116)),
                    ("Arcade", "night", "#573A9B", "#DC72C8", QSize(198, 140)),
                    ("Pier", "coast", "#2F808F", "#E0C17E", QSize(136, 106)),
                ), flow_view, True)
                flow_view.setModel(model)
                flow_view.setSelectedIndex(1)
                """,
                _MODEL_IMPORTS,
            ),
        ),
        "flow-view-scroll-bounce": (
            "flow_view",
            _script(
                _PHOTO_MODEL_HELPER
                + """
                flow_view = fluentqt.FlowView(globals().get("gallery_parent"))
                flow_view.setFixedSize(420, 238)
                flow_view.setHeaderText("Contained flow")
                flow_view.setDefaultItemSize(QSize(126, 88))
                flow_view.setMinimumItemSize(QSize(112, 80))
                flow_view.setMaximumItemSize(QSize(146, 102))
                flow_view.setHorizontalSpacing(10)
                flow_view.setVerticalSpacing(10)
                flow_view.setContentMargins(QMargins(8, 8, 8, 8))
                flow_view.setScrollChainingEnabled(False)
                flow_view.setOverscrollEnabled(True)
                flow_view.setItemDelegate(PhotoDelegate(flow_view))
                palette = tuple(
                    QColor(value)
                    for value in (
                        "#0078D4", "#038387", "#CA5010",
                        "#8764B8", "#C239B3", "#498205",
                    )
                )
                photos = []
                for index in range(18):
                    photos.append((
                        f"Tile {index + 1}",
                        "",
                        palette[index % len(palette)],
                        palette[(index + 2) % len(palette)].lighter(135),
                        QSize(126 + (index % 3) * 12, 88 + (index % 2) * 12),
                    ))
                model = make_photo_model(photos, flow_view)
                flow_view.setModel(model)
                flow_view.setSelectedIndex(0)
                """,
                _MODEL_IMPORTS,
            ),
        ),
        "flow-view-placeholder": (
            "flow_view",
            _script(
                """
                flow_view = fluentqt.FlowView(globals().get("gallery_parent"))
                flow_view.setFixedSize(420, 178)
                flow_view.setHeaderText("Uploads")
                flow_view.setPlaceholderText("No queued uploads")
                flow_view.setModel(QStandardItemModel(flow_view))
                """,
                "from PySide6.QtGui import QStandardItemModel",
            ),
        ),
    },
)


register_source_samples(
    "grid-view",
    ("GridView",),
    {
        "grid-view-basic": (
            "grid_view",
            _script(
                _PHOTO_MODEL_HELPER
                + """
                grid_view = fluentqt.GridView(globals().get("gallery_parent"))
                grid_view.setFixedSize(508, 256)
                grid_view.setCellSize(QSize(150, 112))
                grid_view.setMaxColumns(3)
                grid_view.setHorizontalSpacing(10)
                grid_view.setVerticalSpacing(10)
                photo_delegate = PhotoDelegate(grid_view, QSize(150, 112))
                grid_view.setItemDelegate(photo_delegate)
                model = make_photo_model((
                    ("Sunrise", "Warm", "#F7975B", "#F2C94C", QSize(150, 112)),
                    ("Ocean", "Blue", "#1E6FD9", "#6FD1F2", QSize(150, 112)),
                    ("Forest", "Green", "#2F9E44", "#A9E34B", QSize(150, 112)),
                    ("Dusk", "Violet", "#6B4FA2", "#C26FB8", QSize(150, 112)),
                    ("Desert", "Amber", "#C86B2D", "#E8C06E", QSize(150, 112)),
                    ("Glacier", "Ice", "#3D8BA3", "#B3E5E8", QSize(150, 112)),
                    ("Meadow", "Spring", "#6FA82F", "#D4E67A", QSize(150, 112)),
                    ("Harbor", "Teal", "#1C6E8C", "#73C8D0", QSize(150, 112)),
                ), grid_view, True)
                grid_view.setModel(model)
                grid_view.setSelectedIndex(0)
                """,
                _MODEL_IMPORTS,
            ),
        ),
        "grid-view-multi-select": (
            "grid_view",
            _script(
                _PHOTO_MODEL_HELPER
                + """
                grid_view = fluentqt.GridView(globals().get("gallery_parent"))
                grid_view.setFixedSize(508, 256)
                grid_view.setCellSize(QSize(150, 112))
                grid_view.setMaxColumns(3)
                grid_view.setHorizontalSpacing(10)
                grid_view.setVerticalSpacing(10)
                grid_view.setSelectionMode(fluentqt.SelectionMode.Multiple)
                grid_view.setItemDelegate(
                    PhotoDelegate(grid_view, QSize(150, 112), True)
                )
                model = make_photo_model((
                    ("Sunrise", "Warm", "#F7975B", "#F2C94C", QSize(150, 112)),
                    ("Ocean", "Blue", "#1E6FD9", "#6FD1F2", QSize(150, 112)),
                    ("Forest", "Green", "#2F9E44", "#A9E34B", QSize(150, 112)),
                    ("Dusk", "Violet", "#6B4FA2", "#C26FB8", QSize(150, 112)),
                    ("Desert", "Amber", "#C86B2D", "#E8C06E", QSize(150, 112)),
                    ("Glacier", "Ice", "#3D8BA3", "#B3E5E8", QSize(150, 112)),
                    ("Meadow", "Spring", "#6FA82F", "#D4E67A", QSize(150, 112)),
                    ("Harbor", "Teal", "#1C6E8C", "#73C8D0", QSize(150, 112)),
                ), grid_view, True)
                grid_view.setModel(model)
                selection = grid_view.selectionModel()
                for row in (0, 2, 5):
                    selection.select(
                        model.index(row, 0),
                        QItemSelectionModel.SelectionFlag.Select,
                    )
                """,
                _MODEL_IMPORTS,
            ),
        ),
        "grid-view-reorder": (
            "grid_view",
            _script(
                _PHOTO_MODEL_HELPER
                + """
                grid_view = fluentqt.GridView(globals().get("gallery_parent"))
                grid_view.setFixedSize(508, 256)
                grid_view.setCellSize(QSize(150, 112))
                grid_view.setMaxColumns(3)
                grid_view.setHorizontalSpacing(10)
                grid_view.setVerticalSpacing(10)
                grid_view.setSelectionMode(fluentqt.SelectionMode.Multiple)
                grid_view.setCanReorderItems(True)
                grid_view.setItemDelegate(
                    PhotoDelegate(grid_view, QSize(150, 112), True)
                )
                model = make_photo_model((
                    ("Sunrise", "Warm", "#F7975B", "#F2C94C", QSize(150, 112)),
                    ("Ocean", "Blue", "#1E6FD9", "#6FD1F2", QSize(150, 112)),
                    ("Forest", "Green", "#2F9E44", "#A9E34B", QSize(150, 112)),
                    ("Dusk", "Violet", "#6B4FA2", "#C26FB8", QSize(150, 112)),
                    ("Desert", "Amber", "#C86B2D", "#E8C06E", QSize(150, 112)),
                    ("Glacier", "Ice", "#3D8BA3", "#B3E5E8", QSize(150, 112)),
                    ("Meadow", "Spring", "#6FA82F", "#D4E67A", QSize(150, 112)),
                    ("Harbor", "Teal", "#1C6E8C", "#73C8D0", QSize(150, 112)),
                ), grid_view, True)
                grid_view.setModel(model)
                selection = grid_view.selectionModel()
                for row in (1, 3):
                    selection.select(
                        model.index(row, 0),
                        QItemSelectionModel.SelectionFlag.Select,
                    )
                """,
                _MODEL_IMPORTS,
            ),
        ),
        "grid-view-scroll-bounce": (
            "grid_view",
            _script(
                _PHOTO_MODEL_HELPER
                + """
                grid_view = fluentqt.GridView(globals().get("gallery_parent"))
                grid_view.setFixedSize(410, 238)
                grid_view.setHeaderText("Contained grid")
                grid_view.setScrollChainingEnabled(False)
                grid_view.setOverscrollEnabled(True)
                grid_view.setCellSize(QSize(118, 88))
                grid_view.setMaxColumns(3)
                grid_view.setHorizontalSpacing(10)
                grid_view.setVerticalSpacing(10)
                grid_view.setItemDelegate(
                    PhotoDelegate(grid_view, QSize(118, 88))
                )
                palette = tuple(
                    QColor(value)
                    for value in (
                        "#0078D4", "#038387", "#CA5010",
                        "#8764B8", "#C239B3", "#498205",
                    )
                )
                photos = []
                for index in range(18):
                    photos.append((
                        f"Cell {index + 1}",
                        "",
                        palette[index % len(palette)],
                        palette[(index + 3) % len(palette)].lighter(135),
                        QSize(118, 88),
                    ))
                model = make_photo_model(photos, grid_view)
                grid_view.setModel(model)
                grid_view.setSelectedIndex(0)
                """,
                _MODEL_IMPORTS,
            ),
        ),
        "grid-view-placeholder": (
            "grid_view",
            _script(
                """
                grid_view = fluentqt.GridView(globals().get("gallery_parent"))
                grid_view.setFixedSize(410, 178)
                grid_view.setHeaderText("Pinned photos")
                grid_view.setPlaceholderText("No pinned photos")
                grid_view.setModel(QStandardItemModel(grid_view))
                """,
                "from PySide6.QtGui import QStandardItemModel",
            ),
        ),
    },
)


register_source_samples(
    "split-view",
    ("SplitView", "SplitViewPaneOptions"),
    {
        "split-view-basic": (
            "split_view",
            _script(
                _GRADIENT_PANE_HELPER
                + """
                split_view = fluentqt.SplitView(globals().get("gallery_parent"))
                split_view.setFixedSize(460, 168)
                split_view.addOwnedPane(
                    make_gradient_pane("Pane 1", "#1E6FD9", "#6FD1F2")
                )
                split_view.addOwnedPane(
                    make_gradient_pane("Pane 2", "#2F9E44", "#A9E34B")
                )
                split_view.addOwnedPane(
                    make_gradient_pane("Pane 3", "#F7975B", "#F2C94C")
                )
                for index in range(3):
                    split_view.setPaneMinimumSize(index, 96)
                split_view.setPanePreferredSize(0, 150)
                split_view.setPanePreferredSize(1, 150)
                split_view.setPaneFill(2, True)
                """,
                _GRADIENT_IMPORTS,
            ),
        ),
        "split-view-vertical-constraints": (
            "split_view",
            _script(
                _GRADIENT_PANE_HELPER
                + """
                split_view = fluentqt.SplitView(globals().get("gallery_parent"))
                split_view.setFixedSize(360, 320)
                split_view.setOrientation(Qt.Orientation.Vertical)
                split_view.setHandleWidth(6)
                split_view.addOwnedPane(
                    make_gradient_pane("Top 110", "#1E6FD9", "#6FD1F2"),
                    fluentqt.SplitViewPaneOptions(80, 110, 150, False),
                )
                split_view.addOwnedPane(
                    make_gradient_pane("Fill pane", "#2F9E44", "#A9E34B"),
                    fluentqt.SplitViewPaneOptions(90, 160, 500, True),
                )
                split_view.addOwnedPane(
                    make_gradient_pane("Bottom max 140", "#F7975B", "#F2C94C"),
                    fluentqt.SplitViewPaneOptions(60, 120, 140, False),
                )
                """,
                _GRADIENT_IMPORTS,
            ),
        ),
        "split-view-hidden-pane": (
            "root",
            _script(
                _GRADIENT_PANE_HELPER
                + """
                root = QWidget()
                layout = QVBoxLayout(root)
                split_view = fluentqt.SplitView(root)
                split_view.setFixedSize(460, 160)
                first = make_gradient_pane("Nav", "#1E6FD9", "#6FD1F2")
                details = make_gradient_pane("Details", "#8764B8", "#C26FB8")
                fill = make_gradient_pane("Content", "#2F9E44", "#A9E34B")
                split_view.addOwnedPane(first, fluentqt.SplitViewPaneOptions(60, 120, 240, False))
                split_view.addOwnedPane(details, fluentqt.SplitViewPaneOptions(60, 150, 260, False))
                split_view.addOwnedPane(fill, fluentqt.SplitViewPaneOptions(60, 180, 500, True))

                controls = QWidget(root)
                controls_layout = QHBoxLayout(controls)
                controls_layout.setContentsMargins(0, 0, 0, 0)
                controls_layout.setSpacing(8)
                controls_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                toggle = fluentqt.Button("Hide details", controls)
                toggle.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
                status = fluentqt.Label("Pane count: 3, details visible", controls)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)

                def toggle_details():
                    visible = details.isHidden()
                    details.setVisible(visible)
                    toggle.setText("Hide details" if visible else "Show details")
                    status.setText(
                        "Pane count: 3, details visible"
                        if visible
                        else "Pane count: 3, details hidden"
                    )

                toggle.clicked.connect(toggle_details)
                controls_layout.addWidget(toggle)
                controls_layout.addWidget(status)
                layout.addWidget(split_view)
                layout.addWidget(controls)
                """,
                _GRADIENT_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "stack-view",
    ("StackView",),
    {
        "stack-view-basic": (
            "root",
            _script(
                _GRADIENT_PANE_HELPER
                + """
                root = QWidget()
                layout = QVBoxLayout(root)
                stack_view = fluentqt.StackView(root)
                stack_view.setFixedSize(360, 150)
                stack_view.setInitialOwnedItem(
                    make_gradient_pane("Page 1", "#1E6FD9", "#6FD1F2")
                )
                controls = QWidget(root)
                controls_layout = QHBoxLayout(controls)
                controls_layout.setContentsMargins(0, 0, 0, 0)
                controls_layout.setSpacing(8)
                controls_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                push = fluentqt.Button("Push page", controls)
                push.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
                pop = fluentqt.Button("Pop page", controls)
                status = fluentqt.Label("Depth: 1", controls)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                palette = (
                    ("#2F9E44", "#A9E34B"),
                    ("#F7975B", "#F2C94C"),
                    ("#8764B8", "#C26FB8"),
                )

                def push_page():
                    depth = stack_view.depth()
                    colors = palette[(depth - 1) % len(palette)]
                    stack_view.pushOwnedItem(
                        make_gradient_pane(
                            "Page {0}".format(depth + 1), colors[0], colors[1]
                        )
                    )

                push.clicked.connect(push_page)
                pop.clicked.connect(stack_view.pop)
                stack_view.depthChanged.connect(
                    lambda depth, label=status: label.setText(
                        "Depth: {0}".format(depth)
                    )
                )
                controls_layout.addWidget(push)
                controls_layout.addWidget(pop)
                controls_layout.addWidget(status)
                layout.addWidget(stack_view)
                layout.addWidget(controls)
                """,
                _GRADIENT_IMPORTS,
            ),
        ),
        "stack-view-transition-type": (
            "root",
            _script(
                _GRADIENT_PANE_HELPER
                + """
                root = QWidget()
                layout = QVBoxLayout(root)
                stack_view = fluentqt.StackView(root)
                stack_view.setFixedSize(360, 150)
                stack_view.setTransitionDuration(220)
                stack_view.setTransitionType(
                    fluentqt.StackView.StackViewTransitionType.ScaleFade
                )
                stack_view.setInitialOwnedItem(
                    make_gradient_pane("Root", "#1E6FD9", "#6FD1F2")
                )
                controls = QWidget(root)
                controls_layout = QHBoxLayout(controls)
                controls_layout.setContentsMargins(0, 0, 0, 0)
                controls_layout.setSpacing(8)
                controls_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                scale = fluentqt.Button("ScaleFade", controls)
                slide = fluentqt.Button("SlideFade", controls)
                push = fluentqt.Button("Push", controls)
                push.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
                pop = fluentqt.Button("Pop", controls)
                scale.clicked.connect(
                    lambda: stack_view.setTransitionType(
                        fluentqt.StackView.StackViewTransitionType.ScaleFade
                    )
                )
                slide.clicked.connect(
                    lambda: stack_view.setTransitionType(
                        fluentqt.StackView.StackViewTransitionType.SlideFade
                    )
                )
                push.clicked.connect(
                    lambda: stack_view.pushOwnedItem(
                        make_gradient_pane(
                            "Page {0}".format(stack_view.depth() + 1),
                            "#8764B8",
                            "#C26FB8",
                        )
                    )
                )
                pop.clicked.connect(stack_view.pop)
                controls_layout.addWidget(scale)
                controls_layout.addWidget(slide)
                controls_layout.addWidget(push)
                controls_layout.addWidget(pop)
                layout.addWidget(stack_view)
                layout.addWidget(controls)
                """,
                _GRADIENT_IMPORTS,
            ),
        ),
        "stack-view-replace-pop-to-root": (
            "root",
            _script(
                _GRADIENT_PANE_HELPER
                + """
                root = QWidget()
                layout = QVBoxLayout(root)
                stack_view = fluentqt.StackView(root)
                stack_view.setFixedSize(360, 150)
                stack_view.setTransitionAnimationEnabled(False)
                stack_view.setInitialOwnedItem(
                    make_gradient_pane("Root", "#1E6FD9", "#6FD1F2")
                )
                stack_view.pushOwnedItem(
                    make_gradient_pane("Details", "#2F9E44", "#A9E34B")
                )
                controls = QWidget(root)
                controls_layout = QHBoxLayout(controls)
                controls_layout.setContentsMargins(0, 0, 0, 0)
                controls_layout.setSpacing(8)
                controls_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                replace = fluentqt.Button("Replace current", controls)
                replace.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
                to_root = fluentqt.Button("Pop to root", controls)
                status = fluentqt.Label("Depth: 2", controls)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                replace.clicked.connect(
                    lambda: stack_view.replaceOwnedItem(
                        make_gradient_pane("Replacement", "#F7975B", "#F2C94C")
                    )
                )
                to_root.clicked.connect(stack_view.popToRoot)
                stack_view.depthChanged.connect(
                    lambda depth, label=status: label.setText(
                        "Depth: {0}".format(depth)
                    )
                )
                controls_layout.addWidget(replace)
                controls_layout.addWidget(to_root)
                controls_layout.addWidget(status)
                layout.addWidget(stack_view)
                layout.addWidget(controls)
                """,
                _GRADIENT_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "tree-view",
    ("TreeView",),
    {
        "tree-view-basic": (
            "tree",
            _script(
                _TREE_MODEL_HELPER
                + _TREE_DELEGATE_HELPER
                + dedent("""
                tree = fluentqt.TreeView(globals().get("gallery_parent"))
                tree.setHeaderHidden(True)
                tree.setFixedHeight(252)
                tree.setBackgroundVisible(False)
                tree.setBorderVisible(False)
                row_delegate = TreeRowDelegate(tree, 36)
                tree.setItemDelegate(row_delegate)
                model = make_tree_model(tree)
                tree.setModel(model)
                tree.expandAll()
                tree.setSelectedItem(model.index(0, 0))
                """),
                _MODEL_IMPORTS,
            ),
        ),
        "tree-view-checkboxes": (
            "tree",
            _script(
                _TREE_MODEL_HELPER
                + _TREE_DELEGATE_HELPER
                + dedent("""
                tree = fluentqt.TreeView(globals().get("gallery_parent"))
                tree.setHeaderHidden(True)
                tree.setFixedHeight(258)
                tree.setBackgroundVisible(False)
                tree.setBorderVisible(False)
                row_delegate = TreeRowDelegate(tree, 36)
                row_delegate.setCheckBoxVisible(True)
                tree.setItemDelegate(row_delegate)

                def leaf(text, state, glyph, color):
                    item = make_tree_item(text, glyph, color)
                    item.setCheckable(True)
                    item.setCheckState(state)
                    return item

                def group(text, state):
                    item = make_tree_item(text, "\ue838", "#CA8A1A")
                    item.setCheckable(True)
                    item.setCheckState(state)
                    return item

                model = QStandardItemModel(tree)
                sync = group("Sync these settings", Qt.CheckState.PartiallyChecked)
                sync.appendRow(leaf("Passwords", Qt.CheckState.Checked, "\ue718", "#498205"))
                sync.appendRow(leaf("Bookmarks", Qt.CheckState.Checked, "\ue734", "#CA5010"))
                sync.appendRow(leaf("History", Qt.CheckState.Unchecked, "\ue787", "#8764B8"))
                model.appendRow(sync)

                notify = group("Notifications", Qt.CheckState.Checked)
                notify.appendRow(leaf("Email", Qt.CheckState.Checked, "\ue715", "#0078D4"))
                notify.appendRow(leaf("Messages", Qt.CheckState.Checked, "\ue8bd", "#038387"))
                model.appendRow(notify)

                privacy = group("Privacy", Qt.CheckState.Unchecked)
                privacy.appendRow(leaf("Location", Qt.CheckState.Unchecked, "\ue707", "#D83B01"))
                privacy.appendRow(leaf("Camera", Qt.CheckState.Unchecked, "\ue722", "#2D7D9A"))
                privacy.appendRow(leaf("Microphone", Qt.CheckState.Unchecked, "\ue720", "#5C2D91"))
                model.appendRow(privacy)

                tree.setModel(model)
                tree.expandAll()
                """),
                _MODEL_IMPORTS,
            ),
        ),
        "tree-view-reorder": (
            "tree",
            _script(
                _TREE_MODEL_HELPER
                + _TREE_DELEGATE_HELPER
                + dedent("""
                tree = fluentqt.TreeView(globals().get("gallery_parent"))
                tree.setHeaderHidden(True)
                tree.setFixedHeight(252)
                tree.setBackgroundVisible(False)
                tree.setBorderVisible(False)
                tree.setCanReorderItems(True)
                row_delegate = TreeRowDelegate(tree, 36)
                tree.setItemDelegate(row_delegate)
                model = make_tree_model(tree)
                tree.setModel(model)
                tree.expandAll()
                """),
                _MODEL_IMPORTS,
            ),
        ),
        "tree-view-indicator-motion": (
            "root",
            _script(
                _TREE_MODEL_HELPER
                + _TREE_DELEGATE_HELPER
                + dedent("""
                root = QWidget()
                layout = QVBoxLayout(root)
                tree = fluentqt.TreeView(root)
                tree.setHeaderHidden(True)
                tree.setFixedHeight(238)
                tree.setBackgroundVisible(False)
                tree.setBorderVisible(False)
                tree.setSelectionIndicatorVisible(True)
                tree.setIndicatorMotionAnimationEnabled(True)
                row_delegate = TreeRowDelegate(tree, 36)
                tree.setItemDelegate(row_delegate)
                model = make_tree_model(tree)
                tree.setModel(model)
                tree.expandAll()
                parent_index = model.index(0, 0)
                child_index = model.index(0, 0, parent_index)
                sibling_index = model.index(1, 0, parent_index)
                tree.setSelectedItem(parent_index)

                controls = QWidget(root)
                controls_layout = QHBoxLayout(controls)
                controls_layout.setContentsMargins(0, 0, 0, 0)
                controls_layout.setSpacing(8)
                controls_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                for text, index in (
                    ("Parent", parent_index),
                    ("Child", child_index),
                    ("Sibling", sibling_index),
                ):
                    button = fluentqt.Button(text, controls)
                    button.clicked.connect(
                        lambda _checked=False, value=index: tree.setSelectedItem(value)
                    )
                    controls_layout.addWidget(button)

                status = fluentqt.Label("Transition: none", controls)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setText("Transition: same level")
                status.setMinimumWidth(max(status.minimumWidth(), status.sizeHint().width()))
                status.setText("Transition: none")
                controls_layout.addWidget(status)

                def update_status():
                    transition = tree.indicatorHierarchyTransition()
                    values = {
                        fluentqt.TreeView.IndicatorHierarchyTransition.Inward: "inward",
                        fluentqt.TreeView.IndicatorHierarchyTransition.Outward: "outward",
                        fluentqt.TreeView.IndicatorHierarchyTransition.SameLevel: "same level",
                        fluentqt.TreeView.IndicatorHierarchyTransition.None_: "none",
                    }
                    status.setText("Transition: {0}".format(values.get(transition, "none")))

                tree.indicatorHierarchyTransitionChanged.connect(
                    update_status
                )
                update_status()
                layout.addWidget(tree)
                layout.addWidget(controls)
                """),
                _MODEL_IMPORTS,
            ),
        ),
        "tree-view-scroll-bounce": (
            "tree",
            _script(
                _TREE_MODEL_HELPER
                + _TREE_DELEGATE_HELPER
                + dedent("""
                tree = fluentqt.TreeView(globals().get("gallery_parent"))
                tree.setHeaderHidden(True)
                tree.setFixedHeight(258)
                tree.setBackgroundVisible(False)
                tree.setBorderVisible(False)
                tree.setScrollChainingEnabled(False)
                tree.setOverscrollEnabled(True)
                row_delegate = TreeRowDelegate(tree, 36)
                tree.setItemDelegate(row_delegate)
                model = QStandardItemModel(tree)
                for folder_index in range(9):
                    folder = make_tree_item(
                        "Folder {0}".format(folder_index + 1),
                        "\ue838",
                        "#CA8A1A",
                    )
                    for file_index in range(3):
                        folder.appendRow(make_tree_item(
                            "Document {0}.{1}".format(folder_index + 1, file_index + 1),
                            "\ue8a5",
                            "#528BC4",
                        ))
                    model.appendRow(folder)
                tree.setModel(model)
                tree.expandAll()
                tree.setSelectedItem(model.index(0, 0))
                """),
                _MODEL_IMPORTS,
            ),
        ),
    },
)
