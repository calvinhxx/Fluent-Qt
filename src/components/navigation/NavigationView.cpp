#include "NavigationView.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QShowEvent>

#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/navigation/StackContentHost.h"

namespace fluent::navigation {

namespace {
constexpr int kMinimumTopBarHeight = 40;
constexpr int kMinimumPaneWidth = 1;

int interpolatedInt(int from, int to, qreal progress)
{
    return qRound(from + (to - from) * progress);
}

QRect collapsedRectFor(const QRect& rect)
{
    if (rect.isEmpty())
        return QRect();
    return QRect(rect.topLeft(), QSize(0, 0));
}

QRect interpolatedRect(QRect from, QRect to, qreal progress)
{
    if (from.isEmpty())
        from = collapsedRectFor(to);
    if (to.isEmpty())
        to = collapsedRectFor(from);
    if (from.isEmpty() && to.isEmpty())
        return QRect();

    return QRect(interpolatedInt(from.x(), to.x(), progress),
                 interpolatedInt(from.y(), to.y(), progress),
                 qMax(0, interpolatedInt(from.width(), to.width(), progress)),
                 qMax(0, interpolatedInt(from.height(), to.height(), progress)));
}

QRect horizontallyCollapsedRectFor(const QRect& rect)
{
    if (rect.isEmpty())
        return QRect();
    return QRect(rect.left(), rect.top(), 0, rect.height());
}

QRect horizontallyInterpolatedRect(QRect from, QRect to, qreal progress)
{
    const bool fromEmpty = from.isEmpty();
    const bool toEmpty = to.isEmpty();
    if (fromEmpty)
        from = horizontallyCollapsedRectFor(to);
    if (toEmpty)
        to = horizontallyCollapsedRectFor(from);
    if (from.isEmpty() && to.isEmpty())
        return QRect();

    const QRect& verticalSource = toEmpty ? from : to;
    return QRect(interpolatedInt(from.x(), to.x(), progress),
                 verticalSource.y(),
                 qMax(0, interpolatedInt(from.width(), to.width(), progress)),
                 qMax(0, verticalSource.height()));
}

QRect offsetRevealRect(const QRect& target, const QPoint& startOffset, qreal progress)
{
    if (target.isEmpty())
        return QRect();
    const QRect from = target.translated(startOffset);
    return QRect(interpolatedInt(from.x(), target.x(), progress),
                 interpolatedInt(from.y(), target.y(), progress),
                 target.width(),
                 target.height());
}

// A thin, click-through overlay drawn on top of the (opaque) content panel. It carves the
// rounded top-left corner so the pane backdrop / Mica shows there, and strokes the frame
// border. Painting it on top (rather than behind) is what makes it survive the opaque content
// and the translucent window. zh_CN: 画在（不透明）内容面板之上的薄覆盖层、可点击穿透。它挖出左上
// 圆角让窗格背景/Mica 露出，并描出框边。画在「上面」（而非后面）才能越过不透明内容与半透明窗口存活。
class ContentFrameOverlay : public QWidget {
public:
    explicit ContentFrameOverlay(QWidget* parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);  // do not clear: keep the content beneath
    }

    void configure(qreal radius, const QColor& border)
    {
        if (qFuzzyCompare(m_radius + 1.0, radius + 1.0) && m_border == border)
            return;
        m_radius = qMax(0.0, radius);
        m_border = border;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (m_radius <= 0.0 || rect().isEmpty())
            return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // Carve the top-left corner (outside the rounded arc) to transparent so the backdrop
        // shows. zh_CN: 把左上角（圆弧之外）挖成透明，露出背景。
        QPainterPath cornerCut;
        cornerCut.moveTo(0.0, 0.0);
        cornerCut.lineTo(m_radius, 0.0);
        cornerCut.arcTo(QRectF(0.0, 0.0, 2.0 * m_radius, 2.0 * m_radius), 90.0, 90.0);
        cornerCut.closeSubpath();
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillPath(cornerCut, Qt::black);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        if (m_border.isValid() && m_border.alpha() > 0) {
            const QPainterPath frame = fluent::overlay::roundedCornerRectPath(
                QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), m_radius,
                /*TL*/ true, /*TR*/ false, /*BR*/ false, /*BL*/ false);
            painter.setPen(QPen(m_border, 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(frame);
        }
    }

private:
    qreal m_radius = 0.0;
    QColor m_border;
};
}

NavigationView::NavigationView(QWidget* parent)
    : QWidget(parent)
    , m_contentHost(new StackContentHost(this))
    , m_layoutAnimation(new QPropertyAnimation(this, "layoutTransitionProgress", this))
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(minimumSizeHint());
    m_contentHost->setTransitionAnimationEnabled(true);
    setAccessibleName(QStringLiteral("NavigationView"));

