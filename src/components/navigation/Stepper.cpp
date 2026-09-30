#include "Stepper.h"

#include <cmath>

#include <QAccessible>
#include <QHideEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPointer>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QVariantAnimation>
#include "components/basicinput/Button.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/private/LogicalItemAccessibility_p.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/Label.h"

namespace fluent::navigation {
namespace {
using basicinput::Button;
using textfields::Label;
using scrolling::ScrollView;

class StepScrollView final : public ScrollView {
public:
    explicit StepScrollView(QWidget* parent) : ScrollView(parent) { keepViewportTransparent(); }

protected:
    void onThemeUpdated() override
    {
        ScrollView::onThemeUpdated();
        keepViewportTransparent();
    }

private:
    void keepViewportTransparent()
    {
        viewport()->setAutoFillBackground(false);
        viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);
    }
};

QColor blend(const QColor& from, const QColor& to, qreal t)
{
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * t,
                            from.greenF() + (to.greenF() - from.greenF()) * t,
                            from.blueF() + (to.blueF() - from.blueF()) * t,
                            from.alphaF() + (to.alphaF() - from.alphaF()) * t);
}

class StepButton final : public Button {
public:
    StepButton(QWidget* parent, Stepper* view) : Button(parent), owner(view), motion(this)
    {
        motion.setObjectName(QStringLiteral("stepperMarkerTransition"));
        setFluentStyle(Button::Subtle);
        setCheckable(true);
        setFocusVisual(true);
        title = new Label(this);
        description = new Label(this);
        for (auto* label : {title, description}) {
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
            label->setTextFormat(Qt::PlainText);
            label->setTextElideMode(Qt::ElideRight);
        }
        description->setFluentTypography(Typography::FontRole::Caption);
        connect(&motion, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            const qreal t = value.toReal();
            fill = blend(fromFill, toFill, t);
            foreground = blend(fromForeground, toForeground, t);
            activeWeight = fromActive + (toActive - fromActive) * t;
            completedWeight = fromCompleted + (toCompleted - fromCompleted) * t;
            update();
            if (parentWidget())
                parentWidget()->update();
        });
    }
    QPointer<Stepper> owner;
    StepperItem item;
    int number = 1;
    Qt::Orientation axis = Qt::Horizontal;
    bool animations = true;
    Label* title = nullptr;
    Label* description = nullptr;
    QVariantAnimation motion;
    qreal activeWeight = 0.0, completedWeight = 0.0;
    qreal fromActive = 0.0, toActive = 0.0, fromCompleted = 0.0, toCompleted = 0.0;
    QColor fill, foreground, fromFill, toFill, fromForeground, toForeground;

    void applyGeometry(int stepNumber, Qt::Orientation orientation)
    {
        number = stepNumber;
        axis = orientation;
        layoutLabels();
        update();
    }

    QRectF markerRect() const
    {
        if (axis == Qt::Horizontal)
            return QRectF(width() / 2.0 - 16, 8, 32, 32);
        const bool rtl = layoutDirection() == Qt::RightToLeft;
        return QRectF(rtl ? width() - 44 : 12, height() / 2.0 - 16, 32, 32);
    }
    void refresh(bool animate)
    {
        title->setText(item.text);
        description->setText(item.description);
        description->setVisible(!item.description.isEmpty());
        title->setFluentTypography(isChecked() ? Typography::FontRole::BodyStrong
                                               : Typography::FontRole::Body);
        title->setTextColorRole(isEnabled() ? Label::TextColorRole::Primary
                                            : Label::TextColorRole::Disabled);
        description->setTextColorRole(isEnabled() ? Label::TextColorRole::Secondary
                                                  : Label::TextColorRole::Disabled);
        setAccessibleName(item.accessibleName.isEmpty() ? item.text : item.accessibleName);
        setAccessibleDescription(item.description);
        const auto& c = themeColorsRef();
        QColor targetFill = c.controlAltSecondary;
        QColor targetText = c.textSecondary;
        if (item.state == Stepper::Error) {
            targetFill = c.systemCriticalBg;
            targetText = c.systemCritical;
        } else if (item.state == Stepper::Completed)
            targetText = c.textAccentPrimary;
        if (isChecked() && item.state != Stepper::Error) {
            targetFill = c.accentDefault;
            targetText = c.textOnAccent;
        }
        if (!isEnabled()) {
            targetFill = c.controlDisabled;
            targetText = c.textDisabled;
        }
        motion.stop();
        fromFill = fill.isValid() ? fill : targetFill;
        fromForeground = foreground.isValid() ? foreground : targetText;
        toFill = targetFill;
        toForeground = targetText;
        fromActive = activeWeight;
        toActive = isChecked() ? 1.0 : 0.0;
        fromCompleted = completedWeight;
        toCompleted = item.state == Stepper::Completed ? 1.0 : 0.0;
        motion.setStartValue(0.0);
        motion.setEndValue(1.0);
        motion.setEasingCurve(themeAnimation().decelerate);
        fluent::detail::startMotionTransition(&motion, themeAnimation().normal,
                                              animate && animations && isVisible());
        layoutLabels();
    }
    void onThemeUpdated() override
    {
        Button::onThemeUpdated();
        if (title)
            refresh(false);
    }

