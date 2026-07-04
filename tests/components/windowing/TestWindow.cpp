#include <gtest/gtest.h>

#include <QApplication>
#include <QColor>
#include <QDirIterator>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QWindow>

#include "compatibility/WindowChromeCompat.h"
#include "design/Breakpoints.h"
#include "design/Typography.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/basicinput/Button.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"
#include "components/windowing/TitleBar.h"
#include "components/windowing/Window.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using compatibility::WindowChromeCompat;
using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::textfields::AutoSuggestBox;
using fluent::textfields::Label;
using fluent::windowing::TitleBar;
using fluent::windowing::Window;

namespace {

using Edge = AnchorLayout::Edge;

constexpr int TitleBarIconSize = 24;
constexpr int TitleBarAppIconSize = 14;
constexpr int TitleBarAvatarSize = 24;
constexpr int TitleBarSearchWidth = 220;
constexpr int TitleBarSearchHeight = 28;

struct WindowVisualLauncher {
    QWidget* window = nullptr;
    Button* showButton = nullptr;
};

Button* createTitleBarIconButton(const QString& glyph, QWidget* parent) {
    auto* button = new Button(parent);
    button->setFluentStyle(Button::Subtle);
    button->setFluentLayout(Button::IconOnly);
    button->setFluentSize(Button::Small);
    button->setIconGlyph(glyph, 12);
    button->setFixedSize(TitleBarIconSize, TitleBarIconSize);
    return button;
}

QIcon createWindowAppIcon() {
    const qreal dpr = qApp && qApp->primaryScreen()
                          ? qApp->primaryScreen()->devicePixelRatio()
                          : qreal(1);

    QPixmap pixmap(QSize(qRound(TitleBarAppIconSize * dpr), qRound(TitleBarAppIconSize * dpr)));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#0078D4"));
    painter.drawRoundedRect(QRectF(0.75, 0.75, TitleBarAppIconSize - 1.5, TitleBarAppIconSize - 1.5), 2.5, 2.5);

    QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
    iconFont.setPixelSize(8);
    painter.setFont(iconFont);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(0, 0, TitleBarAppIconSize, TitleBarAppIconSize),
                     Qt::AlignCenter,
                     Typography::Icons::AppIconDefault);

    return QIcon(pixmap);
}

void anchorFromLeft(AnchorLayout* layout,
                    QWidget* widget,
                    TitleBar* titleBar,
                    QWidget* target,
                    Edge edge,
                    int offset) {
    AnchorLayout::Anchors anchors;
    anchors.left = {target, edge, offset};
    anchors.verticalCenter = {titleBar, Edge::VCenter, 0};
    layout->addAnchoredWidget(widget, anchors);
}

void anchorFromRight(AnchorLayout* layout,
                     QWidget* widget,
                     TitleBar* titleBar,
                     QWidget* target,
                     Edge edge,
                     int offset) {
    AnchorLayout::Anchors anchors;
    anchors.right = {target, edge, offset};
    anchors.verticalCenter = {titleBar, Edge::VCenter, 0};
    layout->addAnchoredWidget(widget, anchors);
}

Label* createTitleBarTitle(TitleBar* titleBar) {
    auto* title = new Label("Fluent Window", titleBar);
    title->setObjectName(QStringLiteral("titleBarWindowTitle"));
    title->setFluentTypography(Typography::FontRole::Caption);
    title->setStyleSheet("#titleBarWindowTitle { font-weight: 600; }");
    title->setFixedHeight(20);
    return title;
}

AutoSuggestBox* createTitleBarSearch(TitleBar* titleBar) {
    auto* search = new AutoSuggestBox(titleBar);
    search->setPlaceholderText("Search...");
    search->setSuggestions(QStringList{
        QStringLiteral("TitleBar"),
        QStringLiteral("WindowChromeCompat"),
        QStringLiteral("AutoSuggestBox")
    });
    search->setQueryIconVisible(false);
    search->setFontRole(Typography::FontRole::Caption);
    search->setSuggestionFontRole(Typography::FontRole::Caption);
    search->setSuggestionItemHeight(24);
    search->setInputHeight(TitleBarSearchHeight);
    search->setQueryButtonSize(16);
    search->setClearButtonSize(16);
    search->setFixedSize(TitleBarSearchWidth, TitleBarSearchHeight);
    return search;
}

Label* createTitleBarAvatar(TitleBar* titleBar) {
    auto* avatar = new Label("JD", titleBar);
    avatar->setObjectName(QStringLiteral("titleBarAvatar"));
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(TitleBarAvatarSize, TitleBarAvatarSize);
    avatar->setStyleSheet("#titleBarAvatar { background: #E1DFDD; color: #323130; border-radius: 12px; font-weight: 600; font-size: 11px; }");
    return avatar;
}