    connect(m_layoutAnimation, &QPropertyAnimation::finished, this, [this]() {
        finishLayoutTransition();
    });
}

NavigationView::~NavigationView() = default;

NavigationView::DisplayMode NavigationView::effectiveDisplayMode() const
{
    return resolveDisplayMode(width() > 0 ? width() : sizeHint().width());
}

void NavigationView::setDisplayMode(DisplayMode mode)
{
    if (m_displayMode == mode)
        return;
    m_displayMode = mode;
    invalidateLayout();
    emit displayModeChanged(m_displayMode);
}

void NavigationView::setPaneOpen(bool open)
{
    if (m_paneOpen == open)
        return;
    m_paneOpen = open;
    invalidateLayout(false);
    emit paneOpenChanged(m_paneOpen);
}

void NavigationView::setCompactModeThresholdWidth(int width)
{
    const int normalized = qMax(1, qMin(width, m_expandedModeThresholdWidth - 1));
    if (m_compactModeThresholdWidth == normalized)
        return;
    m_compactModeThresholdWidth = normalized;
    invalidateLayout();
    emit compactModeThresholdWidthChanged(m_compactModeThresholdWidth);
}

void NavigationView::setExpandedModeThresholdWidth(int width)
{
    const int normalized = qMax(m_compactModeThresholdWidth + 1, width);
    if (m_expandedModeThresholdWidth == normalized)
        return;
    m_expandedModeThresholdWidth = normalized;
    invalidateLayout();
    emit expandedModeThresholdWidthChanged(m_expandedModeThresholdWidth);
}

void NavigationView::setExpandedPaneWidth(int width)
{
    const int normalized = qMax(m_compactPaneWidth, width);
    if (m_expandedPaneWidth == normalized)
        return;
    m_expandedPaneWidth = normalized;
    invalidateLayout();
    emit expandedPaneWidthChanged(m_expandedPaneWidth);
}

void NavigationView::setCompactPaneWidth(int width)
{
    const int normalized = qMax(kMinimumPaneWidth, qMin(width, m_expandedPaneWidth));
    if (m_compactPaneWidth == normalized)
        return;
    m_compactPaneWidth = normalized;
    invalidateLayout();
    emit compactPaneWidthChanged(m_compactPaneWidth);
}

void NavigationView::setTopBarHeight(int height)
{
    const int normalized = qMax(kMinimumTopBarHeight, height);
    if (m_topBarHeight == normalized)
        return;
    m_topBarHeight = normalized;
    invalidateLayout();
    emit topBarHeightChanged(m_topBarHeight);
}

void NavigationView::setAnimationEnabled(bool enabled)
{
    if (m_animationEnabled == enabled)
        return;

    m_animationEnabled = enabled;
    if (!m_animationEnabled)
        stopLayoutTransition();
    emit animationEnabledChanged(m_animationEnabled);
}

void NavigationView::setHeaderChromeWidget(QWidget* widget)
{
    if (m_headerChromeWidget == widget)
        return;
    assignChromeWidget(m_headerChromeWidget, widget);
    invalidateLayout(false);
    emit headerChromeWidgetChanged(m_headerChromeWidget.data());
}

void NavigationView::setMainChromeWidget(QWidget* widget)
{
    if (m_mainChromeWidget == widget)
        return;
    assignChromeWidget(m_mainChromeWidget, widget);
    invalidateLayout(false);
    emit mainChromeWidgetChanged(m_mainChromeWidget.data());
}

void NavigationView::setFooterChromeWidget(QWidget* widget)
{
    if (m_footerChromeWidget == widget)
        return;
    assignChromeWidget(m_footerChromeWidget, widget);
    invalidateLayout(false);
    emit footerChromeWidgetChanged(m_footerChromeWidget.data());
}

