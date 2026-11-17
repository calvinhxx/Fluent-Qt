#include "compatibility/InputEventCompat.h"
#include "compatibility/private/RuntimePlatformCapabilities_p.h"
#include "NavigationView.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QShowEvent>

#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/private/DpiPaintMetrics_p.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/navigation/StackContentHost.h"
#include "components/windowing/WindowBackdrop.h"
#include "design/Elevation.h"

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

    return QRect(interpolatedInt(from.x(), to.x(), progress), interpolatedInt(from.y(), to.y(), progress),
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
    return QRect(interpolatedInt(from.x(), to.x(), progress), verticalSource.y(),
        qMax(0, interpolatedInt(from.width(), to.width(), progress)), qMax(0, verticalSource.height()));
}

QRect offsetRevealRect(const QRect& target, const QPoint& startOffset, qreal progress)
{
    if (target.isEmpty())
        return QRect();
    const QRect from = target.translated(startOffset);
    return QRect(interpolatedInt(from.x(), target.x(), progress), interpolatedInt(from.y(), target.y(), progress),
        target.width(), target.height());
}

// Click-through overlay painted above the opaque content host to carve the rounded top-left
// notch for the window backdrop and stroke the frame border.
class ContentFrameOverlay : public QWidget {
public:
    explicit ContentFrameOverlay(QWidget* parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true); // do not clear: keep the content beneath
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

        // Carve the top-left corner outside the rounded arc only when using a composited system backdrop.
        // In Normal mode the top-level window on backdrop-capable Windows builds is still translucent, so
        // CompositionMode_Clear would punch a black/white hole into the opaque chrome backing.
        const bool realBackdrop = window() && window()->testAttribute(Qt::WA_TranslucentBackground)
            && windowing::windowBackdropRequiresTransparentClear(window());
        if (realBackdrop) {
            QPainterPath cornerCut;
            cornerCut.moveTo(0.0, 0.0);
            cornerCut.lineTo(m_radius, 0.0);
            cornerCut.arcTo(QRectF(0.0, 0.0, 2.0 * m_radius, 2.0 * m_radius), 90.0, 90.0);
            cornerCut.closeSubpath();
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.fillPath(cornerCut, Qt::black);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }

        if (m_border.isValid() && m_border.alpha() > 0) {
            const fluent::painting::DeviceAlignedStroke stroke =
                fluent::painting::DpiPaintMetrics(painter).alignedStroke(QRectF(rect()), 1.0);
            const QPainterPath frame = fluent::overlay::roundedCornerRectPath(stroke.rect, m_radius,
                /*TL*/ true, /*TR*/ false, /*BR*/ false, /*BL*/ false);
            painter.setPen(QPen(m_border, stroke.width));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(frame);
        }
    }

private:
    qreal m_radius = 0.0;
    QColor m_border;
};

// Light-dismiss card backing shown behind the inline pane widgets when opened as a flyout in compact/minimal modes.
class PaneFlyoutOverlay : public QWidget {
public:
    explicit PaneFlyoutOverlay(QWidget* parent) : QWidget(parent)
    {
        // Keep WA_NoSystemBackground without clearing: unpainted areas must preserve the content host
        // already rendered into the shared backing store, whereas WA_TranslucentBackground + Source clear erases it.
        setAttribute(Qt::WA_NoSystemBackground, true);
        // Focusable as a keyboard fallback, while Esc during normal use goes through the app-level event filter.
        setFocusPolicy(Qt::StrongFocus);
    }

