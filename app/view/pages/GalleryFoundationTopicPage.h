#ifndef GALLERYFOUNDATIONTOPICPAGE_H
#define GALLERYFOUNDATIONTOPICPAGE_H

#include "GalleryContentPage.h"
#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

class GalleryNavigationViewModel;

/**
 * @brief One Foundation topic sub-page (typography, color, iconography, geometry).
 * zh_CN: 单个基础主题子页（排版、颜色、图标、几何）。
 *
 * The concrete topic is chosen by routeId; each topic renders live design-token
 * specimens drawn directly from the theme (themeFont / themeColors / themeRadius),
 * so the page stays correct across light/dark and token changes.
 * zh_CN: 具体主题由 routeId 决定；每个主题从主题 token（themeFont / themeColors / themeRadius）
 * 实时绘制样本，明暗与 token 变化时自动保持正确。
 */
class GalleryFoundationTopicPage : public GalleryContentPage {
    Q_OBJECT

public:
    GalleryFoundationTopicPage(const GalleryContentEntry& entry,
                               const GalleryNavigationViewModel& navigationViewModel,
                               QWidget* parent = nullptr);

private:
    void buildQmlPlus();
    void buildTypography();
    void buildColor();
    void buildIconography();
    void buildGeometry();
};

} // namespace fluent::gallery

#endif // GALLERYFOUNDATIONTOPICPAGE_H
