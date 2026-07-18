#include <gtest/gtest.h>

#include <QApplication>
#include <QColor>
#include <QDirIterator>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QWindow>

#include "compatibility/QtCompat.h"
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
#include "components/windowing/WindowBackdrop.h"
#include "components/windowing/WindowBackdropMaterial.h"
#include "components/windowing/WindowChromeFrame.h"

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
using fluent::windowing::ClientSideFrameEdgeOverlay;
using fluent::windowing::ClientSideFramePaintOptions;
using fluent::windowing::paintClientSideFrame;
using fluent::windowing::TitleBar;
using fluent::windowing::Window;
using fluent::windowing::BackdropBackend;
using fluent::windowing::BackdropCapabilities;
using fluent::windowing::BackdropEffect;
using fluent::windowing::BackdropFidelity;
using fluent::windowing::BackdropState;
using fluent::windowing::BackdropSurfaceMode;
using fluent::windowing::WindowBackdropMaterial;
using fluent::windowing::WindowBackdropMaterialOptions;

namespace {

using Edge = AnchorLayout::Edge;

constexpr int TitleBarIconSize = 24;
constexpr int TitleBarAppIconSize = 14;
constexpr int TitleBarAvatarSize = 24;
constexpr int TitleBarSearchWidth = 220;
constexpr int TitleBarSearchHeight = 28;

#ifdef Q_OS_WIN
class NativeEventTestWindow final : public Window {
public:
    bool dispatchNativeMessage(MSG* message,
                               compatibility::FluentNativeEventResult* result)
    {
        return nativeEvent(QByteArrayLiteral("windows_generic_MSG"), message, result);
    }
};
#endif

struct WindowVisualLauncher {
    QWidget* window = nullptr;
    Button* showButton = nullptr;
};

QImage renderBackdropMaterial(BackdropEffect effect, bool dark = false) {
    const QSize size(160, 96);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    WindowBackdropMaterialOptions options = WindowBackdropMaterialOptions::forTheme(
        dark,
        dark ? QColor(32, 32, 32) : QColor(243, 243, 243),
        QColor(0, 120, 212));
    options.effect = effect;
    options.active = true;
    options.devicePixelRatio = 1.0;

    QPainter painter(&image);
    WindowBackdropMaterial::paint(painter, QRectF(image.rect()), options);
    painter.end();
    return image;
}

bool imageIsFullyOpaque(const QImage& image) {
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).alpha() != 255)
                return false;
        }
    }
    return true;
}

int differingPixelCount(const QImage& first, const QImage& second) {
    if (first.size() != second.size())
        return -1;

    int differenceCount = 0;
    for (int y = 0; y < first.height(); ++y) {
        for (int x = 0; x < first.width(); ++x) {
            if (first.pixel(x, y) != second.pixel(x, y))
                ++differenceCount;
        }
    }
    return differenceCount;
}

class SquareSurfaceWidget : public QWidget {
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::white);
        painter.fillRect(QRect(width() - 24, 0, 24, 24), QColor(196, 43, 28));
    }
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

    QFont iconFont(Typography::FontFamily::FluentIcons);
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
    window->setCustomWindowChromeEnabled(true);
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

    const bool linuxPlatform = WindowChromeCompat::currentPlatform()
        == WindowChromeCompat::Platform::Linux;
    EXPECT_EQ(window.findChild<QWidget*>(QStringLiteral("fluentWindowFrameHost")) != nullptr,
              linuxPlatform);
    EXPECT_EQ(window.findChild<QWidget*>(QStringLiteral("fluentWindowFrameEdgeOverlay")) != nullptr,
              linuxPlatform);
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
    const bool selfDrawnCaptionButtons = customChrome && !macPlatform;
    EXPECT_EQ(window.titleBar()->systemReservedTrailingWidth() > 0, selfDrawnCaptionButtons);

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

