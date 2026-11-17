#ifndef SCROLLVIEW_H
#define SCROLLVIEW_H

#include <QScrollArea>
#include <QPropertyAnimation>
#include <QElapsedTimer>
#include <QPointer>
#include <QPointF>
#include <QSizeF>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/WidgetOwnership.h"

class QScrollBar;
class QWheelEvent;
class QObject;

namespace fluent::scrolling {

class ScrollBar;

/**
 * @brief Interface for ScrollView content widgets that handle zoom directly.
 * zh_CN: 供直接响应缩放的 ScrollView 内容控件实现的接口。
 */
class ScrollViewZoomAware {
public:
    virtual ~ScrollViewZoomAware() = default;

    virtual QSizeF scrollViewUnscaledSize() const = 0;
    virtual void setScrollViewZoomFactor(qreal factor) = 0;
};

/**
 * @brief Scrollable content container with Fluent scroll bars and optional zoom support.
 * zh_CN: 带 Fluent 滚动条与可选缩放能力的可滚动内容容器。
 */
class ScrollView : public QScrollArea, public FluentElement, public QMLPlus {
    Q_OBJECT

    /**
     * @brief Horizontal scrolling interaction mode.
     * zh_CN: 水平滚动交互模式。
     */
    Q_PROPERTY(ScrollMode horizontalScrollMode
        READ horizontalScrollMode
        WRITE setHorizontalScrollMode
        NOTIFY horizontalScrollModeChanged)
    /**
     * @brief Vertical scrolling interaction mode.
     * zh_CN: 垂直滚动交互模式。
     */
    Q_PROPERTY(ScrollMode verticalScrollMode
        READ verticalScrollMode
        WRITE setVerticalScrollMode
        NOTIFY verticalScrollModeChanged)
    /**
     * @brief Visibility policy for the horizontal scrollbar.
     * zh_CN: 水平滚动条可见性策略。
     */
    Q_PROPERTY(ScrollBarVisibility horizontalScrollBarVisibility
        READ horizontalScrollBarVisibility
        WRITE setHorizontalScrollBarVisibility
        NOTIFY horizontalScrollBarVisibilityChanged)
    /**
     * @brief Visibility policy for the vertical scrollbar.
     * zh_CN: 垂直滚动条可见性策略。
     */
    Q_PROPERTY(ScrollBarVisibility verticalScrollBarVisibility
        READ verticalScrollBarVisibility
        WRITE setVerticalScrollBarVisibility
        NOTIFY verticalScrollBarVisibilityChanged)
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
    /**
     * @brief Whether wheel input at a scroll boundary forwards to an enclosing scroller (WinUI IsScrollChainingEnabled).
     * Disabled by default so nested ScrollViews consume wheel events across their scrollable range without scrolling the outer page mid-gesture.
     * zh_CN: 滚动到边界后滚轮事件是否继续传给外层滚动容器（对应 WinUI IsScrollChainingEnabled）。
     * 默认关闭，避免嵌套 ScrollView 到达边缘时在手势中途带动外层页面滚动。
     */
    Q_PROPERTY(bool scrollChainingEnabled
        READ isScrollChainingEnabled
        WRITE setScrollChainingEnabled
        NOTIFY scrollChainingEnabledChanged)
    /**
     * @brief Widget displayed inside the scroll viewport.
     * zh_CN: 显示在滚动视口中的内容控件。
     */
    Q_PROPERTY(QWidget* contentWidget READ contentWidget WRITE setContentWidget NOTIFY contentWidgetChanged)
    /**
     * @brief Ownership policy for the current content widget.
     * zh_CN: 当前内容控件的所有权策略。
     */
    Q_PROPERTY(fluent::WidgetOwnership contentOwnership READ contentOwnership NOTIFY contentOwnershipChanged)

public:
    enum class ScrollMode { Disabled, Enabled, Auto };
    Q_ENUM(ScrollMode)

    enum class ScrollBarVisibility { Disabled, Auto, Visible, Hidden };
    Q_ENUM(ScrollBarVisibility)

    enum class ZoomMode { Disabled, Enabled };
    Q_ENUM(ZoomMode)

    explicit ScrollView(QWidget* parent = nullptr);
    ~ScrollView() override;

    /**
     * @brief Installs `widget` with Owned ownership (matching QScrollArea).
     * zh_CN: 按与 QScrollArea 一致的 Owned 语义设置内容控件。
     */
    void setWidget(QWidget* widget);

    /**
     * @brief Detaches and returns the current content widget without deleting it.
     * zh_CN: 卸下并返回当前内容控件，不销毁该控件。
     */
    QWidget* takeWidget();

    QWidget* contentWidget() const;
    WidgetOwnership contentOwnership() const;
    void setContentWidget(QWidget* widget);

    /**
     * @brief Installs `widget` as content with an explicit ownership policy; returns true if applied.
     * zh_CN: 按指定所有权策略设置内容控件，成功应用时返回 true。
     */
    bool setContentWidget(QWidget* widget, WidgetOwnership ownership);

    /**
     * @brief Detaches and returns the content widget as a parentless widget owned by the caller.
     * zh_CN: 将内容控件卸下并置为无父对象后返回，由调用方负责销毁。
     */
    QWidget* takeContentWidget();

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

    bool isScrollChainingEnabled() const { return m_scrollChainingEnabled; }
    void setScrollChainingEnabled(bool enabled);

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
    void scrollChainingEnabledChanged();
    void scrollPositionChanged(int horizontalOffset, int verticalOffset);
    void contentWidgetChanged(QWidget* widget);
    void contentOwnershipChanged(WidgetOwnership ownership);

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void onThemeUpdated() override;
    bool viewportEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    enum class Axis { Horizontal, Vertical };

    void init();
    void synchronizeContentWidget(bool notify = true);
    void observeContentWidget(QWidget* content, QWidget* originalParent, WidgetOwnership ownership, bool notify);
    QWidget* releaseContentWidget(bool deleteOwned, bool restoreParent);
    void applyScrollPolicies();
    void applyAxisPolicy(Axis axis);
    void syncFloatingScrollBar();
    void positionFloatingScrollBar();
    void updateViewportPalette();
    void updateCornerWidget();
    void stopAnimations();
    void stopZoomAnimation();
    void animateScrollBar(QScrollBar* scrollBar, QPropertyAnimation* animation, int targetValue, bool fast = false);
    bool handleDiscreteWheel(QWheelEvent* event);
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
    bool hasScrollableRange() const;
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
    bool m_scrollChainingEnabled = false;

    QPropertyAnimation* m_horizontalAnimation = nullptr;
    QPropertyAnimation* m_verticalAnimation = nullptr;
    QPropertyAnimation* m_zoomAnimation = nullptr;
    QWidget* m_cornerWidget = nullptr;
    // Floating overlay bar used in vertical Visible mode without reserving layout gutter.
    ScrollBar* m_floatingVerticalBar = nullptr;
    QPointer<QWidget> m_observedContentWidget;
    QPointer<QWidget> m_originalContentParent;
    WidgetOwnership m_contentOwnership = WidgetOwnership::Owned;
    ScrollViewZoomAware* m_zoomAwareContent = nullptr;
    QSizeF m_unscaledContentSize;
    QPointF m_zoomAnimationAnchor = QPointF(-1.0, -1.0);
    QElapsedTimer m_lastNativeZoomGestureTime;
    bool m_nativeZoomGestureActive = false;
    bool m_nativeZoomGestureHasZoomed = false;
    bool m_contentMutationInProgress = false;
};

} // namespace fluent::scrolling

#endif // SCROLLVIEW_H