protected:
    // Qt keeps pointer, focus and Space semantics; the application owns checking.
    // zh_CN: 保留 Qt 指针、焦点和空格语义，勾选状态由应用控制。
    void nextCheckState() override {}
    void resizeEvent(QResizeEvent* event) override
    {
        Button::resizeEvent(event);
        layoutLabels();
    }
    void paintEvent(QPaintEvent* event) override
    {
        Button::paintEvent(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QRectF marker = markerRect();
        const qreal inset = (1.0 - activeWeight) * 2.0;
        marker.adjust(inset, inset, -inset, -inset);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawEllipse(marker);
        const auto& c = themeColorsRef();
        if (!isEnabled()) {
            p.setPen(QPen(c.strokeStrong, 1.0, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(marker);
        }
        p.setPen(foreground);
        QString glyph = item.iconGlyph;
        if (glyph.isEmpty() && item.state == Stepper::Completed)
            glyph = Typography::Icons::glyph(QStringLiteral("ic_fluent_checkmark_16_regular"));
        if (glyph.isEmpty() && item.state == Stepper::Error)
            glyph = Typography::Icons::glyph(QStringLiteral("ic_fluent_error_circle_16_regular"));
        QFont markerFont = themeFont(Typography::FontRole::BodyStrong).toQFont();
        if (!glyph.isEmpty()) {
            markerFont.setFamily(Typography::FontFamily::FluentIcons);
            markerFont.setPixelSize(16);
        }
        p.setFont(markerFont);
        p.drawText(marker, Qt::AlignCenter, glyph.isEmpty() ? QString::number(number) : glyph);
    }

private:
    void layoutLabels()
    {
        if (!title)
            return;
        if (axis == Qt::Horizontal) {
            title->setAlignment(Qt::AlignCenter);
            description->setAlignment(Qt::AlignCenter);
            title->setGeometry(6, 48, qMax(0, width() - 12), 24);
            description->setGeometry(6, 72, qMax(0, width() - 12), 20);
        } else {
            const bool rtl = layoutDirection() == Qt::RightToLeft;
            const int x = rtl ? 8 : 56;
            const int textWidth = qMax(0, width() - 64);
            const Qt::Alignment alignment =
                (rtl ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignAbsolute;
            title->setAlignment(alignment);
            description->setAlignment(alignment);
            title->setGeometry(x, item.description.isEmpty() ? 20 : 10, textWidth, 24);
            description->setGeometry(x, 34, textWidth, 20);
        }
    }
};

class StepTrack final : public QWidget {
public:
    Stepper* owner = nullptr;
    QVector<StepButton*> buttons;

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!owner || buttons.size() < 2)
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto& c = owner->themeColorsRef();
        for (int i = 0; i + 1 < buttons.size(); ++i) {
            const auto* a = buttons.at(i);
            const auto* b = buttons.at(i + 1);
            QPointF start = a->markerRect().center() + a->pos();
            QPointF end = b->markerRect().center() + b->pos();
            const QPointF delta = end - start;
            const qreal length = std::hypot(delta.x(), delta.y());
            if (length <= 44)
                continue;
            const QPointF offset = delta * (22.0 / length);
            start += offset;
            end -= offset;
            p.setPen(QPen(c.strokeDivider, 2));
            p.drawLine(start, end);
            if (a->completedWeight > 0 && owner->isEnabled() && a->isEnabled()) {
                QColor accent = c.accentDefault;
                accent.setAlphaF(accent.alphaF() * a->completedWeight);
                p.setPen(QPen(accent, 2));
                p.drawLine(start, end);
            }
        }
    }
};
} // namespace

