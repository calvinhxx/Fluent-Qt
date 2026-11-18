#include "GalleryWindow.h"
#include "GallerySpatialController.h"
#ifdef FLUENT_QT_HAS_SPATIAL
#include "components/spatial/SpatialSession.h"
#include "support/spatial/SpatialTextureHost.h"
#endif

#include <algorithm>

#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QLineEdit>
#include <QMoveEvent>
#include <QHideEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QWindow>

#include "components/foundation/FluentElement.h"
#include "components/foundation/MotionPolicy.h"
#include "compatibility/WindowChromeCompat.h"
#include "compatibility/QtCompat.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/StackContentHost.h"
#include "components/textfields/EditingCommandRouter.h"
#include "components/windowing/TitleBar.h"
#include "design/Typography.h"
#include "model/GalleryContentCatalog.h"
#include "platform/GalleryPlatform.h"
#include "support/logging/Log.h"
#include "AppIcon.h"
#include "GalleryCommands.h"
#include "GalleryContentPresenter.h"
#include "GalleryInterfaceZoomController.h"
#include "GalleryIntroTour.h"
#include "GalleryNavigationPane.h"
#include "GallerySearchService.h"
#include "GallerySplashScreen.h"
#include "GalleryTitleBarController.h"
#include "GalleryTopNavigationPane.h"
#include "GalleryWindowMetrics.h"
#include "view/pages/GalleryContentPage.h"
#include "view/pages/SettingsPage.h"
#include "view/widgets/GallerySampleCatalog.h"
#include "view/support/GalleryEditingCommands.h"
#include "view/support/GalleryToast.h"

namespace fluent::gallery {
namespace {

using AppWindowMetrics = metrics::AppWindow;
using NavigationMetrics = metrics::Navigation;
using TitleBarMetrics = metrics::TitleBar;

// Loading time counts toward the minimum brand reveal; Reduced/Disabled motion skips the wait.
// zh_CN: 加载耗时计入品牌展示最小时长；减弱或禁用动效时不额外等待。
constexpr int kStartupSplashMinimumMs = 1400;
constexpr int kStartupBrandReadyHoldMs = 250;
// Allow a few compositor frames after loading so DWM finishes initial backdrop setup.
// zh_CN: 加载完成后留出数帧合成时间，等 DWM 完成首屏背景初始化。
constexpr int kStartupSplashHoldMs = 120;

// Delay before starting the first-launch tour so splash fade-out and chrome settle first.
constexpr int kIntroTourDelayMs = 480;

// Debounce gap after move/resize events before resuming splash-phase prewarm.
constexpr int kPrewarmInteractionResumeMs = 200;

} // namespace

GalleryWindow::GalleryWindow(QWidget* parent) : fluent::windowing::Window(parent), m_navigationState(this)
{
    GallerySpatialController::prepareApplicationStyle();
#ifdef FLUENT_QT_HAS_SPATIAL
    spatial::SpatialSession startupSession(this);
    if (qEnvironmentVariableIntValue("FLUENT_QT_GALLERY_DISABLE_3D") == 0
        && qApp->property("fluentqtGalleryGraphicsBackendFailure").toString().isEmpty() && startupSession.activate()) {
        if (spatial::SpatialTextureHost::isPreferred())
            spatial::SpatialTextureHost::prepareWindow(this);
        else
            startupSession.prepareWindow(this);
    }
#endif
    setObjectName(QStringLiteral("galleryWindow"));
    setWindowTitle(platform::capabilities().windowTitle);
    setWindowIcon(appicon::icon());
    const auto chromePlatform = compatibility::WindowChromeCompat::currentPlatform();
    const bool useClientTitleBar = platform::capabilities().usesClientSideTitleBar
        || chromePlatform == compatibility::WindowChromeCompat::Platform::Windows
        || chromePlatform == compatibility::WindowChromeCompat::Platform::Linux;
    const bool useCaptionButtons = useClientTitleBar
        && chromePlatform != compatibility::WindowChromeCompat::Platform::MacOS;
    setCustomWindowChromeEnabled(useClientTitleBar);
    // Application-owned chrome includes caption controls on desktop Linux,
    // Windows, and browser-hosted windows. macOS keeps native traffic lights.
    // zh_CN: Windows、Linux 与浏览器宿主窗口的应用自绘 chrome 包含标题栏按钮；
    // macOS 保留原生 traffic lights。
    if (useCaptionButtons) {
        setCaptionButtonToolTips(QStringLiteral("Minimize"), QStringLiteral("Maximize"), QStringLiteral("Close"),
            QStringLiteral("Restore"));
        setCaptionButtonAccessibleNames(QStringLiteral("Minimize"), QStringLiteral("Maximize"), QStringLiteral("Close"),
            QStringLiteral("Restore"));
    }
    // Allow narrow windows so the adaptive nav can collapse to its compact / minimal
    // modes; a 980 floor would pin the layout above the 640 breakpoint.
    // zh_CN: 允许窄窗口，让自适应导航能进入紧凑/最小模式；980 的下限会把布局钉在 640 断点之上。
    setMinimumSize(AppWindowMetrics::MinWidth, AppWindowMetrics::MinHeight);

    // Apply the persisted window background effect before the chrome is built and shown, so the nav
    // pane / title bar paint against the right backdrop from the first frame.
    // zh_CN: 在构建并显示 chrome 之前施加持久化的窗口背景效果，使导航栏/标题栏从第一帧就按正确背景绘制。
    setBackdropEffect(GallerySettings::instance().windowEffect());

    m_editingCommandRouter = new fluent::textfields::EditingCommandRouter(this, this);
    m_editingCommandRouter->setObjectName(QStringLiteral("Gallery.WindowEditingCommandRouter"));

    createTitleBarContent();
    buildNavigationShell();
    buildContentPresenter();
    installCommands();
    installSplashScreen();
    new GallerySpatialController(this, m_navigationView);
    showInitialRouteContent();
    prewarmRemainingRoutes();
    LOG_INFO(QStringLiteral("GalleryWindow constructed defaultRoute=%1").arg(m_navigationViewModel.defaultRouteId()));
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
        LOG_WARN(QStringLiteral("GalleryWindow selectRoute rejected routeId=%1 reason=missing-route").arg(routeId));
        return false;
    }

