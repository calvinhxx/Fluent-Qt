#ifndef GALLERYPAGEFACTORY_H
#define GALLERYPAGEFACTORY_H

#include <QString>

class QWidget;

namespace fluent::gallery {

class GalleryNavigationViewModel;

/**
 * @brief Creates right-side page widgets for Gallery routes.
 * zh_CN: 为 Gallery 路由创建右侧页面控件。
 *
 * The factory maps a route ID to a Home, category, component, Settings, or
 * placeholder fallback page using the app-layer content catalog and navigation
 * model. It keeps route-to-page selection out of GalleryWindow.
 * zh_CN: 工厂结合应用层内容目录与导航模型，将路由 ID 映射为首页、分类页、组件页、
 * 设置页或占位回退页，使路由到页面的选择逻辑独立于 GalleryWindow。
 */
class GalleryPageFactory {
public:
    explicit GalleryPageFactory(const GalleryNavigationViewModel& navigationViewModel);

    /**
     * @brief Creates a new page widget for a route, or nullptr if the route is unknown.
     * zh_CN: 为某路由创建新的页面控件；路由未知时返回 nullptr。
     */
    QWidget* createPage(const QString& routeId) const;

private:
    const GalleryNavigationViewModel& m_navigationViewModel;
};

} // namespace fluent::gallery

#endif // GALLERYPAGEFACTORY_H
