#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/dialogs_flyouts/Dialog.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include <QApplication>
#include <QDebug>
#include <QGraphicsOpacityEffect>
#include <QImage>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <gtest/gtest.h>

using namespace fluent::dialogs_flyouts;
using namespace fluent::basicinput;
using namespace fluent;
using fluent::overlay::OverlayScrim;

// ── FluentTestWindow ─────────────────────────────────────────────────────────

class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

// ── Test fixture ─────────────────────────────────────────────────────────────

class DialogTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FluentTestWindow();
        window->setFixedSize(600, 500);
        window->setWindowTitle("Dialog Base Test");
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    FluentTestWindow* window;
};

// ══════════════════════════════════════════════════════════════════════════════
//  自动化测试 — Dialog 基类（纯 view 层：阴影 + 动画 + 拖拽）
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(DialogTest, DefaultProperties) {
    Dialog dialog(window);
    EXPECT_TRUE(dialog.isDragEnabled());
    EXPECT_TRUE(dialog.isAnimationEnabled());
    EXPECT_FALSE(dialog.isSmokeEnabled());
    EXPECT_EQ(dialog.shadowSize(), 16);
    EXPECT_DOUBLE_EQ(dialog.animationProgress(), 1.0);
}

TEST_F(DialogTest, Contract_OpenStateAndAliases) {
    window->show();
    QApplication::processEvents();

    Dialog dialog(window);
    dialog.setAnimationEnabled(false);
    dialog.setFixedSize(300, 200);

    QStringList order;
    QObject::connect(&dialog, &Dialog::opening, [&] {
        EXPECT_FALSE(dialog.isOpen());
        EXPECT_FALSE(dialog.isVisible());
        order << QStringLiteral("opening");
    });
    QObject::connect(&dialog, &Dialog::aboutToShow, [&] {
        EXPECT_FALSE(dialog.isOpen());
        EXPECT_FALSE(dialog.isVisible());
        order << QStringLiteral("aboutToShow");
    });
    QObject::connect(&dialog, &Dialog::isOpenChanged, [&](bool open) {
        EXPECT_EQ(dialog.isOpen(), open);
        EXPECT_EQ(dialog.isVisible(), !open);
        order << (open ? QStringLiteral("isOpenChanged(true)")
                       : QStringLiteral("isOpenChanged(false)"));
    });
    QObject::connect(&dialog, &Dialog::opened, [&] {
        EXPECT_TRUE(dialog.isOpen());
        EXPECT_TRUE(dialog.isVisible());
        order << QStringLiteral("opened");
    });
    QObject::connect(&dialog, &Dialog::closing, [&] {
        EXPECT_TRUE(dialog.isOpen());
        EXPECT_TRUE(dialog.isVisible());
        order << QStringLiteral("closing");
    });
    QObject::connect(&dialog, &Dialog::aboutToHide, [&] { order << QStringLiteral("aboutToHide"); });
    QObject::connect(&dialog, &Dialog::closed, [&] {
        EXPECT_FALSE(dialog.isOpen());
        EXPECT_FALSE(dialog.isVisible());
        order << QStringLiteral("closed");
    });

    dialog.open();
    QApplication::processEvents();
    EXPECT_TRUE(dialog.isOpen());
    EXPECT_TRUE(dialog.isVisible());
    EXPECT_EQ(order, (QStringList{
                          QStringLiteral("opening"),
                          QStringLiteral("aboutToShow"),
                          QStringLiteral("isOpenChanged(true)"),
                          QStringLiteral("opened"),
                      }));

    const int openSignals = order.count();
    dialog.open();
    EXPECT_EQ(order.count(), openSignals);

    dialog.done(QDialog::Rejected);
    QApplication::processEvents();
    EXPECT_FALSE(dialog.isOpen());
    EXPECT_FALSE(dialog.isVisible());
    EXPECT_EQ(order, (QStringList{
                          QStringLiteral("opening"),
                          QStringLiteral("aboutToShow"),
                          QStringLiteral("isOpenChanged(true)"),
                          QStringLiteral("opened"),
                          QStringLiteral("closing"),
                          QStringLiteral("aboutToHide"),
                          QStringLiteral("isOpenChanged(false)"),
                          QStringLiteral("closed"),
                      }));
}

