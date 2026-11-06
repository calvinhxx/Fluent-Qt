#include "compatibility/AccessibilityCompat.h"
#include "ComboBoxAccessibility_p.h"
#include "components/basicinput/ComboBox.h"
#include <QAccessible>
#include <QAccessibleWidget>
#include <QApplication>
#include <QLineEdit>

namespace fluent::basicinput::detail {

#if QT_CONFIG(accessibility)

// Qt's native combo interface enumerates only its fallback list and editor.
// Use the actual widget children so hosted actions retain their own interfaces.
// zh_CN: Qt 原生组合框接口只枚举备用列表和编辑器；枚举真实子控件，保留托管操作自身的接口。
class ComboBoxAccessible final : public QAccessibleWidget {
public:
    explicit ComboBoxAccessible(ComboBox* box) : QAccessibleWidget(box, QAccessible::ComboBox) {}

    QString text(QAccessible::Text type) const override
    {
        ComboBox* box = view();
        if (box && type == QAccessible::Value)
            return box->currentText();
        const QString inherited = QAccessibleWidget::text(type);
        return box && type == QAccessible::Name && inherited.isEmpty() ? box->currentText() : inherited;
    }

    QAccessible::State state() const override
    {
        auto result = QAccessibleWidget::state();
        if (ComboBox* box = view()) {
            result.hasPopup = true;
            result.expandable = true;
            result.expanded = box->m_popupVisible;
            result.collapsed = !box->m_popupVisible;
            result.editable = box->isEditable();
        }
        return result;
    }

    QAccessibleInterface* focusChild() const override
    {
        ComboBox* box = view();
        if (!box)
            return nullptr;
        QWidget* focused = QApplication::focusWidget();
        if (focused && box->isAncestorOf(focused))
            return QAccessible::queryAccessibleInterface(focused);
        if (box->hasFocus() && box->isEditable() && box->lineEdit())
            return QAccessible::queryAccessibleInterface(box->lineEdit());
        return nullptr;
    }

    FluentAccessibleRelationList relations(QAccessible::Relation match) const override
    {
        auto result = QAccessibleWidget::relations(match);
        if (view() && (match & QAccessible::Controller)) {
            if (QWidget* popup = view()->accessiblePopupWidget()) {
                if (auto* accessible = QAccessible::queryAccessibleInterface(popup))
                    result.append({accessible, QAccessible::Controller});
            }
        }
        return result;
    }

    QStringList actionNames() const override
    {
        auto result = QAccessibleWidget::actionNames();
        result.append(QAccessibleActionInterface::showMenuAction());
        result.append(QAccessibleActionInterface::pressAction());
        result.removeDuplicates();
        return result;
    }

    void doAction(const QString& action) override
    {
        ComboBox* box = view();
        if (!box || !box->isEnabled())
            return;
        if (action == QAccessibleActionInterface::showMenuAction()
            || action == QAccessibleActionInterface::pressAction()) {
            if (box->m_popupVisible)
                box->hidePopup();
            else
                box->showPopup();
        } else {
            QAccessibleWidget::doAction(action);
        }
    }

private:
    ComboBox* view() const { return static_cast<ComboBox*>(widget()); }
};

namespace {
QAccessibleInterface* comboBoxAccessibilityFactory(const QString&, QObject* object)
{
    auto* box = dynamic_cast<ComboBox*>(object);
    return box ? new ComboBoxAccessible(box) : nullptr;
}
} // namespace

#endif

void ensureComboBoxAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(comboBoxAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

void notifyComboBoxPopupChanged(ComboBox* box)
{
#if QT_CONFIG(accessibility)
    QAccessible::State changed;
    changed.expanded = true;
    changed.collapsed = true;
    QAccessibleStateChangeEvent event(box, changed);
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(box)
#endif
}

void notifyComboBoxContentChanged(ComboBox* box)
{
#if QT_CONFIG(accessibility)
    QAccessibleEvent event(box, QAccessible::ObjectReorder);
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(box)
#endif
}

} // namespace fluent::basicinput::detail
