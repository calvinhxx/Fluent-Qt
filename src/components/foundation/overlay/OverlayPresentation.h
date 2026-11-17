#ifndef FLUENTQT_OVERLAYPRESENTATION_H
#define FLUENTQT_OVERLAYPRESENTATION_H

#include <QPoint>
#include <QPointer>
#include <QTransform>
#include <QWidget>

class QMenu;

/**
 * @brief Maps overlay coordinates for hosts under QTransform or embedded scene presentations (GUI thread only).
 * zh_CN: 为带坐标变换或嵌入场景中的宿主控件提供浮层坐标映射（仅限 GUI 线程调用）。
 */
namespace fluent::overlay::presentation {

/**
 * @brief Publishes a local transform on @p root; non-finite or singular transforms clear it.
 * zh_CN: 为 @p root 设置局部坐标变换；传入无效或不可逆矩阵时清除变换。
 */
void setTransform(QWidget* root, const QTransform& transform);

/**
 * @brief Clears the published transform on @p root and restores normal overlay coordinates.
 * zh_CN: 清除 @p root 的坐标变换，恢复正常浮层坐标。
 */
void clearTransform(QWidget* root);

/**
 * @brief Returns whether @p widget is affected by a published transform or embedded scene.
 * zh_CN: 返回 @p widget 当前是否处于坐标变换或嵌入场景中。
 */
bool hasTransform(const QWidget* widget);

/**
 * @brief Maps @p localPoint to displayed global coordinates, or calls QWidget::mapToGlobal() when untransformed.
 * zh_CN: 将局部坐标映射为显示的全局坐标；无变换时直接调用 mapToGlobal()。
 */
QPoint mapToGlobal(const QWidget* widget, const QPoint& localPoint);

/**
 * @brief Trigger source and local point recorded while opening a menu with popupMenu().
 * zh_CN: 调用 popupMenu() 弹出菜单时记录的触发控件与局部坐标。
 */
struct MenuAnchor {
    QPointer<QWidget> source;
    QPoint point;
};

/**
 * @brief Returns the active invocation anchor while popupMenu() is opening @p menu, or an empty anchor otherwise.
 * zh_CN: 返回 popupMenu() 弹出 @p menu 期间的触发锚点，其他时间返回空锚点。
 */
MenuAnchor menuAnchor(const QMenu* menu);

/**
 * @brief Opens @p menu at the mapped @p localPoint on @p source, adding unscaled @p offset in native pixels.
 * Behaves like menu->popup(source->mapToGlobal(localPoint) + offset) when no transform is active.
 * zh_CN: 在映射后的 @p localPoint 处弹出 @p menu，并叠加不参与变换的原生像素偏移 @p offset；无变换时等效于 menu->popup(source->mapToGlobal(localPoint) + offset)。
 */
void popupMenu(QMenu* menu, QWidget* source, const QPoint& localPoint, const QPoint& offset = QPoint());

} // namespace fluent::overlay::presentation

#endif // FLUENTQT_OVERLAYPRESENTATION_H
