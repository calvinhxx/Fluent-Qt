#ifndef GALLERYSPLASHSCREEN_H
#define GALLERYSPLASHSCREEN_H

#include <FluentQt/StatusInfo.h>

namespace fluent::gallery {

/** @brief Supplies Gallery branding and one-shot ownership to the shared startup surface.
 * zh_CN: 为公共启动遮罩提供 Gallery 图标和一次性生命周期。
 */
class GallerySplashScreen : public fluent::status_info::SplashScreen {
    Q_OBJECT
public:
    explicit GallerySplashScreen(QWidget* parent = nullptr);
};

} // namespace fluent::gallery
#endif // GALLERYSPLASHSCREEN_H
