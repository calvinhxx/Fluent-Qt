#include "GalleryWindow.h"

#include <QLabel>
#include <QPoint>

#include "components/basicinput/Button.h"
#include "components/foundation/QMLPlus.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/StackContentHost.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"
#include "components/windowing/TitleBar.h"
#include "design/Typography.h"
#include "AppIcon.h"
#include "GalleryNavigationPane.h"
#include "PlaceholderPage.h"
#include "SettingsPage.h"

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

int titleBarLeadingOffset(const fluent::windowing::TitleBar* bar)
{
    if (!bar)
        return kTitleBarHorizontalMargin;
    return bar->systemReservedLeadingWidth() > 0
        ? bar->systemReservedLeadingWidth() + kTitleBarHorizontalMargin
        : kTitleBarHorizontalMargin;
}

} // namespace

GalleryWindow::GalleryWindow(QWidget* parent)
    : fluent::windowing::Window(parent)
{
    setObjectName(QStringLiteral("galleryWindow"));
    setWindowTitle(QStringLiteral("WinUI 3 Gallery"));
    setWindowIcon(appicon::icon());
    setMinimumSize(980, 640);

    buildTitleBarContent();
    buildNavigationShell();
    selectRoute(m_navigationViewModel.defaultRouteId());
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
    const GalleryNavigationItem* item = m_navigationViewModel.itemById(routeId);
    if (!item || !m_navigationView)
        return false;

    m_currentRouteId = routeId;
    syncPaneSelection();

    QWidget* page = routeId == QStringLiteral("settings")
        ? static_cast<QWidget*>(new SettingsPage(*item))
        : static_cast<QWidget*>(new PlaceholderPage(*item));
    auto* contentHost = m_navigationView->contentHost();
    if (contentHost->count() == 0) {
        contentHost->insertPage(0, page);
        contentHost->setCurrentIndex(0, 0, false);
    } else {
        QWidget* previousPage = contentHost->replacePage(0, page);
        delete previousPage;
        contentHost->setCurrentIndex(0, 0, false);
    }
    return true;
}

PlaceholderPage* GalleryWindow::currentPlaceholderPage() const
{
    if (!m_navigationView)
        return nullptr;
    return dynamic_cast<PlaceholderPage*>(m_navigationView->contentHost()->pageWidget(0));
}

SettingsPage* GalleryWindow::currentSettingsPage() const
{
    if (!m_navigationView)
        return nullptr;
    return dynamic_cast<SettingsPage*>(m_navigationView->contentHost()->pageWidget(0));
}

void GalleryWindow::buildTitleBarContent()
{
    createTitleBarContent();
}

void GalleryWindow::buildNavigationShell()
{
    m_navigationView = new fluent::navigation::NavigationView(this);
    m_navigationView->setObjectName(QStringLiteral("galleryNavigationView"));
    m_navigationView->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Left);
    m_navigationView->setExpandedPaneWidth(272);
    m_navigationView->setCompactPaneWidth(48);
    m_navigationView->setAnimationEnabled(false);

    m_mainNavigationPane = new GalleryNavigationPane(m_navigationViewModel.mainPaneItems(), m_navigationView);
    m_mainNavigationPane->setObjectName(QStringLiteral("galleryMainNavigationPane"));
    m_footerNavigationPane = new GalleryNavigationPane(m_navigationViewModel.footerPaneItems(), m_navigationView);
    m_footerNavigationPane->setObjectName(QStringLiteral("galleryFooterNavigationPane"));

    connect(m_mainNavigationPane, &GalleryNavigationPane::routeActivated,
            this, &GalleryWindow::selectRoute);
    connect(m_footerNavigationPane, &GalleryNavigationPane::routeActivated,
            this, &GalleryWindow::selectRoute);

    m_navigationView->setMainChromeWidget(m_mainNavigationPane);
    m_navigationView->setFooterChromeWidget(m_footerNavigationPane);
    setContentWidget(m_navigationView);
}

void GalleryWindow::syncPaneSelection()
{
    if (m_mainNavigationPane)
        m_mainNavigationPane->setSelectedRouteId(m_currentRouteId);
    if (m_footerNavigationPane)
        m_footerNavigationPane->setSelectedRouteId(m_currentRouteId);
}

void GalleryWindow::createTitleBarContent()
{
    auto* bar = titleBar();
    auto* layout = qobject_cast<fluent::AnchorLayout*>(bar->layout());
    if (!layout)
        return;

    auto* backButton = new fluent::basicinput::Button(bar);
    backButton->setObjectName(QStringLiteral("GalleryTitleBar.BackButton"));
    backButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    backButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    backButton->setFluentSize(fluent::basicinput::Button::Small);
    backButton->setFont(backButton->themeFont(Typography::FontRole::Caption).toQFont());
    backButton->setIconGlyph(Typography::Icons::TitleBarBack, kTitleBarButtonIconSize);
    backButton->setFixedSize(kTitleBarButtonSize, kTitleBarButtonSize);
    backButton->setFocusPolicy(Qt::NoFocus);

    auto* menuButton = new fluent::basicinput::Button(bar);
    menuButton->setObjectName(QStringLiteral("GalleryTitleBar.MenuButton"));
    menuButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    menuButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    menuButton->setFluentSize(fluent::basicinput::Button::Small);
    menuButton->setIconGlyph(Typography::Icons::GlobalNav, kTitleBarButtonIconSize);
    menuButton->setFixedSize(kTitleBarButtonSize, kTitleBarButtonSize);
    menuButton->setFocusPolicy(Qt::NoFocus);

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
    menuAnchors.left = {backButton, Edge::Right, kTitleBarItemGap/2};
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
    searchBox->setObjectName(QStringLiteral("GalleryTitleBar.SearchBox"));
    searchBox->setPlaceholderText(QStringLiteral("Search controls and samples..."));
    searchBox->setSuggestions(QStringList{
        QStringLiteral("Button"),
        QStringLiteral("NavigationView"),
        QStringLiteral("AutoSuggestBox"),
        QStringLiteral("Settings")
    });
    searchBox->setInputHeight(kTitleBarSearchHeight);
    searchBox->setQueryButtonSize(24);
    searchBox->setClearButtonSize(24);
    searchBox->setFixedSize(kTitleBarSearchWidth, kTitleBarSearchHeight);

    fluent::AnchorLayout::Anchors searchAnchors;
    searchAnchors.horizontalCenter = {bar, Edge::HCenter, 0};
    searchAnchors.verticalCenter = {bar, Edge::VCenter, 0};
    layout->addAnchoredWidget(searchBox, searchAnchors);
}

} // namespace fluent::gallery
