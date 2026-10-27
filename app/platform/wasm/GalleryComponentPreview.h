#pragma once

class QWidget;

namespace fluent::gallery::platform {

// Embed an existing Gallery factory without the Gallery navigation or settings.
// zh_CN: 直接承载现有 Gallery 工厂，不加载 Gallery 导航和设置。
QWidget* createComponentPreview();

} // namespace fluent::gallery::platform
