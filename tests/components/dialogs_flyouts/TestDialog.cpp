#include <gtest/gtest.h>
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QElapsedTimer>
#include <QTest>
#include <QWindow>
#include "components/dialogs_flyouts/Dialog.h"
#include "components/basicinput/Button.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/overlay/OverlayWindow.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include <QImage>

using namespace fluent::dialogs_flyouts;
using namespace fluent::basicinput;
using namespace fluent;

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

TEST_F(DialogTest, SmokeProperty) {
    Dialog dialog(window);
    EXPECT_FALSE(dialog.isSmokeEnabled());
    dialog.setSmokeEnabled(true);
    EXPECT_TRUE(dialog.isSmokeEnabled());
}

TEST_F(DialogTest, SmokeOverlayHonorsRoundedSurface) {
    SmokeOverlay overlay(nullptr);
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

TEST_F(DialogTest, LinuxSameWindowDialogRepositionsInsideOwnerSurface) {
#ifdef Q_OS_LINUX
    if (!::fluent::overlay::linuxDesktopUsesSameWindowSurfaces()) {
        GTEST_SKIP() << "same-window Dialog backend is only used on Linux desktop QPA plugins";
    }

    window->show();
    QApplication::processEvents();

    Dialog dialog(window);
    dialog.setAnimationEnabled(false);
    dialog.setFixedSize(300, 200);
    dialog.move(10000, 10000);
    dialog.open();
    QApplication::processEvents();

    const QPoint expected((window->width() - dialog.width()) / 2,
                          (window->height() - dialog.height()) / 2);
    EXPECT_EQ(dialog.pos(), expected);
    EXPECT_TRUE(window->rect().contains(dialog.geometry()));

    dialog.done(QDialog::Rejected);
    QApplication::processEvents();
#else
    GTEST_SKIP() << "Linux same-window Dialog positioning is only asserted on Linux";
#endif
}

TEST_F(DialogTest, SmokeDialogKeepsNativeTransientParentAndBlocksScrimClicks) {
    window->show();
    QApplication::processEvents();

    Dialog dialog(window);
    dialog.setSmokeEnabled(true);
    dialog.setAnimationEnabled(false);
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setFixedSize(300, 200);
    dialog.open();
    QApplication::processEvents();

    ASSERT_NE(window->windowHandle(), nullptr);
    ASSERT_NE(dialog.windowHandle(), nullptr);
    EXPECT_EQ(dialog.windowHandle()->transientParent(), window->windowHandle());

    auto* smoke = window->findChild<SmokeOverlay*>();
    ASSERT_NE(smoke, nullptr);
    ASSERT_TRUE(smoke->isVisible());

    QTest::mouseClick(smoke, Qt::LeftButton, Qt::NoModifier, smoke->rect().center());
    QApplication::processEvents();

    EXPECT_TRUE(dialog.isVisible());
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);

    dialog.done(QDialog::Rejected);
    QApplication::processEvents();
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
    // 入场：progress=0 时 windowOpacity 应为 0；progress=1 时为 1
    Dialog dialog(window);
    dialog.setFixedSize(400, 300);
    const QSize target = dialog.size();

    window->show();
    QApplication::processEvents();
    dialog.open();
    QApplication::processEvents();

    dialog.setAnimationProgress(0.0);
    EXPECT_NEAR(dialog.windowOpacity(), 0.0, 0.01);
    // 尺寸不应被动画修改
    EXPECT_EQ(dialog.size(), target);

    dialog.setAnimationProgress(1.0);
    EXPECT_NEAR(dialog.windowOpacity(), 1.0, 0.01);
    EXPECT_EQ(dialog.size(), target);

    dialog.setAnimationEnabled(false);
    dialog.done(0);
}

