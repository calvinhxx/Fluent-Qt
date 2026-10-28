#include "RenderToTextureCompat_p.h"

#include <QWidget>
#ifdef FLUENT_QT_HAS_WIDGETS_PRIVATE
#include <QtWidgets/private/qwidget_p.h>
#endif

namespace compatibility::detail {

bool schedulePaintInBackingStorePass(QWidget* textureWidget)
{
#ifdef FLUENT_QT_HAS_WIDGETS_PRIVATE
    // Qt 5.15 and 6.x drawWidget() send the paint event of a texture widget reached by
    // a backing-store pass only while this flag is set, and then clear it.
    // zh_CN: Qt 5.15 与 6.x 的 drawWidget() 仅在该标志置位时向经过的纹理控件发送绘制事件，随后清除。
    if (!textureWidget)
        return false;
    auto* widget = QWidgetPrivate::get(textureWidget);
    if (!widget->renderToTexture)
        return false;
    widget->renderToTextureReallyDirty = 1;
    return true;
#else
    Q_UNUSED(textureWidget);
    return false;
#endif
}

} // namespace compatibility::detail
