#ifndef GALLERYICONTILE_H
#define GALLERYICONTILE_H

#include <QPixmap>
#include <QString>
#include <QWidget>

#include "components/foundation/FluentElement.h"

class QPaintEvent;

namespace fluent::gallery {

/**
 * @brief Square control-icon tile rendering the WinUI Gallery image for a control.
 * zh_CN: 方形控件图标块，渲染某控件对应的 WinUI Gallery 图标图片。
 *
 * The image is looked up by control name from bundled resources; controls without a
 * matching asset (project-specific ones) fall back to a neutral placeholder so a
 * designer can supply art later. Tiles can alternatively render an icon-font glyph
 * (used by category cards, which have no per-control art).
 * zh_CN: 图标按控件名从打包资源中查找；没有同名素材的（项目特有控件）回退到中性占位图，
 * 方便后续由设计师补充。也可改为渲染图标字体字符（分类卡片没有控件图片时使用）。
 */
class GalleryIconTile : public QWidget, public fluent::FluentElement {
public:
    explicit GalleryIconTile(const QString& controlName, QWidget* parent = nullptr);

    /**
     * @brief Switches the tile to glyph mode, replacing the image lookup.
     * zh_CN: 切换为字形模式，替代图片查找。
     */
    void setIconGlyph(const QString& glyph);

    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap m_pixmap;
    QString m_iconGlyph;
};

} // namespace fluent::gallery

#endif // GALLERYICONTILE_H
