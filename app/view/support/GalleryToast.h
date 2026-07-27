#ifndef GALLERYTOAST_H
#define GALLERYTOAST_H

#include <QString>

class QWidget;

namespace fluent::gallery {

/**
 * @brief Applies Gallery placement policy to the reusable UILib Toast.
 * zh_CN: 在 anchor 所属窗口上弹出一个短暂的半透明成功提示 toast。
 *
 * Visible text remains application-owned while severity, animation, lifecycle,
 * replacement, and host tracking are provided by fluent::status_info::Toast.
 * zh_CN: toast 在标题栏下方（顶部居中）下滑淡入，短暂停留后淡出并自销毁；它是轻盈的半透明胶囊
 * （后面的窗口内容会透出），对鼠标透明，并替换上一个 toast，避免堆叠。
 */
void showGalleryToast(QWidget* anchor, const QString& message);

} // namespace fluent::gallery

#endif // GALLERYTOAST_H
