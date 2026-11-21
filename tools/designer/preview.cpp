#include "DesignerForms.h"
#include "ui_ContentDialog.h"
#include "ui_Dialog.h"
#include "ui_SpatialItem.h"
#include "ui_Window.h"
#include "ui_preview.h"

#include <FluentQt/FluentQt.h>
#include <QApplication>
#include <QDebug>
#include <memory>

int main(int argc, char** argv)
{
    fluent::prepareHighDpiApplication();
    QApplication app(argc, argv);
    fluent::initializeResources();
    auto body = std::make_unique<QWidget>();
    const QString name = app.arguments().value(1);
    if (name.isEmpty())
        Ui::ProjectSettings().setupUi(body.get());
    else if (name == QStringLiteral("Window"))
        Ui::WindowContent().setupUi(body.get());
    else if (name == QStringLiteral("Dialog"))
        Ui::DialogContent().setupUi(body.get());
    else if (name == QStringLiteral("ContentDialog"))
        Ui::ContentDialogContent().setupUi(body.get());
    else if (name == QStringLiteral("SpatialItem"))
        Ui::SpatialItemContent().setupUi(body.get());
    else {
        qWarning() << "Choose Window, Dialog, ContentDialog or SpatialItem.";
        return 2;
    }

    auto* host = fluentqt_designer::makeFormHost(body.get());
    if (!host) {
        qWarning() << "This form needs the Spatial extension.";
        return 2;
    }
    body.release(); // The host owns the form body, or is itself the form.
    std::unique_ptr<QWidget> window(host);
    fluentqt_designer::showFormHost(host);
    return app.exec();
}
