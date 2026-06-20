#include "GalleryWindow.h"

#include <algorithm>

#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRegion>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

#include "components/collections/DrawerView.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/StackContentHost.h"
#include "components/windowing/TitleBar.h"
#include "utils/Log.h"
#include "AppIcon.h"
#include "GalleryContentPresenter.h"
#include "GalleryNavigationPane.h"
#include "GallerySearchRanking.h"
#include "GallerySplashScreen.h"
#include "GalleryTitleBarController.h"
#include "GalleryWindowMetrics.h"
#include "view/pages/GalleryContentPage.h"
#include "view/pages/PlaceholderPage.h"
#include "view/pages/SettingsPage.h"

namespace fluent::gallery {
namespace {

using AppWindowMetrics = metrics::AppWindow;
using NavigationMetrics = metrics::Navigation;
using TitleBarMetrics = metrics::TitleBar;

constexpr int kNavigationDrawerShadowMargin = 8;

// Brief hold after the initial route is on screen before dismissing the splash: long enough
// for the window to composite a few frames (so DWM applies Mica), short enough to feel instant.
// zh_CN: 首个路由上屏后到消除 splash 之间的短暂停留：足够窗口合成几帧（使 DWM 施加 Mica），又短到感觉即时。
constexpr int kStartupSplashHoldMs = 120;

class NavigationDrawerContentPanel : public QWidget, public fluent::FluentElement {
public:
    explicit NavigationDrawerContentPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAutoFillBackground(false);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        updateClipMask();
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        const int shadowMargin = qMin(kNavigationDrawerShadowMargin, qMax(0, width() / 4));
        const QRectF panelRect(0.0, 0.0,
                               qMax(0, width() - shadowMargin) - 0.5,
                               height() - 0.5);
        if (panelRect.isEmpty())
            return;
        const QPainterPath panelPath = drawerPanelPath(panelRect);
        const auto colors = themeColors();

        auto shadow = themeShadow(Elevation::Medium);
        shadow.color.setAlpha(currentTheme() == Dark ? 150 : 92);
        fluent::overlay::paintLayeredShadow(painter,
                                            panelRect.toAlignedRect().adjusted(0, 0, -2, -1),
                                            themeRadius().overlay,
                                            shadow,
                                            currentTheme() == Dark ? 0.34 : 0.26);

        QColor base = colors.bgLayer;
        base.setAlpha(currentTheme() == Dark ? 240 : 226);
        painter.fillPath(panelPath, base);

        QLinearGradient wash(panelRect.topLeft(), panelRect.bottomRight());
        wash.setColorAt(0.0, QColor(255, 255, 255, currentTheme() == Dark ? 14 : 92));
        wash.setColorAt(0.38, QColor(246, 250, 255, currentTheme() == Dark ? 9 : 52));
        wash.setColorAt(1.0, QColor(232, 250, 247, currentTheme() == Dark ? 8 : 42));
        painter.fillPath(panelPath, wash);

        QRadialGradient glow(QPointF(panelRect.width() * 0.18, panelRect.height() * 0.22),
                             qMax(panelRect.width(), panelRect.height()) * 0.72);
        glow.setColorAt(0.0, QColor(255, 255, 255, currentTheme() == Dark ? 16 : 84));
        glow.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.fillPath(panelPath, glow);

        painter.save();
        painter.setClipPath(panelPath);
        QLinearGradient edgeWash(panelRect.right() - 20.0, 0.0, panelRect.right(), 0.0);
        edgeWash.setColorAt(0.0, QColor(0, 0, 0, 0));
        edgeWash.setColorAt(1.0, QColor(0, 0, 0, currentTheme() == Dark ? 38 : 16));
        painter.fillRect(QRectF(panelRect.right() - 20.0, panelRect.top(), 20.0, panelRect.height()),
                         edgeWash);
        painter.restore();

