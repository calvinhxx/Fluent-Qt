#include "GalleryHomePage.h"

#include <QAbstractItemView>
#include <QDesktopServices>
#include <QEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRadialGradient>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <QPainterPath>

#include "components/collections/ListView.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/textfields/Label.h"
#include "design/CornerRadius.h"
#include "design/Typography.h"
#include "model/GalleryComponentCatalog.h"
#include "model/GalleryNavigationItem.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "view/shell/AppIcon.h"
#include "view/support/GalleryStyleSupport.h"
#include "view/widgets/GalleryEntryGrid.h"
#include "utils/Log.h"

namespace fluent::gallery {
namespace {

constexpr int kHeroHeight = 392;
constexpr int kHeroMarginX = 24;     // Text inset (was 48) — content shifts left overall.
constexpr int kHeroBottomFade = 80;  // Bottom band that dissolves the banner into the page.
constexpr int kBodyMarginX = 24;     // Body content inset (was 48) — matches the hero.
constexpr int kHeroIconSize = 56;
constexpr int kHeroLinkCardWidth = 232;
constexpr int kHeroLinkCardHeight = 172;
constexpr int kHeroLinkCardSpacing = 12;
constexpr int kHeroLinkCardPadding = 24;
constexpr int kHeroLinkStripTop = 196;
constexpr int kHeroLinkStripHeight = kHeroLinkCardHeight + 2;
const QString kExternalLinkGlyph = QString::fromUtf16(u"\uE8A7");
const QString kCodeGlyph = QString::fromUtf16(u"\uE943");

enum HomeLinkRole {
    LinkTitleRole = Qt::UserRole + 1,
    LinkDescriptionRole,
    LinkUrlRole,
    LinkIconRole
};

enum class HomeLinkIcon {
    WinUiDesign,
    GitHub,
    Code,
    FluentQt
};

void drawWindowsDesignIcon(QPainter& painter, const QRectF& rect)
{
    const QColor blue(0, 120, 212);
    const qreal gap = 2.0;
    const qreal cell = (qMin(rect.width(), rect.height()) - gap) / 2.0;
    QRectF box(rect.left(), rect.top(), cell, cell);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 120, 212, 235));
    painter.drawRect(box);
    painter.setBrush(QColor(0, 150, 255, 235));
    painter.drawRect(box.translated(cell + gap, 0));
    painter.setBrush(QColor(0, 95, 184, 235));
    painter.drawRect(box.translated(0, cell + gap));
    painter.setBrush(QColor(0, 130, 220, 235));
    painter.drawRect(box.translated(cell + gap, cell + gap));
}

void drawGitHubIcon(QPainter& painter, const QRectF& rect, bool dark)
{
    const qreal side = qMin(rect.width(), rect.height());
    const QRectF circle(rect.left(), rect.top(), side, side);
    painter.setPen(Qt::NoPen);
    painter.setBrush(dark ? QColor(255, 255, 255, 235) : QColor(24, 24, 24));
    painter.drawEllipse(circle);

    const QColor mark = dark ? QColor(24, 24, 24) : QColor(255, 255, 255, 245);
    painter.setBrush(mark);
    QPainterPath head;
    const qreal cx = circle.center().x();
    const qreal cy = circle.center().y() + side * 0.03;
    head.moveTo(cx - side * 0.30, cy - side * 0.04);
    head.lineTo(cx - side * 0.36, cy - side * 0.22);
    head.lineTo(cx - side * 0.18, cy - side * 0.15);
    head.cubicTo(cx - side * 0.08, cy - side * 0.22,
                 cx + side * 0.08, cy - side * 0.22,
                 cx + side * 0.18, cy - side * 0.15);
    head.lineTo(cx + side * 0.36, cy - side * 0.22);
    head.lineTo(cx + side * 0.30, cy - side * 0.04);
    head.cubicTo(cx + side * 0.33, cy + side * 0.22,
                 cx + side * 0.18, cy + side * 0.34,
                 cx, cy + side * 0.34);
    head.cubicTo(cx - side * 0.18, cy + side * 0.34,
                 cx - side * 0.33, cy + side * 0.22,
                 cx - side * 0.30, cy - side * 0.04);
    painter.drawPath(head);

    painter.setBrush(dark ? QColor(255, 255, 255, 235) : QColor(24, 24, 24));
    painter.drawEllipse(QPointF(cx - side * 0.12, cy + side * 0.06), side * 0.025, side * 0.035);
    painter.drawEllipse(QPointF(cx + side * 0.12, cy + side * 0.06), side * 0.025, side * 0.035);
}

