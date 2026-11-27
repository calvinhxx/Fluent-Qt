#pragma once

#include "DesignerForms.h"

#include <QtDesigner/QDesignerContainerExtension>
#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QExtensionFactory>
#include <QtDesigner/QExtensionManager>

#include <FluentQt/Layout.h>
#include <FluentQt/Scrolling.h>

namespace fluentqt_designer {

class PageExtension final : public QObject, public QDesignerContainerExtension {
    Q_OBJECT
    Q_INTERFACES(QDesignerContainerExtension)
public:
    PageExtension(QWidget* host, QObject* parent) : QObject(parent), m_host(host) {}
    QWidget* widget(int index) const override
    {
        if (index != 0)
            return nullptr;
        if (auto* field = qobject_cast<fluent::layout::Field*>(m_host))
            return field->editor();
        if (auto* expander = qobject_cast<fluent::layout::Expander*>(m_host))
            return expander->contentWidget();
        return static_cast<fluent::scrolling::ScrollView*>(m_host)->contentWidget();
    }
    int count() const override { return widget(0) ? 1 : 0; }
    int currentIndex() const override { return count() - 1; }
    bool canAddWidget() const override { return count() == 0; }
    bool canRemove(int index) const override { return widget(index) != nullptr; }
    void setCurrentIndex(int) override {}
    void addWidget(QWidget* page) override { insertWidget(0, page); }
    void insertWidget(int index, QWidget* page) override
    {
        if (index != 0 || !canAddWidget())
            return;
        if (auto* field = qobject_cast<fluent::layout::Field*>(m_host))
            field->setEditor(page, fluent::WidgetOwnership::Owned);
        else if (auto* expander = qobject_cast<fluent::layout::Expander*>(m_host))
            expander->setContentWidget(page, fluent::WidgetOwnership::Owned);
        else
            static_cast<fluent::scrolling::ScrollView*>(m_host)->setContentWidget(page, fluent::WidgetOwnership::Owned);
    }
    void remove(int index) override
    {
        if (!canRemove(index))
            return;
        if (auto* field = qobject_cast<fluent::layout::Field*>(m_host))
            field->takeEditor();
        else if (auto* expander = qobject_cast<fluent::layout::Expander*>(m_host))
            expander->takeContentWidget();
        else
            static_cast<fluent::scrolling::ScrollView*>(m_host)->takeContentWidget();
    }

private:
    QWidget* m_host;
};

class AccordionExtension final : public QObject, public QDesignerContainerExtension {
    Q_OBJECT
    Q_INTERFACES(QDesignerContainerExtension)
public:
    AccordionExtension(fluent::layout::Accordion* host, QObject* parent) : QObject(parent), m_host(host) {}
    QWidget* widget(int index) const override { return formSectionPage(m_host, index); }
    int count() const override { return m_host->count(); }
    int currentIndex() const override { return qMin(m_current, count() - 1); }
    bool canAddWidget() const override { return true; }
    bool canRemove(int index) const override { return index >= 0 && index < count(); }
    void setCurrentIndex(int index) override
    {
        if (canRemove(index))
            m_current = index;
    }
    void addWidget(QWidget* page) override { insertWidget(count(), page); }
    void insertWidget(int index, QWidget* page) override { insertFormSection(m_host, index, page); }
    void remove(int index) override { removeFormSection(m_host, index); }

private:
    fluent::layout::Accordion* m_host;
    int m_current = 0;
};

class PageFactory final : public QExtensionFactory {
public:
    using QExtensionFactory::QExtensionFactory;
    QObject* createExtension(QObject* object, const QString& iid, QObject* parent) const override
    {
        if (iid != Q_TYPEID(QDesignerContainerExtension))
            return nullptr;
        if (auto* accordion = qobject_cast<fluent::layout::Accordion*>(object))
            return new AccordionExtension(accordion, parent);
        if (qobject_cast<fluent::layout::Field*>(object) || qobject_cast<fluent::layout::Expander*>(object)
            || qobject_cast<fluent::scrolling::ScrollView*>(object))
            return new PageExtension(static_cast<QWidget*>(object), parent);
        return nullptr;
    }
};

} // namespace fluentqt_designer