QRect NavigationView::chromeGeometry() const
{
    ensureLayout();
    return m_layout.chromeRect;
}

QRect NavigationView::paneGeometry() const
{
    ensureLayout();
    return isSideMode(m_layout.effectiveMode) ? m_layout.chromeRect : QRect();
}

QRect NavigationView::topBarGeometry() const
{
    ensureLayout();
    return m_layout.effectiveMode == DisplayMode::Top ? m_layout.chromeRect : QRect();
}

QRect NavigationView::contentGeometry() const
{
    ensureLayout();
    return m_layout.contentRect;
}

QRect NavigationView::headerChromeGeometry() const
{
    ensureLayout();
    return m_layout.headerChromeRect;
}

QRect NavigationView::mainChromeGeometry() const
{
    ensureLayout();
    return m_layout.mainChromeRect;
}

QRect NavigationView::footerChromeGeometry() const
{
    ensureLayout();
    return m_layout.footerChromeRect;
}

QSize NavigationView::sizeHint() const
{
    return QSize(800, 600);
}

QSize NavigationView::minimumSizeHint() const
{
    return QSize(Breakpoints::MinWindowWidth, 240);
}

void NavigationView::onThemeUpdated()
{
    if (m_contentHost)
        m_contentHost->onThemeUpdated();
    for (QWidget* widget : {m_headerChromeWidget.data(), m_mainChromeWidget.data(), m_footerChromeWidget.data()}) {
        if (widget)
            widget->update();
    }
    invalidateLayout(false);
}

void NavigationView::paintEvent(QPaintEvent*)
{
    ensureLayout();
    const auto colors = themeColors();
    const LayoutState visual = currentVisualLayout();

    // Under a real Mica backdrop the pane (chrome) stays transparent so the OS-composited
    // backdrop shows through (it handles active/inactive + wallpaper tint). Otherwise the pane
    // uses the solid themeBackdrop shared with the title bar.
    // zh_CN: 有真实 Mica 背景时窗格（chrome）透明，露出系统合成背景（系统处理激活/非激活+壁纸着色）；
    // 否则窗格用与标题栏共用的纯色 themeBackdrop。
    const bool mica = window() && window()->property("fluentMicaBackdrop").toBool();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (!mica) {
        const QColor backdrop = themeBackdrop(isActiveWindow());
        painter.fillRect(rect(), backdrop);
        if (!visual.chromeRect.isEmpty())
            painter.fillRect(visual.chromeRect, backdrop);
    }

    // The content surface (layer + rounded top-left frame) is painted by the content host
    // itself (configured in applyChildGeometries) — painting it there, not here, keeps it
    // visible under a translucent Mica top-level, where an ancestor's paint in the host's
    // region is cleared. Top mode keeps a divider beneath the bar.
    // zh_CN: 内容表面（层 + 左上圆角框）由内容宿主自绘（在 applyChildGeometries 配置）——画在那里而非
    // 此处，才能在半透明 Mica 顶层下保留（祖先在宿主区域的绘制会被清除）。Top 模式在栏下保留分割线。
    if (visual.effectiveMode == DisplayMode::Top && !visual.chromeRect.isEmpty()) {
        painter.setPen(colors.strokeDivider);
        painter.drawLine(visual.chromeRect.bottomLeft(), visual.chromeRect.bottomRight());
    }
}

bool NavigationView::event(QEvent* event)
{
    if (event->type() == QEvent::LayoutRequest)
        invalidateLayout(false);
    else if (event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate)
        update();  // backdrop tracks window focus
    return QWidget::event(event);
}

void NavigationView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Snap (don't animate) responsive mode changes driven by this resize: during a live drag the
    // width changes every frame, so animating would start a transition that the next frame
    // interrupts, leaving a half-transitioned pane (the Windows jank). zh_CN: 让本次 resize 引发的
    // 响应式模式切换直接吸附（不播放动画）：拖拽时宽度逐帧变化，动画会被下一帧打断、留下半过渡窗格（Windows 卡顿）。
    m_inResize = true;
    invalidateLayout(false);
    m_inResize = false;
}

void NavigationView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    invalidateLayout(false);
}