    if (m_navigationState.selectedRouteId() != routeId) {
        LOG_DEBUG(QStringLiteral("GalleryWindow selectRoute routeId=%1 state=change").arg(routeId));
        m_navigationState.setSelectedRouteId(routeId);
        return true;
    }

    LOG_TRACE(QStringLiteral("GalleryWindow selectRoute routeId=%1 state=show-current").arg(routeId));
    return m_contentPresenter && m_contentPresenter->presentRoute(routeId);
}

bool GalleryWindow::navigateToSearchResult(const QString& searchText)
{
    const QString needle = searchText.trimmed();
    if (needle.isEmpty() || !m_searchService)
        return false;

    // Enter resolves through the ranking that orders the suggestion list, so it lands on the
    // list's first result: routes before examples, then exact title > prefix > token match.
    // zh_CN: 回车与建议列表共用同一排序，落到列表首个结果：路由先于示例，其次精确标题 > 前缀 > 词元匹配。
    const QVector<search::Entry> ranked = m_searchService->rankedResults(needle);
    if (ranked.isEmpty()) {
        LOG_DEBUG(QStringLiteral("GalleryWindow searchNavigate miss text=%1").arg(needle));
        return false;
    }

    const search::Entry& best = ranked.first();
    LOG_DEBUG(QStringLiteral("GalleryWindow searchNavigate text=%1 routeId=%2 match=%3")
            .arg(needle, best.routeId, best.isExample() ? QStringLiteral("example") : QStringLiteral("route")));
    return selectRoute(best.routeId);
}

void GalleryWindow::showInitialRouteContent()
{
    selectRoute(m_navigationViewModel.defaultRouteId());
}