TEST_F(WindowTest, TitleBarPublishesWindowActivationState) {
    TitleBar titleBar;
    QSignalSpy activeSpy(&titleBar, &TitleBar::windowActiveChanged);

    EXPECT_FALSE(titleBar.isWindowActive());

    QEvent activateEvent(QEvent::WindowActivate);
    QApplication::sendEvent(&titleBar, &activateEvent);
    EXPECT_TRUE(titleBar.isWindowActive());
    ASSERT_EQ(activeSpy.count(), 1);
    EXPECT_TRUE(activeSpy.takeFirst().at(0).toBool());

    QApplication::sendEvent(&titleBar, &activateEvent);
    EXPECT_EQ(activeSpy.count(), 0) << "Repeated activation must not publish a duplicate state";

    QEvent deactivateEvent(QEvent::WindowDeactivate);
    QApplication::sendEvent(&titleBar, &deactivateEvent);
    EXPECT_FALSE(titleBar.isWindowActive());
    ASSERT_EQ(activeSpy.count(), 1);
    EXPECT_FALSE(activeSpy.takeFirst().at(0).toBool());
}

TEST_F(WindowTest, ClientCaptionForegroundTracksWindowActivation) {
    Window window;
    auto* closeButton = window.findChild<Button*>(QStringLiteral("fluentWindowCloseButton"));
    if (!closeButton)
        GTEST_SKIP() << "The active platform uses native caption controls";

    QEvent activateEvent(QEvent::WindowActivate);
    QApplication::sendEvent(window.titleBar(), &activateEvent);
    EXPECT_DOUBLE_EQ(closeButton->contentOpacity(), 1.0);

    QEvent deactivateEvent(QEvent::WindowDeactivate);
    QApplication::sendEvent(window.titleBar(), &deactivateEvent);
    EXPECT_DOUBLE_EQ(closeButton->contentOpacity(), 0.55);

    QApplication::sendEvent(window.titleBar(), &activateEvent);
    EXPECT_DOUBLE_EQ(closeButton->contentOpacity(), 1.0);
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

TEST_F(WindowTest, BackdropBackingStoreMatchesResolvedSurface) {
    Window window;
#ifdef Q_OS_LINUX
    window.setCustomWindowChromeEnabled(true);
#endif
    window.resize(320, 240);
    window.show();
    QApplication::processEvents();

    const int clientFrameMargin = window.chromeFrameRect().left();
    const BackdropState state = window.backdropState();
    const bool hasBackdropSurface =
        state.surfaceMode == BackdropSurfaceMode::CompositedTransparent
        || clientFrameMargin > 0;
    if (!window.testAttribute(Qt::WA_TranslucentBackground)
        || !hasBackdropSurface) {
        SCOPED_TRACE(QStringLiteral("provider=%1 reason=%2")
                         .arg(window.backdropCapabilities().provider, state.reason)
                         .toStdString());
        QImage fallback(window.size(), QImage::Format_ARGB32_Premultiplied);
        fallback.fill(Qt::transparent);
        QPainter fallbackPainter(&fallback);
        window.render(&fallbackPainter, QPoint(), QRegion(), QWidget::DrawWindowBackground);
        fallbackPainter.end();
        EXPECT_EQ(state.backend, BackdropBackend::PaintedMaterial);
        EXPECT_EQ(state.surfaceMode, BackdropSurfaceMode::PaintedOpaque);
        EXPECT_EQ(fallback.pixelColor(window.rect().center()).alpha(), 255)
            << "A rejected/unavailable native backdrop must render an opaque material";
        window.close();
        return;
    }

    QImage image(window.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 255, 255));

    QPainter painter(&image);
    window.render(&painter, QPoint(), QRegion(), QWidget::DrawWindowBackground);
    painter.end();

    if (clientFrameMargin > 0) {
        EXPECT_LT(image.pixelColor(QPoint(clientFrameMargin / 2, clientFrameMargin / 2)).alpha(),
                  255)
            << "A Linux client-side frame must clear stale opaque pixels outside the rounded body";
        if (state.surfaceMode == BackdropSurfaceMode::CompositedTransparent) {
            EXPECT_LT(image.pixelColor(window.rect().center()).alpha(), 255)
                << "A Linux compositor backdrop tint must remain translucent";
        } else {
            EXPECT_EQ(image.pixelColor(window.rect().center()).alpha(), 255)
                << "Linux without compositor blur should keep the app-painted body opaque";
        }
    } else {
        EXPECT_EQ(image.pixelColor(window.rect().center()).alpha(), 0)
            << "A translucent backdrop frame must replace stale backing-store pixels";
    }

    // Verify every requested material against its per-effect capability. Win11
    // and macOS composite both; KWin blur composites Acrylic while Mica keeps
    // its stable painted semantics; Wayland/unsupported sessions paint both.
    const BackdropCapabilities capabilities = window.backdropCapabilities();
    for (BackdropEffect effect : {BackdropEffect::Mica, BackdropEffect::Acrylic}) {
        window.setBackdropEffect(effect);
        QApplication::processEvents();
        const BackdropState applied = window.backdropState();
        EXPECT_EQ(applied.requestedEffect, effect);
        EXPECT_EQ(applied.effectiveEffect, effect);
        if (capabilities.supportsTransparentMaterial(effect)) {
            EXPECT_EQ(applied.surfaceMode, BackdropSurfaceMode::CompositedTransparent)
                << applied.reason.toStdString();
            EXPECT_TRUE(applied.platformApplied) << applied.reason.toStdString();
        } else {
            EXPECT_EQ(applied.backend, BackdropBackend::PaintedMaterial);
            EXPECT_EQ(applied.fidelity, BackdropFidelity::Emulated);
            EXPECT_EQ(applied.surfaceMode, BackdropSurfaceMode::PaintedOpaque);
            EXPECT_FALSE(applied.platformApplied);
        }
    }

    window.close();
}