    void configure(const QRect& paneRect,
        qreal radius,
        const QColor& fill,
        const QColor& stroke,
        const Elevation::ShadowParams& shadow,
        qreal shadowIntensity)
    {
        m_paneRect = paneRect;
        m_radius = qMax(0.0, radius);
        m_fill = fill;
        m_stroke = stroke;
        m_shadow = shadow;
        m_shadowIntensity = shadowIntensity;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (m_paneRect.isEmpty())
            return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // Shadow casts rightward over the content; the left edge sits flush at the window edge.
        fluent::overlay::paintLayeredShadow(painter, m_paneRect.adjusted(0, 0, -1, -1), m_radius, m_shadow,
            m_shadowIntensity);

        // Paint one unified opaque drawer card while inline panes render transparently via fluentNavPaneFloating,
        // avoiding per-widget backdrop seams across header/main/footer gaps.
        const fluent::painting::DeviceAlignedStroke stroke =
            fluent::painting::DpiPaintMetrics(painter).alignedStroke(QRectF(m_paneRect), 1.0);
        const QPainterPath panel = fluent::overlay::roundedCornerRectPath(stroke.rect, m_radius,
            /*TL*/ false, /*TR*/ true, /*BR*/ true, /*BL*/ false);
        if (m_fill.isValid())
            painter.fillPath(panel, m_fill);
        if (m_stroke.isValid() && m_stroke.alpha() > 0) {
            painter.setPen(QPen(m_stroke, stroke.width));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(panel);
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        // Dismiss only on clicks outside the pane column; absorb clicks in gaps between pane widgets.
        if (fluentMousePos(event).x() > m_paneRect.right())
            dismiss();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Escape) {
            dismiss();
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    void dismiss()
    {
        if (auto* nav = qobject_cast<NavigationView*>(parentWidget()))
            nav->setPaneOpen(false);
    }

    QRect m_paneRect;
    qreal m_radius = 8.0;
    QColor m_fill;
    QColor m_stroke;
    Elevation::ShadowParams m_shadow;
    qreal m_shadowIntensity = 0.3;
};
} // namespace

NavigationView::NavigationView(QWidget* parent)
    : QWidget(parent), m_layoutAnimation(new QPropertyAnimation(this, "layoutTransitionProgress", this)),
      m_contentHost(new StackContentHost(this))
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(minimumSizeHint());
    m_contentHost->setTransitionAnimationEnabled(true);

    connect(m_layoutAnimation, &QPropertyAnimation::finished, this, [this]() { finishLayoutTransition(); });
}

NavigationView::~NavigationView()
{
    for (ChromeRecord* record : {&m_headerChrome, &m_mainChrome, &m_footerChrome}) {
        QObject::disconnect(record->destroyedConnection);
        QWidget* widget = record->widget.data();
        if (!widget)
            continue;
        widget->setProperty("fluentNavPaneFloating", false);
        if (record->ownership == WidgetOwnership::Borrowed) {
            widget->setParent(nullptr);
        } else if (record->ownership == WidgetOwnership::Reparented) {
            widget->setParent(record->originalParent.data());
        }
    }
    m_headerChrome = ChromeRecord();
    m_mainChrome = ChromeRecord();
    m_footerChrome = ChromeRecord();
}

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
    if (headerChromeWidget() == widget)
        return;
    assignChromeWidget(ChromeSlot::Header, widget, WidgetOwnership::Owned, false);
}

bool NavigationView::setHeaderChromeWidget(QWidget* widget, WidgetOwnership ownership)
{
    return assignChromeWidget(ChromeSlot::Header, widget, ownership, true);
}

QWidget* NavigationView::takeHeaderChromeWidget()
{
    return takeChromeWidget(ChromeSlot::Header);
}

bool NavigationView::releaseHeaderChromeWidget()
{
    return releaseChromeWidget(ChromeSlot::Header);
}

void NavigationView::setMainChromeWidget(QWidget* widget)
{
    if (mainChromeWidget() == widget)
        return;
    assignChromeWidget(ChromeSlot::Main, widget, WidgetOwnership::Owned, false);
}

bool NavigationView::setMainChromeWidget(QWidget* widget, WidgetOwnership ownership)
{
    return assignChromeWidget(ChromeSlot::Main, widget, ownership, true);
}

QWidget* NavigationView::takeMainChromeWidget()
{
    return takeChromeWidget(ChromeSlot::Main);
}

bool NavigationView::releaseMainChromeWidget()
{
    return releaseChromeWidget(ChromeSlot::Main);
}

void NavigationView::setFooterChromeWidget(QWidget* widget)
{
    if (footerChromeWidget() == widget)
        return;
    assignChromeWidget(ChromeSlot::Footer, widget, WidgetOwnership::Owned, false);
}

bool NavigationView::setFooterChromeWidget(QWidget* widget, WidgetOwnership ownership)
{
    return assignChromeWidget(ChromeSlot::Footer, widget, ownership, true);
}

