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
#include "components/navigation/StackContentHost.h"
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

        // Carve the top-left corner (outside the rounded arc) to transparent so the OS backdrop shows
        // through the rounded notch — but ONLY under a real backdrop (Mica/Acrylic). On Win11 the
        // top-level is ALWAYS translucent, so in Normal a CompositionMode_Clear here writes (0,0,0,0)
        // onto the opaque chrome backing, rendering a stray block at the corner (black in dark, white
        // in light) instead of the chrome color. Gate on the fluentMicaBackdrop paint-hint, never bare
        // WA_TranslucentBackground — same rule as the TreeView / nav-pane seams.
        // zh_CN: 把左上角（圆弧之外）挖透明，让系统背景透出圆角缺口——但仅在真实背景（Mica/Acrylic）下。Win11 顶层
        // 始终半透明，故 Normal 下这里的 Clear 会在不透明 chrome 背板上写 (0,0,0,0)，在角上渲染出杂块（dark 黑 / light 白）
        // 而非 chrome 色。按 fluentMicaBackdrop paint-hint 门控，绝不用裸的 WA_TranslucentBackground——与 TreeView/导航栏缝同一规则。
        const bool realBackdrop = window()
            && window()->testAttribute(Qt::WA_TranslucentBackground)
            && window()->property("fluentMicaBackdrop").toBool();
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

// Floating backing for the pane when it opens as a light-dismiss FLYOUT (the compact / minimal
// modes). It paints a single rounded surface + soft shadow behind the (transparent) nav panes so
// the opened pane reads as a card lifted over the content, and dismisses the pane on an outside
// click. This folds the DrawerView capability into NavigationView: the SAME header/main/footer
// pane widgets are raised on top of this backing — no duplicate "drawer" panes to keep in sync.
// zh_CN: 窗格以轻关闭浮层（紧凑/最小模式）打开时的浮动底。它在（透明的）导航窗格之后绘制单一圆角表面+柔和阴影，
// 使打开的窗格像浮在内容之上的卡片，并在窗格外点击时关闭。这把 DrawerView 能力收进 NavigationView：同一组
// header/main/footer 窗格控件被叠在此底之上——无需再维护一份同步的“抽屉”窗格。
class PaneFlyoutOverlay : public QWidget {
public:
    explicit PaneFlyoutOverlay(QWidget* parent)
        : QWidget(parent)
    {
        // Like ContentFrameOverlay: WA_NoSystemBackground and DON'T clear our backing — the content
        // we don't paint keeps showing the content host beneath (which already painted into the
        // shared backing). A WA_TranslucentBackground + Source-clear would instead ERASE the content
        // host's pixels and blank the content area. zh_CN: 与 ContentFrameOverlay 一致：WA_NoSystemBackground
        // 且不清背景——未绘制处保留已绘制到共享后备缓冲的内容宿主。若用 WA_TranslucentBackground + Source 清除，
        // 反而会擦掉内容宿主像素、让内容区变空白。
        setAttribute(Qt::WA_NoSystemBackground, true);
        // Focusable so opening the flyout parks keyboard focus here: without a focused widget Qt
        // dispatches no KeyPress, and the Esc light-dismiss filter would never fire.
        // zh_CN: 可获焦，使打开浮层时键盘焦点停在此处：没有获焦控件时 Qt 不会派发 KeyPress，Esc 轻关闭过滤器永远不触发。
        setFocusPolicy(Qt::StrongFocus);
    }

    void configure(const QRect& paneRect, qreal radius, const QColor& fill,
                   const QColor& stroke, const Elevation::ShadowParams& shadow, qreal shadowIntensity)
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

        // Soft shadow grows from the pane column onto the content to its right (the left/edge sits
        // at the window border and is naturally clipped). zh_CN: 柔和阴影从窗格列向右扩散到内容上
        //（左/边缘贴着窗口边界，自然被裁剪）。
        fluent::overlay::paintLayeredShadow(painter, m_paneRect.adjusted(0, 0, -1, -1), m_radius,
                                            m_shadow, m_shadowIntensity);

