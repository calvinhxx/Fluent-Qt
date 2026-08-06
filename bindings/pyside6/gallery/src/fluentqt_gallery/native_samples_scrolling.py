"""Standalone Gallery ports of native scrolling SampleCards."""

from __future__ import annotations

from textwrap import dedent

from .native_samples import register_source_samples


def _script(body: str, imports: str = "") -> str:
    prefix = "import fluentqt\n"
    if imports:
        prefix += imports.strip() + "\n"
    return prefix + "\n" + dedent(body).strip() + "\n"


_WIDGETS = "from PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget"

_PAINTED_SCROLLING_IMPORTS = (
    "from PySide6.QtCore import QRect, QRectF, QSize, QSizeF, Qt\n"
    "from PySide6.QtGui import (QColor, QFont, QGuiApplication, QLinearGradient, QPainter, QPainterPath, QPen, QPixmap)\n"
    "from PySide6.QtWidgets import (QHBoxLayout, QLabel, QSizePolicy, QVBoxLayout, QWidget)\n"
    "from fluentqt_gallery.foundation_pages import (_theme_snapshot, _theme_tokens)"
)

_PAGER_PICTURE_HELPER = dedent(
    """
    class ScrollingSampleSurface(QWidget):
        def __init__(self, parent=None, spacing=12):
            super().__init__(parent)
            self.setSizePolicy(
                QSizePolicy.Policy.MinimumExpanding,
                QSizePolicy.Policy.Fixed,
            )
            layout = QVBoxLayout(self)
            layout.setContentsMargins(16, 14, 16, 16)
            layout.setSpacing(spacing)
            layout.setAlignment(
                Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
            )
            self.setProperty(
                "fluentSurfaceColor", _theme_tokens(self)["bgCanvas"]
            )

        def paintEvent(self, event):
            del event
            colors = _theme_tokens(self)
            self.setProperty("fluentSurfaceColor", colors["bgCanvas"])
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setPen(QPen(colors["strokeCard"], 1.0))
            painter.setBrush(colors["bgCanvas"])
            painter.drawRoundedRect(
                QRectF(self.rect()).adjusted(0.0, 0.0, -1.0, -1.0),
                8.0,
                8.0,
            )


    def make_scrolling_surface(parent=None, spacing=12):
        return ScrollingSampleSurface(parent, spacing)


    def make_pager_picture_canvas(size):
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


    class PagerPicture(QLabel):
        def __init__(self, caption, from_color, to_color, size=None, parent=None):
            super().__init__(parent)
            logical_size = size if size is not None else QSize(420, 220)
            self.setFixedSize(logical_size)
            pixmap = make_pager_picture_canvas(logical_size)
            painter = QPainter(pixmap)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
            surface = QRectF(
                0.0,
                0.0,
                float(logical_size.width()),
                float(logical_size.height()),
            )
            gradient = QLinearGradient(surface.topLeft(), surface.bottomRight())
            gradient.setColorAt(0.0, QColor(from_color))
            gradient.setColorAt(1.0, QColor(to_color))
            path = QPainterPath()
            path.addRoundedRect(surface, 8.0, 8.0)
            painter.fillPath(path, gradient)
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
            self.setPixmap(pixmap)


    def make_status_label(parent, text):
        label = fluentqt.Label(text, parent)
        label.setFluentTypography(fluentqt.FontRole.Body)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        label.setWordWrap(True)
        return label


    def make_sample_button(parent, text):
        button = fluentqt.Button(text, parent)
        button.setFluentSize(fluentqt.Button.ButtonSize.Small)
        button.setMinimumWidth(74)
        return button


    def horizontal_group(parent, spacing=8):
        group = QWidget(parent)
        layout = QHBoxLayout(group)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(spacing)
        layout.setAlignment(
            Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
        )
        return group


    def vertical_group(parent, spacing=8):
        group = QWidget(parent)
        layout = QVBoxLayout(group)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(spacing)
        layout.setAlignment(
            Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
        )
        return group


    def set_button_active(button, active):
        button.setFluentStyle(
            fluentqt.Button.ButtonStyle.Accent
            if active
            else fluentqt.Button.ButtonStyle.Standard
        )


    def refresh_pager_fixed_size(pager):
        pager.setFixedSize(pager.sizeHint())


    def pager_picture_pages():
        return (
            ("Coastal route", "#375F90", "#B9D7EA"),
            ("Forest trail", "#3F7B52", "#C8E3B4"),
            ("Desert light", "#9A6339", "#F0CF9A"),
            ("Evening ridge", "#7B4F91", "#D9C2EA"),
            ("Harbor morning", "#4B6D73", "#C7E6E4"),
            ("Mountain dusk", "#8C4F4F", "#E9C7C7"),
        )


    def make_scroll_view_stack_content(parent=None, image_size=None):
        size = QSize(360, 130) if image_size is None else QSize(image_size)
        pages = pager_picture_pages()
        margin = 12
        spacing = 10
        content = QWidget(parent)
        content.setFixedSize(
            size.width() + margin * 2,
            margin * 2 + len(pages) * size.height()
            + (len(pages) - 1) * spacing,
        )
        layout = QVBoxLayout(content)
        layout.setContentsMargins(margin, margin, margin, margin)
        layout.setSpacing(spacing)
        layout.setAlignment(
            Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
        )
        for index, page in enumerate(pages, 1):
            layout.addWidget(PagerPicture(
                "{0}  Page {1} of {2}".format(page[0], index, len(pages)),
                page[1],
                page[2],
                size,
                content,
            ))
        return content


    def scroll_view_offset_text(scroll_view):
        return "Offset: {0}, {1} / Range: {2}, {3} / Zoom: {4:.1f}x".format(
            scroll_view.horizontalOffset(),
            scroll_view.verticalOffset(),
            scroll_view.scrollableWidth(),
            scroll_view.scrollableHeight(),
            scroll_view.zoomFactor(),
        )


    def bind_scroll_view_status(scroll_view, status):
        def update_status(*unused):
            del unused
            status.setText(scroll_view_offset_text(scroll_view))
        scroll_view.scrollPositionChanged.connect(update_status)
        scroll_view.zoomFactorChanged.connect(update_status)
        update_status()
    """
)

