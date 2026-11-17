#ifndef OVERLAYSCRIM_H
#define OVERLAYSCRIM_H

#include <QColor>
#include <QImage>
#include <QPointer>
#include <QRect>
#include <QWidget>

#include "components/foundation/FluentElement.h"

class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

namespace fluent::overlay {

/**
 * @brief Same-window dim/scrim used behind modal overlays, drawers, and spotlights.
 * zh_CN: 用于模态浮层、抽屉与聚光引导背后的同窗口压暗遮罩。
 *
 * OverlayScrim paints directly onto the owning top-level window using SourceOver
 * (avoiding a separate translucent child surface) and optionally intercepts pointer
 * input, with optional rounded surface bounds and spotlight cut-outs.
 * zh_CN: OverlayScrim 直接在所属顶层窗口内以 SourceOver 绘制压暗和背景模糊
 *（不创建独立透明子表面），支持指针拦截、圆角边界和聚光灯挖空。
 */
class OverlayScrim : public QWidget, public FluentElement {
    Q_OBJECT
    /**
     * @brief Shared blur and dim fade progress from 0 (clear) to 1 (fully shown).
     * zh_CN: 模糊和压暗共用的淡入进度，从 0（透明）到 1（完全显示）。
     */
    Q_PROPERTY(double progress READ progress WRITE setProgress)
    /**
     * @brief Rounded cut-out kept undimmed (scrim-local coordinates).
     * zh_CN: 保持不压暗的圆角挖空（遮罩局部坐标）。
     */
    Q_PROPERTY(QRect spotlightRect READ spotlightRect WRITE setSpotlightRect)

public:
    explicit OverlayScrim(QWidget* parent = nullptr, const QString& objectName = QString());

    void setModalAndDim(bool modal, bool dim);

    void setOpacityProgress(qreal progress);
    qreal opacityProgress() const { return m_opacityProgress; }

    // Alias used by Dialog fade and spotlight animations.
    double progress() const { return m_opacityProgress; }
    void setProgress(double progress) { setOpacityProgress(progress); }

    /**
     * @brief Overrides the theme smoke color; an invalid color restores the theme token.
     * zh_CN: 覆盖主题烟雾色；无效颜色则恢复主题 token。
     */
    void setColor(const QColor& color);
    QColor color() const { return m_color; }

    void setDimOpacity(qreal opacity);
    void setBackdrop(QWidget* overlay, qreal radius);
    void refreshBackdrop();

    void setSurfaceRadius(int radius);
    int surfaceRadius() const { return m_surfaceRadius; }

    bool spotlightEnabled() const { return m_spotEnabled; }
    void setSpotlightEnabled(bool enabled);

    QRect spotlightRect() const { return m_spot; }
    void setSpotlightRect(const QRect& rect);

    void setSpotlightRadius(int radius);
    int spotlightRadius() const { return m_spotRadius; }

    void onThemeUpdated() override;

signals:
    void pressed(const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void handlePointerEvent(QEvent* event);
    void queueBackdropRefresh();

    bool m_modal = true;
    bool m_dim = true;
    qreal m_opacityProgress = 1.0;
    QColor m_color; // invalid -> themeSmoke(). zh_CN: 无效颜色时使用 themeSmoke()。
    qreal m_dimOpacity = -1.0;
    qreal m_blurRadius = 0.0;
    QPointer<QWidget> m_overlay;
    QImage m_backdrop;
    QSize m_backdropSize;
    bool m_refreshPending = false;
    bool m_capturing = false;
    int m_surfaceRadius = 0;
    bool m_spotEnabled = false;
    QRect m_spot;
    int m_spotRadius = 8;
};

} // namespace fluent::overlay

#endif // OVERLAYSCRIM_H