void createTitleBarContent(Window* window) {
    auto* titleBar = window->titleBar();
    auto* layout = qobject_cast<AnchorLayout*>(titleBar->layout());
    if (!layout)
        return;

    auto* appIcon = new Label(titleBar);
    appIcon->setFixedSize(TitleBarAppIconSize, TitleBarAppIconSize);
    appIcon->setPixmap(createWindowAppIcon().pixmap(QSize(TitleBarAppIconSize, TitleBarAppIconSize)));
    anchorFromLeft(layout, appIcon, titleBar, titleBar, Edge::Left, titleBar->systemReservedLeadingWidth() + 8);

    auto* pane = createTitleBarIconButton(Typography::Icons::GlobalNav, titleBar);
    anchorFromLeft(layout, pane, titleBar, appIcon, Edge::Right, 10);

    auto* back = createTitleBarIconButton(Typography::Icons::TitleBarBack, titleBar);
    anchorFromLeft(layout, back, titleBar, pane, Edge::Right, 2);

    auto* title = createTitleBarTitle(titleBar);
    anchorFromLeft(layout, title, titleBar, back, Edge::Right, 8);

    auto* search = createTitleBarSearch(titleBar);
    anchorFromLeft(layout, search, titleBar, title, Edge::Right, 12);

    auto* avatar = createTitleBarAvatar(titleBar);
    anchorFromRight(layout,
                    avatar,
                    titleBar,
                    titleBar,
                    Edge::Right,
                    -(titleBar->systemReservedTrailingWidth() + 8));

    auto* favorite = createTitleBarIconButton(Typography::Icons::FavoriteStar, titleBar);
    anchorFromRight(layout, favorite, titleBar, avatar, Edge::Left, -6);

    auto* share = createTitleBarIconButton(Typography::Icons::Share, titleBar);
    anchorFromRight(layout, share, titleBar, favorite, Edge::Left, -6);

    auto* link = createTitleBarIconButton(Typography::Icons::Link, titleBar);
    anchorFromRight(layout, link, titleBar, share, Edge::Left, -6);
}

QWidget* createWindowContent() {
    auto* content = new QWidget();
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(24, 28, 24, 24);
    contentLayout->setSpacing(16);

    auto* heading = new Label("Custom TitleBar with platform window behavior", content);
    heading->setFluentTypography(Typography::FontRole::Title);
    contentLayout->addWidget(heading);

    auto* body = new Label(
        "On macOS, native traffic lights stay on the left. On Windows, the caption buttons are drawn by Qt while DWM still provides resize, Snap, shadow, and maximize geometry.",
        content);
    body->setFluentTypography(Typography::FontRole::Body);
    body->setWordWrap(true);
    contentLayout->addWidget(body);
    contentLayout->addStretch(1);

    return content;
}

WindowVisualLauncher createWindowVisualLauncher() {
    WindowVisualLauncher ui;

    auto* launcher = new QWidget();
    launcher->setAttribute(Qt::WA_DeleteOnClose);
    launcher->setWindowTitle("Window VisualCheck");
    launcher->resize(280, 120);

    auto* root = new QVBoxLayout(launcher);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(0);

    auto* showButton = new Button("Show window", launcher);
    showButton->setIconGlyph(Typography::Icons::BackToWindow, Typography::FontSize::Body);
    showButton->setFluentLayout(Button::IconBefore);
    showButton->setFixedSize(150, 34);
    root->addWidget(showButton, 0, Qt::AlignCenter);

    ui.window = launcher;
    ui.showButton = showButton;
    return ui;
}

void showOrActivateVisualWindow(QPointer<Window>& appWindow, QWidget* launcher) {
    if (!appWindow) {
        appWindow = new Window();
        appWindow->setAttribute(Qt::WA_DeleteOnClose);
        appWindow->setWindowTitle("Fluent Window");
        createTitleBarContent(appWindow);
        appWindow->setContentWidget(createWindowContent());
    }

    appWindow->resize(860, 420);
    appWindow->move(launcher->geometry().center() - QPoint(430, 210));
    appWindow->show();
    appWindow->raise();
    appWindow->activateWindow();
}

} // namespace

class WindowTest : public ::testing::Test {
protected:
};

TEST_F(WindowTest, DefaultConstructionCreatesChromeAndContentHost) {
    Window window;

    ASSERT_NE(window.titleBar(), nullptr);
    ASSERT_NE(window.contentHost(), nullptr);
    EXPECT_TRUE(window.isWindow());
    EXPECT_GE(window.minimumWidth(), Breakpoints::MinWindowWidth);
    EXPECT_GE(window.minimumHeight(), Breakpoints::MinWindowHeight);
    EXPECT_EQ(window.titleBar()->titleBarHeight(), TitleBar::defaultTitleBarHeight());
    EXPECT_EQ(window.titleBar()->sizeHint().height(), window.titleBar()->titleBarHeight());
    EXPECT_EQ(window.graphicsEffect(), nullptr);
}

TEST_F(WindowTest, TitleBarHeightIsConfigurable) {
    TitleBar titleBar;
    QSignalSpy heightSpy(&titleBar, &TitleBar::titleBarHeightChanged);
    QSignalSpy chromeSpy(&titleBar, &TitleBar::chromeGeometryChanged);

    EXPECT_EQ(titleBar.titleBarHeight(), TitleBar::defaultTitleBarHeight());

    titleBar.setTitleBarHeight(40);
    EXPECT_EQ(titleBar.titleBarHeight(), 40);
    EXPECT_EQ(titleBar.height(), 40);
    EXPECT_EQ(titleBar.sizeHint().height(), 40);
    EXPECT_EQ(titleBar.minimumSizeHint().height(), 40);
    EXPECT_EQ(heightSpy.count(), 1);
    EXPECT_EQ(chromeSpy.count(), 1);

    titleBar.setTitleBarHeight(40);
    EXPECT_EQ(heightSpy.count(), 1);
    EXPECT_EQ(chromeSpy.count(), 1);

    titleBar.setTitleBarHeight(0);
    EXPECT_EQ(titleBar.titleBarHeight(), 1);
    EXPECT_EQ(titleBar.height(), 1);
    EXPECT_EQ(heightSpy.count(), 2);
    EXPECT_EQ(chromeSpy.count(), 2);
}

