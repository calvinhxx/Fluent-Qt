#include "GalleryWindow.h"

#include <algorithm>

#include <QMoveEvent>
#include <QResizeEvent>
#include <QTimer>

#include "components/foundation/FluentElement.h"
#include "compatibility/WindowChromeCompat.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/StackContentHost.h"
#include "components/windowing/TitleBar.h"
#include "design/Typography.h"
#include "model/GalleryContentCatalog.h"
#include "utils/Log.h"
#include "AppIcon.h"
#include "GalleryContentPresenter.h"
#include "GalleryIntroTour.h"
#include "GalleryNavigationPane.h"
#include "GallerySearchRanking.h"
#include "GallerySplashScreen.h"
#include "GalleryTitleBarController.h"
#include "GalleryTopNavigationPane.h"
#include "GalleryWindowMetrics.h"
#include "view/pages/GalleryContentPage.h"
#include "view/pages/PlaceholderPage.h"
#include "view/pages/SettingsPage.h"

namespace fluent::gallery {
namespace {

using AppWindowMetrics = metrics::AppWindow;
using NavigationMetrics = metrics::Navigation;
using TitleBarMetrics = metrics::TitleBar;

// Brief hold after the initial route is on screen before dismissing the splash: long enough
// for the window to composite a few frames (so DWM applies Mica), short enough to feel instant.
// zh_CN: 首个路由上屏后到消除 splash 之间的短暂停留：足够窗口合成几帧（使 DWM 施加 Mica），又短到感觉即时。
constexpr int kStartupSplashHoldMs = 120;

// Delay from splash dismissal to the first-launch intro tour, so the chrome fade-in finishes and
// the window is settled before the coach marks appear. zh_CN: 从 splash 消失到首启引导之间的延迟，
// 让 chrome 淡入完成、窗口稳定后再出现操作引导。
constexpr int kIntroTourDelayMs = 480;

// Idle gap after the last window move/resize event before splash-phase prewarm resumes. Long enough
// to span the lulls between drag updates (so warming stays paused for the whole gesture), short enough
// that warming picks back up promptly once the user lets go. zh_CN: 最后一次窗口移动/缩放事件后、恢复 splash 期
// 预热前的空闲间隔。足够跨过拖动更新之间的停顿（使整个手势期间保持暂停），又短到松手后预热迅速接续。
constexpr int kPrewarmInteractionResumeMs = 200;

} // namespace

GalleryWindow::GalleryWindow(QWidget* parent)
    : fluent::windowing::Window(parent)
    , m_navigationState(this)
{
    setObjectName(QStringLiteral("galleryWindow"));
    setWindowTitle(QStringLiteral("Fluent-Qt Gallery"));
    setWindowIcon(appicon::icon());
    const auto chromePlatform = compatibility::WindowChromeCompat::currentPlatform();
    setCustomWindowChromeEnabled(
        chromePlatform == compatibility::WindowChromeCompat::Platform::Windows
        || chromePlatform == compatibility::WindowChromeCompat::Platform::Linux);
    // Allow narrow windows so the adaptive nav can collapse to its compact / minimal
    // modes; a 980 floor would pin the layout above the 640 breakpoint.
    // zh_CN: 允许窄窗口，让自适应导航能进入紧凑/最小模式；980 的下限会把布局钉在 640 断点之上。
    setMinimumSize(AppWindowMetrics::MinWidth, AppWindowMetrics::MinHeight);

    // Apply the persisted window background effect before the chrome is built and shown, so the nav
    // pane / title bar paint against the right backdrop from the first frame.
    // zh_CN: 在构建并显示 chrome 之前施加持久化的窗口背景效果，使导航栏/标题栏从第一帧就按正确背景绘制。
    setBackdropEffect(GallerySettings::instance().windowEffect());

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

    // Warm Home's directly clickable featured routes first, then the remaining
    // navigation order. Debug builds may hit the fixed startup budget before the
    // whole catalog is resident; prioritizing the landing-page entry points keeps
    // Button/TabView/etc. instant without extending the splash.
    // zh_CN: 先预热 Home 上可直接点击的精选路由，再按导航顺序补齐。Debug 构建可能在
    // 全目录常驻前触及固定启动预算；优先落地页入口可让 Button/TabView 等保持瞬时，且不延长 splash。
    QStringList routeIds;
    auto appendUnique = [&routeIds](const QString& routeId) {
        if (!routeId.isEmpty() && !routeIds.contains(routeId))
            routeIds.append(routeId);
    };
    if (const GalleryContentEntry* home = galleryContentEntry(
            m_navigationViewModel.defaultRouteId())) {
        for (const QString& routeId : home->relatedRouteIds)
            appendUnique(routeId);
    }
    if (m_mainNavigationPane)
        for (const QString& routeId : m_mainNavigationPane->routeIds())
            appendUnique(routeId);
    if (m_footerNavigationPane) {
        for (const QString& routeId : m_footerNavigationPane->routeIds())
            appendUnique(routeId);
    }
    m_contentPresenter->prewarmRoutes(routeIds);
}

void GalleryWindow::moveEvent(QMoveEvent* event)
{
    fluent::windowing::Window::moveEvent(event);
    deferPrewarmDuringInteraction();
}

void GalleryWindow::resizeEvent(QResizeEvent* event)
{
    fluent::windowing::Window::resizeEvent(event);
    deferPrewarmDuringInteraction();
}

void GalleryWindow::deferPrewarmDuringInteraction()
{
    if (!m_contentPresenter)
        return;
    // A top-level move/resize means the user is manipulating the window (or we just placed it at
    // startup). Pause page warming so a synchronous build can't stutter the gesture, then (re)arm a
    // short idle timer; continuous drag updates keep resetting it, so warming resumes only once the
    // window has been still for kPrewarmInteractionResumeMs. zh_CN: 顶层窗口移动/缩放即用户在操作窗口（或启动时
    // 刚摆放）。暂停建页使同步构建不卡顿手势，再（重新）启动短空闲计时器；连续拖动会不断重置它，于是窗口静止
    // kPrewarmInteractionResumeMs 后才恢复预热。
    m_contentPresenter->setPrewarmPaused(true);
    if (!m_prewarmResumeTimer) {
        m_prewarmResumeTimer = new QTimer(this);
        m_prewarmResumeTimer->setSingleShot(true);
        connect(m_prewarmResumeTimer, &QTimer::timeout, this, [this]() {
            if (m_contentPresenter)
                m_contentPresenter->setPrewarmPaused(false);
        });
    }
    m_prewarmResumeTimer->start(kPrewarmInteractionResumeMs);
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

    // First launch only: once the chrome has settled, run the intro tour. zh_CN: 仅首次启动：chrome 稳定后跑引导。
    if (!GallerySettings::instance().introCompleted())
        QTimer::singleShot(kIntroTourDelayMs, this, [this]() { maybeStartIntroTour(); });
}

void GalleryWindow::maybeStartIntroTour()
{
    if (m_introTour)  // already started this session
        return;

    using Tip = fluent::dialogs_flyouts::CoachMark;
    namespace Icons = Typography::Icons;
    QVector<GalleryIntroTour::Step> steps;
    // Centered opener (no target/tail), then anchored coach marks. Each carries a leading glyph.
    // zh_CN: 居中开场(无目标/尾巴),随后是锚定的操作提示;每步带一个前导字形。
    steps.append({nullptr, Icons::Emoji,
                  QStringLiteral("Welcome to Fluent Gallery"),
                  QStringLiteral("A live catalog of Fluent controls for Qt, with runnable samples. "
                                 "Here's a 15-second tour of the essentials."),
                  Tip::Auto,
                  /*centered*/ true});
    if (m_titleBar && m_titleBar->searchBox())
        steps.append({m_titleBar->searchBox(), Icons::Search,
                      QStringLiteral("Search"),
                      QStringLiteral("Find any control or sample by name — just start typing."),
                      Tip::Bottom});
    if (m_mainNavigationPane)
        steps.append({m_mainNavigationPane, Icons::AllApps,
                      QStringLiteral("Browse by category"),
                      QStringLiteral("Controls are grouped by category here. Expand one to explore its samples."),
                      Tip::Right});
    if (m_footerNavigationPane)
        steps.append({m_footerNavigationPane, Icons::Settings,
                      QStringLiteral("Make it yours"),
                      QStringLiteral("Switch between light and dark theme and adjust preferences in Settings."),
                      Tip::Right});

    if (steps.isEmpty())
        return;

    m_introTour = new GalleryIntroTour(this, this);
    m_introTour->setSteps(steps);
    connect(m_introTour, &GalleryIntroTour::finished, this, []() {
        GallerySettings::instance().setIntroCompleted(true);
    });
    m_introTour->start();
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
                // The widen animation has finished: if the pane is still open (inline or flyout),
                // reveal its full labels now. zh_CN: 加宽动画结束：若窗格仍打开（内联或浮层），此刻显示完整标签。
                if (m_navigationView && m_navigationView->isPaneOpen())
                    setNavigationPanesCompact(false);
            });

    m_mainNavigationPane = new GalleryNavigationPane(m_navigationViewModel.mainPaneItems(), m_navigationView);
    m_mainNavigationPane->setObjectName(QStringLiteral("galleryMainNavigationPane"));
    m_footerNavigationPane = new GalleryNavigationPane(m_navigationViewModel.footerPaneItems(), m_navigationView);
    m_footerNavigationPane->setObjectName(QStringLiteral("galleryFooterNavigationPane"));
    m_topMainNavigationPane = new GalleryTopNavigationPane(m_navigationViewModel.mainPaneItems(),
                                                           m_navigationView);
    m_topMainNavigationPane->setObjectName(QStringLiteral("galleryTopMainNavigationPane"));
    m_topFooterNavigationPane = new GalleryTopNavigationPane(m_navigationViewModel.footerPaneItems(),
                                                             m_navigationView);
    m_topFooterNavigationPane->setObjectName(QStringLiteral("galleryTopFooterNavigationPane"));
    m_topMainNavigationPane->hide();
    m_topFooterNavigationPane->hide();

    m_mainNavigationPane->bind("selectedRouteId",
                               &m_navigationState,
                               "selectedRouteId",
                               fluent::PropertyBinder::TwoWay);
    m_footerNavigationPane->bind("selectedRouteId",
                                 &m_navigationState,
                                 "selectedRouteId",
                                 fluent::PropertyBinder::TwoWay);
    m_topMainNavigationPane->bind("selectedRouteId",
                                  &m_navigationState,
                                  "selectedRouteId",
                                  fluent::PropertyBinder::TwoWay);
    m_topFooterNavigationPane->bind("selectedRouteId",
                                    &m_navigationState,
                                    "selectedRouteId",
                                    fluent::PropertyBinder::TwoWay);
    connect(&m_navigationState, &GalleryNavigationState::selectedRouteIdChanged,
            this, [this](const QString& routeId) {
                handleSelectedRouteChanged(routeId);
            });
    connect(m_navigationView, &fluent::navigation::NavigationView::effectiveDisplayModeChanged,
            this, [this](fluent::navigation::NavigationView::DisplayMode mode) {
                // Title-bar content adapts with the layout (WinUI Gallery): the app title+icon
                // only show when the pane is expanded or a compact rail; in the hidden minimal
                // layout they are dropped so the search box keeps its room. updateLayout() also
                // re-flows the search box for the new leading-group width.
                // zh_CN: 标题栏内容随布局自适应（对齐 WinUI Gallery）：应用标题+图标只在窗格展开或紧凑栏时显示；
                // 在隐藏的最小布局里去掉它们，给搜索框让位。updateLayout() 也会按新的前导组宽度重排搜索框。
                if (m_titleBar)
                    m_titleBar->updateLayout();
                // Auto-expand when the layout reaches the Left rail and auto-collapse otherwise, like
                // WinUI: the inline pane is open only in Left. The signal fires only on an actual mode
                // change, so a manual pane toggle within Left (which leaves the mode untouched) is not
                // overridden. zh_CN: 到达 Left 栏时自动展开、否则自动收起，与 WinUI 一致：内联窗格仅在 Left 打开。
                // 该信号仅在模式真正变化时触发，故 Left 内的手动开合（模式不变）不会被覆盖。
                m_navigationView->setPaneOpen(
                    mode == fluent::navigation::NavigationView::DisplayMode::Left);
                applyNavigationPaneDensity();
            });
    // The inline rail vs full-label density is purely a function of the pane's open state now —
    // the same in every side mode — so drive it off paneOpenChanged (NavigationView presents the
    // open pane inline when expanded, or as a flyout when compact / minimal).
    // zh_CN: 内联栏 vs 完整标签的密度现在只取决于窗格开合——各侧边模式一致——故由 paneOpenChanged 驱动
    //（NavigationView 展开时内联呈现打开的窗格，紧凑/最小时呈现为浮层）。
    connect(m_navigationView, &fluent::navigation::NavigationView::paneOpenChanged,
            this, [this](bool) { applyNavigationPaneDensity(); });

    m_navigationView->setMainChromeWidget(m_mainNavigationPane);
    m_navigationView->setFooterChromeWidget(m_footerNavigationPane);
    setNavigationPanesCompact(false);
    setContentWidget(m_navigationView);
    auto& settings = GallerySettings::instance();
    connect(&settings, &GallerySettings::navigationStyleChanged,
            this, &GalleryWindow::applyNavigationStyle);
    connect(&settings, &GallerySettings::windowEffectChanged,
            this, &GalleryWindow::setBackdropEffect);
    applyNavigationStyle(settings.navigationStyle());
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

    // Selecting a destination from the pane while it is a light-dismiss flyout (compact / minimal)
    // closes it, like WinUI — the inline expanded pane (wide windows) stays put. Gate on isVisible():
    // effectiveDisplayMode() reads the live width, which during construction (before show()) is 0 and
    // resolves to LeftMinimal — so the INITIAL selectRoute() would spuriously close the pane and leave
    // it shut once the window opens wide. A flyout only exists on a shown window, so only dismiss then.
    // zh_CN: 在窗格作为轻关闭浮层（紧凑/最小）时选择目标会将其关闭，与 WinUI 一致——内联展开的窗格（宽窗）保持不动。
    // 以 isVisible() 门控：effectiveDisplayMode() 取实时宽度，构造期（show() 之前）宽度为 0、会解析成 LeftMinimal——
    // 于是「初始 selectRoute()」会误关窗格，并在窗口随后以宽布局打开后仍保持关闭。浮层只在已显示的窗口上存在，故仅此时关闭。
    if (m_navigationView && m_navigationView->isPaneOpen() && isVisible()) {
        using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
        const DisplayMode mode = m_navigationView->effectiveDisplayMode();
        // Selecting a leaf destination dismisses the flyout (WinUI). But selecting a CATEGORY only
        // expands its children inline, so the drawer must stay open for the user to drill into them —
        // closing it there would discard the expansion the click just performed. zh_CN: 选择叶子目标会关闭
        // 浮层（与 WinUI 一致）。但选择「分类」只是内联展开其子项，故抽屉须保持打开以便用户继续下钻——此时关闭会丢掉这次
        // 点击刚展开的内容。
        const GalleryNavigationItem* item = m_navigationViewModel.itemById(routeId);
        const bool isCategory = item && item->kind == GalleryNavigationItem::Kind::CategoryRoute;
        if (!isCategory
            && (mode == DisplayMode::LeftCompact || mode == DisplayMode::LeftMinimal))
            m_navigationView->setPaneOpen(false);
    }
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