TEST_F(DialogTest, Contract_CloseWhileOpeningCancelsEntrance) {
    window->show();
    QApplication::processEvents();

    Dialog dialog(window);
    dialog.setAnimationEnabled(false);
    dialog.setFixedSize(300, 200);

    QStringList order;
    QObject::connect(&dialog, &Dialog::opening, [&] { order << QStringLiteral("opening"); });
    QObject::connect(&dialog, &Dialog::aboutToShow, [&] { order << QStringLiteral("aboutToShow"); });
    QObject::connect(&dialog, &Dialog::opened, [&] { order << QStringLiteral("opened"); });
    QObject::connect(&dialog, &Dialog::closing, [&] { order << QStringLiteral("closing"); });
    QObject::connect(&dialog, &Dialog::aboutToHide, [&] { order << QStringLiteral("aboutToHide"); });
    QObject::connect(&dialog, &Dialog::closed, [&] { order << QStringLiteral("closed"); });
    QObject::connect(&dialog, &Dialog::opening, &dialog, [&]() { dialog.done(QDialog::Rejected); });

    dialog.open();
    QApplication::processEvents();
    EXPECT_FALSE(dialog.isOpen());
    EXPECT_FALSE(dialog.isVisible());
    EXPECT_EQ(order, (QStringList{
                          QStringLiteral("opening"),
                          QStringLiteral("closing"),
                          QStringLiteral("aboutToHide"),
                          QStringLiteral("closed"),
                      }));
}

TEST_F(DialogTest, Contract_NotifyNoOpsAndSmokeBundle) {
    Dialog dialog(window);
    QSignalSpy modalSpy(&dialog, &Dialog::modalChanged);
    QSignalSpy dimSpy(&dialog, &Dialog::dimChanged);
    QSignalSpy smokeSpy(&dialog, &Dialog::smokeEnabledChanged);
    QSignalSpy dragSpy(&dialog, &Dialog::dragEnabledChanged);
    QSignalSpy animSpy(&dialog, &Dialog::animationEnabledChanged);

    dialog.setModal(true);
    dialog.setModal(true);
    EXPECT_FALSE(dialog.isSmokeEnabled());
    EXPECT_EQ(smokeSpy.count(), 0);
    dialog.setDim(true);
    dialog.setDim(true);
    EXPECT_TRUE(dialog.isSmokeEnabled());
    dialog.setDragEnabled(false);
    dialog.setDragEnabled(false);
    dialog.setAnimationEnabled(false);
    dialog.setAnimationEnabled(false);
    EXPECT_EQ(modalSpy.count(), 1);
    EXPECT_EQ(dimSpy.count(), 1);
    EXPECT_EQ(smokeSpy.count(), 1);
    EXPECT_EQ(dragSpy.count(), 1);
    EXPECT_EQ(animSpy.count(), 1);

    dialog.setSmokeEnabled(false);
    dialog.setSmokeEnabled(false);
    EXPECT_FALSE(dialog.isDim());
    EXPECT_FALSE(dialog.isModal());
    EXPECT_FALSE(dialog.isSmokeEnabled());

    dialog.setSmokeEnabled(true);
    EXPECT_TRUE(dialog.isDim());
    EXPECT_TRUE(dialog.isModal());
    EXPECT_TRUE(dialog.isSmokeEnabled());

    dialog.setModal(false);
    EXPECT_FALSE(dialog.isSmokeEnabled());
    dialog.setModal(true);
    EXPECT_TRUE(dialog.isSmokeEnabled());
}

TEST_F(DialogTest, Contract_ThemeChangeDoesNotMutateOpenState) {
    window->show();
    QApplication::processEvents();

    Dialog dialog(window);
    dialog.setAnimationEnabled(false);
    dialog.setFixedSize(300, 200);
    dialog.open();
    QApplication::processEvents();
    ASSERT_TRUE(dialog.isOpen());

    QSignalSpy openChanged(&dialog, &Dialog::isOpenChanged);
    QSignalSpy opened(&dialog, &Dialog::opened);
    QSignalSpy closed(&dialog, &Dialog::closed);

    const auto previous = fluent::FluentElement::currentTheme();
    fluent::FluentElement::setTheme(previous == fluent::FluentElement::Light
                                        ? fluent::FluentElement::Dark
                                        : fluent::FluentElement::Light);
    dialog.onThemeUpdated();

    EXPECT_TRUE(dialog.isOpen());
    EXPECT_EQ(openChanged.count(), 0);
    EXPECT_EQ(opened.count(), 0);
    EXPECT_EQ(closed.count(), 0);

    fluent::FluentElement::setTheme(previous);
    dialog.done(QDialog::Rejected);
    QApplication::processEvents();
}

