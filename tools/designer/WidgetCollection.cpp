#include "fluentqt_designer.h"

#include <FluentQt/FluentQt.h>
#include <QtUiPlugin/customwidget.h>

namespace {

class WidgetEntry final : public QObject, public QDesignerCustomWidgetInterface {
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
    using Factory = QWidget* (*)(QWidget*);

    WidgetEntry(QObject* parent,
        const QMetaObject& metaObject,
        const char* group,
        const char* xml,
        bool container,
        Factory factory)
        : QObject(parent), m_name(QString::fromLatin1(metaObject.className())), m_group(QString::fromUtf8(group)),
          m_xml(QString::fromUtf8(xml)), m_container(container), m_factory(factory)
    {}

    QString name() const override { return m_name; }
    QString group() const override { return m_group; }
    QString includeFile() const override { return QStringLiteral("fluentqt_designer.h"); }
    QString domXml() const override { return m_xml; }
    QString toolTip() const override { return m_name; }
    QString whatsThis() const override { return {}; }
    QIcon icon() const override { return QIcon(QStringLiteral(":/fluentqt/designer/icons/%1.png").arg(m_name)); }
    bool isContainer() const override { return m_container; }
    QWidget* createWidget(QWidget* parent) override
    {
        fluent::initializeResources();
        return m_factory(parent);
    }

private:
    QString m_name;
    QString m_group;
    QString m_xml;
    bool m_container;
    Factory m_factory;
};

template <typename Widget> WidgetEntry* entry(QObject* parent, const char* group, const char* xml, bool container)
{
    return new WidgetEntry(parent, Widget::staticMetaObject, group, xml, container, [](QWidget* host) -> QWidget* {
        return new Widget(host);
    });
}

} // namespace

class FluentQtDesignerCollection final : public QObject, public QDesignerCustomWidgetCollectionInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QDesignerCustomWidgetCollectionInterface_iid)
    Q_INTERFACES(QDesignerCustomWidgetCollectionInterface)

public:
    QList<QDesignerCustomWidgetInterface*> customWidgets() const override { return m_widgets; }

private:
    QList<QDesignerCustomWidgetInterface*> m_widgets{
#include "widget_entries.inc"
    };
};

#include "WidgetCollection.moc"