_SCROLL_CANVAS_HELPER = dedent(
    """
    class ScrollViewDemoCanvas(fluentqt.ScrollViewZoomAwareWidget):
        def __init__(self, logical_size, title, parent=None):
            super().__init__(parent)
            self._logical_size = QSize(logical_size)
            self._title = title
            self._zoom_factor = 1.0
            self.setFixedSize(self._logical_size)

        def scrollViewUnscaledSize(self):
            return QSizeF(self._logical_size)

        def setScrollViewZoomFactor(self, factor):
            self._zoom_factor = float(factor)
            self.resize(
                round(self._logical_size.width() * self._zoom_factor),
                round(self._logical_size.height() * self._zoom_factor),
            )
            self.update()

        def paintEvent(self, event):
            del event
            snapshot = _theme_snapshot(self)
            colors = snapshot["colors"]
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
            painter.fillRect(self.rect(), colors["bgLayer"])
            painter.scale(self._zoom_factor, self._zoom_factor)

            painter.setPen(QPen(colors["strokeDivider"], 1.0))
            for x in range(0, self._logical_size.width(), 48):
                painter.drawLine(x, 0, x, self._logical_size.height())
            for y in range(0, self._logical_size.height(), 48):
                painter.drawLine(0, y, self._logical_size.width(), y)

            swatches = (
                colors["accentDefault"],
                colors["systemSuccess"],
                colors["systemCaution"],
                colors["systemInfo"],
            )
            for index in range(18):
                col = index % 6
                row = index // 6
                tile = QRect(28 + col * 102, 76 + row * 96, 72, 64)
                fill = QColor(swatches[index % len(swatches)])
                dark = int(snapshot["theme"]) == int(fluentqt.Theme.Dark)
                fill.setAlphaF(0.72 if dark else 0.88)
                painter.setBrush(fill)
                painter.setPen(Qt.PenStyle.NoPen)
                painter.drawRoundedRect(tile, 4.0, 4.0)

            title_font = fluentqt.font_for_role(fluentqt.FontRole.BodyStrong)
            title_font.setWeight(QFont.Weight.DemiBold)
            painter.setFont(title_font)
            painter.setPen(colors["textPrimary"])
            painter.drawText(
                QRect(24, 20, self._logical_size.width() - 48, 28),
                Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter,
                self._title,
            )
            painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.Caption))
            painter.setPen(colors["textSecondary"])
            painter.drawText(
                QRect(24, 46, self._logical_size.width() - 48, 22),
                Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter,
                "{0} x {1}, zoom {2:.1f}x".format(
                    self._logical_size.width(),
                    self._logical_size.height(),
                    self._zoom_factor,
                ),
            )
    """
)


_ANNOTATED_SCROLLBAR_HELPER = dedent(
    """
    def annotated_status_label(parent, text):
        label = fluentqt.Label(text, parent)
        label.setFluentTypography(fluentqt.FontRole.Body)
        label.setWordWrap(True)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        return label


    def year_labels():
        return [
            fluentqt.AnnotatedScrollBarLabel(
                str(year),
                (2023 - year) * 120,
                "October {0}".format(year),
            )
            for year in range(2023, 2014, -1)
        ]


    def year_detail_for_offset(offset):
        year = 2023 - max(0, min(offset // 120, 8))
        return "October {0}".format(year)


    def label_text_for_offset(labels, offset):
        if not labels:
            return ""
        current = labels[0].text
        for label in labels:
            if offset >= label.offset:
                current = label.text
        return current


    COLOR_SECTIONS = (
        ("Azure", QColor("#0078D4"), 32),
        ("Crimson", QColor("#DC143C"), 50),
        ("Cyan", QColor("#00B7C3"), 8),
        ("Fuchsia", QColor("#C239B3"), 70),
        ("Gold", QColor("#FFB900"), 90),
    )
    ANNOTATED_ITEM_WIDTH = 120
    ANNOTATED_ITEM_HEIGHT = 90
    ANNOTATED_VIEWPORT_WIDTH = 380
    ANNOTATED_CONTENT_WIDTH = ANNOTATED_VIEWPORT_WIDTH


    def items_per_row_for_width(width):
        return max(1, width // ANNOTATED_ITEM_WIDTH)


    def offset_for_item(item_index, width):
        return ANNOTATED_ITEM_HEIGHT * (
            item_index // items_per_row_for_width(width)
        )


    def color_content_height_for_width(width):
        total = sum(section[2] for section in COLOR_SECTIONS)
        per_row = items_per_row_for_width(width)
        return ((total + per_row - 1) // per_row) * ANNOTATED_ITEM_HEIGHT


    def color_section_labels(width):
        labels = []
        first_item_index = 0
        for name, _color, count in COLOR_SECTIONS:
            labels.append(fluentqt.AnnotatedScrollBarLabel(
                name,
                offset_for_item(first_item_index, width),
                name,
            ))
            first_item_index += count
        return labels


    def color_section_for_offset(offset, width):
        current = COLOR_SECTIONS[0][0]
        first_item_index = 0
        for name, _color, count in COLOR_SECTIONS:
            if offset >= offset_for_item(first_item_index, width):
                current = name
            first_item_index += count
        return current


    def month_labels():
        months = (
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
        )
        return [
            fluentqt.AnnotatedScrollBarLabel(
                month,
                index * 100,
                "{0} section".format(month),
            )
            for index, month in enumerate(months)
        ]


    class AnnotatedColorSectionsContent(QWidget):
        def __init__(self, parent=None):
            super().__init__(parent)
            self.setFixedSize(
                ANNOTATED_CONTENT_WIDTH,
                color_content_height_for_width(ANNOTATED_CONTENT_WIDTH),
            )

        def paintEvent(self, event):
            del event
            snapshot = _theme_snapshot(self)
            colors = snapshot["colors"]
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
            painter.fillRect(self.rect(), colors["bgLayer"])

            font = fluentqt.font_for_role(fluentqt.FontRole.Caption)
            font.setWeight(QFont.Weight.DemiBold)
            painter.setFont(font)
            dark = int(snapshot["theme"]) == int(fluentqt.Theme.Dark)
            per_row = items_per_row_for_width(self.width())
            item_index = 0
            for name, color, count in COLOR_SECTIONS:
                fill = QColor(color)
                fill.setAlphaF(0.72 if dark else 0.88)
                for section_index in range(count):
                    row = item_index // per_row
                    column = item_index % per_row
                    cell = QRect(
                        column * ANNOTATED_ITEM_WIDTH,
                        row * ANNOTATED_ITEM_HEIGHT,
                        ANNOTATED_ITEM_WIDTH,
                        ANNOTATED_ITEM_HEIGHT,
                    )
                    item_rect = cell.adjusted(12, 10, -12, -10)
                    painter.setPen(Qt.PenStyle.NoPen)
                    painter.setBrush(fill)
                    painter.drawRoundedRect(item_rect, 6.0, 6.0)
                    if section_index == 0:
                        painter.setPen(QColor(255, 255, 255, 235))
                        painter.drawText(
                            item_rect.adjusted(6, 0, -6, 0),
                            Qt.AlignmentFlag.AlignCenter
                            | Qt.TextFlag.TextSingleLine,
                            name,
                        )
                    item_index += 1
    """
)