TEST_F(DialogTest, SmokeProperty) {
    Dialog dialog(window);
    EXPECT_FALSE(dialog.isSmokeEnabled());
    dialog.setSmokeEnabled(true);
    EXPECT_TRUE(dialog.isSmokeEnabled());
}

TEST_F(DialogTest, DialogSmokeUsesRoundedOverlayScrim) {
    OverlayScrim overlay(nullptr);
    overlay.resize(80, 80);
    overlay.setColor(QColor(0, 0, 0, 200));
    overlay.setProgress(1.0);
    overlay.setSurfaceRadius(16);

    QImage image(overlay.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    overlay.render(&painter);
    painter.end();

    EXPECT_EQ(image.pixelColor(0, 0).alpha(), 0);
    EXPECT_GT(image.pixelColor(image.rect().center()).alpha(), 0);
}

TEST_F(DialogTest, DialogSmokeMatchesSharedBackingScrimContract) {
    OverlayScrim overlay(nullptr);

    // Dialog smoke must use the shared OverlayScrim contract: shared-backing SourceOver dim.
    // Independent translucent surfaces + Source-clear erase Mica content and thicken text.
    // zh_CN: Dialog 烟雾必须使用统一 OverlayScrim 契约：共享后备缓冲上的 SourceOver 压暗。
    // 独立透明表面 + Source 清屏会擦掉 Mica 内容并让文字变粗。
    EXPECT_TRUE(overlay.testAttribute(Qt::WA_NoSystemBackground));
    EXPECT_FALSE(overlay.testAttribute(Qt::WA_TranslucentBackground));
    EXPECT_FALSE(overlay.autoFillBackground());
    EXPECT_EQ(overlay.graphicsEffect(), nullptr);

    overlay.setProgress(0.5);
    EXPECT_DOUBLE_EQ(overlay.progress(), 0.5);

    overlay.setProgress(2.0);
    EXPECT_DOUBLE_EQ(overlay.progress(), 1.0);

    overlay.resize(20, 20);
    overlay.setColor(QColor(0, 0, 0, 200));
    overlay.setProgress(0.5);
    QImage image(overlay.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    overlay.render(&painter);
    painter.end();
    EXPECT_NEAR(image.pixelColor(image.rect().center()).alpha(), 100, 1);
}

TEST_F(DialogTest, DragProperty) {
    Dialog dialog(window);
    EXPECT_TRUE(dialog.isDragEnabled());
    dialog.setDragEnabled(false);
    EXPECT_FALSE(dialog.isDragEnabled());
}

TEST_F(DialogTest, AnimationProperty) {
    Dialog dialog(window);
    EXPECT_TRUE(dialog.isAnimationEnabled());
    dialog.setAnimationEnabled(false);
    EXPECT_FALSE(dialog.isAnimationEnabled());
}

TEST_F(DialogTest, AnimationProgressProperty) {
    Dialog dialog(window);
    dialog.setAnimationProgress(0.5);
    EXPECT_DOUBLE_EQ(dialog.animationProgress(), 0.5);
}

TEST_F(DialogTest, ExecWithoutAnimation) {
    Dialog dialog(window);
    dialog.setAnimationEnabled(false);
    dialog.setFixedSize(300, 200);

    QTimer::singleShot(50, [&]() { dialog.done(QDialog::Accepted); });
    int result = dialog.exec();
    EXPECT_EQ(result, QDialog::Accepted);
}

TEST_F(DialogTest, FinishedHandlerCanSynchronouslyDeleteDialogWithoutAnimation) {
    auto* dialog = new Dialog(window);
    dialog->setAnimationEnabled(false);
    QPointer<Dialog> guard(dialog);
    QObject::connect(dialog, &QDialog::finished, window, [dialog] {
        delete dialog;
    });

    dialog->show();
    dialog->done(QDialog::Accepted);

    EXPECT_TRUE(guard.isNull());
}

TEST_F(DialogTest, OpenPreservesExplicitApplicationModality) {
    window->show();
    QApplication::processEvents();

    Dialog dialog(window);
    dialog.setAnimationEnabled(false);
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.open();
    QApplication::processEvents();

    EXPECT_TRUE(dialog.isVisible());
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);

    dialog.done(QDialog::Rejected);
    QApplication::processEvents();
}

TEST_F(DialogTest, SameWindowDialogRepositionsInsideOwnerSurface) {
    window->show();
    QApplication::processEvents();

    Dialog dialog(window);
    dialog.setAnimationEnabled(false);
    dialog.setFixedSize(300, 200);
    dialog.move(10000, 10000);
    dialog.open();
    QApplication::processEvents();

    EXPECT_EQ(dialog.parentWidget(), window);
    EXPECT_EQ(dialog.windowType(), Qt::Widget);

    const QPoint expected((window->width() - dialog.width()) / 2,
                          (window->height() - dialog.height()) / 2);
    EXPECT_EQ(dialog.pos(), expected);
    EXPECT_TRUE(window->rect().contains(dialog.geometry()));

    dialog.done(QDialog::Rejected);
    QApplication::processEvents();
}

TEST_F(DialogTest, OpenReResolvesOwnerAfterHostJoinsFinalWindow) {
    auto* page = new QWidget;
    auto* host = new QWidget(page);
    host->setGeometry(0, 0, 300, 200);

    {
        Dialog dialog(host);
        dialog.setAnimationEnabled(false);
        dialog.setFixedSize(300, 200);

        page->setParent(window);
        page->setGeometry(180, 0, 420, 500);
        window->show();
        page->show();
        host->show();
        QApplication::processEvents();

        dialog.open();
        QApplication::processEvents();

        EXPECT_EQ(dialog.parentWidget(), window);
        EXPECT_EQ(dialog.windowType(), Qt::Widget);
        EXPECT_EQ(dialog.pos(), QPoint(150, 150));

        dialog.done(QDialog::Rejected);
        QApplication::processEvents();
    }

    delete page;
}

TEST_F(DialogTest, SmokeDialogBlocksScrimClicks) {
    window->show();
    QApplication::processEvents();

    Dialog dialog(window);
    dialog.setSmokeEnabled(true);
    dialog.setAnimationEnabled(false);
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setFixedSize(300, 200);
    dialog.open();
    QApplication::processEvents();

    EXPECT_EQ(dialog.parentWidget(), window);
    EXPECT_EQ(dialog.windowType(), Qt::Widget);

    auto* smoke = window->findChild<OverlayScrim*>(QStringLiteral("DialogSmokeScrim"));
    ASSERT_NE(smoke, nullptr);
    ASSERT_TRUE(smoke->isVisible());

    QTest::mouseClick(smoke, Qt::LeftButton, Qt::NoModifier, smoke->rect().center());
    QApplication::processEvents();

    EXPECT_TRUE(dialog.isVisible());
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);

    // Trackpad/mouse wheel input must not leak through the modal smoke into a scrollable owner.
    // zh_CN: 触控板/滚轮输入不得穿过模态 smoke 继续滚动宿主界面。
    FLUENT_MAKE_WHEEL_EVENT(wheel, smoke->rect().center().x(), smoke->rect().center().y(),
                            -120, Qt::NoModifier);
    wheel.ignore();
    QApplication::sendEvent(smoke, &wheel);
    EXPECT_TRUE(wheel.isAccepted());

    dialog.done(QDialog::Rejected);
    QApplication::processEvents();
}

