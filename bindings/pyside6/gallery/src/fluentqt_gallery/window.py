"""Native-shaped shell and pages for the standalone PySide6 Gallery."""

from __future__ import annotations

from pathlib import Path
import sys
from typing import Callable, Iterable

import fluentqt
from PySide6.QtCore import (
    QEasingCurve,
    QElapsedTimer,
    QEvent,
    QPoint,
    QPropertyAnimation,
    QRect,
    QRectF,
    QSize,
    QStandardPaths,
    Qt,
    QTimer,
    QUrl,
    QVariantAnimation,
)
from PySide6.QtGui import (
    QColor,
    QDesktopServices,
    QFontMetrics,
    QPalette,
    QPainter,
    QPainterPath,
    QPen,
)
from PySide6.QtWidgets import (
    QApplication,
    QFrame,
    QGridLayout,
    QGraphicsOpacityEffect,
    QHBoxLayout,
    QLabel,
    QSizePolicy,
    QVBoxLayout,
    QWidget,
)

from .catalog import (
    CATEGORIES,
    CATEGORY_BY_ID,
    ENTRIES,
    ENTRY_BY_ROUTE_ID,
    ROUTES,
    ROUTE_BY_ID,
    GalleryEntry,
    GalleryRoute,
    GallerySampleEntry,
    entries_for_category,
)
from .foundation_pages import populate_foundation_topic_page
from .intro_tour import GalleryIntroTour, TourStep
from .metrics import TITLE_BAR_HEIGHT
from .samples import PreviewResult, build_sample
from .settings import (
    NavigationStyle,
    gallery_settings,
    persistence_available,
)
from .update_checker import (
    GalleryUpdateChecker,
    UpdateResult,
    UpdateStatus,
)
from .visual import (
    GalleryCodeBlock,
    GalleryEntryCard,
    GalleryEntryGrid,
    GalleryHomeHero,
    GalleryNavigationFooter,
    GalleryNavigationPane,
    GalleryPageSkeleton,
    GalleryReferenceCard,
    GallerySplashScreen,
    GalleryTopNavigationPane,
    app_icon,
    app_icon_pixmap,
    control_image_path,
    css_color,
    _draw_native_font_icon,
    _single_shot,
    gallery_colors,
    refresh_gallery_display_scale,
    refresh_gallery_visuals,
    route_icon_name,
)


CONTENT_MARGINS = (24, 34, 24, 48)
FEATURED_ROUTES = (
    "button",
    "toggle-switch",
    "combobox",
    "list-view",
    "calendar-view",
    "info-bar",
    "tree-view",
    "slider",
    "tab-view",
)
_EDITING_COMMAND_ROUTER_OBJECT_NAME = "Gallery.WindowEditingCommandRouter"


_CONTENT_PRIMARY_LABEL_NAMES = frozenset(
    ("galleryContentTitle", "galleryContentSectionHeader")
)
_CONTENT_SECONDARY_LABEL_NAMES = frozenset(
    ("galleryContentBody", "galleryContentSubtitle")
)


def gallery_window_editing_command_router(
    fallback_context: QWidget,
) -> fluentqt.EditingCommandRouter:
    """Return the one editing-command router owned by the current window."""

    scope_window = fallback_context.window() or fallback_context
    router = scope_window.findChild(
        fluentqt.EditingCommandRouter,
        _EDITING_COMMAND_ROUTER_OBJECT_NAME,
        Qt.FindDirectChildrenOnly,
    )
    if router is not None:
        return router
    router = fluentqt.EditingCommandRouter(scope_window, scope_window)
    router.setObjectName(_EDITING_COMMAND_ROUTER_OBJECT_NAME)
    return router


def _apply_content_label_color(
    label: fluentqt.Label,
    *,
    primary: bool,
) -> None:
    """Give app-owned copy a semantic color that survives ancestor QSS."""

    role = (
        fluentqt.Label.TextColorRole.Primary
        if primary
        else fluentqt.Label.TextColorRole.Secondary
    )
    label.setTextColorRole(role)


def _search_tokens(query: str) -> tuple[str, ...]:
    return tuple(token.casefold() for token in query.strip().split() if token)


def _title_matches_tokens(title: str, tokens: tuple[str, ...]) -> bool:
    folded = title.casefold()
    return all(token in folded for token in tokens)


def _ranked_route_titles(query: str) -> list[str]:
    needle = query.strip()
    tokens = _search_tokens(needle)
    titles = sorted((route.title for route in ROUTES), key=str.casefold)
    if not tokens:
        return titles
    matched = [
        title for title in titles if _title_matches_tokens(title, tokens)
    ]
    folded_needle = needle.casefold()
    return sorted(
        matched,
        key=lambda title: (
            0 if title.casefold().startswith(folded_needle) else 1,
            title.casefold(),
        ),
    )


def _best_search_route(query: str) -> GalleryRoute | None:
    needle = query.strip()
    tokens = _search_tokens(needle)
    if not tokens:
        return None
    folded_needle = needle.casefold()
    candidates = []
    for route in ROUTES:
        if not _title_matches_tokens(route.title, tokens):
            continue
        folded_title = route.title.casefold()
        score = 0 if folded_title == folded_needle else (
            1 if folded_title.startswith(folded_needle) else 2
        )
        candidates.append((score, folded_title, route))
    if not candidates:
        return None
    return min(candidates, key=lambda value: (value[0], value[1]))[2]


class _GalleryContentScrollView(fluentqt.ScrollView):
    """Keep the native NavigationView content frame visible behind each page."""

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._gallery_first_show = True
        self._apply_transparent_surface()

    def _apply_transparent_surface(self) -> None:
        self.setAutoFillBackground(False)
        palette = self.palette()
        palette.setColor(QPalette.Window, Qt.transparent)
        palette.setColor(QPalette.Base, Qt.transparent)
        self.setPalette(palette)
        viewport = self.viewport()
        if viewport is None:
            return
        viewport.setAutoFillBackground(False)
        viewport.setAttribute(Qt.WA_TranslucentBackground, False)
        viewport.setAttribute(Qt.WA_OpaquePaintEvent, False)
        viewport_palette = viewport.palette()
        viewport_palette.setColor(QPalette.Window, Qt.transparent)
        viewport_palette.setColor(QPalette.Base, Qt.transparent)
        viewport.setPalette(viewport_palette)

    def _apply_content_label_colors(self) -> None:
        for label in self.findChildren(fluentqt.Label):
            object_name = label.objectName()
            if object_name in _CONTENT_PRIMARY_LABEL_NAMES:
                _apply_content_label_color(label, primary=True)
            elif object_name in _CONTENT_SECONDARY_LABEL_NAMES:
                _apply_content_label_color(label, primary=False)

    def onThemeUpdated(self) -> None:
        super().onThemeUpdated()
        self._apply_transparent_surface()
        self._apply_content_label_colors()

    def showEvent(self, event) -> None:
        super().showEvent(event)
        if not self._gallery_first_show:
            return
        self._gallery_first_show = False
        scroll_bar = self.verticalScrollBar()
        for delay_ms in (0, 50, 250):
            _single_shot(
                delay_ms,
                scroll_bar,
                lambda bar=scroll_bar: bar.setValue(bar.minimum()),
            )


def _scroll_page(
    object_name: str,
    margins: tuple[int, int, int, int] = CONTENT_MARGINS,
) -> tuple[fluentqt.ScrollView, QVBoxLayout]:
    scroll = _GalleryContentScrollView()
    scroll.setObjectName(object_name)
    scroll.setWidgetResizable(True)
    scroll.setFrameShape(QFrame.NoFrame)
    scroll.setHorizontalScrollMode(fluentqt.ScrollView.ScrollMode.Disabled)
    scroll.setHorizontalScrollBarVisibility(
        fluentqt.ScrollView.ScrollBarVisibility.Hidden
    )
    scroll.setVerticalScrollBarVisibility(
        fluentqt.ScrollView.ScrollBarVisibility.Visible
    )
    content = QWidget()
    content.setObjectName(object_name + ".Content")
    content.setAutoFillBackground(False)
    content.setAttribute(Qt.WA_OpaquePaintEvent, False)
    content_palette = content.palette()
    content_palette.setColor(QPalette.Window, Qt.transparent)
    content_palette.setColor(QPalette.Base, Qt.transparent)
    content.setPalette(content_palette)
    layout = QVBoxLayout(content)
    layout.setContentsMargins(*margins)
    layout.setSpacing(16)
    layout.setAlignment(Qt.AlignTop)
    scroll.setOwnedContentWidget(content)
    scroll._gallery_content = content
    scroll._gallery_layout = layout
    return scroll, layout


def _heading(text: str, parent: QWidget) -> fluentqt.Label:
    label = fluentqt.Label(text, parent)
    label.setObjectName("galleryContentTitle")
    label.setFluentTypography(fluentqt.FontRole.Title)
    label.setWordWrap(True)
    _apply_content_label_color(label, primary=True)
    return label


def _section_heading(text: str, parent: QWidget) -> fluentqt.Label:
    label = fluentqt.Label(text, parent)
    label.setObjectName("galleryContentSectionHeader")
    label.setFluentTypography(fluentqt.FontRole.Subtitle)
    label.setWordWrap(True)
    _apply_content_label_color(label, primary=True)
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


def _body(text: str, parent: QWidget) -> fluentqt.Label:
    label = fluentqt.Label(text, parent)
    label.setObjectName("galleryContentBody")
    label.setFluentTypography(fluentqt.FontRole.Body)
    label.setWordWrap(True)
    _apply_content_label_color(label, primary=False)
    return label


def _enum_name(value: object) -> str:
    name = value.name
    return name.decode("ascii") if isinstance(name, bytes) else str(name)


def _add_page_header(
    layout: QVBoxLayout,
    content: QWidget,
    title: str,
    subtitle: str = "",
    action: QWidget | None = None,
) -> None:
    row = QWidget(content)
    row.setObjectName("galleryContentHeaderRow")
    row_layout = QHBoxLayout(row)
    row_layout.setContentsMargins(0, 0, 0, 0)
    row_layout.setSpacing(8)
    row_layout.addWidget(_heading(title, row), 1, Qt.AlignVCenter)
    if action is not None:
        row_layout.addWidget(action, 0, Qt.AlignVCenter)
    layout.addWidget(row)
    if subtitle:
        subtitle_label = _body(subtitle, content)
        subtitle_label.setObjectName("galleryContentSubtitle")
        _apply_content_label_color(subtitle_label, primary=False)
        layout.addWidget(subtitle_label)


def _component_card(
    entry: GalleryEntry,
    navigate: Callable[[str], None],
    parent: QWidget,
) -> GalleryEntryCard:
    card = GalleryEntryCard(
        entry.route_id,
        entry.title,
        entry.description,
        image_path=control_image_path(entry.category_id, entry.title),
        parent=parent,
    )
    card.activated.connect(navigate)
    return card


def _route_card(
    route: GalleryRoute,
    navigate: Callable[[str], None],
    parent: QWidget,
    image_category: str = "",
) -> GalleryEntryCard:
    image = (
        control_image_path(image_category, route.title)
        if image_category
        else None
    )
    card = GalleryEntryCard(
        route.id,
        route.title,
        route.description,
        image_path=image,
        icon_name=route_icon_name(route.id),
        parent=parent,
    )
    card.activated.connect(navigate)
    return card


def _add_entry_grid(
    layout: QVBoxLayout,
    cards: Iterable[GalleryEntryCard],
    object_name: str,
    parent: QWidget,
) -> GalleryEntryGrid:
    grid = GalleryEntryGrid(parent)
    grid.setObjectName(object_name)
    grid.set_cards(cards)
    layout.addWidget(grid)
    return grid