TEST_F(WindowTest, BackdropEffectSwitchesModes) {
    Window window;
    const bool platformTranslucent = window.testAttribute(Qt::WA_TranslucentBackground);
    EXPECT_TRUE(fluentMetaTypeNameIsRegistered("BackdropEffect"));
    EXPECT_TRUE(fluentMetaTypeNameIsRegistered("BackdropState"));
    QSignalSpy effectSpy(&window, &Window::backdropEffectChanged);
    QSignalSpy stateSpy(&window, &Window::backdropStateChanged);
    ASSERT_TRUE(effectSpy.isValid());
    ASSERT_TRUE(stateSpy.isValid());

    const auto expectState = [&](BackdropEffect effect,
                                 BackdropSurfaceMode expectedSurfaceMode) {
        window.setBackdropEffect(effect);
        const BackdropState state = window.backdropState();

        EXPECT_EQ(window.backdropEffect(), effect);
        EXPECT_EQ(state.requestedEffect, effect);
        EXPECT_EQ(state.effectiveEffect, effect);
        EXPECT_EQ(state.surfaceMode, expectedSurfaceMode);
        EXPECT_EQ(fluent::windowing::windowBackdropState(&window), state);
        EXPECT_EQ(window.property("fluentWindowBackdropEffect").toInt(),
                  static_cast<int>(state.requestedEffect));
        EXPECT_EQ(window.property("fluentMicaBackdrop").toBool(),
                  state.surfaceMode == BackdropSurfaceMode::CompositedTransparent);
        EXPECT_EQ(window.property("fluentBackdropSurfaceMode").toInt(),
                  static_cast<int>(state.surfaceMode));
        EXPECT_EQ(window.testAttribute(Qt::WA_TranslucentBackground), platformTranslucent);
    };

    expectState(BackdropEffect::Solid, BackdropSurfaceMode::SolidOpaque);
    expectState(BackdropEffect::Mica, BackdropSurfaceMode::PaintedOpaque);
    expectState(BackdropEffect::Acrylic, BackdropSurfaceMode::PaintedOpaque);
    window.setBackdropEffect(BackdropEffect::Acrylic);  // no-op must not notify
    EXPECT_EQ(effectSpy.count(), 3);
    EXPECT_EQ(stateSpy.count(), 3);
}

