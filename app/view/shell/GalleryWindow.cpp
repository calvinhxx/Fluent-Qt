#include "GalleryWindow.h"

#include <algorithm>

#include <QAbstractAnimation>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPoint>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QTimer>
#include <QVariantAnimation>

#include "components/basicinput/Button.h"
#include "components/foundation/QMLPlus.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/StackContentHost.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"
#include "components/windowing/TitleBar.h"
#include "design/Typography.h"
#include "utils/Log.h"
#include "AppIcon.h"
#include "GalleryContentPresenter.h"
#include "GalleryNavigationPane.h"
#include "GallerySplashScreen.h"
#include "view/pages/GalleryContentPage.h"
#include "view/pages/PlaceholderPage.h"
#include "view/pages/SettingsPage.h"

namespace fluent::gallery {
namespace {

using Edge = fluent::AnchorLayout::Edge;

constexpr int kTitleBarHeight = 42;  // Taller than the 36 default; 42 reads better than WinUI's 48 on macOS.
constexpr int kTitleBarHorizontalMargin = 8;
constexpr int kTitleBarItemGap = 8;
constexpr int kTitleBarButtonSize = 24;
constexpr int kTitleBarIconSize = 18;
constexpr int kTitleBarButtonIconSize = 12;
constexpr int kTitleBarTitleWidth = 144;
constexpr int kTitleBarTitleHeight = 24;
constexpr int kTitleBarSearchWidth = 360;
constexpr int kTitleBarSearchMinWidth = 180;
constexpr int kTitleBarSearchHeight = 28;
constexpr int kTitleBarToolTipGap = 4;
constexpr char kTitleBarButtonPressAnimationName[] = "galleryTitleBarButtonPressAnimation";
constexpr qreal kTitleBarButtonPressScale = 0.86;  // WinUI-like press depth for icon buttons. zh_CN: 仿 WinUI 的图标按钮按下缩放深度。

// WinUI-Gallery parity search: split the query on whitespace and require a title
// to contain every token (AND semantics) rather than matching the whole string as
// one substring, so multi-word queries like "split button" still reach SplitButton.
// zh_CN: 对齐 WinUI-Gallery 的搜索：按空白把查询拆成词元，标题需包含每个词元（AND 语义），
// 而非把整串当作单一子串匹配，使「split button」之类的多词查询仍能命中 SplitButton。
QStringList splitSearchTokens(const QString& query)
{
    return query.trimmed().split(QRegularExpression(QStringLiteral("\\s+")),
                                 Qt::SkipEmptyParts);
}

bool titleMatchesTokens(const QString& title, const QStringList& tokens)
{
    for (const QString& token : tokens) {
        if (!title.contains(token, Qt::CaseInsensitive))
            return false;
    }
    return true;
}

// Filter then rank like WinUI-Gallery: titles whose start matches the raw query
// come first, the rest follow alphabetically (case-insensitive).
// zh_CN: 仿 WinUI-Gallery 过滤后排序：以原始查询为前缀的标题优先，其余按字母序（忽略大小写）。
QStringList rankedSearchTitles(const QStringList& titles, const QString& query)
{
    const QString needle = query.trimmed();
    const QStringList tokens = splitSearchTokens(needle);
    if (tokens.isEmpty())
        return titles;

    QStringList matched;
    matched.reserve(titles.size());
    for (const QString& title : titles) {
        if (titleMatchesTokens(title, tokens))
            matched.append(title);
    }

    std::stable_sort(matched.begin(), matched.end(),
                     [&needle](const QString& a, const QString& b) {
                         const bool aPrefix = a.startsWith(needle, Qt::CaseInsensitive);
                         const bool bPrefix = b.startsWith(needle, Qt::CaseInsensitive);
                         if (aPrefix != bPrefix)
                             return aPrefix;  // prefix matches rank ahead
                         return a.compare(b, Qt::CaseInsensitive) < 0;
                     });
    return matched;
}

int titleBarLeadingOffset(const fluent::windowing::TitleBar* bar)
{
    if (!bar)
        return kTitleBarHorizontalMargin;
    return bar->systemReservedLeadingWidth() > 0
        ? bar->systemReservedLeadingWidth() + kTitleBarHorizontalMargin
        : kTitleBarHorizontalMargin;
}

// WinUI-style click feedback: the glyph quickly dips to ~0.86 scale and springs back,
// reading as a press without moving the button or disturbing the layout.
// zh_CN: 仿 WinUI 的点击反馈：字形快速缩小到约 0.86 再弹回，呈现按下感，
// 且不移动按钮、不影响布局。
void startTitleBarButtonPress(fluent::basicinput::Button* button)
{
    if (!button || !button->isEnabled())
        return;

    if (auto* currentAnimation = button->findChild<QPropertyAnimation*>(QString::fromLatin1(kTitleBarButtonPressAnimationName))) {
        currentAnimation->stop();
        currentAnimation->deleteLater();
    }

    button->setIconScale(1.0);
    auto* animation = new QPropertyAnimation(button, "iconScale", button);
    animation->setObjectName(QString::fromLatin1(kTitleBarButtonPressAnimationName));
    const auto motion = button->themeAnimation();
    animation->setDuration(motion.fast);
    animation->setEasingCurve(motion.decelerate);
    animation->setStartValue(1.0);
    animation->setKeyValueAt(0.4, kTitleBarButtonPressScale);
    animation->setEndValue(1.0);
    QObject::connect(animation, &QPropertyAnimation::finished,
                     button, [button]() {
                         button->setIconScale(1.0);
                     });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

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
    setMinimumSize(460, 500);

    createTitleBarContent();
    buildNavigationShell();
    buildContentPresenter();
    installSplashScreen();
    showInitialRouteContent();
    prewarmAllRoutes();
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

    const QStringList tokens = splitSearchTokens(needle);

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
        if (!titleMatchesTokens(item.title, tokens))
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
    setTitleBarChromeVisible(false);

    // Cover the content area with the splash from the very first frame; the real title
    // bar stays live above it. zh_CN: 从第一帧起用 splash 盖住内容区；真实标题栏仍在其上方可用。
    m_splashScreen = new GallerySplashScreen(contentHost());
    m_splashScreen->show();
    m_splashScreen->raise();

    connect(m_contentPresenter, &GalleryContentPresenter::prewarmProgress,
            this, [this](int done, int total) {
                if (m_splashScreen)
                    m_splashScreen->setProgress(done, total);
            });
    connect(m_contentPresenter, &GalleryContentPresenter::prewarmFinished,
            this, [this]() {
                setTitleBarChromeVisible(true, /*animated*/ true);
                // Startup loading is done and the window has composited many frames, so we're well
                // past the DWM first-show race that can leave Mica un-applied (a flat neutral
                // surface until the next activation). Force the backdrop on now, as the content
                // surfaces from behind the splash. zh_CN: 启动加载已完成、窗口已合成多帧，已远过那个可能让
                // Mica 未生效（停在扁平中性表面、要等下次激活）的 DWM 首屏竞争；在内容从 splash 后浮现时强制施加背景。
                reapplySystemBackdrop();
                if (m_splashScreen)
                    m_splashScreen->dismiss();  // fades out, then self-deletes
            });
}

void GalleryWindow::setTitleBarChromeVisible(bool visible, bool animated)
{
    // The custom title-bar widgets all share the "GalleryTitleBar." object-name prefix;
    // the native min/max/close buttons do not, so toggling by prefix leaves them alone.
    // zh_CN: 自定义标题栏控件都以 "GalleryTitleBar." 为对象名前缀；原生最小化/最大化/关闭按钮没有，
    // 因此按前缀切换不会影响它们。
    const auto* bar = titleBar();
    if (!bar)
        return;

    m_titleBarChromeVisible = visible;
    const QList<QWidget*> chrome = bar->findChildren<QWidget*>();
    const auto motion = themeAnimation();
    for (QWidget* widget : chrome) {
        if (!widget->objectName().startsWith(QStringLiteral("GalleryTitleBar.")))
            continue;

        if (!visible) {
            widget->setGraphicsEffect(nullptr);
            widget->setVisible(false);
            continue;
        }

        widget->setVisible(true);
        if (!animated) {
            widget->setGraphicsEffect(nullptr);
            continue;
        }

        // Subtle fade-in, timed to the splash's dissolve, so the chrome eases in rather
        // than popping. The effect is dropped once the fade completes so it adds no
        // steady-state paint cost. zh_CN: 简约淡入，与 splash 淡出同步，组件渐入而非生硬弹出；
        // 淡入结束即移除效果，不留常态绘制开销。
        auto* effect = new QGraphicsOpacityEffect(widget);
        widget->setGraphicsEffect(effect);
        auto* fade = new QPropertyAnimation(effect, "opacity", widget);
        fade->setStartValue(0.0);
        fade->setEndValue(1.0);
        fade->setDuration(motion.normal);
        fade->setEasingCurve(motion.decelerate);
        QPointer<QWidget> guard = widget;
        connect(fade, &QPropertyAnimation::finished, widget, [guard]() {
            if (guard)
                guard->setGraphicsEffect(nullptr);
        });
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    }

    // setVisible(true) above un-hides every chrome widget uniformly; re-apply the adaptive
    // rules so the title/icon and search box land in their width-appropriate state.
    // zh_CN: 上面的 setVisible(true) 会统一显示所有 chrome 控件；重新套用自适应规则，
    // 使标题/图标与搜索框回到与宽度匹配的状态。
    updateTitleBarLayout();
}

void GalleryWindow::prewarmAllRoutes()
{
    if (!m_contentPresenter)
        return;

    // Hand every nav route to the presenter so it builds them up front (one per
    // event-loop tick) while a splash screen covers the work. After this drains, the
    // first visit to any page is a pure show/hide with no build jank.
    // zh_CN: 把每个导航路由交给 presenter 提前建好（每帧一个），由 splash 遮挡这段构建。
    // 排空后，任何页面的首次访问都是纯显示/隐藏、无建页卡顿。
    QStringList routeIds;
    if (m_mainNavigationPane)
        routeIds += m_mainNavigationPane->routeIds();
    if (m_footerNavigationPane)
        routeIds += m_footerNavigationPane->routeIds();
    m_contentPresenter->prewarmRoutes(routeIds);
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
    m_navigationView->setExpandedPaneWidth(240);  // Slightly tighter than 256 so content sits a bit closer to the menu items. zh_CN: 比 256 略紧凑，内容稍贴近菜单项。
    m_navigationView->setCompactPaneWidth(48);
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
                // 在隐藏的最小布局里去掉它们，给搜索框让位。updateTitleBarLayout() 也会按新的前导组宽度重排搜索框。
                updateTitleBarLayout();

                if (mode != DisplayMode::Left) {
                    if (m_navigationCompactReleaseTimer)
                        m_navigationCompactReleaseTimer->stop();
                    setNavigationPanesCompact(true);
                    return;
                }

                if (!m_navigationView->isAnimationEnabled()
                    || m_navigationView->property("layoutTransitionProgress").toDouble() >= 1.0) {
                    setNavigationPanesCompact(false);
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
    auto* bar = titleBar();
    auto* layout = qobject_cast<fluent::AnchorLayout*>(bar->layout());
    if (!layout)
        return;

    bar->setTitleBarHeight(kTitleBarHeight);

    m_backButton = new fluent::basicinput::Button(bar);
    auto* backButton = m_backButton;
    backButton->setObjectName(QStringLiteral("GalleryTitleBar.BackButton"));
    backButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    backButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    backButton->setFluentSize(fluent::basicinput::Button::Small);
    backButton->setFont(backButton->themeFont(Typography::FontRole::Caption).toQFont());
    backButton->setIconGlyph(Typography::Icons::TitleBarBack, kTitleBarButtonIconSize);
    // Height is fixed; the width is driven by the reveal animation (0 when there is no
    // history, kTitleBarButtonSize once back navigation is available).
    // zh_CN: 高度固定，宽度由展开动画驱动（无历史时为 0，可后退时为 kTitleBarButtonSize）。
    backButton->setFixedHeight(kTitleBarButtonSize);
    backButton->setFocusPolicy(Qt::NoFocus);
    backButton->setToolTip(QStringLiteral("Back"));
    backButton->setEnabled(false);
    backButton->installEventFilter(this);
    // Start faded out; the reveal animation drives contentOpacity alongside the width collapse
    // so showing/hiding reads as a smooth slide-in rather than a hard pop. zh_CN: 初始淡出；展开动画
    // 同时驱动 contentOpacity 与宽度收展，使显隐呈现顺滑滑入而非硬切。
    backButton->setContentOpacity(0.0);
    connect(backButton, &fluent::basicinput::Button::clicked,
            this, [this]() {
                navigateBack();
            });

    m_menuButton = new fluent::basicinput::Button(bar);
    auto* menuButton = m_menuButton;
    menuButton->setObjectName(QStringLiteral("GalleryTitleBar.MenuButton"));
    menuButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    menuButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    menuButton->setFluentSize(fluent::basicinput::Button::Small);
    menuButton->setIconGlyph(Typography::Icons::GlobalNav, kTitleBarButtonIconSize);
    menuButton->setFixedSize(kTitleBarButtonSize, kTitleBarButtonSize);
    menuButton->setFocusPolicy(Qt::NoFocus);
    menuButton->setToolTip(QStringLiteral("Toggle navigation pane"));
    menuButton->setEnabled(false);
    menuButton->installEventFilter(this);
    connect(menuButton, &fluent::basicinput::Button::clicked,
            this, [this]() {
                toggleNavigationDisplayMode();
            });

    auto* appIcon = new QLabel(bar);
    m_titleBarAppIcon = appIcon;
    appIcon->setObjectName(QStringLiteral("GalleryTitleBar.AppIcon"));
    appIcon->setAlignment(Qt::AlignCenter);
    appIcon->setFixedSize(kTitleBarIconSize, kTitleBarIconSize);
    appIcon->setPixmap(appicon::pixmap(kTitleBarIconSize, devicePixelRatioF()));

    auto* title = new fluent::textfields::Label(QStringLiteral("WinUI 3 Gallery"), bar);
    m_titleBarTitle = title;
    title->setObjectName(QStringLiteral("GalleryTitleBar.Title"));
    title->setFluentTypography(Typography::FontRole::Caption);
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    title->setFixedSize(kTitleBarTitleWidth, kTitleBarTitleHeight);

    fluent::AnchorLayout::Anchors backAnchors;
    backAnchors.left = {bar, Edge::Left, titleBarLeadingOffset(bar)};
    backAnchors.verticalCenter = {bar, Edge::VCenter, 0};
    layout->addAnchoredWidget(backButton, backAnchors);

    connect(bar, &fluent::windowing::TitleBar::systemReservedLeadingWidthChanged,
            backButton, [backButton, layout](int newWidth) {
                backButton->anchors()->left.offset = newWidth > 0
                    ? newWidth + kTitleBarHorizontalMargin
                    : kTitleBarHorizontalMargin;
                layout->invalidate();
            });

    fluent::AnchorLayout::Anchors menuAnchors;
    menuAnchors.left = {backButton, Edge::Right, kTitleBarItemGap};
    menuAnchors.verticalCenter = {bar, Edge::VCenter, 0};
    layout->addAnchoredWidget(menuButton, menuAnchors);

    fluent::AnchorLayout::Anchors appIconAnchors;
    appIconAnchors.left = {menuButton, Edge::Right, kTitleBarItemGap};
    appIconAnchors.verticalCenter = {bar, Edge::VCenter, 0};
    layout->addAnchoredWidget(appIcon, appIconAnchors);

    fluent::AnchorLayout::Anchors titleAnchors;
    titleAnchors.left = {appIcon, Edge::Right, kTitleBarItemGap};
    titleAnchors.verticalCenter = {bar, Edge::VCenter, 0};
    layout->addAnchoredWidget(title, titleAnchors);

    auto* searchBox = new fluent::textfields::AutoSuggestBox(bar);
    m_searchBox = searchBox;
    searchBox->setObjectName(QStringLiteral("GalleryTitleBar.SearchBox"));
    searchBox->setPlaceholderText(QStringLiteral("Search controls and samples..."));
    // Suggest every navigable title so search reaches the same routes as the pane.
    // zh_CN: 把所有可导航标题作为建议项，搜索可达范围与导航栏一致。
    QStringList searchTitles;
    for (const GalleryNavigationItem& item : m_navigationViewModel.items()) {
        if (item.kind != GalleryNavigationItem::Kind::SectionHeader)
            searchTitles.append(item.title);
    }
    searchTitles.sort(Qt::CaseInsensitive);
    searchBox->setSuggestions(searchTitles);
    searchBox->setInputHeight(kTitleBarSearchHeight);
    searchBox->setQueryButtonSize(24);
    searchBox->setClearButtonSize(24);
    // Height is fixed; width is driven by updateTitleBarLayout() so the box can shrink to
    // fit the free span between the leading group and the native caption controls.
    // zh_CN: 高度固定，宽度由 updateTitleBarLayout() 驱动，使其能收缩以适配前导组与原生标题栏控件之间的空闲区间。
    searchBox->setFixedHeight(kTitleBarSearchHeight);
    // AutoSuggestBox leaves filtering to the owner (WinUI semantics), so narrow
    // the suggestion list with token-AND matching and prefix-first ranking.
    // zh_CN: AutoSuggestBox 把过滤交给使用方（WinUI 语义），按词元 AND 匹配并前缀优先排序。
    connect(searchBox, &fluent::textfields::AutoSuggestBox::textChangedWithReason,
            searchBox,
            [searchBox, searchTitles](const QString& text,
                                      fluent::textfields::AutoSuggestBox::TextChangeReason reason) {
                if (reason != fluent::textfields::AutoSuggestBox::TextChangeReason::UserInput)
                    return;
                searchBox->setSuggestions(rankedSearchTitles(searchTitles, text));
            });
    connect(searchBox, &fluent::textfields::AutoSuggestBox::querySubmitted,
            this, [this](const QString& queryText, const QVariant& chosenSuggestion) {
                const QString chosen = chosenSuggestion.toString();
                navigateToSearchResult(chosen.isEmpty() ? queryText : chosen);
            });
    connect(searchBox, &fluent::textfields::AutoSuggestBox::suggestionChosen,
            this, [this](const QVariant& item) {
                navigateToSearchResult(item.toString());
            });

    // The search box is intentionally left out of the AnchorLayout: it needs a max width,
    // centering-with-clamp, and collapse behaviour the anchor model can't express, so
    // updateTitleBarLayout() positions it manually against the live bar width.
    // zh_CN: 搜索框刻意不加入 AnchorLayout：它需要最大宽度、居中并夹取、以及锚点模型无法表达的收起行为，
    // 故由 updateTitleBarLayout() 依据实时栏宽手动定位。
    bar->installEventFilter(this);
    // Start fully collapsed: no history at launch, so width / opacity / menu gap all read 0.
    // updateNavigationCommands() later animates it open the first time a route pushes history.
    // zh_CN: 启动时完全收起：尚无历史，故宽度/不透明度/菜单间隙均为 0；首次入栈历史时由
    // updateNavigationCommands() 动画展开。
    applyBackButtonReveal(0.0);
    updateTitleBarLayout();

    LOG_TRACE(QStringLiteral("GalleryWindow titleBarContent built searchWidth=%1 searchHeight=%2 buttonSize=%3")
                  .arg(kTitleBarSearchWidth)
                  .arg(kTitleBarSearchHeight)
                  .arg(kTitleBarButtonSize));
}

bool GalleryWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_backButton || watched == m_menuButton) {
        auto* button = qobject_cast<fluent::basicinput::Button*>(watched);
        switch (event->type()) {
        case QEvent::ToolTip:
            // Replace Qt's native tooltip with the Fluent ToolTip (reusing Qt's hover delay);
            // returning true suppresses the platform bubble. zh_CN: 用 Fluent ToolTip 替换原生提示
            //（复用 Qt 的悬停延迟），返回 true 抑制平台气泡。
            showTitleBarToolTip(button);
            return true;
        case QEvent::Leave:
        case QEvent::Hide:
            hideTitleBarToolTip();
            break;
        case QEvent::MouseButtonPress:
            startTitleBarButtonPress(button);
            hideTitleBarToolTip();
            break;
        default:
            break;
        }
    } else if (watched == titleBar() && event->type() == QEvent::Resize) {
        hideTitleBarToolTip();  // a width change can orphan a bubble that got no Leave event
        updateTitleBarLayout();
    }