void GalleryWindow::installSplashScreen()
{
    if (!m_contentPresenter)
        return;

    // Hide Gallery title-bar chrome while splash owns the content area, keeping only native window controls.
    // zh_CN: 启动遮罩覆盖期间隐藏自绘标题栏控件，仅保留原生窗口按钮，待退场时恢复。
    if (m_titleBar)
        m_titleBar->setChromeVisible(false);

    // Cover the content area from the first frame while the title bar stays interactive above it.
    m_splashScreen = new GallerySplashScreen(contentHost());
    m_splashEventSource = m_splashScreen;
    if (m_titleBar) {
        m_splashScreen->setTransitionTarget(m_titleBar->appIconWidget());
        m_titleBar->setAppIconRevealed(false);
    }
    connect(m_splashScreen, &GallerySplashScreen::dismissed, this, [this]() {
        if (m_titleBar)
            m_titleBar->setAppIconRevealed(true);
        LOG_DEBUG(QStringLiteral("GalleryWindow startup dismissed visibleMs=%1").arg(m_startupVisibleTimer.elapsed()));
        if (!m_startupFailedRoutes.isEmpty())
            showGalleryToast(this,
                QStringLiteral("Some pages could not be prepared: %1. Select a page to retry.")
                    .arg(m_startupFailedRoutes.join(QStringLiteral(", "))),
                status_info::Toast::Warning);
        // Schedule from actual completion so the tour never covers a travelling logo.
        // zh_CN: 从实际退场完成时计时，避免首启引导盖住仍在移动的图标。
        if (platform::capabilities().showsIntroTour && !GallerySettings::instance().introCompleted())
            QTimer::singleShot(kIntroTourDelayMs, this, [this]() { maybeStartIntroTour(); });
    });
    connect(m_splashScreen, &GallerySplashScreen::replaced, this, [this]() {
        finishStartup();
        if (m_titleBar)
            m_titleBar->setAppIconRevealed(true);
    });
    m_startupFinishTimer = new QTimer(this);
    m_startupFinishTimer->setSingleShot(true);
    m_startupFinishTimer->setTimerType(Qt::PreciseTimer);
    connect(m_startupFinishTimer, &QTimer::timeout, this, &GalleryWindow::finishStartup);
    connect(&MotionPolicy::instance(), &MotionPolicy::modeChanged, this, [this]() { scheduleStartupFinish(); });
    m_splashScreen->show();
    m_splashScreen->raise();
    // Defer page construction until the splash paints its first frame.
    // zh_CN: 等 splash 绘出首帧后再开始构建页面，避免启动阶段长时间白屏。
    m_contentPresenter->setPrewarmPaused(true);
    m_contentPresenter->setStartupCovered(true);
    m_splashScreen->installEventFilter(this);

    connect(m_contentPresenter, &GalleryContentPresenter::prewarmProgress, this, [this](int done, int total) {
        if (m_splashScreen)
            m_splashScreen->setProgress(done, total);
    });
    connect(m_contentPresenter, &GalleryContentPresenter::prewarmFinished, this, [this]() {
        if (!m_startupReadyTimer.isValid()) {
            m_startupReadyTimer.start();
            LOG_DEBUG(QStringLiteral("GalleryWindow startup ready visibleMs=%1")
                    .arg(m_startupVisibleTimer.isValid() ? m_startupVisibleTimer.elapsed() : 0));
        }
        scheduleStartupFinish();
    });
    connect(m_contentPresenter, &GalleryContentPresenter::prewarmFailed, this, [this](const QString& routeId) {
        m_startupFailedRoutes.append(routeId);
        setProperty("galleryStartupFailedRoutes", m_startupFailedRoutes);
        if (m_splashScreen)
            m_splashScreen->setText(QStringLiteral("Some pages could not be prepared"));
        LOG_WARN(QStringLiteral("GalleryWindow startup page failed routeId=%1").arg(routeId));
    });
}

bool GalleryWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_splashEventSource && event->type() == QEvent::Paint && !m_startupPrewarmStarted
        && !m_startupReadyTimer.isValid() && isVisible() && !isMinimized()) {
        m_startupPrewarmStarted = true;
        QTimer::singleShot(0, this, [this] {
            if (m_startupPrewarmStarted && !m_startupReadyTimer.isValid() && isVisible() && !isMinimized()
                && m_contentPresenter && (!m_prewarmResumeTimer || !m_prewarmResumeTimer->isActive()))
                m_contentPresenter->setPrewarmPaused(false);
        });
    }
    return fluent::windowing::Window::eventFilter(watched, event);
}

