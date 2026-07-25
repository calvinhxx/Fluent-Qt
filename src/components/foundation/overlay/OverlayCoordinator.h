#ifndef FLUENTQT_COMPONENTS_FOUNDATION_OVERLAY_OVERLAYCOORDINATOR_H
#define FLUENTQT_COMPONENTS_FOUNDATION_OVERLAY_OVERLAYCOORDINATOR_H

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QString>

#include "components/foundation/overlay/OverlayScrim.h"

namespace fluent::overlay {

/**
 * @brief Internal lifecycle coordinator for same-window overlays.
 * zh_CN: 同窗口浮层使用的内部生命周期协调器。
 *
 * It owns top-level attachment, host resize tracking, scrim lifetime, geometry,
 * and the scrim-to-overlay stacking order. Interaction and placement policies
 * remain in the concrete overlay component.
 * zh_CN: 它统一管理顶层挂载、宿主尺寸跟踪、遮罩生命周期与几何，以及
 * scrim 到 overlay 的层叠顺序；交互和定位策略仍由具体浮层组件负责。
 */
class OverlayCoordinator final : public QObject {
    Q_OBJECT

public:
    enum class ScrimDeletion {
        Deferred,
        Immediate,
    };

    explicit OverlayCoordinator(QWidget* overlay, QObject* parent = nullptr);
    ~OverlayCoordinator() override;

    QWidget* overlayWidget() const { return m_overlay.data(); }
    QWidget* topLevelWidget() const { return m_topLevel.data(); }
    OverlayScrim* scrim() const { return m_scrim.data(); }

    void attachTo(QWidget* topLevel);
    void detach();

    OverlayScrim* ensureScrim(const QString& objectName);
    void releaseScrim(
        ScrimDeletion deletion = ScrimDeletion::Deferred);
    void syncScrimGeometry();
    void raiseStack();

signals:
    void hostGeometryChanged();
    void hostDestroyed();
    void scrimPressed(const QPoint& globalPos);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QPointer<QWidget> m_overlay;
    QPointer<QWidget> m_topLevel;
    QPointer<OverlayScrim> m_scrim;
};

} // namespace fluent::overlay

#endif // FLUENTQT_COMPONENTS_FOUNDATION_OVERLAY_OVERLAYCOORDINATOR_H
