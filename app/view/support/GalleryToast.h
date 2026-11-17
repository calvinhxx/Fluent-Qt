#ifndef GALLERYTOAST_H
#define GALLERYTOAST_H

#include <QString>

#include "components/status_info/Toast.h"

class QWidget;

namespace fluent::gallery {

/**
 * @brief Shows a transient Gallery Toast positioned below the title bar on @p anchor's window.
 * Visible text is caller-supplied while severity, animation, and replacement follow fluent::status_info::Toast.
 * zh_CN: 在 @p anchor 所属窗口标题栏下方弹出半透明状态提示 Toast；文案由调用方传入，样式、动效与原地替换由 fluent::status_info::Toast 统一处理。
 */
void showGalleryToast(QWidget* anchor,
    const QString& message,
    status_info::Toast::Severity severity = status_info::Toast::Success);

} // namespace fluent::gallery

#endif // GALLERYTOAST_H
