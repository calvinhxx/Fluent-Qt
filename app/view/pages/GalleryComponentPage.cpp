#include "GalleryComponentPage.h"

#include "components/basicinput/Button.h"
#include "components/status_info/ToolTip.h"
#include "design/Typography.h"
#include "model/GalleryComponentCatalog.h"
#include "model/GalleryNavigationItem.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "view/widgets/GalleryComponentReferenceCard.h"
#include "view/widgets/GalleryEntryCard.h"
#include "view/widgets/GallerySampleCard.h"
#include "view/widgets/GallerySampleCatalog.h"
#include "support/logging/Log.h"

namespace fluent::gallery {

namespace {
constexpr int kThemeButtonSize = 32;
constexpr int kThemeButtonIconSize = Typography::IconSize::Standard;

QString previewThemeGlyph(FluentElement::Theme theme)
{
    if (theme == FluentElement::Dark) {
        return Typography::Icons::glyph(
            QStringLiteral(
                "ic_fluent_weather_moon_16_regular"));
    }
    return Typography::Icons::Sunny;
}
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
    m_themeButton->setFluentStyle(fluent::basicinput::Button::Standard);
    m_themeButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_themeButton->setFluentSize(fluent::basicinput::Button::StandardSize);
    m_themeButton->setFixedSize(kThemeButtonSize, kThemeButtonSize);
    connect(m_themeButton, &fluent::basicinput::Button::clicked,
            this, &GalleryComponentPage::toggleSampleTheme);
    addHeaderAction(m_themeButton);
    updateThemeButton();

    addSectionHeader(QStringLiteral("Overview"));
    if (!m_overviewText.isEmpty())
        addBodyText(m_overviewText);

    const GalleryComponentReference reference = galleryComponentReference(entry.routeId);
    if (reference.isValid()) {
        addSectionHeader(QStringLiteral("Use"));
        m_referenceCard = new GalleryComponentReferenceCard(reference, this);
        addContentWidget(m_referenceCard);
    } else {
        LOG_WARN(QStringLiteral("GalleryComponentPage reference missing routeId=%1 title=%2")
                     .arg(entry.routeId, entry.title));
    }

    addSectionHeader(QStringLiteral("Live examples"));
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

    if (!entry.relatedRouteIds.isEmpty()) {
        addSectionHeader(QStringLiteral("Category"));
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
    if (m_referenceCard)
        m_referenceCard->onThemeUpdated();
    updateThemeButton();
    // Without a local override the previews follow the global theme manager directly.
    // Reapplying an empty override here used to traverse and relayout every sample twice.
    if (m_sampleThemeExplicit)
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
    const QString themeName = visibleTheme == FluentElement::Dark
        ? QStringLiteral("Dark")
        : QStringLiteral("Light");
    if (m_themeButton->property("gallerySampleTheme").toString() != themeName)
        m_themeButton->setProperty("gallerySampleTheme", themeName);
    const QString nextThemeName =
        visibleTheme == FluentElement::Dark
        ? QStringLiteral("Light")
        : QStringLiteral("Dark");
    const QString description = QStringLiteral(
        "Preview theme: %1. Switch to %2.")
        .arg(themeName, nextThemeName);
    m_themeButton->setAccessibleName(description);
    fluent::status_info::ToolTip::attach(
        m_themeButton, description);
    const QString iconGlyph =
        previewThemeGlyph(visibleTheme);
    m_themeButton->setProperty(
        "gallerySampleThemeGlyph", iconGlyph);
    m_themeButton->setIconGlyph(
        iconGlyph,
        kThemeButtonIconSize);
    m_themeButton->update();
}

} // namespace fluent::gallery