void drawCodeIcon(QPainter& painter, const QRectF& rect, const QColor& color)
{
    QFont font(Typography::FontFamily::SegoeFluentIcons);
    font.setPixelSize(28);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(rect, Qt::AlignCenter, kCodeGlyph);
}

} // namespace

class GalleryHomeLinkDelegate : public QStyledItemDelegate, public fluent::FluentElement {
public:
    explicit GalleryHomeLinkDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return QSize(kHeroLinkCardWidth, kHeroLinkCardHeight);
    }

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        if (!painter || !index.isValid())
            return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

        const Colors colors = themeColors();
        const bool dark = currentTheme() == Dark;
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        const bool pressed = option.state.testFlag(QStyle::State_Sunken);

        QRectF cardRect(option.rect);
        cardRect.adjust(0.5, 0.5, -0.5, -0.5);
        QColor fill = colors.bgLayer;
        fill.setAlpha(dark ? 232 : 242);
        if (hovered)
            fill = colors.subtleSecondary;
        if (pressed)
            fill = colors.subtleTertiary;

        painter->setPen(QPen(colors.strokeCard, 1.0));
        painter->setBrush(fill);
        painter->drawRoundedRect(cardRect, ::CornerRadius::Overlay, ::CornerRadius::Overlay);

        const QRectF iconRect(cardRect.left() + kHeroLinkCardPadding,
                              cardRect.top() + kHeroLinkCardPadding,
                              36,
                              36);
        const auto icon = static_cast<HomeLinkIcon>(
            index.data(LinkIconRole).toInt());
        switch (icon) {
        case HomeLinkIcon::WinUiDesign:
            drawWindowsDesignIcon(*painter, iconRect);
            break;
        case HomeLinkIcon::GitHub:
            drawGitHubIcon(*painter, iconRect, dark);
            break;
        case HomeLinkIcon::Code:
            drawCodeIcon(*painter, iconRect, colors.textPrimary);
            break;
        case HomeLinkIcon::FluentQt:
            painter->drawPixmap(iconRect.toRect(),
                                appicon::pixmap(qRound(iconRect.width()),
                                                painter->device()->devicePixelRatioF()));
            break;
        }

        const int textLeft = qRound(cardRect.left()) + kHeroLinkCardPadding;
        const int textWidth = qRound(cardRect.width()) - kHeroLinkCardPadding * 2;
        int textY = qRound(cardRect.top()) + kHeroLinkCardPadding + 36 + 16;

        QFont titleFont = themeFont(Typography::FontRole::BodyStrong).toQFont();
        QFont descFont = themeFont(Typography::FontRole::Caption).toQFont();
        QFontMetrics titleMetrics(titleFont);
        QFontMetrics descMetrics(descFont);

        const QString title = index.data(LinkTitleRole).toString();
        painter->setFont(titleFont);
        painter->setPen(colors.textPrimary);
        painter->drawText(QRect(textLeft, textY, textWidth, titleMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          titleMetrics.elidedText(title, Qt::ElideRight, textWidth));

        textY += titleMetrics.height() + 4;
        const QString description = index.data(LinkDescriptionRole).toString();
        painter->setFont(descFont);
        painter->setPen(colors.textSecondary);
        painter->drawText(QRect(textLeft,
                                textY,
                                textWidth,
                                qRound(cardRect.bottom()) - kHeroLinkCardPadding - textY),
                          Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                          description);

        QFont glyphFont(Typography::FontFamily::SegoeFluentIcons);
        glyphFont.setPixelSize(14);
        painter->setFont(glyphFont);
        painter->setPen(colors.textSecondary);
        const QRect externalRect(qRound(cardRect.right()) - kHeroLinkCardPadding,
                                 qRound(cardRect.bottom()) - kHeroLinkCardPadding,
                                 16,
                                 16);
        painter->drawText(externalRect, Qt::AlignCenter, kExternalLinkGlyph);

        painter->restore();
    }
};

