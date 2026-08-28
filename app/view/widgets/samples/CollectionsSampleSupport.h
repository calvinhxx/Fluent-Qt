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
