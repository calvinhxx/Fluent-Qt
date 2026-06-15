#include "GalleryWindow.h"

#include <algorithm>

#include <QAbstractAnimation>
#include <QEvent>
#include <QLabel>
#include <QPoint>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QTimer>

#include "components/basicinput/Button.h"
#include "components/foundation/QMLPlus.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/StackContentHost.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"
#include "components/windowing/TitleBar.h"
#include "design/Typography.h"
#include "utils/Log.h"
#include "AppIcon.h"
#include "GalleryContentPresenter.h"
#include "GalleryNavigationPane.h"
#include "view/pages/GalleryContentPage.h"
#include "view/pages/PlaceholderPage.h"
#include "view/pages/SettingsPage.h"

namespace fluent::gallery {
namespace {

using Edge = fluent::AnchorLayout::Edge;

constexpr int kTitleBarHorizontalMargin = 8;
constexpr int kTitleBarItemGap = 8;
constexpr int kTitleBarButtonSize = 24;
constexpr int kTitleBarIconSize = 18;
constexpr int kTitleBarButtonIconSize = 12;
constexpr int kTitleBarTitleHeight = 24;
constexpr int kTitleBarSearchWidth = 360;
constexpr int kTitleBarSearchHeight = 28;
constexpr char kTitleBarIconJitterAnimationName[] = "galleryTitleBarIconJitterAnimation";

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

void startTitleBarIconJitter(fluent::basicinput::Button* button)
{
    if (!button || !button->isEnabled())
        return;

    if (auto* currentAnimation = button->findChild<QPropertyAnimation*>(QString::fromLatin1(kTitleBarIconJitterAnimationName))) {
        currentAnimation->stop();
        currentAnimation->deleteLater();
    }

    button->setIconOffset(QPoint(0, 0));
    auto* animation = new QPropertyAnimation(button, "iconOffset", button);
    animation->setObjectName(QString::fromLatin1(kTitleBarIconJitterAnimationName));
    const auto motion = button->themeAnimation();
    animation->setDuration(motion.normal);
    animation->setEasingCurve(motion.standard);
    animation->setStartValue(QPoint(0, 0));
    animation->setKeyValueAt(0.35, QPoint(-1, 0));
    animation->setKeyValueAt(0.65, QPoint(1, 0));
    animation->setEndValue(QPoint(0, 0));
    QObject::connect(animation, &QPropertyAnimation::finished,
                     button, [button]() {
                         button->setIconOffset(QPoint(0, 0));
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
    setMinimumSize(980, 640);

    createTitleBarContent();
    buildNavigationShell();
    buildContentPresenter();
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
    m_navigationView->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Left);
    m_navigationView->setExpandedPaneWidth(256);
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

    m_backButton = new fluent::basicinput::Button(bar);
    auto* backButton = m_backButton;
    backButton->setObjectName(QStringLiteral("GalleryTitleBar.BackButton"));
    backButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    backButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    backButton->setFluentSize(fluent::basicinput::Button::Small);
    backButton->setFont(backButton->themeFont(Typography::FontRole::Caption).toQFont());
    backButton->setIconGlyph(Typography::Icons::TitleBarBack, kTitleBarButtonIconSize);
    backButton->setFixedSize(kTitleBarButtonSize, kTitleBarButtonSize);
    backButton->setFocusPolicy(Qt::NoFocus);
    backButton->setToolTip(QStringLiteral("Back"));
    backButton->setEnabled(false);
    backButton->installEventFilter(this);
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
    appIcon->setObjectName(QStringLiteral("GalleryTitleBar.AppIcon"));
    appIcon->setAlignment(Qt::AlignCenter);
    appIcon->setFixedSize(kTitleBarIconSize, kTitleBarIconSize);
    appIcon->setPixmap(appicon::pixmap(kTitleBarIconSize, devicePixelRatioF()));

    auto* title = new fluent::textfields::Label(QStringLiteral("WinUI 3 Gallery"), bar);
    title->setObjectName(QStringLiteral("GalleryTitleBar.Title"));
    title->setFluentTypography(Typography::FontRole::Caption);
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    title->setFixedSize(144, kTitleBarTitleHeight);

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
    searchBox->setFixedSize(kTitleBarSearchWidth, kTitleBarSearchHeight);
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

    fluent::AnchorLayout::Anchors searchAnchors;
    searchAnchors.horizontalCenter = {bar, Edge::HCenter, 0};
    searchAnchors.verticalCenter = {bar, Edge::VCenter, 0};
    layout->addAnchoredWidget(searchBox, searchAnchors);

    LOG_TRACE(QStringLiteral("GalleryWindow titleBarContent built searchWidth=%1 searchHeight=%2 buttonSize=%3")
                  .arg(kTitleBarSearchWidth)
                  .arg(kTitleBarSearchHeight)
                  .arg(kTitleBarButtonSize));
}

bool GalleryWindow::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_backButton || watched == m_menuButton)
        && event->type() == QEvent::MouseButtonPress) {
        startTitleBarIconJitter(qobject_cast<fluent::basicinput::Button*>(watched));
    }

    return fluent::windowing::Window::eventFilter(watched, event);
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
    const bool expandPane = m_navigationView->displayMode() == DisplayMode::LeftCompact;
    const DisplayMode nextMode = expandPane ? DisplayMode::Left : DisplayMode::LeftCompact;
    if (!expandPane) {
        if (m_navigationCompactReleaseTimer)
            m_navigationCompactReleaseTimer->stop();
        setNavigationPanesCompact(true);
        m_navigationView->setPaneOpen(false);
        m_navigationView->setDisplayMode(nextMode);
    } else {
        if (m_navigationCompactReleaseTimer)
            m_navigationCompactReleaseTimer->stop();
        setNavigationPanesCompact(false);
        m_navigationView->setDisplayMode(nextMode);
        m_navigationView->setPaneOpen(true);
    }
    LOG_DEBUG(QStringLiteral("GalleryWindow navigationDisplayModeChanged mode=%1 paneOpen=%2 chromeWidth=%3")
                  .arg(expandPane ? QStringLiteral("Left") : QStringLiteral("LeftCompact"),
                       m_navigationView->isPaneOpen() ? QStringLiteral("true") : QStringLiteral("false"))
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
    if (m_backButton)
        m_backButton->setEnabled(!m_backRouteStack.isEmpty());
    if (m_menuButton)
        m_menuButton->setEnabled(m_navigationView != nullptr);
}

} // namespace fluent::gallery