class GalleryHomeLinkStrip : public fluent::collections::ListView {
public:
    explicit GalleryHomeLinkStrip(QWidget* parent = nullptr)
        : fluent::collections::ListView(parent)
    {
        setObjectName(QStringLiteral("galleryHomeHeroLinksView"));
        setAccessibleName(QStringLiteral("Home links"));
        setViewMode(QListView::IconMode);
        setFlow(QListView::LeftToRight);
        setWrapping(false);
        setResizeMode(QListView::Adjust);
        setMovement(QListView::Static);
        setUniformItemSizes(true);
        setGridSize(QSize(kHeroLinkCardWidth, kHeroLinkCardHeight));
        setSpacing(kHeroLinkCardSpacing);
        setFixedHeight(kHeroLinkStripHeight);
        setSelectionMode(ListSelectionMode::None);
        setBackgroundVisible(false);
        setBorderVisible(false);
        setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setAttribute(Qt::WA_TranslucentBackground);
        viewport()->setAttribute(Qt::WA_TranslucentBackground);
        setStyleSheet(QStringLiteral(
            "QListView#galleryHomeHeroLinksView { background: transparent; border: none; }"
            "QListView#galleryHomeHeroLinksView::item { background: transparent; }"));

        auto* delegate = new GalleryHomeLinkDelegate(this);
        setItemDelegate(delegate);

        auto* model = new QStandardItemModel(this);
        auto append = [model](const QString& title,
                              const QString& description,
                              const QString& url,
                              HomeLinkIcon icon) {
            auto* item = new QStandardItem(title);
            item->setEditable(false);
            item->setData(title, LinkTitleRole);
            item->setData(description, LinkDescriptionRole);
            item->setData(QUrl(url), LinkUrlRole);
            item->setData(static_cast<int>(icon), LinkIconRole);
            model->appendRow(item);
        };
        append(QStringLiteral("Design"),
               QStringLiteral("Guidelines and toolkits for creating stunning WinUI experiences."),
               QStringLiteral("https://aka.ms/WinUI/3.0-figma-toolkit"),
               HomeLinkIcon::WinUiDesign);
        append(QStringLiteral("WinUI on GitHub"),
               QStringLiteral("Explore the WinUI Gallery source code and repository."),
               QStringLiteral("https://github.com/microsoft/WinUI-Gallery"),
               HomeLinkIcon::GitHub);
        append(QStringLiteral("Fluent UI controls"),
               QStringLiteral("Explore Fluent UI controls for web experiences."),
               QStringLiteral("https://developer.microsoft.com/en-us/fluentui#/controls/web"),
               HomeLinkIcon::Code);
        append(QStringLiteral("Fluent Qt on GitHub"),
               QStringLiteral("Explore the Fluent Qt source code and repository."),
               QStringLiteral("https://github.com/calvinhxx/Fluent-QT"),
               HomeLinkIcon::FluentQt);
        setModel(model);

        connect(this, &fluent::collections::ListView::itemClicked,
                this, [model](int row) {
                    const QModelIndex index = model->index(row, 0);
                    const QUrl url = index.data(LinkUrlRole).toUrl();
                    if (url.isValid())
                        QDesktopServices::openUrl(url);
                });
    }

    void onThemeUpdated() override
    {
        fluent::collections::ListView::onThemeUpdated();
        if (viewport())
            viewport()->update();
    }
};

