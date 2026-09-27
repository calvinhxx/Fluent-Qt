#ifndef FLUENTQT_OVERLAYPRESENTATION_H
#define FLUENTQT_OVERLAYPRESENTATION_H

#include <QPoint>
#include <QPointer>
#include <QTransform>
#include <QWidget>

class QMenu;

/**
 * @brief Widget-only host adapter for overlays on transformed presentations.
 * zh_CN: 为变换显示中的浮层提供仅依赖 Widgets 的宿主适配接口。
 *
 * The nearest published root maps its local coordinates to presented local
 * coordinates. Embedded proxy roots continue through their scene viewport;
 * native windows start a new coordinate space. This adapter does
 * not render, reparent widgets, create contexts or change overlay ownership.
 * Call on the GUI thread. The C++ host adapter is intentionally not a Python API.
 * zh_CN: 最近的发布根节点将局部坐标映射到显示局部坐标；嵌入代理经场景视口继续映射，
 * 原生窗口隔离坐标空间。
 * 本接口不绘制、不重设父对象、不创建上下文或改变浮层所有权。仅在 GUI 线程调用，
 * 此 C++ 宿主适配接口不属于 Python 公共 API。
 */
namespace fluent::overlay::presentation {

/**
 * @brief Publish a root-local transform; nonfinite or singular values clear it.
 * zh_CN: 发布根节点局部变换；非有限或不可逆变换会清除当前发布值。
 */
void setTransform(QWidget* root, const QTransform& transform);

/** @brief Restore the root's ordinary untransformed overlay coordinates.
 * zh_CN: 恢复根节点普通、未变换的浮层坐标。 */
void clearTransform(QWidget* root);

/** @brief Whether a widget uses a published transform or an embedded scene presentation.
 * zh_CN: 控件是否使用已发布变换或嵌入场景的显示坐标。 */
bool hasTransform(const QWidget* widget);

/** @brief Map a local point to displayed global coordinates, or ordinary mapToGlobal.
 * zh_CN: 将局部点映射到显示全局坐标；没有发布值时精确沿用 mapToGlobal。 */
QPoint mapToGlobal(const QWidget* widget, const QPoint& localPoint);

/** @brief Borrowed invocation anchor, available while a transformed menu is shown.
 * zh_CN: 在变换菜单的同步显示事件中可用的借用调用锚点。 */
struct MenuAnchor {
    QPointer<QWidget> source;
    QPoint point;
};

/**
 * @brief Read the synchronous popupMenu invocation anchor, or an empty anchor.
 * zh_CN: 读取 popupMenu 同步调用期间的锚点；其余时间返回空锚点。
 *
 * Hosts may retain the guarded source and local point to follow later motion.
 * The source is never owned and becomes null if deleted by an aboutToShow handler.
 * zh_CN: 宿主可保留受保护的源和局部点以跟随移动；不拥有源，aboutToShow 删除源时指针归空。
 */
MenuAnchor menuAnchor(const QMenu* menu);

/**
 * @brief Map before QMenu screen containment; the optional offset stays untransformed.
 * zh_CN: 在 QMenu 屏幕边界约束前映射调用点；可选偏移保持未变换的原生像素。
 *
 * With no transform or embedding this is exactly popup(source->mapToGlobal(point)
 * + offset). Embedded anchors use native popups, not auto-embedded child proxies.
 * Menu ownership, native submenu coordinates and input remain with Qt.
 * zh_CN: 无变换且未嵌入时等同原生 mapToGlobal 后 popup；嵌入锚点使用原生弹窗，
 * 不自动嵌入子代理；菜单所有权、子菜单坐标和输入仍由 Qt 管理。
 */
void popupMenu(QMenu* menu, QWidget* source, const QPoint& localPoint,
               const QPoint& offset = QPoint());

} // namespace fluent::overlay::presentation

#endif // FLUENTQT_OVERLAYPRESENTATION_H