        QColor stroke = colors.strokeSurface;
        stroke.setAlpha(currentTheme() == Dark ? 92 : 64);
        painter.setPen(QPen(stroke, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(panelPath);
    }

private:
    QPainterPath drawerPanelPath(const QRectF& bounds) const
    {
        return fluent::overlay::roundedCornerRectPath(bounds, themeRadius().overlay,
                                                      /*TL*/ false, /*TR*/ true,
                                                      /*BR*/ true, /*BL*/ false);
    }

    void updateClipMask()
    {
        if (rect().isEmpty()) {
            clearMask();
            return;
        }
        const int shadowMargin = qMin(kNavigationDrawerShadowMargin, qMax(0, width() / 4));
        const QRectF panelRect(0.0, 0.0,
                               qMax(0, width() - shadowMargin),
                               height());
        QRegion region(drawerPanelPath(panelRect).toFillPolygon().toPolygon());
        if (shadowMargin > 0)
            region += QRegion(qMax(0, width() - shadowMargin), 0, shadowMargin, height());
        setMask(region);
    }
};

} // namespace

GalleryWindow::GalleryWindow(QWidget* parent)
    : fluent::windowing::Window(parent)
    , m_navigationState(this)
{
    setObjectName(QStringLiteral("galleryWindow"));
    setWindowTitle(QStringLiteral("WinUI 3 Gallery"));
    setWindowIcon(appicon::icon());
    // Allow narrow windows so the adaptive nav can collapse to its compact / minimal
    // modes; a 980 floor would pin the layout above the 640 breakpoint.
    // zh_CN: 允许窄窗口，让自适应导航能进入紧凑/最小模式；980 的下限会把布局钉在 640 断点之上。
    setMinimumSize(AppWindowMetrics::MinWidth, AppWindowMetrics::MinHeight);

    createTitleBarContent();
    buildNavigationShell();
    buildContentPresenter();
    installSplashScreen();
    showInitialRouteContent();
    prewarmRemainingRoutes();
    LOG_INFO(QStringLiteral("GalleryWindow constructed defaultRoute=%1")
                 .arg(m_navigationViewModel.defaultRouteId()));
}

QString GalleryWindow::currentRouteId() const
{
    return m_contentPresenter ? m_contentPresenter->currentRouteId() : QString();
}

QStringList GalleryWindow::navigationEntryIds() const
{
    return m_navigationViewModel.navigationEntryIds();
}

QStringList GalleryWindow::visibleNavigationTitles() const
{
    return m_navigationViewModel.visibleTitles();
}

bool GalleryWindow::selectRoute(const QString& routeId)
{
    if (!m_navigationView) {
        LOG_WARN(QStringLiteral("GalleryWindow selectRoute rejected routeId=%1 reason=missing-navigation-view")
                     .arg(routeId));
        return false;
    }

    if (!m_navigationViewModel.itemById(routeId)) {
        LOG_WARN(QStringLiteral("GalleryWindow selectRoute rejected routeId=%1 reason=missing-route")
                     .arg(routeId));
        return false;
    }

    if (m_navigationState.selectedRouteId() != routeId) {
        LOG_DEBUG(QStringLiteral("GalleryWindow selectRoute routeId=%1 state=change")
                      .arg(routeId));
        m_navigationState.setSelectedRouteId(routeId);
        return true;
    }

    LOG_TRACE(QStringLiteral("GalleryWindow selectRoute routeId=%1 state=show-current")
                  .arg(routeId));
    return m_contentPresenter && m_contentPresenter->presentRoute(routeId);
}

bool GalleryWindow::navigateToSearchResult(const QString& searchText)
{
    const QString needle = searchText.trimmed();
    if (needle.isEmpty())
        return false;

    const QStringList tokens = search::splitTokens(needle);

    // Rank candidates the same way the suggestion dropdown does so pressing Enter
    // lands on its top result: exact title (0) > prefix (1) > any token subset (2),
    // ties broken alphabetically (case-insensitive).
    // zh_CN: 候选排序与建议下拉一致，使回车落到其首个结果：精确标题(0) > 前缀(1) >
    // 任意词元子集(2)，同档按字母序（忽略大小写）。
    QString bestRouteId;
    QString bestTitle;
    int bestScore = 3;
    for (const GalleryNavigationItem& item : m_navigationViewModel.items()) {
        if (item.kind == GalleryNavigationItem::Kind::SectionHeader)
            continue;
        if (!search::titleMatchesTokens(item.title, tokens))
            continue;

        int score = 2;
        if (item.title.compare(needle, Qt::CaseInsensitive) == 0)
            score = 0;
        else if (item.title.startsWith(needle, Qt::CaseInsensitive))
            score = 1;

        if (score < bestScore
            || (score == bestScore && item.title.compare(bestTitle, Qt::CaseInsensitive) < 0)) {
            bestScore = score;
            bestRouteId = item.id;
            bestTitle = item.title;
        }
        if (bestScore == 0)
            break;
    }

    if (bestRouteId.isEmpty()) {
        LOG_DEBUG(QStringLiteral("GalleryWindow searchNavigate miss text=%1").arg(needle));
        return false;
    }

    LOG_DEBUG(QStringLiteral("GalleryWindow searchNavigate text=%1 routeId=%2 match=%3")
                  .arg(needle, bestRouteId,
                       bestScore == 0 ? QStringLiteral("exact")
                                      : (bestScore == 1 ? QStringLiteral("prefix")
                                                        : QStringLiteral("token"))));
    return selectRoute(bestRouteId);
}

void GalleryWindow::showInitialRouteContent()
{
    selectRoute(m_navigationViewModel.defaultRouteId());
}

void GalleryWindow::installSplashScreen()
{
    if (!m_contentPresenter)
        return;

    // While loading we show only the native window controls (min/max/close): the
    // splash owns the screen, so the back/menu buttons, app title and search box would
    // just be inert chrome. They come back when the splash dismisses.
    // zh_CN: 加载期间只保留原生窗口按钮（最小化/最大化/关闭）：splash 占据画面，返回/菜单按钮、
    // 应用标题与搜索框只是无用装饰，故隐藏；splash 关闭时再恢复。
    if (m_titleBar)
        m_titleBar->setChromeVisible(false);

    // Cover the content area with the splash from the very first frame; the real title
    // bar stays live above it. zh_CN: 从第一帧起用 splash 盖住内容区；真实标题栏仍在其上方可用。
    m_splashScreen = new GallerySplashScreen(contentHost());
    m_splashScreen->show();
    m_splashScreen->raise();

    // Dismiss the splash as soon as the initial route (Home) is built and on screen — no
    // Dismiss the splash once splash-phase prewarm finishes — either the time budget elapsed or
    // the queue drained. Pages warmed in time become instant; the un-warmed tail builds lazily
    // behind a shimmer skeleton on first visit. zh_CN: splash 期预热一结束（时间预算到点或队列排空）就消除
    // splash。及时预热的页瞬时显示；没预热到的尾部在首次访问时于 shimmer 骨架屏背后懒构建。
    connect(m_contentPresenter, &GalleryContentPresenter::prewarmProgress,
            this, [this](int done, int total) {
                if (m_splashScreen)
                    m_splashScreen->setProgress(done, total);
            });
    connect(m_contentPresenter, &GalleryContentPresenter::prewarmFinished,
            this, [this]() {
                // Let the window composite a few frames before finishing, so DWM has applied
                // Mica and reapplySystemBackdrop() reinforces it past the first-show race.
                // zh_CN: 收尾前先让窗口合成几帧，使 DWM 施加 Mica，reapplySystemBackdrop() 再强化一遍，越过首屏竞争。
                QTimer::singleShot(kStartupSplashHoldMs, this, [this]() { finishStartup(); });
            });
}

void GalleryWindow::prewarmRemainingRoutes()
{
    if (!m_contentPresenter)
        return;

    // Warm the nav routes (in order) behind the splash, time-budgeted by the presenter. Home is
    // already built; common top pages warm next so their first visit is instant, and the splash
    // hides the build jank. zh_CN: 在 splash 背后按顺序预热导航路由，由 presenter 做时间预算。Home 已建好；
    // 靠前的常用页接着预热，使其首次访问瞬时，且 splash 遮挡建页卡顿。
    QStringList routeIds;
    if (m_mainNavigationPane)
        routeIds += m_mainNavigationPane->routeIds();
    if (m_footerNavigationPane)
        routeIds += m_footerNavigationPane->routeIds();
    m_contentPresenter->prewarmRoutes(routeIds);
}

void GalleryWindow::finishStartup()
{
    if (m_titleBar)
        m_titleBar->setChromeVisible(true, /*animated*/ true);
    // The window has composited several frames, so we're past the DWM first-show race that can
    // leave Mica un-applied (a flat neutral surface until the next activation). Force the
    // backdrop on now, as the content surfaces from behind the splash.
    // zh_CN: 窗口已合成数帧，已过那个可能让 Mica 未生效（停在扁平中性表面、要等下次激活）的 DWM 首屏竞争；
    // 在内容从 splash 后浮现时强制施加背景。
    reapplySystemBackdrop();
    if (m_splashScreen)
        m_splashScreen->dismiss();  // fades out, then self-deletes
}

GalleryContentPage* GalleryWindow::currentContentPage() const
{
    return m_contentPresenter
        ? dynamic_cast<GalleryContentPage*>(m_contentPresenter->currentPage())
        : nullptr;
}

PlaceholderPage* GalleryWindow::currentPlaceholderPage() const
{
    return m_contentPresenter
        ? dynamic_cast<PlaceholderPage*>(m_contentPresenter->currentPage())
        : nullptr;
}

SettingsPage* GalleryWindow::currentSettingsPage() const
{
    return m_contentPresenter
        ? dynamic_cast<SettingsPage*>(m_contentPresenter->currentPage())
        : nullptr;
}

void GalleryWindow::buildNavigationShell()
{
    m_navigationView = new fluent::navigation::NavigationView(this);
    m_navigationView->setObjectName(QStringLiteral("galleryNavigationView"));
    // Auto adapts the pane to the window width, like WinUI Gallery: >=1008 expanded with
    // labels, >=640 compact icon rail, and below that fully hidden (LeftMinimal) behind the
    // title-bar menu button.
    // zh_CN: Auto 让窗格随窗口宽度自适应（对齐 WinUI Gallery）：>=1008 展开带标签，>=640 紧凑图标栏，
    // 更窄则完全隐藏（LeftMinimal），收进标题栏菜单按钮后。
    m_navigationView->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Auto);
    m_navigationView->setCompactModeThresholdWidth(NavigationMetrics::CompactThresholdWidth);
    m_navigationView->setExpandedModeThresholdWidth(NavigationMetrics::ExpandedThresholdWidth);
    m_navigationView->setExpandedPaneWidth(NavigationMetrics::ExpandedPaneWidth);
    m_navigationView->setCompactPaneWidth(NavigationMetrics::CompactPaneWidth);
    m_navigationView->setAnimationEnabled(true);
    m_navigationCompactReleaseTimer = new QTimer(this);
    m_navigationCompactReleaseTimer->setSingleShot(true);
    connect(m_navigationCompactReleaseTimer, &QTimer::timeout,
            this, [this]() {
                if (!m_navigationView)
                    return;

                using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
                if (m_navigationView->effectiveDisplayMode() == DisplayMode::Left
                    && m_navigationView->isPaneOpen()) {
                    setNavigationPanesCompact(false);
                }
            });

