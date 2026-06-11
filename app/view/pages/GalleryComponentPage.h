#ifndef GALLERYCOMPONENTPAGE_H
#define GALLERYCOMPONENTPAGE_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"
#include "GalleryContentPage.h"

namespace fluent::gallery {

class GalleryNavigationViewModel;
class GallerySampleCard;

/**
 * @brief Component detail page with overview, examples, API notes, and related sections.
 * zh_CN: 组件详情页，含概览、示例、API 说明与相关内容分区。
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

private:
    QString m_overviewText;
    QVector<GallerySampleCard*> m_sampleCards;
};

} // namespace fluent::gallery

#endif // GALLERYCOMPONENTPAGE_H
