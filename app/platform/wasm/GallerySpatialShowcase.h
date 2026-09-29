#pragma once

class QWidget;

namespace fluent::gallery::platform {

// Browser-only host for an unchanged Gallery sample, without Gallery navigation or settings.
// zh_CN: 浏览器独立承载原有 Gallery 示例，不加载导航或用户设置。
QWidget* createSpatialShowcase();

} // namespace fluent::gallery::platform