    m_mainNavigationPane = new GalleryNavigationPane(m_navigationViewModel.mainPaneItems(), m_navigationView);
    m_mainNavigationPane->setObjectName(QStringLiteral("galleryMainNavigationPane"));
    m_footerNavigationPane = new GalleryNavigationPane(m_navigationViewModel.footerPaneItems(), m_navigationView);
    m_footerNavigationPane->setObjectName(QStringLiteral("galleryFooterNavigationPane"));

    m_mainNavigationPane->bind("selectedRouteId",
                               &m_navigationState,
                               "selectedRouteId",
                               fluent::PropertyBinder::TwoWay);
    m_footerNavigationPane->bind("selectedRouteId",
                                 &m_navigationState,
                                 "selectedRouteId",
                                 fluent::PropertyBinder::TwoWay);
    connect(&m_navigationState, &GalleryNavigationState::selectedRouteIdChanged,
            this, [this](const QString& routeId) {
                handleSelectedRouteChanged(routeId);
            });
    connect(m_navigationView, &fluent::navigation::NavigationView::effectiveDisplayModeChanged,
            this, [this](fluent::navigation::NavigationView::DisplayMode mode) {
                using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
                // Title-bar content adapts with the layout (WinUI Gallery): the app title+icon
                // only show when the pane is expanded or a compact rail; in the hidden minimal
                // layout they are dropped so the search box keeps its room. updateTitleBarLayout()
                // also re-flows the search box for the new leading-group width.
                // zh_CN: 标题栏内容随布局自适应（对齐 WinUI Gallery）：应用标题+图标只在窗格展开或紧凑栏时显示；
                // 在隐藏的最小布局里去掉它们，给搜索框让位。updateLayout() 也会按新的前导组宽度重排搜索框。
                if (m_titleBar)
                    m_titleBar->updateLayout();

                if (mode != DisplayMode::Left) {
                    if (m_navigationCompactReleaseTimer)
                        m_navigationCompactReleaseTimer->stop();
                    setNavigationPanesCompact(true);
                    return;
                }

                closeNavigationDrawer();
                if (!m_navigationView->isAnimationEnabled()
                    || m_navigationView->property("layoutTransitionProgress").toDouble() >= 1.0) {
                    setNavigationPanesCompact(!m_navigationView->isPaneOpen());
                    return;
                }

                if (m_navigationCompactReleaseTimer)
                    m_navigationCompactReleaseTimer->start(qMax(1, m_navigationView->themeAnimation().normal));
            });