TEST_F(DialogTest, ClosingSmokeOverlayImmediatelyReleasesOwnerInput) {
    window->show();
    QApplication::processEvents();

    Dialog dialog(window);
    dialog.setSmokeEnabled(true);
    dialog.setAnimationEnabled(false);
    dialog.setFixedSize(300, 200);
    dialog.open();
    QApplication::processEvents();

    QPointer<OverlayScrim> smoke = window->findChild<OverlayScrim*>(QStringLiteral("DialogSmokeScrim"));
    ASSERT_FALSE(smoke.isNull());
    ASSERT_TRUE(smoke->isVisible());
    EXPECT_FALSE(smoke->testAttribute(Qt::WA_TransparentForMouseEvents));

    dialog.done(QDialog::Rejected);

    // The owner must not retain a visual or input surface after the dialog is closed.
    // zh_CN: Dialog 关闭后宿主不得保留任何可见或可命中的 smoke 表面。
    EXPECT_TRUE(smoke.isNull());
    EXPECT_TRUE(window->findChildren<OverlayScrim*>(QStringLiteral("DialogSmokeScrim")).isEmpty());
}

TEST_F(DialogTest, ExecSmokeDialogDoesNotPromoteOwnerContentToNative) {
    // Same-window Dialog must not sticky-promote overlapping owner content to WA_NativeWindow
    // (the historical macOS content-area input freeze when Dialog was a native transient window).
    // zh_CN: 同窗口 Dialog 不得把重叠宿主内容粘性提升为 WA_NativeWindow
    //（历史问题：Dialog 曾为原生临时窗口时会导致 macOS 内容区输入卡死）。
    auto* content = new QWidget(window);
    content->setObjectName(QStringLiteral("ownerContent"));
    content->setGeometry(0, 0, 600, 500);
    auto* inner = new QWidget(content);
    inner->setObjectName(QStringLiteral("ownerInner"));
    inner->setGeometry(20, 20, 200, 40);

    window->show();
    QApplication::processEvents();
    ASSERT_FALSE(content->testAttribute(Qt::WA_NativeWindow));
    ASSERT_EQ(content->windowHandle(), nullptr);

    for (int index = 0; index < 2; ++index) {
        auto* dialog = new Dialog(window);
        dialog->setSmokeEnabled(true);
        dialog->setAnimationEnabled(false);
        dialog->setFixedSize(300, 200);
        QTimer::singleShot(30, [dialog]() { dialog->done(QDialog::Rejected); });
        EXPECT_EQ(dialog->exec(), QDialog::Rejected);
        delete dialog;
        QApplication::processEvents();
    }

    EXPECT_FALSE(content->testAttribute(Qt::WA_NativeWindow))
        << "owner content widget was promoted to a native window by the dialog";
    EXPECT_EQ(content->windowHandle(), nullptr)
        << "owner content widget acquired its own native window handle";
    EXPECT_FALSE(inner->testAttribute(Qt::WA_NativeWindow));
}

