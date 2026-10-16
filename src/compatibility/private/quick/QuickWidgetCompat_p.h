#pragma once

#include "QuickFeatures_p.h"
#include <QPointer>
#include <QQuickWidget>
#include <QQuickWindow>

namespace compatibility::detail {

class QuickWidget final : public QQuickWidget {
public:
    explicit QuickWidget(QWidget* parent) : QQuickWidget(parent) {}

protected:
    bool event(QEvent* event) override
    {
        const auto type = event->type();
#if FLUENT_QUICK_NEEDS_UNSHOWN_REPARENT_GUARD
        // Qt 6.2 reloads the component when an unshown preview is adopted by a
        // window. Before scene-graph initialization there are no GPU resources
        // to rebind, and first show will create them for the final host.
        // zh_CN: Qt 6.2 会在预览首次重父时重载组件；首次场景图初始化前保留已加载的场景。
        if (type == QEvent::WindowChangeInternal && !m_hasBeenShown && !quickWindow()->isSceneGraphInitialized())
            return QWidget::event(event);
        if (type == QEvent::Show)
            m_hasBeenShown = true;
#endif
        const QPointer<QuickWidget> lifetime(this);
        const bool handled = QQuickWidget::event(event);
        // Qt recreates the Quick window on adoption into another top-level host.
        // zh_CN: Qt 在接入另一顶层宿主时重建 Quick 窗口，须恢复透明底色。
        if (lifetime && type == QEvent::WindowChangeInternal)
            setClearColor(Qt::transparent);
        return handled;
    }

private:
#if FLUENT_QUICK_NEEDS_UNSHOWN_REPARENT_GUARD
    bool m_hasBeenShown = false;
#endif
};

} // namespace compatibility::detail
