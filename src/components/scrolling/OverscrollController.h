#ifndef FLUENT_SCROLLING_OVERSCROLLCONTROLLER_H
#define FLUENT_SCROLLING_OVERSCROLLCONTROLLER_H

#include <functional>

#include <QObject>

class QWidget;
class QScrollBar;
class QWheelEvent;
class QVariantAnimation;
class QTimer;

namespace fluent::scrolling {

/**
 * @brief Reusable elastic-overscroll / bounce state machine for the collection views.
 * zh_CN: 集合视图共享的弹性 overscroll / 回弹状态机。
 *
 * Hoisted from ListView's reference implementation so GridView, TreeView, FlowView, and
 * ListView all share one wheel-handling path (the value, the bounce animation + timer, the
 * NoPhaseDiscrete cluster handling, and the elastic rubber-band). The host view forwards
 * wheelEvent() to handleWheel() and supplies a few hooks describing how it reads its scroll
 * range, performs an in-range scroll, and repaints; it renders the offset by reading value().
 * zh_CN: 自 ListView 的参考实现抽取，使 GridView/TreeView/FlowView/ListView 共用同一套滚轮处理
 *（overscroll 值、回弹动画+定时器、NoPhaseDiscrete 成簇处理、弹性橡皮筋）。宿主把 wheelEvent()
 * 转发到 handleWheel()，并通过若干 hook 描述如何读取滚动范围、执行范围内滚动、重绘；宿主读取 value()
 * 渲染偏移。
 *
 * Single-axis: the controller tracks one overscroll magnitude for the *active* scroll axis.
 * A horizontally-flowing view supplies isHorizontal() and its horizontal scrollbar via the
 * hooks; the value then maps to that axis.
 * zh_CN: 单轴：控制器只跟踪“当前滚动轴”的一个 overscroll 量。横向流动的视图通过 hook 提供
 * isHorizontal() 与横向滚动条，value() 即映射到该轴。
 */
class OverscrollController : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Host-supplied callbacks describing the view's scrolling specifics.
     * zh_CN: 宿主提供的回调，描述视图各自的滚动细节。
     */
    struct Hooks {
        /// Returns the active-axis scrollbar used for boundary checks. zh_CN: 当前轴用于边界判定的滚动条。
        std::function<QScrollBar*()> scrollBar;
        /// Performs an in-range scroll of scrollPx pixels on the active axis; returns whether it moved.
        /// zh_CN: 在当前轴执行 scrollPx 像素的范围内滚动；返回是否发生移动。
        std::function<bool(qreal scrollPx)> normalScroll;
        /// Called when the rendered offset changes so the host can repaint / sync chrome.
        /// zh_CN: 渲染偏移变化时调用，使宿主重绘 / 同步滚动条 chrome。
        std::function<void()> onOverscrollChanged;
        /// Default wheel handling when the gesture is not an overscroll case (may be empty → ignore).
        /// zh_CN: 非 overscroll 情形下的默认滚轮处理（可为空 → ignore）。
        std::function<void(QWheelEvent*)> fallbackWheel;
        /// True when the active scroll axis is horizontal (default vertical). zh_CN: 当前滚动轴是否为横向（默认纵向）。
        std::function<bool()> isHorizontal;
    };

    /**
     * @param viewport       The view's viewport (parenting / lifetime only).
     * @param discreteStepPx Pixels scrolled per mouse-wheel notch (delta 120).
     */
    OverscrollController(QWidget* viewport, qreal discreteStepPx, Hooks hooks,
                         QObject* parent = nullptr);

    /// Current overscroll offset in pixels for the active axis (0 when settled).
    /// zh_CN: 当前轴的 overscroll 偏移像素（静止时为 0）。
    qreal value() const { return m_overscroll; }

    bool isOverscrollEnabled() const { return m_overscrollEnabled; }
    void setOverscrollEnabled(bool enabled);

    bool isScrollChainingEnabled() const { return m_chaining; }
    void setScrollChainingEnabled(bool enabled);

    /// Runs the wheel state machine and sets accept/ignore on the event.
    /// zh_CN: 运行滚轮状态机，并在事件上设置 accept/ignore。
    void handleWheel(QWheelEvent* event);

    /// Animates the active-axis overscroll back to zero. zh_CN: 把当前轴 overscroll 动画回到 0。
    void startBounceBack();

    /// Stops any in-flight bounce and settles at the boundary immediately (no animation).
    /// Useful when the scroll axis changes. zh_CN: 立即停止回弹并落到边界（无动画），用于滚动轴改变时。
    void cancel();

private:
    void notifyChanged();   // Host repaint / chrome sync.
    bool horizontal() const;
    void resetNoPhaseCluster();
    void resetNoPhaseBoundaryBounce();

    QWidget* m_viewport = nullptr;
    qreal m_step = 0.0;
    Hooks m_hooks;

    qreal m_overscroll = 0.0;
    bool m_overscrollEnabled = true;
    bool m_chaining = false;

    QVariantAnimation* m_bounceAnim = nullptr;
    QTimer* m_bounceTimer = nullptr;

    // NoPhaseDiscrete (mouse wheel / RDP) cluster + boundary-bounce arming state.
    // zh_CN: NoPhaseDiscrete（鼠标滚轮 / RDP）成簇与边界回弹预备状态。
    qint64 m_lastNoPhaseTs = 0;
    qreal  m_clusterAccum = 0.0;
    int    m_clusterDir = 0;
    int    m_noPhaseBoundaryDir = 0;
    bool   m_noPhaseBounceArmed = false;
};

} // namespace fluent::scrolling

#endif // FLUENT_SCROLLING_OVERSCROLLCONTROLLER_H
