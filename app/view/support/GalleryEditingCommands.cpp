#include "GalleryEditingCommands.h"

#include <QString>
#include <QWidget>

#include "components/textfields/EditingCommandRouter.h"

namespace fluent::gallery {
namespace {

using fluent::textfields::EditingCommandRouter;

const QString& routerObjectName()
{
    static const QString name =
        QStringLiteral("Gallery.WindowEditingCommandRouter");
    return name;
}

} // namespace

EditingCommandRouter* galleryWindowEditingCommandRouter(
    QWidget* fallbackContext)
{
    if (!fallbackContext)
        return nullptr;

    QWidget* scopeWindow = fallbackContext->window();
    if (!scopeWindow)
        scopeWindow = fallbackContext;
    if (auto* router =
            scopeWindow->findChild<EditingCommandRouter*>(
                routerObjectName(),
                Qt::FindDirectChildrenOnly)) {
        return router;
    }

    auto* router =
        new EditingCommandRouter(scopeWindow, scopeWindow);
    router->setObjectName(routerObjectName());
    return router;
}

} // namespace fluent::gallery
