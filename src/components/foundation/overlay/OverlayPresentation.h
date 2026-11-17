#ifndef FLUENTQT_OVERLAYPRESENTATION_H
#define FLUENTQT_OVERLAYPRESENTATION_H

#include <QPoint>
#include <QPointer>
#include <QTransform>
#include <QWidget>

class QMenu;

/**
 * @brief Widget-only host adapter for overlays on transformed presentations.
 * zh_CN: 为存在坐标变换的宿主提供浮层坐标适配接口（仅依赖 Qt Widgets）。
 *
 * The nearest published root maps its local coordinates to presented coordinates.
 * Embedded proxy roots continue through their scene viewport; native windows start
 * a new coordinate space. Does not render, reparent widgets, create contexts, or
 * alter overlay ownership. Must be called on the GUI thread.
 * zh_CN: 最近的变换根节点将局部坐标映射到显示坐标；嵌入代理会经场景视口继续映射，
 * 原生窗口开启独立的坐标空间。本接口不负责绘制、不重设父对象、不创建渲染上下文，
 * 也不改变浮层所有权，须在 GUI 线程调用。
 */
namespace fluent::overlay::presentation {

/**
 * @brief Publishes a root-local transform; non-finite or singular transforms clear it.
 * zh_CN: 发布根节点的局部变换；传入非有限值或不可逆矩阵时清除当前变换。
 */
void setTransform(QWidget* root, const QTransform& transform);

/**
 * @brief Restores ordinary untransformed overlay coordinates for @p root.
 * zh_CN: 恢复根节点常规未变换的浮层坐标。
 */
void clearTransform(QWidget* root);

/**
 * @brief Returns whether a widget uses a published transform or embedded scene presentation.
 * zh_CN: 返回控件是否处于已发布的坐标变换或嵌入场景中。
 */
bool hasTransform(const QWidget* widget);

/**
 * @brief Maps a local point to displayed global coordinates, or falls back to mapToGlobal().
 * zh_CN: 将局部坐标映射为显示全局坐标；未设置变换时直接调用 mapToGlobal()。
 */
QPoint mapToGlobal(const QWidget* widget, const QPoint& localPoint);

/**
 * @brief Non-owning invocation anchor available while a transformed menu is opening.
 * zh_CN: 变换菜单弹出期间提供的触发锚点信息（不接管源控件所有权）。
 */
struct MenuAnchor {
    QPointer<QWidget> source;
    QPoint point;
};

/**
 * @brief Returns the active popupMenu() invocation anchor, or an empty anchor otherwise.
 * zh_CN: 返回 popupMenu() 调用期间的触发锚点；其余时间返回空锚点。
 *
 * Callers may store the guarded @c source and local @c point to track later movement.
 * Does not take ownership of @c source; @c source becomes null if destroyed in aboutToShow.
 * zh_CN: 调用方可保留受保护的 source 与局部坐标以跟随后续移动。不接管 source 所有权，
 * 若源控件在 aboutToShow 期间被销毁则 pointer 自动置空。
 */
MenuAnchor menuAnchor(const QMenu* menu);

/**
 * @brief Maps @p localPoint before QMenu screen clamping; @p offset stays in native pixels.
 * zh_CN: 在 QMenu 执行屏幕边界约束前映射调用点；可选偏移保持原生像素不参与变换。
 *
 * With no transform or embedding, behaves identically to popup(source->mapToGlobal(point) + offset).
 * Embedded anchors open native popups; menu ownership, submenu geometry, and input remain with Qt.
 * zh_CN: 无变换且未嵌入时等同于 popup(source->mapToGlobal(point) + offset)；
 * 嵌入场景下的锚点仍使用原生弹窗，菜单所有权、子菜单位置与输入均由 Qt 管理。
 */
void popupMenu(QMenu* menu, QWidget* source, const QPoint& localPoint, const QPoint& offset = QPoint());

} // namespace fluent::overlay::presentation

#endif // FLUENTQT_OVERLAYPRESENTATION_H
