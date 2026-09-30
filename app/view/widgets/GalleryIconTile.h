#ifndef GALLERYICONTILE_H
#define GALLERYICONTILE_H

#include <QString>
#include <QWidget>

#include "components/foundation/FluentElement.h"

class QPaintEvent;

namespace fluent::gallery {

/**
 * @brief Square control-icon tile rendering bundled SVG artwork at the display resolution.
 * zh_CN: 方形控件图标块，按显示分辨率渲染内置 SVG 素材。
 *
 * Artwork is looked up by control name. Tiles can alternatively render a bundled
 * icon-font glyph, as used by category cards.
 * zh_CN: 按控件名查找素材；也可渲染内置图标字体字形，供分类卡片使用。
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
    QString m_imageResource;
    QString m_iconGlyph;
};

} // namespace fluent::gallery

#endif // GALLERYICONTILE_H