void GalleryWindow::hideEvent(QHideEvent* event)
{
    if (!m_startupFinished && m_contentPresenter) {
        m_contentPresenter->setPrewarmPaused(true);
        m_startupPrewarmStarted = false;
        if (m_prewarmResumeTimer)
            m_prewarmResumeTimer->stop();
        if (m_startupFinishTimer)
            m_startupFinishTimer->stop();
    }
    fluent::windowing::Window::hideEvent(event);
}

void GalleryWindow::showEvent(QShowEvent* event)
{
    fluent::windowing::Window::showEvent(event);
    if (m_splashScreen && !m_startupVisibleTimer.isValid())
        m_startupVisibleTimer.start();
    scheduleStartupFinish();
}

void GalleryWindow::scheduleStartupFinish()
{
    if (m_startupFinished || !m_startupFinishTimer || !m_splashScreen || !isVisible() || isMinimized()
        || !m_startupReadyTimer.isValid() || !m_startupVisibleTimer.isValid())
        return;
    const bool branded = MotionPolicy::instance().mode() == MotionPolicy::Mode::Full
        && m_splashScreen->presentation() == fluent::status_info::SplashScreen::Presentation::Branded;
    const qint64 presentationLeft = (branded ? kStartupSplashMinimumMs : 0) - m_startupVisibleTimer.elapsed();
    const qint64 readyHoldLeft = (branded ? kStartupBrandReadyHoldMs : kStartupSplashHoldMs)
        - m_startupReadyTimer.elapsed();
    m_startupFinishTimer->start(int(qMax(qint64(0), qMax(presentationLeft, readyHoldLeft))));
}

void GalleryWindow::prewarmRemainingRoutes()
{
    if (!m_contentPresenter)
        return;

    // Prepare only navigation essentials. Unvisited demos do not extend Splash
    // or run background construction during interactive use.
    QStringList routeIds;
    if (platform::capabilities().prewarmsRoutes)
        routeIds.append(QStringLiteral("settings"));
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
    if (!m_contentPresenter || m_startupReadyTimer.isValid())
        return;
    // Pause prewarm while moving/resizing so synchronous page builds do not stutter the drag.
    // zh_CN: 移动或缩放窗口期间暂停预热，松手空闲 kPrewarmInteractionResumeMs 后再恢复。
    m_contentPresenter->setPrewarmPaused(true);
    if (!m_prewarmResumeTimer) {
        m_prewarmResumeTimer = new QTimer(this);
        m_prewarmResumeTimer->setSingleShot(true);
        connect(m_prewarmResumeTimer, &QTimer::timeout, this, [this]() {
            if (m_contentPresenter && m_startupPrewarmStarted && isVisible() && !isMinimized()
                && !m_startupReadyTimer.isValid())
                m_contentPresenter->setPrewarmPaused(false);
        });
    }
    m_prewarmResumeTimer->start(kPrewarmInteractionResumeMs);
}

void GalleryWindow::finishStartup()
{
    if (m_startupFinished || !m_splashScreen)
        return;
    m_startupFinished = true;
    m_startupFinishTimer->stop();
    // Replacing the splash ends startup early; leave unbuilt routes for on-demand loading.
    // zh_CN: 若启动遮罩被提前替换，则中止后台预热，其余页面留待按需加载。
    m_contentPresenter->cancelPrewarm();
    m_contentPresenter->setStartupCovered(false);
    LOG_DEBUG(QStringLiteral("GalleryWindow startup dismiss visibleMs=%1 readyMs=%2")
            .arg(m_startupVisibleTimer.elapsed())
            .arg(m_startupReadyTimer.elapsed()));
    if (m_titleBar)
        m_titleBar->setChromeVisible(true, /*animated*/ true);
    // Reapply backdrop after initial frames settle to avoid the DWM first-show Mica race.
    // zh_CN: 首屏数帧合成完毕后重新应用系统背景材质，绕过 DWM 首次显示时 Mica 偶发不生效的竞争。
    reapplySystemBackdrop();
    if (m_splashScreen) {
        m_splashScreen->cacheDismissalContent(m_navigationView);
        m_splashScreen->dismiss(); // connects the logo to the title bar, then self-deletes
    }
}

