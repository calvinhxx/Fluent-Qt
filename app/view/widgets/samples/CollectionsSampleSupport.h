#ifndef GALLERY_COLLECTIONS_SAMPLE_SUPPORT_H
#define GALLERY_COLLECTIONS_SAMPLE_SUPPORT_H

#include "model/GalleryContentCatalog.h"

#include "components/textfields/Label.h"
#include "design/Typography.h"

namespace fluent::gallery::detail {

// Collection views paint a bgLayer surface that reads as an extra sunken layer
// inside the Gallery's bgLayerAlt preview panel. Keep only Gallery sample
// instances flat; the reusable controls retain their default surface elsewhere.
// zh_CN: 集合控件会在 Gallery 预览面板中形成额外的下陷层；这里只展平示例实例，
// 可复用控件在其他位置仍保留默认表面。
template <typename View>
View* flatPreviewSurface(View* view)
{
    if (view) {
        view->setBackgroundVisible(false);
        view->setBorderVisible(false);
        // These transparent views sit on GallerySampleCard's painted LayerAlt
        // preview surface. Keep that local surface on composited Mica windows
        // instead of clearing through it to the OS backdrop.
        // zh_CN: 这些透明集合视图位于 GallerySampleCard 自绘的 LayerAlt 预览面上；
        // 合成式 Mica 下应保留该局部表面，不能向下擦穿到系统背景材质。
        view->setProperty("fluentPreserveParentSurface", true);
        if (view->viewport())
            view->viewport()->setProperty("fluentPreserveParentSurface", true);
    }
    return view;
}

inline textfields::Label* makeStatusLabel(QWidget* parent, const QString& text)
{
    auto* label = new textfields::Label(text, parent);
    label->setFluentTypography(Typography::FontRole::Body);
    // Keep the semantic text role effective on styled preview surfaces.
    label->setTextColorRole(textfields::Label::TextColorRole::Primary);
    return label;
}

QVector<GallerySample> treeViewSamples();

} // namespace fluent::gallery::detail

#endif // GALLERY_COLLECTIONS_SAMPLE_SUPPORT_H
