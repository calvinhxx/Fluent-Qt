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
 * GridView, TreeView, and FlowView previously each carried an identical ~70-line copy of
 * this wheel handling. The controller owns the overscroll value, the bounce animation, and
 * the timer; the host view forwards wheelEvent() to handleWheel() and supplies a few hooks
 * describing how it reads its scroll range, performs an in-range scroll, and repaints. The
 * host renders the offset itself (e.g. via verticalOffset() or a paint translate) by reading
 * value().
 * zh_CN: GridView/TreeView/FlowView 此前各自复制了约 70 行相同的滚轮处理。控制器持有 overscroll
 * 值、回弹动画与定时器；宿主视图把 wheelEvent() 转发到 handleWheel()，并通过若干 hook 描述如何读取
 * 滚动范围、执行范围内滚动、以及重绘。宿主自行读取 value() 渲染偏移（如 verticalOffset() 或绘制平移）。
 */
class OverscrollController : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Host-supplied callbacks describing the view's scrolling specifics.
     * zh_CN: 宿主提供的回调，描述视图各自的滚动细节。
     */
    struct Hooks {
        /// Returns the scrollbar used for boundary checks. zh_CN: 返回用于边界判定的滚动条。
        std::function<QScrollBar*()> scrollBar;
        /// Performs an in-range scroll of scrollPx pixels; returns whether the value moved.
        /// zh_CN: 执行 scrollPx 像素的范围内滚动；返回值是否发生变化。
        std::function<bool(qreal scrollPx)> normalScroll;
        /// Called when the overscroll value changes so the host can repaint / sync chrome.
        /// zh_CN: overscroll 值变化时调用，使宿主重绘 / 同步滚动条 chrome。
        std::function<void()> onOverscrollChanged;
        /// Default wheel handling when the gesture is not an overscroll case (may be empty).
        /// zh_CN: 非 overscroll 情形下的默认滚轮处理（可为空）。
        std::function<void(QWheelEvent*)> fallbackWheel;
    };

    /**
     * @param viewport      The view's viewport (kept for lifetime/parenting only).
     * @param discreteStepPx Pixels scrolled per mouse-wheel notch (delta 120).
     */
    OverscrollController(QWidget* viewport, qreal discreteStepPx, Hooks hooks,
                         QObject* parent = nullptr);

    /// Current overscroll offset in pixels (0 when settled). zh_CN: 当前 overscroll 偏移像素（静止时为 0）。
    qreal value() const { return m_overscroll; }

    bool isOverscrollEnabled() const { return m_overscrollEnabled; }
    void setOverscrollEnabled(bool enabled);

    bool isScrollChainingEnabled() const { return m_chaining; }
    void setScrollChainingEnabled(bool enabled);

    /// Runs the wheel state machine and sets accept/ignore on the event.
    /// zh_CN: 运行滚轮状态机，并在事件上设置 accept/ignore。
    void handleWheel(QWheelEvent* event);

    /// Animates the overscroll back to zero. zh_CN: 把 overscroll 动画回到 0。
    void startBounceBack();

private:
    void cancel();  // Stop any in-flight bounce and settle at the boundary. zh_CN: 停止回弹并落到边界。
    void notifyChanged();

    QWidget* m_viewport = nullptr;
    qreal m_step = 0.0;
    Hooks m_hooks;

    qreal m_overscroll = 0.0;
    bool m_overscrollEnabled = true;
    bool m_chaining = false;

    QVariantAnimation* m_bounceAnim = nullptr;
    QTimer* m_bounceTimer = nullptr;
};

} // namespace fluent::scrolling

#endif // FLUENT_SCROLLING_OVERSCROLLCONTROLLER_H
