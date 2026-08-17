#include "ProgressAccessibility_p.h"

#include <QAccessible>
#include <QCoreApplication>

#include "components/foundation/private/ValueAccessibility_p.h"
#include "components/status_info/ProgressBar.h"
#include "components/status_info/ProgressRing.h"

namespace fluent::status_info::detail {

#if QT_CONFIG(accessibility)

namespace {

using accessibility::detail::ValueAccessibleAdapter;

QString progressText(const char* source)
{
    return QCoreApplication::translate("ProgressAccessibility", source);
}

class ProgressBarAccessible final : public ValueAccessibleAdapter {
public:
    explicit ProgressBarAccessible(ProgressBar* bar)
        : ValueAccessibleAdapter(bar, QAccessible::ProgressBar)
    {
    }

    QVariant currentValue() const override
    {
        return view() && !view()->isIndeterminate()
            ? QVariant(view()->value()) : QVariant();
    }
    void setCurrentValue(const QVariant&) override {}
    QVariant maximumValue() const override
    {
        return view() ? QVariant(view()->maximum()) : QVariant();
    }
    QVariant minimumValue() const override
    {
        return view() ? QVariant(view()->minimum()) : QVariant();
    }
    QVariant minimumStepSize() const override { return QVariant(); }

    QString text(QAccessible::Text type) const override
    {
        const QString inherited = ValueAccessibleAdapter::text(type);
        if (type != QAccessible::Description || !inherited.isEmpty()
            || !view()) {
            return inherited;
        }
        if (view()->showError())
            return progressText("Error");
        if (view()->showPaused())
            return progressText("Paused");
        if (view()->isIndeterminate())
            return progressText("In progress");
        return {};
    }

protected:
    bool accessibleValueReadOnly() const override { return true; }
    bool accessibleValueBusy() const override
    {
        return view() && view()->isEnabled() && view()->isIndeterminate()
            && !view()->showPaused() && !view()->showError();
    }
    bool accessibleValueAnimated() const override
    {
        return view() && view()->isAnimationRunning();
    }
    QString accessibleValueText() const override
    {
        if (!view())
            return {};
        return view()->isIndeterminate()
            ? progressText("In progress") : view()->progressText();
    }

private:
    ProgressBar* view() const
    {
        return static_cast<ProgressBar*>(widget());
    }
};

class ProgressRingAccessible final : public ValueAccessibleAdapter {
public:
    explicit ProgressRingAccessible(ProgressRing* ring)
        : ValueAccessibleAdapter(ring, QAccessible::ProgressBar)
    {
    }

    QVariant currentValue() const override
    {
        return view() && !view()->isIndeterminate()
            ? QVariant(view()->value()) : QVariant();
    }
    void setCurrentValue(const QVariant&) override {}
    QVariant maximumValue() const override
    {
        return view() ? QVariant(view()->maximum()) : QVariant();
    }
    QVariant minimumValue() const override
    {
        return view() ? QVariant(view()->minimum()) : QVariant();
    }
    QVariant minimumStepSize() const override { return QVariant(); }

    QString text(QAccessible::Text type) const override
    {
        const QString inherited = ValueAccessibleAdapter::text(type);
        if (type != QAccessible::Description || !inherited.isEmpty()
            || !view()) {
            return inherited;
        }
        if (view()->status() == ProgressRing::ProgressRingStatus::Error)
            return progressText("Error");
        if (view()->status() == ProgressRing::ProgressRingStatus::Paused)
            return progressText("Paused");
        if (!view()->isActive())
            return progressText("Inactive");
        if (view()->isIndeterminate())
            return progressText("In progress");
        return {};
    }

protected:
    bool accessibleValueReadOnly() const override { return true; }
    bool accessibleValueBusy() const override
    {
        return view() && view()->isEnabled() && view()->isActive()
            && view()->isIndeterminate()
            && view()->status()
                == ProgressRing::ProgressRingStatus::Running;
    }
    bool accessibleValueAnimated() const override
    {
        return view() && view()->isAnimationRunning();
    }
    QString accessibleValueText() const override
    {
        if (!view())
            return {};
        return view()->isIndeterminate()
            ? progressText("In progress")
            : QString::number(view()->value());
    }

private:
    ProgressRing* view() const
    {
        return static_cast<ProgressRing*>(widget());
    }
};

QAccessibleInterface* progressAccessibilityFactory(
    const QString&, QObject* object)
{
    if (auto* bar = dynamic_cast<ProgressBar*>(object))
        return new ProgressBarAccessible(bar);
    if (auto* ring = dynamic_cast<ProgressRing*>(object))
        return new ProgressRingAccessible(ring);
    return nullptr;
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureProgressAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(progressAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

} // namespace fluent::status_info::detail
