#ifndef GALLERYHOMEPAGE_H
#define GALLERYHOMEPAGE_H

#include "model/GalleryContentCatalog.h"
#include "GalleryContentPage.h"

namespace fluent::gallery {

class GalleryNavigationViewModel;
class GalleryHomeHeroBanner;

/**
 * @brief WinUI-Gallery-style landing page: hero banner plus curated card grids.
 * zh_CN: WinUI-Gallery 风格落地页：hero 横幅加精选卡片网格。
 */
class GalleryHomePage : public GalleryContentPage {
    Q_OBJECT

public:
    GalleryHomePage(const GalleryContentEntry& entry,
                    const GalleryNavigationViewModel& navigationViewModel,
                    QWidget* parent = nullptr);

    void onThemeUpdated() override;

private:
    GalleryHomeHeroBanner* m_heroBanner = nullptr;
};

} // namespace fluent::gallery

#endif // GALLERYHOMEPAGE_H
