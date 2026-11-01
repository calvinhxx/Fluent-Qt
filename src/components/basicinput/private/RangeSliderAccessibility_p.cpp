#include "RangeSliderAccessibility_p.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QCoreApplication>
#include <QPointer>

#include <algorithm>
#include <limits>

namespace fluent::basicinput::detail {

#if QT_CONFIG(accessibility)

namespace {

using Thumb = RangeSlider::Thumb;

QString rangeSliderText(const char* source)
{
    return QCoreApplication::translate("RangeSlider", source);
}

int thumbIndex(Thumb thumb)
{
    return thumb == Thumb::Lower ? 0 : 1;
}

qint64 minimumGap(const RangeSlider* slider)
{
    return qMin<qint64>(slider->minimumRange(), static_cast<qint64>(slider->maximum()) - slider->minimum());
}

class RangeSliderThumbAccessible final
    : public QAccessibleInterface,
      public QAccessibleValueInterface,
      public QAccessibleActionInterface {
public:
    RangeSliderThumbAccessible(RangeSlider* slider, Thumb thumb) : m_slider(slider), m_thumb(thumb) {}

    bool isValid() const override { return !m_slider.isNull(); }
    QObject* object() const override { return nullptr; }
    QAccessibleInterface* childAt(int, int) const override { return nullptr; }
    QAccessibleInterface* parent() const override
    {
        return m_slider ? QAccessible::queryAccessibleInterface(m_slider.data()) : nullptr;
    }
    QAccessibleInterface* child(int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface*) const override { return -1; }

    QString text(QAccessible::Text type) const override
    {
        if (!m_slider)
            return QString();
        if (type == QAccessible::Name)
            return m_thumb == Thumb::Lower ? rangeSliderText("Lower value") : rangeSliderText("Upper value");
        if (type == QAccessible::Value)
            return QString::number(value());
        return QString();
    }
    void setText(QAccessible::Text, const QString&) override {}

    QRect rect() const override
    {
        if (!m_slider)
            return QRect();
        const QSize hint = m_slider->minimumSizeHint();
        const int side = m_slider->orientation() == Qt::Horizontal ? hint.height() : hint.width();
        QRect local(QPoint(), QSize(side, side));
        local.moveCenter(m_slider->thumbCenter(m_thumb));
        return QRect(m_slider->mapToGlobal(local.topLeft()), local.size());
    }

    QAccessible::Role role() const override { return QAccessible::Slider; }

    QAccessible::State state() const override
    {
        QAccessible::State result;
        if (!m_slider) {
            result.invalid = true;
            result.invisible = true;
            return result;
        }
        const bool enabled = m_slider->isEnabled();
        result.disabled = !enabled;
        result.focusable = enabled;
        result.focused = m_slider->hasFocus() && m_slider->activeThumb() == m_thumb;
        result.editable = enabled;
        result.invisible = !m_slider->isVisible();
        return result;
    }

    void* interface_cast(QAccessible::InterfaceType type) override
    {
        if (type == QAccessible::ValueInterface)
            return static_cast<QAccessibleValueInterface*>(this);
        if (type == QAccessible::ActionInterface)
            return static_cast<QAccessibleActionInterface*>(this);
        return nullptr;
    }

    // The live bounds follow the other thumb, as a multi-thumb slider should.
    // zh_CN: 实时上下界随另一个手柄变化，符合多手柄滑块的语义。
    QVariant currentValue() const override { return m_slider ? QVariant(value()) : QVariant(); }
    QVariant minimumValue() const override
    {
        if (!m_slider)
            return QVariant();
        if (m_thumb == Thumb::Lower)
            return m_slider->minimum();
        return static_cast<int>(m_slider->lowerValue() + minimumGap(m_slider));
    }
    QVariant maximumValue() const override
    {
        if (!m_slider)
            return QVariant();
        if (m_thumb == Thumb::Upper)
            return m_slider->maximum();
        return static_cast<int>(m_slider->upperValue() - minimumGap(m_slider));
    }
    QVariant minimumStepSize() const override { return m_slider ? QVariant(m_slider->singleStep()) : QVariant(); }

    void setCurrentValue(const QVariant& requested) override
    {
        if (!canChange())
            return;
        bool ok = false;
        const qlonglong raw = requested.toLongLong(&ok);
        if (!ok)
            return;
        const qlonglong low = std::numeric_limits<int>::min();
        const qlonglong high = std::numeric_limits<int>::max();
        setValue(static_cast<int>(std::max(low, std::min(raw, high))));
    }

    QStringList actionNames() const override
    {
        QStringList result;
        if (!canChange())
            return result;
        result.append(QAccessibleActionInterface::setFocusAction());
        if (value() < maximumValue().toInt())
            result.append(QAccessibleActionInterface::increaseAction());
        if (value() > minimumValue().toInt())
            result.append(QAccessibleActionInterface::decreaseAction());
        return result;
    }

    void doAction(const QString& actionName) override
    {
        if (!canChange())
            return;
        if (actionName == QAccessibleActionInterface::setFocusAction()) {
            m_slider->setActiveThumb(m_thumb);
            m_slider->setFocus(Qt::OtherFocusReason);
            return;
        }
        const qint64 step = m_slider->singleStep();
        if (actionName == QAccessibleActionInterface::increaseAction())
            setValue(static_cast<int>(qMin<qint64>(std::numeric_limits<int>::max(), value() + step)));
        else if (actionName == QAccessibleActionInterface::decreaseAction())
            setValue(static_cast<int>(qMax<qint64>(std::numeric_limits<int>::min(), value() - step)));
    }

    QStringList keyBindingsForAction(const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::increaseAction())
            return {QStringLiteral("Up")};
        if (actionName == QAccessibleActionInterface::decreaseAction())
            return {QStringLiteral("Down")};
        return {};
    }