void GalleryWindow::maybeStartIntroTour()
{
    if (m_introTour) // already started this session
        return;

    using Tip = fluent::dialogs_flyouts::CoachMark;
    namespace Icons = Typography::Icons;
    QVector<GalleryIntroTour::Step> steps;
    // Centered opener (no target/tail), then anchored coach marks. Each carries a leading glyph.
    // zh_CN: 居中开场(无目标/尾巴),随后是锚定的操作提示;每步带一个前导字形。
    steps.append({nullptr, Icons::Emoji, QStringLiteral("Welcome to Fluent Gallery"),
        QStringLiteral(
            "A live catalog of Fluent controls for Qt, with runnable samples. "
            "Here's a 15-second tour of the essentials."),
        Tip::Auto,
        /*centered*/ true});
    if (m_titleBar && m_titleBar->searchBox())
        steps.append({m_titleBar->searchBox(), Icons::Search, QStringLiteral("Search"),
            QStringLiteral("Find any control or sample by name — just start typing."), Tip::Bottom});
    const bool top = m_navigationView->effectiveDisplayMode() == fluent::navigation::NavigationView::DisplayMode::Top;
    if (auto* main = m_navigationView->mainChromeWidget())
        steps.append({main, Icons::AllApps, QStringLiteral("Browse by category"),
            QStringLiteral("Controls are grouped by category here. Expand one to explore its samples."),
            top ? Tip::Bottom : Tip::Right});
    if (auto* footer = m_navigationView->footerChromeWidget())
        steps.append({footer, Icons::Settings, QStringLiteral("Make it yours"),
            QStringLiteral("Switch between light and dark theme and adjust preferences in Settings."),
            top ? Tip::Bottom : Tip::Right});

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
    return m_contentPresenter ? dynamic_cast<GalleryContentPage*>(m_contentPresenter->currentPage()) : nullptr;
}

SettingsPage* GalleryWindow::currentSettingsPage() const
{
    return m_contentPresenter ? dynamic_cast<SettingsPage*>(m_contentPresenter->currentPage()) : nullptr;
}

