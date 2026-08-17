#include "ToolTipAccessibility_p.h"

#include <QAccessible>
#include <QAccessibleWidget>

#include "compatibility/QtCompat.h"
#include "components/status_info/ToolTip.h"

namespace fluent::status_info::detail {

#if QT_CONFIG(accessibility)

class ToolTipAccessible final : public QAccessibleWidget {
public:
    explicit ToolTipAccessible(ToolTip* toolTip)
        : QAccessibleWidget(toolTip, QAccessible::ToolTip)
    {
    }

    QString text(QAccessible::Text type) const override
    {
        const QString inherited = QAccessibleWidget::text(type);
        ToolTip* surface = toolTip();
        if (!surface || !inherited.isEmpty())
            return inherited;
        if (type == QAccessible::Name)
            return surface->text();
        return inherited;
    }

    QAccessible::State state() const override
    {
        QAccessible::State result = QAccessibleWidget::state();
        ToolTip* surface = toolTip();
        if (!surface)
            return result;

        const bool visible = surface->m_accessibilityVisible;
        result.active = visible;
        result.invisible = !visible;
        result.offscreen = !visible || result.offscreen;
        result.focusable = false;
        result.focused = false;
        return result;
    }

    FluentAccessibleRelationList
    relations(QAccessible::Relation match) const override
    {
        auto result = QAccessibleWidget::relations(match);
#if FLUENT_HAS_ACCESSIBLE_DESCRIPTION_RELATION
        ToolTip* surface = toolTip();
        if (!surface || !surface->m_target
            || !(match & QAccessible::DescriptionFor)) {
            return result;
        }

        QAccessibleInterface* target =
            QAccessible::queryAccessibleInterface(surface->m_target);
        if (!target)
            return result;
        for (const auto& relation : result) {
            if (relation.first == target
                && relation.second.testFlag(
                    QAccessible::DescriptionFor)) {
                return result;
            }
        }
        result.append({target, QAccessible::DescriptionFor});
#endif
        return result;
    }

    QStringList actionNames() const override { return {}; }

private:
    ToolTip* toolTip() const
    {
        return static_cast<ToolTip*>(widget());
    }
};

namespace {

QAccessibleInterface* toolTipAccessibilityFactory(
    const QString&, QObject* object)
{
    auto* toolTip = qobject_cast<ToolTip*>(object);
    return toolTip ? new ToolTipAccessible(toolTip) : nullptr;
}

void sendEvent(QObject* object, QAccessible::Event type)
{
    if (!object)
        return;
    QAccessibleEvent event(object, type);
    QAccessible::updateAccessibility(&event);
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureToolTipAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(toolTipAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

void notifyToolTipAccessibilityTextChanged(ToolTip* toolTip)
{
#if QT_CONFIG(accessibility)
    if (toolTip && toolTip->accessibleName().isEmpty())
        sendEvent(toolTip, QAccessible::NameChanged);
#else
    Q_UNUSED(toolTip)
#endif
}

void notifyToolTipAccessibilityVisibilityChanged(ToolTip* toolTip)
{
#if QT_CONFIG(accessibility)
    if (!toolTip)
        return;
    QAccessible::State changed;
    changed.active = true;
    changed.invisible = true;
    QAccessibleStateChangeEvent event(toolTip, changed);
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(toolTip)
#endif
}

void notifyToolTipAccessibilityTargetChanged(ToolTip* toolTip)
{
#if QT_CONFIG(accessibility)
    sendEvent(toolTip, QAccessible::ObjectReorder);
#else
    Q_UNUSED(toolTip)
#endif
}

} // namespace fluent::status_info::detail