    RangeSlider* slider() const { return m_slider.data(); }
    Thumb thumb() const { return m_thumb; }

private:
    bool canChange() const { return m_slider && m_slider->isEnabled(); }
    int value() const { return m_thumb == Thumb::Lower ? m_slider->lowerValue() : m_slider->upperValue(); }
    void setValue(int requested)
    {
        if (m_thumb == Thumb::Lower)
            m_slider->setLowerValue(requested);
        else
            m_slider->setUpperValue(requested);
    }

    QPointer<RangeSlider> m_slider;
    Thumb m_thumb;
};

class RangeSliderAccessible final : public QAccessibleWidget {
public:
    explicit RangeSliderAccessible(RangeSlider* slider) : QAccessibleWidget(slider, QAccessible::Grouping) {}

    ~RangeSliderAccessible() override
    {
        for (QAccessible::Id& id : m_childIds) {
            if (id != 0 && QAccessible::accessibleInterface(id))
                QAccessible::deleteAccessibleInterface(id);
            id = 0;
        }
    }

    int childCount() const override { return 2; }

    QAccessibleInterface* child(int index) const override
    {
        if (index < 0 || index >= 2 || !widget())
            return nullptr;
        QAccessible::Id& id = m_childIds[index];
        if (id != 0) {
            if (QAccessibleInterface* cached = QAccessible::accessibleInterface(id))
                return cached;
        }
        auto* item = new RangeSliderThumbAccessible(view(), index == 0 ? Thumb::Lower : Thumb::Upper);
        id = QAccessible::registerAccessibleInterface(item);
        return item;
    }

    int indexOfChild(const QAccessibleInterface* childInterface) const override
    {
        const auto* item = dynamic_cast<const RangeSliderThumbAccessible*>(childInterface);
        return item && item->slider() == view() ? thumbIndex(item->thumb()) : -1;
    }

    QAccessibleInterface* childAt(int x, int y) const override
    {
        RangeSlider* slider = view();
        if (!slider)
            return nullptr;
        // The active thumb paints on top, so it wins hit testing as well.
        // zh_CN: 活动手柄绘制在上层，命中测试同样优先。
        const int active = thumbIndex(slider->activeThumb());
        for (const int index : {active, 1 - active}) {
            QAccessibleInterface* item = child(index);
            if (item && item->rect().contains(x, y))
                return item;
        }
        return nullptr;
    }

    QAccessibleInterface* focusChild() const override
    {
        RangeSlider* slider = view();
        return slider && slider->hasFocus() ? child(thumbIndex(slider->activeThumb())) : nullptr;
    }

    QString text(QAccessible::Text type) const override
    {
        RangeSlider* slider = view();
        if (type == QAccessible::Value && slider) {
            return rangeSliderText("%1 to %2").arg(slider->lowerValue()).arg(slider->upperValue());
        }
        return QAccessibleWidget::text(type);
    }

private:
    RangeSlider* view() const { return static_cast<RangeSlider*>(widget()); }

    mutable QAccessible::Id m_childIds[2] = {0, 0};
};

QAccessibleInterface* rangeSliderAccessibilityFactory(const QString&, QObject* object)
{
    if (auto* slider = qobject_cast<RangeSlider*>(object))
        return new RangeSliderAccessible(slider);
    return nullptr;
}

QAccessibleInterface* thumbInterface(RangeSlider* slider, Thumb thumb)
{
    QAccessibleInterface* root = slider ? QAccessible::queryAccessibleInterface(slider) : nullptr;
    return root ? root->child(thumbIndex(thumb)) : nullptr;
}

} // namespace

void ensureRangeSliderAccessibilityFactory()
{
    static const bool installed = [] {
        QAccessible::installFactory(rangeSliderAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
}

void notifyRangeSliderValueChanged(RangeSlider* slider, RangeSlider::Thumb thumb)
{
    QAccessibleInterface* item = thumbInterface(slider, thumb);
    if (!item)
        return;
    QAccessibleValueChangeEvent event(item, item->text(QAccessible::Value).toInt());
    QAccessible::updateAccessibility(&event);
}

void notifyRangeSliderFocusChanged(RangeSlider* slider)
{
    if (!slider || !slider->hasFocus())
        return;
    QAccessibleInterface* item = thumbInterface(slider, slider->activeThumb());
    if (!item)
        return;
    QAccessibleEvent event(item, QAccessible::Focus);
    QAccessible::updateAccessibility(&event);
}

#else

void ensureRangeSliderAccessibilityFactory() {}
void notifyRangeSliderValueChanged(RangeSlider*, RangeSlider::Thumb) {}
void notifyRangeSliderFocusChanged(RangeSlider*) {}

#endif // QT_CONFIG(accessibility)

} // namespace fluent::basicinput::detail