TEST_F(WindowTest, NativeMacModeUsesUnifiedTitleBar) {
    Window window;
    const bool customChrome = WindowChromeCompat::platformPrefersCustomWindowChrome();
    const bool macPlatform = (WindowChromeCompat::currentPlatform() == WindowChromeCompat::Platform::MacOS);
    const bool cocoaRuntime = QGuiApplication::platformName() == QStringLiteral("cocoa");

    if (macPlatform) {
        window.resize(520, 360);
        window.show();
        QApplication::processEvents();
        QApplication::processEvents();
    }

    EXPECT_EQ(window.titleBar()->isHidden(), !(customChrome || macPlatform));
    EXPECT_EQ(window.titleBar()->systemReservedLeadingWidth() > 0, macPlatform && cocoaRuntime);
#ifdef Q_OS_WIN
    EXPECT_GT(window.titleBar()->systemReservedTrailingWidth(), 0);
#else
    EXPECT_EQ(window.titleBar()->systemReservedTrailingWidth(), 0);
#endif

    if (macPlatform) {
        const bool expandedClientAreaHintsAvailable =
            WindowChromeCompat::expandedClientAreaHintsAvailable();
        EXPECT_EQ(WindowChromeCompat::windowHasExpandedClientAreaHint(&window),
                  expandedClientAreaHintsAvailable);
        EXPECT_EQ(WindowChromeCompat::windowHasNoTitleBarBackgroundHint(&window),
                  expandedClientAreaHintsAvailable);
        if (expandedClientAreaHintsAvailable)
            EXPECT_FALSE(window.testAttribute(Qt::WA_ContentsMarginsRespectsSafeArea));
    }
}

TEST_F(WindowTest, TopLevelShowSmoke) {
    Window window;
    window.resize(520, 560);
    window.show();
    QApplication::processEvents();

    EXPECT_TRUE(window.isWindow());
    EXPECT_TRUE(window.isVisible());

    window.close();
}

TEST_F(WindowTest, TranslucentBackdropClearsWindowBackingStore) {
    Window window;
    if (!window.testAttribute(Qt::WA_TranslucentBackground)
        || !window.property("fluentMicaBackdrop").toBool()) {
        GTEST_SKIP() << "System backdrop is unavailable on this Qt platform";
    }

    window.resize(320, 240);
    QImage image(window.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 255, 255));

    QPainter painter(&image);
    window.render(&painter, QPoint(), QRegion(), QWidget::DrawWindowBackground);
    painter.end();

    EXPECT_EQ(image.pixelColor(window.rect().center()).alpha(), 0)
        << "A translucent backdrop frame must replace stale backing-store pixels";
}

TEST_F(WindowTest, BackdropSwitchKeepsPlatformTranslucencyStable) {
    Window window;
    const bool platformTranslucent = window.testAttribute(Qt::WA_TranslucentBackground);

    window.setBackdropEffect(compatibility::BackdropEffect::Solid);
    EXPECT_FALSE(window.property("fluentMicaBackdrop").toBool());
    EXPECT_EQ(window.testAttribute(Qt::WA_TranslucentBackground), platformTranslucent);

    window.setBackdropEffect(compatibility::BackdropEffect::Mica);
    EXPECT_EQ(window.property("fluentMicaBackdrop").toBool(), platformTranslucent);
    EXPECT_EQ(window.testAttribute(Qt::WA_TranslucentBackground), platformTranslucent);

    window.setBackdropEffect(compatibility::BackdropEffect::Acrylic);
    EXPECT_EQ(window.property("fluentMicaBackdrop").toBool(), platformTranslucent);
    EXPECT_EQ(window.testAttribute(Qt::WA_TranslucentBackground), platformTranslucent);
}