class Stepper::Private {
public:
    explicit Private(Stepper* view) : q(view)
    {
        scroll = new StepScrollView(q);
        scroll->setFocusPolicy(Qt::NoFocus);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        track = new StepTrack;
        track->owner = q;
        scroll->setContentWidget(track);
        track->setAutoFillBackground(false);
        scroll->viewport()->installEventFilter(q);
    }
    Stepper* q;
    ScrollView* scroll;
    StepTrack* track;
    QVector<StepperItem> items;
    int current = -1;
    Qt::Orientation orientation = Qt::Horizontal;
    bool animations = true;
    bool layingOut = false;
    void layout()
    {
        if (layingOut)
            return;
        QScopedValueRollback<bool> guard(layingOut, true);
        scroll->setGeometry(q->rect());
        const int count = items.size();
        const bool horizontal = orientation == Qt::Horizontal;
        const int width = qMax(96, qMax(scroll->viewport()->width(), horizontal ? count * 112 : 0));
        const int height = horizontal ? 100 : qMax(1, count * 64);
        track->setMinimumSize(horizontal ? qMax(96, count * 112) : 96, height);
        track->resize(width, height);
        for (int i = 0; i < count; ++i) {
            auto* button = track->buttons.at(i);
            if (horizontal) {
                const int slot = qMax(1, width / qMax(1, count));
                const int position = q->layoutDirection() == Qt::RightToLeft ? count - i - 1 : i;
                button->setGeometry(position * slot, 0, slot, height);
            } else
                button->setGeometry(0, i * 64, width, 64);
            button->applyGeometry(i + 1, orientation);
        }
        q->updateGeometry();
        track->update();
    }
    void refresh(bool animate)
    {
        for (int i = 0; i < items.size(); ++i) {
            auto* button = track->buttons.at(i);
            button->item = items.at(i);
            button->animations = animations;
            button->setEnabled(items.at(i).enabled);
            button->setChecked(i == current);
            button->refresh(animate);
        }
        StepButton* entry =
            current >= 0 && items.at(current).enabled ? track->buttons.at(current) : nullptr;
        if (!entry) {
            for (int i = 0; i < items.size(); ++i) {
                if (items.at(i).enabled) {
                    entry = track->buttons.at(i);
                    break;
                }
            }
        }
        q->setFocusProxy(entry);
        q->setFocusPolicy(entry ? Qt::StrongFocus : Qt::NoFocus);
        track->update();
    }
};