NavigationView::DisplayMode NavigationView::resolveDisplayMode(int availableWidth) const
{
    switch (m_displayMode) {
    case DisplayMode::Left:
    case DisplayMode::LeftCompact:
    case DisplayMode::LeftMinimal:
    case DisplayMode::Top:
        return m_displayMode;
    case DisplayMode::Auto:
        break;
    }

    if (availableWidth >= m_expandedModeThresholdWidth)
        return DisplayMode::Left;
    if (availableWidth >= m_compactModeThresholdWidth)
        return DisplayMode::LeftCompact;
    return DisplayMode::LeftMinimal;
}

bool NavigationView::isSideMode(DisplayMode mode) const
{
    return mode != DisplayMode::Top;
}

int NavigationView::chromeWidthForMode(DisplayMode mode) const
{
    switch (mode) {
    case DisplayMode::Left:
        return m_expandedPaneWidth;
    case DisplayMode::LeftCompact:
        return m_paneOpen ? m_expandedPaneWidth : m_compactPaneWidth;
    case DisplayMode::LeftMinimal:
        return m_paneOpen ? m_expandedPaneWidth : 0;
    case DisplayMode::Top:
    case DisplayMode::Auto:
        return 0;
    }
    return 0;
}

void NavigationView::setLayoutTransitionProgress(qreal progress)
{
    const qreal normalized = qBound<qreal>(0.0, progress, 1.0);
    if (qAbs(m_layoutTransitionProgress - normalized) <= 0.0001)
        return;

    m_layoutTransitionProgress = normalized;
    applyChildGeometries();
    update();
}

bool NavigationView::shouldAnimateLayoutTransition(const LayoutState& from, const LayoutState& to) const
{
    if (!m_animationEnabled || !isVisible() || !window() || !window()->isVisible())
        return false;
    if (m_inResize)  // responsive changes while live-resizing snap; see resizeEvent. zh_CN: live-resize 中的响应式切换直接吸附。
        return false;
    if (from.bounds.isEmpty() || to.bounds.isEmpty() || from.bounds != to.bounds)
        return false;
    return transitionKind(from.effectiveMode, to.effectiveMode) != LayoutTransitionKind::None
        || (isSideMode(from.effectiveMode) && isSideMode(to.effectiveMode) && from.chromeRect != to.chromeRect);
}

NavigationView::LayoutTransitionKind NavigationView::transitionKind(DisplayMode from, DisplayMode to) const
{
    if (from == to)
        return LayoutTransitionKind::None;
    const bool fromTop = from == DisplayMode::Top;
    const bool toTop = to == DisplayMode::Top;
    if (fromTop != toTop)
        return LayoutTransitionKind::TopSide;
    if (!fromTop && !toTop)
        return LayoutTransitionKind::Side;
    return LayoutTransitionKind::None;
}

void NavigationView::startLayoutTransition(const LayoutState& from, const LayoutState& to, LayoutTransitionKind kind)
{
    if (!m_layoutAnimation || kind == LayoutTransitionKind::None) {
        stopLayoutTransition();
        return;
    }

    m_layoutAnimation->stop();
    m_layoutTransitionFrom = from;
    m_layoutTransitionTo = to;
    m_layoutTransitionKind = kind;
    m_layoutTransitionProgress = 0.0;
    applyChildGeometries();
    update();

    const auto animation = themeAnimation();
    m_layoutAnimation->setDuration(animation.normal);
    m_layoutAnimation->setEasingCurve(animation.decelerate);
    m_layoutAnimation->setStartValue(0.0);
    m_layoutAnimation->setEndValue(1.0);
    m_layoutAnimation->start();
}

void NavigationView::finishLayoutTransition()
{
    m_layoutTransitionProgress = 1.0;
    m_layoutTransitionKind = LayoutTransitionKind::None;
    applyChildGeometries(m_layout);
    update();
}

void NavigationView::stopLayoutTransition()
{
    if (m_layoutAnimation)
        m_layoutAnimation->stop();
    m_layoutTransitionProgress = 1.0;
    m_layoutTransitionKind = LayoutTransitionKind::None;
    applyChildGeometries(m_layout);
    update();
}