TEST_F(WindowTest, WindowsCustomChromeSuppressesNativeCaption) {
#ifdef Q_OS_WIN
    Window window;
    window.resize(520, 360);
    window.show();
    QApplication::processEvents();

    EXPECT_EQ(WindowChromeCompat::windowHasExpandedClientAreaHint(&window),
              WindowChromeCompat::expandedClientAreaHintsAvailable());
    EXPECT_EQ(WindowChromeCompat::windowHasNoTitleBarBackgroundHint(&window),
              WindowChromeCompat::expandedClientAreaHintsAvailable());
    EXPECT_FALSE(window.windowFlags().testFlag(Qt::FramelessWindowHint));
    EXPECT_FALSE(window.windowFlags().testFlag(Qt::WindowMinimizeButtonHint));
    EXPECT_FALSE(window.windowFlags().testFlag(Qt::WindowMaximizeButtonHint));
    EXPECT_FALSE(window.windowFlags().testFlag(Qt::WindowCloseButtonHint));
    EXPECT_FALSE(window.windowFlags().testFlag(Qt::WindowTitleHint));
    EXPECT_FALSE(window.windowFlags().testFlag(Qt::WindowSystemMenuHint));
    EXPECT_FALSE(window.testAttribute(Qt::WA_ContentsMarginsRespectsSafeArea));
    EXPECT_GT(window.titleBar()->systemReservedTrailingWidth(), 0);
    EXPECT_NE(window.findChild<Button*>(QStringLiteral("fluentWindowCloseButton")), nullptr);
    ASSERT_NE(window.windowHandle(), nullptr);

    const QString platformName = QGuiApplication::platformName().toLower();
    if (platformName.contains(QStringLiteral("offscreen")) ||
        platformName.contains(QStringLiteral("minimal"))) {
        window.close();
        return;
    }

    const auto hwnd = reinterpret_cast<HWND>(window.windowHandle()->winId());
    ASSERT_NE(hwnd, nullptr);

    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    EXPECT_EQ(style & WS_CAPTION, 0);
    EXPECT_NE(style & WS_THICKFRAME, 0);
    EXPECT_NE(style & WS_SYSMENU, 0);
    EXPECT_NE(style & WS_MINIMIZEBOX, 0);
    EXPECT_NE(style & WS_MAXIMIZEBOX, 0);

    window.close();
#else
    GTEST_SKIP() << "Windows DWM caption style is only checked on Windows";
#endif
}

TEST_F(WindowTest, WindowsCustomTitleBarSharesNativeCaptionRow) {
#ifdef Q_OS_WIN
    const QString platformName = QGuiApplication::platformName().toLower();
    if (platformName.contains(QStringLiteral("offscreen")) ||
        platformName.contains(QStringLiteral("minimal"))) {
        GTEST_SKIP() << "Native titlebar geometry is not meaningful on this Qt platform";
    }

    Window window;
    window.resize(640, 420);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isVisible(), 1000);
    ASSERT_NE(window.windowHandle(), nullptr);
    ASSERT_NE(window.titleBar(), nullptr);
    QApplication::processEvents();

    QScreen* screen = window.screen() ? window.screen() : qApp->primaryScreen();
    ASSERT_NE(screen, nullptr);

    const int titleBarTop = window.titleBar()->mapToGlobal(QPoint(0, 0)).y();
    const int frameTop = window.frameGeometry().top();
    const int tolerance = qMax(18, qRound(10 * screen->devicePixelRatio()));

    EXPECT_LE(qAbs(titleBarTop - frameTop), tolerance)
        << "Custom TitleBar should occupy the native caption row instead of starting "
        << "below a separate Windows titlebar";

    window.close();
#else
    GTEST_SKIP() << "Windows native titlebar geometry is only checked on Windows";
#endif
}

TEST_F(WindowTest, WindowsWinUiLayoutUsesSelfDrawnCaptionButtons) {
#ifdef Q_OS_WIN
    Window window;
    window.setWindowTitle("Fluent Window");
    window.resize(640, 420);
    window.show();
    QApplication::processEvents();

    EXPECT_EQ(window.titleBar()->systemReservedLeadingWidth(), 0);
    EXPECT_GT(window.titleBar()->systemReservedTrailingWidth(), 0);
    EXPECT_EQ(window.windowTitle(), QStringLiteral("Fluent Window"));

    auto* minimizeButton = window.findChild<Button*>(QStringLiteral("fluentWindowMinimizeButton"));
    auto* maximizeButton = window.findChild<Button*>(QStringLiteral("fluentWindowMaximizeButton"));
    auto* closeButton = window.findChild<Button*>(QStringLiteral("fluentWindowCloseButton"));
    ASSERT_NE(minimizeButton, nullptr);
    ASSERT_NE(maximizeButton, nullptr);
    ASSERT_NE(closeButton, nullptr);
    EXPECT_TRUE(minimizeButton->isVisible());
    EXPECT_TRUE(maximizeButton->isVisible());
    EXPECT_TRUE(closeButton->isVisible());
    EXPECT_FALSE(minimizeButton->criticalOnHover());
    EXPECT_FALSE(maximizeButton->criticalOnHover());
    EXPECT_TRUE(closeButton->criticalOnHover());
    EXPECT_EQ(minimizeButton->cornerRadii(), QMargins(0, 0, 0, 0));
    EXPECT_EQ(maximizeButton->cornerRadii(), QMargins(0, 0, 0, 0));
    EXPECT_EQ(closeButton->cornerRadii(), QMargins(0, closeButton->themeRadius().control, 0, 0));
    EXPECT_EQ(window.titleBar()->systemReservedTrailingWidth(),
              minimizeButton->width() + maximizeButton->width() + closeButton->width());

    window.close();
#else
    GTEST_SKIP() << "Windows self-drawn caption buttons are only checked on Windows";
#endif
}

