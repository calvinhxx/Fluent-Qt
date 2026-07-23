#ifndef OVERLAYSCRIM_H
#define OVERLAYSCRIM_H

#include <QColor>
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
 * OverlayScrim paints into the owning top-level's shared backing (SourceOver, no translucent
 * child surface) and optionally intercepts pointer input. Optional rounded surface and spotlight
 * cut-outs cover Dialog-style smoke and intro-tour highlights without a second scrim type.
 * zh_CN: OverlayScrim 画进宿主顶层的共享后备缓冲（SourceOver，无独立透明子表面），并可拦截指针输入。
 * 可选圆角表面与聚光挖空覆盖 Dialog 式烟雾与引导高亮，无需第二套遮罩类型。
 */
class OverlayScrim : public QWidget, public FluentElement {
    Q_OBJECT
    /**
     * @brief Dim fade progress from 0 (clear) to 1 (full smoke opacity).
     * zh_CN: 压暗淡入进度，从 0（透明）到 1（完整烟雾不透明度）。
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

    // Alias used by Dialog smoke fade and Gallery intro tour animations.
    // zh_CN: Dialog 烟雾淡入与 Gallery 引导动画使用的别名。
    double progress() const { return m_opacityProgress; }
    void setProgress(double progress) { setOpacityProgress(progress); }

    /**
     * @brief Overrides the theme smoke color; an invalid color restores the theme token.
     * zh_CN: 覆盖主题烟雾色；无效颜色则恢复主题 token。
     */
    void setColor(const QColor& color);
    QColor color() const { return m_color; }

    void setSurfaceRadius(int radius);
    int surfaceRadius() const { return m_surfaceRadius; }

    bool spotlightEnabled() const { return m_spotEnabled; }
    void setSpotlightEnabled(bool enabled);

    QRect spotlightRect() const { return m_spot; }
    void setSpotlightRect(const QRect& rect);

    void setSpotlightRadius(int radius);
    int spotlightRadius() const { return m_spotRadius; }

    void onThemeUpdated() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void handlePointerEvent(QEvent* event);

    bool m_modal = true;
    bool m_dim = true;
    qreal m_opacityProgress = 1.0;
    QColor m_color;  // invalid → themeSmoke(). zh_CN: 无效则用 themeSmoke()。
    int m_surfaceRadius = 0;
    bool m_spotEnabled = false;
    QRect m_spot;
    int m_spotRadius = 8;
};

} // namespace fluent::overlay

#endif // OVERLAYSCRIM_H