void GalleryWindow::buildNavigationShell()
{
    m_navigationView = new fluent::navigation::NavigationView(this);
    m_navigationView->setObjectName(QStringLiteral("galleryNavigationView"));
    // Responsive thresholds matching WinUI Gallery: >=1008 expanded, >=640 compact rail, <640 hidden.
    // zh_CN: 响应式断点对齐 WinUI Gallery：>=1008 展开，>=640 紧凑图标栏，<640 隐藏。
    m_navigationView->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Auto);
    m_navigationView->setCompactModeThresholdWidth(NavigationMetrics::CompactThresholdWidth);
    m_navigationView->setExpandedModeThresholdWidth(NavigationMetrics::ExpandedThresholdWidth);
    m_navigationView->setExpandedPaneWidth(NavigationMetrics::ExpandedPaneWidth);
    m_navigationView->setCompactPaneWidth(NavigationMetrics::CompactPaneWidth);
    m_navigationView->setAnimationEnabled(true);
    m_navigationCompactReleaseTimer = new QTimer(this);
    m_navigationCompactReleaseTimer->setSingleShot(true);
    connect(m_navigationCompactReleaseTimer, &QTimer::timeout, this, [this]() {
        // The widen animation has finished: if the pane is still open (inline or flyout),
        // reveal its full labels now. zh_CN: 加宽动画结束：若窗格仍打开（内联或浮层），此刻显示完整标签。
        if (m_navigationView && m_navigationView->isPaneOpen())
            setNavigationPanesCompact(false);
    });

    m_mainNavigationPane = new GalleryNavigationPane(m_navigationViewModel.mainPaneItems(), m_navigationView);
    m_mainNavigationPane->setObjectName(QStringLiteral("galleryMainNavigationPane"));
    m_footerNavigationPane = new GalleryNavigationPane(m_navigationViewModel.footerPaneItems(), m_navigationView);
    m_footerNavigationPane->setObjectName(QStringLiteral("galleryFooterNavigationPane"));
    m_topMainNavigationPane = new GalleryTopNavigationPane(m_navigationViewModel.mainPaneItems(), m_navigationView);
    m_topMainNavigationPane->setObjectName(QStringLiteral("galleryTopMainNavigationPane"));
    m_topFooterNavigationPane = new GalleryTopNavigationPane(m_navigationViewModel.footerPaneItems(), m_navigationView);
    m_topFooterNavigationPane->setObjectName(QStringLiteral("galleryTopFooterNavigationPane"));
    m_topMainNavigationPane->hide();
    m_topFooterNavigationPane->hide();

    m_mainNavigationPane->bind("selectedRouteId", &m_navigationState, "selectedRouteId",
        fluent::PropertyBinder::TwoWay);
    m_footerNavigationPane->bind("selectedRouteId", &m_navigationState, "selectedRouteId",
        fluent::PropertyBinder::TwoWay);
    m_topMainNavigationPane->bind("selectedRouteId", &m_navigationState, "selectedRouteId",
        fluent::PropertyBinder::TwoWay);
    m_topFooterNavigationPane->bind("selectedRouteId", &m_navigationState, "selectedRouteId",
        fluent::PropertyBinder::TwoWay);
    connect(&m_navigationState, &GalleryNavigationState::selectedRouteIdChanged, this, [this](const QString& routeId) {
        handleSelectedRouteChanged(routeId);
    });
    connect(m_navigationView, &fluent::navigation::NavigationView::effectiveDisplayModeChanged, this,
        [this](fluent::navigation::NavigationView::DisplayMode mode) {
            // Re-flow title bar so LeftMinimal hides app title/icon to leave room for search.
            if (m_titleBar)
                m_titleBar->updateLayout();
            // Auto-open the inline pane only in Left mode; effectiveDisplayModeChanged avoids
            // overriding manual pane toggles within the same mode.
            // zh_CN: 仅在宽屏 Left 模式下自动展开内联窗格，且只在断点模式变化时同步，不覆盖手动开合。
            m_navigationView->setPaneOpen(mode == fluent::navigation::NavigationView::DisplayMode::Left);
            applyNavigationPaneDensity();
        });
    connect(m_navigationView, &fluent::navigation::NavigationView::paneOpenChanged, this, [this](bool) {
        applyNavigationPaneDensity();
    });

    m_navigationView->setMainChromeWidget(m_mainNavigationPane);
    m_navigationView->setFooterChromeWidget(m_footerNavigationPane);
    setNavigationPanesCompact(false);
    setContentWidget(m_navigationView);
    auto& settings = GallerySettings::instance();
    connect(&settings, &GallerySettings::navigationStyleChanged, this, &GalleryWindow::applyNavigationStyle);
    connect(&settings, &GallerySettings::windowEffectChanged, this, &GalleryWindow::setBackdropEffect);
    applyNavigationStyle(settings.navigationStyle());
    updateNavigationCommands();
    LOG_DEBUG(QStringLiteral(
        "GalleryWindow navigationShell built mainRoutes=%1 footerRoutes=%2 "
        "expandedPaneWidth=%3 compactPaneWidth=%4")
            .arg(m_mainNavigationPane->routeIds().size())
            .arg(m_footerNavigationPane->routeIds().size())
            .arg(m_navigationView->expandedPaneWidth())
            .arg(m_navigationView->compactPaneWidth()));
}

void GalleryWindow::buildContentPresenter()
{
    m_contentPresenter = new GalleryContentPresenter(m_navigationView->contentHost(), m_navigationViewModel, this,
        platform::capabilities().maxResidentRoutes);
    connect(m_contentPresenter, &GalleryContentPresenter::routeActivated, this, [this](const QString& routeId) {
        selectRoute(routeId);
    });
}