NavigationView::LayoutState NavigationView::currentVisualLayout() const
{
    if (m_layoutTransitionKind == LayoutTransitionKind::None || m_layoutTransitionProgress >= 1.0)
        return m_layout;
    return interpolatedLayout(m_layoutTransitionFrom, m_layoutTransitionTo, m_layoutTransitionProgress, m_layoutTransitionKind);
}

NavigationView::LayoutState NavigationView::interpolatedLayout(const LayoutState& from, const LayoutState& to, qreal progress, LayoutTransitionKind kind) const
{
    LayoutState visual;
    visual.bounds = to.bounds;
    if (kind == LayoutTransitionKind::TopSide) {
        visual.effectiveMode = to.effectiveMode;
        const QPoint chromeStartOffset = to.effectiveMode == DisplayMode::Top
            ? QPoint(0, -to.chromeRect.height())
            : QPoint(-to.chromeRect.width(), 0);
        visual.chromeRect = offsetRevealRect(to.chromeRect, chromeStartOffset, progress);
        visual.headerChromeRect = offsetRevealRect(to.headerChromeRect, chromeStartOffset, progress);
        visual.mainChromeRect = offsetRevealRect(to.mainChromeRect, chromeStartOffset, progress);
        visual.footerChromeRect = offsetRevealRect(to.footerChromeRect, chromeStartOffset, progress);
        visual.contentRect = interpolatedRect(from.contentRect, to.contentRect, progress);
        return visual;
    }

    visual.effectiveMode = progress < 0.5 ? from.effectiveMode : to.effectiveMode;
    const auto interpolate = kind == LayoutTransitionKind::Side ? horizontallyInterpolatedRect : interpolatedRect;
    visual.chromeRect = interpolate(from.chromeRect, to.chromeRect, progress);
    visual.contentRect = interpolate(from.contentRect, to.contentRect, progress);
    visual.headerChromeRect = interpolate(from.headerChromeRect, to.headerChromeRect, progress);
    visual.mainChromeRect = interpolate(from.mainChromeRect, to.mainChromeRect, progress);
    visual.footerChromeRect = interpolate(from.footerChromeRect, to.footerChromeRect, progress);
    return visual;
}

void NavigationView::invalidateLayout(bool updateGeometryHint)
{
    m_layoutDirty = true;
    if (updateGeometryHint)
        updateGeometry();
    update();
    updateLayout();
}

void NavigationView::ensureLayout() const
{
    if (!m_layoutDirty && m_layout.bounds == rect())
        return;
    const_cast<NavigationView*>(this)->updateLayout();
}