TEST_F(WindowTest, WindowsCloseCaptionButtonHoverUsesCriticalRed) {
#ifdef Q_OS_WIN
    Window window;
    window.resize(640, 420);
    window.show();
    QApplication::processEvents();

    auto* closeButton = window.findChild<Button*>(QStringLiteral("fluentWindowCloseButton"));
    ASSERT_NE(closeButton, nullptr);

    closeButton->setInteractionState(Button::Hover);
    QApplication::processEvents();

    const QImage image = closeButton->grab().toImage();
    ASSERT_FALSE(image.isNull());
    const QColor sample = image.pixelColor(qMin(4, image.width() - 1), image.height() / 2);

    EXPECT_GT(sample.red(), 150);
    EXPECT_LT(sample.green(), 80);
    EXPECT_LT(sample.blue(), 80);
    EXPECT_GT(sample.alpha(), 200);

    window.close();
#else
    GTEST_SKIP() << "Windows close caption button hover is only checked on Windows";
#endif
}

TEST_F(WindowTest, WindowsSelfDrawnCaptionButtonsDriveWindowSlots) {
#ifdef Q_OS_WIN
    Window window;
    window.resize(640, 420);
    window.show();
    QApplication::processEvents();

    auto* minimizeButton = window.findChild<Button*>(QStringLiteral("fluentWindowMinimizeButton"));
    auto* maximizeButton = window.findChild<Button*>(QStringLiteral("fluentWindowMaximizeButton"));
    auto* closeButton = window.findChild<Button*>(QStringLiteral("fluentWindowCloseButton"));
    ASSERT_NE(minimizeButton, nullptr);
    ASSERT_NE(maximizeButton, nullptr);
    ASSERT_NE(closeButton, nullptr);

    QSignalSpy minimizeSpy(&window, &Window::minimizeRequested);
    QSignalSpy maximizeSpy(&window, &Window::maximizeRequested);
    QSignalSpy restoreSpy(&window, &Window::restoreRequested);
    QSignalSpy closeSpy(&window, &Window::closeRequested);

    maximizeButton->click();
    EXPECT_EQ(maximizeSpy.count(), 1);

    maximizeButton->click();
    EXPECT_EQ(restoreSpy.count(), 1);

    minimizeButton->click();
    EXPECT_EQ(minimizeSpy.count(), 1);

    closeButton->click();
    EXPECT_EQ(closeSpy.count(), 1);

    window.close();
#else
    GTEST_SKIP() << "Windows self-drawn caption buttons are only checked on Windows";
#endif
}

TEST_F(WindowTest, WindowsSelfDrawnCaptionButtonsAreHitTestExclusions) {
#ifdef Q_OS_WIN
    Window window;
    window.resize(640, 420);
    window.show();
    QApplication::processEvents();

    auto* closeButton = window.findChild<Button*>(QStringLiteral("fluentWindowCloseButton"));
    ASSERT_NE(closeButton, nullptr);

    const QRect closeButtonRect(closeButton->mapTo(window.titleBar(), QPoint(0, 0)), closeButton->size());
    compatibility::WindowChromeOptions options;
    options.useCustomWindowChrome = true;
    options.titleBarRect = window.titleBar()->geometry();
    options.resizeBorderWidth = 8;
    options.dragExclusionRects = window.titleBar()->dragExclusionRects();

    EXPECT_EQ(WindowChromeCompat::classifyHitTest(options,
                                                  window.size(),
                                                  closeButtonRect.center()),
              WindowChromeCompat::HitTest::Client);
    EXPECT_EQ(WindowChromeCompat::classifyHitTest(options,
                                                  window.size(),
                                                  QPoint(120, window.titleBar()->titleBarHeight() / 2)),
              WindowChromeCompat::HitTest::Caption);

    window.close();
#else
    GTEST_SKIP() << "Windows self-drawn caption hit tests are only checked on Windows";
#endif
}

TEST_F(WindowTest, ContentWidgetInsertionAndReplacement) {
    Window window;
    auto* first = new Label("first");
    auto* second = new Label("second");

    window.setContentWidget(first);
    EXPECT_EQ(window.contentWidget(), first);
    EXPECT_EQ(first->parentWidget(), window.contentHost());
    ASSERT_NE(window.contentHost()->layout(), nullptr);
    EXPECT_EQ(window.contentHost()->layout()->count(), 1);

    window.setContentWidget(second);
    EXPECT_EQ(window.contentWidget(), second);
    EXPECT_EQ(second->parentWidget(), window.contentHost());
    EXPECT_EQ(first->parentWidget(), nullptr);
    EXPECT_EQ(window.contentHost()->layout()->count(), 1);

    delete first;
}