QWidget* NavigationView::takeFooterChromeWidget()
{
    return takeChromeWidget(ChromeSlot::Footer);
}

bool NavigationView::releaseFooterChromeWidget()
{
    return releaseChromeWidget(ChromeSlot::Footer);
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
    for (QWidget* widget : {headerChromeWidget(), mainChromeWidget(), footerChromeWidget()}) {
        if (widget)
            widget->update();
    }
    invalidateLayout(false);
}

void NavigationView::paintEvent(QPaintEvent*)
{
    ensureLayout();
    const auto& colors = themeColorsRef();
    const LayoutState visual = currentVisualLayout();

    // Keep the chrome region transparent under composited backdrops; otherwise use the shared solid backdrop.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const bool paintedMaterial = windowing::windowBackdropUsesPaintedMaterial(window());
    const QColor backdrop = paintedMaterial
        ? QColor()
        : windowing::windowChromeBackdropFill(*this, window(),
              compatibility::detail::effectiveWindowActivation(isActiveWindow()));
    if (paintedMaterial) {
        // Window paints the continuous backdrop material across the root; don't cover it here.
    } else if (backdrop.isValid()) {
        painter.fillRect(rect(), backdrop);
        if (!visual.chromeRect.isEmpty())
            painter.fillRect(visual.chromeRect, backdrop);
    } else if (!visual.chromeRect.isEmpty() && window() && window()->testAttribute(Qt::WA_TranslucentBackground)) {
        // Clear only the chrome rect to transparent so macOS vibrancy shows cleanly without stale startup frames;
        // never clear the content-host region since it paints its own opaque surface.
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(visual.chromeRect, Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    // Content surface is painted by StackContentHost so it survives ancestor clears under Mica;
    // Top mode only needs a divider beneath the bar.
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
        update(); // backdrop tracks window focus
    return QWidget::event(event);
}

bool NavigationView::eventFilter(QObject* watched, QEvent* event)
{
    // Application-level Esc handler while the flyout is open, regardless of which child holds focus.
    if (m_paneFlyoutEscFilterInstalled && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape && m_paneOpen) {
            setPaneOpen(false);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void NavigationView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Snap responsive mode changes during live resize so frame-by-frame drag doesn't restart transitions.
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
        return m_paneOpen ? m_expandedPaneWidth : m_compactPaneWidth;
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
    if (m_inResize) // Live-resize mode switches snap; see resizeEvent().
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
    // Use standard accelerate/decelerate easing for short rail transitions.
    m_layoutAnimation->setEasingCurve(animation.standard);
    m_layoutAnimation->setStartValue(0.0);
    m_layoutAnimation->setEndValue(1.0);
    ::fluent::detail::startMotionTransition(m_layoutAnimation, animation.normal, m_animationEnabled);
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
    return interpolatedLayout(m_layoutTransitionFrom, m_layoutTransitionTo, m_layoutTransitionProgress,
        m_layoutTransitionKind);
}

NavigationView::LayoutState NavigationView::interpolatedLayout(const LayoutState& from,
    const LayoutState& to,
    qreal progress,
    LayoutTransitionKind kind) const
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
    const bool hadResolvedLayout = !m_layout.bounds.isEmpty();
    LayoutState next;
    const QRect bounds = rect();
    next.bounds = bounds;
    next.effectiveMode = resolveDisplayMode(bounds.width() > 0 ? bounds.width() : sizeHint().width());

    // Sync paneOpen with adaptive width transitions only after initial show so hidden setup can configure it freely.
    if (hadResolvedLayout && isVisible() && m_displayMode == DisplayMode::Auto
        && next.effectiveMode != m_lastEffectiveDisplayMode) {
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

    // Let an in-flight transition toward the same target finish rather than restarting at progress 0
    // (which can loop when effectiveDisplayModeChanged triggers another relayout).
    const bool sameTargetInFlight = m_layoutTransitionKind != LayoutTransitionKind::None && m_layoutAnimation
        && m_layoutAnimation->state() != QAbstractAnimation::Stopped
        && m_layoutTransitionTo.effectiveMode == next.effectiveMode && m_layoutTransitionTo.bounds == next.bounds
        && m_layoutTransitionTo.chromeRect == next.chromeRect && m_layoutTransitionTo.contentRect == next.contentRect;
    if (sameTargetInFlight) {
        m_layoutTransitionTo = next; // refresh target geometry, keep the running animation
    } else if (animateTransition) {
        startLayoutTransition(previousVisual, m_layout,
            kind == LayoutTransitionKind::None ? LayoutTransitionKind::Side : kind);
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
    const int contentLeft = state.effectiveMode == DisplayMode::Left
        ? chromeWidth
        : (state.effectiveMode == DisplayMode::LeftCompact ? m_compactPaneWidth : 0);
    state.contentRect = QRect(contentLeft, bounds.top(), qMax(0, bounds.width() - contentLeft), bounds.height());
    if (state.effectiveMode == DisplayMode::LeftMinimal && m_paneOpen)
        state.contentRect = bounds;

    if (state.chromeRect.isEmpty())
        return;

    const int headerHeight = qMin(preferredHeight(headerChromeWidget()), state.chromeRect.height());
    const int footerHeight = qMin(preferredHeight(footerChromeWidget()),
        qMax(0, state.chromeRect.height() - headerHeight));
    const int mainHeight = qMax(0, state.chromeRect.height() - headerHeight - footerHeight);

    int y = state.chromeRect.top();
    if (headerChromeWidget())
        state.headerChromeRect = QRect(state.chromeRect.left(), y, state.chromeRect.width(), headerHeight);
    y += headerHeight;
    if (mainChromeWidget())
        state.mainChromeRect = QRect(state.chromeRect.left(), y, state.chromeRect.width(), mainHeight);
    if (footerChromeWidget())
        state.footerChromeRect = QRect(state.chromeRect.left(), state.chromeRect.bottom() + 1 - footerHeight,
            state.chromeRect.width(), footerHeight);
}

void NavigationView::buildTopLayout(LayoutState& state, const QRect& bounds)
{
    state.chromeRect = QRect(bounds.left(), bounds.top(), bounds.width(), qMin(m_topBarHeight, bounds.height()));
    state.contentRect = QRect(bounds.left(), state.chromeRect.bottom() + 1, bounds.width(),
        qMax(0, bounds.height() - state.chromeRect.height()));

    const int headerWidth = qMin(preferredWidth(headerChromeWidget()), state.chromeRect.width());
    const int footerWidth = qMin(preferredWidth(footerChromeWidget()), qMax(0, state.chromeRect.width() - headerWidth));
    const int remainingAfterFixed = qMax(0, state.chromeRect.width() - headerWidth - footerWidth);
    const int mainWidth = qMin(preferredWidth(mainChromeWidget(), remainingAfterFixed), remainingAfterFixed);

    int x = state.chromeRect.left();
    if (headerChromeWidget())
        state.headerChromeRect = QRect(x, state.chromeRect.top(), headerWidth, state.chromeRect.height());
    x += headerWidth;
    if (mainChromeWidget())
        state.mainChromeRect = QRect(x, state.chromeRect.top(), mainWidth, state.chromeRect.height());
    if (footerChromeWidget())
        state.footerChromeRect = QRect(state.chromeRect.right() + 1 - footerWidth, state.chromeRect.top(), footerWidth,
            state.chromeRect.height());
}

void NavigationView::applyChildGeometries()
{
    applyChildGeometries(currentVisualLayout());
}

void NavigationView::applyChildGeometries(const LayoutState& state)
{
    const bool framed = isSideMode(state.effectiveMode) && !state.contentRect.isEmpty() && state.contentRect.left() > 0;
    // Suppress the rounded content frame while the flyout covers the content edge.
    const bool flyout = isPaneFlyoutVisible(state);

    if (m_contentHost) {
        m_contentHost->setGeometry(state.contentRect);
        m_contentHost->setContentSurface(themeColorsRef().bgLayerOverlay, framed ? themeRadius().overlay : 0.0,
            QColor());
        m_contentHost->show();
        m_contentHost->lower();
    }

    // Z-order: content host (bottom) < flyout backing < pane widgets < content frame overlay (top).
    updatePaneFlyout(state);

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

    apply(headerChromeWidget(), state.headerChromeRect);
    apply(mainChromeWidget(), state.mainChromeRect);
    apply(footerChromeWidget(), state.footerChromeRect);

    if (framed && !flyout) {
        if (!m_contentFrameOverlay)
            m_contentFrameOverlay = new ContentFrameOverlay(this);
        m_contentFrameOverlay->setGeometry(state.contentRect);
        QColor frame = themeColorsRef().strokeDivider;
        frame.setAlpha(qMin(28, frame.alpha()));
        static_cast<ContentFrameOverlay*>(m_contentFrameOverlay)->configure(themeRadius().overlay, frame);
        m_contentFrameOverlay->show();
        m_contentFrameOverlay->raise();
    } else if (m_contentFrameOverlay) {
        m_contentFrameOverlay->hide();
    }
}

bool NavigationView::isPaneFlyoutVisible(const LayoutState& state) const
{
    if (state.effectiveMode != DisplayMode::LeftCompact && state.effectiveMode != DisplayMode::LeftMinimal)
        return false;
    // Compare animated rects rather than fixed mode widths so intermediate inline frames don't trigger flyout state.
    return state.chromeRect.intersects(state.contentRect);
}

void NavigationView::updatePaneFlyout(const LayoutState& state)
{
    if (!isPaneFlyoutVisible(state)) {
        if (m_paneFlyoutOverlay)
            m_paneFlyoutOverlay->hide();
        setChromeWidgetsFloating(false);
        setPaneFlyoutEscFilter(false);
        return;
    }

    if (!m_paneFlyoutOverlay)
        m_paneFlyoutOverlay = new PaneFlyoutOverlay(this);
    auto* overlay = static_cast<PaneFlyoutOverlay*>(m_paneFlyoutOverlay);
    overlay->setGeometry(rect());

    setChromeWidgetsFloating(true);
    const auto& colors = themeColorsRef();
    const bool dark = effectiveThemeUsesDarkAppearance();
    QColor fill = colors.bgLayer;
    fill.setAlpha(255);
    // strokeDefault (~5-7% opacity) disappears against matching content tones; use ~14-19% for a clear 1 px edge.
    QColor stroke = dark ? QColor(255, 255, 255, 48) : QColor(0, 0, 0, 36);
    auto shadow = themeShadow(Elevation::Medium);
    shadow.color.setAlpha(dark ? 160 : 100);

    overlay->configure(state.chromeRect, themeRadius().overlay, fill, stroke, shadow, dark ? 0.36 : 0.28);
    overlay->show();
    overlay->raise();
    setPaneFlyoutEscFilter(true);
    // Do not steal focus on flyout open; an ancestor ScrollView would scroll to the overlay.
}

void NavigationView::setPaneFlyoutEscFilter(bool installed)
{
    if (m_paneFlyoutEscFilterInstalled == installed)
        return;
    // Install on qApp rather than window() because Escape KeyPress is delivered only to the focused child widget.
    if (installed)
        qApp->installEventFilter(this);
    else
        qApp->removeEventFilter(this);
    m_paneFlyoutEscFilterInstalled = installed;
}

void NavigationView::setChromeWidgetsFloating(bool floating)
{
    // Opt-in pane widgets observe "fluentNavPaneFloating" to suppress their own fill while floating over the flyout card.
    for (QWidget* widget : {headerChromeWidget(), mainChromeWidget(), footerChromeWidget()}) {
        if (widget)
            widget->setProperty("fluentNavPaneFloating", floating);
    }
}

bool NavigationView::canHostChromeWidget(ChromeSlot slot, QWidget* widget) const
{
    if (!widget)
        return true;
    if (widget == this || widget->isAncestorOf(this) || widget == m_contentHost || widget == m_contentFrameOverlay
        || widget == m_paneFlyoutOverlay) {
        return false;
    }

    for (ChromeSlot candidate : {ChromeSlot::Header, ChromeSlot::Main, ChromeSlot::Footer}) {
        if (candidate != slot && chromeRecord(candidate).identity == widget)
            return false;
    }
    return true;
}

bool NavigationView::assignChromeWidget(ChromeSlot slot,
    QWidget* widget,
    WidgetOwnership ownership,
    bool applyPreviousOwnership)
{
    ChromeRecord& destination = chromeRecord(slot);
    if (destination.identity == widget) {
        if (widget)
            destination.ownership = ownership;
        return true;
    }
    if (!canHostChromeWidget(slot, widget))
        return false;

    ChromeRecord previous = destination;
    QObject::disconnect(previous.destroyedConnection);
    destination = ChromeRecord();

    if (widget) {
        destination.widget = widget;
        destination.identity = widget;
        destination.originalParent = widget->parentWidget();
        destination.ownership = ownership;
        destination.destroyedConnection = connect(widget, &QObject::destroyed, this, [this, slot, widget]() {
            handleChromeWidgetDestroyed(slot, widget);
        });
        widget->setParent(this);
        widget->show();
    }

    if (applyPreviousOwnership)
        releaseChromeRecord(previous);
    else
        transferChromeRecord(previous);

    invalidateLayout(false);
    notifyChromeWidgetChanged(slot);
    return true;
}

QWidget* NavigationView::takeChromeWidget(ChromeSlot slot)
{
    ChromeRecord& source = chromeRecord(slot);
    if (!source.identity)
        return nullptr;
    ChromeRecord previous = source;
    QObject::disconnect(previous.destroyedConnection);
    source = ChromeRecord();
    QWidget* widget = transferChromeRecord(previous);
    invalidateLayout(false);
    notifyChromeWidgetChanged(slot);
    return widget;
}

bool NavigationView::releaseChromeWidget(ChromeSlot slot)
{
    ChromeRecord& source = chromeRecord(slot);
    if (!source.identity)
        return false;
    ChromeRecord previous = source;
    QObject::disconnect(previous.destroyedConnection);
    source = ChromeRecord();
    releaseChromeRecord(previous);
    invalidateLayout(false);
    notifyChromeWidgetChanged(slot);
    return true;
}

void NavigationView::releaseChromeRecord(const ChromeRecord& record)
{
    QWidget* widget = record.widget.data();
    if (!widget)
        return;
    widget->setProperty("fluentNavPaneFloating", false);
    widget->hide();
    if (record.ownership == WidgetOwnership::Owned) {
        delete widget;
    } else if (record.ownership == WidgetOwnership::Reparented) {
        widget->setParent(record.originalParent.data());
    } else {
        widget->setParent(nullptr);
    }
}

QWidget* NavigationView::transferChromeRecord(const ChromeRecord& record)
{
    QWidget* widget = record.widget.data();
    if (!widget)
        return nullptr;
    widget->setProperty("fluentNavPaneFloating", false);
    widget->hide();
    widget->setParent(nullptr);
    return widget;
}

void NavigationView::handleChromeWidgetDestroyed(ChromeSlot slot, QWidget* widget)
{
    ChromeRecord& record = chromeRecord(slot);
    if (record.identity != widget)
        return;
    record = ChromeRecord();
    invalidateLayout(false);
    notifyChromeWidgetChanged(slot);
}

void NavigationView::notifyChromeWidgetChanged(ChromeSlot slot)
{
    switch (slot) {
    case ChromeSlot::Header:
        emit headerChromeWidgetChanged(headerChromeWidget());
        break;
    case ChromeSlot::Main:
        emit mainChromeWidgetChanged(mainChromeWidget());
        break;
    case ChromeSlot::Footer:
        emit footerChromeWidgetChanged(footerChromeWidget());
        break;
    }
}

NavigationView::ChromeRecord& NavigationView::chromeRecord(ChromeSlot slot)
{
    switch (slot) {
    case ChromeSlot::Header:
        return m_headerChrome;
    case ChromeSlot::Main:
        return m_mainChrome;
    case ChromeSlot::Footer:
        return m_footerChrome;
    }
    return m_headerChrome;
}

const NavigationView::ChromeRecord& NavigationView::chromeRecord(ChromeSlot slot) const
{
    switch (slot) {
    case ChromeSlot::Header:
        return m_headerChrome;
    case ChromeSlot::Main:
        return m_mainChrome;
    case ChromeSlot::Footer:
        return m_footerChrome;
    }
    return m_headerChrome;
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