#if QT_CONFIG(accessibility)
class StepperAccessible final : public accessibility::detail::LogicalItemAccessibleAdapter {
public:
    explicit StepperAccessible(Stepper* view)
        : LogicalItemAccessibleAdapter(view, QAccessible::PageTabList)
    {}
    Stepper* view() const { return qobject_cast<Stepper*>(widget()); }
    QAccessibleInterface* child(int i) const override
    {
        return i >= 0 && i < view()->itemCount()
                   ? QAccessible::queryAccessibleInterface(view()->d->track->buttons.at(i))
                   : nullptr;
    }
    int indexOfChild(const QAccessibleInterface* child) const override
    {
        if (!child)
            return -1;
        for (int i = 0; i < view()->itemCount(); ++i)
            if (child->object() == view()->d->track->buttons.at(i))
                return i;
        return -1;
    }
    int logicalChildCount() const override { return view()->itemCount(); }
    QAccessible::Role logicalChildRole(int) const override { return QAccessible::PageTab; }
    QString logicalChildText(int i, QAccessible::Text type) const override
    {
        const auto item = view()->itemAt(i);
        if (type == QAccessible::Name)
            return item.accessibleName.isEmpty() ? item.text : item.accessibleName;
        if (type != QAccessible::Description)
            return {};
        QString state = item.state == Stepper::Completed ? Stepper::tr("Completed")
                        : item.state == Stepper::Error   ? Stepper::tr("Error")
                                                         : Stepper::tr("Pending");
        if (i == view()->currentIndex())
            state += QStringLiteral(", ") + Stepper::tr("Current step");
        if (!item.enabled)
            state += QStringLiteral(", ") + Stepper::tr("Unavailable");
        return QStringList{item.description, state,
                           Stepper::tr("Step %1 of %2").arg(i + 1).arg(view()->itemCount())}
            .join(QStringLiteral("; "));
    }
    QRect logicalChildRect(int i) const override { return toGlobalRect(view()->itemGeometry(i)); }
    accessibility::detail::LogicalItemAccessibleState logicalChildState(int i) const override
    {
        accessibility::detail::LogicalItemAccessibleState s;
        s.valid = i >= 0 && i < view()->itemCount();
        if (!s.valid)
            return s;
        s.enabled = view()->isEnabled() && view()->itemAt(i).enabled;
        s.focusable = s.enabled;
        s.selected = i == view()->currentIndex();
        s.focused = view()->d->track->buttons.at(i)->hasFocus();
        s.invisible = !view()->isVisible();
        const QRect globalClip(view()->d->scroll->viewport()->mapToGlobal(QPoint()),
                               view()->d->scroll->viewport()->size());
        s.offscreen = !logicalChildRect(i).intersects(globalClip);
        return s;
    }
    int logicalFocusChild() const override
    {
        for (int i = 0; i < view()->itemCount(); ++i)
            if (view()->d->track->buttons.at(i)->hasFocus())
                return i;
        return -1;
    }
    QStringList logicalChildActions(int i) const override
    {
        const auto state = logicalChildState(i);
        return state.valid && state.enabled
                   ? QStringList{QAccessibleActionInterface::pressAction(),
                                 QAccessibleActionInterface::setFocusAction()}
                   : QStringList();
    }
    void performLogicalChildAction(int i, const QString& action) override
    {
        if (!logicalChildState(i).valid || !logicalChildState(i).enabled)
            return;
        auto* button = view()->d->track->buttons.at(i);
        if (action == QAccessibleActionInterface::pressAction())
            button->click();
        else if (action == QAccessibleActionInterface::setFocusAction())
            button->setFocus();
    }
    bool setLogicalChildSelected(int i, bool selected) override
    {
        QPointer<Stepper> guarded(view());
        if (!selected || !logicalChildState(i).valid || !logicalChildState(i).enabled)
            return false;
        performLogicalChildAction(i, QAccessibleActionInterface::pressAction());
        return guarded && guarded->currentIndex() == i;
    }
};

// The actual focus widget and the logical tree expose one accessible step.
// zh_CN: 实际焦点控件与逻辑树共同暴露同一个步骤无障碍对象。
class StepButtonAccessible final : public QAccessibleWidget {
public:
    explicit StepButtonAccessible(StepButton* button)
        : QAccessibleWidget(button, QAccessible::PageTab)
    {}
    QAccessibleInterface* parent() const override { return root(); }
    QAccessibleInterface* child(int) const override { return nullptr; }
    QAccessibleInterface* childAt(int, int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface*) const override { return -1; }
    QString text(QAccessible::Text type) const override
    {
        auto* adapter = root();
        return adapter ? adapter->logicalChildText(adapter->indexOfChild(this), type) : QString();
    }
    QAccessible::State state() const override
    {
        QAccessible::State result;
        auto* adapter = root();
        if (!adapter) {
            result.invalid = result.invisible = true;
            return result;
        }
        const auto item = adapter->logicalChildState(adapter->indexOfChild(this));
        if (!item.valid) {
            result.invalid = result.invisible = true;
            return result;
        }
        result.invalid = !item.valid;
        result.disabled = !item.enabled;
        result.focusable = item.focusable && item.enabled;
        result.selectable = item.selectable && item.enabled;
        result.selected = item.selected;
        result.focused = item.focused;
        result.invisible = item.invisible;
        result.offscreen = item.offscreen;
        return result;
    }
    QStringList actionNames() const override
    {
        auto* adapter = root();
        return adapter ? adapter->logicalChildActions(adapter->indexOfChild(this)) : QStringList();
    }
    void doAction(const QString& action) override
    {
        auto* adapter = root();
        if (adapter)
            adapter->performLogicalChildAction(adapter->indexOfChild(this), action);
    }
    QStringList keyBindingsForAction(const QString& action) const override
    {
        auto* adapter = root();
        return adapter ? adapter->logicalChildKeyBindings(adapter->indexOfChild(this), action)
                       : QStringList();
    }

private:
    StepperAccessible* root() const
    {
        auto* button = static_cast<StepButton*>(widget());
        return button && button->owner ? dynamic_cast<StepperAccessible*>(
                                             QAccessible::queryAccessibleInterface(button->owner))
                                       : nullptr;
    }
};
#endif