    return fluent::windowing::Window::eventFilter(watched, event);
}

void GalleryWindow::updateTitleBarLayout()
{
    auto* bar = titleBar();
    if (!bar || !m_searchBox)
        return;

    using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
    const DisplayMode mode = m_navigationView ? m_navigationView->effectiveDisplayMode()
                                              : DisplayMode::Left;

    // In the minimal (hidden-pane) layout the app title+icon give way to the search box;
    // otherwise they show. Nothing shows while the splash owns the title bar.
    // zh_CN: 最小（隐藏窗格）布局下，应用标题+图标让位给搜索框；其余情况显示。splash 接管标题栏期间全部隐藏。
    const bool showTitle = m_titleBarChromeVisible && mode != DisplayMode::LeftMinimal;
    if (m_titleBarTitle)
        m_titleBarTitle->setVisible(showTitle);
    if (m_titleBarAppIcon)
        m_titleBarAppIcon->setVisible(showTitle);

    // Settle the left-anchored chain (back/menu/icon/title) before we publish hit-test rects.
    // zh_CN: 在发布命中测试区域前，先让左锚链（返回/菜单/图标/标题）落到最终几何。
    if (auto* barLayout = bar->layout())
        barLayout->activate();

    // Left boundary = right edge of the last visible leading widget + gap, derived from the
    // fixed metrics so it doesn't depend on layout timing. The back button only occupies space
    // proportional to its reveal progress, so the search box tracks the collapse/expand smoothly.
    // zh_CN: 左边界 = 最后一个可见前导控件的右缘 + 间隙，按固定度量推导，不依赖布局时序。返回按钮按其展开
    // 进度占位，故搜索框能随收展平滑跟随。
    const int backContribution =
        qRound(m_backButtonReveal * (kTitleBarButtonSize + kTitleBarItemGap));
    int leadingRight = titleBarLeadingOffset(bar)
                       + backContribution                          // back + its trailing gap, animated
                       + kTitleBarButtonSize;                      // menu
    if (showTitle) {
        leadingRight += kTitleBarItemGap + kTitleBarIconSize       // app icon
                        + kTitleBarItemGap + kTitleBarTitleWidth;  // title
    }
    const int leftBound = leadingRight + kTitleBarItemGap;
    const int rightBound = bar->width() - bar->systemReservedTrailingWidth() - kTitleBarHorizontalMargin;
    const int avail = rightBound - leftBound;

    const bool showSearch = m_titleBarChromeVisible && avail >= kTitleBarSearchMinWidth;
    m_searchBox->setVisible(showSearch);
    if (showSearch) {
        // Keep a small gap before the caption controls so the box never butts them.
        // zh_CN: 在窗口按钮前留一点空隙，避免搜索框顶到控件。
        constexpr int kSearchEdgeGap = 12;
        const int searchW = qBound(kTitleBarSearchMinWidth, avail - kSearchEdgeGap, kTitleBarSearchWidth);
        // Center it in the whole bar (WinUI feel) while it fits between the leading and trailing
        // groups; once the bar is narrow enough that a centered box would be pushed against the
        // caption controls, left-align it next to the leading group instead — a centered-then-
        // right-clamped box leaves an ugly gap on its left.
        // zh_CN: 有空间时在整条栏内居中（贴合 WinUI 观感）；栏一旦窄到居中会被顶向右侧窗口按钮，就改为靠左、
        // 紧挨前导组——居中后被右夹会在左侧留下难看的空隙。
        const int centeredX = (bar->width() - searchW) / 2;
        const int x = (centeredX >= leftBound && centeredX <= rightBound - searchW)
            ? centeredX
            : leftBound;
        const int y = (bar->height() - kTitleBarSearchHeight) / 2;
        m_searchBox->setGeometry(x, y, searchW, kTitleBarSearchHeight);
    }

    // The search box lives outside the AnchorLayout and title/icon visibility just changed —
    // republish the native hit-test regions so every control stays click-through.
    // zh_CN: 搜索框在 AnchorLayout 之外、标题/图标可见性刚变化——重新发布原生命中测试区域，保证各控件可点击。
    bar->refreshChromeExclusions();
}

void GalleryWindow::showTitleBarToolTip(fluent::basicinput::Button* button)
{
    if (!button)
        return;
    const QString text = button->toolTip();
    if (text.isEmpty())
        return;

    if (!m_titleBarToolTip) {
        m_titleBarToolTip = new fluent::status_info::ToolTip(nullptr);
        m_titleBarToolTip->setAnimationEnabled(true);
    }
    m_titleBarToolTip->setText(text);  // adjustSize() runs inside

    // Center the bubble under the button. The widget carries a transparent shadow band, so
    // offset by shadowMargin to land the visible bubble (not the band) at the gap below.
    // zh_CN: 把气泡居中显示在按钮下方。控件带有透明阴影带，故按 shadowMargin 偏移，使可见气泡（而非阴影带）
    // 落在按钮下方的间隙处。
    const int shadow = m_titleBarToolTip->shadowMargin();
    const QPoint anchor = button->mapToGlobal(QPoint(button->width() / 2, button->height()));
    const int x = anchor.x() - m_titleBarToolTip->width() / 2;
    const int y = anchor.y() + kTitleBarToolTipGap - shadow;
    m_titleBarToolTip->move(x, y);
    m_titleBarToolTip->show();
    m_titleBarToolTip->raise();
}

void GalleryWindow::hideTitleBarToolTip()
{
    if (m_titleBarToolTip)
        m_titleBarToolTip->hide();
}

void GalleryWindow::handleSelectedRouteChanged(const QString& routeId)
{
    LOG_TRACE(QStringLiteral("GalleryWindow selectedRouteSignal routeId=%1")
                  .arg(routeId));

    // Present synchronously: with pages prewarmed and resident in the stack, a swap is a
    // ~1ms show/hide, so there is nothing heavy to defer off this signal. navigateBack()
    // also relies on currentRouteId() being updated by the time this returns.
    // zh_CN: 同步换页：页面已预建并常驻栈中，换页只是 ~1ms 的显示/隐藏，没有重活需要从此信号上推迟；
    // navigateBack() 也依赖本函数返回时 currentRouteId() 已更新。
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

void GalleryWindow::toggleNavigationDisplayMode()
{
    if (!m_navigationView) {
        LOG_TRACE(QStringLiteral("GalleryWindow toggleNavigationDisplayMode skipped reason=missing-navigation-view"));
        return;
    }

    using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
    if (m_navigationCompactReleaseTimer)
        m_navigationCompactReleaseTimer->stop();

    // Keep the adaptive Auto mode and just open/close the pane. In the compact / minimal
    // modes opening overlays the pane with full labels; closing returns it to the icon
    // rail (or hidden). zh_CN: 保持自适应 Auto，仅开关窗格。紧凑/最小模式下打开时以全标签浮层覆盖，
    // 关闭则回到图标栏（或隐藏）。
    const bool open = !m_navigationView->isPaneOpen();
    m_navigationView->setPaneOpen(open);
    if (m_navigationView->effectiveDisplayMode() != DisplayMode::Left)
        setNavigationPanesCompact(!open);
    LOG_DEBUG(QStringLiteral("GalleryWindow navigationPaneToggled paneOpen=%1 effectiveMode=%2 chromeWidth=%3")
                  .arg(open ? QStringLiteral("true") : QStringLiteral("false"))
                  .arg(static_cast<int>(m_navigationView->effectiveDisplayMode()))
                  .arg(m_navigationView->chromeGeometry().width()));
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
    setBackButtonRevealed(!m_backRouteStack.isEmpty());
    if (m_menuButton)
        m_menuButton->setEnabled(m_navigationView != nullptr);
}

void GalleryWindow::applyBackButtonReveal(qreal reveal)
{
    m_backButtonReveal = reveal;
    if (m_backButton) {
        m_backButton->setFixedWidth(qRound(reveal * kTitleBarButtonSize));
        m_backButton->setContentOpacity(reveal);
    }
    if (m_menuButton && m_menuButton->anchors())
        m_menuButton->anchors()->left.offset = qRound(reveal * kTitleBarItemGap);
    if (auto* barLayout = titleBar() ? titleBar()->layout() : nullptr)
        barLayout->invalidate();
    updateTitleBarLayout();
}

void GalleryWindow::setBackButtonRevealed(bool revealed)
{
    if (!m_backButton || m_backButtonRevealed == revealed)
        return;
    m_backButtonRevealed = revealed;
    m_backButton->setEnabled(revealed);

    const auto motion = m_backButton->themeAnimation();
    if (!m_backButtonRevealAnimation) {
        m_backButtonRevealAnimation = new QVariantAnimation(this);
        m_backButtonRevealAnimation->setDuration(motion.normal);
        connect(m_backButtonRevealAnimation, &QVariantAnimation::valueChanged, this,
                [this](const QVariant& value) {
                    applyBackButtonReveal(value.toReal());
                });
    }
    m_backButtonRevealAnimation->stop();
    // Ease out on the way in (decisive arrival), the standard curve on the way out.
    // zh_CN: 入场用缓出（落点干脆），退场用标准曲线。
    m_backButtonRevealAnimation->setEasingCurve(revealed ? motion.decelerate : motion.standard);
    m_backButtonRevealAnimation->setStartValue(m_backButtonReveal);
    m_backButtonRevealAnimation->setEndValue(revealed ? 1.0 : 0.0);
    m_backButtonRevealAnimation->start();
}

} // namespace fluent::gallery