TEST_F(WindowTest, TitleBarHostsExternalContentAfterSystemArea) {
    TitleBar titleBar;
    titleBar.resize(720, titleBar.titleBarHeight());

    auto* anchorLayout = qobject_cast<AnchorLayout*>(titleBar.layout());
    ASSERT_NE(anchorLayout, nullptr);

    auto* title = new Label("Fluent Window", &titleBar);
    title->setFluentTypography(Typography::FontRole::Caption);
    title->setFixedSize(112, 24);
    auto* search = new AutoSuggestBox(&titleBar);
    search->setPlaceholderText("Search...");
    search->setQueryIconVisible(false);
    search->setFixedSize(160, 32);
    auto* action = new Button("Action", &titleBar);
    action->setFluentSize(Button::Small);
    action->setFixedSize(72, 28);

    using Edge = AnchorLayout::Edge;
    AnchorLayout::Anchors titleAnchors;
    titleAnchors.left = {&titleBar, Edge::Left, 86};
    titleAnchors.verticalCenter = {&titleBar, Edge::VCenter, 0};
    anchorLayout->addAnchoredWidget(title, titleAnchors);

    AnchorLayout::Anchors searchAnchors;
    searchAnchors.left = {title, Edge::Right, 12};
    searchAnchors.verticalCenter = {&titleBar, Edge::VCenter, 0};
    anchorLayout->addAnchoredWidget(search, searchAnchors);

    AnchorLayout::Anchors actionAnchors;
    actionAnchors.left = {search, Edge::Right, 8};
    actionAnchors.verticalCenter = {search, Edge::VCenter, 0};
    anchorLayout->addAnchoredWidget(action, actionAnchors);

    QSignalSpy reservedSpy(&titleBar, &TitleBar::systemReservedLeadingWidthChanged);
    titleBar.setSystemReservedLeadingWidth(78);
    titleBar.show();
    QApplication::processEvents();

    EXPECT_EQ(titleBar.systemReservedLeadingWidth(), 78);
    EXPECT_EQ(search->parentWidget(), titleBar.contentHost());
    EXPECT_GE(search->geometry().left(), titleBar.systemReservedLeadingWidth());
    EXPECT_GT(search->geometry().left(), title->geometry().right());
    EXPECT_EQ(search->geometry().center().y(), titleBar.rect().center().y());
    EXPECT_GT(action->geometry().left(), search->geometry().right());
    EXPECT_EQ(reservedSpy.count(), 1);
    EXPECT_GE(titleBar.dragExclusionRects().size(), 2);
}

TEST_F(WindowTest, TitleBarAutoSuggestClearButtonWorksWhilePopupOpen) {
    Window window;
    window.resize(860, 420);
    window.setWindowTitle("Fluent Window");
    createTitleBarContent(&window);
    window.setContentWidget(createWindowContent());
    window.show();
    QApplication::processEvents();

    auto* search = window.titleBar()->findChild<AutoSuggestBox*>();
    ASSERT_NE(search, nullptr);
    search->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();

    QTest::keyClicks(search, "asdasd");
    QApplication::processEvents();
    ASSERT_TRUE(search->isSuggestionListOpen());

    auto* clearButton = search->findChild<Button*>("AutoSuggestBoxClearButton");
    ASSERT_NE(clearButton, nullptr);
    ASSERT_TRUE(clearButton->isVisible());

    const QPoint clearCenterGlobal = clearButton->mapToGlobal(clearButton->rect().center());
    QWidget* hitWidget = QApplication::widgetAt(clearCenterGlobal);
    ASSERT_NE(hitWidget, nullptr);
    EXPECT_TRUE(hitWidget == clearButton || clearButton->isAncestorOf(hitWidget))
        << "TitleBar AutoSuggestBox popup must not cover the clear button";

    EXPECT_TRUE(search->isSuggestionListOpen());

    QTest::mouseClick(hitWidget, Qt::LeftButton, Qt::NoModifier,
                      hitWidget->mapFromGlobal(clearCenterGlobal));
    QApplication::processEvents();

    EXPECT_TRUE(search->text().isEmpty());
    EXPECT_TRUE(clearButton->isHidden());
    EXPECT_FALSE(search->isSuggestionListOpen());

    window.close();
}

TEST_F(WindowTest, TitleBarDoubleClickEmitsFromNonInteractiveArea) {
    TitleBar titleBar;
    titleBar.resize(720, titleBar.titleBarHeight());
    titleBar.show();
    QApplication::processEvents();

    QSignalSpy doubleClickSpy(&titleBar, &TitleBar::doubleClicked);
    QTest::mouseDClick(&titleBar, Qt::LeftButton, Qt::NoModifier, QPoint(320, titleBar.titleBarHeight() / 2));

    EXPECT_EQ(doubleClickSpy.count(), 1);
}

TEST_F(WindowTest, WindowTitleBarDoubleClickDoesNotCrash) {
    Window window;
    window.resize(640, 420);
    window.show();
    QApplication::processEvents();

    QTest::mouseDClick(window.titleBar(), Qt::LeftButton, Qt::NoModifier, QPoint(320, window.titleBar()->titleBarHeight() / 2));
    QApplication::processEvents();

    SUCCEED();
}

TEST_F(WindowTest, WindowsTitleBarDoubleClickTogglesNativeMaximizeRestore) {
#ifdef Q_OS_WIN
    const QString platformName = QGuiApplication::platformName().toLower();
    if (platformName.contains(QStringLiteral("offscreen")) ||
        platformName.contains(QStringLiteral("minimal"))) {
        GTEST_SKIP() << "Native titlebar double-click is not meaningful on this Qt platform";
    }

    Window window;
    window.resize(640, 420);
    window.show();
    QApplication::processEvents();

    QTest::mouseDClick(window.titleBar(),
                       Qt::LeftButton,
                       Qt::NoModifier,
                       QPoint(320, window.titleBar()->titleBarHeight() / 2));
    QTRY_VERIFY_WITH_TIMEOUT(window.isMaximized(), 3000);

    QTest::mouseDClick(window.titleBar(),
                       Qt::LeftButton,
                       Qt::NoModifier,
                       QPoint(320, window.titleBar()->titleBarHeight() / 2));
    QTRY_VERIFY_WITH_TIMEOUT(!window.isMaximized(), 3000);

    window.close();
#else
    GTEST_SKIP() << "Windows native titlebar double-click is only checked on Windows";
#endif
}

