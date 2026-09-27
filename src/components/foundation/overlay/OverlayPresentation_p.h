#ifndef FLUENTQT_OVERLAYPRESENTATION_P_H
#define FLUENTQT_OVERLAYPRESENTATION_P_H

#include <QMenu>
#include <QPointer>
#include <QTransform>

#include "compatibility/WidgetPresentationCompat_p.h"
#include "components/foundation/overlay/OverlayGeometry.h"

namespace fluent::overlay {

// Optional compositor contract: a source root's local coordinates are mapped
// to its presented local coordinates. The nearest root wins; a native window
// starts a new coordinate space. Ordinary Widgets never publish this property.
// zh_CN: 可选合成契约：源根节点局部坐标映射到显示局部坐标；最近根节点优先，
// 原生窗口开启新的坐标空间。普通 Widgets 不设置此属性。
constexpr const char* presentationTransformPropertyName()
{
    return "_fluent_qt_overlay_presentation_transform";
}

inline const QWidget* presentationRoot(const QWidget* source)
{
    for (const QWidget* node = source; node; node = node->parentWidget()) {
        const QVariant value = node->property(presentationTransformPropertyName());
        if (value.canConvert<QTransform>() && value.value<QTransform>().isInvertible())
            return node;
        if (node->isWindow())
            break;
    }
    return nullptr;
}

QPoint presentedPointInTopLevel(const QWidget* source, const QPoint& localPoint);
QRect presentedRectInTopLevel(const QWidget* source);
QPoint presentedPointToGlobal(const QWidget* source, const QPoint& localPoint);
QPoint localPointFromPresentedGlobal(const QWidget* source, const QPoint& globalPoint);
bool hasPresentedTransform(const QWidget* source);

inline QWidget* presentedTopLevel(const QWidget* source)
{
    return compat::widgetPresentationWindow(source);
}

constexpr const char* presentedMenuSourcePropertyName()
{
    return "_fluent_qt_presented_menu_source";
}

constexpr const char* presentedMenuPointPropertyName()
{
    return "_fluent_qt_presented_menu_point";
}

// Map the invocation point before QMenu applies screen containment. The scoped
// source also tells the compositor not to project this native popup twice.
// zh_CN: 在 QMenu 做屏幕边界约束前映射调用点；临时源标记避免合成器再次投影。
inline void popupMenuAt(QMenu* menu, QWidget* source, const QPoint& localPoint,
                        const QPoint& popupOffset = QPoint())
{
    if (!menu || !source)
        return;
    const QPointer<QMenu> guard(menu);
    const QPointer<QWidget> sourceGuard(source);
    if (compat::widgetPresentationBridge(source).viewport)
        compat::prepareNativeWidgetPopup(menu);
    if (!guard || !sourceGuard)
        return;
    if (!hasPresentedTransform(source)) {
        menu->popup(source->mapToGlobal(localPoint) + popupOffset);
        return;
    }
    const QPointer<QWidget> previousSource =
        menu->property(presentedMenuSourcePropertyName()).value<QWidget*>();
    const QVariant previousPoint = menu->property(presentedMenuPointPropertyName());
    const auto destroyed = QObject::connect(source, &QObject::destroyed, menu, [menu] {
        menu->setProperty(presentedMenuSourcePropertyName(), QVariant());
        menu->setProperty(presentedMenuPointPropertyName(), QVariant());
    });
    menu->setProperty(presentedMenuSourcePropertyName(), QVariant::fromValue(source));
    if (!guard || !sourceGuard)
        return;
    menu->setProperty(presentedMenuPointPropertyName(), localPoint);
    if (!guard || !sourceGuard)
        return;
    menu->popup(presentedPointToGlobal(source, localPoint) + popupOffset);
    if (guard) {
        QObject::disconnect(destroyed);
        menu->setProperty(presentedMenuSourcePropertyName(),
                          previousSource ? QVariant::fromValue(previousSource.data()) : QVariant());
        menu->setProperty(presentedMenuPointPropertyName(), previousPoint);
    }
}

} // namespace fluent::overlay

#endif // FLUENTQT_OVERLAYPRESENTATION_P_H