void GalleryWindow::applyNavigationStyle(GallerySettings::NavigationStyle style)
{
    if (!m_navigationView)
        return;

    using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
    DisplayMode mode = DisplayMode::Auto;
    if (style == GallerySettings::NavigationStyle::Left)
        mode = DisplayMode::Left;
    else if (style == GallerySettings::NavigationStyle::LeftCompact)
        mode = DisplayMode::LeftCompact;
    else if (style == GallerySettings::NavigationStyle::LeftMinimal)
        mode = DisplayMode::LeftMinimal;
    else if (style == GallerySettings::NavigationStyle::Top)
        mode = DisplayMode::Top;

    if (m_navigationCompactReleaseTimer)
        m_navigationCompactReleaseTimer->stop();
    setTopNavigationChrome(mode == DisplayMode::Top);
    m_navigationView->setDisplayMode(mode);
    const DisplayMode effectiveMode = m_navigationView->effectiveDisplayMode();
    // The pane is presented inline-open only when the effective mode is the expanded Left rail; in
    // compact/minimal it starts closed (icon rail / hidden) and opens on demand as a flyout, and Top
    // has no side pane. This is re-synced on effectiveDisplayModeChanged so widening auto-expands and
    // narrowing auto-collapses, like WinUI — and so it stays correct regardless of the construction
    // vs show ordering (effectiveDisplayMode() reads 0 width before show()).
    // zh_CN: 仅当有效模式为展开的 Left 栏时窗格内联打开；紧凑/最小下起始关闭（图标栏/隐藏）、按需以浮层打开，
    // Top 无侧栏。此状态在 effectiveDisplayModeChanged 时重新同步，故加宽自动展开、变窄自动收起，与 WinUI 一致——
    // 且无论「构造 vs 显示」的先后都正确（show() 之前 effectiveDisplayMode() 读到的是 0 宽度）。
    m_navigationView->setPaneOpen(effectiveMode == DisplayMode::Left);
    applyNavigationPaneDensity();
    if (m_titleBar)
        m_titleBar->updateLayout();
    LOG_INFO(QStringLiteral("GalleryWindow navigationStyleApplied style=%1 effectiveMode=%2")
                 .arg(static_cast<int>(style))
                 .arg(static_cast<int>(effectiveMode)));
    updateNavigationCommands();
}

