#include "ImageViewerAccessibility_p.h"

#include <algorithm>
#include <cmath>
#include <QAccessible>
#include <QAccessibleWidget>
#include <QCoreApplication>
#include <QVariant>

#include "components/scrolling/ImageViewer.h"
#include "ImageViewerChrome_p.h"

namespace fluent::scrolling::detail {

#if QT_CONFIG(accessibility)

namespace {

QString viewerText(const char* source)
{
    return QCoreApplication::translate("ImageViewer", source);
}

bool hasImage(const ImageViewer* viewer)
{
    return viewer && !viewer->imageSize().isEmpty();
}

qreal effectiveMinimum(const ImageViewer* viewer)
{
    return std::min(viewer->minZoomFactor(), viewer->fitZoomFactor());
}

qreal effectiveMaximum(const ImageViewer* viewer)
{
    return std::max(viewer->maxZoomFactor(), viewer->fitZoomFactor());
}

bool canZoomIn(const ImageViewer* viewer)
{
    return hasImage(viewer) && viewer->isEnabled() && viewer->zoomFactor() < effectiveMaximum(viewer) - 1e-9;
}

bool canZoomOut(const ImageViewer* viewer)
{
    return hasImage(viewer) && viewer->isEnabled() && viewer->zoomFactor() > effectiveMinimum(viewer) + 1e-9;
}

QVariant percentValue(const ImageViewer* viewer)
{
    return hasImage(viewer) ? QVariant(viewer->zoomFactor() * 100.0) : QVariant();
}

QWidget* zoomBar(const ImageViewer* viewer)
{
    return viewer
        ? viewer->findChild<QWidget*>(QString::fromLatin1(kImageViewerZoomBarName), Qt::FindDirectChildrenOnly)
        : nullptr;
}

void sendEvent(QObject* object, QAccessible::Event type)
{
    if (!object)
        return;
    QAccessibleEvent event(object, type);
    QAccessible::updateAccessibility(&event);
}

// A zoomable image: the zoom percentage is its value, zoom in/out its
// increase/decrease actions, and the toolbar its only logical child.
// zh_CN: 可缩放图片：缩放百分比是数值，放大/缩小是增/减动作，工具条是唯一的逻辑子项。
class ImageViewerAccessible final : public QAccessibleWidget, public QAccessibleValueInterface {
public:
    explicit ImageViewerAccessible(ImageViewer* viewer) : QAccessibleWidget(viewer, QAccessible::Graphic) {}

    int childCount() const override { return zoomBar(viewer()) ? 1 : 0; }

    QAccessibleInterface* child(int index) const override
    {
        QWidget* bar = index == 0 ? zoomBar(viewer()) : nullptr;
        return bar ? QAccessible::queryAccessibleInterface(bar) : nullptr;
    }

    int indexOfChild(const QAccessibleInterface* childInterface) const override
    {
        QWidget* bar = zoomBar(viewer());
        return bar && childInterface && childInterface->object() == bar ? 0 : -1;
    }

    QAccessibleInterface* childAt(int x, int y) const override
    {
        QAccessibleInterface* bar = child(0);
        if (bar && !bar->state().invisible && bar->rect().contains(x, y))
            return bar;
        return nullptr;
    }

    QString text(QAccessible::Text type) const override
    {
        const QString authored = QAccessibleWidget::text(type);
        if (!authored.isEmpty() || type != QAccessible::Value || !hasImage(viewer()))
            return authored;
        return viewerText("%1%").arg(qRound(viewer()->zoomFactor() * 100.0));
    }

    void* interface_cast(QAccessible::InterfaceType type) override
    {
        if (type == QAccessible::ValueInterface)
            return static_cast<QAccessibleValueInterface*>(this);
        return QAccessibleWidget::interface_cast(type);
    }

    QStringList actionNames() const override
    {
        QStringList result = QAccessibleWidget::actionNames();
        if (canZoomIn(viewer()))
            result.append(QAccessibleActionInterface::increaseAction());
        if (canZoomOut(viewer()))
            result.append(QAccessibleActionInterface::decreaseAction());
        return result;
    }