/**
 * @brief Full-bleed gradient banner with app icon, title, and tagline.
 * zh_CN: 通栏渐变横幅，含应用图标、标题与标语。
 *
 * Mirrors the WinUI Gallery hero: a soft diagonal wash with translucent
 * decorative circles, recolored per theme.
 * zh_CN: 仿 WinUI Gallery hero：柔和的对角渐变叠加半透明装饰圆，随主题换色。
 */
class GalleryHomeHeroBanner : public QWidget, public fluent::FluentElement {
public:
    explicit GalleryHomeHeroBanner(const QString& title,
                                   const QString& tagline,
                                   QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("galleryHomeHero"));
        setFixedHeight(kHeroHeight);

        auto* layout = new QVBoxLayout(this);
        // The floating link ListView occupies the lower half of the hero, so keep the
        // text block in the upper band and let the card strip sit on the artwork.
        // zh_CN: 悬浮链接 ListView 占用 hero 下半区，因此文字块放在上方，卡片条压在横幅图上。
        layout->setContentsMargins(kHeroMarginX, 36, kHeroMarginX,
                                   kHeroHeight - kHeroLinkStripTop + 12);
        layout->setSpacing(12);

        m_iconLabel = new QLabel(this);
        m_iconLabel->setObjectName(QStringLiteral("galleryHomeHeroIcon"));
        m_iconLabel->setFixedSize(kHeroIconSize, kHeroIconSize);
        m_iconLabel->setPixmap(appicon::pixmap(kHeroIconSize, devicePixelRatioF()));
        m_iconLabel->setStyleSheet(QStringLiteral("background: transparent;"));
        layout->addWidget(m_iconLabel);

        m_titleLabel = new fluent::textfields::Label(title, this);
        m_titleLabel->setObjectName(QStringLiteral("galleryHomeHeroTitle"));
        m_titleLabel->setFluentTypography(Typography::FontRole::TitleLarge);
        layout->addWidget(m_titleLabel);

        m_taglineLabel = new fluent::textfields::Label(tagline, this);
        m_taglineLabel->setObjectName(QStringLiteral("galleryHomeHeroTagline"));
        m_taglineLabel->setFluentTypography(Typography::FontRole::Body);
        m_taglineLabel->setWordWrap(true);
        layout->addWidget(m_taglineLabel);
        layout->addStretch(1);

        m_linkStrip = new GalleryHomeLinkStrip(this);
        m_linkStrip->raise();

