#ifndef GALLERYFOUNDATIONPAGE_H
#define GALLERYFOUNDATIONPAGE_H

#include "GalleryContentPage.h"
#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

class GalleryNavigationViewModel;

/**
 * @brief Foundation landing page: feature cards that drill into design-token topics.
 * zh_CN: 基础落地页：钻入各设计 token 主题的特性卡片。
 *
 * Reuses the shared GalleryEntryGrid so the cards reflow 1/2/3 columns with the
 * window, matching the category and home pages.
 * zh_CN: 复用共享的 GalleryEntryGrid，卡片随窗口在 1/2/3 列间重排，与分类页、首页一致。
 */
class GalleryFoundationPage : public GalleryContentPage {
    Q_OBJECT

public:
    GalleryFoundationPage(const GalleryContentEntry& entry,
                          const GalleryNavigationViewModel& navigationViewModel,
                          QWidget* parent = nullptr);
};

} // namespace fluent::gallery

#endif // GALLERYFOUNDATIONPAGE_H
