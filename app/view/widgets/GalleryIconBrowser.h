#ifndef GALLERYICONBROWSER_H
#define GALLERYICONBROWSER_H

#include <QWidget>

#include "components/foundation/FluentElement.h"

namespace fluent::textfields {
class Label;
}

namespace fluent::scrolling {
class PipsPager;
}

namespace fluent::gallery {

/**
 * @brief Virtualized, searchable browser for the complete bundled icon catalog.
 * zh_CN: 用于完整内置图标目录的虚拟化可搜索浏览器。
 */
class GalleryIconBrowser : public QWidget, public fluent::FluentElement {
    Q_OBJECT

public:
    explicit GalleryIconBrowser(QWidget* parent = nullptr);

    void onThemeUpdated() override;

    int iconCount() const;
    int visibleIconCount() const;
    bool showingClosestMatches() const;

private:
    class Grid;

    void updateCountLabel();
    void updatePagination();
    void selectPage(int pageIndex, bool scrollToGrid);
    void copyIcon(int pageItemIndex);

    Grid* m_grid = nullptr;
    fluent::textfields::Label* m_countLabel = nullptr;
    QWidget* m_pagination = nullptr;
    fluent::textfields::Label* m_pageLabel = nullptr;
    fluent::scrolling::PipsPager* m_pager = nullptr;
};

} // namespace fluent::gallery

#endif // GALLERYICONBROWSER_H
