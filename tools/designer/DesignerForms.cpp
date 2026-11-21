#include "DesignerForms.h"

#include <FluentQt/DialogsFlyouts.h>
#include <FluentQt/Windowing.h>
#ifdef FLUENT_QT_HAS_SPATIAL
#include <FluentQt/Spatial.h>
#endif
#include <QDialogButtonBox>
#include <QVariant>
#include <QVBoxLayout>

namespace fluentqt_designer {
using fluent::dialogs_flyouts::ContentDialog;
using fluent::dialogs_flyouts::Dialog;
using fluent::windowing::Window;

#ifdef FLUENT_QT_HAS_SPATIAL
fluent::spatial::SpatialItem* addSpatialForm(fluent::spatial::SpatialView* view, QWidget* body)
{
    if (!view || !body)
        return nullptr;
    auto* item = view->addWidget(body, fluent::WidgetOwnership::Owned);
    if (item) {
        const QVariant scale = body->property("fluentItemScale");
        if (scale.isValid())
            item->setScale(scale.toDouble());
        item->setRotation(QVector3D(0, body->property("fluentItemRotationY").toFloat(), 0));
    }
    return item;
}
#endif

QWidget* makeFormHost(QWidget* body, QWidget* owner)
{
    if (!body)
        return nullptr;
    const QString kind = body->property("fluentFormHost").toString();
    const QString title = body->windowTitle();
    const QSize size = body->size();
    QWidget* host = nullptr;
    if (kind.isEmpty())
        return body;
    if (owner == body || (owner && body->isAncestorOf(owner)))
        return nullptr;
    if (kind == QStringLiteral("Window")) {
        auto* window = new Window(owner);
        window->setContentWidget(body);
        host = window;
    } else if (kind == QStringLiteral("Dialog")) {
        auto* dialog = new Dialog(owner);
        auto* layout = new QVBoxLayout(dialog);
        layout->addWidget(body);
        if (auto* buttons = body->findChild<QDialogButtonBox*>(QStringLiteral("buttonBox"))) {
            QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
            QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
        }
        host = dialog;
    } else if (kind == QStringLiteral("ContentDialog")) {
        auto* dialog = new ContentDialog(owner);
        dialog->setTitle(title);
        dialog->setPrimaryButtonText(body->property("fluentPrimaryText").toString());
        dialog->setCloseButtonText(body->property("fluentCloseText").toString());
        dialog->setContent(body);
        host = dialog;
#ifdef FLUENT_QT_HAS_SPATIAL
    } else if (kind == QStringLiteral("SpatialItem")) {
        auto* view = new fluent::spatial::SpatialView(owner);
        if (!addSpatialForm(view, body)) {
            delete view;
            return nullptr;
        }
        host = view;
#endif
    } else {
        return nullptr;
    }
    host->setWindowTitle(title);
    host->resize(size.expandedTo(host->sizeHint()));
    return host;
}

void showFormHost(QWidget* host)
{
    if (auto* dialog = qobject_cast<Dialog*>(host))
        dialog->open();
    else if (host)
        host->show();
}
} // namespace fluentqt_designer