TEST_F(WindowTest, ThemeSwitchNoCrash) {
    Window window;

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    window.onThemeUpdated();
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    window.onThemeUpdated();

    SUCCEED();
}

TEST_F(WindowTest, LocalThemeOverrideRefreshesTitleBarAndContent) {
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);

    Window window;
    window.setBackdropEffect(compatibility::BackdropEffect::Solid);
    window.setProperty("fluentThemeOverride", static_cast<int>(fluent::FluentElement::Dark));

    auto* titleContent = new QWidget(window.titleBar());
    auto* titleLayout = new QHBoxLayout(titleContent);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    auto* title = new Label("Local title", titleContent);
    title->setFluentTypography(Typography::FontRole::Caption);
    titleLayout->addWidget(title);
    window.titleBar()->setContentWidget(titleContent);

    auto* body = new Label("Local body");
    body->setFluentTypography(Typography::FontRole::Body);
    window.setContentWidget(body);
    window.onThemeUpdated();

    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(window.effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(window.titleBar()->effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(title->effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(body->effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(title->palette().color(QPalette::WindowText), title->themeColors().textPrimary);
    EXPECT_EQ(body->palette().color(QPalette::WindowText), body->themeColors().textPrimary);
}

TEST_F(WindowTest, ExternalTitleBarActionsCanDriveWindowSlots) {
    Window window;
    auto* content = new QWidget();
    auto* layout = new QHBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* minimizeButton = new QPushButton("Minimize", content);
    auto* maximizeButton = new QPushButton("Maximize", content);
    auto* closeButton = new QPushButton("Close", content);
    layout->addWidget(minimizeButton);
    layout->addWidget(maximizeButton);
    layout->addWidget(closeButton);
    window.titleBar()->setContentWidget(content);

    QSignalSpy minimizeSpy(&window, &Window::minimizeRequested);
    QSignalSpy maximizeSpy(&window, &Window::maximizeRequested);
    QSignalSpy restoreSpy(&window, &Window::restoreRequested);
    QSignalSpy closeSpy(&window, &Window::closeRequested);

    QObject::connect(minimizeButton, &QPushButton::clicked, &window, &Window::minimizeWindow);
    QObject::connect(maximizeButton, &QPushButton::clicked, &window, &Window::toggleMaximizeRestore);
    QObject::connect(closeButton, &QPushButton::clicked, &window, &Window::closeWindow);

    minimizeButton->click();
    EXPECT_EQ(minimizeSpy.count(), 1);

    maximizeButton->click();
    EXPECT_EQ(maximizeSpy.count(), 1);

    maximizeButton->click();
    EXPECT_EQ(restoreSpy.count(), 1);

    closeButton->click();
    EXPECT_EQ(closeSpy.count(), 1);
}

TEST_F(WindowTest, TitleBarInteractiveChildrenCreateDragExclusions) {
    TitleBar titleBar;
    auto* content = new QWidget();
    auto* layout = new QHBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new Label("Label", content));
    layout->addWidget(new QPushButton("Search", content));
    titleBar.setContentWidget(content);
    titleBar.resize(500, titleBar.titleBarHeight());
    titleBar.show();
    QApplication::processEvents();

    const QVector<QRect> exclusions = titleBar.dragExclusionRects();
    EXPECT_FALSE(exclusions.isEmpty());
}

TEST_F(WindowTest, WindowingSourcesDoNotContainPlatformMacrosOrNativeHeaders) {
#ifndef PROJECT_SOURCE_DIR
#error PROJECT_SOURCE_DIR must be defined for this test target
#endif

    const QString sourceDir = QString::fromUtf8(PROJECT_SOURCE_DIR) + "/src/components/windowing";
    const QStringList forbidden = {
        "Q_OS_WIN",
        "Q_OS_MAC",
        "_WIN32",
        "__APPLE__",
        QStringLiteral("QT") + QStringLiteral("_VERSION") + QStringLiteral("_CHECK"),
        "windows.h",
        "dwmapi.h",
        "Cocoa",
        "AppKit"
    };

    QDirIterator it(sourceDir, QStringList() << "*.h" << "*.cpp",
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFile file(it.next());
        ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text))
            << file.fileName().toStdString();
        const QString text = QString::fromUtf8(file.readAll());
        for (const QString& pattern : forbidden) {
            EXPECT_FALSE(text.contains(pattern))
                << pattern.toStdString() << " found in " << file.fileName().toStdString();
        }
    }
}

TEST_F(WindowTest, WindowChromeCompatClassifiesHitTestAreas) {
    compatibility::WindowChromeOptions options;
    options.titleBarRect = QRect(0, 0, 500, 48);
    options.resizeBorderWidth = 8;
    options.dragExclusionRects << QRect(360, 0, 140, 32);

    EXPECT_EQ(WindowChromeCompat::classifyHitTest(options, QSize(500, 400), QPoint(2, 2)),
              WindowChromeCompat::HitTest::TopLeft);
    EXPECT_EQ(WindowChromeCompat::classifyHitTest(options, QSize(500, 400), QPoint(20, 20)),
              WindowChromeCompat::HitTest::Caption);
    EXPECT_EQ(WindowChromeCompat::classifyHitTest(options, QSize(500, 400), QPoint(380, 16)),
              WindowChromeCompat::HitTest::Client);
    EXPECT_EQ(WindowChromeCompat::classifyHitTest(options, QSize(500, 400), QPoint(498, 200)),
              WindowChromeCompat::HitTest::Right);
}