TEST_F(WindowTest, BackdropCapabilityHelpersResolvePerEffect) {
    BackdropCapabilities nativeMica;
    nativeMica.alphaSurfaceSupported = true;
    nativeMica.nativeMica = true;

    EXPECT_TRUE(nativeMica.supportsNative(BackdropEffect::Solid));
    EXPECT_TRUE(nativeMica.supportsNative(BackdropEffect::Mica));
    EXPECT_FALSE(nativeMica.supportsNative(BackdropEffect::Acrylic));
    EXPECT_FALSE(nativeMica.supportsTransparentMaterial(BackdropEffect::Solid));
    EXPECT_TRUE(nativeMica.supportsTransparentMaterial(BackdropEffect::Mica));
    EXPECT_FALSE(nativeMica.supportsTransparentMaterial(BackdropEffect::Acrylic));

    BackdropCapabilities compositor;
    compositor.alphaSurfaceSupported = true;
    compositor.compositorBlur = true;

    EXPECT_FALSE(compositor.supportsCompositor(BackdropEffect::Solid));
    EXPECT_FALSE(compositor.supportsCompositor(BackdropEffect::Mica));
    EXPECT_TRUE(compositor.supportsCompositor(BackdropEffect::Acrylic));
    EXPECT_FALSE(compositor.supportsTransparentMaterial(BackdropEffect::Mica));
    EXPECT_TRUE(compositor.supportsTransparentMaterial(BackdropEffect::Acrylic));

    compositor.alphaSurfaceSupported = false;
    EXPECT_FALSE(compositor.supportsTransparentMaterial(BackdropEffect::Mica));
    EXPECT_FALSE(compositor.supportsTransparentMaterial(BackdropEffect::Acrylic));
}

TEST_F(WindowTest, TypedBackdropStatePublishesToDescendants) {
    QWidget topLevel;
    QWidget child(&topLevel);

    BackdropState queried;
    EXPECT_FALSE(fluent::windowing::tryWindowBackdropState(&child, &queried));

    BackdropState state;
    state.requestedEffect = BackdropEffect::Acrylic;
    state.effectiveEffect = BackdropEffect::Acrylic;
    state.backend = BackdropBackend::LinuxCompositor;
    state.fidelity = BackdropFidelity::Composited;
    state.surfaceMode = BackdropSurfaceMode::CompositedTransparent;
    state.platformApplied = true;
    state.reason = QStringLiteral("unit-test-compositor");
    fluent::windowing::publishWindowBackdropState(&child, state);

    EXPECT_TRUE(fluent::windowing::tryWindowBackdropState(&child, &queried));
    EXPECT_EQ(queried, state);
    EXPECT_EQ(fluent::windowing::windowBackdropState(&topLevel), state);
    EXPECT_EQ(fluent::windowing::windowBackdropState(&child), state);
    EXPECT_TRUE(fluent::windowing::windowBackdropRequiresTransparentClear(&child));
    EXPECT_FALSE(fluent::windowing::windowBackdropUsesPaintedMaterial(&child));
    EXPECT_TRUE(fluent::windowing::windowHasMaterialBackdrop(&child));

    state.backend = BackdropBackend::PaintedMaterial;
    state.fidelity = BackdropFidelity::Emulated;
    state.surfaceMode = BackdropSurfaceMode::PaintedOpaque;
    state.platformApplied = false;
    state.reason = QStringLiteral("unit-test-painted");
    fluent::windowing::publishWindowBackdropState(&topLevel, state);

    EXPECT_EQ(fluent::windowing::windowBackdropState(&child), state);
    EXPECT_FALSE(fluent::windowing::windowBackdropRequiresTransparentClear(&child));
    EXPECT_TRUE(fluent::windowing::windowBackdropUsesPaintedMaterial(&child));
    EXPECT_TRUE(fluent::windowing::windowHasMaterialBackdrop(&child));
}

TEST_F(WindowTest, PaintedBackdropMaterialsAreOpaqueDistinctAndDeterministic) {
    for (bool dark : {false, true}) {
        const QImage solid = renderBackdropMaterial(BackdropEffect::Solid, dark);
        const QImage mica = renderBackdropMaterial(BackdropEffect::Mica, dark);
        const QImage acrylic = renderBackdropMaterial(BackdropEffect::Acrylic, dark);

        EXPECT_TRUE(imageIsFullyOpaque(solid));
        EXPECT_TRUE(imageIsFullyOpaque(mica));
        EXPECT_TRUE(imageIsFullyOpaque(acrylic));

        EXPECT_TRUE(solid == renderBackdropMaterial(BackdropEffect::Solid, dark));
        EXPECT_TRUE(mica == renderBackdropMaterial(BackdropEffect::Mica, dark));
        EXPECT_TRUE(acrylic == renderBackdropMaterial(BackdropEffect::Acrylic, dark));

        const int pixelCount = solid.width() * solid.height();
        EXPECT_GT(differingPixelCount(solid, mica), pixelCount / 4);
        EXPECT_GT(differingPixelCount(solid, acrylic), pixelCount / 4);
        EXPECT_GT(differingPixelCount(mica, acrylic), pixelCount / 4);
    }
}