        // A single cohesive elevated card: an opaque bgLayer fill, a clearly visible border, and the
        // top-right / bottom-right corners rounded (the left edge stays flush to the window border).
        // This is the native WinUI overlay-pane look — a DISTINCT floating drawer that reads as lifted
        // ABOVE the content / backdrop, NOT blended into it. An earlier version cleared the column to
        // the live backdrop instead; the inline panes' sub-regions (header padding, footer gap, the
        // TreeView viewport) then each composited their own way → visible material seams ("奇怪的图层").
        // The inline panes drawn on top are now switched into their transparent "surface" mode
        // (fluentNavPaneFloating), so this one card shows through them uniformly with no seam.
        // zh_CN: 单一、整体的抬升卡片：不透明 bgLayer 填充、清晰可见的边框、右上/右下圆角（左缘贴窗口边界）。
        // 这就是原生 WinUI 浮层窗格的样子——一张明确浮在内容/背景「之上」的抽屉卡片，而非融进背景。早先版本改为把整列清成
        // 实时背景，结果内联窗格各子区域（头部留白、页脚间隙、TreeView 视口）各自合成 → 出现可见的材质拼缝（“奇怪的图层”）。
        // 现在其上绘制的内联窗格被切到透明的「surface」模式（fluentNavPaneFloating），故这张卡片均匀透出、无缝。
        const QPainterPath panel = fluent::overlay::roundedCornerRectPath(
            QRectF(m_paneRect).adjusted(0.0, 0.0, -0.5, -0.5), m_radius,
            /*TL*/ false, /*TR*/ true, /*BR*/ true, /*BL*/ false);
        if (m_fill.isValid())
            painter.fillPath(panel, m_fill);
        if (m_stroke.isValid() && m_stroke.alpha() > 0) {
            painter.setPen(QPen(m_stroke, 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(panel);
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        // Light dismiss: a press in the content area (right of the pane column) closes the pane.
        // Presses inside the column (gaps between the raised header/main/footer panes) are absorbed.
        // zh_CN: 轻关闭：在内容区（窗格列右侧）按下即关闭窗格；列内（被抬升的 header/main/footer 间隙）的按下被吸收。
        if (event->position().x() > m_paneRect.right())
            dismiss();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        // Esc light-dismiss when the flyout holds focus. zh_CN: 浮层持有焦点时 Esc 轻关闭。
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
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    // chromeBackdropFill() is the single backdrop decision shared with the title bar: a valid color
    // is the solid fallback; an invalid color means a real OS backdrop is in play (paint transparent).
    // zh_CN: chromeBackdropFill() 是与标题栏共用的单一背景决策：有效色为纯色回退；无效色表示有真实系统背景（画透明）。
    const QColor backdrop = chromeBackdropFill(window(), isActiveWindow());
    if (backdrop.isValid()) {
        painter.fillRect(rect(), backdrop);
        if (!visual.chromeRect.isEmpty())
            painter.fillRect(visual.chromeRect, backdrop);
    } else if (!visual.chromeRect.isEmpty()
               && window() && window()->testAttribute(Qt::WA_TranslucentBackground)) {
        // Erase the chrome (pane) region to transparent each paint so the OS backdrop shows
        // cleanly. Under a translucent top-level macOS doesn't auto-clear the backing, so a frame
        // painted before the native vibrancy had composited (a startup race) would otherwise
        // persist as a bright tint-only seam at the pane's top edge. We clear only the chrome
        // rect, never the content-host region (it paints its own opaque surface).
        // zh_CN: 每帧把 chrome（窗格）区域擦成透明，让系统背景干净露出。半透明顶层下 macOS 不会自动清除后备
        // 缓冲，故在原生 vibrancy 合成前绘制的帧（启动竞态）会在窗格顶边残留一条仅含 tint 的亮缝。只清 chrome
        // 矩形，绝不碰内容宿主区域（它自绘不透明表面）。
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(visual.chromeRect, Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
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

bool NavigationView::eventFilter(QObject* watched, QEvent* event)
{
    // Esc light-dismiss for the pane flyout. The filter is installed on the top-level window only
    // while a flyout is showing, so Escape closes the pane regardless of which child has focus
    // (search box, page, etc.). zh_CN: 窗格浮层的 Esc 轻关闭。该过滤器仅在浮层显示期间装在顶层窗口上，
    // 故无论焦点在哪个子控件（搜索框、页面等），Escape 都能关闭窗格。
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
    const bool hadResolvedLayout = !m_layout.bounds.isEmpty();
    LayoutState next;
    const QRect bounds = rect();
    next.bounds = bounds;
    next.effectiveMode = resolveDisplayMode(bounds.width() > 0 ? bounds.width() : sizeHint().width());

    // Auto-managed pane open state (matches WinUI) after the control is visible: when
    // adaptive layout drops to compact or minimal, collapse the pane; when it returns to
    // expanded, open it. Hidden construction/layout passes must not rewrite the public
    // paneOpen state before callers can observe or configure it.
    // zh_CN: 控件可见后的自适应窗格开合（对齐 WinUI）：降到紧凑或最小模式时收起窗格，回到展开
    // 模式时打开。隐藏构造/布局阶段不能在调用方观察或配置前改写公开的 paneOpen 状态。
    if (hadResolvedLayout
        && isVisible()
        && m_displayMode == DisplayMode::Auto
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
    // A pane to the content's left means a framed side mode: the content gets a rounded top-left
    // corner + border. zh_CN: 内容左侧有窗格即带框侧边模式：内容获得左上圆角 + 边框。
    const bool framed = isSideMode(state.effectiveMode)
                        && !state.contentRect.isEmpty()
                        && state.contentRect.left() > 0;
    // When the pane opens as a flyout over the content (compact / minimal), the content's rounded
    // frame would peek out from under the floating pane — so suppress it for the flyout.
    // zh_CN: 窗格以浮层覆盖内容（紧凑/最小）时，内容圆角框会从浮动窗格下露出——故浮层期间抑制它。
    const bool flyout = isPaneFlyoutVisible(state);

    if (m_contentHost) {
        m_contentHost->setGeometry(state.contentRect);
        // Provide the opaque content-layer surface (bgLayerAlt) the host paints when NO translucent
        // system backdrop is active (Normal / unsupported platforms); under Mica/Acrylic the host
        // paints nothing so the backdrop shows through the transparent pages. The host gates this on
        // the window backdrop at paint time. zh_CN: 提供宿主在无半透明系统背景（Normal / 不支持的平台）时
        // 绘制的不透明内容层表面（bgLayerAlt）；Mica/Acrylic 下宿主不绘制，经透明页面透出系统背景。宿主在绘制时
        // 依据窗口背景判断。
        m_contentHost->setContentSurface(themeColors().bgLayerAlt,
                                         framed ? themeRadius().overlay : 0.0,
                                         QColor());
        m_contentHost->show();
        m_contentHost->lower();
    }

    // Floating pane backing sits just above the content (and below the panes raised next), so the
    // opened pane reads as a flyout over the content. zh_CN: 浮动窗格底位于内容之上（在随后抬升的窗格之下），
    // 使打开的窗格表现为浮在内容之上的浮层。
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

    apply(m_headerChromeWidget, state.headerChromeRect);
    apply(m_mainChromeWidget, state.mainChromeRect);
    apply(m_footerChromeWidget, state.footerChromeRect);

    // Content frame: a rounded top-left corner + border drawn on top of the content, but only in the
    // framed side modes — and not while a pane flyout covers it.
    // zh_CN: 内容框：内容之上绘制左上圆角 + 边框，仅带框侧边模式——且窗格浮层覆盖时不画。
    if (framed && !flyout) {
        if (!m_contentFrameOverlay)
            m_contentFrameOverlay = new ContentFrameOverlay(this);
        m_contentFrameOverlay->setGeometry(state.contentRect);
        // Match WinUI Gallery's subtle content frame without adding layout spacing.
        QColor frame = themeColors().strokeDivider;
        frame.setAlpha(qMin(28, frame.alpha()));
        static_cast<ContentFrameOverlay*>(m_contentFrameOverlay)
            ->configure(themeRadius().overlay, frame);
        m_contentFrameOverlay->show();
        m_contentFrameOverlay->raise();
    } else if (m_contentFrameOverlay) {
        m_contentFrameOverlay->hide();
    }
}

bool NavigationView::isPaneFlyoutVisible(const LayoutState& state) const
{
    if (state.effectiveMode != DisplayMode::LeftCompact
        && state.effectiveMode != DisplayMode::LeftMinimal)
        return false;
    // The pane is a flyout once it has opened past its inline rail (compact rail width, or 0 for
    // minimal) — true at rest when open AND throughout the slide animation.
    // zh_CN: 窗格一旦展开超过其内联栏（紧凑栏宽，或最小模式的 0）即为浮层——静止打开时与整个滑动动画期间皆为真。
    const int rail = state.effectiveMode == DisplayMode::LeftCompact ? m_compactPaneWidth : 0;
    return state.chromeRect.width() > rail;
}

void NavigationView::updatePaneFlyout(const LayoutState& state)
{
    if (!isPaneFlyoutVisible(state)) {
        if (m_paneFlyoutOverlay)
            m_paneFlyoutOverlay->hide();
        // Back to the inline rail / expanded pane: the panes paint their own chrome background again.
        // zh_CN: 回到内联栏/展开窗格：窗格重新绘制自身 chrome 背景。
        setChromeWidgetsFloating(false);
        setPaneFlyoutEscFilter(false);
        return;
    }

    if (!m_paneFlyoutOverlay)
        m_paneFlyoutOverlay = new PaneFlyoutOverlay(this);
    auto* overlay = static_cast<PaneFlyoutOverlay*>(m_paneFlyoutOverlay);
    overlay->setGeometry(rect());

    // The flyout is a DISTINCT elevated drawer card (native WinUI overlay-pane look): an opaque
    // bgLayer surface, a clearly visible border, top-right / bottom-right rounded corners and a soft
    // shadow — it reads as lifted ABOVE the content / backdrop rather than blended into it. The inline
    // panes are switched into their transparent "surface" mode (setChromeWidgetsFloating) so this one
    // card shows through them uniformly, with no per-region material seam.
    // zh_CN: 浮层是一张明确的抬升抽屉卡片（原生 WinUI 浮层窗格的样子）：不透明 bgLayer 表面、清晰可见的边框、
    // 右上/右下圆角与柔和阴影——读作浮在内容/背景「之上」，而非融入。内联窗格被切到透明的「surface」模式
    //（setChromeWidgetsFloating），使这张卡片均匀透出、各区域无材质拼缝。
    setChromeWidgetsFloating(true);
    const auto colors = themeColors();
    const bool dark = currentTheme() == Dark;
    QColor fill = colors.bgLayer;
    fill.setAlpha(255);
    // A clearly visible drawer border (the user wants "一个比较明显的边框色"). The theme strokeDefault is
    // only ~7% white in dark / ~5% black in light — too faint to read as the card edge against a same-
    // toned content area — so use a stronger explicit hairline (~19% white / ~14% black) that still
    // stays a tasteful 1px rule, not a harsh line. zh_CN: 一道清晰可见的抽屉边框（用户要「一个比较明显的边框色」）。
    // 主题 strokeDefault 在暗色仅约 7% 白 / 亮色约 5% 黑——在同色调内容旁太淡、读不出卡片边缘——故用更强的显式细线
    //（约 19% 白 / 14% 黑），仍保持得体的 1px 描边，而非生硬线条。
    QColor stroke = dark ? QColor(255, 255, 255, 48) : QColor(0, 0, 0, 36);
    auto shadow = themeShadow(Elevation::Medium);
    shadow.color.setAlpha(dark ? 160 : 100);

    overlay->configure(state.chromeRect, themeRadius().overlay, fill, stroke, shadow,
                       dark ? 0.36 : 0.28);
    overlay->show();
    overlay->raise();
    setPaneFlyoutEscFilter(true);
    // Park keyboard focus on the flyout so Esc (handled in its keyPressEvent) works; without a
    // focused widget Qt dispatches no key events. updatePaneFlyout only runs on layout changes, so
    // this does not fight the user's focus while the flyout simply sits open.
    // zh_CN: 把键盘焦点停在浮层上，使 Esc（在其 keyPressEvent 处理）生效；没有获焦控件时 Qt 不派发键事件。
    // updatePaneFlyout 仅在布局变化时运行，故浮层静止打开期间不会与用户焦点相争。
    if (!overlay->hasFocus())
        overlay->setFocus(Qt::OtherFocusReason);
}

void NavigationView::setPaneFlyoutEscFilter(bool installed)
{
    if (m_paneFlyoutEscFilterInstalled == installed)
        return;
    // Filter on the application, not window(): an Escape KeyPress targets the focused child widget
    // (search box / a page control), which a window-level filter never sees. zh_CN: 装在应用上而非
    // window()：Escape 的 KeyPress 投递给获得焦点的子控件（搜索框/页面控件），窗口级过滤器看不到它。
    if (installed)
        qApp->installEventFilter(this);
    else
        qApp->removeEventFilter(this);
    m_paneFlyoutEscFilterInstalled = installed;
}

void NavigationView::setChromeWidgetsFloating(bool floating)
{
    // Hint each chrome (pane) widget whether it is currently floating inside the overlay flyout. A
    // pane that opts in (e.g. by observing the "fluentNavPaneFloating" dynamic property) drops its own
    // chrome background so the overlay's single elevated card shows through it without a seam; in the
    // rail / inline modes (floating == false) it paints its normal chrome background again. The hint
    // is generic — NavigationView stays decoupled from any concrete pane type.
    // zh_CN: 向各 chrome（窗格）控件提示当前是否浮在浮层抽屉中。选择响应的窗格（如观察 fluentNavPaneFloating
    // 动态属性者）会放弃自身 chrome 背景，让浮层那张抬升卡片无缝透出；栏/内联模式（floating==false）下重新绘制
    // 正常 chrome 背景。该提示是通用的——NavigationView 不与任何具体窗格类型耦合。
    for (QWidget* widget : {m_headerChromeWidget.data(), m_mainChromeWidget.data(),
                            m_footerChromeWidget.data()}) {
        if (widget)
            widget->setProperty("fluentNavPaneFloating", floating);
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