void GalleryWindow::setTopNavigationChrome(bool top)
{
    if (!m_navigationView)
        return;

    QWidget* nextMain = top ? static_cast<QWidget*>(m_topMainNavigationPane)
                            : static_cast<QWidget*>(m_mainNavigationPane);
    QWidget* nextFooter = top ? static_cast<QWidget*>(m_topFooterNavigationPane)
                              : static_cast<QWidget*>(m_footerNavigationPane);

    if (m_navigationView->mainChromeWidget() != nextMain) {
        QWidget* previous = m_navigationView->mainChromeWidget();
        m_navigationView->setMainChromeWidget(nextMain);
        if (previous) {
            previous->setParent(m_navigationView);
            previous->hide();
        }
    }
    if (m_navigationView->footerChromeWidget() != nextFooter) {
        QWidget* previous = m_navigationView->footerChromeWidget();
        m_navigationView->setFooterChromeWidget(nextFooter);
        if (previous) {
            previous->setParent(m_navigationView);
            previous->hide();
        }
    }
}

void GalleryWindow::toggleNavigationDisplayMode()
{
    if (!m_navigationView) {
        LOG_TRACE(QStringLiteral("GalleryWindow toggleNavigationDisplayMode skipped reason=missing-navigation-view"));
        return;
    }

    // One gesture for every side mode: just flip the pane open/closed. NavigationView decides the
    // presentation — inline push when expanded, a light-dismiss flyout (its own surface + shadow +
    // outside/Esc dismiss) when compact / minimal — so there is no separate drawer to drive here.
    // The icon/label density follows via applyNavigationPaneDensity() (paneOpenChanged).
    // zh_CN: 所有侧边模式同一个手势：仅翻转窗格开合。由 NavigationView 决定呈现——展开时内联推开，
    // 紧凑/最小时为轻关闭浮层（自带表面+阴影+外部/Esc 关闭）——故此处不再需要单独的抽屉。
    // 图标/标签密度经 applyNavigationPaneDensity()（paneOpenChanged）跟随。
    m_navigationView->setPaneOpen(!m_navigationView->isPaneOpen());
    LOG_DEBUG(QStringLiteral("GalleryWindow navigationPaneToggled paneOpen=%1 effectiveMode=%2 chromeWidth=%3")
                  .arg(m_navigationView->isPaneOpen() ? QStringLiteral("true") : QStringLiteral("false"))
                  .arg(static_cast<int>(m_navigationView->effectiveDisplayMode()))
                  .arg(m_navigationView->chromeGeometry().width()));
}

void GalleryWindow::applyNavigationPaneDensity()
{
    if (!m_navigationView)
        return;
    if (m_navigationCompactReleaseTimer)
        m_navigationCompactReleaseTimer->stop();

    // Collapsing or closed: drop to the icon rail immediately. zh_CN: 收起或关闭：立即变为图标栏。
    if (!m_navigationView->isPaneOpen()) {
        setNavigationPanesCompact(true);
        return;
    }
    // Opening: reveal full labels, but only once the widen animation has settled so they are not
    // clipped mid-slide. zh_CN: 展开：显示完整标签，但仅在加宽动画稳定后，避免标签在滑动中途被裁剪。
    const bool animating = m_navigationView->isAnimationEnabled()
        && m_navigationView->property("layoutTransitionProgress").toDouble() < 1.0;
    if (animating && m_navigationCompactReleaseTimer)
        m_navigationCompactReleaseTimer->start(qMax(1, m_navigationView->themeAnimation().normal));
    else
        setNavigationPanesCompact(false);
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
        m_titleBar->setMenuEnabled(m_navigationView
                                   && m_navigationView->effectiveDisplayMode()
                                       != fluent::navigation::NavigationView::DisplayMode::Top);
    }
}

} // namespace fluent::gallery
