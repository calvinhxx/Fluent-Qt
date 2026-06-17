#ifndef NAVIGATIONVIEW_H
#define NAVIGATIONVIEW_H

#include <QPointer>
#include <QRect>
#include <QWidget>

#include "design/Breakpoints.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QPaintEvent;
class QEvent;
class QPropertyAnimation;
class QResizeEvent;
class QShowEvent;

namespace fluent::navigation {

class StackContentHost;

/**
 * @brief Lightweight navigation shell with pane display modes and content hosting.
 * zh_CN: 支持窗格显示模式和内容承载的轻量导航外壳。
 *
 * NavigationView owns pane geometry, top/side layout transitions, compact widths,
 * and responsive thresholds while leaving item lists and pages to callers.
 * zh_CN: NavigationView 管理窗格几何、顶部/侧边布局过渡、紧凑宽度和响应式阈值，
 * item 列表与页面内容由调用方提供。
 */
class NavigationView : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Requested navigation layout mode before responsive constraints are applied.
     * zh_CN: 响应式约束应用前请求的导航布局模式。
     */
    Q_PROPERTY(DisplayMode displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    /**
     * @brief Display mode after responsive constraints are applied.
     * zh_CN: 响应式约束应用后的实际显示模式。
     */
    Q_PROPERTY(DisplayMode effectiveDisplayMode READ effectiveDisplayMode NOTIFY effectiveDisplayModeChanged)
    /**
     * @brief Whether the navigation pane is open.
     * zh_CN: 导航窗格是否打开。
     */
    Q_PROPERTY(bool paneOpen READ isPaneOpen WRITE setPaneOpen NOTIFY paneOpenChanged)
    /**
     * @brief Width threshold for compact navigation mode.
     * zh_CN: 进入紧凑导航模式的宽度阈值。
     */
    Q_PROPERTY(int compactModeThresholdWidth READ compactModeThresholdWidth WRITE setCompactModeThresholdWidth NOTIFY compactModeThresholdWidthChanged)
    /**
     * @brief Width threshold for expanded navigation mode.
     * zh_CN: 进入展开导航模式的宽度阈值。
     */
    Q_PROPERTY(int expandedModeThresholdWidth READ expandedModeThresholdWidth WRITE setExpandedModeThresholdWidth NOTIFY expandedModeThresholdWidthChanged)
    /**
     * @brief Navigation pane width in expanded mode.
     * zh_CN: 展开模式下的导航窗格宽度。
     */
    Q_PROPERTY(int expandedPaneWidth READ expandedPaneWidth WRITE setExpandedPaneWidth NOTIFY expandedPaneWidthChanged)
    /**
     * @brief Navigation pane width in compact mode.
     * zh_CN: 紧凑模式下的导航窗格宽度。
     */
    Q_PROPERTY(int compactPaneWidth READ compactPaneWidth WRITE setCompactPaneWidth NOTIFY compactPaneWidthChanged)
    /**
     * @brief Top navigation bar height in pixels.
     * zh_CN: 顶部导航栏高度，单位为像素。
     */
    Q_PROPERTY(int topBarHeight READ topBarHeight WRITE setTopBarHeight NOTIFY topBarHeightChanged)
    /**
     * @brief Whether pane and layout-mode transitions are animated.
     * zh_CN: 窗格和布局模式切换是否播放过渡动画。
     */
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    /**
     * @brief Progress used by navigation layout transitions.
     * zh_CN: 导航布局转场使用的进度。
     */
    Q_PROPERTY(qreal layoutTransitionProgress READ layoutTransitionProgress WRITE setLayoutTransitionProgress)

public:
    enum class DisplayMode {
        Auto,
        Left,
        LeftCompact,
        LeftMinimal,
        Top
    };
    Q_ENUM(DisplayMode)

    explicit NavigationView(QWidget* parent = nullptr);
    ~NavigationView() override;

    DisplayMode displayMode() const { return m_displayMode; }
    void setDisplayMode(DisplayMode mode);
    DisplayMode effectiveDisplayMode() const;

    bool isPaneOpen() const { return m_paneOpen; }
    void setPaneOpen(bool open);

    int compactModeThresholdWidth() const { return m_compactModeThresholdWidth; }
    void setCompactModeThresholdWidth(int width);
    int expandedModeThresholdWidth() const { return m_expandedModeThresholdWidth; }
    void setExpandedModeThresholdWidth(int width);
    int expandedPaneWidth() const { return m_expandedPaneWidth; }
    void setExpandedPaneWidth(int width);
    int compactPaneWidth() const { return m_compactPaneWidth; }
    void setCompactPaneWidth(int width);
    int topBarHeight() const { return m_topBarHeight; }
    void setTopBarHeight(int height);
    bool isAnimationEnabled() const { return m_animationEnabled; }
    void setAnimationEnabled(bool enabled);

    QWidget* headerChromeWidget() const { return m_headerChromeWidget.data(); }
    void setHeaderChromeWidget(QWidget* widget);
    QWidget* mainChromeWidget() const { return m_mainChromeWidget.data(); }
    void setMainChromeWidget(QWidget* widget);
    QWidget* footerChromeWidget() const { return m_footerChromeWidget.data(); }
    void setFooterChromeWidget(QWidget* widget);

    StackContentHost* contentHost() const
    {
        ensureLayout();
        return m_contentHost;
    }

    QRect chromeGeometry() const;
    QRect paneGeometry() const;
    QRect topBarGeometry() const;
    QRect contentGeometry() const;
    QRect headerChromeGeometry() const;
    QRect mainChromeGeometry() const;
    QRect footerChromeGeometry() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void displayModeChanged(DisplayMode mode);
    void effectiveDisplayModeChanged(DisplayMode mode);
    void paneOpenChanged(bool open);
    void compactModeThresholdWidthChanged(int width);
    void expandedModeThresholdWidthChanged(int width);
    void expandedPaneWidthChanged(int width);
    void compactPaneWidthChanged(int width);
    void topBarHeightChanged(int height);
    void animationEnabledChanged(bool enabled);
    void headerChromeWidgetChanged(QWidget* widget);
    void mainChromeWidgetChanged(QWidget* widget);
    void footerChromeWidgetChanged(QWidget* widget);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    struct LayoutState {
        QRect bounds;
        QRect chromeRect;
        QRect contentRect;
        QRect headerChromeRect;
        QRect mainChromeRect;
        QRect footerChromeRect;
        DisplayMode effectiveMode = DisplayMode::Left;
    };

    enum class LayoutTransitionKind {
        None,
        TopSide,
        Side
    };

    qreal layoutTransitionProgress() const { return m_layoutTransitionProgress; }
    void setLayoutTransitionProgress(qreal progress);

    DisplayMode resolveDisplayMode(int availableWidth) const;
    bool isSideMode(DisplayMode mode) const;
    int chromeWidthForMode(DisplayMode mode) const;
    bool shouldAnimateLayoutTransition(const LayoutState& from, const LayoutState& to) const;
    LayoutTransitionKind transitionKind(DisplayMode from, DisplayMode to) const;
    void startLayoutTransition(const LayoutState& from, const LayoutState& to, LayoutTransitionKind kind);
    void finishLayoutTransition();
    void stopLayoutTransition();
    LayoutState currentVisualLayout() const;
    LayoutState interpolatedLayout(const LayoutState& from, const LayoutState& to, qreal progress, LayoutTransitionKind kind) const;
    void invalidateLayout(bool updateGeometryHint = true);
    void ensureLayout() const;
    void updateLayout();
    void buildSideLayout(LayoutState& state, const QRect& bounds);
    void buildTopLayout(LayoutState& state, const QRect& bounds);
    void applyChildGeometries();
    void applyChildGeometries(const LayoutState& state);
    void assignChromeWidget(QPointer<QWidget>& slot, QWidget* widget);
    int preferredHeight(QWidget* widget, int fallback = 0) const;
    int preferredWidth(QWidget* widget, int fallback = 0) const;

    mutable LayoutState m_layout;
    mutable bool m_layoutDirty = true;
    // True only while handling a resize: responsive mode changes then snap instead of animating,
    // so a live drag can't kick off (and immediately interrupt) a layout transition — the source
    // of the janky, half-transitioned nav on Windows. zh_CN: 仅在处理 resize 期间为真：此时响应式
    // 模式切换直接吸附而非播放动画，避免拖拽中途触发又立刻打断布局过渡——Windows 上导航栏卡顿、半过渡的根因。
    bool m_inResize = false;
    DisplayMode m_displayMode = DisplayMode::Auto;
    DisplayMode m_lastEffectiveDisplayMode = DisplayMode::Left;
    bool m_paneOpen = true;
    int m_compactModeThresholdWidth = 640;
    int m_expandedModeThresholdWidth = 1008;
    int m_expandedPaneWidth = Breakpoints::NavigationPaneExpandedWidth;
    int m_compactPaneWidth = Breakpoints::NavigationPaneCompactWidth;
    int m_topBarHeight = 48;
    bool m_animationEnabled = true;
    QPropertyAnimation* m_layoutAnimation = nullptr;
    qreal m_layoutTransitionProgress = 1.0;
    LayoutTransitionKind m_layoutTransitionKind = LayoutTransitionKind::None;
    LayoutState m_layoutTransitionFrom;
    LayoutState m_layoutTransitionTo;
    QPointer<QWidget> m_headerChromeWidget;
    QPointer<QWidget> m_mainChromeWidget;
    QPointer<QWidget> m_footerChromeWidget;
    StackContentHost* m_contentHost = nullptr;
    // Thin top overlay that carves the content's rounded top-left corner (revealing the pane
    // backdrop) and strokes the frame border. zh_CN: 薄覆盖层，挖出内容左上圆角（露出窗格背景）并描边框。
    QWidget* m_contentFrameOverlay = nullptr;
};

} // namespace fluent::navigation

Q_DECLARE_METATYPE(fluent::navigation::NavigationView::DisplayMode)

#endif // NAVIGATIONVIEW_H