        applyTextPalette();
    }

    void onThemeUpdated() override
    {
        applyTextPalette();
        if (m_titleLabel)
            m_titleLabel->onThemeUpdated();
        if (m_taglineLabel)
            m_taglineLabel->onThemeUpdated();
        if (m_linkStrip)
            m_linkStrip->onThemeUpdated();
        update();
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        if (!m_linkStrip)
            return;
        const int stripWidth = qMax(0, width() - kBodyMarginX * 2);
        m_linkStrip->setGeometry(kBodyMarginX,
                                 kHeroLinkStripTop,
                                 stripWidth,
                                 kHeroLinkStripHeight);
    }

    bool event(QEvent* e) override
    {
        if (e->type() == QEvent::WindowActivate || e->type() == QEvent::WindowDeactivate)
            update();  // bottom dissolve tracks the window backdrop
        return QWidget::event(e);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool dark = currentTheme() == Dark;
        const Colors colors = themeColors();

        // The banner is full-bleed with only its top-LEFT corner rounded (matching the content
        // surface, which rounds top-left where it meets the pane and stays square on the right where
        // it runs to the window edge); its bottom dissolves into the content layer so the image
        // connects to the page seamlessly (WinUI home). A rounded top-right would leave a small
        // backdrop notch against the window edge.
        // zh_CN: 横幅通栏铺满、仅左上角圆角（与内容表面一致：左上与窗格相接处圆角，右侧跑到窗口边缘保持直角）；
        // 底部淡入内容层，使图像与页面无缝衔接（WinUI 首页）。右上角若圆角会在窗口边缘留下一小块背景缺口。
        const QRectF banner(0, 0, width(), height());
        const qreal radius = themeRadius().overlay;
        QPainterPath clip = fluent::overlay::roundedCornerRectPath(
            banner, radius, /*TL*/ true, /*TR*/ false, /*BR*/ false, /*BL*/ false);
        painter.save();
        painter.setClipPath(clip);

        QLinearGradient wash(banner.topLeft(), banner.bottomRight());
        if (dark) {
            wash.setColorAt(0.0, QColor(0x1B, 0x2A, 0x41));
            wash.setColorAt(0.55, QColor(0x27, 0x22, 0x44));
            wash.setColorAt(1.0, QColor(0x33, 0x26, 0x3C));
        } else {
            wash.setColorAt(0.0, QColor(0xD6, 0xE7, 0xF7));
            wash.setColorAt(0.55, QColor(0xE4, 0xDF, 0xF6));
            wash.setColorAt(1.0, QColor(0xF4, 0xE7, 0xEA));
        }
        painter.fillRect(banner, wash);

        // Decorative translucent circles drift off the right edge, like the
        // abstract shapes in the WinUI Gallery banner art.
        // zh_CN: 半透明装饰圆漂出右缘，呼应 WinUI Gallery 横幅的抽象图形。
        const QColor circle = dark ? QColor(255, 255, 255, 14) : QColor(255, 255, 255, 90);
        painter.setPen(Qt::NoPen);
        painter.setBrush(circle);
        const qreal w = banner.width();
        const qreal h = banner.height();
        const qreal bx = banner.left();
        const qreal by = banner.top();
        painter.drawEllipse(QPointF(bx + w * 0.80, by + h * 0.12), h * 0.85, h * 0.85);
        painter.drawEllipse(QPointF(bx + w * 0.95, by + h * 0.90), h * 0.55, h * 0.55);
        painter.drawEllipse(QPointF(bx + w * 0.62, by + h * 1.05), h * 0.35, h * 0.35);

        // Bottom dissolve into the content layer surface (bgLayerAlt) painted by the
        // NavigationView behind the transparent page, so the banner transitions seamlessly
        // into the content instead of ending on a hard seam above "Featured samples".
        // zh_CN: 底部渐隐到 NavigationView 在透明页面之后绘制的内容层表面（bgLayerAlt），
        // 使横幅无缝过渡到内容，而非在「Featured samples」上方留硬缝。
        const QRectF fadeRect(banner.left(), banner.bottom() - kHeroBottomFade,
                              banner.width(), kHeroBottomFade);
        QColor base = colors.bgLayerAlt;
        QColor clear = base;
        clear.setAlpha(0);
        QLinearGradient fade(fadeRect.topLeft(), fadeRect.bottomLeft());
        fade.setColorAt(0.0, clear);
        fade.setColorAt(1.0, base);
        painter.fillRect(fadeRect, fade);

        painter.restore();
    }

private:
    void applyTextPalette()
    {
        const Colors colors = themeColors();
        if (m_titleLabel) {
            m_titleLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                            .arg(cssColor(colors.textPrimary)));
        }
        if (m_taglineLabel) {
            m_taglineLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                              .arg(cssColor(colors.textSecondary)));
        }
    }

    QLabel* m_iconLabel = nullptr;
    fluent::textfields::Label* m_titleLabel = nullptr;
    fluent::textfields::Label* m_taglineLabel = nullptr;
    GalleryHomeLinkStrip* m_linkStrip = nullptr;
};

