#ifndef GALLERYCONTENTCATALOG_H
#define GALLERYCONTENTCATALOG_H

#include <functional>

#include <QString>
#include <QStringList>
#include <QVector>

class QWidget;

namespace fluent::gallery {

/**
 * @brief Right-side page archetype resolved for a Gallery route.
 * zh_CN: 为 Gallery 路由解析出的右侧页面类型。
 */
enum class GalleryPageKind {
    Home,
    Category,
    Component,
    Settings,
    Fallback
};

/**
 * @brief A single live sample: metadata plus a preview/options widget factory.
 * zh_CN: 单个实样：元数据加上预览/选项控件工厂。
 *
 * The factories build live reusable Fluent controls on demand so the catalog can
 * stay declarative while the component page owns widget lifetime.
 * zh_CN: 工厂按需构建实时 Fluent 控件，使目录保持声明式，控件生命周期由组件页负责。
 */
struct GallerySample {
    QString id;
    QString title;
    QString description;
    QString codeSnippet;
    std::function<QWidget*(QWidget*)> createPreview;
    std::function<QWidget*(QWidget*)> createOptions;
};

/**
 * @brief Right-side page metadata for one Gallery route.
 * zh_CN: 单个 Gallery 路由的右侧页面元数据。
 *
 * Content entries describe what the selected route shows; they intentionally do
 * not drive left navigation tree structure.
 * zh_CN: 内容条目描述选中路由展示的内容；刻意不驱动左侧导航树结构。
 */
struct GalleryContentEntry {
    GalleryPageKind kind = GalleryPageKind::Fallback;
    QString routeId;
    QString title;
    QString description;
    QString categoryId;
    QStringList relatedRouteIds;
    QStringList sampleIds;
};

/**
 * @brief Returns the static Gallery content catalog seeded for this phase.
 * zh_CN: 返回本阶段种子化的静态 Gallery 内容目录。
 */
const QVector<GalleryContentEntry>& galleryContentCatalog();

/**
 * @brief Resolves the content entry for a route ID, or nullptr when uncovered.
 * zh_CN: 按路由 ID 解析内容条目，未覆盖时返回 nullptr。
 */
const GalleryContentEntry* galleryContentEntry(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYCONTENTCATALOG_H
