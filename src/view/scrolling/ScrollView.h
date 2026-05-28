#ifndef SCROLLVIEW_H
#define SCROLLVIEW_H

#include <QScrollArea>
#include <QPropertyAnimation>
#include <QElapsedTimer>
#include <QPointer>
#include <QPointF>
#include <QSizeF>

#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"

class QScrollBar;
class QWheelEvent;
class QObject;

namespace view::scrolling {

class ScrollBar;

/**
 * @brief Helper content widget that exposes ScrollView zoom state to layout code.
 * zh_CN: 向布局代码暴露 ScrollView 缩放状态的辅助内容控件。
 *
 * ScrollViewZoomAware separates wheel and gesture observation from the public
 * ScrollView API while keeping zoom-sensitive content testable.
 * zh_CN: ScrollViewZoomAware 将滚轮/手势观察从 ScrollView 公开 API 中拆出，
 * 并让缩放敏感内容更易测试。
 */
class ScrollViewZoomAware {
public:
    virtual ~ScrollViewZoomAware() = default;

    virtual QSizeF scrollViewUnscaledSize() const = 0;
    virtual void setScrollViewZoomFactor(qreal factor) = 0;
};

/**
 * @brief Scrollable content host with Fluent scroll bars and optional zoom support.
 * zh_CN: 带 Fluent 滚动条和可选缩放能力的可滚动内容宿主。
 *
 * ScrollView hosts a single content widget and coordinates scroll modes, bar
 * visibility policies, custom scroll bars, gesture handling, and zoom bounds.
 * zh_CN: ScrollView 承载单个内容控件，并协调滚动模式、滚动条可见策略、自定义滚动条、
 * 手势处理和缩放边界。
 */
class ScrollView : public QScrollArea, public FluentElement, public QMLPlus {
    Q_OBJECT

    /**
     * @brief Horizontal scrolling interaction mode.
     * zh_CN: 水平滚动交互模式。
     */
    Q_PROPERTY(ScrollMode horizontalScrollMode READ horizontalScrollMode WRITE setHorizontalScrollMode NOTIFY horizontalScrollModeChanged)
    /**
     * @brief Vertical scrolling interaction mode.
     * zh_CN: 垂直滚动交互模式。
     */
    Q_PROPERTY(ScrollMode verticalScrollMode READ verticalScrollMode WRITE setVerticalScrollMode NOTIFY verticalScrollModeChanged)
    /**
     * @brief Visibility policy for the horizontal scrollbar.
     * zh_CN: 水平滚动条可见性策略。
     */
    Q_PROPERTY(ScrollBarVisibility horizontalScrollBarVisibility READ horizontalScrollBarVisibility WRITE setHorizontalScrollBarVisibility NOTIFY horizontalScrollBarVisibilityChanged)
    /**
     * @brief Visibility policy for the vertical scrollbar.
     * zh_CN: 垂直滚动条可见性策略。
     */
    Q_PROPERTY(ScrollBarVisibility verticalScrollBarVisibility READ verticalScrollBarVisibility WRITE setVerticalScrollBarVisibility NOTIFY verticalScrollBarVisibilityChanged)
    /**
     * @brief Zoom interaction mode.
     * zh_CN: 缩放交互模式。
     */
    Q_PROPERTY(ZoomMode zoomMode READ zoomMode WRITE setZoomMode NOTIFY zoomModeChanged)
    /**
     * @brief Current zoom factor applied to content.
     * zh_CN: 应用到内容的当前缩放因子。
     */
    Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY zoomFactorChanged)
    /**
     * @brief Minimum allowed zoom factor.
     * zh_CN: 允许的最小缩放因子。
     */
    Q_PROPERTY(qreal minZoomFactor READ minZoomFactor WRITE setMinZoomFactor NOTIFY minZoomFactorChanged)
    /**
     * @brief Maximum allowed zoom factor.
     * zh_CN: 允许的最大缩放因子。
     */
    Q_PROPERTY(qreal maxZoomFactor READ maxZoomFactor WRITE setMaxZoomFactor NOTIFY maxZoomFactorChanged)

public:
    enum class ScrollMode {
        Disabled,
        Enabled,
        Auto
    };
    Q_ENUM(ScrollMode)

    enum class ScrollBarVisibility {
        Disabled,
        Auto,
        Visible,
        Hidden
    };
    Q_ENUM(ScrollBarVisibility)

    enum class ZoomMode {
        Disabled,
        Enabled
    };
    Q_ENUM(ZoomMode)

    explicit ScrollView(QWidget* parent = nullptr);
    ~ScrollView() override;

    void setWidget(QWidget* widget);

    ScrollMode horizontalScrollMode() const { return m_horizontalScrollMode; }
    void setHorizontalScrollMode(ScrollMode mode);