StepperItem::StepperItem(const QString& value, const QString& detail)
    : text(value), description(detail)
{}
bool StepperItem::operator==(const StepperItem& o) const
{
    return text == o.text && description == o.description && iconGlyph == o.iconGlyph &&
           state == o.state && enabled == o.enabled && data == o.data &&
           accessibleName == o.accessibleName;
}

Stepper::Stepper(QWidget* parent) : QWidget(parent), d(new Private(this))
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory([](const QString&, QObject* object) -> QAccessibleInterface* {
            if (auto* view = qobject_cast<Stepper*>(object))
                return new StepperAccessible(view);
            if (auto* button = dynamic_cast<StepButton*>(object))
                return new StepButtonAccessible(button);
            return nullptr;
        });
        return true;
    }();
    Q_UNUSED(installed)
#endif
    connect(&MotionPolicy::instance(), &MotionPolicy::modeChanged, this,
            [this] { d->refresh(false); });
}
Stepper::~Stepper()
{
    d->scroll->viewport()->removeEventFilter(this);
    for (auto* button : d->track->buttons)
        button->removeEventFilter(this);
}
int Stepper::itemCount() const
{
    return d->items.size();
}
QVector<StepperItem> Stepper::items() const
{
    return d->items;
}
StepperItem Stepper::itemAt(int i) const
{
    return i >= 0 && i < itemCount() ? d->items.at(i) : StepperItem();
}
int Stepper::addItem(const QString& text)
{
    return addItem(StepperItem(text));
}
int Stepper::addItem(const StepperItem& item)
{
    const int i = itemCount();
    return insertItem(i, item) ? i : -1;
}
bool Stepper::insertItem(int i, const StepperItem& item)
{
    if (i < 0 || i > itemCount() || item.state < Pending || item.state > Error)
        return false;
    const int previous = d->current;
    auto* button = new StepButton(d->track, this);
    button->setObjectName(QStringLiteral("stepperStep"));
    button->installEventFilter(this);
    connect(button, &Button::clicked, this, [this, button] {
        const int index = d->track->buttons.indexOf(button);
        if (isEnabled() && index >= 0 && d->items.at(index).enabled)
            emit stepRequested(index);
    });
    d->items.insert(i, item);
    d->track->buttons.insert(i, button);
    if (d->current >= i)
        ++d->current;
    button->show();
    d->refresh(false);
    d->layout();
    accessibility::detail::notifyLogicalItemAccessibilityStructure(this);
    const int count = itemCount(), current = d->current;
    QPointer<Stepper> guarded(this);
    emit itemCountChanged(count);
    if (guarded)
        emit guarded->itemsChanged();
    if (guarded && previous != current)
        emit guarded->currentIndexChanged(current);
    return true;
}
bool Stepper::removeItem(int i)
{
    if (i < 0 || i >= itemCount())
        return false;
    const int previous = d->current;
    delete d->track->buttons.takeAt(i);
    d->items.removeAt(i);
    if (d->current == i)
        d->current = -1;
    else if (d->current > i)
        --d->current;
    d->refresh(false);
    d->layout();
    accessibility::detail::notifyLogicalItemAccessibilityStructure(this);
    const int count = itemCount(), current = d->current;
    QPointer<Stepper> guarded(this);
    emit itemCountChanged(count);
    if (guarded)
        emit guarded->itemsChanged();
    if (guarded && previous != current)
        emit guarded->currentIndexChanged(current);
    return true;
}
void Stepper::clearItems()
{
    if (d->items.isEmpty())
        return;
    qDeleteAll(d->track->buttons);
    d->track->buttons.clear();
    d->items.clear();
    const bool selected = d->current != -1;
    d->current = -1;
    d->refresh(false);
    d->layout();
    accessibility::detail::notifyLogicalItemAccessibilityStructure(this);
    QPointer<Stepper> guarded(this);
    emit itemCountChanged(0);
    if (guarded)
        emit guarded->itemsChanged();
    if (guarded && selected)
        emit guarded->currentIndexChanged(-1);
}
bool Stepper::setItem(int i, const StepperItem& item)
{
    if (i < 0 || i >= itemCount() || item.state < Pending || item.state > Error ||
        d->items.at(i) == item)
        return false;
    d->items[i] = item;
    d->refresh(true);
    accessibility::detail::notifyLogicalItemAccessibilityName(this, i);
    accessibility::detail::notifyLogicalItemAccessibilityState(this, i);
    emit itemsChanged();
    return true;
}
bool Stepper::setItemState(int i, State state)
{
    auto item = itemAt(i);
    item.state = state;
    return setItem(i, item);
}
bool Stepper::setItemEnabled(int i, bool enabled)
{
    auto item = itemAt(i);
    item.enabled = enabled;
    return setItem(i, item);
}
int Stepper::currentIndex() const
{
    return d->current;
}
void Stepper::setCurrentIndex(int i)
{
    if (i < -1 || i >= itemCount() || i == d->current)
        return;
    d->current = i;
    d->refresh(true);
    if (i >= 0)
        d->scroll->ensureWidgetVisible(d->track->buttons.at(i), 8, 8);
    accessibility::detail::notifyLogicalItemAccessibilitySelection(this, i);
    emit currentIndexChanged(i);
}
Qt::Orientation Stepper::orientation() const
{
    return d->orientation;
}
void Stepper::setOrientation(Qt::Orientation axis)
{
    if ((axis != Qt::Horizontal && axis != Qt::Vertical) || d->orientation == axis)
        return;
    d->orientation = axis;
    d->layout();
    emit orientationChanged(axis);
}
bool Stepper::isAnimationEnabled() const
{
    return d->animations;
}
void Stepper::setAnimationEnabled(bool enabled)
{
    if (d->animations == enabled)
        return;
    d->animations = enabled;
    d->refresh(false);
    emit animationEnabledChanged(enabled);
}
QRect Stepper::itemGeometry(int i) const
{
    if (i < 0 || i >= itemCount())
        return {};
    const auto* button = d->track->buttons.at(i);
    return QRect(button->mapTo(this, QPoint()), button->size());
}
QSize Stepper::sizeHint() const
{
    return orientation() == Qt::Horizontal ? QSize(qMax(112, itemCount() * 112), 104)
                                           : QSize(320, qMin(360, qMax(64, itemCount() * 64)));
}
QSize Stepper::minimumSizeHint() const
{
    return QSize(96, orientation() == Qt::Horizontal ? 104 : 64);
}
bool Stepper::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == d->scroll->viewport() && event->type() == QEvent::Resize)
        d->layout();
    auto* button = qobject_cast<Button*>(watched);
    const int current = button ? d->track->buttons.indexOf(static_cast<StepButton*>(button)) : -1;
    if (current >= 0 && event->type() == QEvent::FocusIn)
        accessibility::detail::notifyLogicalItemAccessibilityFocus(this, current);
    if (current < 0 || event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(watched, event);
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
        if (!key->isAutoRepeat())
            button->click();
        return true;
    }
    int direction = 0;
    if (orientation() == Qt::Horizontal &&
        (key->key() == Qt::Key_Left || key->key() == Qt::Key_Right))
        direction = (key->key() == Qt::Key_Right ? 1 : -1) *
                    (layoutDirection() == Qt::RightToLeft ? -1 : 1);
    if (orientation() == Qt::Vertical && (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down))
        direction = key->key() == Qt::Key_Down ? 1 : -1;
    int target = current + direction;
    if (key->key() == Qt::Key_Home) {
        target = 0;
        direction = 1;
    }
    if (key->key() == Qt::Key_End) {
        target = itemCount() - 1;
        direction = -1;
    }
    if (!direction)
        return QWidget::eventFilter(watched, event);
    while (target >= 0 && target < itemCount()) {
        if (d->items.at(target).enabled) {
            auto* next = d->track->buttons.at(target);
            next->setFocus(Qt::TabFocusReason);
            d->scroll->ensureWidgetVisible(next, 8, 8);
            break;
        }
        target += direction;
    }
    return true;
}
void Stepper::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    d->layout();
}
void Stepper::hideEvent(QHideEvent* e)
{
    d->refresh(false);
    QWidget::hideEvent(e);
}
void Stepper::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    if (!d)
        return;
    if (e->type() == QEvent::LayoutDirectionChange)
        d->layout();
    if (e->type() == QEvent::EnabledChange)
        d->refresh(false);
}
void Stepper::onThemeUpdated()
{
    if (d) {
        d->refresh(false);
        d->track->update();
    }
}
} // namespace fluent::navigation