    void doAction(const QString& actionName) override
    {
        ImageViewer* owner = viewer();
        if (actionName == QAccessibleActionInterface::increaseAction()) {
            if (canZoomIn(owner))
                owner->zoomIn(true);
            return;
        }
        if (actionName == QAccessibleActionInterface::decreaseAction()) {
            if (canZoomOut(owner))
                owner->zoomOut(true);
            return;
        }
        QAccessibleWidget::doAction(actionName);
    }

    QStringList keyBindingsForAction(const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::increaseAction())
            return {QStringLiteral("+")};
        if (actionName == QAccessibleActionInterface::decreaseAction())
            return {QStringLiteral("-")};
        return QAccessibleWidget::keyBindingsForAction(actionName);
    }

    QString localizedActionName(const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::increaseAction())
            return viewerText("Zoom in");
        if (actionName == QAccessibleActionInterface::decreaseAction())
            return viewerText("Zoom out");
        return QAccessibleWidget::localizedActionName(actionName);
    }

    QString localizedActionDescription(const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::increaseAction())
            return viewerText("Zooms in to the next step");
        if (actionName == QAccessibleActionInterface::decreaseAction())
            return viewerText("Zooms out to the previous step");
        return QAccessibleWidget::localizedActionDescription(actionName);
    }

    QVariant currentValue() const override { return percentValue(viewer()); }

    void setCurrentValue(const QVariant& value) override
    {
        ImageViewer* owner = viewer();
        bool ok = false;
        const qreal percent = value.toDouble(&ok);
        if (ok && std::isfinite(percent) && hasImage(owner) && owner->isEnabled())
            owner->zoomTo(percent / 100.0);
    }

    QVariant minimumValue() const override
    {
        return hasImage(viewer()) ? QVariant(effectiveMinimum(viewer()) * 100.0) : QVariant();
    }

    QVariant maximumValue() const override
    {
        return hasImage(viewer()) ? QVariant(effectiveMaximum(viewer()) * 100.0) : QVariant();
    }

    // Steps follow presets, so there is no uniform step. zh_CN: 档位不等距，因此没有统一步长。
    QVariant minimumStepSize() const override { return QVariant(); }

private:
    ImageViewer* viewer() const { return static_cast<ImageViewer*>(widget()); }
};

QAccessibleInterface* imageViewerAccessibilityFactory(const QString&, QObject* object)
{
    if (auto* viewer = dynamic_cast<ImageViewer*>(object))
        return new ImageViewerAccessible(viewer);
    auto* widget = qobject_cast<QWidget*>(object);
    if (widget && widget->objectName() == QLatin1String(kImageViewerZoomBarName)
        && dynamic_cast<ImageViewer*>(widget->parentWidget())) {
        return new QAccessibleWidget(widget, QAccessible::ToolBar);
    }
    return nullptr;
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureImageViewerAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(imageViewerAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

void notifyImageViewerAccessibilityZoomChanged(ImageViewer* viewer, bool actionsChanged)
{
#if QT_CONFIG(accessibility)
    if (!viewer)
        return;
    QAccessibleValueChangeEvent event(viewer, percentValue(viewer));
    QAccessible::updateAccessibility(&event);
    if (actionsChanged && viewer->isEnabled())
        sendEvent(viewer, QAccessible::ActionChanged);
#else
    Q_UNUSED(viewer)
    Q_UNUSED(actionsChanged)
#endif
}

void notifyImageViewerAccessibilityImageChanged(ImageViewer* viewer, bool actionsChanged)
{
#if QT_CONFIG(accessibility)
    if (!viewer)
        return;
    QAccessibleValueChangeEvent event(viewer, percentValue(viewer));
    QAccessible::updateAccessibility(&event);
    if (actionsChanged && viewer->isEnabled())
        sendEvent(viewer, QAccessible::ActionChanged);
#else
    Q_UNUSED(viewer)
    Q_UNUSED(actionsChanged)
#endif
}

void notifyImageViewerAccessibilityActionsChanged(ImageViewer* viewer)
{
#if QT_CONFIG(accessibility)
    if (viewer && hasImage(viewer))
        sendEvent(viewer, QAccessible::ActionChanged);
#else
    Q_UNUSED(viewer)
#endif
}

} // namespace fluent::scrolling::detail
