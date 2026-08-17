#include "ColorPickerAccessibility_p.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QCoreApplication>
#include <QWidget>

#include <cmath>

#include "components/basicinput/ColorPicker.h"
#include "components/foundation/private/ValueAccessibility_p.h"

namespace fluent::basicinput::detail {

#if QT_CONFIG(accessibility)

namespace {

using accessibility::detail::ValueAccessibleAdapter;

QString colorText(const char* source)
{
    return QCoreApplication::translate("ColorPickerAccessibility", source);
}

ColorPicker* ownerFor(QWidget* widget)
{
    QWidget* current = widget;
    while (current) {
        if (auto* picker = dynamic_cast<ColorPicker*>(current))
            return picker;
        current = current->parentWidget();
    }
    return nullptr;
}

QString colorValueText(const ColorPicker* picker)
{
    if (!picker || !picker->color().isValid())
        return {};
    const QColor color = picker->color();
    QString result = QStringLiteral("#%1%2%3")
        .arg(color.red(), 2, 16, QLatin1Char('0'))
        .arg(color.green(), 2, 16, QLatin1Char('0'))
        .arg(color.blue(), 2, 16, QLatin1Char('0'))
        .toUpper();
    if (picker->alphaEnabled()) {
        result += QStringLiteral("%1")
            .arg(color.alpha(), 2, 16, QLatin1Char('0')).toUpper();
    }
    return result;
}

QString spectrumValueText(const ColorPicker* picker)
{
    if (!picker)
        return {};
    return colorText("Saturation %1%, brightness %2%")
        .arg(qRound(picker->saturation() * 100.0))
        .arg(qRound(picker->value() * 100.0));
}

class ColorPickerAccessible final : public QAccessibleWidget {
public:
    explicit ColorPickerAccessible(ColorPicker* picker)
        : QAccessibleWidget(picker, QAccessible::ColorChooser)
    {
    }

    QString text(QAccessible::Text type) const override
    {
        const QString inherited = QAccessibleWidget::text(type);
        if (!inherited.isEmpty())
            return inherited;
        if (type == QAccessible::Name)
            return colorText("Color picker");
        if (type == QAccessible::Value)
            return colorValueText(view());
        return inherited;
    }

private:
    ColorPicker* view() const
    {
        return static_cast<ColorPicker*>(widget());
    }
};

class HueBarAccessible final : public ValueAccessibleAdapter {
public:
    explicit HueBarAccessible(QWidget* bar)
        : ValueAccessibleAdapter(bar, QAccessible::Slider)
    {
    }

    QString text(QAccessible::Text type) const override
    {
        const QString inherited = ValueAccessibleAdapter::text(type);
        if (type == QAccessible::Name && inherited.isEmpty())
            return colorText("Hue");
        return inherited;
    }

    QVariant currentValue() const override
    {
        return owner() ? QVariant(qRound(owner()->hue() * 359.0))
                       : QVariant();
    }

    void setCurrentValue(const QVariant& value) override
    {
        if (!owner() || !widget()->isEnabled())
            return;
        bool ok = false;
        const int requested = value.toInt(&ok);
        if (ok)
            owner()->setHueFromBar(qBound(0, requested, 359) / 359.0);
    }

    QVariant maximumValue() const override { return 359; }
    QVariant minimumValue() const override { return 0; }
    QVariant minimumStepSize() const override { return 1; }

protected:
    bool accessibleValueReadOnly() const override { return false; }
    bool canIncreaseAccessibleValue() const override
    {
        return owner() && widget()->isEnabled();
    }
    bool canDecreaseAccessibleValue() const override
    {
        return owner() && widget()->isEnabled();
    }
    void changeAccessibleValueByStep(int direction) override
    {
        if (!owner())
            return;
        int hue = currentValue().toInt() + direction;
        if (hue < 0)
            hue = 359;
        if (hue > 359)
            hue = 0;
        owner()->setHueFromBar(hue / 359.0);
    }
    QString accessibleValueText() const override
    {
        return colorText("%1 degrees").arg(currentValue().toInt());
    }

private:
    ColorPicker* owner() const { return ownerFor(widget()); }
};

class SpectrumAccessible final : public QAccessibleWidget {
public:
    explicit SpectrumAccessible(QWidget* spectrum)
        : QAccessibleWidget(spectrum, QAccessible::ColorChooser)
    {
    }