TEST_F(WindowTest, PaintedAcrylicKeepsBroadAccentGlassField) {
    const QImage acrylic = renderBackdropMaterial(BackdropEffect::Acrylic, false);
    const QColor upperLeft = acrylic.pixelColor(12, 12);
    const QColor upperRight = acrylic.pixelColor(acrylic.width() - 12, 12);

    const int leftCoolChroma = upperLeft.blue() - upperLeft.red();
    const int rightCoolChroma = upperRight.blue() - upperRight.red();
    EXPECT_GT(rightCoolChroma, 24);
    EXPECT_GT(rightCoolChroma, leftCoolChroma + 10);
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

TEST_F(WindowTest, WindowsNativeHitTestRepairsLostResizeStyle) {
#ifdef Q_OS_WIN
    NativeEventTestWindow window;
    window.resize(520, 360);
    window.show();
    QApplication::processEvents();

    const QString platformName = QGuiApplication::platformName().toLower();
    if (platformName.contains(QStringLiteral("offscreen"))
        || platformName.contains(QStringLiteral("minimal"))) {
        window.close();
        GTEST_SKIP() << "Native Win32 hit testing requires the Windows platform plugin";
    }

    ASSERT_NE(window.windowHandle(), nullptr);
    const auto hwnd = reinterpret_cast<HWND>(window.windowHandle()->winId());
    ASSERT_NE(hwnd, nullptr);

    const LONG_PTR originalStyle = GetWindowLongPtrW(hwnd, GWL_STYLE);
    ASSERT_NE(originalStyle & WS_THICKFRAME, 0);
    SetWindowLongPtrW(hwnd, GWL_STYLE, originalStyle & ~WS_THICKFRAME);
    ASSERT_EQ(GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_THICKFRAME, 0);

    const QPoint globalEdge = window.mapToGlobal(QPoint(1, window.height() / 2));
    MSG message = {};
    message.hwnd = hwnd;
    message.message = WM_NCHITTEST;
    message.lParam = MAKELPARAM(globalEdge.x(), globalEdge.y());
    compatibility::FluentNativeEventResult result = HTCLIENT;

    EXPECT_TRUE(window.dispatchNativeMessage(&message, &result));
    EXPECT_EQ(result, HTLEFT);
    EXPECT_NE(GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_THICKFRAME, 0)
        << "Win10/Qt 6.2 must repair a resize style lost during a native transition";

    window.close();
#else
    GTEST_SKIP() << "Native Win32 resize-style recovery is only checked on Windows";
#endif
}

TEST_F(WindowTest, WindowsStateTransitionRepairsLostResizeStyle) {
#ifdef Q_OS_WIN
    Window window;
    window.resize(520, 360);
    window.show();
    QApplication::processEvents();

    const QString platformName = QGuiApplication::platformName().toLower();
    if (platformName.contains(QStringLiteral("offscreen"))
        || platformName.contains(QStringLiteral("minimal"))) {
        window.close();
        GTEST_SKIP() << "Native Win32 style recovery requires the Windows platform plugin";
    }

    ASSERT_NE(window.windowHandle(), nullptr);
    const auto hwnd = reinterpret_cast<HWND>(window.windowHandle()->winId());
    ASSERT_NE(hwnd, nullptr);

    const LONG_PTR originalStyle = GetWindowLongPtrW(hwnd, GWL_STYLE);
    ASSERT_NE(originalStyle & WS_THICKFRAME, 0);
    SetWindowLongPtrW(hwnd, GWL_STYLE, originalStyle & ~WS_THICKFRAME);
    ASSERT_EQ(GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_THICKFRAME, 0);

    QWindowStateChangeEvent stateChange(Qt::WindowNoState);
    QApplication::sendEvent(&window, &stateChange);
    QApplication::processEvents();

    EXPECT_NE(GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_THICKFRAME, 0)
        << "A completed window-state transition must re-assert the resize style";

    window.close();
#else
    GTEST_SKIP() << "Native Win32 resize-style recovery is only checked on Windows";
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

TEST_F(WindowTest, PrepareForNativeRestoreClearsCaptionButtonPointerState) {
    Window window;
    window.setCustomWindowChromeEnabled(true);
    window.resize(640, 420);
    window.show();
    QApplication::processEvents();

    auto* closeButton = window.findChild<Button*>(QStringLiteral("fluentWindowCloseButton"));
    if (!closeButton)
        GTEST_SKIP() << "The active platform uses native caption controls";

    closeButton->setDown(true);
    closeButton->setInteractionState(Button::Hover);
    closeButton->setAttribute(Qt::WA_UnderMouse, true);
    window.hide();

    window.prepareForNativeRestore();

    EXPECT_FALSE(closeButton->isDown());
    EXPECT_EQ(closeButton->interactionState(), Button::Rest);
    EXPECT_FALSE(closeButton->underMouse());
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
    QTRY_VERIFY_WITH_TIMEOUT(search->isSuggestionListOpen(), 1000);

    auto* clearButton = search->findChild<Button*>("AutoSuggestBoxClearButton");
    ASSERT_NE(clearButton, nullptr);
    ASSERT_TRUE(clearButton->isVisible());

    const QPoint clearCenterGlobal = clearButton->mapToGlobal(clearButton->rect().center());
    QWidget* hitWidget = QApplication::widgetAt(clearCenterGlobal);
#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC)
    if (!hitWidget
        && QGuiApplication::platformName().compare(QStringLiteral("xcb"),
                                                   Qt::CaseInsensitive) == 0) {
        hitWidget = clearButton;
    }
#endif
    ASSERT_NE(hitWidget, nullptr);
    EXPECT_TRUE(hitWidget == clearButton || clearButton->isAncestorOf(hitWidget))
        << "TitleBar AutoSuggestBox popup must not cover the clear button";

    EXPECT_TRUE(search->isSuggestionListOpen());

    QTest::mouseClick(hitWidget, Qt::LeftButton, Qt::NoModifier,
                      hitWidget->mapFromGlobal(clearCenterGlobal));
    QApplication::processEvents();

    EXPECT_TRUE(search->text().isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(clearButton->isHidden(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!search->isSuggestionListOpen(), 1000);

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

TEST_F(WindowTest, WindowChromeCompatLinuxIsFirstClassPlatform) {
#ifdef Q_OS_LINUX
    QWidget widget;
    WindowChromeCompat chrome(&widget);
    EXPECT_EQ(WindowChromeCompat::currentPlatform(), WindowChromeCompat::Platform::Linux);
    EXPECT_FALSE(WindowChromeCompat::platformPrefersCustomWindowChrome());
    EXPECT_EQ(chrome.clientSideFrameMargin(), 0);

    auto options = chrome.options();
    options.useCustomWindowChrome = true;
    chrome.configure(options);

    EXPECT_GT(chrome.clientSideFrameMargin(), 0);
#else
    GTEST_SKIP() << "Linux chrome routing is only asserted on Linux";
#endif
}

TEST_F(WindowTest, LinuxClientSideResizeInputCoversX11AndWayland) {
#ifdef Q_OS_LINUX
    Window window;
    window.setCustomWindowChromeEnabled(true);
    window.resize(640, 420);
    window.show();
    QApplication::processEvents();

    auto* frameEdgeOverlay = window.findChild<QWidget*>(QStringLiteral("fluentWindowFrameEdgeOverlay"));
    ASSERT_NE(frameEdgeOverlay, nullptr);
    ASSERT_TRUE(frameEdgeOverlay->isVisible());
    EXPECT_TRUE(frameEdgeOverlay->testAttribute(Qt::WA_TransparentForMouseEvents));
    EXPECT_TRUE(frameEdgeOverlay->mask().isEmpty());

    auto* bottomRightHitZone =
        window.findChild<QWidget*>(QStringLiteral("fluentWindowResizeHitZone7"));
    ASSERT_NE(bottomRightHitZone, nullptr);
    ASSERT_TRUE(bottomRightHitZone->isVisible());
    EXPECT_EQ(bottomRightHitZone->cursor().shape(), Qt::SizeFDiagCursor);
    EXPECT_TRUE(bottomRightHitZone->geometry().contains(
        QPoint(frameEdgeOverlay->width() - 2, frameEdgeOverlay->height() - 2)));

    auto* closeButton = window.findChild<Button*>(QStringLiteral("fluentWindowCloseButton"));
    ASSERT_NE(closeButton, nullptr);
    EXPECT_EQ(closeButton->cornerRadii(), QMargins(0, 0, 0, 0));
    window.close();
#else
    GTEST_SKIP() << "Linux client-side resize input is only asserted on Linux";
#endif
}

TEST_F(WindowTest, LinuxCloseCaptionHoverReachesRoundedOuterCorner) {
#ifdef Q_OS_LINUX
    Window window;
    window.setCustomWindowChromeEnabled(true);
    window.resize(640, 420);
    window.show();
    QApplication::processEvents();

    auto* closeButton = window.findChild<Button*>(QStringLiteral("fluentWindowCloseButton"));
    ASSERT_NE(closeButton, nullptr);
    closeButton->setInteractionState(Button::Hover);
    QApplication::processEvents();

    QImage image(window.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    window.render(&painter);
    painter.end();

    const QRect frame = window.chromeFrameRect();
    const int radius = qRound(qMax<qreal>(window.themeRadius().overlay,
                                         window.themeRadius().control));
    const QColor cornerHover = image.pixelColor(frame.right() - 1,
                                                 frame.top() + radius);
    EXPECT_GT(cornerHover.red(), cornerHover.green() * 2);
    EXPECT_GT(cornerHover.red(), cornerHover.blue() * 2);
    const int topLeftAlpha = image.pixelColor(frame.topLeft()).alpha();
    const int topRightAlpha = image.pixelColor(frame.topRight()).alpha();
    const int bottomLeftAlpha = image.pixelColor(frame.bottomLeft()).alpha();
    const int bottomRightAlpha = image.pixelColor(frame.bottomRight()).alpha();
    EXPECT_LT(topLeftAlpha, 128);
    EXPECT_LT(topRightAlpha, 128);
    EXPECT_LT(bottomLeftAlpha, 128);
    EXPECT_LT(bottomRightAlpha, 128);

    window.close();
#else
    GTEST_SKIP() << "Linux caption corner composition is only asserted on Linux";
#endif
}

TEST_F(WindowTest, ClientSideFrameRoutesMouseInputThroughEdgeZones) {
    QWidget host;
    host.resize(320, 240);
    ClientSideFrameEdgeOverlay overlay(&host);
    overlay.setGeometry(host.rect());
    overlay.setFrameVisualRect(QRect(16, 16, 288, 208));
    overlay.setFrameRadius(8.0);
    overlay.setResizeInputEnabled(true, 12);

    QWidget* routedSource = nullptr;
    QEvent::Type routedType = QEvent::None;
    overlay.setMouseEventHandler([&](QWidget* source, QMouseEvent* event) {
        routedSource = source;
        routedType = event ? event->type() : QEvent::None;
        return true;
    });

    host.show();
    overlay.show();
    QApplication::processEvents();
    auto* topEdge = host.findChild<QWidget*>(QStringLiteral("fluentWindowResizeHitZone0"));
    auto* rightEdge = host.findChild<QWidget*>(QStringLiteral("fluentWindowResizeHitZone3"));
    ASSERT_NE(topEdge, nullptr);
    ASSERT_NE(rightEdge, nullptr);
    ASSERT_TRUE(topEdge->isVisible());
    ASSERT_TRUE(rightEdge->isVisible());

    const QRect visualRect(16, 16, 288, 208);
    EXPECT_TRUE(topEdge->geometry().contains(QPoint(visualRect.center().x(), visualRect.top())));
    EXPECT_TRUE(rightEdge->geometry().contains(QPoint(visualRect.right(), visualRect.center().y())));
    EXPECT_TRUE(overlay.testAttribute(Qt::WA_TransparentForMouseEvents));
    EXPECT_TRUE(overlay.mask().isEmpty());

    QEvent enterRight(QEvent::Enter);
    QApplication::sendEvent(rightEdge, &enterRight);
    EXPECT_EQ(overlay.cursor().shape(), Qt::SizeHorCursor);
    EXPECT_EQ(host.cursor().shape(), Qt::SizeHorCursor);

    QTest::mousePress(topEdge, Qt::LeftButton, Qt::NoModifier, topEdge->rect().center());
    EXPECT_EQ(routedSource, topEdge);
    EXPECT_EQ(routedType, QEvent::MouseButtonPress);

    host.close();
}

TEST_F(WindowTest, ClientSideFrameCompositeKeepsPartialAlphaAtEveryCorner) {
    QImage image(QSize(64, 64), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    const QRect frameRect(12, 12, 40, 40);
    ClientSideFramePaintOptions options;
    options.windowRect = image.rect();
    options.frameRect = frameRect;
    options.fill = Qt::white;
    options.stroke = Qt::black;
    options.radius = 8.0;
    options.shadow = {0, 0, 0, 0, Qt::transparent, 0.0};

    QPainter painter(&image);
    paintClientSideFrame(painter, options);

    painter.end();

    const auto hasPartialAlpha = [&image](const QRect& area) {
        for (int y = area.top(); y <= area.bottom(); ++y) {
            for (int x = area.left(); x <= area.right(); ++x) {
                const int alpha = image.pixelColor(x, y).alpha();
                if (alpha > 0 && alpha < 255)
                    return true;
            }
        }
        return false;
    };

    const int cornerSize = static_cast<int>(options.radius) + 2;
    const QRect topLeft(frameRect.topLeft(), QSize(cornerSize, cornerSize));
    const QRect topRight(QPoint(frameRect.right() - cornerSize + 1, frameRect.top()),
                         QSize(cornerSize, cornerSize));
    const QRect bottomLeft(QPoint(frameRect.left(), frameRect.bottom() - cornerSize + 1),
                           QSize(cornerSize, cornerSize));
    const QRect bottomRight(QPoint(frameRect.right() - cornerSize + 1,
                                   frameRect.bottom() - cornerSize + 1),
                            QSize(cornerSize, cornerSize));

    EXPECT_TRUE(hasPartialAlpha(topLeft));
    EXPECT_TRUE(hasPartialAlpha(topRight));
    EXPECT_TRUE(hasPartialAlpha(bottomLeft));
    EXPECT_TRUE(hasPartialAlpha(bottomRight));
}

TEST_F(WindowTest, ClientSideFrameOverlayDoesNotClearHostBackingPixels) {
    SquareSurfaceWidget host;
    host.setAttribute(Qt::WA_TranslucentBackground, true);
    host.resize(64, 64);

    const QRect frameRect(12, 12, 40, 40);
    ClientSideFramePaintOptions options;
    options.windowRect = host.rect();
    options.frameRect = frameRect;
    options.fill = Qt::white;
    options.stroke = Qt::black;
    options.radius = 8.0;
    options.shadow = {0, 0, 0, 0, Qt::transparent, 0.0};

    ClientSideFrameEdgeOverlay overlay(&host);
    overlay.setGeometry(host.rect());
    overlay.setFrameVisualRect(frameRect);
    overlay.setFrameRadius(options.radius);
    overlay.setFrameStroke(options.stroke);

    host.show();
    overlay.show();
    QApplication::processEvents();

    QImage image(host.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    host.render(&painter);
    painter.end();

    EXPECT_EQ(image.pixelColor(frameRect.topLeft()).alpha(), 255);
    EXPECT_EQ(image.pixelColor(frameRect.topRight()).alpha(), 255);
    EXPECT_EQ(image.pixelColor(frameRect.bottomLeft()).alpha(), 255);
    EXPECT_EQ(image.pixelColor(frameRect.bottomRight()).alpha(), 255);

    const QColor topRightFill = image.pixelColor(frameRect.right() - 1,
                                                 frameRect.top() + qRound(options.radius));
    EXPECT_GT(topRightFill.red(), topRightFill.green() * 2);
    EXPECT_GT(topRightFill.red(), topRightFill.blue() * 2);

    host.close();
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
