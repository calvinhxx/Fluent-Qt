#include "BasicValueAccessibility_p.h"

#include <QAccessible>
#include <QCoreApplication>
#include <QtGlobal>

#include <cmath>

#include "components/basicinput/RatingControl.h"
#include "components/basicinput/ToggleSwitch.h"
#include "components/foundation/private/ValueAccessibility_p.h"

namespace fluent::basicinput::detail {

#if QT_CONFIG(accessibility)

namespace {

using accessibility::detail::ValueAccessibleAdapter;

QString ratingText(const char* source)
{
    return QCoreApplication::translate(
        "BasicValueAccessibility", source);
}

class ToggleSwitchAccessible final : public QAccessibleWidget {
public:
    explicit ToggleSwitchAccessible(ToggleSwitch* toggle)
        : QAccessibleWidget(toggle, QAccessible::CheckBox)
    {
    }

    QAccessible::State state() const override
    {
        QAccessible::State result = QAccessibleWidget::state();
        result.checkable = true;
        result.checked = view() && view()->isOn();
        return result;
    }

    QStringList actionNames() const override
    {
        QStringList result = QAccessibleWidget::actionNames();
        if (view() && view()->isEnabled()) {
            const QString action =
                QAccessibleActionInterface::toggleAction();
            if (!result.contains(action))
                result.append(action);
        }
        return result;
    }

    void doAction(const QString& actionName) override
    {
        if (view() && view()->isEnabled()
            && actionName == QAccessibleActionInterface::toggleAction()) {
            view()->setIsOn(!view()->isOn());
            return;
        }
        QAccessibleWidget::doAction(actionName);
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::toggleAction())
            return {QStringLiteral("Space")};
        return QAccessibleWidget::keyBindingsForAction(actionName);
    }

private:
    ToggleSwitch* view() const
    {
        return static_cast<ToggleSwitch*>(widget());
    }
};

class RatingControlAccessible final : public ValueAccessibleAdapter {
public:
    explicit RatingControlAccessible(RatingControl* rating)
        : ValueAccessibleAdapter(rating, QAccessible::Slider)
    {
    }

    QVariant currentValue() const override
    {
        return view() ? QVariant(committedValue()) : QVariant();
    }

    void setCurrentValue(const QVariant& value) override
    {
        RatingControl* rating = view();
        if (!rating || !rating->isEnabled() || rating->isReadOnly())
            return;
        bool ok = false;
        double requested = value.toDouble(&ok);
        if (!ok || !std::isfinite(requested))
            return;
        requested = qBound(0.0, requested,
                           static_cast<double>(rating->maxRating()));
        if (requested <= 0.0) {
            rating->setValue(rating->isClearEnabled() ? -1.0 : 0.5);
        } else {
            rating->setValue(requested);
        }
    }

    QVariant maximumValue() const override
    {
        return view() ? QVariant(view()->maxRating()) : QVariant();
    }

    QVariant minimumValue() const override { return 0.0; }
    QVariant minimumStepSize() const override { return 0.5; }

    QString text(QAccessible::Text type) const override
    {
        const QString inherited = ValueAccessibleAdapter::text(type);
        if (type == QAccessible::Description && inherited.isEmpty()
            && view()) {
            return view()->caption();
        }
        return inherited;
    }

protected:
    bool accessibleValueReadOnly() const override
    {
        return !view() || view()->isReadOnly();
    }

    bool canIncreaseAccessibleValue() const override
    {
        return canChange()
            && committedValue() < static_cast<double>(view()->maxRating());
    }

    bool canDecreaseAccessibleValue() const override
    {
        if (!canChange() || committedValue() <= 0.0)
            return false;
        return committedValue() > 0.5 || view()->isClearEnabled();
    }

    void changeAccessibleValueByStep(int direction) override
    {
        RatingControl* rating = view();
        if (!rating || !canChange())
            return;
        const double current = committedValue();
        if (direction > 0) {
            rating->setValue(qMin(
                static_cast<double>(rating->maxRating()),
                current <= 0.0 ? 0.5 : current + 0.5));
            return;
        }
        if (current <= 0.5) {
            if (rating->isClearEnabled())
                rating->setValue(-1.0);
            return;
        }
        rating->setValue(current - 0.5);
    }

    QString accessibleValueText() const override
    {
        RatingControl* rating = view();
        if (!rating || rating->value() < 0.0)
            return ratingText("No rating");
        return ratingText("%1 of %2")
            .arg(QString::number(rating->value(), 'g', 12))
            .arg(rating->maxRating());
    }

private:
    RatingControl* view() const
    {
        return static_cast<RatingControl*>(widget());
    }

    double committedValue() const
    {
        return view() ? qMax(0.0, view()->value()) : 0.0;
    }

    bool canChange() const
    {
        return view() && view()->isEnabled() && !view()->isReadOnly();
    }
};

QAccessibleInterface* basicValueAccessibilityFactory(
    const QString&, QObject* object)
{
    if (auto* toggle = dynamic_cast<ToggleSwitch*>(object))
        return new ToggleSwitchAccessible(toggle);
    if (auto* rating = dynamic_cast<RatingControl*>(object))
        return new RatingControlAccessible(rating);
    return nullptr;
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureBasicValueAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(basicValueAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

} // namespace fluent::basicinput::detail