void NavigationView::updateLayout()
{
    const LayoutState previousVisual = currentVisualLayout();
    LayoutState next;
    const QRect bounds = rect();
    next.bounds = bounds;
    next.effectiveMode = resolveDisplayMode(bounds.width() > 0 ? bounds.width() : sizeHint().width());

    // Auto-managed pane open state (matches WinUI): when the adaptive layout first drops to
    // the compact rail or the hidden minimal mode, collapse the pane so it doesn't linger as
    // a wide overlay; when it returns to the expanded mode, open it. Otherwise an
    // expanded-mode `paneOpen=true` would make LeftMinimal keep a 256px overlay instead of
    // hiding. The menu button can still toggle the pane within a mode.
    // zh_CN: 自适应窗格开合（对齐 WinUI）：自适应布局首次降到紧凑栏或隐藏的最小模式时收起窗格，避免残留为
    // 宽浮层；回到展开模式时打开。否则展开模式遗留的 paneOpen=true 会让 LeftMinimal 仍保留 256px 浮层而非隐藏。
    // 菜单按钮仍可在同一模式内开关窗格。
    if (m_displayMode == DisplayMode::Auto && next.effectiveMode != m_lastEffectiveDisplayMode) {
        if (next.effectiveMode == DisplayMode::LeftCompact || next.effectiveMode == DisplayMode::LeftMinimal)
            m_paneOpen = false;
        else if (next.effectiveMode == DisplayMode::Left)
            m_paneOpen = true;
    }

    if (isSideMode(next.effectiveMode))
        buildSideLayout(next, bounds);
    else
        buildTopLayout(next, bounds);

    const LayoutTransitionKind kind = transitionKind(previousVisual.effectiveMode, next.effectiveMode);
    const bool animateTransition = shouldAnimateLayoutTransition(previousVisual, next);

    m_layout = next;
    m_layoutDirty = false;

    // If a transition toward this exact target (mode + bounds) is already in flight, let it
    // finish instead of restarting it. Restarting on every relayout — e.g. when an
    // effectiveDisplayModeChanged handler re-lays-out the pane — pins the animation at
    // progress 0 and leaves a half-transitioned chrome on screen. That feedback loop is why
    // the compact icon rail never fully collapsed into LeftMinimal.
    // zh_CN: 若已经在向这个确切目标（模式 + 边界）过渡，就让它跑完而不是重启。每次重排都重启
    //（如 effectiveDisplayModeChanged 处理器重排窗格时）会把动画钉在进度 0、留下半过渡的 chrome。
    // 这个反馈环正是紧凑图标栏始终收不成 LeftMinimal 的原因。
    const bool sameTargetInFlight =
        m_layoutTransitionKind != LayoutTransitionKind::None
        && m_layoutAnimation && m_layoutAnimation->state() == QAbstractAnimation::Running
        && m_layoutTransitionTo.effectiveMode == next.effectiveMode
        && m_layoutTransitionTo.bounds == next.bounds;
    if (sameTargetInFlight) {
        m_layoutTransitionTo = next;  // refresh target geometry, keep the running animation
    } else if (animateTransition) {
        startLayoutTransition(previousVisual, m_layout, kind == LayoutTransitionKind::None ? LayoutTransitionKind::Side : kind);
    } else {
        m_layoutTransitionKind = LayoutTransitionKind::None;
        m_layoutTransitionProgress = 1.0;
        if (m_layoutAnimation)
            m_layoutAnimation->stop();
        applyChildGeometries(m_layout);
    }

    if (m_lastEffectiveDisplayMode != m_layout.effectiveMode) {
        m_lastEffectiveDisplayMode = m_layout.effectiveMode;
        emit effectiveDisplayModeChanged(m_layout.effectiveMode);
    }
}

void NavigationView::buildSideLayout(LayoutState& state, const QRect& bounds)
{
    const int chromeWidth = qMin(chromeWidthForMode(state.effectiveMode), bounds.width());
    state.chromeRect = chromeWidth > 0 ? QRect(bounds.left(), bounds.top(), chromeWidth, bounds.height()) : QRect();
    const int contentLeft = state.effectiveMode == DisplayMode::Left ? chromeWidth : (state.effectiveMode == DisplayMode::LeftCompact ? m_compactPaneWidth : 0);
    state.contentRect = QRect(contentLeft, bounds.top(), qMax(0, bounds.width() - contentLeft), bounds.height());
    if (state.effectiveMode == DisplayMode::LeftMinimal && m_paneOpen)
        state.contentRect = bounds;

    if (state.chromeRect.isEmpty())
        return;

    const int headerHeight = qMin(preferredHeight(m_headerChromeWidget), state.chromeRect.height());
    const int footerHeight = qMin(preferredHeight(m_footerChromeWidget), qMax(0, state.chromeRect.height() - headerHeight));
    const int mainHeight = qMax(0, state.chromeRect.height() - headerHeight - footerHeight);

    int y = state.chromeRect.top();
    if (m_headerChromeWidget)
        state.headerChromeRect = QRect(state.chromeRect.left(), y, state.chromeRect.width(), headerHeight);
    y += headerHeight;
    if (m_mainChromeWidget)
        state.mainChromeRect = QRect(state.chromeRect.left(), y, state.chromeRect.width(), mainHeight);
    if (m_footerChromeWidget)
        state.footerChromeRect = QRect(state.chromeRect.left(), state.chromeRect.bottom() + 1 - footerHeight, state.chromeRect.width(), footerHeight);
}