    ScrollMode verticalScrollMode() const { return m_verticalScrollMode; }
    void setVerticalScrollMode(ScrollMode mode);

    ScrollBarVisibility horizontalScrollBarVisibility() const { return m_horizontalScrollBarVisibility; }
    void setHorizontalScrollBarVisibility(ScrollBarVisibility visibility);

    ScrollBarVisibility verticalScrollBarVisibility() const { return m_verticalScrollBarVisibility; }
    void setVerticalScrollBarVisibility(ScrollBarVisibility visibility);

    ZoomMode zoomMode() const { return m_zoomMode; }
    void setZoomMode(ZoomMode mode);

    qreal zoomFactor() const { return m_zoomFactor; }
    void setZoomFactor(qreal factor);

    qreal minZoomFactor() const { return m_minZoomFactor; }
    void setMinZoomFactor(qreal factor);

    qreal maxZoomFactor() const { return m_maxZoomFactor; }
    void setMaxZoomFactor(qreal factor);

    int horizontalOffset() const;
    int verticalOffset() const;
    int scrollableWidth() const;
    int scrollableHeight() const;

    Q_INVOKABLE void scrollTo(int x, int y, bool animated = false);
    Q_INVOKABLE void scrollBy(int dx, int dy, bool animated = false);
    Q_INVOKABLE void zoomTo(qreal factor, bool animated = false);
    Q_INVOKABLE void zoomBy(qreal factorMultiplier, bool animated = false);
    Q_INVOKABLE void resetZoom(bool animated = false);

signals:
    void horizontalScrollModeChanged();
    void verticalScrollModeChanged();
    void horizontalScrollBarVisibilityChanged();
    void verticalScrollBarVisibilityChanged();
    void zoomModeChanged();
    void zoomFactorChanged();
    void minZoomFactorChanged();
    void maxZoomFactorChanged();
    void scrollPositionChanged(int horizontalOffset, int verticalOffset);

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void onThemeUpdated() override;
    bool viewportEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    enum class Axis {
        Horizontal,
        Vertical
    };

    void init();
    void applyScrollPolicies();
    void applyAxisPolicy(Axis axis);
    void updateViewportPalette();
    void updateCornerWidget();
    void stopAnimations();
    void stopZoomAnimation();
    void animateScrollBar(QScrollBar* scrollBar, QPropertyAnimation* animation, int targetValue);
    int clampedTarget(QScrollBar* scrollBar, int value) const;
    bool isAxisEnabled(Axis axis) const;
    ScrollMode modeForAxis(Axis axis) const;
    ScrollBarVisibility visibilityForAxis(Axis axis) const;
    void captureContentBaseSize();
    void applyZoomToContent();
    void setZoomFactorAt(qreal factor, const QPointF& viewportAnchor);
    QPointF defaultZoomAnchor() const;
    QPointF effectiveZoomAnchor(const QPointF& viewportAnchor) const;
    qreal clampedZoomFactor(qreal factor) const;
    bool handleZoomWheel(QWheelEvent* event, QObject* source);
    bool shouldSuppressScrollWheel() const;
    bool handleNativeGesture(QEvent* event, QObject* source);
    bool handlePinchGesture(QEvent* event, QObject* source);
    QPointF mapEventPositionToViewport(QObject* source, const QPointF& position) const;

    ScrollMode m_horizontalScrollMode = ScrollMode::Auto;
    ScrollMode m_verticalScrollMode = ScrollMode::Auto;
    ScrollBarVisibility m_horizontalScrollBarVisibility = ScrollBarVisibility::Auto;
    ScrollBarVisibility m_verticalScrollBarVisibility = ScrollBarVisibility::Auto;
    ZoomMode m_zoomMode = ZoomMode::Disabled;
    qreal m_zoomFactor = 1.0;
    qreal m_minZoomFactor = 0.1;
    qreal m_maxZoomFactor = 10.0;

    QPropertyAnimation* m_horizontalAnimation = nullptr;
    QPropertyAnimation* m_verticalAnimation = nullptr;
    QPropertyAnimation* m_zoomAnimation = nullptr;
    QWidget* m_cornerWidget = nullptr;
    QPointer<QWidget> m_contentWidget;
    ScrollViewZoomAware* m_zoomAwareContent = nullptr;
    QSizeF m_unscaledContentSize;
    QPointF m_zoomAnimationAnchor = QPointF(-1.0, -1.0);
    QElapsedTimer m_lastNativeZoomGestureTime;
    bool m_nativeZoomGestureActive = false;
    bool m_nativeZoomGestureHasZoomed = false;
};

} // namespace view::scrolling

#endif // SCROLLVIEW_H