    m_navigationView->setMainChromeWidget(m_mainNavigationPane);
    m_navigationView->setFooterChromeWidget(m_footerNavigationPane);
    setNavigationPanesCompact(false);
    setContentWidget(m_navigationView);
    updateNavigationCommands();
    LOG_DEBUG(QStringLiteral("GalleryWindow navigationShell built mainRoutes=%1 footerRoutes=%2 expandedPaneWidth=%3 compactPaneWidth=%4")
                  .arg(m_mainNavigationPane->routeIds().size())
                  .arg(m_footerNavigationPane->routeIds().size())
                  .arg(m_navigationView->expandedPaneWidth())
                  .arg(m_navigationView->compactPaneWidth()));
}

void GalleryWindow::buildContentPresenter()
{
    // The presenter owns the route -> page swap inside the navigation view's
    // content host; in-page navigation flows back through routeActivated.
    // zh_CN: presenter 负责在导航视图 content host 内完成路由 → 页面的替换；
    // 页面内导航经 routeActivated 回流。
    m_contentPresenter = new GalleryContentPresenter(m_navigationView->contentHost(),
                                                     m_navigationViewModel,
                                                     this);
    connect(m_contentPresenter, &GalleryContentPresenter::routeActivated,
            this, [this](const QString& routeId) {
                selectRoute(routeId);
            });
}

void GalleryWindow::createTitleBarContent()
{
    // Every navigable title feeds the search box so it reaches the same routes as the pane.
    // zh_CN: 把所有可导航标题作为搜索建议，使搜索可达范围与导航栏一致。
    QStringList searchTitles;
    for (const GalleryNavigationItem& item : m_navigationViewModel.items()) {
        if (item.kind != GalleryNavigationItem::Kind::SectionHeader)
            searchTitles.append(item.title);
    }
    searchTitles.sort(Qt::CaseInsensitive);

    using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
    GalleryTitleBarController::Callbacks callbacks;
    callbacks.onBack = [this]() { navigateBack(); };
    callbacks.onToggleNav = [this]() { toggleNavigationDisplayMode(); };
    callbacks.onSearch = [this](const QString& text) { navigateToSearchResult(text); };
    callbacks.isMinimalNavLayout = [this]() {
        return m_navigationView
            && m_navigationView->effectiveDisplayMode() == DisplayMode::LeftMinimal;
    };
    m_titleBar = new GalleryTitleBarController(titleBar(), searchTitles, std::move(callbacks), this);
}

void GalleryWindow::handleSelectedRouteChanged(const QString& routeId)
{
    LOG_TRACE(QStringLiteral("GalleryWindow selectedRouteSignal routeId=%1")
                  .arg(routeId));

    // Present synchronously: the presenter itself decides whether this is an instant swap
    // (warm, resident page) or a shimmer-skeleton-then-lazy-build (cold page), so there is
    // nothing heavy to defer off this signal. navigateBack() also relies on currentRouteId()
    // being updated by the time this returns.
    // zh_CN: 同步换页：是瞬时切换（已预热的常驻页）还是「先骨架屏再懒构建」（冷页）由 presenter 自行决定，
    // 故没有重活需要从此信号上推迟；navigateBack() 也依赖本函数返回时 currentRouteId() 已更新。
    recordNavigationHistory(routeId);
    if (m_contentPresenter)
        m_contentPresenter->presentRoute(routeId);
}

void GalleryWindow::recordNavigationHistory(const QString& nextRouteId)
{
    // History must capture the on-screen route before the presenter swaps it.
    // zh_CN: 历史栈要在 presenter 换页之前记录屏幕上的路由。
    const QString previousRouteId = currentRouteId();
    if (m_isNavigatingHistory || previousRouteId.isEmpty() || previousRouteId == nextRouteId)
        return;
    if (!m_navigationViewModel.itemById(nextRouteId))
        return;
    if (!m_backRouteStack.isEmpty() && m_backRouteStack.last() == previousRouteId)
        return;

    m_backRouteStack.append(previousRouteId);
    LOG_TRACE(QStringLiteral("GalleryWindow historyPush routeId=%1 depth=%2")
                  .arg(previousRouteId)
                  .arg(m_backRouteStack.size()));
    updateNavigationCommands();
}

bool GalleryWindow::navigateBack()
{
    if (!m_navigationView || m_backRouteStack.isEmpty()) {
        LOG_TRACE(QStringLiteral("GalleryWindow navigateBack skipped reason=%1")
                      .arg(m_navigationView ? QStringLiteral("empty-history")
                                            : QStringLiteral("missing-navigation-view")));
        updateNavigationCommands();
        return false;
    }

    const QString routeId = m_backRouteStack.takeLast();
    LOG_DEBUG(QStringLiteral("GalleryWindow navigateBack routeId=%1 remainingDepth=%2")
                  .arg(routeId)
                  .arg(m_backRouteStack.size()));

    m_isNavigatingHistory = true;
    if (m_navigationState.selectedRouteId() != routeId)
        m_navigationState.setSelectedRouteId(routeId);
    else if (m_contentPresenter)
        m_contentPresenter->presentRoute(routeId);
    m_isNavigatingHistory = false;
    updateNavigationCommands();
    return currentRouteId() == routeId;
}

GalleryWindow::AppWindowWidthState GalleryWindow::appWindowWidthState() const
{
    using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
    const DisplayMode mode = m_navigationView ? m_navigationView->effectiveDisplayMode()
                                              : DisplayMode::Left;
    switch (mode) {
    case DisplayMode::Left:
        return AppWindowWidthState::Expanded;
    case DisplayMode::LeftCompact:
        return AppWindowWidthState::Compact;
    case DisplayMode::LeftMinimal:
        return AppWindowWidthState::Minimal;
    case DisplayMode::Auto:
    case DisplayMode::Top:
        break;
    }
    return AppWindowWidthState::Expanded;
}

void GalleryWindow::toggleNavigationDisplayMode()
{
    if (!m_navigationView) {
        LOG_TRACE(QStringLiteral("GalleryWindow toggleNavigationDisplayMode skipped reason=missing-navigation-view"));
        return;
    }

    if (m_navigationCompactReleaseTimer)
        m_navigationCompactReleaseTimer->stop();

    if (appWindowWidthState() != AppWindowWidthState::Expanded) {
        toggleNavigationDrawer();
        return;
    }

    closeNavigationDrawer();

    // Keep the adaptive Auto mode and just open/close the pane. In the compact / minimal
    // modes opening overlays the pane with full labels; closing returns it to the icon
    // rail (or hidden). zh_CN: 保持自适应 Auto，仅开关窗格。紧凑/最小模式下打开时以全标签浮层覆盖，
    // 关闭则回到图标栏（或隐藏）。
    const bool open = !m_navigationView->isPaneOpen();
    m_navigationView->setPaneOpen(open);
    setNavigationPanesCompact(!open);
    LOG_DEBUG(QStringLiteral("GalleryWindow navigationPaneToggled paneOpen=%1 effectiveMode=%2 chromeWidth=%3")
                  .arg(open ? QStringLiteral("true") : QStringLiteral("false"))
                  .arg(static_cast<int>(m_navigationView->effectiveDisplayMode()))
                  .arg(m_navigationView->chromeGeometry().width()));
}

void GalleryWindow::ensureNavigationDrawer()
{
    if (m_navigationDrawer)
        return;

    m_navigationDrawer = new fluent::collections::DrawerView(this);
    m_navigationDrawer->setObjectName(QStringLiteral("galleryNavigationDrawer"));
    m_navigationDrawer->setEdge(fluent::collections::DrawerView::DrawerEdge::Left);
    m_navigationDrawer->setDrawerLength(NavigationMetrics::ExpandedPaneWidth + kNavigationDrawerShadowMargin);
    m_navigationDrawer->setProperty("fluentSkipSurfacePaint", true);
    m_navigationDrawer->setDim(false);
    m_navigationDrawer->setModal(false);
    m_navigationDrawer->setInteractive(false);
    m_navigationDrawer->setDragMargin(0);
    m_navigationDrawer->setGraphicsEffect(nullptr);
    m_navigationDrawer->setClosePolicy(fluent::collections::DrawerView::ClosePolicy(
        fluent::collections::DrawerView::CloseOnPressOutside
        | fluent::collections::DrawerView::CloseOnEscape));
    m_navigationDrawer->setOuterCornerRadius(themeRadius().overlay);

    m_navigationDrawerContent = new NavigationDrawerContentPanel(m_navigationDrawer);
    m_navigationDrawerContent->setObjectName(QStringLiteral("galleryNavigationDrawerContent"));
    auto* layout = new QVBoxLayout(m_navigationDrawerContent);
    layout->setContentsMargins(0, 0, kNavigationDrawerShadowMargin, 0);
    layout->setSpacing(0);

    m_drawerMainNavigationPane = new GalleryNavigationPane(m_navigationViewModel.mainPaneItems(),
                                                           m_navigationDrawerContent);
    m_drawerMainNavigationPane->setObjectName(QStringLiteral("galleryDrawerMainNavigationPane"));
    m_drawerFooterNavigationPane = new GalleryNavigationPane(m_navigationViewModel.footerPaneItems(),
                                                             m_navigationDrawerContent);
    m_drawerFooterNavigationPane->setObjectName(QStringLiteral("galleryDrawerFooterNavigationPane"));
    m_drawerMainNavigationPane->setSurfaceVisible(true);
    m_drawerFooterNavigationPane->setSurfaceVisible(true);
    m_drawerMainNavigationPane->setCompact(false);
    m_drawerFooterNavigationPane->setCompact(false);

    m_drawerMainNavigationPane->bind("selectedRouteId",
                                     &m_navigationState,
                                     "selectedRouteId",
                                     fluent::PropertyBinder::TwoWay);
    m_drawerFooterNavigationPane->bind("selectedRouteId",
                                       &m_navigationState,
                                       "selectedRouteId",
                                       fluent::PropertyBinder::TwoWay);

    const auto closeAfterRoute = [this](const QString&) {
        QTimer::singleShot(0, this, [this]() {
            closeNavigationDrawer();
        });
    };
    connect(m_drawerMainNavigationPane, &GalleryNavigationPane::routeActivated,
            this, closeAfterRoute);
    connect(m_drawerFooterNavigationPane, &GalleryNavigationPane::routeActivated,
            this, closeAfterRoute);

    layout->addWidget(m_drawerMainNavigationPane, 1);
    layout->addWidget(m_drawerFooterNavigationPane, 0);
    m_navigationDrawer->setContentWidget(m_navigationDrawerContent);
}

void GalleryWindow::toggleNavigationDrawer()
{
    ensureNavigationDrawer();
    if (!m_navigationDrawer)
        return;

    const int titleBarHeight = titleBar() ? titleBar()->height() : TitleBarMetrics::Height;
    m_navigationDrawer->setAvailableMargins(QMargins(0, titleBarHeight, 0, 0));
    m_navigationDrawer->setDrawerLength((m_navigationView
                                             ? m_navigationView->expandedPaneWidth()
                                             : NavigationMetrics::ExpandedPaneWidth)
                                        + kNavigationDrawerShadowMargin);
    if (m_drawerMainNavigationPane)
        m_drawerMainNavigationPane->setCompact(false);
    if (m_drawerFooterNavigationPane)
        m_drawerFooterNavigationPane->setCompact(false);

    if (m_navigationDrawer->isOpen() || m_navigationDrawer->isVisible())
        m_navigationDrawer->close();
    else
        m_navigationDrawer->open();
}

void GalleryWindow::closeNavigationDrawer()
{
    if (m_navigationDrawer && (m_navigationDrawer->isOpen() || m_navigationDrawer->isVisible()))
        m_navigationDrawer->close();
}

void GalleryWindow::setNavigationPanesCompact(bool compact)
{
    if (m_mainNavigationPane)
        m_mainNavigationPane->setCompact(compact);
    if (m_footerNavigationPane)
        m_footerNavigationPane->setCompact(compact);
}

void GalleryWindow::updateNavigationCommands()
{
    if (m_titleBar) {
        m_titleBar->setBackAvailable(!m_backRouteStack.isEmpty());
        m_titleBar->setMenuEnabled(m_navigationView != nullptr);
    }
}

} // namespace fluent::gallery