TEST_F(DialogTest, ThemeSwitchNoCrash) {
    Dialog dialog(window);
    dialog.setAnimationEnabled(false);

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    dialog.onThemeUpdated();

    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    dialog.onThemeUpdated();

    SUCCEED();
}

// ══════════════════════════════════════════════════════════════════════════════
//  入场/退场动画：仅 opacity（scale 已移除以避免子控件错位）
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(DialogTest, ThemeSourceInheritsLocalOverride) {
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    window->onThemeUpdated();

    auto* host = new QWidget(window);
    host->setProperty("fluentThemeOverride", static_cast<int>(fluent::FluentElement::Light));
    host->setGeometry(24, 24, 220, 120);
    host->show();

    auto* trigger = new Button(QStringLiteral("Open"), host);
    trigger->setGeometry(16, 16, 96, 32);
    trigger->show();

    Dialog dialog(window);
    dialog.setThemeSource(trigger);

    EXPECT_EQ(dialog.effectiveTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(dialog.themeColors().bgLayer, QColor("#FFFFFF"));
    EXPECT_EQ(trigger->effectiveTheme(), fluent::FluentElement::Light);
}

TEST_F(DialogTest, DialogEntranceAnimatesOpacity) {
    // 入场：progress=0 时 graphics opacity 应为 0；progress=1 时为 1
    Dialog dialog(window);
    dialog.setFixedSize(400, 300);
    const QSize target = dialog.size();

    window->show();
    QApplication::processEvents();
    dialog.open();
    QApplication::processEvents();

    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(dialog.graphicsEffect());
    ASSERT_NE(effect, nullptr);

    dialog.setAnimationProgress(0.0);
    EXPECT_NEAR(effect->opacity(), 0.0, 0.01);
    EXPECT_NEAR(dialog.animationProgress(), 0.0, 0.01);
    // 尺寸不应被动画修改
    EXPECT_EQ(dialog.size(), target);

    dialog.setAnimationProgress(1.0);
    EXPECT_NEAR(effect->opacity(), 1.0, 0.01);
    EXPECT_NEAR(dialog.animationProgress(), 1.0, 0.01);
    EXPECT_EQ(dialog.size(), target);

    dialog.setAnimationEnabled(false);
    dialog.done(0);
}

TEST_F(DialogTest, DialogExitAnimatesOpacity) {
    // 退场：progress=0 时 graphics opacity 应回到 0
    Dialog dialog(window);
    dialog.setFixedSize(400, 300);
    const QSize target = dialog.size();

    window->show();
    QApplication::processEvents();
    dialog.open();
    QApplication::processEvents();

    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(dialog.graphicsEffect());
    ASSERT_NE(effect, nullptr);

    dialog.setAnimationProgress(1.0);
    EXPECT_NEAR(effect->opacity(), 1.0, 0.01);

    dialog.done(0);

    dialog.setAnimationProgress(0.0);
    EXPECT_NEAR(effect->opacity(), 0.0, 0.01);
    EXPECT_NEAR(dialog.animationProgress(), 0.0, 0.01);
    // 尺寸在退场期间也保持不变
    EXPECT_EQ(dialog.size(), target);
}

TEST_F(DialogTest, SequentialExecDialogsLeaveNoSmokeSurface) {
    // Gallery samples create a fresh ContentDialog for each action. Two sequential exec() calls
    // must not overlap owner-child smoke surfaces after either nested event loop exits.
    // zh_CN: Gallery 每个操作都会创建新的 ContentDialog；两个连续 exec() 在各自嵌套事件循环
    // 退出后都不得留下相互重叠的宿主 smoke 子表面。
    window->show();
    QApplication::processEvents();

    for (int index = 0; index < 2; ++index) {
        Dialog dialog(window);
        dialog.setSmokeEnabled(true);
        dialog.setFixedSize(300, 200);
        dialog.setAnimationEnabled(false);

        bool overlayPresentWhileOpen = false;
        QTimer::singleShot(30, [&]() {
            overlayPresentWhileOpen =
                !window->findChildren<OverlayScrim*>(QStringLiteral("DialogSmokeScrim")).isEmpty();
            dialog.done(QDialog::Rejected);
        });

        EXPECT_EQ(dialog.exec(), QDialog::Rejected);
        EXPECT_TRUE(overlayPresentWhileOpen);
        EXPECT_TRUE(window->findChildren<OverlayScrim*>(QStringLiteral("DialogSmokeScrim")).isEmpty())
            << "Sequential dialog " << index << " left an owner smoke surface";
    }
}


TEST_F(DialogTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    auto* layout = new AnchorLayout(window);
    window->setLayout(layout);
    window->setFixedSize(700, 500);

    using Edge = AnchorLayout::Edge;

    // --- 弹出空白 Dialog（仅阴影 + 动画） ---
    Button* btn1 = new Button("Open Empty Dialog", window);
    btn1->setFluentStyle(Button::Accent);
    btn1->setFixedSize(240, 32);
    btn1->anchors()->top  = {window, Edge::Top,  40};
    btn1->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(btn1);

    QObject::connect(btn1, &Button::clicked, [this]() {
        Dialog dialog(window);
        dialog.setFixedSize(400, 260);
        dialog.exec();
    });

    // --- 弹出禁用动画的 Dialog ---
    Button* btn2 = new Button("No-Animation Dialog", window);
    btn2->setFixedSize(240, 32);
    btn2->anchors()->top  = {btn1, Edge::Bottom, 16};
    btn2->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(btn2);

    QObject::connect(btn2, &Button::clicked, [this]() {
        Dialog dialog(window);
        dialog.setAnimationEnabled(false);
        dialog.setFixedSize(400, 260);
        dialog.exec();
    });

    // --- Toggle theme ---
    Button* themeBtn = new Button("Toggle Dark/Light", window);
    themeBtn->setFixedSize(240, 32);
    themeBtn->anchors()->top  = {btn2, Edge::Bottom, 32};
    themeBtn->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(themeBtn);

    QObject::connect(themeBtn, &Button::clicked, [this]() {
        auto theme = fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                         ? fluent::FluentElement::Dark : fluent::FluentElement::Light;
        fluent::FluentElement::setTheme(theme);
        window->onThemeUpdated();
    });

    window->show();
    qApp->exec();
}