GalleryHomePage::GalleryHomePage(const GalleryContentEntry& entry,
                                 const GalleryNavigationViewModel& navigationViewModel,
                                 QWidget* parent)
    : GalleryContentPage(entry.routeId, entry.title, QString(), parent)
{
    setObjectName(QStringLiteral("galleryHomePage"));

    // The hero replaces the stock heading and spans the full page width.
    // zh_CN: hero 取代默认标题，并横贯整页宽度。
    setPageHeaderVisible(false);
    setViewportMargins(QMargins(0, 0, 0, 0));
    setContentSpacing(0);

    m_heroBanner = new GalleryHomeHeroBanner(
        QStringLiteral("WinUI 3 Gallery"), entry.description, this);
    addContentWidget(m_heroBanner);

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("galleryHomeBody"));
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(kBodyMarginX, 32, kBodyMarginX, 48);
    bodyLayout->setSpacing(16);

    // Each section's cards are drawn by one responsive GalleryEntryGrid (same as the
    // category pages), so they reflow 1/2/3 columns with the window instead of clipping.
    // zh_CN: 每个分区的卡片用一个自适应 GalleryEntryGrid 绘制（与分类页一致），随窗口在 1/2/3 列间重排而非裁切。
    auto addEntryGrid = [this, body, bodyLayout](const QString& objectName) -> GalleryEntryGrid* {
        auto* grid = new GalleryEntryGrid(body);
        grid->setObjectName(objectName);
        connect(grid, &GalleryEntryGrid::activated,
                this, &GalleryContentPage::routeActivated);
        bodyLayout->addWidget(grid);
        return grid;
    };

    // Featured samples mirror the curated routes from the content catalog.
    // zh_CN: 精选示例来自内容目录的精选路由。
    fluent::textfields::Label* featuredHeader = createTrackedLabel(
        QStringLiteral("Featured samples"), Typography::FontRole::Subtitle, TextRole::Primary);
    featuredHeader->setObjectName(QStringLiteral("galleryHomeFeaturedHeader"));
    bodyLayout->addWidget(featuredHeader);

    QVector<GalleryEntryGrid::Entry> featuredEntries;
    for (const QString& routeId : entry.relatedRouteIds) {
        const GalleryNavigationItem* item = navigationViewModel.itemById(routeId);
        if (!item)
            continue;
        QString description;
        if (const GalleryContentEntry* componentEntry = galleryContentEntry(routeId))
            description = componentEntry->description;
        featuredEntries.append({item->id, item->title, description,
                                QPixmap(galleryControlImageResource(item->title)), QString()});
    }
    addEntryGrid(QStringLiteral("galleryHomeCards"))->setEntries(featuredEntries);

    // Category quick links cover the rest of the catalog.
    // zh_CN: 分类捷径覆盖目录其余部分。
    bodyLayout->addSpacing(12);
    fluent::textfields::Label* categoriesHeader = createTrackedLabel(
        QStringLiteral("Browse by category"), Typography::FontRole::Subtitle, TextRole::Primary);
    categoriesHeader->setObjectName(QStringLiteral("galleryHomeCategoriesHeader"));
    bodyLayout->addWidget(categoriesHeader);

    QVector<GalleryEntryGrid::Entry> categoryEntries;
    auto appendCategory = [&](const GalleryNavigationItem& item) {
        QString description;
        if (const GalleryContentEntry* categoryEntry = galleryContentEntry(item.id))
            description = categoryEntry->description;
        categoryEntries.append({item.id, item.title, description, QPixmap(), item.iconGlyph});
    };
    if (const GalleryNavigationItem* allControls =
            navigationViewModel.itemById(QStringLiteral("all-controls")))
        appendCategory(*allControls);
    for (const GalleryNavigationItem& item : navigationViewModel.items()) {
        if (item.kind == GalleryNavigationItem::Kind::CategoryRoute)
            appendCategory(item);
    }
    addEntryGrid(QStringLiteral("galleryHomeCategoryCards"))->setEntries(categoryEntries);

    addContentWidget(body);

    LOG_DEBUG(QStringLiteral("GalleryHomePage created routeId=%1 featuredCards=%2 categoryCards=%3")
                  .arg(entry.routeId)
                  .arg(featuredEntries.size())
                  .arg(categoryEntries.size()));
}

void GalleryHomePage::onThemeUpdated()
{
    GalleryContentPage::onThemeUpdated();
    if (m_heroBanner)
        m_heroBanner->onThemeUpdated();
}

} // namespace fluent::gallery
