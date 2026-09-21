#ifndef GALLERYSPATIALSUPPORTBADGE_H
#define GALLERYSPATIALSUPPORTBADGE_H

#include "components/status_info/InfoBadge.h"

namespace fluent::gallery {

// Shared runtime support indicator for navigation and Settings.
// zh_CN: 导航与设置共用的运行时支持状态提示。
class GallerySpatialSupportBadge final : public status_info::InfoBadge {
    Q_OBJECT

public:
    explicit GallerySpatialSupportBadge(QWidget* parent = nullptr);
    void onThemeUpdated() override;

private:
    void refresh();
};

} // namespace fluent::gallery

#endif // GALLERYSPATIALSUPPORTBADGE_H
