#include "BindingApi.h"

#include <support/interfacezoom/InterfaceZoom.h>

// The Python Gallery reaches the shared interface-zoom backend through these
// private forwarders; the backend owns every Qt private API call.
// zh_CN: Python Gallery 通过这些私有转发函数调用共享界面缩放后端；Qt 私有 API 调用均由后端负责。

bool supportsInterfaceZoomForBinding()
{
    return fluent::support::interfacezoom::isSupported();
}

bool applyInterfaceZoomForBinding(qreal zoom)
{
    return fluent::support::interfacezoom::apply(zoom);
}

bool prepareInterfaceZoomForBinding(qreal zoom)
{
    return fluent::support::interfacezoom::prepareStartup(zoom);
}

qreal appliedInterfaceZoomForBinding()
{
    return fluent::support::interfacezoom::appliedZoom();
}
