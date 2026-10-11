#pragma once

#include <QPointer>
#include <QWidget>
#include "components/spatial/SpatialSession.h"
#include "components/spatial/SpatialView.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery::samples {

class SpatialPreviewPanel final : public QWidget {
public:
    using QWidget::QWidget;
    ~SpatialPreviewPanel() override
    {
        // Stop while this panel's native focus chain still exists. If an enclosing
        // host is already dying, Spatial avoids restoring content into that tree.
        // zh_CN: 在面板原生焦点链仍存活时停用；若外层宿主已开始销毁，Spatial 不再向其中恢复内容。
        if (auto* session =
                findChild<spatial::SpatialSession*>(QString(), Qt::FindDirectChildrenOnly))
            session->deactivate();
    }
};

inline void makeSpatialPreviewFlat(spatial::SpatialView* view)
{
    view->setSpatialEnabled(false);
    view->setRenderMode(spatial::SpatialView::RenderMode::Raster);
}

template <typename View> struct SpatialPreviewSurface {
    View* view;
    bool attached;
};

// Failure keeps the existing sample content in the native 2D host. A failed
// session/factory must not later regain 3D through Gallery's mode preference.
// zh_CN: 失败时保留原示例内容的原生二维宿主；模式偏好不得重新开启失败的表面。
template <typename Session>
SpatialPreviewSurface<spatial::SpatialView> createSpatialPreview(Session* session, QWidget* panel)
{
    QPointer<Session> guardedSession(session);
    const bool active = guardedSession && guardedSession->activate();
    QPointer<spatial::SpatialView> view;
    if (active && guardedSession)
        view = guardedSession->createView(panel);
    const bool attached = view && guardedSession && guardedSession->isActive();
    if (!view)
        view = new spatial::SpatialView(panel);
    if (!attached)
        makeSpatialPreviewFlat(view);
    return {view, attached};
}

template <typename View, typename Session>
SpatialPreviewSurface<View> createAttachedSpatialPreview(Session* session, QWidget* panel)
{
    QPointer<Session> guardedSession(session);
    const bool active = guardedSession && guardedSession->activate();
    QPointer<View> view(new View(panel));
    const bool attached = active && guardedSession && guardedSession->attachView(view) && view &&
                          guardedSession && guardedSession->isActive();
    if (!view)
        view = new View(panel);
    if (!attached)
        makeSpatialPreviewFlat(view);
    return {view, attached};
}

// Gallery owns one mode; SpatialView owns rendering and visibility adaptation.
// zh_CN: Gallery 统一管理模式，SpatialView 自行适配渲染后端和可见性。
class SpatialPreviewBinding final : public QObject {
public:
    explicit SpatialPreviewBinding(QWidget* panel, bool attached) : QObject(panel)
    {
        const QPointer<spatial::SpatialView> view(
            panel->findChild<spatial::SpatialView*>("spatialPreviewView"));
        const QPointer<spatial::SpatialSession> session(
            panel->findChild<spatial::SpatialSession*>());
        if (!view)
            return;
        if (!attached || !session) {
            makeSpatialPreviewFlat(view);
            return;
        }
        // Maintenance previews use an explicit local state without loading user preferences.
        // zh_CN: 维护预览使用显式局部状态，不加载用户偏好。
        if (panel->window()->property("galleryPreviewRouteId").isValid()) {
            view->setSpatialEnabled(false);
            connect(session, &spatial::SpatialSession::activeChanged, this, [view](bool active) {
                if (!active && view)
                    view->setSpatialEnabled(false);
            });
            return;
        }
        auto& settings = GallerySettings::instance();
        const auto synchronize = [this, view, session, &settings] {
            if (!view)
                return;
            m_synchronizing = true;
            view->setSpatialEnabled(session && session->isActive() && settings.spatialAvailable() &&
                                    settings.spatialModeEnabled());
            m_synchronizing = false;
        };
        connect(&settings, &GallerySettings::spatialModeEnabledChanged, this, synchronize);
        connect(&settings, &GallerySettings::spatialAvailabilityChanged, this, synchronize);
        connect(session, &spatial::SpatialSession::activeChanged, this, synchronize);
        connect(view, &spatial::SpatialView::spatialEnabledChanged, this,
                [this, session, &settings](bool enabled) {
                    // Tab/Escape can restore native input. Keep the whole Gallery in that mode.
                    // zh_CN: Tab/Escape 可恢复原生输入，Gallery 同步回到同一模式。
                    if (!m_synchronizing && !enabled && session && session->isActive())
                        settings.setSpatialModeEnabled(false);
                });
        synchronize();
    }

private:
    bool m_synchronizing = false;
};

} // namespace fluent::gallery::samples