void NavigationView::buildTopLayout(LayoutState& state, const QRect& bounds)
{
    state.chromeRect = QRect(bounds.left(), bounds.top(), bounds.width(), qMin(m_topBarHeight, bounds.height()));
    state.contentRect = QRect(bounds.left(), state.chromeRect.bottom() + 1, bounds.width(), qMax(0, bounds.height() - state.chromeRect.height()));

    const int headerWidth = qMin(preferredWidth(m_headerChromeWidget), state.chromeRect.width());
    const int footerWidth = qMin(preferredWidth(m_footerChromeWidget), qMax(0, state.chromeRect.width() - headerWidth));
    const int remainingAfterFixed = qMax(0, state.chromeRect.width() - headerWidth - footerWidth);
    const int mainWidth = qMin(preferredWidth(m_mainChromeWidget, remainingAfterFixed), remainingAfterFixed);

    int x = state.chromeRect.left();
    if (m_headerChromeWidget)
        state.headerChromeRect = QRect(x, state.chromeRect.top(), headerWidth, state.chromeRect.height());
    x += headerWidth;
    if (m_mainChromeWidget)
        state.mainChromeRect = QRect(x, state.chromeRect.top(), mainWidth, state.chromeRect.height());
    if (m_footerChromeWidget)
        state.footerChromeRect = QRect(state.chromeRect.right() + 1 - footerWidth, state.chromeRect.top(), footerWidth, state.chromeRect.height());
}

void NavigationView::applyChildGeometries()
{
    applyChildGeometries(currentVisualLayout());
}

void NavigationView::applyChildGeometries(const LayoutState& state)
{
    if (m_contentHost) {
        // The page paints the opaque content layer itself, so the host needs no surface.
        // zh_CN: 内容层由页面自绘不透明面板，宿主无需自绘表面。
        m_contentHost->setGeometry(state.contentRect);
        m_contentHost->setContentSurface(QColor(), 0.0, QColor());
        m_contentHost->show();
        m_contentHost->lower();
    }

    const auto apply = [](QWidget* widget, const QRect& geometry) {
        if (!widget)
            return;
        if (geometry.isValid() && !geometry.isEmpty()) {
            widget->setGeometry(geometry);
            widget->show();
            widget->raise();
        } else {
            widget->hide();
        }
    };

    apply(m_headerChromeWidget, state.headerChromeRect);
    apply(m_mainChromeWidget, state.mainChromeRect);
    apply(m_footerChromeWidget, state.footerChromeRect);

    // Content frame: a rounded top-left corner + border drawn on top of the opaque content,
    // but only when a pane sits to its left (framed side modes). zh_CN: 内容框：在不透明内容之上
    // 绘制左上圆角 + 边框，仅当左侧有窗格时（带框侧边模式）。
    const bool framed = isSideMode(state.effectiveMode)
                        && !state.contentRect.isEmpty()
                        && state.contentRect.left() > 0;
    if (framed) {
        if (!m_contentFrameOverlay)
            m_contentFrameOverlay = new ContentFrameOverlay(this);
        m_contentFrameOverlay->setGeometry(state.contentRect);
        // No border stroke: the 1px line down the content's left edge read as a divider/gap. Keep
        // only the rounded top-left carve, which reveals the (same) Mica backdrop seamlessly.
        // zh_CN: 不描边框：内容左缘的 1px 线看起来像分割线/间距。仅保留左上圆角挖切，无缝露出（同样的）Mica 背景。
        static_cast<ContentFrameOverlay*>(m_contentFrameOverlay)
            ->configure(themeRadius().overlay, QColor());
        m_contentFrameOverlay->show();
        m_contentFrameOverlay->raise();
    } else if (m_contentFrameOverlay) {
        m_contentFrameOverlay->hide();
    }
}

void NavigationView::assignChromeWidget(QPointer<QWidget>& slot, QWidget* widget)
{
    if (slot && slot->parentWidget() == this) {
        slot->hide();
        slot->setParent(nullptr);
    }

    slot = widget;
    if (slot) {
        slot->setParent(this);
        slot->show();
    }
}

int NavigationView::preferredHeight(QWidget* widget, int fallback) const
{
    if (!widget)
        return 0;
    if (widget->minimumHeight() > 0)
        return widget->minimumHeight();
    const int hint = widget->sizeHint().height();
    return hint > 0 ? hint : fallback;
}

int NavigationView::preferredWidth(QWidget* widget, int fallback) const
{
    if (!widget)
        return 0;
    if (widget->minimumWidth() > 0)
        return widget->minimumWidth();
    const int hint = widget->sizeHint().width();
    return hint > 0 ? hint : fallback;
}

} // namespace fluent::navigation
