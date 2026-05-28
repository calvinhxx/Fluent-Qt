#include "NavigationView.h"

#include <QEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QShowEvent>

#include "view/navigation/StackContentHost.h"

namespace view::navigation {

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

    QPainter painter(this);
    painter.fillRect(rect(), colors.bgCanvas);
    if (!visual.contentRect.isEmpty())
        painter.fillRect(visual.contentRect, colors.bgLayer);
    if (!visual.chromeRect.isEmpty())
        painter.fillRect(visual.chromeRect, colors.bgCanvas);

    painter.setPen(colors.strokeDivider);
    if (visual.effectiveMode == DisplayMode::Top && !visual.chromeRect.isEmpty()) {
        painter.drawLine(visual.chromeRect.bottomLeft(), visual.chromeRect.bottomRight());
    } else if (!visual.chromeRect.isEmpty() && visual.contentRect.left() > 0) {
        painter.drawLine(visual.contentRect.topLeft(), visual.contentRect.bottomLeft());
    }
}

bool NavigationView::event(QEvent* event)
{
    if (event->type() == QEvent::LayoutRequest)
        invalidateLayout(false);
    return QWidget::event(event);
}

void NavigationView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    invalidateLayout(false);
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

    if (isSideMode(next.effectiveMode))
        buildSideLayout(next, bounds);
    else
        buildTopLayout(next, bounds);

    const LayoutTransitionKind kind = transitionKind(previousVisual.effectiveMode, next.effectiveMode);
    const bool animateTransition = shouldAnimateLayoutTransition(previousVisual, next);

    m_layout = next;
    m_layoutDirty = false;
    if (animateTransition)
        startLayoutTransition(previousVisual, m_layout, kind == LayoutTransitionKind::None ? LayoutTransitionKind::Side : kind);
    else {
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
        m_contentHost->setGeometry(state.contentRect);
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

} // namespace view::navigation
