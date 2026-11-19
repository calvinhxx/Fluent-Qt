#ifndef GALLERYRELEASEHISTORYVIEW_H
#define GALLERYRELEASEHISTORYVIEW_H

#include "components/collections/ListView.h"

namespace fluent::gallery {

class GalleryReleaseHistoryView final : public collections::ListView {
public:
    explicit GalleryReleaseHistoryView(QWidget* parent);
    void onThemeUpdated() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
};

} // namespace fluent::gallery

#endif
