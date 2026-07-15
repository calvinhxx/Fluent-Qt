#include "GalleryComponentPage.h"

#include "components/basicinput/Button.h"
#include "components/status_info/ToolTip.h"
#include "design/Typography.h"
#include "model/GalleryNavigationItem.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "view/widgets/GalleryEntryCard.h"
#include "view/widgets/GallerySampleCard.h"
#include "view/widgets/GallerySampleCatalog.h"
#include "support/logging/Log.h"

namespace fluent::gallery {

namespace {
constexpr int kThemeButtonSize = 32;
constexpr int kThemeButtonIconSize = 16;
}

GalleryComponentPage::GalleryComponentPage(const GalleryContentEntry& entry,
                                           const GalleryNavigationViewModel& navigationViewModel,
                                           QWidget* parent)
    : GalleryContentPage(entry.routeId, entry.title, QString(), parent)
    , m_overviewText(entry.description)
    , m_sampleTheme(currentTheme())
{
    setObjectName(QStringLiteral("galleryComponentPage"));

    m_themeButton = new fluent::basicinput::Button(this);
    m_themeButton->setObjectName(QStringLiteral("galleryComponentPageThemeButton"));
    m_themeButton->setAccessibleName(QStringLiteral("Toggle theme"));
    m_themeButton->setToolTip(QStringLiteral("Toggle theme"));
    m_themeButton->setFluentStyle(fluent::basicinput::Button::Standard);
    m_themeButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_themeButton->setFluentSize(fluent::basicinput::Button::StandardSize);
    m_themeButton->setFixedSize(kThemeButtonSize, kThemeButtonSize);
    m_themeButton->setIconGlyph(Typography::Icons::Sunny, kThemeButtonIconSize);
    fluent::status_info::ToolTip::attach(m_themeButton, QStringLiteral("Toggle theme"));
    connect(m_themeButton, &fluent::basicinput::Button::clicked,
            this, &GalleryComponentPage::toggleSampleTheme);
    addHeaderAction(m_themeButton);
    updateThemeButton();

    addSectionHeader(QStringLiteral("Overview"));
    if (!m_overviewText.isEmpty())
        addBodyText(m_overviewText);

    addSectionHeader(QStringLiteral("Examples"));
    const QVector<GallerySample> samples = gallerySamplesForRoute(entry.routeId);
    // A component page without samples is a coverage gap in the sample catalog,
    // not a normal state — surface it loudly.
    // zh_CN: 组件页没有任何示例说明示例目录存在覆盖缺口，不是正常状态——大声暴露出来。
    if (samples.isEmpty()) {
        LOG_WARN(QStringLiteral("GalleryComponentPage samples missing routeId=%1 title=%2")
                     .arg(entry.routeId, entry.title));
    }
    for (const GallerySample& sample : samples) {
        auto* card = new GallerySampleCard(sample, this);
        addContentWidget(card);
        m_sampleCards.append(card);
    }
    applySampleTheme();

    addSectionHeader(QStringLiteral("API notes"));
    addBodyText(QStringLiteral(
        "These examples use the public %1 API from the FluentQt UI component library. "
        "No Gallery-specific behavior is added to the reusable control.")
                    .arg(entry.title));

    if (!entry.relatedRouteIds.isEmpty()) {
        addSectionHeader(QStringLiteral("Related"));
        for (const QString& relatedRouteId : entry.relatedRouteIds) {
            const GalleryNavigationItem* relatedItem = navigationViewModel.itemById(relatedRouteId);
            if (!relatedItem)
                continue;
            QString relatedDescription;
            if (const GalleryContentEntry* relatedEntry = galleryContentEntry(relatedRouteId))
                relatedDescription = relatedEntry->description;
            auto* card = new GalleryEntryCard(relatedItem->id,
                                              relatedItem->title,
                                              relatedDescription,
                                              this);
            // Categories have no per-control art; render their nav glyph instead.
            // zh_CN: 分类没有控件图片，改用其导航字形图标。
            if (relatedItem->kind != GalleryNavigationItem::Kind::ComponentRoute)
                card->setIconGlyph(relatedItem->iconGlyph);
            connect(card, &GalleryEntryCard::activated,
                    this, &GalleryContentPage::routeActivated);
            addContentWidget(card);
        }
    }

    LOG_DEBUG(QStringLiteral("GalleryComponentPage created routeId=%1 samples=%2 related=%3")
                  .arg(entry.routeId)
                  .arg(samples.size())
                  .arg(entry.relatedRouteIds.size()));
}

void GalleryComponentPage::onThemeUpdated()
{
    GalleryContentPage::onThemeUpdated();
    if (!m_sampleThemeExplicit)
        m_sampleTheme = currentTheme();
    if (m_themeButton)
        m_themeButton->onThemeUpdated();
    updateThemeButton();
    applySampleTheme();
}

void GalleryComponentPage::toggleSampleTheme()
{
    const FluentElement::Theme currentSampleTheme =
        m_sampleThemeExplicit ? m_sampleTheme : currentTheme();
    m_sampleTheme = currentSampleTheme == FluentElement::Dark
        ? FluentElement::Light
        : FluentElement::Dark;
    m_sampleThemeExplicit = true;
    updateThemeButton();
    applySampleTheme();
}

void GalleryComponentPage::applySampleTheme()
{
    for (GallerySampleCard* card : m_sampleCards) {
        if (!card)
            continue;
        if (m_sampleThemeExplicit)
            card->setPreviewThemeOverride(m_sampleTheme);
        else
            card->clearPreviewThemeOverride();
    }
}

void GalleryComponentPage::updateThemeButton()
{
    if (!m_themeButton)
        return;
    const FluentElement::Theme visibleTheme =
        m_sampleThemeExplicit ? m_sampleTheme : currentTheme();
    m_themeButton->setProperty("gallerySampleTheme",
                               visibleTheme == FluentElement::Dark
                                   ? QStringLiteral("Dark")
                                   : QStringLiteral("Light"));
    m_themeButton->setIconGlyph(Typography::Icons::Sunny, kThemeButtonIconSize);
    m_themeButton->update();
}

} // namespace fluent::gallery