TEST_F(WindowTest, WindowChromeCompatFallbackIsSafe) {
    QWidget widget;
    WindowChromeCompat chrome(&widget);
    compatibility::FluentNativeEventResult result = 0;

    EXPECT_FALSE(chrome.handleNativeEvent(QByteArray(), nullptr, &result));
    EXPECT_FALSE(chrome.beginSystemMove(QPoint(0, 0)));
    EXPECT_FALSE(chrome.beginSystemResize(Qt::LeftEdge, QPoint(0, 0)));
    EXPECT_FALSE(chrome.beginSystemResize(Qt::Edges(), QPoint(0, 0)));
}

TEST_F(WindowTest, WindowChromeCompatRejectsIneligibleWidgets) {
    QWidget parent;
    QWidget child(&parent);
    parent.resize(240, 160);
    child.resize(120, 80);
    parent.show();
    child.show();
    QApplication::processEvents();

    WindowChromeCompat chrome(&child);

    EXPECT_FALSE(child.isWindow());
    EXPECT_FALSE(chrome.beginSystemMove(child.mapToGlobal(QPoint(10, 10))));
    EXPECT_FALSE(chrome.beginSystemResize(Qt::RightEdge, child.mapToGlobal(QPoint(119, 40))));

    parent.close();
}

TEST_F(WindowTest, WindowsHitTestClassificationSkippedOffWindows) {
#ifdef Q_OS_WIN
    compatibility::WindowChromeOptions options;
    options.titleBarRect = QRect(0, 0, 640, 48);
    options.resizeBorderWidth = 8;
    options.useCustomWindowChrome = true;
    options.dragExclusionRects << QRect(502, 0, 138, 32);

    EXPECT_EQ(WindowChromeCompat::classifyHitTest(options, QSize(640, 480), QPoint(100, 20)),
              WindowChromeCompat::HitTest::Caption);
    EXPECT_EQ(WindowChromeCompat::classifyHitTest(options, QSize(640, 480), QPoint(620, 16)),
              WindowChromeCompat::HitTest::Client);
#else
    GTEST_SKIP() << "Windows hit-test native classification is only required on Windows";
#endif
}

TEST_F(WindowTest, WindowsMaximizedCustomChromeFillsAvailableGeometry) {
#ifdef Q_OS_WIN
    if (!WindowChromeCompat::platformPrefersCustomWindowChrome())
        GTEST_SKIP() << "Windows custom chrome is disabled";

    const QString platformName = QGuiApplication::platformName().toLower();
    if (platformName.contains(QStringLiteral("offscreen")) ||
        platformName.contains(QStringLiteral("minimal"))) {
        GTEST_SKIP() << "Native maximize geometry is not meaningful on this Qt platform";
    }

    Window window;
    auto* content = new QWidget();
    window.setContentWidget(content);
    window.resize(720, 480);
    window.show();
    QApplication::processEvents();

    window.showMaximized();
    QTRY_VERIFY_WITH_TIMEOUT(window.isMaximized(), 3000);
    QApplication::processEvents();

    QScreen* screen = window.screen() ? window.screen() : qApp->primaryScreen();
    ASSERT_NE(screen, nullptr);

    const QRect available = screen->availableGeometry();
    const QRect geometry = window.geometry();
    const QRect frameGeometry = window.frameGeometry();
    const int tolerance = qMax(48, qRound(24 * screen->devicePixelRatio()));

    EXPECT_GE(geometry.width(), available.width() - tolerance * 2);
    EXPECT_GE(geometry.height(), available.height() - tolerance * 2);
    EXPECT_LE(qAbs(geometry.left() - frameGeometry.left()), tolerance);
    EXPECT_LE(qAbs(geometry.top() - frameGeometry.top()), tolerance);
    EXPECT_LE(qAbs(geometry.right() - frameGeometry.right()), tolerance);
    EXPECT_LE(qAbs(geometry.bottom() - frameGeometry.bottom()), tolerance);
    EXPECT_EQ(window.titleBar()->geometry().left(), 0);
    EXPECT_EQ(window.contentHost()->geometry().left(), 0);
    EXPECT_EQ(window.titleBar()->geometry().width(), window.width());
    EXPECT_EQ(window.contentHost()->geometry().width(), window.width());

    window.showNormal();
    QTRY_VERIFY_WITH_TIMEOUT(!window.isMaximized(), 3000);
    window.close();
#else
    GTEST_SKIP() << "Windows custom chrome maximize geometry is only required on Windows";
#endif
}

TEST_F(WindowTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    const WindowVisualLauncher launcher = createWindowVisualLauncher();
    QPointer<Window> appWindow;

    QObject::connect(launcher.showButton, &QPushButton::clicked, launcher.window, [&]() {
        showOrActivateVisualWindow(appWindow, launcher.window);
    });

    QObject::connect(launcher.window, &QObject::destroyed, qApp, [&]() {
        if (appWindow)
            appWindow->close();
        qApp->quit();
    });

    launcher.window->show();
    qApp->exec();
}