    QString text(QAccessible::Text type) const override
    {
        const QString inherited = QAccessibleWidget::text(type);
        if (!inherited.isEmpty())
            return inherited;
        if (type == QAccessible::Name)
            return colorText("Saturation and brightness");
        if (type == QAccessible::Value)
            return spectrumValueText(owner());
        return inherited;
    }

    QStringList actionNames() const override
    {
        QStringList result = QAccessibleWidget::actionNames();
        if (!owner() || !widget()->isEnabled())
            return result;
        const QString increase = QAccessibleActionInterface::increaseAction();
        const QString decrease = QAccessibleActionInterface::decreaseAction();
        if (owner()->value() < 1.0 && !result.contains(increase))
            result.append(increase);
        if (owner()->value() > 0.0 && !result.contains(decrease))
            result.append(decrease);
        return result;
    }

    void doAction(const QString& actionName) override
    {
        ColorPicker* picker = owner();
        if (picker && widget()->isEnabled()
            && actionName == QAccessibleActionInterface::increaseAction()) {
            picker->setSVFromSpectrum(
                picker->saturation(), qMin(1.0, picker->value() + 0.01));
            return;
        }
        if (picker && widget()->isEnabled()
            && actionName == QAccessibleActionInterface::decreaseAction()) {
            picker->setSVFromSpectrum(
                picker->saturation(), qMax(0.0, picker->value() - 0.01));
            return;
        }
        QAccessibleWidget::doAction(actionName);
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::increaseAction())
            return {QStringLiteral("Up")};
        if (actionName == QAccessibleActionInterface::decreaseAction())
            return {QStringLiteral("Down")};
        return QAccessibleWidget::keyBindingsForAction(actionName);
    }

private:
    ColorPicker* owner() const { return ownerFor(widget()); }
};

class PreviewAccessible final : public QAccessibleWidget {
public:
    explicit PreviewAccessible(QWidget* preview)
        : QAccessibleWidget(preview, QAccessible::Graphic)
    {
    }

    QString text(QAccessible::Text type) const override
    {
        const QString inherited = QAccessibleWidget::text(type);
        if (!inherited.isEmpty())
            return inherited;
        if (type == QAccessible::Name)
            return colorText("Selected color");
        if (type == QAccessible::Value)
            return colorValueText(ownerFor(widget()));
        return inherited;
    }
};

QAccessibleInterface* colorPickerAccessibilityFactory(
    const QString&, QObject* object)
{
    if (auto* picker = dynamic_cast<ColorPicker*>(object))
        return new ColorPickerAccessible(picker);
    auto* widget = qobject_cast<QWidget*>(object);
    if (!widget || !ownerFor(widget))
        return nullptr;
    const QString name = widget->objectName();
    if (name == QStringLiteral("ColorPicker.HueBar"))
        return new HueBarAccessible(widget);
    if (name == QStringLiteral("ColorPicker.Spectrum"))
        return new SpectrumAccessible(widget);
    if (name == QStringLiteral("ColorPicker.PreviewPane"))
        return new PreviewAccessible(widget);
    return nullptr;
}

void notifyWidgetValue(QWidget* widget, const QVariant& value)
{
    if (!widget)
        return;
    QAccessibleValueChangeEvent event(widget, value);
    QAccessible::updateAccessibility(&event);
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureColorPickerAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(colorPickerAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

void notifyColorPickerValueChanged(ColorPicker* picker)
{
#if QT_CONFIG(accessibility)
    if (!picker)
        return;
    notifyWidgetValue(picker, colorValueText(picker));
    notifyWidgetValue(picker->findChild<QWidget*>(
        QStringLiteral("ColorPicker.PreviewPane")), colorValueText(picker));
#else
    Q_UNUSED(picker)
#endif
}

void notifyColorPickerHueChanged(QWidget* hueBar)
{
#if QT_CONFIG(accessibility)
    ColorPicker* picker = ownerFor(hueBar);
    notifyWidgetValue(hueBar,
        picker ? QVariant(qRound(picker->hue() * 359.0)) : QVariant());
#else
    Q_UNUSED(hueBar)
#endif
}

void notifyColorPickerSpectrumChanged(QWidget* spectrum)
{
#if QT_CONFIG(accessibility)
    notifyWidgetValue(spectrum, spectrumValueText(ownerFor(spectrum)));
#else
    Q_UNUSED(spectrum)
#endif
}

void notifyColorPickerStructureChanged(ColorPicker* picker)
{
#if QT_CONFIG(accessibility)
    if (!picker)
        return;
    QAccessibleEvent event(picker, QAccessible::ObjectReorder);
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(picker)
#endif
}

} // namespace fluent::basicinput::detail
