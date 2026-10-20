#ifndef FLUENTQT_COMPONENTS_FOUNDATION_OVERLAY_OVERLAYCOORDINATOR_H
#define FLUENTQT_COMPONENTS_FOUNDATION_OVERLAY_OVERLAYCOORDINATOR_H

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QString>
#include <functional>

#include "components/foundation/overlay/OverlayScrim.h"

namespace fluent::overlay {

class OverlayInputFilter;

/**
 * @brief Internal lifecycle coordinator for same-window overlays.
 * zh_CN: 同窗口浮层使用的内部生命周期协调器。
 *
 * It owns top-level attachment, host resize tracking, scrim lifetime, geometry,
 * and the scrim-to-overlay stacking order. Backdrop configuration and modal input routing
 * are shared; concrete components retain placement, animation, and close results.
 * zh_CN: 它统一管理顶层挂载、宿主尺寸跟踪、遮罩生命周期与几何，以及
 * scrim 到 overlay 的层叠顺序，并共享背景配置和模态输入；具体组件保留定位、动画及关闭结果。
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
    bool focusOnOpenEnabled() const { return m_focusOnOpenEnabled; }
    void setFocusOnOpenEnabled(bool enabled) { m_focusOnOpenEnabled = enabled; }

    void attachTo(QWidget* topLevel);
    void detach();

    OverlayScrim* ensureScrim(const QString& objectName);
    void releaseScrim(ScrimDeletion deletion = ScrimDeletion::Deferred);
    void syncScrimGeometry();
    void raiseStack();

    // Shared state and normalization for public overlay facades.
    // zh_CN: 公共浮层外观共用的状态和参数归一化。
    bool isModal() const { return m_modal; }
    bool setModal(bool modal);
    bool isDim() const { return m_dim; }
    bool setDim(bool dim);
    double dimOpacity() const { return m_dimOpacity; }
    bool setDimOpacity(double opacity);
    double backdropBlurRadius() const { return m_blurRadius; }
    quint64 backdropBlurRevision() const { return m_blurRevision; }
    bool setBackdropBlurRadius(double radius);
    bool needsScrim() const;
    OverlayScrim* updateScrim(const QString& objectName);
    void refreshBackdrop();

    enum class DismissReason { OutsidePress, Escape };
    void setClosePolicy(int policy) { m_closePolicy = policy; }
    void setHitTest(std::function<bool(const QPoint&)> contains) { m_contains = std::move(contains); }
    void setOutsidePressConsumption(std::function<bool(const QPoint&)> consumes) { m_consumes = std::move(consumes); }
    void setInputActive(bool active);
    bool hasInputOverlayAbove() const;

signals:
    void hostGeometryChanged();
    void hostDestroyed();
    void scrimPressed(const QPoint& globalPos);
    void dismissRequested(DismissReason reason);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    friend class OverlayInputFilter;
    bool filterInput(QObject* watched, QEvent* event);
    bool containsGlobalPoint(const QPoint& point) const;
    void focusInside(bool backwards);

    QPointer<QWidget> m_overlay;
    QPointer<QWidget> m_topLevel;
    QPointer<OverlayScrim> m_scrim;
    bool m_focusOnOpenEnabled = true;
    bool m_modal = false;
    bool m_dim = false;
    double m_dimOpacity = -1.0;
    double m_blurRadius = 0.0;
    bool m_blurRadiusExplicit = false;
    quint64 m_blurRevision = 0;
    bool m_inputActive = false;
    OverlayInputFilter* m_inputFilter = nullptr;
    int m_closePolicy = 0;
    std::function<bool(const QPoint&)> m_contains;
    std::function<bool(const QPoint&)> m_consumes;
};

} // namespace fluent::overlay

#endif // FLUENTQT_COMPONENTS_FOUNDATION_OVERLAY_OVERLAYCOORDINATOR_H
