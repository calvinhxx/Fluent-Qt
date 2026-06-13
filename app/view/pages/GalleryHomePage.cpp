#include "GalleryHomePage.h"

#include <QGridLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QVBoxLayout>
#include <QWidget>

#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "model/GalleryNavigationItem.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "view/shell/AppIcon.h"
#include "view/support/GalleryStyleSupport.h"
#include "view/widgets/GalleryEntryCard.h"
#include "utils/Log.h"

namespace fluent::gallery {
namespace {

constexpr int kCardColumns = 3;
constexpr int kHeroHeight = 260;
constexpr int kHeroMarginX = 48;
constexpr int kBodyMarginX = 48;
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
        layout->setContentsMargins(kHeroMarginX, 40, kHeroMarginX, 36);
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
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool dark = currentTheme() == Dark;

        QLinearGradient wash(rect().topLeft(), rect().bottomRight());
        if (dark) {
            wash.setColorAt(0.0, QColor(0x1B, 0x2A, 0x41));
            wash.setColorAt(0.55, QColor(0x27, 0x22, 0x44));
            wash.setColorAt(1.0, QColor(0x33, 0x26, 0x3C));
        } else {
            wash.setColorAt(0.0, QColor(0xD6, 0xE7, 0xF7));
            wash.setColorAt(0.55, QColor(0xE4, 0xDF, 0xF6));
            wash.setColorAt(1.0, QColor(0xF4, 0xE7, 0xEA));
        }
        painter.fillRect(rect(), wash);

        // Decorative translucent circles drift off the right edge, like the
        // abstract shapes in the WinUI Gallery banner art.
        // zh_CN: 半透明装饰圆漂出右缘，呼应 WinUI Gallery 横幅的抽象图形。
        const QColor circle = dark ? QColor(255, 255, 255, 14) : QColor(255, 255, 255, 90);
        painter.setPen(Qt::NoPen);
        painter.setBrush(circle);
        const int w = width();
        const int h = height();
        painter.drawEllipse(QPointF(w * 0.78, h * 0.15), h * 0.85, h * 0.85);
        painter.drawEllipse(QPointF(w * 0.94, h * 0.9), h * 0.55, h * 0.55);
        painter.drawEllipse(QPointF(w * 0.6, h * 1.05), h * 0.35, h * 0.35);
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

    auto addCardGrid = [this, body, bodyLayout](const QString& objectName) {
        auto* container = new QWidget(body);
        container->setObjectName(objectName);
        auto* grid = new QGridLayout(container);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(12);
        for (int column = 0; column < kCardColumns; ++column)
            grid->setColumnStretch(column, 1);
        bodyLayout->addWidget(container);
        return grid;
    };

    // Featured samples mirror the curated routes from the content catalog.
    // zh_CN: 精选示例来自内容目录的精选路由。
    fluent::textfields::Label* featuredHeader = createTrackedLabel(
        QStringLiteral("Featured samples"), Typography::FontRole::Subtitle, TextRole::Primary);
    featuredHeader->setObjectName(QStringLiteral("galleryHomeFeaturedHeader"));
    bodyLayout->addWidget(featuredHeader);

    QGridLayout* featuredGrid = addCardGrid(QStringLiteral("galleryHomeCards"));
    int cellIndex = 0;
    for (const QString& routeId : entry.relatedRouteIds) {
        const GalleryNavigationItem* item = navigationViewModel.itemById(routeId);
        if (!item)
            continue;
        QString description;
        if (const GalleryContentEntry* componentEntry = galleryContentEntry(routeId))
            description = componentEntry->description;
        auto* card = new GalleryEntryCard(item->id, item->title, description, body);
        connect(card, &GalleryEntryCard::activated,
                this, &GalleryContentPage::routeActivated);
        featuredGrid->addWidget(card, cellIndex / kCardColumns, cellIndex % kCardColumns);
        ++cellIndex;
    }

    const int featuredCount = cellIndex;

    // Category quick links cover the rest of the catalog.
    // zh_CN: 分类捷径覆盖目录其余部分。
    bodyLayout->addSpacing(12);
    fluent::textfields::Label* categoriesHeader = createTrackedLabel(
        QStringLiteral("Browse by category"), Typography::FontRole::Subtitle, TextRole::Primary);
    categoriesHeader->setObjectName(QStringLiteral("galleryHomeCategoriesHeader"));
    bodyLayout->addWidget(categoriesHeader);

    QGridLayout* categoriesGrid = addCardGrid(QStringLiteral("galleryHomeCategoryCards"));
    cellIndex = 0;
    auto addCategoryCard = [&](const GalleryNavigationItem& item) {
        QString description;
        if (const GalleryContentEntry* categoryEntry = galleryContentEntry(item.id))
            description = categoryEntry->description;
        auto* card = new GalleryEntryCard(item.id, item.title, description, body);
        card->setIconGlyph(item.iconGlyph);
        connect(card, &GalleryEntryCard::activated,
                this, &GalleryContentPage::routeActivated);
        categoriesGrid->addWidget(card, cellIndex / kCardColumns, cellIndex % kCardColumns);
        ++cellIndex;
    };
    if (const GalleryNavigationItem* allControls =
            navigationViewModel.itemById(QStringLiteral("all-controls")))
        addCategoryCard(*allControls);
    for (const GalleryNavigationItem& item : navigationViewModel.items()) {
        if (item.kind == GalleryNavigationItem::Kind::CategoryRoute)
            addCategoryCard(item);
    }

    addContentWidget(body);

    LOG_DEBUG(QStringLiteral("GalleryHomePage created routeId=%1 featuredCards=%2 categoryCards=%3")
                  .arg(entry.routeId)
                  .arg(featuredCount)
                  .arg(cellIndex));
}

void GalleryHomePage::onThemeUpdated()
{
    GalleryContentPage::onThemeUpdated();
    if (m_heroBanner)
        m_heroBanner->onThemeUpdated();
}

} // namespace fluent::gallery
