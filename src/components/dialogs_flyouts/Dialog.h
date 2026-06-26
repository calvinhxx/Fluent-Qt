#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QPropertyAnimation>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "design/Spacing.h"

class QHideEvent;

namespace fluent::dialogs_flyouts {

// ── SmokeOverlay ──────────────────────────────────────────────────────────────
/**
 * @brief Animated dimming layer used behind dialog content.
 * zh_CN: 用于 Dialog 内容背后的动画遮罩层。
 *
 * SmokeOverlay paints the smoke/dim surface inside the widget tree, avoiding
 * native-window opacity dependencies while keeping dialog transitions testable.
 * zh_CN: SmokeOverlay 在 widget 树内绘制烟雾/遮罩层，避免依赖原生窗口透明度，
 * 同时让 Dialog 过渡更易测试。
 */
class SmokeOverlay : public QWidget {
    Q_OBJECT
    /**
     * @brief Smoke overlay fade progress, from transparent to target dim color.
     * zh_CN: 遮罩淡入进度，从透明过渡到目标暗化颜色。
     */
    Q_PROPERTY(double progress READ progress WRITE setProgress)
    /**
     * @brief Rounded cut-out kept un-dimmed to spotlight a target (window-local coords).
     * zh_CN: 保持不压暗的圆角挖空区域，用于聚光高亮某个目标（窗口局部坐标）。
     */
    Q_PROPERTY(QRect spotlightRect READ spotlightRect WRITE setSpotlightRect)
public:
    explicit SmokeOverlay(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        // Don't auto-fill an opaque background: paintEvent composites the dim over the live content
        // beneath, and a spotlight cut-out must reveal that content un-dimmed. zh_CN: 不自动填充不透明背景：
        // paintEvent 把压暗叠加在下方实时内容上，聚光挖空处需露出未压暗的内容。
        setAttribute(Qt::WA_NoSystemBackground, true);
    }
    void setColor(const QColor& c) { m_color = c; update(); }
    double progress() const { return m_progress; }
    void   setProgress(double p) {
        if (qFuzzyCompare(m_progress, p)) return;
        m_progress = p;
        update();
    }

    // Spotlight: punch a rounded hole through the dim so one target stays at full brightness. Disabled
    // by default (empty rect) → uniform dim, so Dialog and other callers are unchanged. zh_CN: 聚光：在压暗层
    // 上挖一个圆角洞,让某个目标保持全亮。默认关闭(空矩形)→均匀压暗,Dialog 等调用方行为不变。
    bool spotlightEnabled() const { return m_spotEnabled; }
    void setSpotlightEnabled(bool e) {
        if (m_spotEnabled == e) return;
        m_spotEnabled = e;
        update();
    }
    QRect spotlightRect() const { return m_spot; }
    void setSpotlightRect(const QRect& r) {
        if (m_spot == r) return;
        m_spot = r;
        if (m_spotEnabled) update();
    }
    void setSpotlightRadius(int radius) {
        if (m_spotRadius == radius) return;
        m_spotRadius = radius;
        if (m_spotEnabled) update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        QColor c = m_color;
        c.setAlpha(int(m_color.alpha() * qBound(0.0, m_progress, 1.0)));
        if (!m_spotEnabled || m_spot.isEmpty()) {
            p.fillRect(rect(), c);
            return;
        }
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath dim;
        dim.addRect(QRectF(rect()));
        QPainterPath hole;
        hole.addRoundedRect(QRectF(m_spot), m_spotRadius, m_spotRadius);
        p.fillPath(dim.subtracted(hole), c);
    }
private:
    QColor  m_color{0, 0, 0, 102};
    double  m_progress = 0.0;
    bool    m_spotEnabled = false;
    QRect   m_spot;
    int     m_spotRadius = 8;
};
/**
 * @brief Fluent dialog window with smoke overlay, drag support, and enter/exit animation.
 * zh_CN: 带遮罩、拖拽支持和进出场动画的 Fluent 对话框窗口。
 *
 * Dialog provides the reusable dialog shell used by ContentDialog and related
 * modal surfaces, including progress animation and optional drag behavior.
 * zh_CN: Dialog 为 ContentDialog 等模态表面提供可复用外壳，包含进度动画和可选拖拽行为。
 */
class Dialog : public QDialog, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Dialog transition animation progress.
     * zh_CN: 对话框转场动画进度。
     */
    Q_PROPERTY(double animationProgress READ animationProgress WRITE setAnimationProgress)
    /**
     * @brief Whether the dialog can be dragged by users.
     * zh_CN: 用户是否可以拖拽对话框。
     */
    Q_PROPERTY(bool dragEnabled READ isDragEnabled WRITE setDragEnabled)
    /**
     * @brief Whether dialog enter/exit transitions are animated.
     * zh_CN: 对话框进入/退出过程是否播放过渡动画。
     */
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled)
public:
    explicit Dialog(QWidget *parent = nullptr);
    ~Dialog() override;

    void onThemeUpdated() override;

    int  shadowSize() const { return m_shadowSize; }

    void setDragEnabled(bool e)      { m_dragEnabled = e; }
    bool isDragEnabled()  const      { return m_dragEnabled; }

    void setSmokeEnabled(bool e)     { m_smokeEnabled = e; }
    bool isSmokeEnabled() const      { return m_smokeEnabled; }

    void setAnimationEnabled(bool e) { m_animationEnabled = e; }
    bool isAnimationEnabled() const  { return m_animationEnabled; }

    /**
     * @brief Uses a widget as the local theme source while the dialog is shown.
     * zh_CN: 指定对话框显示时继承局部主题的来源控件。
     */
    void setThemeSource(QWidget* source);

    double animationProgress() const { return m_animationProgress; }
    void   setAnimationProgress(double p);

    void open() override;
    int  exec() override;

    void done(int r) override;

protected:
    bool isAnimating() const { return m_isAnimating; }

    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event)   override;
    void hideEvent(QHideEvent* event)   override;

    void mousePressEvent(QMouseEvent* event)   override;
    void mouseMoveEvent(QMouseEvent* event)    override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    void drawShadow(QPainter& painter, const QRect& contentRect);

private:
    bool syncThemeOverrideFromSource();
    void showSmokeOverlay();
    void hideSmokeOverlay();

    const int m_shadowSize = ::Spacing::Standard;

    bool   m_smokeEnabled     = false;
    bool   m_dragEnabled       = true;
    QPoint m_dragPosition;
    QPointer<QWidget> m_themeSource;
    SmokeOverlay* m_smokeOverlay   = nullptr;

    bool   m_animationEnabled  = true;
    bool   m_isAnimating       = false;
    bool   m_isClosing         = false;
    double m_animationProgress = 1.0;
    int    m_closingResult     = 0;

    QPropertyAnimation* m_animation;
    QPropertyAnimation* m_smokeAnim       = nullptr;  // Smoke fade animation. zh_CN: Smoke 淡入淡出动画。
    bool                m_smokeFadingOut  = false;    // Whether the smoke is fading out. zh_CN: Smoke 是否正在淡出。
    QSize               m_targetSize;
    QSize               m_savedMinSize;
    QSize               m_savedMaxSize;
};

} // namespace fluent::dialogs_flyouts

#endif // DIALOG_H
