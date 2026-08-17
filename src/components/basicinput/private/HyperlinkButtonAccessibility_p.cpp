#include "HyperlinkButtonAccessibility_p.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QCoreApplication>
#include <QKeySequence>

#include "components/basicinput/HyperlinkButton.h"
#include "compatibility/QtCompat.h"

namespace fluent::basicinput::detail {

#if QT_CONFIG(accessibility)

namespace {

QString stripMnemonic(const QString& source)
{
    QString result;
    result.reserve(source.size());
    for (int index = 0; index < source.size(); ++index) {
        if (source.at(index) != QLatin1Char('&')) {
            result.append(source.at(index));
            continue;
        }
        if (index + 1 < source.size()
            && source.at(index + 1) == QLatin1Char('&')) {
            result.append(QLatin1Char('&'));
            ++index;
        }
    }
    return result;
}

QString encodedUrl(const HyperlinkButton* button)
{
    return button
        ? button->url().toString(QUrl::FullyEncoded)
        : QString{};
}

QString hyperlinkText(const char* source)
{
    return QCoreApplication::translate(
        "HyperlinkButtonAccessibility", source);
}

void sendUrlChanged(HyperlinkButton* button)
{
    if (!button)
        return;
    QAccessibleValueChangeEvent event(button, encodedUrl(button));
    QAccessible::updateAccessibility(&event);
}

void sendTraversedChanged(HyperlinkButton* button)
{
    if (!button)
        return;
    QAccessible::State changed;
    changed.traversed = true;
    QAccessibleStateChangeEvent event(button, changed);
    QAccessible::updateAccessibility(&event);
}

} // namespace

class HyperlinkButtonAccessible final
    : public QAccessibleWidget
#if FLUENT_HAS_ACCESSIBLE_HYPERLINK_INTERFACE
    , public QAccessibleHyperlinkInterface
#endif
{
public:
    explicit HyperlinkButtonAccessible(HyperlinkButton* button)
        : QAccessibleWidget(button, QAccessible::Link)
    {
    }

    QString text(QAccessible::Text type) const override
    {
        const QString authored = QAccessibleWidget::text(type);
        HyperlinkButton* current = button();
        if (!authored.isEmpty() || !current)
            return authored;
        if (type == QAccessible::Name)
            return stripMnemonic(current->text());
        if (type == QAccessible::Value)
            return encodedUrl(current);
        if (type == QAccessible::Accelerator)
            return current->shortcut().toString(
                QKeySequence::NativeText);
        return authored;
    }

    QAccessible::State state() const override
    {
        QAccessible::State result = QAccessibleWidget::state();
        HyperlinkButton* current = button();
        if (!current)
            return result;
        result.linked = true;
        result.traversed = current->m_accessibilityVisited;
        result.focusable = current->isEnabled()
            && current->focusPolicy() != Qt::NoFocus;
        result.focused = current->hasFocus();
        result.pressed = current->isDown();
        return result;
    }

    void* interface_cast(QAccessible::InterfaceType type) override
    {
#if FLUENT_HAS_ACCESSIBLE_HYPERLINK_INTERFACE
        if (type == QAccessible::HyperlinkInterface)
            return static_cast<QAccessibleHyperlinkInterface*>(this);
#endif
        return QAccessibleWidget::interface_cast(type);
    }

    QStringList actionNames() const override
    {
        return button() && button()->isEnabled()
            ? QStringList{
                  QAccessibleActionInterface::pressAction()}
            : QStringList{};
    }

    void doAction(const QString& actionName) override
    {
        HyperlinkButton* current = button();
        if (current && current->isEnabled()
            && actionName
                == QAccessibleActionInterface::pressAction()) {
            current->click();
        }
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::pressAction()) {
            return {QStringLiteral("Space"),
                    QStringLiteral("Enter")};
        }
        return {};
    }

    QString localizedActionName(
        const QString& actionName) const override
    {
        return actionName == QAccessibleActionInterface::pressAction()
            ? hyperlinkText("Open link")
            : QAccessibleWidget::localizedActionName(actionName);
    }

    QString localizedActionDescription(
        const QString& actionName) const override
    {
        return actionName == QAccessibleActionInterface::pressAction()
            ? hyperlinkText("Opens the link target")
            : QAccessibleWidget::localizedActionDescription(
                  actionName);
    }

#if FLUENT_HAS_ACCESSIBLE_HYPERLINK_INTERFACE
    QString anchor() const override
    {
        return button() ? stripMnemonic(button()->text()) : QString{};
    }

    QString anchorTarget() const override
    {
        return encodedUrl(button());
    }

    int startIndex() const override { return 0; }
    int endIndex() const override { return anchor().size(); }
#endif

    bool isValid() const override
    {
        return button() && button()->url().isValid();
    }

private:
    HyperlinkButton* button() const
    {
        return static_cast<HyperlinkButton*>(widget());
    }
};

namespace {

QAccessibleInterface* hyperlinkButtonAccessibilityFactory(
    const QString&, QObject* object)
{
    auto* button = qobject_cast<HyperlinkButton*>(object);
    return button
        ? new HyperlinkButtonAccessible(button)
        : nullptr;
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureHyperlinkButtonAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(
            hyperlinkButtonAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

void notifyHyperlinkButtonAccessibilityUrlChanged(
    HyperlinkButton* button, bool visitedChanged)
{
#if QT_CONFIG(accessibility)
    sendUrlChanged(button);
    if (visitedChanged)
        sendTraversedChanged(button);
#else
    Q_UNUSED(button)
    Q_UNUSED(visitedChanged)
#endif
}

void notifyHyperlinkButtonAccessibilityVisited(
    HyperlinkButton* button)
{
#if QT_CONFIG(accessibility)
    sendTraversedChanged(button);
#else
    Q_UNUSED(button)
#endif
}

} // namespace fluent::basicinput::detail