def build_home_page(
    navigate: Callable[[str], None],
    parent: QWidget | None = None,
) -> fluentqt.ScrollView:
    page, layout = _scroll_page(
        "FluentQtPythonGallery.HomePage",
        margins=(0, 0, 0, 0),
    )
    page.setParent(parent)
    content = page._gallery_content
    layout.setSpacing(0)

    hero = GalleryHomeHero(content)
    layout.addWidget(hero)

    body = QWidget(content)
    body.setObjectName("galleryHomeBody")
    body_layout = QVBoxLayout(body)
    body_layout.setContentsMargins(24, 20, 24, 48)
    body_layout.setSpacing(16)
    body_layout.setAlignment(Qt.AlignTop)

    featured_header = _section_heading("Featured components", body)
    featured_header.setObjectName("galleryHomeFeaturedHeader")
    body_layout.addWidget(featured_header)
    featured_cards = [
        _component_card(ENTRY_BY_ROUTE_ID[route_id], navigate, body)
        for route_id in FEATURED_ROUTES
    ]
    featured_grid = _add_entry_grid(
        body_layout,
        featured_cards,
        "galleryHomeCards",
        body,
    )

    body_layout.addSpacing(12)
    categories_header = _section_heading("Browse by category", body)
    categories_header.setObjectName("galleryHomeCategoriesHeader")
    body_layout.addWidget(categories_header)
    category_routes = [
        ROUTE_BY_ID["all-controls"],
        ROUTE_BY_ID["foundation"],
    ] + [
        ROUTE_BY_ID[category.id]
        for category in CATEGORIES
        if category.id != "foundation"
    ]
    category_cards = [
        _route_card(route, navigate, body) for route in category_routes
    ]
    category_grid = _add_entry_grid(
        body_layout,
        category_cards,
        "galleryHomeCategoryCards",
        body,
    )
    layout.addWidget(body)

    page._gallery_hero = hero
    page._gallery_buttons = tuple(featured_cards + category_cards)
    page._gallery_featured_grid = featured_grid
    page._gallery_category_grid = category_grid
    return page


def build_category_page(
    route: GalleryRoute,
    entries: Iterable[GalleryEntry],
    navigate: Callable[[str], None],
    parent: QWidget | None = None,
) -> fluentqt.ScrollView:
    page, layout = _scroll_page(
        "FluentQtPythonGallery.CategoryPage.{0}".format(route.id)
    )
    page.setParent(parent)
    content = page._gallery_content
    _add_page_header(layout, content, route.title, route.description)
    _add_section_heading(layout, "Components", content)
    cards = [_component_card(entry, navigate, content) for entry in entries]
    grid = _add_entry_grid(
        layout,
        cards,
        "galleryCategoryCards.{0}".format(route.id),
        content,
    )
    layout.addStretch()
    page._gallery_buttons = tuple(cards)
    page._gallery_entry_grid = grid
    return page


def build_all_controls_page(
    navigate: Callable[[str], None],
    parent: QWidget | None = None,
) -> fluentqt.ScrollView:
    route = ROUTE_BY_ID["all-controls"]
    page, layout = _scroll_page("FluentQtPythonGallery.CategoryPage.all-controls")
    page.setParent(parent)
    content = page._gallery_content
    _add_page_header(layout, content, "All controls", route.description)
    _add_section_heading(layout, "Components", content)

    foundation_route_ids = (
        "foundation-qmlplus",
        "foundation-typography",
        "foundation-color",
        "foundation-iconography",
        "font-icon",
        "foundation-geometry",
        "foundation-spacing",
    )
    cards = []
    for route_id in foundation_route_ids:
        if route_id == "font-icon":
            cards.append(
                _component_card(ENTRY_BY_ROUTE_ID[route_id], navigate, content)
            )
        else:
            cards.append(
                _route_card(
                    ROUTE_BY_ID[route_id],
                    navigate,
                    content,
                    image_category="foundation",
                )
            )
    cards.extend(
        _component_card(entry, navigate, content)
        for entry in ENTRIES
        if entry.route_id != "font-icon"
    )
    grid = _add_entry_grid(
        layout,
        cards,
        "galleryCategoryCards.all-controls",
        content,
    )
    layout.addStretch()
    page._gallery_buttons = tuple(cards)
    page._gallery_entry_grid = grid
    return page


def build_foundation_page(
    navigate: Callable[[str], None],
    parent: QWidget | None = None,
) -> fluentqt.ScrollView:
    page, layout = _scroll_page("FluentQtPythonGallery.FoundationPage")
    page.setParent(parent)
    content = page._gallery_content
    route = ROUTE_BY_ID["foundation"]
    _add_page_header(layout, content, route.title, route.description)
    _add_section_heading(layout, "Topics", content)
    child_routes = tuple(
        candidate for candidate in ROUTES if candidate.parent_id == "foundation"
    )
    cards = [
        _route_card(child, navigate, content, image_category="foundation")
        for child in child_routes
    ]
    grid = _add_entry_grid(
        layout,
        cards,
        "galleryFoundationCards",
        content,
    )
    layout.addStretch()
    page._gallery_buttons = tuple(cards)
    page._gallery_entry_grid = grid
    return page


class _TypographyRampCard(fluentqt.Card):
    _ROWS = (
        (fluentqt.FontRole.Display, "Display", 68, 92, "SemiBold"),
        (fluentqt.FontRole.TitleLarge, "Title Large", 40, 52, "SemiBold"),
        (fluentqt.FontRole.Title, "Title", 28, 36, "SemiBold"),
        (fluentqt.FontRole.Subtitle, "Subtitle", 20, 28, "SemiBold"),
        (
            fluentqt.FontRole.BodyLargeStrong,
            "Body Large Strong",
            18,
            24,
            "SemiBold",
        ),
        (fluentqt.FontRole.BodyLarge, "Body Large", 18, 24, "Regular"),
        (fluentqt.FontRole.BodyStrong, "Body Strong", 14, 20, "SemiBold"),
        (fluentqt.FontRole.Body, "Body", 14, 20, "Regular"),
        (fluentqt.FontRole.Caption, "Caption", 12, 16, "Regular"),
    )

    def __init__(self, parent: QWidget) -> None:
        super().__init__(parent)
        self.setAppearance(fluentqt.Card.Appearance.Layer)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)

    def _row_height(self, role: object, line_height: int) -> int:
        return max(line_height, QFontMetrics(fluentqt.font_for_role(role)).height())

    def _total_height(self) -> int:
        return 40 + sum(
            self._row_height(role, line_height)
            for role, _name, _size, line_height, _weight in self._ROWS
        ) + 12 * (len(self._ROWS) - 1)

    def sizeHint(self) -> QSize:
        return QSize(480, self._total_height())

    def minimumSizeHint(self) -> QSize:
        return QSize(0, self._total_height())

    def paintEvent(self, event) -> None:
        super().paintEvent(event)
        colors = gallery_colors()
        painter = QPainter(self)
        painter.setRenderHint(QPainter.TextAntialiasing)
        caption_font = fluentqt.font_for_role(fluentqt.FontRole.Caption)
        content_left = 20
        content_right = self.width() - 20
        metrics_width = 220
        y = 20
        divider = (
            QColor(255, 255, 255, 20)
            if fluentqt.current_theme() == fluentqt.Theme.Dark
            else QColor(0, 0, 0, 20)
        )
        for index, (role, name, size, line_height, weight) in enumerate(self._ROWS):
            role_font = fluentqt.font_for_role(role)
            height = self._row_height(role, line_height)
            painter.setFont(role_font)
            painter.setPen(colors.text_primary)
            painter.drawText(
                QRect(
                    content_left,
                    y,
                    content_right - content_left - metrics_width,
                    height,
                ),
                Qt.AlignLeft | Qt.AlignVCenter,
                name,
            )
            painter.setFont(caption_font)
            painter.setPen(colors.text_secondary)
            painter.drawText(
                QRect(content_right - metrics_width, y, metrics_width, height),
                Qt.AlignRight | Qt.AlignVCenter,
                "{0} / {1} · {2}".format(size, line_height, weight),
            )
            y += height
            if index != len(self._ROWS) - 1:
                painter.setPen(QPen(divider, 1.0))
                painter.drawLine(content_left, y + 6, content_right, y + 6)
                y += 12


def _typography_specimens(parent: QWidget) -> QWidget:
    return _TypographyRampCard(parent)