def _annotated_scrollbar_script(body: str) -> str:
    return _script(
        _ANNOTATED_SCROLLBAR_HELPER + "\n\n" + dedent(body).strip(),
        _PAINTED_SCROLLING_IMPORTS,
    )


register_source_samples(
    "annotated-scrollbar",
    ("AnnotatedScrollBar", "AnnotatedScrollBarLabel", "ScrollView", "Slider"),
    {
        "annotated-scrollbar-basic": (
            "root",
            _annotated_scrollbar_script(
                """
                root = QWidget(globals().get("gallery_parent"))
                root.setObjectName("galleryStandaloneAnnotatedScrollBarCard")
                root.setSizePolicy(
                    QSizePolicy.Policy.Fixed,
                    QSizePolicy.Policy.Fixed,
                )
                root.setFixedSize(390, 300)
                layout = QHBoxLayout(root)
                layout.setContentsMargins(0, 0, 0, 0)
                layout.setSpacing(18)

                bar = fluentqt.AnnotatedScrollBar(root)
                labels = year_labels()
                bar.setFixedSize(148, 300)
                bar.setRange(0, 960)
                bar.setPageStep(120)
                bar.setLabelColumnWidth(56)
                bar.setIndicatorWidth(32)
                bar.setLabels(labels)
                bar.setDetailLabelProvider(year_detail_for_offset)

                details = QWidget(root)
                details_layout = QVBoxLayout(details)
                details_layout.setContentsMargins(0, 4, 0, 0)
                details_layout.setSpacing(8)
                details_layout.setAlignment(
                    Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                )
                offset_label = annotated_status_label(details, "Offset: 0")
                current_label = annotated_status_label(
                    details, "Current label: 2023"
                )
                detail_label = annotated_status_label(
                    details, "Detail: October 2023"
                )
                offset_label.setMinimumWidth(
                    offset_label.fontMetrics().horizontalAdvance("Offset: 8888")
                )
                current_label.setMinimumWidth(
                    current_label.fontMetrics().horizontalAdvance(
                        "Current label: 8888"
                    )
                )
                detail_label.setMinimumWidth(
                    detail_label.fontMetrics().horizontalAdvance(
                        "Detail: October 8888"
                    )
                )
                details_layout.addWidget(offset_label)
                details_layout.addWidget(current_label)
                details_layout.addWidget(detail_label)
                details_layout.addStretch(1)

                def update_details(value, *unused):
                    del unused
                    offset_label.setText("Offset: {0}".format(value))
                    current_label.setText(
                        "Current label: {0}".format(
                            label_text_for_offset(labels, value)
                        )
                    )
                    detail_label.setText(
                        "Detail: {0}".format(year_detail_for_offset(value))
                    )

                bar.valueChanged.connect(update_details)
                bar.labelActivated.connect(update_details)
                layout.addWidget(bar)
                layout.addWidget(details, 0, Qt.AlignmentFlag.AlignTop)
                """
            ),
        ),
        "annotated-scrollbar-scrollview": (
            "root",
            _annotated_scrollbar_script(
                """
                root = QWidget(globals().get("gallery_parent"))
                root.setObjectName("galleryLinkedAnnotatedScrollBarCard")
                root.setSizePolicy(
                    QSizePolicy.Policy.Fixed,
                    QSizePolicy.Policy.Fixed,
                )
                root.setFixedSize(542, 354)
                layout = QVBoxLayout(root)
                layout.setContentsMargins(0, 0, 0, 0)
                layout.setSpacing(10)

                row = QWidget(root)
                row_layout = QHBoxLayout(row)
                row_layout.setContentsMargins(0, 0, 0, 0)
                row_layout.setSpacing(12)
                scroll_view = fluentqt.ScrollView(row)
                scroll_view.setFixedSize(ANNOTATED_VIEWPORT_WIDTH, 320)
                scroll_view.setHorizontalScrollBarVisibility(
                    fluentqt.ScrollView.ScrollBarVisibility.Hidden
                )
                scroll_view.setVerticalScrollBarVisibility(
                    fluentqt.ScrollView.ScrollBarVisibility.Hidden
                )
                scroll_view.setOwnedContentWidget(
                    AnnotatedColorSectionsContent()
                )

                bar = fluentqt.AnnotatedScrollBar(row)
                bar.setFixedSize(150, 320)
                bar.setPreferredSize(QSize(150, 320))
                bar.setMinimumBarSize(QSize(120, 220))
                bar.setLabelColumnWidth(86)
                bar.setMinimumLabelSpacing(56)
                bar.setIndicatorWidth(34)
                bar.setCaretSize(QSize(16, 18))
                bar.setLabels(color_section_labels(ANNOTATED_CONTENT_WIDTH))
                bar.setDetailLabelProvider(
                    lambda offset: color_section_for_offset(
                        offset, ANNOTATED_CONTENT_WIDTH
                    )
                )
                bar.connectToScrollView(scroll_view)
                row_layout.addWidget(scroll_view)
                row_layout.addWidget(bar)

                status = annotated_status_label(
                    root, "Section: Azure - offset 0"
                )
                status.setMinimumWidth(
                    status.fontMetrics().horizontalAdvance(
                        "Section: Fuchsia - offset 8888"
                    )
                )

                def update_status(offset, *unused):
                    del unused
                    status.setText("Section: {0} - offset {1}".format(
                        color_section_for_offset(
                            offset, ANNOTATED_CONTENT_WIDTH
                        ),
                        offset,
                    ))

                scroll_view.scrollPositionChanged.connect(
                    lambda horizontal, vertical: update_status(vertical)
                )
                bar.labelActivated.connect(update_status)
                layout.addWidget(row)
                layout.addWidget(status)
                """
            ),
        ),
        "annotated-scrollbar-label-density": (
            "root",
            _annotated_scrollbar_script(
                """
                root = QWidget(globals().get("gallery_parent"))
                root.setObjectName("galleryAnnotatedScrollBarHeightCard")
                root.setSizePolicy(
                    QSizePolicy.Policy.Fixed,
                    QSizePolicy.Policy.Fixed,
                )
                root.setFixedSize(382, 360)
                layout = QHBoxLayout(root)
                layout.setContentsMargins(0, 0, 0, 0)
                layout.setSpacing(18)

                bar = fluentqt.AnnotatedScrollBar(root)
                bar.setFixedSize(144, 360)
                bar.setRange(0, 1100)
                bar.setPageStep(120)
                bar.setLabelColumnWidth(56)
                bar.setMinimumLabelSpacing(28)
                bar.setIndicatorWidth(32)
                bar.setLabels(month_labels())

                controls = QWidget(root)
                controls_layout = QVBoxLayout(controls)
                controls_layout.setContentsMargins(0, 4, 0, 0)
                controls_layout.setSpacing(8)
                controls_layout.setAlignment(
                    Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                )
                height_value = annotated_status_label(
                    controls, "Height: 360 px"
                )
                visible_value = annotated_status_label(
                    controls, "Visible labels: 12 of 12"
                )
                height_value.setMinimumWidth(
                    height_value.fontMetrics().horizontalAdvance("Height: 888 px")
                )
                visible_value.setMinimumWidth(
                    visible_value.fontMetrics().horizontalAdvance(
                        "Visible labels: 88 of 88"
                    )
                )
                slider = fluentqt.Slider(Qt.Orientation.Horizontal, controls)
                slider.setRange(180, 360)
                slider.setSingleStep(20)
                slider.setPageStep(40)
                slider.setValue(360)
                slider.setFixedSize(220, 36)

                def update_summary(*unused):
                    del unused
                    height_value.setText("Height: {0} px".format(bar.height()))
                    visible_value.setText(
                        "Visible labels: {0} of {1}".format(
                            bar.visibleLabelCount(), len(bar.labels())
                        )
                    )

                def update_height(value):
                    bar.setFixedHeight(value)
                    bar.updateGeometry()
                    update_summary()

                slider.valueChanged.connect(update_height)
                controls_layout.addWidget(height_value)
                controls_layout.addWidget(visible_value)
                controls_layout.addWidget(slider)
                controls_layout.addStretch(1)
                layout.addWidget(bar, 0, Qt.AlignmentFlag.AlignTop)
                layout.addWidget(controls, 0, Qt.AlignmentFlag.AlignTop)
                update_summary()
                """
            ),
        ),
    },
)