TEST_F(DialogTest, DialogExitAnimatesOpacity) {
    // 退场：progress=0 时 windowOpacity 应回到 0
    Dialog dialog(window);
    dialog.setFixedSize(400, 300);
    const QSize target = dialog.size();

    window->show();
    QApplication::processEvents();
    dialog.open();
    QApplication::processEvents();

    dialog.setAnimationProgress(1.0);
    EXPECT_NEAR(dialog.windowOpacity(), 1.0, 0.01);

    dialog.done(0);

    dialog.setAnimationProgress(0.0);
    EXPECT_NEAR(dialog.windowOpacity(), 0.0, 0.01);
    // 尺寸在退场期间也保持不变
    EXPECT_EQ(dialog.size(), target);
}

TEST_F(DialogTest, SmokeOverlayFadesInOutDelayedDestroy) {
    // 关键不变量：smoke 启用时，done() 返回后 overlay 应仍存在于 parent
    // children 中（延迟销毁），而非立即从 children 中消失。
    window->show();
    QApplication::processEvents();
    const int childCountBefore = window->findChildren<QWidget*>().size();

    Dialog dialog(window);
    dialog.setSmokeEnabled(true);
    dialog.setFixedSize(300, 200);
    dialog.setAnimationEnabled(false);  // 关闭 Dialog 主动画，仅测 smoke 行为

    bool overlayPresentAfterOpen = false;
    bool overlayStillPresentAfterDone = false;
    QTimer::singleShot(30, [&]() {
        // Dialog 已 open，smoke overlay 应作为 window 的子节点存在
        overlayPresentAfterOpen =
            (window->findChildren<QWidget*>().size() > childCountBefore);

        dialog.done(0);
        // done() 返回后，由于 smoke 走异步淡出，overlay 应仍在 children 中
        overlayStillPresentAfterDone =
            (window->findChildren<QWidget*>().size() > childCountBefore);
    });
    dialog.exec();

    EXPECT_TRUE(overlayPresentAfterOpen)
        << "Smoke overlay should be a child of window after open()";
    EXPECT_TRUE(overlayStillPresentAfterDone)
        << "Smoke overlay should still exist immediately after done() (delayed destroy)";

    // 等待淡出完成并让 deferred delete 处理完成（取决于 themeAnimation().fast）
    QElapsedTimer t; t.start();
    while (window->findChildren<QWidget*>().size() > childCountBefore && t.elapsed() < 1500) {
        QApplication::processEvents(QEventLoop::AllEvents, 10);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    // 允许不一定在 1500ms 内完全清理（平台/事件循环依赖）——记录但不硬性断言
    if (window->findChildren<QWidget*>().size() > childCountBefore) {
        qWarning() << "Smoke overlay still present after 1500ms; deferred delete may need full app loop";
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  Design-language × theme sweep — Dialog card surface/border per design language
// ──────────────────────────────────────────────────────────────────────────────
//
// Dialog::paintEvent now branches on the design language for the card SURFACE fill and the BORDER
// pen (Material 3 → borderless single bgLayer surface, elevation via shadow; macOS → bgLayer with a
// 1px strokeStrong hairline; Fluent → unchanged bgLayer + strokeDefault). For every language
// (Fluent / Material 3 / macOS) crossed with every theme (Light / Dark) the card must paint a valid,
// non-empty image with content and must NOT render an opaque near-black surface at the card centre
// (the invalid-QColor trap: a default-constructed QColor is INVALID yet QColor::alpha() == 255, so a
// bare alpha()>0 guard + setBrush(invalidColor) paints SOLID OPAQUE BLACK). Design language + theme
// are GLOBAL singletons, so the fixture restores both in TearDown.
// zh_CN: Dialog::paintEvent 现按设计语言分支绘制卡片「表面填充」与「边框画笔」(Material 3 → 无边框单一
// bgLayer 表面,高度靠阴影;macOS → bgLayer + 1px strokeStrong 发丝边;Fluent → 不变的 bgLayer +
// strokeDefault)。三种语言(Fluent/Material 3/macOS)× 两种主题(Light/Dark)下,卡片都必须绘制出有效、
// 非空且有内容的图像,且卡片中心不得呈现不透明近黑表面(无效 QColor 陷阱:默认构造的 QColor 无效却返回
// alpha==255,裸 alpha()>0 + setBrush(无效色) 会涂成不透明纯黑)。设计语言与主题为全局单例,夹具在
// TearDown 中恢复二者。
class DialogDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Design language + theme are GLOBAL — reset so later suites see defaults.
        // zh_CN: 设计语言与主题为全局状态;复位以保证后续套件看到默认值。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Build a borderless empty Dialog (whole card is bgLayer, no button bar / divider), show it
    // offscreen, and grab it as an image. No exec() — open()+processEvents realizes the overlay so
    // paintEvent runs. zh_CN: 构建无按钮的空 Dialog(整块卡片为 bgLayer,无按钮栏/分割线),离屏 show 后抓取
    // 为图像。不用 exec()——open()+processEvents 即可实现浮层并触发 paintEvent。
    static QImage grabDialog() {
        Dialog dialog;
        dialog.setAnimationEnabled(false);
        dialog.setSmokeEnabled(false);
        dialog.setFixedSize(360, 240);
        dialog.move(-2000, -2000);  // offscreen so it never steals focus / flashes. zh_CN: 离屏,避免抢焦点/闪烁。
        dialog.show();
        QApplication::processEvents();
        const QImage img = dialog.grab().toImage();
        dialog.hide();
        QApplication::processEvents();
        return img;
    }

    static bool hasPaintedContent(const QImage& img) {
        const QRgb bg = img.pixel(0, 0);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixel(x, y) != bg)
                    return true;
        return false;
    }
};

TEST_F(DialogDesignLanguageTest, AllLanguagesAndThemesPaintWithoutOpaqueBlackSurface) {
    struct LangCase { fluent::FluentElement::DesignLanguage lang; const char* name; };
    struct ThemeCase { fluent::FluentElement::Theme theme; const char* name; };

    const LangCase langs[] = {
        { fluent::FluentElement::DesignFluent, "Fluent" },
        { fluent::FluentElement::DesignMaterial, "Material" },
        { fluent::FluentElement::DesignCupertino, "Cupertino" },
    };
    const ThemeCase themes[] = {
        { fluent::FluentElement::Light, "Light" },
        { fluent::FluentElement::Dark, "Dark" },
    };

    for (const auto& lang : langs) {
        for (const auto& th : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(lang.lang);
            fluent::FluentElement::setTheme(th.theme);

            const std::string ctx = std::string(lang.name) + "/" + th.name;
            const QImage img = grabDialog();

            // 1. The card paints a valid, non-empty image with content. zh_CN: 卡片绘制出有效、非空且有内容的图像。
            ASSERT_FALSE(img.isNull()) << ctx;
            EXPECT_GT(img.width(), 0) << ctx;
            EXPECT_GT(img.height(), 0) << ctx;
            EXPECT_TRUE(hasPaintedContent(img)) << "Dialog painted nothing: " << ctx;

            // 2. The card centre (bgLayer surface, inside the shadow margin) must not be opaque near-black.
            // zh_CN: 卡片中心(阴影边距内的 bgLayer 表面)不得为不透明近黑。
            const QColor c = img.pixelColor(img.width() / 2, img.height() / 2);
            const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
            const bool opaqueBlack = c.alpha() > 200 && lum < 16;
            EXPECT_FALSE(opaqueBlack)
                << "Dialog painted an opaque black surface at the card centre: " << ctx
                << " rgba=(" << c.red() << "," << c.green() << "," << c.blue() << "," << c.alpha() << ")";
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  VisualCheck — 手动观察 Dialog 基类的渲染效果
// ══════════════════════════════════════════════════════════════════════════════

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
