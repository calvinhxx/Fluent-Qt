#pragma once

#include <functional>

class QWidget;

namespace fluent::gallery::platform {

// Browser-only host for an unchanged Gallery sample, without Gallery navigation or settings.
// zh_CN: 浏览器独立承载原有 Gallery 示例，不加载导航或用户设置。
QWidget* createSpatialShowcase();

// Shared local 3D controls for the homepage and isolated component documents.
// zh_CN: 首页和独立组件文档共用的局部 3D 控制，不读写 Gallery 偏好。
QWidget*
createSpatialPreviewControls(QWidget* preview, QWidget* parent, const std::function<void()>& stateChanged = {});

} // namespace fluent::gallery::platform