def _iconography_specimens(parent: QWidget) -> QWidget:
    card = fluentqt.Card(parent)
    card.setAppearance(fluentqt.Card.Appearance.Layer)
    grid = QGridLayout(card)
    grid.setContentsMargins(20, 18, 20, 20)
    grid.setSpacing(12)
    icons = (
        "ic_fluent_home_20_regular",
        "ic_fluent_settings_20_regular",
        "ic_fluent_search_20_regular",
        "ic_fluent_add_20_regular",
        "ic_fluent_calendar_20_regular",
        "ic_fluent_checkmark_20_regular",
        "ic_fluent_copy_20_regular",
        "ic_fluent_edit_20_regular",
        "ic_fluent_grid_20_regular",
        "ic_fluent_info_20_regular",
        "ic_fluent_navigation_20_regular",
        "ic_fluent_save_20_regular",
    )
    for index, icon_name in enumerate(icons):
        tile = fluentqt.Card(card)
        tile.setAppearance(fluentqt.Card.Appearance.LayerAlt)
        tile.setFixedSize(72, 64)
        tile_layout = QVBoxLayout(tile)
        tile_layout.setContentsMargins(0, 0, 0, 0)
        glyph = fluentqt.FontIcon(icon_name, tile)
        glyph.setFixedSize(28, 28)
        tile_layout.addWidget(glyph, 0, Qt.AlignCenter)
        grid.addWidget(tile, index // 6, index % 6)
    grid.setColumnStretch(6, 1)
    return card


def _foundation_token_card(route: GalleryRoute, parent: QWidget) -> QWidget:
    card = fluentqt.Card(parent)
    card.setAppearance(fluentqt.Card.Appearance.Layer)
    layout = QVBoxLayout(card)
    layout.setContentsMargins(20, 18, 20, 20)
    layout.setSpacing(12)
    labels = {
        "foundation-qmlplus": (
            "Anchor layouts",
            "Reactive property binding",
            "Named visual states",
        ),
        "foundation-color": (
            "Text and fill tokens",
            "Stroke and background tokens",
            "Light and dark palettes",
        ),
        "foundation-geometry": (
            "4 px control radius",
            "8 px card radius",
            "1 px standard stroke",
        ),
        "foundation-spacing": (
            "4 px base unit",
            "12 px related-item gap",
            "24 px page inset",
        ),
    }.get(route.id, (route.description,))
    for text in labels:
        row = fluentqt.Card(card)
        row.setAppearance(fluentqt.Card.Appearance.LayerAlt)
        row.setMinimumHeight(54)
        row_layout = QHBoxLayout(row)
        row_layout.setContentsMargins(16, 10, 16, 10)
        label = fluentqt.Label(text, row)
        label.setFluentTypography(fluentqt.FontRole.BodyStrong)
        row_layout.addWidget(label)
        row_layout.addStretch()
        layout.addWidget(row)
    return card


def build_foundation_topic_page(
    route: GalleryRoute,
    navigate: Callable[[str], None],
    parent: QWidget | None = None,
) -> fluentqt.ScrollView:
    del navigate
    page, layout = _scroll_page(
        "FluentQtPythonGallery.FoundationTopicPage.{0}".format(route.id)
    )
    page.setParent(parent)
    content = page._gallery_content
    _add_page_header(layout, content, route.title, route.description)
    widgets = populate_foundation_topic_page(route.id, layout, content)
    page._gallery_foundation_widgets = widgets
    page._gallery_foundation_specimen = widgets[0]
    page._gallery_foundation_code_blocks = tuple(
        widget for widget in widgets if isinstance(widget, GalleryCodeBlock)
    )
    return page


def _refresh_fluent_subtree(root: QWidget) -> None:
    widgets = [root] + root.findChildren(QWidget)
    for widget in widgets:
        refresh = getattr(widget, "onThemeUpdated", None)
        if not callable(refresh):
            refresh = getattr(widget, "on_theme_updated", None)
        if callable(refresh):
            refresh()
        # Item views expose update(QModelIndex), which hides QWidget.update()
        # in some PySide releases. Call the base overload explicitly so a
        # theme refresh remains valid for every QWidget subclass.
        QWidget.update(widget)


class _GallerySampleCardLayout(QVBoxLayout):
    """Preserve AnchorLayout's explicit gaps inside a styled QFrame."""

    def setGeometry(self, rect: QRect) -> None:
        super().setGeometry(rect)
        previous: QWidget | None = None
        for index in range(self.count()):
            widget = self.itemAt(index).widget()
            if widget is None:
                continue
            if previous is not None:
                anchored_y = previous.geometry().bottom() + 1 + self.spacing()
                if widget.y() != anchored_y:
                    widget.move(widget.x(), anchored_y)
            previous = widget


class _GallerySampleCard(QFrame):
    """Responsive port of the native ``GallerySampleCard`` geometry."""

    _LEFT_MARGIN = 20
    _TOP_MARGIN = 18
    _RIGHT_MARGIN = 20
    _BOTTOM_MARGIN = 18
    _SPACING = 12
    _DEFAULT_WIDTH = 640
    _MINIMUM_WIDTH = 280

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("gallerySampleCard")
        self.setFrameShape(QFrame.NoFrame)
        self.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        self._gallery_title: QWidget | None = None
        self._gallery_description: QWidget | None = None
        self._gallery_preview_surface: QWidget | None = None
        self._gallery_preview_widget: QWidget | None = None
        self._gallery_source_expander: QWidget | None = None
        self._layout_update_queued = False
        self._updating_layout = False
        self.refresh_theme()

    @staticmethod
    def _preferred_height(widget: QWidget | None, width: int) -> int:
        if widget is None:
            return 0
        if width > 0 and widget.hasHeightForWidth():
            return max(0, widget.heightForWidth(width))
        return max(0, widget.sizeHint().height())

    def _content_width(self, width: int) -> int:
        return max(0, width - self._LEFT_MARGIN - self._RIGHT_MARGIN)

    def _calculated_height(self, width: int) -> int:
        content_width = self._content_width(width)
        widgets = (
            self._gallery_title,
            self._gallery_description,
            self._gallery_preview_surface,
            self._gallery_source_expander,
        )
        heights = [
            self._preferred_height(widget, content_width)
            for widget in widgets
            if widget is not None
        ]
        return (
            self._TOP_MARGIN
            + self._BOTTOM_MARGIN
            + sum(heights)
            + self._SPACING * max(0, len(heights) - 1)
        )

    def sizeHint(self) -> QSize:
        width = max(self.width(), self._DEFAULT_WIDTH)
        return QSize(width, self._calculated_height(width))

    def minimumSizeHint(self) -> QSize:
        return QSize(
            self._MINIMUM_WIDTH,
            self._calculated_height(self._MINIMUM_WIDTH),
        )

    def _update_anchored_layout(self) -> None:
        self._layout_update_queued = False
        if self._updating_layout:
            return
        self._updating_layout = True
        try:
            card_width = max(self.width(), self._MINIMUM_WIDTH)
            content_width = self._content_width(card_width)
            for widget in (
                self._gallery_title,
                self._gallery_description,
                self._gallery_preview_surface,
            ):
                if widget is not None:
                    widget.setFixedHeight(
                        self._preferred_height(widget, content_width)
                    )
            self.setFixedHeight(self._calculated_height(card_width))
            if self.layout() is not None:
                self.layout().activate()
            self.updateGeometry()
        finally:
            self._updating_layout = False

    def _queue_layout_update(self, *_unused: object) -> None:
        if self._layout_update_queued:
            return
        self._layout_update_queued = True
        _single_shot(0, self, self._update_anchored_layout)

    def eventFilter(self, watched, event) -> bool:
        if event.type() == QEvent.Type.LayoutRequest and watched in (
            self._gallery_preview_surface,
            self._gallery_preview_widget,
            self._gallery_source_expander,
        ):
            self._queue_layout_update()
        return super().eventFilter(watched, event)

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self._update_anchored_layout()

    def refresh_theme(self) -> None:
        colors = gallery_colors()
        self.setStyleSheet(
            "#gallerySampleCard {{ background: {0}; border: 1px solid {1}; "
            "border-radius: 8px; }}".format(
                css_color(colors.layer), css_color(colors.stroke)
            )
        )
        title = self.findChild(fluentqt.Label, "gallerySampleCardTitle")
        if title is not None:
            title.setStyleSheet(
                "color: {0}; background: transparent;".format(
                    css_color(colors.text_primary)
                )
            )
        description = self.findChild(
            fluentqt.Label, "gallerySampleCardDescription"
        )
        if description is not None:
            description.setStyleSheet(
                "color: {0}; background: transparent;".format(
                    css_color(colors.text_secondary)
                )
            )


def _build_sample_card(
    entry: GalleryEntry,
    sample: GallerySampleEntry,
    parent: QWidget,
) -> tuple[QFrame, PreviewResult]:
    card = _GallerySampleCard(parent)
    card.setProperty("gallerySampleId", sample.id)
    card.setProperty("galleryRouteId", entry.route_id)
    card_layout = _GallerySampleCardLayout(card)
    card_layout.setContentsMargins(20, 18, 20, 18)
    card_layout.setSpacing(12)

    title = fluentqt.Label(sample.title, card)
    title.setObjectName("gallerySampleCardTitle")
    title.setFluentTypography(fluentqt.FontRole.BodyStrong)
    card_layout.addWidget(title)
    description = None
    if sample.description:
        description = fluentqt.Label(sample.description, card)
        description.setObjectName("gallerySampleCardDescription")
        description.setFluentTypography(fluentqt.FontRole.Body)
        description.setTextColorRole(
            fluentqt.Label.TextColorRole.Secondary
        )
        description.setWordWrap(True)
        card_layout.addWidget(description)

    preview_surface = fluentqt.Card(card)
    preview_surface.setObjectName("gallerySampleCardPreview")
    preview_surface.setProperty("gallerySampleId", sample.id)
    preview_surface.setProperty("galleryRouteId", entry.route_id)
    preview_surface.setAppearance(fluentqt.Card.Appearance.LayerAlt)
    preview_surface.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
    preview_layout = QHBoxLayout(preview_surface)
    preview_layout.setContentsMargins(20, 20, 20, 20)
    preview_layout.setSpacing(16)
    result = build_sample(entry.route_id, sample.id, preview_surface)
    result.widget.setObjectName("gallerySamplePreviewWidget")
    preview_layout.addWidget(result.widget)
    preview_layout.addStretch(1)
    preview_layout.activate()
    card_layout.addWidget(preview_surface)

    source_expander = GalleryCodeBlock(result.source, card)
    source_expander.setObjectName("galleryCodeBlock")
    source_expander.setProperty("gallerySampleId", sample.id)
    source_expander.setProperty("galleryRouteId", entry.route_id)
    card_layout.addWidget(source_expander)

    card._gallery_title = title
    card._gallery_description = description
    card._gallery_preview_surface = preview_surface
    card._gallery_preview_widget = result.widget
    card._gallery_source_expander = source_expander
    preview_surface.installEventFilter(card)
    result.widget.installEventFilter(card)
    source_expander.installEventFilter(card)
    source_expander.layoutHeightChanged.connect(card._update_anchored_layout)
    card._update_anchored_layout()
    card.refresh_theme()

    card._gallery_result = result
    card._gallery_preview_surface = preview_surface
    card._gallery_source_expander = source_expander
    return card, result


def _update_component_theme_button(page: fluentqt.ScrollView) -> None:
    theme = page._gallery_sample_theme
    is_dark = theme == fluentqt.Theme.Dark
    theme_name = "Dark" if is_dark else "Light"
    next_theme_name = "Light" if is_dark else "Dark"
    description = "Preview theme: {0}. Switch to {1}.".format(
        theme_name, next_theme_name
    )
    button = page._gallery_theme_button
    button.setProperty("gallerySampleTheme", theme_name)
    button.setAccessibleName(description)
    fluentqt.ToolTip.attach(button, description)
    glyph = "ic_fluent_weather_moon_16_regular" if is_dark else "\uE706"
    button.setProperty("gallerySampleThemeGlyph", glyph)
    button.setIconGlyph(glyph, 16)


def _sync_component_sample_theme(page: fluentqt.ScrollView) -> None:
    """Mirror GalleryComponentPage::onThemeUpdated for global changes."""

    if page._gallery_sample_theme_explicit:
        # An explicit preview override survives application-theme changes.
        for card in page._gallery_sample_cards:
            surface = card._gallery_preview_surface
            surface.setProperty(
                "fluentThemeOverride", int(page._gallery_sample_theme)
            )
            _refresh_fluent_subtree(surface)
            card._update_anchored_layout()
    else:
        # Without an override, C++ previews follow the global theme directly.
        page._gallery_sample_theme = fluentqt.current_theme()
    _update_component_theme_button(page)


def _toggle_component_sample_theme(page: fluentqt.ScrollView) -> None:
    current = page._gallery_sample_theme
    target = (
        fluentqt.Theme.Light
        if current == fluentqt.Theme.Dark
        else fluentqt.Theme.Dark
    )
    for card in page._gallery_sample_cards:
        surface = card._gallery_preview_surface
        surface.setProperty("fluentThemeOverride", int(target))
        _refresh_fluent_subtree(surface)
        card._update_anchored_layout()
    page._gallery_sample_theme = target
    page._gallery_sample_theme_explicit = True
    _update_component_theme_button(page)


def build_component_page(
    entry: GalleryEntry,
    navigate: Callable[[str], None],
    parent: QWidget | None = None,
) -> fluentqt.ScrollView:
    page, layout = _scroll_page(
        "FluentQtPythonGallery.ComponentPage.{0}".format(entry.route_id)
    )
    page.setParent(parent)
    content = page._gallery_content
    category = CATEGORY_BY_ID[entry.category_id]

    theme_button = fluentqt.Button("", content)
    theme_button.setObjectName("galleryComponentPageThemeButton")
    theme_button.setFluentStyle(fluentqt.Button.ButtonStyle.Standard)
    theme_button.setFluentLayout(fluentqt.Button.ButtonLayout.IconOnly)
    theme_button.setIconGlyph(
        "ic_fluent_weather_moon_16_regular"
        if fluentqt.current_theme() == fluentqt.Theme.Dark
        else "\uE706",
        16,
    )
    theme_button.setFixedSize(32, 32)
    _add_page_header(layout, content, entry.title, action=theme_button)

    _add_section_heading(layout, "Overview", content)
    layout.addWidget(_body(entry.description, content))
    _add_section_heading(layout, "Use", content)
    reference = GalleryReferenceCard(entry.name, entry.category_id, content)
    layout.addWidget(reference)
    _add_section_heading(layout, "Live examples", content)

    cards = []
    results = []
    for sample in entry.samples:
        card, result = _build_sample_card(entry, sample, content)
        layout.addWidget(card)
        cards.append(card)
        results.append(result)

    _add_section_heading(layout, "Category", content)
    related_route = ROUTE_BY_ID[category.id]
    category_card = _route_card(related_route, navigate, content)
    layout.addWidget(category_card)
    layout.addStretch()

    page._gallery_sample_cards = tuple(cards)
    page._gallery_sample_results = tuple(results)
    page._gallery_reference_card = reference
    page._gallery_category_card = category_card
    page._gallery_theme_button = theme_button
    page._gallery_sample_theme = fluentqt.current_theme()
    page._gallery_sample_theme_explicit = False
    _update_component_theme_button(page)
    theme_button.clicked.connect(lambda: _toggle_component_sample_theme(page))
    return page


class _GallerySettingsPage(_GalleryContentScrollView):
    """Settings scroll surface with the same 640/520 px C++ breakpoints."""

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        layout = getattr(self, "_gallery_layout", None)
        if layout is None:
            return
        narrow = 0 < self.width() < 640
        horizontal = 24 if narrow else 48
        layout.setContentsMargins(horizontal, 24 if narrow else 34, horizontal, 48)
        for row in getattr(self, "_gallery_settings_rows", ()):
            row.set_stacked(narrow or (0 < row.width() < 520))


class _SettingsRow(fluentqt.Card):
    def __init__(
        self,
        icon_name: str,
        title: str,
        subtitle: str,
        trailing: QWidget,
        parent: QWidget,
    ) -> None:
        super().__init__(parent)
        self.setObjectName("gallerySettingsRow")
        self.setAppearance(fluentqt.Card.Appearance.Layer)
        self._trailing = trailing
        self._stacked = False
        self._grid = QGridLayout(self)
        self._grid.setContentsMargins(20, 8, 20, 8)
        self._grid.setHorizontalSpacing(16)
        self._grid.setVerticalSpacing(6)
        self._grid.setColumnStretch(1, 1)
        icon = fluentqt.FontIcon(icon_name, self)
        icon.setObjectName("gallerySettingsRowIcon")
        icon.setIconSize(16)
        icon.setFixedSize(30, 30)
        text = QWidget(self)
        text_layout = QVBoxLayout(text)
        text_layout.setContentsMargins(0, 0, 0, 0)
        text_layout.setSpacing(1)
        title_label = fluentqt.Label(title, text)
        title_label.setFluentTypography(fluentqt.FontRole.BodyStrong)
        title_label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        subtitle_label = fluentqt.Label(subtitle, text)
        subtitle_label.setObjectName("gallerySettingsSubtitle")
        subtitle_label.setFluentTypography(fluentqt.FontRole.Caption)
        subtitle_label.setTextColorRole(
            fluentqt.Label.TextColorRole.Secondary
        )
        subtitle_label.setWordWrap(True)
        text_layout.addStretch(1)
        text_layout.addWidget(title_label)
        text_layout.addWidget(subtitle_label)
        text_layout.addStretch(1)
        self._grid.addWidget(icon, 0, 0, Qt.AlignVCenter)
        self._grid.addWidget(text, 0, 1)
        self._title_label = title_label
        self._subtitle_label = subtitle_label
        self.set_stacked(False, force=True)

    def set_stacked(self, stacked: bool, force: bool = False) -> None:
        if not force and self._stacked == bool(stacked):
            return
        self._stacked = bool(stacked)
        self._trailing.setParent(self)
        self._grid.removeWidget(self._trailing)
        if self._stacked:
            self._grid.addWidget(
                self._trailing, 1, 1, Qt.AlignRight | Qt.AlignVCenter
            )
            self.setMinimumHeight(120)
        else:
            self._grid.addWidget(
                self._trailing, 0, 2, Qt.AlignRight | Qt.AlignVCenter
            )
            self.setMinimumHeight(74)
        self._trailing.show()
        self.updateGeometry()

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self.set_stacked(0 < self.width() < 520)


class _AccentSwatch(QWidget):
    def __init__(
        self,
        color: QColor,
        selected: bool,
        on_activated: Callable[[QColor], None],
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self._color = QColor(color)
        self._selected = bool(selected)
        self._on_activated = on_activated
        self._hovered = False
        self.setFixedSize(30, 30)
        self.setCursor(Qt.PointingHandCursor)
        self.setFocusPolicy(Qt.TabFocus)
        self.setToolTip(self._color.name(QColor.HexRgb).upper())

    def paintEvent(self, event) -> None:
        del event
        from .foundation_pages import _radii, _theme_tokens

        tokens = _theme_tokens()
        control_radius, _overlay_radius = _radii()
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        fill = QRectF(self.rect()).adjusted(2.5, 2.5, -2.5, -2.5)
        if self._selected:
            painter.setPen(QPen(tokens["textPrimary"], 2.0))
            painter.setBrush(Qt.NoBrush)
            painter.drawRoundedRect(
                QRectF(self.rect()).adjusted(1, 1, -1, -1),
                control_radius + 1.5,
                control_radius + 1.5,
            )
            fill = QRectF(self.rect()).adjusted(4.5, 4.5, -4.5, -4.5)
        elif self._hovered:
            painter.setPen(QPen(tokens["strokeStrong"], 1.0))
            painter.setBrush(Qt.NoBrush)
            painter.drawRoundedRect(
                QRectF(self.rect()).adjusted(1, 1, -1, -1),
                control_radius + 1.5,
                control_radius + 1.5,
            )
        painter.setPen(QPen(QColor(0, 0, 0, 30), 1.0))
        painter.setBrush(self._color)
        painter.drawRoundedRect(fill, 5.0, 5.0)

    def enterEvent(self, event) -> None:
        super().enterEvent(event)
        self._hovered = True
        self.update()

    def leaveEvent(self, event) -> None:
        super().leaveEvent(event)
        self._hovered = False
        self.update()

    def _activate(self) -> None:
        self._on_activated(QColor(self._color))

    def mousePressEvent(self, event) -> None:
        if event.button() == Qt.LeftButton:
            self._activate()
            event.accept()
            return
        super().mousePressEvent(event)

    def keyPressEvent(self, event) -> None:
        if event.key() in (Qt.Key_Return, Qt.Key_Space):
            self._activate()
            event.accept()
            return
        super().keyPressEvent(event)


def _notify_gallery_visual_change(widget: QWidget) -> None:
    window = widget.window()
    changed = getattr(window, "_gallery_visuals_changed", None)
    if callable(changed):
        changed()
    else:
        refresh_gallery_visuals(window, visible_only=True)


class _AccentColorControl(QWidget):
    _PRESET_SWATCHES = (
        "#005FB8", "#0078D4", "#6750A4", "#007AFF",
        "#038387", "#0099BC", "#107C10", "#498205",
        "#FFB900", "#FF8C00", "#F7630C", "#CA5010",
        "#D13438", "#E81123", "#EA005E", "#E3008C",
    )

    def __init__(self, parent: QWidget) -> None:
        super().__init__(parent)
        self.setObjectName("gallerySettingsAccentControl")
        self.setCursor(Qt.PointingHandCursor)
        self.setFocusPolicy(Qt.TabFocus)
        self.setMinimumSize(64, 32)
        self.setToolTip("Accent color")
        self._hovered = False

    def sizeHint(self) -> QSize:
        return QSize(64, 32)

    def paintEvent(self, event) -> None:
        del event
        from .foundation_pages import _radii, _theme_tokens

        tokens = _theme_tokens()
        dark = fluentqt.current_theme() == fluentqt.Theme.Dark
        control_radius, _overlay_radius = _radii()
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        box = QRectF(self.rect()).adjusted(0.5, 0.5, -0.5, -0.5)
        painter.setPen(QPen(tokens["strokeDefault"], 1.0))
        painter.setBrush(tokens["controlDefault"])
        painter.drawRoundedRect(box, control_radius, control_radius)
        if self._hovered:
            painter.setPen(Qt.NoPen)
            painter.setBrush(
                QColor(255, 255, 255, 14)
                if dark
                else QColor(0, 0, 0, 10)
            )
            painter.drawRoundedRect(box, control_radius, control_radius)
        swatch = QRectF(8, 7, 24, 18)
        painter.setPen(QPen(QColor(0, 0, 0, 30), 1.0))
        painter.setBrush(QColor(fluentqt.accent_color()))
        painter.drawRoundedRect(swatch, 4.0, 4.0)
        _draw_native_font_icon(
            painter,
            QRectF(self.width() - 22, 0, 18, self.height()),
            "\uE70D",
            12,
            tokens["textSecondary"],
            resolve_optical=False,
        )

    def enterEvent(self, event) -> None:
        self._hovered = True
        self.update()
        super().enterEvent(event)

    def leaveEvent(self, event) -> None:
        self._hovered = False
        self.update()
        super().leaveEvent(event)

    def mousePressEvent(self, event) -> None:
        if event.button() == Qt.LeftButton:
            self._open_flyout()
            event.accept()
            return
        super().mousePressEvent(event)

    def keyPressEvent(self, event) -> None:
        if event.key() in (Qt.Key_Return, Qt.Key_Space):
            self._open_flyout()
            event.accept()
            return
        super().keyPressEvent(event)

    @staticmethod
    def _same_rgb(first: QColor, second: QColor) -> bool:
        return (
            first.isValid()
            and second.isValid()
            and first.rgb() == second.rgb()
        )

    def _apply_accent(
        self, color: QColor, flyout: fluentqt.Flyout
    ) -> None:
        gallery_settings().set_accent_color(color)
        _notify_gallery_visual_change(self)
        flyout.close()

    def _reset_accent(self, flyout: fluentqt.Flyout) -> None:
        gallery_settings().reset_accent_color()
        _notify_gallery_visual_change(self)
        flyout.close()

    def _open_themes_folder(self, flyout: fluentqt.Flyout) -> None:
        folder = Path(
            QStandardPaths.writableLocation(
                QStandardPaths.StandardLocation.AppLocalDataLocation
            )
        ) / "themes"
        folder.mkdir(parents=True, exist_ok=True)
        QDesktopServices.openUrl(QUrl.fromLocalFile(str(folder)))
        flyout.close()

    def _open_flyout(self) -> None:
        flyout = fluentqt.Flyout(self.window())
        flyout.setPlacement(fluentqt.Flyout.Placement.Bottom)
        flyout.closed.connect(flyout.deleteLater)
        layout = QVBoxLayout(flyout)
        layout.setContentsMargins(16, 14, 16, 14)
        layout.setSpacing(10)

        title = fluentqt.Label("Accent color", flyout)
        title.setFluentTypography(fluentqt.FontRole.BodyStrong)
        layout.addWidget(title)

        grid = QGridLayout()
        grid.setSpacing(6)
        current = QColor(fluentqt.accent_color())
        for index, color_name in enumerate(self._PRESET_SWATCHES):
            color = QColor(color_name)
            swatch = _AccentSwatch(
                color,
                self._same_rgb(color, current),
                lambda selected, host=flyout: self._apply_accent(
                    selected, host
                ),
                flyout,
            )
            grid.addWidget(swatch, index // 4, index % 4)
        layout.addLayout(grid)

        actions = QHBoxLayout()
        actions.setSpacing(8)
        custom = fluentqt.Button("Custom…", flyout)
        custom.setFluentStyle(fluentqt.Button.ButtonStyle.Standard)
        custom.setFluentSize(fluentqt.Button.ButtonSize.Small)
        reset = fluentqt.Button("Reset", flyout)
        reset.setFluentStyle(fluentqt.Button.ButtonStyle.Subtle)
        reset.setFluentSize(fluentqt.Button.ButtonSize.Small)

        def open_custom() -> None:
            flyout.close()
            _single_shot(
                0, self, lambda: self._open_custom_picker(self)
            )

        custom.clicked.connect(open_custom)
        reset.clicked.connect(lambda: self._reset_accent(flyout))
        actions.addWidget(custom)
        actions.addWidget(reset)
        actions.addStretch(1)
        layout.addLayout(actions)

        folder = fluentqt.HyperlinkButton("Open themes folder", flyout)
        folder.setFluentSize(fluentqt.Button.ButtonSize.Small)
        folder.clicked.connect(lambda: self._open_themes_folder(flyout))
        layout.addWidget(folder)
        flyout.showAt(self)

    def _open_custom_picker(self, anchor: QWidget) -> None:
        flyout = fluentqt.Flyout(self.window())
        flyout.setPlacement(fluentqt.Flyout.Placement.Bottom)
        flyout.closed.connect(flyout.deleteLater)
        layout = QVBoxLayout(flyout)
        layout.setContentsMargins(16, 14, 16, 14)
        layout.setSpacing(12)
        picker = fluentqt.ColorPicker(flyout)
        picker.setAlphaEnabled(False)
        picker.setColor(fluentqt.accent_color())
        layout.addWidget(picker)
        actions = QHBoxLayout()
        actions.addStretch(1)
        cancel = fluentqt.Button("Cancel", flyout)
        cancel.setFluentSize(fluentqt.Button.ButtonSize.Small)
        apply = fluentqt.Button("Apply", flyout)
        apply.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
        apply.setFluentSize(fluentqt.Button.ButtonSize.Small)
        cancel.clicked.connect(flyout.close)

        def apply_color() -> None:
            gallery_settings().set_accent_color(picker.color())
            _notify_gallery_visual_change(self)
            flyout.close()

        apply.clicked.connect(apply_color)
        actions.addWidget(cancel)
        actions.addWidget(apply)
        layout.addLayout(actions)
        flyout.showAt(anchor)


def _settings_choice(
    object_name: str,
    choices: Iterable[str],
    current_index: int,
    parent: QWidget,
) -> fluentqt.ComboBox:
    combo = fluentqt.ComboBox(parent)
    combo.setObjectName(object_name)
    combo.addItems(list(choices))
    combo.setCurrentIndex(current_index)
    combo.setMinimumWidth(140)
    combo.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
    return combo


def _settings_section(title: str, parent: QWidget) -> fluentqt.Label:
    label = fluentqt.Label(title, parent)
    label.setObjectName("gallerySettingsSectionTitle")
    label.setFluentTypography(fluentqt.FontRole.BodyStrong)
    label.setMinimumHeight(24)
    return label


def build_settings_page(
    set_theme_mode: Callable[[int], None],
    set_style: Callable[[int], None],
    set_navigation_style: Callable[[int], None] | None = None,
    set_effect: Callable[[int], None] | None = None,
    set_close_behavior: Callable[[int], None] | None = None,
    parent: QWidget | None = None,
) -> fluentqt.ScrollView:
    settings = gallery_settings()
    page = _GallerySettingsPage(parent)
    page.setObjectName("gallerySettingsPage")
    page.setWidgetResizable(True)
    page.setFrameShape(QFrame.NoFrame)
    page.setHorizontalScrollMode(fluentqt.ScrollView.ScrollMode.Disabled)
    page.setHorizontalScrollBarVisibility(
        fluentqt.ScrollView.ScrollBarVisibility.Hidden
    )
    page.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
    content = QWidget()
    content.setObjectName("gallerySettingsViewport")
    content.setAutoFillBackground(False)
    layout = QVBoxLayout(content)
    layout.setContentsMargins(48, 34, 48, 48)
    layout.setSpacing(10)
    page.setOwnedContentWidget(content)
    page._gallery_content = content
    page._gallery_layout = layout

    title = _heading("Settings", content)
    title.setObjectName("gallerySettingsTitle")
    layout.addWidget(title)
    layout.addSpacing(16)

    theme = _settings_choice(
        "gallerySettingsThemeChoice",
        ("Use system setting", "Light", "Dark"),
        int(settings.theme_mode),
        content,
    )
    style = _settings_choice(
        "gallerySettingsStyleChoice",
        ("Fluent (Windows)", "Material 3 (Google)", "macOS"),
        int(settings.style_theme),
        content,
    )
    navigation = _settings_choice(
        "gallerySettingsNavigationChoice",
        ("Left", "Top"),
        1 if settings.navigation_style == NavigationStyle.Top else 0,
        content,
    )
    effect = _settings_choice(
        "gallerySettingsEffectChoice",
        ("Normal", "Mica", "Acrylic"),
        int(settings.window_effect),
        content,
    )
    close_behavior = _settings_choice(
        "gallerySettingsCloseBehaviorChoice",
        ("Minimize window", "Keep in system tray", "Quit app"),
        int(settings.close_behavior),
        content,
    )
    theme.currentIndexChanged.connect(set_theme_mode)
    style.currentIndexChanged.connect(set_style)
    if set_navigation_style is not None:
        navigation.currentIndexChanged.connect(set_navigation_style)
    if set_effect is not None:
        effect.currentIndexChanged.connect(set_effect)
    if set_close_behavior is not None:
        close_behavior.currentIndexChanged.connect(set_close_behavior)

    accent = _AccentColorControl(content)
    style_accent = QWidget(content)
    style_accent.setObjectName("gallerySettingsStyleAccentTrailing")
    style_accent_layout = QHBoxLayout(style_accent)
    style_accent_layout.setContentsMargins(0, 0, 0, 0)
    style_accent_layout.setSpacing(12)
    style_accent_layout.addWidget(style)
    style_accent_layout.addWidget(accent)

    update_panel = QWidget(content)
    update_panel.setObjectName("gallerySettingsUpdateCheckControl")
    update_layout = QHBoxLayout(update_panel)
    update_layout.setContentsMargins(0, 0, 0, 0)
    update_layout.setSpacing(12)
    update_layout.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
    update_checker = GalleryUpdateChecker(page)
    update_status = fluentqt.Label(
        "Current {0} / {1}".format(
            update_checker.current_version(), update_checker.platform_label()
        ),
        update_panel,
    )
    update_status.setObjectName("gallerySettingsUpdateStatus")
    update_status.setFluentTypography(fluentqt.FontRole.Caption)
    update_status.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
    update_status.setAlignment(Qt.AlignRight)
    update_status.setWordWrap(True)
    update_status.setMaximumWidth(240)
    update_status.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
    update = fluentqt.Button("Check updates", update_panel)
    update.setObjectName("gallerySettingsCheckUpdatesButton")
    update.setFluentLayout(fluentqt.Button.ButtonLayout.IconBefore)
    update.setIconGlyph("\ue72c", 16)
    update.setMinimumWidth(148)
    update_action_url = QUrl()

    def start_update_check() -> None:
        nonlocal update_action_url
        if update_checker.is_checking():
            return
        update_action_url = QUrl()
        update_checker.check_for_updates()

    def update_check_started() -> None:
        nonlocal update_action_url
        update_action_url = QUrl()
        update_status.setText("Checking latest release...")
        update.setText("Checking")
        update.setEnabled(False)

    def update_check_finished(result: UpdateResult) -> None:
        nonlocal update_action_url
        update.setEnabled(True)
        update.setToolTip("")
        if result.status == UpdateStatus.UpdateAvailable:
            asset_url = result.asset_url or QUrl()
            release_url = result.release_url or QUrl()
            update_action_url = asset_url if asset_url.isValid() else release_url
            if asset_url.isValid():
                update_status.setText(
                    "Version {0} available / {1}".format(
                        result.latest_version, update_checker.platform_label()
                    )
                )
                update.setText("Download")
                update.setIconGlyph("\ue896", 16)
            else:
                update_status.setText(
                    "Version {0} available".format(result.latest_version)
                )
                update.setText("Open release")
                update.setIconGlyph("\ue71b", 16)
            update.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
            if update_action_url.isValid():
                update.setToolTip(update_action_url.toString())
        elif result.status == UpdateStatus.UpToDate:
            update_action_url = QUrl()
            update_status.setText(
                "Latest version {0}".format(result.current_version)
            )
            update.setFluentStyle(fluentqt.Button.ButtonStyle.Standard)
            update.setText("Check again")
            update.setIconGlyph("\ue72c", 16)
        else:
            update_action_url = QUrl()
            update_status.setText("Update check failed")
            update.setToolTip(result.message)
            update.setFluentStyle(fluentqt.Button.ButtonStyle.Standard)
            update.setText("Try again")
            update.setIconGlyph("\ue72c", 16)

    def open_update_target() -> None:
        if not update_action_url.isValid():
            start_update_check()
            return
        if not QDesktopServices.openUrl(update_action_url):
            update_status.setText("Could not open the update link.")

    update_checker.checkStarted.connect(update_check_started)
    update_checker.checkFinished.connect(update_check_finished)
    update.clicked.connect(open_update_target)
    update_layout.addWidget(update_status, 1, Qt.AlignRight | Qt.AlignVCenter)
    update_layout.addWidget(update, 0, Qt.AlignRight)

    rows = (
        _SettingsRow(
            "\uE706",
            "App theme",
            "Select which app theme to display",
            theme,
            content,
        ),
        _SettingsRow(
            "\uE790",
            "Style & accent color",
            "Switch the palette and shape language (Fluent, Material 3, macOS) and pick an accent",
            style_accent,
            content,
        ),
        _SettingsRow(
            "\uEA37",
            "Navigation style",
            "Choose how the navigation pane is presented",
            navigation,
            content,
        ),
        _SettingsRow(
            "\uE80A",
            "Window background effect",
            "Uses the system compositor when available, otherwise a software Fluent material",
            effect,
            content,
        ),
        _SettingsRow(
            "\uE7E8",
            "Close button behavior",
            "Choose what happens when the main window is closed",
            close_behavior,
            content,
        ),
        _SettingsRow(
            "\uE895",
            "Gallery updates",
            "Check GitHub Releases and open the latest package for this platform",
            update_panel,
            content,
        ),
    )
    layout.addWidget(_settings_section("Appearance & behavior", content))
    for row in rows[:4]:
        layout.addWidget(row)
    layout.addSpacing(10)
    layout.addWidget(_settings_section("App behavior", content))
    layout.addWidget(rows[4])
    layout.addSpacing(10)
    layout.addWidget(_settings_section("Updates", content))
    layout.addWidget(rows[5])
    layout.addStretch(1)
    page._gallery_settings_rows = rows
    page._gallery_settings_choices = (
        theme,
        style,
        navigation,
        effect,
        close_behavior,
    )
    page._gallery_settings_buttons = (accent, update)
    page._gallery_update_checker = update_checker
    page._gallery_update_status = update_status
    page._gallery_update_button = update
    page._gallery_handle_update_result = update_check_finished
    return page


class _GalleryTitleContent(QWidget):
    """Title-bar chrome with the same metrics as GalleryTitleBarController."""

    def __init__(
        self,
        on_back: Callable[[], None],
        on_menu: Callable[[], None],
        on_search: Callable[[str, object], None],
        on_search_text: Callable[[str, object], None],
        search_parent: QWidget | None = None,
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self.setObjectName("FluentQtPythonGallery.TitleContent")
        self._minimal = False
        self._back_available = False
        self._back_reveal = 0.0
        self._bar = search_parent

        back = fluentqt.Button("", self)
        back.setObjectName("GalleryTitleBar.BackButton")
        back.setFluentStyle(fluentqt.Button.ButtonStyle.Subtle)
        back.setFluentLayout(fluentqt.Button.ButtonLayout.IconOnly)
        back.setFluentSize(fluentqt.Button.ButtonSize.Small)
        back.setIconGlyph("\ue830", 16)
        back.setFixedHeight(24)
        back.setFixedWidth(0)
        back.setContentOpacity(0.0)
        back.setFocusPolicy(Qt.NoFocus)
        back.clicked.connect(on_back)
        fluentqt.ToolTip.attach(back, "Back")
        back.setEnabled(False)
        back.installEventFilter(self)

        menu = fluentqt.Button("", self)
        menu.setObjectName("GalleryTitleBar.MenuButton")
        menu.setFluentStyle(fluentqt.Button.ButtonStyle.Subtle)
        menu.setFluentLayout(fluentqt.Button.ButtonLayout.IconOnly)
        menu.setFluentSize(fluentqt.Button.ButtonSize.Small)
        menu.setIconGlyph("\ue700", 16)
        menu.setFixedSize(24, 24)
        menu.setFocusPolicy(Qt.NoFocus)
        menu.clicked.connect(on_menu)
        fluentqt.ToolTip.attach(menu, "Toggle navigation pane")
        menu.installEventFilter(self)

        icon = QLabel(self)
        icon.setObjectName("GalleryTitleBar.AppIcon")
        icon.setFixedSize(18, 18)
        icon.setAlignment(Qt.AlignCenter)

        title = fluentqt.Label("Fluent-Qt Gallery", self)
        title.setObjectName("GalleryTitleBar.Title")
        title.setFluentTypography(fluentqt.FontRole.Caption)
        title.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        title.setFixedSize(144, 24)

        search = fluentqt.AutoSuggestBox(search_parent or self)
        search.setObjectName("GalleryTitleBar.SearchBox")
        search.setPlaceholderText("Search components and examples...")
        search.setSuggestions(sorted(route.title for route in ROUTES))
        search.setInputHeight(28)
        search.setQueryButtonSize(24)
        search.setClearButtonSize(24)
        search.setFixedHeight(28)
        search.setMaximumWidth(360)
        search.setFocusPolicy(Qt.ClickFocus)
        search.querySubmitted.connect(on_search)
        search.textChangedWithReason.connect(on_search_text)
        search.suggestionChosen.connect(
            lambda item: on_search(str(item), item)
        )

        self._back = back
        self._menu = menu
        self._icon = icon
        self._title = title
        self._search = search
        self._chrome_widgets = (back, menu, icon, title, search)
        self._window_active = bool(
            self._bar is not None and self._bar.isWindowActive()
        )
        self._chrome_visible = True
        self._chrome_reveal_opacity = 1.0
        self._back_animation = QVariantAnimation(self)
        self._back_animation.setDuration(250)
        self._back_animation.valueChanged.connect(self._apply_back_reveal)
        self._chrome_animation = QVariantAnimation(self)
        self._chrome_animation.setDuration(250)
        self._chrome_animation.setEasingCurve(QEasingCurve.OutCubic)
        self._chrome_animation.valueChanged.connect(
            self._set_chrome_reveal_opacity
        )
        if self._bar is not None:
            active_changed = getattr(self._bar, "windowActiveChanged", None)
            if active_changed is not None:
                active_changed.connect(self._set_window_active)
        self.refresh_display_scale()
        self._apply_chrome_opacity()

    def eventFilter(self, watched, event) -> bool:
        if watched in (self._back, self._menu):
            if event.type() == QEvent.Type.MouseButtonPress:
                self._start_button_press(watched)
        return super().eventFilter(watched, event)

    @staticmethod
    def _start_button_press(button: fluentqt.Button) -> None:
        if not button.isEnabled():
            return
        animation = getattr(button, "_gallery_title_press_animation", None)
        if animation is None:
            animation = QPropertyAnimation(button, b"iconScale", button)
            animation.setObjectName("galleryTitleBarButtonPressAnimation")
            animation.finished.connect(lambda: button.setIconScale(1.0))
            button._gallery_title_press_animation = animation
        animation.stop()
        button.setIconScale(1.0)
        animation.setDuration(100)
        animation.setEasingCurve(QEasingCurve.OutCubic)
        animation.setStartValue(1.0)
        animation.setKeyValueAt(0.4, 0.86)
        animation.setEndValue(1.0)
        animation.start()

    def _set_window_active(self, active: bool) -> None:
        self._window_active = bool(active)
        self._apply_chrome_opacity()

    def refresh_display_scale(self) -> None:
        source = self._bar if self._bar is not None else self
        self._icon.setPixmap(
            app_icon_pixmap(18, source.devicePixelRatioF())
        )

    def _set_chrome_reveal_opacity(self, value: object) -> None:
        self._chrome_reveal_opacity = max(
            0.0, min(1.0, float(value))
        )
        self._apply_chrome_opacity()

    def set_chrome_visible(
        self, visible: bool, animated: bool = False
    ) -> None:
        visible = bool(visible)
        self._chrome_animation.stop()
        self._chrome_visible = visible
        if not visible:
            self._chrome_reveal_opacity = 0.0
            for widget in self._chrome_widgets:
                widget.setGraphicsEffect(None)
                widget.hide()
            return

        self._back.show()
        self._menu.show()
        self._icon.show()
        self._title.setVisible(not self._minimal)
        self._update_geometry()
        if not animated:
            self._set_chrome_reveal_opacity(1.0)
            return
        self._set_chrome_reveal_opacity(0.0)
        self._chrome_animation.setStartValue(0.0)
        self._chrome_animation.setEndValue(1.0)
        self._chrome_animation.start()

    def _apply_chrome_opacity(self) -> None:
        if not self._chrome_visible:
            return
        activation_opacity = 1.0 if self._window_active else 0.55
        opacity = self._chrome_reveal_opacity * activation_opacity
        for widget in self._chrome_widgets:
            if abs(opacity - 1.0) <= 0.0001:
                widget.setGraphicsEffect(None)
                continue
            effect = widget.graphicsEffect()
            if not isinstance(effect, QGraphicsOpacityEffect):
                effect = QGraphicsOpacityEffect(widget)
                widget.setGraphicsEffect(effect)
            effect.setOpacity(opacity)

    @property
    def search_box(self) -> fluentqt.AutoSuggestBox:
        return self._search

    def set_back_available(self, available: bool) -> None:
        available = bool(available)
        if self._back_available == available:
            return
        self._back_available = available
        self._back.setEnabled(available)
        self._back_animation.stop()
        self._back_animation.setEasingCurve(
            QEasingCurve.Type.OutCubic
            if available
            else QEasingCurve.Type.InOutSine
        )
        self._back_animation.setStartValue(self._back_reveal)
        self._back_animation.setEndValue(1.0 if available else 0.0)
        self._back_animation.start()

    def _apply_back_reveal(self, value: object) -> None:
        self._back_reveal = max(0.0, min(1.0, float(value)))
        self._back.setContentOpacity(self._back_reveal)
        self._update_geometry()

    def set_minimal(self, minimal: bool) -> None:
        self._minimal = bool(minimal)
        self._title.setVisible(self._chrome_visible and not self._minimal)
        self._update_geometry()

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self._update_geometry()

    def _update_geometry(self) -> None:
        bar = self._bar or self.parentWidget()
        if not self._chrome_visible:
            for widget in self._chrome_widgets:
                widget.hide()
            refresh_exclusions = getattr(bar, "refreshChromeExclusions", None)
            if callable(refresh_exclusions):
                refresh_exclusions()
            return
        leading = 0
        trailing = 0
        if bar is not None:
            leading_getter = getattr(bar, "systemReservedLeadingWidth", None)
            trailing_getter = getattr(bar, "systemReservedTrailingWidth", None)
            if callable(leading_getter):
                leading = leading_getter()
            if callable(trailing_getter):
                trailing = trailing_getter()

        content_origin_x = self.mapTo(bar, QPoint(0, 0)).x() if bar else 0
        x = leading + 8
        y_button = max(0, (self.height() - 24) // 2)
        back_width = round(self._back_reveal * 24)
        back_gap = round(self._back_reveal * 8)
        self._back.setFixedWidth(back_width)
        self._back.setGeometry(
            x - content_origin_x,
            y_button,
            back_width,
            24,
        )
        x += back_width + back_gap
        self._menu.setGeometry(x - content_origin_x, y_button, 24, 24)
        x += 32
        self._icon.setGeometry(
            x - content_origin_x,
            max(0, (self.height() - 18) // 2),
            18,
            18,
        )
        x += 26
        if not self._minimal:
            self._title.setGeometry(
                x - content_origin_x,
                max(0, (self.height() - 24) // 2),
                144,
                24,
            )
            x += 152

        bar_width = bar.width() if bar is not None else self.width()
        right = max(x, bar_width - trailing - 8)
        available = right - x
        show_search = available >= 180
        self._search.setVisible(show_search)
        if not show_search:
            return
        width = max(180, min(360, available - 12))
        centered = (bar_width - width) // 2
        search_x = centered if x <= centered <= right - width else x
        self._search.setGeometry(
            search_x,
            max(0, ((bar.height() if bar is not None else self.height()) - 28) // 2),
            width,
            28,
        )
        self._search.raise_()
        refresh_exclusions = getattr(bar, "refreshChromeExclusions", None)
        if callable(refresh_exclusions):
            refresh_exclusions()


class GalleryWindow(fluentqt.Window):
    """Fluent window presenting the C++ Gallery route and sample ledger."""

    def __init__(self, startup_visuals: bool | None = None) -> None:
        super().__init__()
        self._settings = gallery_settings()
        self._startup_visuals = (
            persistence_available()
            if startup_visuals is None
            else bool(startup_visuals)
        )
        self.setObjectName("galleryWindow")
        self.setWindowTitle("Fluent-Qt Gallery")
        self.setWindowIcon(app_icon())
        self.resize(1180, 760)
        self.setMinimumSize(460, 500)
        use_custom_chrome = sys.platform.startswith(("win", "linux"))
        self.setCustomWindowChromeEnabled(use_custom_chrome)
        self.setChromeInteractive(True)
        caption_tooltips = getattr(self, "setCaptionButtonToolTips", None)
        if use_custom_chrome and callable(caption_tooltips):
            caption_tooltips("Minimize", "Maximize", "Close", "Restore")
        caption_accessible = getattr(
            self, "setCaptionButtonAccessibleNames", None
        )
        if use_custom_chrome and callable(caption_accessible):
            caption_accessible("Minimize", "Maximize", "Close", "Restore")
        effects = (
            fluentqt.BackdropEffect.Solid,
            fluentqt.BackdropEffect.Mica,
            fluentqt.BackdropEffect.Acrylic,
        )
        self.setBackdropEffect(effects[int(self._settings.window_effect)])
        self._editing_command_router = gallery_window_editing_command_router(
            self
        )

        self._pages: dict[str, tuple[int, QWidget]] = {}
        self._current_route = ""
        self._history: list[str] = []
        self._splash: GallerySplashScreen | None = None
        self._skeleton: tuple[int, GalleryPageSkeleton] | None = None
        self._intro_tour: GalleryIntroTour | None = None
        self._startup_finished = not self._startup_visuals
        self._navigation_request_id = 0
        self._prewarm_queue: list[str] = []
        self._prewarm_total = 0
        self._prewarm_done = 0
        self._prewarm_timer = QElapsedTimer()
        self._prewarm_paused = False
        self._prewarm_resume_timer = QTimer(self)
        self._prewarm_resume_timer.setSingleShot(True)
        self._prewarm_resume_timer.setInterval(200)
        self._prewarm_resume_timer.timeout.connect(self._resume_prewarm)
        self._navigation_compact_release_timer = QTimer(self)
        self._navigation_compact_release_timer.setInterval(16)
        self._navigation_compact_release_timer.timeout.connect(
            self._release_navigation_compact_state
        )
        self._visual_generation = 0
        self._page_visual_generations: dict[str, int] = {}
        self._style_index = int(self._settings.style_theme)
        self._styles = (
            ("Fluent", fluentqt.StyleTheme.Fluent),
            ("Material", fluentqt.StyleTheme.Material),
            ("macOS", fluentqt.StyleTheme.MacOS),
        )

        self._build_title_bar()
        self._build_navigation_shell()
        self._apply_navigation_style(self._settings.navigation_style)
        self.navigate("home", record_history=False)
        if self._startup_visuals:
            self._start_startup()

    def _start_startup(self) -> None:
        self._startup_finished = False
        self._title_content.set_chrome_visible(False)
        # Match GalleryWindow::installSplashScreen: the startup surface belongs
        # to Window's client-area host, above the complete NavigationView.  The
        # NavigationView content host is only the right-hand page surface and
        # would leave the navigation pane visibly loaded beside the spinner.
        splash = GallerySplashScreen(self.contentHost())
        splash.show()
        splash.raise_()
        self._splash = splash
        self._ensure_skeleton()
        prioritized: list[str] = []
        for route_id in FEATURED_ROUTES + self.all_route_ids():
            if route_id == "home" or route_id in prioritized:
                continue
            prioritized.append(route_id)
        self._prewarm_queue = prioritized
        self._prewarm_total = len(prioritized)
        self._prewarm_done = 0
        self._prewarm_timer.start()
        splash.set_progress(0, 100)
        _single_shot(0, self, self._prewarm_next_route)

    def _prewarm_next_route(self) -> None:
        if self._splash is None:
            return
        if self._prewarm_paused:
            return
        if (
            not self._prewarm_queue
            or self._prewarm_timer.elapsed() >= 3000
        ):
            self._prewarm_queue.clear()
            self._splash.set_progress(100, 100)
            _single_shot(120, self, self._finish_startup)
            return
        route_id = self._prewarm_queue.pop(0)
        self._ensure_page(route_id)
        self._prewarm_done += 1
        page_percent = (
            self._prewarm_done * 100 // max(1, self._prewarm_total)
        )
        time_percent = self._prewarm_timer.elapsed() * 100 // 3000
        self._splash.set_progress(
            max(page_percent, min(time_percent, 100)), 100
        )
        _single_shot(0, self, self._prewarm_next_route)

    def _finish_startup(self) -> None:
        self._startup_finished = True
        self._prewarm_paused = False
        self._title_content.set_chrome_visible(True, animated=True)
        reapply = getattr(self, "reapplySystemBackdrop", None)
        if callable(reapply):
            reapply()
        splash = self._splash
        self._splash = None
        if splash is not None:
            splash.dismiss()
        if not self._settings.intro_completed:
            _single_shot(480, self, self._maybe_start_intro_tour)

    def _maybe_start_intro_tour(self) -> None:
        if self._intro_tour is not None:
            return
        steps = [
            TourStep(
                None,
                "\ue899",
                "Welcome to Fluent Gallery",
                "A live catalog of Fluent controls for Qt, with runnable "
                "samples. Here's a 15-second tour of the essentials.",
                fluentqt.CoachMark.Placement.Auto,
                True,
            ),
            TourStep(
                self._search,
                "\ue721",
                "Search",
                "Find any control or sample by name — just start typing.",
                fluentqt.CoachMark.Placement.Bottom,
            ),
            TourStep(
                self._main_navigation_pane,
                "\ue71d",
                "Browse by category",
                "Controls are grouped by category here. Expand one to "
                "explore its samples.",
                fluentqt.CoachMark.Placement.Right,
            ),
            TourStep(
                self._footer_navigation_pane,
                "\ue713",
                "Make it yours",
                "Switch between light and dark theme and adjust "
                "preferences in Settings.",
                fluentqt.CoachMark.Placement.Right,
            ),
        ]
        tour = GalleryIntroTour(self, self)
        tour.set_steps(steps)
        tour.finished.connect(self._finish_intro_tour)
        self._intro_tour = tour
        tour.start()

    def _finish_intro_tour(self) -> None:
        self._settings.set_intro_completed(True)

    def _defer_prewarm_during_interaction(self) -> None:
        if getattr(self, "_splash", None) is None:
            return
        self._prewarm_paused = True
        self._prewarm_resume_timer.start()

    def event(self, event: QEvent) -> bool:
        result = super().event(event)
        display_scale_events = tuple(
            event_type
            for event_type in (
                getattr(QEvent.Type, "ScreenChangeInternal", None),
                getattr(QEvent.Type, "DevicePixelRatioChange", None),
            )
            if event_type is not None
        )
        if event.type() in display_scale_events:
            # QWidget's DPR changes after the native event has propagated. Match
            # the C++ Gallery by rebuilding DPR-tagged pixmaps on the next turn.
            _single_shot(0, self, self._refresh_display_scale_assets)
        return result

    def _refresh_display_scale_assets(self) -> None:
        if not hasattr(self, "_title_content"):
            return
        refresh_gallery_display_scale(self, visible_only=True)

    def _resume_prewarm(self) -> None:
        self._prewarm_paused = False
        # Resume the state machine even when the queue drained while a window
        # move/resize had it paused.  The next tick emits the same finished
        # transition as GalleryContentPresenter instead of stranding splash.
        if self._splash is not None:
            _single_shot(0, self, self._prewarm_next_route)

    def moveEvent(self, event) -> None:
        super().moveEvent(event)
        self._defer_prewarm_during_interaction()

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self._defer_prewarm_during_interaction()

    def _build_title_bar(self) -> None:
        bar = self.titleBar()
        content = _GalleryTitleContent(
            self.navigate_back,
            self._toggle_navigation_pane,
            self._submit_search,
            self._filter_search_suggestions,
            search_parent=bar,
        )
        bar.setTitleBarHeight(TITLE_BAR_HEIGHT)
        bar.setContentWidget(content)
        content.show()
        content._update_geometry()
        self._title_content = content
        self._back_button = content._back
        self._menu_button = content._menu
        self._search = content.search_box

    def _build_navigation_shell(self) -> None:
        nav = fluentqt.NavigationView()
        nav.setObjectName("galleryNavigationView")
        nav.setDisplayMode(fluentqt.NavigationView.DisplayMode.Auto)
        nav.setCompactModeThresholdWidth(640)
        nav.setExpandedModeThresholdWidth(1008)
        nav.setExpandedPaneWidth(260)
        nav.setCompactPaneWidth(48)
        nav.setAnimationEnabled(True)

        foundation_children = tuple(
            (route.id, route.title)
            for route in ROUTES
            if route.parent_id == "foundation"
        )
        categories = tuple(
            (
                category.id,
                category.title,
                tuple(
                    (entry.route_id, entry.title)
                    for entry in entries_for_category(category.id)
                ),
            )
            for category in CATEGORIES
            if category.id != "foundation"
        )
        main = GalleryNavigationPane(categories, foundation_children, nav)
        footer = GalleryNavigationFooter(nav)
        top_main_items = (
            (
                "home",
                "Home",
                route_icon_name("home"),
                (),
            ),
            (
                "foundation",
                "Foundation",
                route_icon_name("foundation"),
                foundation_children,
            ),
            (
                "all-controls",
                "All",
                route_icon_name("all-controls"),
                (),
            ),
        ) + tuple(
            (
                category_id,
                title,
                route_icon_name(category_id),
                child_routes,
            )
            for category_id, title, child_routes in categories
        )
        top_main = GalleryTopNavigationPane(top_main_items, nav)
        top_main.setObjectName("galleryTopMainNavigationPane")
        top_footer = GalleryTopNavigationPane(
            (
                (
                    "settings",
                    "Settings",
                    route_icon_name("settings"),
                    (),
                ),
            ),
            nav,
        )
        top_footer.setObjectName("galleryTopFooterNavigationPane")
        top_main.hide()
        top_footer.hide()
        main.routeActivated.connect(self.navigate)
        footer.routeActivated.connect(self.navigate)
        top_main.routeActivated.connect(self.navigate)
        top_footer.routeActivated.connect(self.navigate)

        nav.setOwnedMainChromeWidget(main)
        nav.setOwnedFooterChromeWidget(footer)
        nav.effectiveDisplayModeChanged.connect(self._navigation_mode_changed)
        nav.paneOpenChanged.connect(self._navigation_pane_changed)
        nav.setPaneOpen(True)
        main.set_compact(False)
        footer.set_compact(False)

        self.setContentWidget(nav)
        self._navigation_view = nav
        self._main_navigation_pane = main
        self._footer_navigation_pane = footer
        self._top_main_navigation_pane = top_main
        self._top_footer_navigation_pane = top_footer
        self._content_host = nav.contentHost()
        self._content_host.setTransitionAnimationEnabled(False)

    @property
    def current_route(self) -> str:
        return self._current_route

    def all_route_ids(self) -> tuple[str, ...]:
        return tuple(route.id for route in ROUTES)

    def _ensure_page(self, route_id: str) -> tuple[int, QWidget]:
        existing = self._pages.get(route_id)
        if existing is not None:
            return existing
        route = ROUTE_BY_ID.get(route_id)
        if route is None:
            raise KeyError("Unknown native Gallery route: {0}".format(route_id))
        if route.kind == "home":
            page = build_home_page(self.navigate)
        elif route.kind == "foundation":
            page = build_foundation_page(self.navigate)
        elif route.kind == "foundation-topic":
            page = build_foundation_topic_page(route, self.navigate)
        elif route.kind == "all-controls":
            page = build_all_controls_page(self.navigate)
        elif route.kind == "category":
            page = build_category_page(
                route, entries_for_category(route.id), self.navigate
            )
        elif route.kind == "component":
            page = build_component_page(
                ENTRY_BY_ROUTE_ID[route.id],
                self.navigate,
                self._content_host,
            )
        elif route.kind == "settings":
            page = build_settings_page(
                self._set_theme_mode,
                self._set_style,
                self._set_navigation_style,
                self._set_effect,
                self._set_close_behavior,
            )
        else:
            raise KeyError(
                "Unsupported native Gallery route kind: {0}".format(route.kind)
            )
        if not self._content_host.addOwnedPage(page):
            raise RuntimeError("Could not host Gallery route: {0}".format(route_id))
        record = (self._content_host.count() - 1, page)
        self._pages[route_id] = record
        self._page_visual_generations[route_id] = self._visual_generation
        return record

    def _ensure_skeleton(self) -> tuple[int, GalleryPageSkeleton]:
        if self._skeleton is not None:
            return self._skeleton
        skeleton = GalleryPageSkeleton()
        if not self._content_host.addOwnedPage(skeleton):
            raise RuntimeError("Could not host Gallery page skeleton")
        self._skeleton = (self._content_host.count() - 1, skeleton)
        return self._skeleton

    def _finish_cold_navigation(
        self, route_id: str, request_id: int
    ) -> None:
        if (
            self._current_route != route_id
            or self._navigation_request_id != request_id
        ):
            return
        index, _page = self._ensure_page(route_id)
        if (
            self._current_route == route_id
            and self._navigation_request_id == request_id
        ):
            self._content_host.setCurrentIndex(index, 0, False)
            self._refresh_route_visuals(route_id)

    def navigate(
        self,
        route_id: str,
        record_history: bool = True,
        animated: bool | None = None,
    ) -> None:
        del animated
        if route_id not in ROUTE_BY_ID:
            raise KeyError("Unknown native Gallery route: {0}".format(route_id))
        resident = self._pages.get(route_id)
        if route_id == self._current_route:
            self._refresh_route_visuals(route_id)
            return
        if record_history and self._current_route:
            self._history.append(self._current_route)
        self._current_route = route_id
        self._navigation_request_id += 1
        request_id = self._navigation_request_id
        self._title_content.set_back_available(bool(self._history))
        self._sync_navigation_selection(route_id)
        display_mode = self._navigation_view.effectiveDisplayMode()
        route_kind = ROUTE_BY_ID[route_id].kind
        # Match the native Gallery: categories expand in place so users can
        # continue drilling down; only leaf destinations dismiss the flyout.
        is_category = route_kind in ("category", "foundation")
        if (
            self._navigation_view.isPaneOpen()
            and self.isVisible()
            and not is_category
            and display_mode
            in (
                fluentqt.NavigationView.DisplayMode.LeftCompact,
                fluentqt.NavigationView.DisplayMode.LeftMinimal,
            )
        ):
            self._navigation_view.setPaneOpen(False)
        if resident is not None:
            self._content_host.setCurrentIndex(resident[0], 0, False)
            self._refresh_route_visuals(route_id)
            return
        if not self._startup_visuals or not self._startup_finished:
            index, _page = self._ensure_page(route_id)
            self._content_host.setCurrentIndex(index, 0, False)
            self._refresh_route_visuals(route_id)
            return
        skeleton_index, _skeleton = self._ensure_skeleton()
        self._content_host.setCurrentIndex(skeleton_index, 0, False)
        _single_shot(
            32,
            self,
            lambda: self._finish_cold_navigation(route_id, request_id),
        )

    def navigate_home(self) -> None:
        self.navigate("home")

    def navigate_category(self, category_id: str) -> None:
        self.navigate(category_id)

    def navigate_component(self, route_id: str) -> None:
        self.navigate(route_id)

    def navigate_back(self) -> None:
        if not self._history:
            return
        route_id = self._history.pop()
        self.navigate(route_id, record_history=False)
        self._title_content.set_back_available(bool(self._history))

    def visit_all_routes(self, process_events: bool = True) -> list[str]:
        failures = []
        for route_id in self.all_route_ids():
            failure = self.visit_route(route_id, process_events=process_events)
            if failure is not None:
                failures.append(failure)
        return failures

    def visit_route(
        self,
        route_id: str,
        process_events: bool = True,
        record_history: bool = False,
    ) -> str | None:
        try:
            self.navigate(
                route_id,
                record_history=record_history,
                animated=False,
            )
            if process_events:
                QApplication.processEvents()
            index, page = self._pages[route_id]
            if self._content_host.pageWidget(index) is not page:
                raise AssertionError("content host lost the route page")
            entry = ENTRY_BY_ROUTE_ID.get(route_id)
            if entry is not None:
                results = page._gallery_sample_results
                if len(results) != len(entry.samples):
                    raise AssertionError("SampleCard count differs from native contract")
                actual_ids = tuple(result.sample_id for result in results)
                expected_ids = tuple(sample.id for sample in entry.samples)
                if actual_ids != expected_ids:
                    raise AssertionError("SampleCard order differs from native contract")
                for result in results:
                    if result.parity_level != "native-equivalent":
                        raise AssertionError(
                            "{0} is only {1}, not a native-equivalent port".format(
                                result.sample_id, result.parity_level
                            )
                        )
                    if entry.name not in result.covered_types:
                        raise AssertionError(
                            "{0} does not instantiate fluentqt.{1}".format(
                                result.sample_id, entry.name
                            )
                        )
                    if not result.source.strip():
                        raise AssertionError(
                            "{0} has no Python source".format(result.sample_id)
                        )
        except Exception as error:
            return "{0}: {1}: {2}".format(route_id, type(error).__name__, error)
        return None

    def _submit_search(self, query: str, chosen_suggestion: object) -> None:
        chosen = str(chosen_suggestion) if chosen_suggestion else query
        route = _best_search_route(chosen)
        if route is not None:
            self.navigate(route.id)

    def _filter_search_suggestions(self, text: str, _reason: object) -> None:
        if _reason is not None and _reason != (
            fluentqt.AutoSuggestBox.TextChangeReason.UserInput
        ):
            return
        self._search.setSuggestions(_ranked_route_titles(text))

    def _sync_navigation_selection(self, route_id: str) -> None:
        self._main_navigation_pane.sync_selected(route_id)
        self._footer_navigation_pane.sync_selected(route_id)
        self._top_main_navigation_pane.sync_selected(route_id)
        self._top_footer_navigation_pane.sync_selected(route_id)

    def _navigation_mode_changed(self, mode: object) -> None:
        display_mode = fluentqt.NavigationView.DisplayMode(mode)
        self._title_content.set_minimal(
            display_mode == fluentqt.NavigationView.DisplayMode.LeftMinimal
        )
        self._menu_button.setEnabled(
            display_mode != fluentqt.NavigationView.DisplayMode.Top
        )
        self._navigation_view.setPaneOpen(
            display_mode == fluentqt.NavigationView.DisplayMode.Left
        )
        self._apply_navigation_pane_density()
        self._title_content._update_geometry()

    def _navigation_pane_changed(self, opened: bool) -> None:
        del opened
        self._apply_navigation_pane_density()

    def _set_navigation_panes_compact(self, compact: bool) -> None:
        self._main_navigation_pane.set_compact(compact)
        self._footer_navigation_pane.set_compact(compact)

    def _apply_navigation_pane_density(self) -> None:
        self._navigation_compact_release_timer.stop()
        if not self._navigation_view.isPaneOpen():
            self._set_navigation_panes_compact(True)
            return
        progress = float(
            self._navigation_view.property("layoutTransitionProgress") or 0.0
        )
        if self._navigation_view.isAnimationEnabled() and progress < 0.999:
            # Keep labels hidden while the rail is sliding in, then release as
            # soon as NavigationView reports the final geometry.  The old code
            # never performed this release after Top -> Left.
            self._set_navigation_panes_compact(True)
            self._navigation_compact_release_timer.start()
            return
        self._set_navigation_panes_compact(False)

    def _release_navigation_compact_state(self) -> None:
        if not self._navigation_view.isPaneOpen():
            self._navigation_compact_release_timer.stop()
            self._set_navigation_panes_compact(True)
            return
        progress = float(
            self._navigation_view.property("layoutTransitionProgress") or 0.0
        )
        if progress < 0.999:
            return
        self._navigation_compact_release_timer.stop()
        self._set_navigation_panes_compact(False)

    def _toggle_navigation_pane(self) -> None:
        self._navigation_view.setPaneOpen(
            not self._navigation_view.isPaneOpen()
        )

    def _refresh_route_visuals(
        self, route_id: str, *, force: bool = False
    ) -> None:
        record = self._pages.get(route_id)
        if record is None:
            return
        if (
            not force
            and self._page_visual_generations.get(route_id)
            == self._visual_generation
        ):
            return
        refresh_gallery_visuals(record[1])
        _refresh_fluent_subtree(record[1])
        self._page_visual_generations[route_id] = self._visual_generation

    def _gallery_visuals_changed(self) -> None:
        """Refresh chrome/current page now and defer hidden pages until shown."""

        self._visual_generation += 1
        roots = (
            self._title_content,
            self._main_navigation_pane,
            self._footer_navigation_pane,
            self._top_main_navigation_pane,
            self._top_footer_navigation_pane,
            self._splash,
            self._skeleton[1] if self._skeleton is not None else None,
        )
        seen: set[int] = set()
        for root in roots:
            if root is None or id(root) in seen:
                continue
            seen.add(id(root))
            refresh_gallery_visuals(root)
        self._refresh_route_visuals(self._current_route, force=True)

    def _toggle_theme(self) -> None:
        dark = fluentqt.current_theme() != fluentqt.Theme.Dark
        self._set_theme_mode(2 if dark else 1)

    def _cycle_style(self) -> None:
        self._set_style((self._style_index + 1) % len(self._styles))

    def _set_theme_mode(self, index: int) -> None:
        self._settings.set_theme_mode(index)
        self._gallery_visuals_changed()
        self._sync_component_sample_themes()

    def _sync_component_sample_themes(self) -> None:
        for _index, page in self._pages.values():
            if hasattr(page, "_gallery_sample_theme_explicit"):
                _sync_component_sample_theme(page)

    def _set_style(self, index: int) -> None:
        self._style_index = max(0, min(int(index), len(self._styles) - 1))
        self._settings.set_style_theme(self._style_index)
        self._gallery_visuals_changed()

    def _set_navigation_style(self, index: int) -> None:
        style = (
            NavigationStyle.Top
            if int(index) == 1
            else NavigationStyle.Auto
        )
        self._settings.set_navigation_style(style)
        self._apply_navigation_style(style)

    def _apply_navigation_style(self, style: NavigationStyle | int) -> None:
        style = NavigationStyle(int(style))
        top = style == NavigationStyle.Top
        self._navigation_compact_release_timer.stop()
        self._set_top_navigation_chrome(top)
        display_modes = {
            NavigationStyle.Auto: fluentqt.NavigationView.DisplayMode.Auto,
            NavigationStyle.Left: fluentqt.NavigationView.DisplayMode.Left,
            NavigationStyle.LeftCompact: (
                fluentqt.NavigationView.DisplayMode.LeftCompact
            ),
            NavigationStyle.LeftMinimal: (
                fluentqt.NavigationView.DisplayMode.LeftMinimal
            ),
            NavigationStyle.Top: fluentqt.NavigationView.DisplayMode.Top,
        }
        self._navigation_view.setDisplayMode(display_modes[style])
        effective_mode = self._navigation_view.effectiveDisplayMode()
        self._navigation_view.setPaneOpen(
            effective_mode == fluentqt.NavigationView.DisplayMode.Left
        )
        self._apply_navigation_pane_density()
        self._title_content._update_geometry()

    def _set_top_navigation_chrome(self, top: bool) -> None:
        navigation = self._navigation_view
        next_main = (
            self._top_main_navigation_pane
            if top
            else self._main_navigation_pane
        )
        next_footer = (
            self._top_footer_navigation_pane
            if top
            else self._footer_navigation_pane
        )
        if navigation.mainChromeWidget() is not next_main:
            previous = navigation.takeMainChromeWidget()
            if not navigation.setOwnedMainChromeWidget(next_main):
                raise RuntimeError("Could not install Gallery main navigation chrome")
            if previous is not None:
                previous.setParent(navigation)
                previous.hide()
        if navigation.footerChromeWidget() is not next_footer:
            previous = navigation.takeFooterChromeWidget()
            if not navigation.setOwnedFooterChromeWidget(next_footer):
                raise RuntimeError("Could not install Gallery footer navigation chrome")
            if previous is not None:
                previous.setParent(navigation)
                previous.hide()

    def _set_effect(self, index: int) -> None:
        effect_index = max(0, min(int(index), 2))
        self._settings.set_window_effect(effect_index)
        effects = (
            fluentqt.BackdropEffect.Solid,
            fluentqt.BackdropEffect.Mica,
            fluentqt.BackdropEffect.Acrylic,
        )
        self.setBackdropEffect(effects[effect_index])

    def _set_close_behavior(self, index: int) -> None:
        self._settings.set_close_behavior(index)
        self._settings.set_close_behavior_confirmed(True)


__all__ = [
    "GalleryWindow",
    "build_category_page",
    "build_all_controls_page",
    "build_component_page",
    "build_foundation_page",
    "build_foundation_topic_page",
    "build_home_page",
    "build_settings_page",
]
