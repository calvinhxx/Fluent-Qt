#ifndef GALLERYCOMPONENTCATALOG_H
#define GALLERYCOMPONENTCATALOG_H

#include <QString>
#include <QVector>

namespace fluent::gallery {

struct GalleryComponentEntry {
    QString id;
    QString title;
    QString iconGlyph;
    QString apiTypeName;
};

struct GalleryComponentCategory {
    QString id;
    QString title;
    QString sourceDirectory;
    QString iconGlyph;
    QVector<GalleryComponentEntry> components;
};

/**
 * @brief Public integration facts shown on a component documentation page.
 * zh_CN: 组件文档页展示的公共集成信息。
 */
struct GalleryComponentReference {
    QString header;
    QString qualifiedType;
    QString cmakeTarget;

    bool isValid() const
    {
        return !header.isEmpty() && !qualifiedType.isEmpty() && !cmakeTarget.isEmpty();
    }
};

const QVector<GalleryComponentCategory>& galleryComponentCatalog();

/** @brief Resolves public integration facts for a component route. */
GalleryComponentReference galleryComponentReference(const QString& routeId);

/**
 * @brief Resolves the bundled control-icon resource for a control title.
 * zh_CN: 按控件标题解析打包的控件图标资源。
 *
 * Images live under `:/app/assets/control_images/<category-id>/<Title>.png`; controls
 * without a matching asset (project-specific ones) fall back to the shared Placeholder.
 * zh_CN: 图标位于 `:/app/assets/control_images/<分类 id>/<标题>.png`；没有同名素材的
 * （项目特有控件）回退到共享的 Placeholder。
 */
QString galleryControlImageResource(const QString& controlTitle);

} // namespace fluent::gallery

#endif // GALLERYCOMPONENTCATALOG_H
