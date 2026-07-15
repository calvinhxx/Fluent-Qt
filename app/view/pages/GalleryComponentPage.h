#ifndef GALLERYCOMPONENTPAGE_H
#define GALLERYCOMPONENTPAGE_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"
#include "GalleryContentPage.h"

namespace fluent::basicinput {
class Button;
}

namespace fluent::gallery {

class GalleryNavigationViewModel;
class GalleryComponentReferenceCard;
class GallerySampleCard;

/**
 * @brief Component documentation page with overview, public API reference, and live examples.
 * zh_CN: 组件文档页，包含概览、公共 API 参考与实时示例。
 */
class GalleryComponentPage : public GalleryContentPage {
    Q_OBJECT

public:
    GalleryComponentPage(const GalleryContentEntry& entry,
                         const GalleryNavigationViewModel& navigationViewModel,
                         QWidget* parent = nullptr);

    QString overviewText() const { return m_overviewText; }
    int sampleCount() const { return m_sampleCards.size(); }
    QVector<GallerySampleCard*> sampleCards() const { return m_sampleCards; }
    GalleryComponentReferenceCard* referenceCard() const { return m_referenceCard; }

    void onThemeUpdated() override;

private:
    void toggleSampleTheme();
    void applySampleTheme();
    void updateThemeButton();

    QString m_overviewText;
    GalleryComponentReferenceCard* m_referenceCard = nullptr;
    QVector<GallerySampleCard*> m_sampleCards;
    ::fluent::basicinput::Button* m_themeButton = nullptr;
    fluent::FluentElement::Theme m_sampleTheme = fluent::FluentElement::Light;
    bool m_sampleThemeExplicit = false;
};

} // namespace fluent::gallery

#endif // GALLERYCOMPONENTPAGE_H