void GalleryWindow::createTitleBarContent()
{
    // Every navigable route is searchable, so search reaches the same pages as the pane; the
    // examples on those pages are indexed lazily once the user first searches.
    // zh_CN: 所有可导航路由均可搜索，使搜索可达范围与导航栏一致；页面中的示例在首次搜索后惰性建立索引。
    QVector<search::Entry> routes;
    for (const GalleryNavigationItem& item : m_navigationViewModel.items()) {
        if (item.kind != GalleryNavigationItem::Kind::SectionHeader)
            routes.append({item.id, item.title, QString()});
    }
    m_searchService = new GallerySearchService(
        std::move(routes),
        [](const search::Entry& route) {
            QVector<search::Entry> examples;
            for (const GallerySample& sample : gallerySamplesForRoute(route.routeId)) {
                if (!sample.title.isEmpty())
                    examples.append({route.routeId, sample.title, route.title});
            }
            return examples;
        },
        this);

    using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
    GalleryTitleBarController::Callbacks callbacks;
    callbacks.onBack = [this]() { navigateBack(); };
    callbacks.onToggleNav = [this]() { toggleNavigationDisplayMode(); };
    callbacks.onSearchResultChosen = [this](const QString& routeId) { selectRoute(routeId); };
    callbacks.isMinimalNavLayout = [this]() {
        return m_navigationView && m_navigationView->effectiveDisplayMode() == DisplayMode::LeftMinimal;
    };
    auto* chrome = titleBar();
    // The controller must die before the title bar's widget children. Parenting
    // it to the chrome establishes that order and lets its destructor detach
    // host-window event filters while both watched objects are still valid.
    // zh_CN: 控制器必须先于标题栏的控件子对象析构。以 chrome 为父对象可建立
    // 该顺序，并让析构函数在两个监听对象仍有效时解除宿主窗口事件过滤器。
    m_titleBar = new GalleryTitleBarController(chrome, m_searchService, std::move(callbacks), chrome);
}

void GalleryWindow::installCommands()
{
    using Group = GalleryCommands::Group;
    m_commands = new GalleryCommands(this);

    if (platform::supportsInterfaceZoom()) {
        auto* zoom = new GalleryInterfaceZoomController(this);
        for (QAction* action : zoom->commands())
            m_commands->addCommand(Group::View, action);
    }
    m_toggleNavigationCommand = m_commands->addCommand(Group::View, QStringLiteral("galleryToggleNavigationAction"),
        QStringLiteral("Toggle Navigation Pane"));
    connect(m_toggleNavigationCommand, &QAction::triggered, this, &GalleryWindow::toggleNavigationDisplayMode);

    // Platform Back bindings, without Backspace: a bare Backspace would leave the page whenever
    // focus is outside a text field. zh_CN: 使用平台后退快捷键但排除 Backspace，避免焦点不在输入框时误离开页面。
    QList<QKeySequence> backKeys;
    for (const QKeySequence& key : QKeySequence::keyBindings(QKeySequence::Back)) {
        if (key != QKeySequence(Qt::Key_Backspace))
            backKeys.append(key);
    }
    m_backCommand = m_commands->addCommand(Group::Go, QStringLiteral("galleryBackAction"), QStringLiteral("Back"),
        backKeys);
    connect(m_backCommand, &QAction::triggered, this, [this]() { navigateBack(); });
    QAction* search = m_commands->addCommand(Group::Go, QStringLiteral("gallerySearchAction"), QStringLiteral("Search"),
        {QKeySequence(Qt::CTRL | Qt::Key_K)});
    connect(search, &QAction::triggered, this, &GalleryWindow::focusSearch);
    QAction* home = m_commands->addCommand(Group::Go, QStringLiteral("galleryHomeAction"), QStringLiteral("Home"));
    connect(home, &QAction::triggered, this, [this]() { selectRoute(m_navigationViewModel.defaultRouteId()); });
    QAction* settings = m_commands->addCommand(Group::Go, QStringLiteral("gallerySettingsAction"),
        QStringLiteral("Settings"), {QKeySequence(Qt::CTRL | Qt::Key_Comma)});
    // macOS moves this command to the application menu, where Settings belongs.
    // zh_CN: macOS 会把该命令移到应用菜单中 Settings 的标准位置。
    settings->setMenuRole(QAction::PreferencesRole);
    connect(settings, &QAction::triggered, this, [this]() { selectRoute(QStringLiteral("settings")); });

    auto* updates = m_commands->addCommand(Group::Help, QStringLiteral("galleryWhatsNewAction"),
        QStringLiteral("What’s new"));
    connect(updates, &QAction::triggered, m_titleBar, &GalleryTitleBarController::showWhatsNew);
    QAction* shortcuts = m_commands->addCommand(Group::Help, QStringLiteral("galleryKeyboardShortcutsAction"),
        QStringLiteral("Keyboard Shortcuts"),
        {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P), QKeySequence(Qt::CTRL | Qt::Key_Slash)});
    connect(shortcuts, &QAction::triggered, this, &GalleryWindow::showCommandPalette);
    updateNavigationCommands();
}

