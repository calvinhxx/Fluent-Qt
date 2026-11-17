#ifndef FLUENTQT_OVERLAYPRESENTATION_P_H
#define FLUENTQT_OVERLAYPRESENTATION_P_H

#include <QMenu>
#include <QPointer>
#include <QTransform>

#include "compatibility/private/WidgetPresentationCompat_p.h"
#include "components/foundation/overlay/OverlayGeometry.h"

namespace fluent::overlay {

// Optional transform published by a host root to map local coords to displayed coords;
// the nearest ancestor root wins, and native windows start a fresh coordinate space.
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
    return ::compatibility::detail::widgetPresentationWindow(source);
}

constexpr const char* presentedMenuSourcePropertyName()
{
    return "_fluent_qt_presented_menu_source";
}

constexpr const char* presentedMenuPointPropertyName()
{
    return "_fluent_qt_presented_menu_point";
}

// Map localPoint before QMenu clamps to screen bounds; the temporary source
// property prevents compositors from projecting this native popup twice.
inline void popupMenuAt(QMenu* menu, QWidget* source, const QPoint& localPoint, const QPoint& popupOffset = QPoint())
{
    if (!menu || !source)
        return;
    const QPointer<QMenu> guard(menu);
    const QPointer<QWidget> sourceGuard(source);
    if (::compatibility::detail::widgetPresentationBridge(source).viewport)
        ::compatibility::detail::prepareNativeWidgetPopup(menu);
    if (!guard || !sourceGuard)
        return;
    if (!hasPresentedTransform(source)) {
        menu->popup(source->mapToGlobal(localPoint) + popupOffset);
        return;
    }
    const QPointer<QWidget> previousSource = menu->property(presentedMenuSourcePropertyName()).value<QWidget*>();
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