register_source_samples(
    "pips-pager",
    ("PipsPager",),
    {
        "pips-pager-flipview": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + dedent("""
                root = make_scrolling_surface(globals().get("gallery_parent"))
                layout = root.layout()
                flip_view = fluentqt.FlipView(root)
                flip_view.setFixedSize(420, 220)
                flip_view.setShowPageIndicator(False)
                flip_view.setShowNavigationButtons(True)
                pages = pager_picture_pages()
                for index, page in enumerate(pages, 1):
                    flip_view.addOwnedPage(
                        PagerPicture(
                            "{0}  Page {1} of {2}".format(page[0], index, len(pages)),
                            page[1],
                            page[2],
                            QSize(420, 220),
                        )
                    )
                pager = fluentqt.PipsPager(root)
                pager.setNumberOfPages(flip_view.pageCount())
                pager.setMaxVisiblePips(5)
                refresh_pager_fixed_size(pager)
                status = make_status_label(root, "Selected page: Coastal route")
                pager.selectedPageIndexChanged.connect(flip_view.setCurrentIndex)
                flip_view.currentIndexChanged.connect(pager.setSelectedPageIndex)
                pager.selectedPageIndexChanged.connect(
                    lambda index: status.setText(
                        "Selected page: {0}".format(pages[index][0])
                    )
                )
                layout.addWidget(flip_view, 0, Qt.AlignmentFlag.AlignHCenter)
                layout.addWidget(pager, 0, Qt.AlignmentFlag.AlignHCenter)
                layout.addWidget(status)
                """),
                _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
        "pips-pager-orientation": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + dedent("""
                root = make_scrolling_surface(globals().get("gallery_parent"))
                layout = root.layout()
                controls = horizontal_group(root, 8)
                horizontal = make_sample_button(controls, "Horizontal")
                vertical = make_sample_button(controls, "Vertical")
                controls.layout().addWidget(horizontal)
                controls.layout().addWidget(vertical)

                row = horizontal_group(root, 28)
                pager = fluentqt.PipsPager(row)
                pager.setNumberOfPages(7)
                pager.setSelectedPageIndex(3)
                pager.setPreviousButtonVisibility(
                    fluentqt.PipsPager.PipsPagerButtonVisibility.Visible
                )
                pager.setNextButtonVisibility(
                    fluentqt.PipsPager.PipsPagerButtonVisibility.Visible
                )
                pager.setOrientation(Qt.Orientation.Vertical)
                reserved_row_height = pager.sizeHint().height()
                pager.setOrientation(Qt.Orientation.Horizontal)
                refresh_pager_fixed_size(pager)
                status = make_status_label(row, "Orientation: Horizontal")
                status.setMinimumWidth(
                    status.fontMetrics().horizontalAdvance("Orientation: Horizontal")
                )
                row.setMinimumHeight(reserved_row_height)
                row.layout().addWidget(pager, 0, Qt.AlignmentFlag.AlignCenter)
                row.layout().addWidget(status, 0, Qt.AlignmentFlag.AlignCenter)

                def apply_orientation(orientation):
                    pager.setOrientation(orientation)
                    refresh_pager_fixed_size(pager)
                    is_horizontal = orientation == Qt.Orientation.Horizontal
                    status.setText(
                        "Orientation: Horizontal"
                        if is_horizontal
                        else "Orientation: Vertical"
                    )
                    set_button_active(horizontal, is_horizontal)
                    set_button_active(vertical, not is_horizontal)

                horizontal.clicked.connect(lambda: apply_orientation(Qt.Orientation.Horizontal))
                vertical.clicked.connect(lambda: apply_orientation(Qt.Orientation.Vertical))
                apply_orientation(Qt.Orientation.Horizontal)
                layout.addWidget(controls)
                layout.addWidget(row)
                """),
                _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
        "pips-pager-button-visibility": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + dedent("""
                root = make_scrolling_surface(
                    globals().get("gallery_parent"), 10
                )
                layout = root.layout()
                values = (
                    (
                        "Collapsed",
                        fluentqt.PipsPager.PipsPagerButtonVisibility.Collapsed,
                    ),
                    (
                        "Visible",
                        fluentqt.PipsPager.PipsPagerButtonVisibility.Visible,
                    ),
                    (
                        "VisibleOnPointerOver",
                        fluentqt.PipsPager.PipsPagerButtonVisibility.VisibleOnPointerOver,
                    ),
                )
                for title, visibility in values:
                    row = horizontal_group(root, 18)
                    label = make_status_label(row, title)
                    label.setMinimumWidth(
                        label.fontMetrics().horizontalAdvance(
                            "VisibleOnPointerOver"
                        )
                    )
                    pager = fluentqt.PipsPager(row)
                    pager.setNumberOfPages(7)
                    pager.setSelectedPageIndex(3)
                    pager.setPreviousButtonVisibility(visibility)
                    pager.setNextButtonVisibility(visibility)
                    refresh_pager_fixed_size(pager)
                    row.layout().addWidget(label)
                    row.layout().addWidget(pager, 0, Qt.AlignmentFlag.AlignCenter)
                    row.layout().addStretch(1)
                    layout.addWidget(row)
                """),
                _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
        "pips-pager-visible-window": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + dedent("""
                root = make_scrolling_surface(globals().get("gallery_parent"))
                layout = root.layout()
                controls = horizontal_group(root, 8)
                first = make_sample_button(controls, "First")
                middle = make_sample_button(controls, "Middle")
                last = make_sample_button(controls, "Last")
                controls.layout().addWidget(first)
                controls.layout().addWidget(middle)
                controls.layout().addWidget(last)

                pager = fluentqt.PipsPager(root)
                pager.setNumberOfPages(10)
                pager.setMaxVisiblePips(5)
                pager.setPreviousButtonVisibility(
                    fluentqt.PipsPager.PipsPagerButtonVisibility.Visible
                )
                pager.setNextButtonVisibility(
                    fluentqt.PipsPager.PipsPagerButtonVisibility.Visible
                )
                refresh_pager_fixed_size(pager)
                status = make_status_label(root, "")

                def update_status(*unused):
                    del unused
                    status.setText(
                        "Selected: {0}, first visible page: {1}".format(
                            pager.selectedPageIndex(), pager.firstVisiblePage()
                        )
                    )
                    set_button_active(first, pager.selectedPageIndex() == 0)
                    set_button_active(middle, pager.selectedPageIndex() == 4)
                    set_button_active(last, pager.selectedPageIndex() == 9)

                first.clicked.connect(lambda: pager.setSelectedPageIndex(0))
                middle.clicked.connect(lambda: pager.setSelectedPageIndex(4))
                last.clicked.connect(lambda: pager.setSelectedPageIndex(9))
                pager.selectedPageIndexChanged.connect(update_status)
                update_status()
                layout.addWidget(controls)
                layout.addWidget(pager, 0, Qt.AlignmentFlag.AlignHCenter)
                layout.addWidget(status)
                """),
                _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
        "pips-pager-metrics-animation": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + dedent("""
                root = make_scrolling_surface(globals().get("gallery_parent"))
                layout = root.layout()
                controls = horizontal_group(root, 8)
                previous = make_sample_button(controls, "Previous")
                next_button = make_sample_button(controls, "Next")
                controls.layout().addWidget(previous)
                controls.layout().addWidget(next_button)

                pager = fluentqt.PipsPager(root)
                pager.setNumberOfPages(8)
                pager.setSelectedPageIndex(2)
                pager.setMaxVisiblePips(6)
                pager.setPipCellSize(16)
                pager.setInactivePipDiameter(6)
                pager.setSelectedPipDiameter(10)
                pager.setNavigationButtonSize(30)
                pager.setNavigationIconSize(12)
                pager.setSelectionAnimationDuration(420)
                pager.setPreviousButtonVisibility(
                    fluentqt.PipsPager.PipsPagerButtonVisibility.Visible
                )
                pager.setNextButtonVisibility(
                    fluentqt.PipsPager.PipsPagerButtonVisibility.Visible
                )
                refresh_pager_fixed_size(pager)
                status = make_status_label(root, "")

                def update_status(*unused):
                    del unused
                    status.setText(
                        "Page {0} of {1}".format(
                            pager.selectedPageIndex() + 1,
                            pager.numberOfPages(),
                        )
                    )

                previous.clicked.connect(pager.goToPreviousPage)
                next_button.clicked.connect(pager.goToNextPage)
                pager.selectedPageIndexChanged.connect(update_status)
                update_status()
                layout.addWidget(controls)
                layout.addWidget(pager, 0, Qt.AlignmentFlag.AlignHCenter)
                layout.addWidget(status)
                """),
                _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
        "pips-pager-disabled-empty": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + dedent("""
                root = make_scrolling_surface(
                    globals().get("gallery_parent"), 10
                )
                layout = root.layout()
                disabled = fluentqt.PipsPager(root)
                disabled.setNumberOfPages(6)
                disabled.setSelectedPageIndex(2)
                disabled.setPreviousButtonVisibility(
                    fluentqt.PipsPager.PipsPagerButtonVisibility.Visible
                )
                disabled.setNextButtonVisibility(
                    fluentqt.PipsPager.PipsPagerButtonVisibility.Visible
                )
                disabled.setEnabled(False)
                empty = fluentqt.PipsPager(root)
                empty.setNumberOfPages(0)
                empty.setPreviousButtonVisibility(
                    fluentqt.PipsPager.PipsPagerButtonVisibility.Visible
                )
                empty.setNextButtonVisibility(
                    fluentqt.PipsPager.PipsPagerButtonVisibility.Visible
                )
                refresh_pager_fixed_size(disabled)
                refresh_pager_fixed_size(empty)
                for title, pager in (("Disabled", disabled), ("Empty", empty)):
                    row = horizontal_group(root, 18)
                    label = make_status_label(row, title)
                    label.setMinimumWidth(
                        label.fontMetrics().horizontalAdvance("Disabled")
                    )
                    pager.setParent(row)
                    row.layout().addWidget(label)
                    row.layout().addWidget(pager, 0, Qt.AlignmentFlag.AlignCenter)
                    row.layout().addStretch(1)
                    layout.addWidget(row)
                layout.addWidget(make_status_label(
                    root,
                    "Empty state accessible description: No pages selected",
                ))
                """),
                _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "scrollbar",
    ("ScrollBar",),
    {
        "scrollbar-basic": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + dedent("""
                root = make_scrolling_surface(globals().get("gallery_parent"))
                root.setFixedWidth(440)
                layout = root.layout()

                row = QWidget(root)
                row_layout = QHBoxLayout(row)
                row_layout.setContentsMargins(0, 0, 0, 0)
                row_layout.setSpacing(22)
                row_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                horizontal_column = QWidget(row)
                horizontal_layout = QVBoxLayout(horizontal_column)
                horizontal_layout.setContentsMargins(0, 0, 0, 0)
                horizontal_layout.setSpacing(10)
                horizontal_layout.setAlignment(
                    Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                )
                horizontal = fluentqt.ScrollBar(
                    Qt.Orientation.Horizontal, horizontal_column
                )
                horizontal.setRange(0, 1000)
                horizontal.setPageStep(100)
                horizontal.setValue(420)
                horizontal.setOpacity(1.0)
                horizontal.setFixedWidth(360)
                status = fluentqt.Label("Value: 420", horizontal_column)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setMinimumWidth(
                    status.fontMetrics().horizontalAdvance("Value: 8888")
                )
                vertical = fluentqt.ScrollBar(Qt.Orientation.Vertical, row)
                vertical.setRange(0, 1000)
                vertical.setPageStep(100)
                vertical.setValue(420)
                vertical.setOpacity(1.0)
                vertical.setFixedHeight(178)

                def sync_vertical(value):
                    vertical.setValue(value)
                    status.setText(f"Value: {value}")

                def sync_horizontal(value):
                    horizontal.setValue(value)
                    status.setText(f"Value: {value}")

                horizontal.valueChanged.connect(sync_vertical)
                vertical.valueChanged.connect(sync_horizontal)
                horizontal_layout.addWidget(horizontal)
                horizontal_layout.addWidget(status)
                row_layout.addWidget(
                    horizontal_column, 0, Qt.AlignmentFlag.AlignTop
                )
                row_layout.addWidget(vertical, 0, Qt.AlignmentFlag.AlignTop)
                row_layout.addStretch(1)
                layout.addWidget(row)
                """),
                _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
        "scrollbar-thickness": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + dedent("""
                root = make_scrolling_surface(globals().get("gallery_parent"))
                root.setFixedSize(470, 170)
                layout = root.layout()
                for text, value, thickness in (
                    ("Thin 6 px", 220, 6),
                    ("Default 7 px", 420, 7),
                    ("Large 24 px", 640, 24),
                ):
                    row = QWidget(root)
                    row_layout = QHBoxLayout(row)
                    row_layout.setContentsMargins(0, 0, 0, 0)
                    row_layout.setSpacing(14)
                    row_layout.setAlignment(
                        Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                    )
                    label = fluentqt.Label(text, row)
                    label.setFluentTypography(fluentqt.FontRole.Body)
                    label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                    label.setMinimumWidth(
                        label.fontMetrics().horizontalAdvance("Default 7 px") + 4
                    )
                    label.setSizePolicy(
                        QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Preferred
                    )
                    bar = fluentqt.ScrollBar(Qt.Orientation.Horizontal, row)
                    bar.setRange(0, 1000)
                    bar.setPageStep(100)
                    bar.setValue(value)
                    bar.setThickness(thickness)
                    bar.setOpacity(1.0)
                    bar.setMinimumWidth(240)
                    bar.setSizePolicy(
                        QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed
                    )
                    row_layout.addWidget(label)
                    row_layout.addWidget(bar, 1)
                    layout.addWidget(row)
                """),
                _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
        "scrollbar-opacity": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + dedent("""
                root = make_scrolling_surface(globals().get("gallery_parent"))
                root.setFixedSize(470, 128)
                layout = root.layout()
                for text, opacity in (("Opacity 1.0", 1.0), ("Opacity 0.45", 0.45)):
                    row = QWidget(root)
                    row_layout = QHBoxLayout(row)
                    row_layout.setContentsMargins(0, 0, 0, 0)
                    row_layout.setSpacing(14)
                    row_layout.setAlignment(
                        Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                    )
                    label = fluentqt.Label(text, row)
                    label.setFluentTypography(fluentqt.FontRole.Body)
                    label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                    label.setMinimumWidth(
                        label.fontMetrics().horizontalAdvance("Opacity 0.45") + 4
                    )
                    label.setSizePolicy(
                        QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Preferred
                    )
                    bar = fluentqt.ScrollBar(Qt.Orientation.Horizontal, row)
                    bar.setRange(0, 1000)
                    bar.setPageStep(100)
                    bar.setValue(420)
                    bar.setOpacity(opacity)
                    bar.setMinimumWidth(240)
                    bar.setSizePolicy(
                        QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed
                    )
                    row_layout.addWidget(label)
                    row_layout.addWidget(bar, 1)
                    layout.addWidget(row)
                """),
                _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "scroll-view",
    ("ScrollView",),
    {
        "scroll-view-content-zoom": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + _SCROLL_CANVAS_HELPER
                + dedent("""
                root = make_scrolling_surface(globals().get("gallery_parent"))
                layout = root.layout()

                controls = QWidget(root)
                controls_layout = QHBoxLayout(controls)
                controls_layout.setContentsMargins(0, 0, 0, 0)
                controls_layout.setSpacing(8)
                controls_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft
                    | Qt.AlignmentFlag.AlignVCenter
                )
                zoom_out = make_sample_button(controls, "Zoom out")
                reset = make_sample_button(controls, "Reset")
                zoom_in = make_sample_button(controls, "Zoom in")
                controls_layout.addWidget(zoom_out)
                controls_layout.addWidget(reset)
                controls_layout.addWidget(zoom_in)

                scroll_view = fluentqt.ScrollView(root)
                scroll_view.setFixedSize(420, 240)
                scroll_view.setOwnedContentWidget(
                    ScrollViewDemoCanvas(QSize(760, 520), "Content canvas")
                )
                scroll_view.setZoomMode(fluentqt.ScrollView.ZoomMode.Enabled)
                scroll_view.setMinZoomFactor(0.5)
                scroll_view.setMaxZoomFactor(2.0)
                scroll_view.setHorizontalScrollBarVisibility(
                    fluentqt.ScrollView.ScrollBarVisibility.Auto
                )
                scroll_view.setVerticalScrollBarVisibility(
                    fluentqt.ScrollView.ScrollBarVisibility.Auto
                )
                status = make_status_label(root, "")

                zoom_in.clicked.connect(lambda: scroll_view.zoomBy(1.25, True))
                zoom_out.clicked.connect(lambda: scroll_view.zoomBy(0.8, True))
                reset.clicked.connect(lambda: scroll_view.resetZoom(True))
                bind_scroll_view_status(scroll_view, status)
                layout.addWidget(controls)
                layout.addWidget(scroll_view)
                layout.addWidget(status)
                """),
                _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
        "scroll-view-scrollbar-policies": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + _SCROLL_CANVAS_HELPER
                + dedent("""
                root = make_scrolling_surface(globals().get("gallery_parent"))
                layout = root.layout()
                controls = horizontal_group(root, 8)
                auto_bars = make_sample_button(controls, "Auto")
                visible_bars = make_sample_button(controls, "Visible")
                hidden_horizontal = make_sample_button(controls, "Hide H")
                vertical_disabled = make_sample_button(controls, "Disable V")
                buttons = (
                    auto_bars,
                    visible_bars,
                    hidden_horizontal,
                    vertical_disabled,
                )
                for button in buttons:
                    controls.layout().addWidget(button)

                scroll_view = fluentqt.ScrollView(root)
                scroll_view.setFixedSize(420, 220)
                ScrollMode = fluentqt.ScrollView.ScrollMode
                ScrollBarVisibility = fluentqt.ScrollView.ScrollBarVisibility
                scroll_view.setOwnedContentWidget(
                    ScrollViewDemoCanvas(QSize(680, 420), "Policy canvas")
                )
                status = make_status_label(root, "")

                def update_status(policy):
                    status.setText(
                        "{0} - {1}".format(
                            policy,
                            scroll_view_offset_text(scroll_view),
                        )
                    )

                def reset_buttons():
                    for button in buttons:
                        set_button_active(button, False)

                def apply_auto():
                    scroll_view.setHorizontalScrollMode(ScrollMode.Auto)
                    scroll_view.setVerticalScrollMode(ScrollMode.Auto)
                    scroll_view.setHorizontalScrollBarVisibility(
                        ScrollBarVisibility.Auto
                    )
                    scroll_view.setVerticalScrollBarVisibility(
                        ScrollBarVisibility.Auto
                    )
                    scroll_view.scrollTo(80, 80, False)
                    reset_buttons()
                    set_button_active(auto_bars, True)
                    update_status("Auto bars")

                def apply_visible():
                    scroll_view.setHorizontalScrollMode(ScrollMode.Enabled)
                    scroll_view.setVerticalScrollMode(ScrollMode.Enabled)
                    scroll_view.setHorizontalScrollBarVisibility(
                        ScrollBarVisibility.Visible
                    )
                    scroll_view.setVerticalScrollBarVisibility(
                        ScrollBarVisibility.Visible
                    )
                    scroll_view.scrollTo(80, 80, False)
                    reset_buttons()
                    set_button_active(visible_bars, True)
                    update_status("Visible bars")

                def hide_horizontal():
                    scroll_view.setHorizontalScrollMode(ScrollMode.Enabled)
                    scroll_view.setVerticalScrollMode(ScrollMode.Auto)
                    scroll_view.setHorizontalScrollBarVisibility(
                        ScrollBarVisibility.Hidden
                    )
                    scroll_view.setVerticalScrollBarVisibility(
                        ScrollBarVisibility.Auto
                    )
                    scroll_view.scrollTo(160, 80, False)
                    reset_buttons()
                    set_button_active(hidden_horizontal, True)
                    update_status("Hidden horizontal bar")

                def disable_vertical():
                    scroll_view.setHorizontalScrollMode(ScrollMode.Auto)
                    scroll_view.setVerticalScrollMode(ScrollMode.Disabled)
                    scroll_view.setHorizontalScrollBarVisibility(
                        ScrollBarVisibility.Auto
                    )
                    scroll_view.setVerticalScrollBarVisibility(
                        ScrollBarVisibility.Disabled
                    )
                    scroll_view.scrollTo(120, 160, False)
                    reset_buttons()
                    set_button_active(vertical_disabled, True)
                    update_status("Vertical disabled")

                auto_bars.clicked.connect(apply_auto)
                visible_bars.clicked.connect(apply_visible)
                hidden_horizontal.clicked.connect(hide_horizontal)
                vertical_disabled.clicked.connect(disable_vertical)
                scroll_view.scrollPositionChanged.connect(
                    lambda *_args: status.setText(
                        scroll_view_offset_text(scroll_view)
                    )
                )
                apply_auto()
                layout.addWidget(controls)
                layout.addWidget(scroll_view)
                layout.addWidget(status)
                """),
                _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
        "scroll-view-programmatic-scroll": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + dedent("""
                root = make_scrolling_surface(globals().get("gallery_parent"))
                layout = root.layout()
                controls = horizontal_group(root, 8)
                top = make_sample_button(controls, "Top")
                center = make_sample_button(controls, "Center")
                end = make_sample_button(controls, "End")
                nudge = make_sample_button(controls, "Nudge")
                for button in (top, center, end, nudge):
                    controls.layout().addWidget(button)

                scroll_view = fluentqt.ScrollView(root)
                scroll_view.setFixedSize(420, 250)
                scroll_view.setOwnedContentWidget(
                    make_scroll_view_stack_content()
                )
                status = make_status_label(root, "")
                bind_scroll_view_status(scroll_view, status)
                top.clicked.connect(lambda: scroll_view.scrollTo(0, 0, True))
                center.clicked.connect(lambda: scroll_view.scrollTo(
                    scroll_view.scrollableWidth() // 2,
                    scroll_view.scrollableHeight() // 2,
                    True,
                ))
                end.clicked.connect(lambda: scroll_view.scrollTo(
                    scroll_view.scrollableWidth(),
                    scroll_view.scrollableHeight(),
                    True,
                ))
                nudge.clicked.connect(lambda: scroll_view.scrollBy(0, 120, True))
                layout.addWidget(controls)
                layout.addWidget(scroll_view)
                layout.addWidget(status)
                """),
                _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
        "scroll-view-constant-velocity": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + dedent("""
                root = make_scrolling_surface(globals().get("gallery_parent"))
                layout = root.layout()
                controls = horizontal_group(root, 8)
                up = make_sample_button(controls, "Up")
                stop = make_sample_button(controls, "Stop")
                down = make_sample_button(controls, "Down")
                for button in (up, stop, down):
                    controls.layout().addWidget(button)

                scroll_view = fluentqt.ScrollView(root)
                scroll_view.setFixedSize(420, 250)
                scroll_view.setOwnedContentWidget(
                    make_scroll_view_stack_content()
                )
                timer = QTimer(scroll_view)
                timer.setInterval(50)
                timer.setProperty("velocity", 0)
                timer.timeout.connect(
                    lambda: scroll_view.scrollBy(0, int(timer.property("velocity")), False)
                )

                status = make_status_label(root, "")

                def update_status(*unused):
                    del unused
                    status.setText(
                        "Velocity: {0} px/tick - {1}".format(
                            timer.property("velocity"),
                            scroll_view_offset_text(scroll_view),
                        )
                    )

                def set_velocity(velocity):
                    timer.setProperty("velocity", velocity)
                    timer.stop() if velocity == 0 else timer.start()
                    set_button_active(up, velocity < 0)
                    set_button_active(stop, velocity == 0)
                    set_button_active(down, velocity > 0)
                    update_status()

                scroll_view.scrollPositionChanged.connect(update_status)
                up.clicked.connect(lambda: set_velocity(-12))
                stop.clicked.connect(lambda: set_velocity(0))
                down.clicked.connect(lambda: set_velocity(12))
                set_velocity(0)
                layout.addWidget(controls)
                layout.addWidget(scroll_view)
                layout.addWidget(status)
                """),
                "from PySide6.QtCore import QTimer\n" + _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
        "scroll-view-scroll-chaining": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + dedent("""
                root = make_scrolling_surface(globals().get("gallery_parent"))
                layout = root.layout()
                row = horizontal_group(root, 14)
                default_column = vertical_group(row, 6)
                contained_column = vertical_group(row, 6)
                default_label = make_status_label(
                    default_column, "Default: chaining enabled"
                )
                contained_label = make_status_label(
                    contained_column, "Contained: chaining disabled"
                )
                default_view = fluentqt.ScrollView(default_column)
                default_view.setFixedSize(220, 180)
                default_view.setOwnedContentWidget(
                    make_scroll_view_stack_content(
                        image_size=QSize(190, 100)
                    )
                )
                default_view.setScrollChainingEnabled(True)
                contained_view = fluentqt.ScrollView(contained_column)
                contained_view.setFixedSize(220, 180)
                contained_view.setOwnedContentWidget(
                    make_scroll_view_stack_content(
                        image_size=QSize(190, 100)
                    )
                )
                contained_view.setScrollChainingEnabled(False)
                default_column.layout().addWidget(default_label)
                default_column.layout().addWidget(default_view)
                contained_column.layout().addWidget(contained_label)
                contained_column.layout().addWidget(contained_view)
                row.layout().addWidget(default_column)
                row.layout().addWidget(contained_column)
                layout.addWidget(row)
                layout.addWidget(make_status_label(
                    root,
                    "Scroll each inner view to an edge, then continue the wheel gesture.",
                ))
                """),
                _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
        "scroll-view-zoom-aware-content": (
            "root",
            _script(
                _PAGER_PICTURE_HELPER
                + _SCROLL_CANVAS_HELPER
                + dedent("""
                root = make_scrolling_surface(globals().get("gallery_parent"))
                layout = root.layout()
                controls = horizontal_group(root, 8)
                zoom50 = make_sample_button(controls, "50%")
                zoom100 = make_sample_button(controls, "100%")
                zoom150 = make_sample_button(controls, "150%")
                for button in (zoom50, zoom100, zoom150):
                    controls.layout().addWidget(button)

                scroll_view = fluentqt.ScrollView(root)
                scroll_view.setFixedSize(420, 230)
                scroll_view.setZoomMode(fluentqt.ScrollView.ZoomMode.Enabled)
                scroll_view.setMinZoomFactor(0.5)
                scroll_view.setMaxZoomFactor(2.0)
                scroll_view.setOwnedContentWidget(
                    ScrollViewDemoCanvas(
                        QSize(560, 360), "Zoom-aware canvas"
                    )
                )
                status = make_status_label(root, "")
                bind_scroll_view_status(scroll_view, status)

                def set_zoom(zoom):
                    scroll_view.zoomTo(zoom, True)
                    set_button_active(zoom50, abs(zoom - 0.5) < 0.000001)
                    set_button_active(zoom100, abs(zoom - 1.0) < 0.000001)
                    set_button_active(zoom150, abs(zoom - 1.5) < 0.000001)

                zoom50.clicked.connect(lambda: set_zoom(0.5))
                zoom100.clicked.connect(lambda: set_zoom(1.0))
                zoom150.clicked.connect(lambda: set_zoom(1.5))
                set_zoom(1.0)
                layout.addWidget(controls)
                layout.addWidget(scroll_view)
                layout.addWidget(status)
                """),
                _PAINTED_SCROLLING_IMPORTS,
            ),
        ),
    },
)