void GalleryWindow::showCommandPalette()
{
    if (m_commands)
        m_commands->showPalette();
}

void GalleryWindow::focusSearch()
{
    QWidget* searchBox = m_titleBar ? m_titleBar->searchBox() : nullptr;
    if (!searchBox || !searchBox->isVisible()) {
        LOG_DEBUG(QStringLiteral("GalleryWindow focusSearch skipped reason=search-hidden"));
        return;
    }
    searchBox->setFocus(Qt::ShortcutFocusReason);
    if (auto* edit = qobject_cast<QLineEdit*>(searchBox))
        edit->selectAll();
}

void GalleryWindow::handleSelectedRouteChanged(const QString& routeId)
{
    if (routeId != QStringLiteral("settings"))
        setProperty("galleryLastContentRoute", routeId);
    if (m_splashScreen)
        m_splashScreen->clearDismissalContent();
    LOG_TRACE(QStringLiteral("GalleryWindow selectedRouteSignal routeId=%1").arg(routeId));

    // Present synchronously because navigateBack() expects currentRouteId() to update before return.
    // zh_CN: 同步切换路由（冷页由内部先切骨架屏），确保 navigateBack() 返回前 currentRouteId() 已更新。
    recordNavigationHistory(routeId);
    if (m_contentPresenter)
        m_contentPresenter->presentRoute(routeId);

    // Dismiss flyout nav after choosing a leaf route, but guard on isVisible(): before show()
    // width is 0 so effectiveDisplayMode() reports LeftMinimal and would wrongly close the initial pane.
    // zh_CN: 紧凑/最小时选定目标页后收起浮层窗格；以 isVisible() 防护，避免构造期宽度为 0 误关初始窗格。
    if (m_navigationView && m_navigationView->isPaneOpen() && isVisible()) {
        using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
        const DisplayMode mode = m_navigationView->effectiveDisplayMode();
        // Keep the flyout open when clicking a category so its child items can expand inline.
        // zh_CN: 点击分类项仅内联展开子项，保持浮层打开以便继续选择子页面。
        const GalleryNavigationItem* item = m_navigationViewModel.itemById(routeId);
        const bool isCategory = item && item->kind == GalleryNavigationItem::Kind::CategoryRoute;
        if (!isCategory && (mode == DisplayMode::LeftCompact || mode == DisplayMode::LeftMinimal))
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
                .arg(m_navigationView ? QStringLiteral("empty-history") : QStringLiteral("missing-navigation-view")));
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
    const DisplayMode mode = m_navigationView ? m_navigationView->effectiveDisplayMode() : DisplayMode::Left;
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
    // Only Left mode starts inline-open; compact/minimal open on demand as flyouts.
    // zh_CN: 仅宽屏 Left 模式默认内联展开，其余模式默认收起（紧凑/最小按需弹出浮层）。
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

    QWidget* nextMain =
        top ? static_cast<QWidget*>(m_topMainNavigationPane) : static_cast<QWidget*>(m_mainNavigationPane);
    QWidget* nextFooter =
        top ? static_cast<QWidget*>(m_topFooterNavigationPane) : static_cast<QWidget*>(m_footerNavigationPane);

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

    // NavigationView chooses inline vs overlay display based on mode; density tracks paneOpenChanged.
    // zh_CN: 展开形态（内联或浮层）由 NavigationView 根据模式自行决定，标签密度随 paneOpenChanged 同步。
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
    const bool backAvailable = !m_backRouteStack.isEmpty();
    const bool sidePaneAvailable = m_navigationView
        && m_navigationView->effectiveDisplayMode() != fluent::navigation::NavigationView::DisplayMode::Top;
    if (m_titleBar) {
        m_titleBar->setBackAvailable(backAvailable);
        m_titleBar->setMenuEnabled(sidePaneAvailable);
    }
    if (m_backCommand)
        m_backCommand->setEnabled(backAvailable);
    if (m_toggleNavigationCommand)
        m_toggleNavigationCommand->setEnabled(sidePaneAvailable);
}

} // namespace fluent::gallery
