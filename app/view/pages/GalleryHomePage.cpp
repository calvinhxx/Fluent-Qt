#include "GalleryHomePage.h"

#include <QEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>
#include <QRadialGradient>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <QPainterPath>

#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/textfields/Label.h"
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

constexpr int kHeroHeight = 260;
constexpr int kHeroMarginX = 24;     // Text inset (was 48) — content shifts left overall.
constexpr int kHeroBottomFade = 80;  // Bottom band that dissolves the banner into the page.
constexpr int kBodyMarginX = 24;     // Body content inset (was 48) — matches the hero.
constexpr int kHeroIconSize = 56;

} // namespace

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
        // Bottom margin clears the dissolve band so the tagline stays crisp above it.
        // zh_CN: 底部留白避开渐隐带，使标语在其上方保持清晰。
        layout->setContentsMargins(kHeroMarginX, 36, kHeroMarginX, kHeroBottomFade + 4);
        layout->setSpacing(12);
        layout->addStretch(1);

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

        applyTextPalette();
    }

    void onThemeUpdated() override
    {
        applyTextPalette();
        if (m_titleLabel)
            m_titleLabel->onThemeUpdated();
        if (m_taglineLabel)
            m_taglineLabel->onThemeUpdated();
        update();
    }

protected:
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

        // The banner is full-bleed with only its top corners rounded; its bottom dissolves
        // into the content layer, so the image connects to the page seamlessly (WinUI home).
        // zh_CN: 横幅通栏铺满、仅上方两角圆角；底部淡入内容层，使图像与页面无缝衔接（WinUI 首页）。
        const QRectF banner(0, 0, width(), height());
        const qreal radius = themeRadius().overlay;
        QPainterPath clip = fluent::overlay::roundedCornerRectPath(
            banner, radius, /*TL*/ true, /*TR*/ true, /*BR*/ false, /*BL*/ false);
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
