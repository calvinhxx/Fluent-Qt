#include <gtest/gtest.h>
#include <QApplication>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QSizePolicy>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include "components/basicinput/RepeatButton.h"
#include "components/status_info/ProgressRing.h"
#include "components/textfields/NumberBox.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"

using namespace fluent::status_info;
using fluent::basicinput::RepeatButton;
using fluent::textfields::NumberBox;

class ProgressRingTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        QPalette pal = palette();
        pal.setColor(QPalette::Window, c.bgCanvas);
        setPalette(pal);
        setAutoFillBackground(true);
    }
};

class ProgressRingTest : public ::testing::Test {
protected:
    void SetUp() override {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        window = new ProgressRingTestWindow();
        window->setWindowTitle("ProgressRing Visual Test");
        window->resize(720, 520);
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
        window = nullptr;
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    ProgressRingTestWindow* window = nullptr;
};

TEST_F(ProgressRingTest, DefaultPropertyValues) {
    ProgressRing ring;
    EXPECT_FALSE(ring.isActive());
    EXPECT_TRUE(ring.isIndeterminate());
    EXPECT_EQ(ring.minimum(), 0);
    EXPECT_EQ(ring.maximum(), 100);
    EXPECT_EQ(ring.value(), 0);
    EXPECT_EQ(ring.ringSize(), ProgressRing::ProgressRingSize::Medium);
    EXPECT_DOUBLE_EQ(ring.strokeWidth(), 3.0);
    EXPECT_EQ(ring.status(), ProgressRing::ProgressRingStatus::Running);
    EXPECT_FALSE(ring.backgroundVisible());
    EXPECT_FALSE(ring.isBackgroundVisible());
    EXPECT_FALSE(ring.isAnimationRunning());
}

TEST_F(ProgressRingTest, PropertySignalsAndSameValueNoSignal) {
    ProgressRing ring;

    QSignalSpy activeSpy(&ring, &ProgressRing::isActiveChanged);
    ring.setIsActive(true);
    EXPECT_EQ(activeSpy.count(), 1);
    ring.setIsActive(true);
    EXPECT_EQ(activeSpy.count(), 1);

    QSignalSpy indeterminateSpy(&ring, &ProgressRing::isIndeterminateChanged);
    ring.setIsIndeterminate(false);
    EXPECT_EQ(indeterminateSpy.count(), 1);
    ring.setIsIndeterminate(false);
    EXPECT_EQ(indeterminateSpy.count(), 1);

    QSignalSpy sizeSpy(&ring, &ProgressRing::ringSizeChanged);
    ring.setRingSize(ProgressRing::ProgressRingSize::Large);
    EXPECT_EQ(sizeSpy.count(), 1);
    ring.setRingSize(ProgressRing::ProgressRingSize::Large);
    EXPECT_EQ(sizeSpy.count(), 1);

    QSignalSpy statusSpy(&ring, &ProgressRing::statusChanged);
    ring.setStatus(ProgressRing::ProgressRingStatus::Paused);
    EXPECT_EQ(statusSpy.count(), 1);
    ring.setStatus(ProgressRing::ProgressRingStatus::Paused);
    EXPECT_EQ(statusSpy.count(), 1);
}

TEST_F(ProgressRingTest, RangeAndValueClamp) {
    ProgressRing ring;
    QSignalSpy valueSpy(&ring, &ProgressRing::valueChanged);

    ring.setValue(140);
    EXPECT_EQ(ring.value(), 100);
    EXPECT_EQ(valueSpy.count(), 1);

    ring.setValue(-20);
    EXPECT_EQ(ring.value(), 0);
    EXPECT_EQ(valueSpy.count(), 2);

    QSignalSpy minSpy(&ring, &ProgressRing::minimumChanged);
    QSignalSpy maxSpy(&ring, &ProgressRing::maximumChanged);
    ring.setRange(20, 10);
    EXPECT_EQ(ring.minimum(), 20);
    EXPECT_EQ(ring.maximum(), 21);
    EXPECT_EQ(ring.value(), 20);
    EXPECT_EQ(minSpy.count(), 1);
    EXPECT_EQ(maxSpy.count(), 1);
    EXPECT_EQ(valueSpy.count(), 3);
}

TEST_F(ProgressRingTest, DeterminateProgressRatio) {
    ProgressRing ring;
    ring.setIsIndeterminate(false);
    ring.setValue(40);

    EXPECT_FALSE(ring.isIndeterminate());
    EXPECT_DOUBLE_EQ(ring.progressRatio(), 0.4);
}

TEST_F(ProgressRingTest, SizeHintsReflectRingSize) {
    ProgressRing ring;

    ring.setRingSize(ProgressRing::ProgressRingSize::Small);
    EXPECT_EQ(ring.sizeHint(), QSize(16, 16));
    EXPECT_EQ(ring.minimumSizeHint(), QSize(16, 16));

    ring.setRingSize(ProgressRing::ProgressRingSize::Medium);
    EXPECT_EQ(ring.sizeHint(), QSize(32, 32));

    ring.setRingSize(ProgressRing::ProgressRingSize::Large);
    EXPECT_EQ(ring.sizeHint(), QSize(64, 64));
}

TEST_F(ProgressRingTest, StrokeWidthValidation) {
    ProgressRing ring;
    QSignalSpy spy(&ring, &ProgressRing::strokeWidthChanged);

    ring.setStrokeWidth(4.5);
    EXPECT_DOUBLE_EQ(ring.strokeWidth(), 4.5);
    EXPECT_EQ(spy.count(), 1);

    ring.setStrokeWidth(0.0);
    ring.setStrokeWidth(-1.0);
    EXPECT_DOUBLE_EQ(ring.strokeWidth(), 4.5);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ProgressRingTest, BackgroundVisibleSignal) {
    ProgressRing ring;
    QSignalSpy spy(&ring, &ProgressRing::backgroundVisibleChanged);

    ring.setBackgroundVisible(true);
    EXPECT_TRUE(ring.backgroundVisible());
    EXPECT_TRUE(ring.isBackgroundVisible());
    EXPECT_EQ(spy.count(), 1);

    ring.setBackgroundVisible(true);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ProgressRingTest, AnimationLifecycle) {
    ProgressRing ring;
    ring.show();
    QApplication::processEvents();

    ring.setIsActive(true);
    EXPECT_TRUE(ring.isAnimationRunning());

    ring.setStatus(ProgressRing::ProgressRingStatus::Paused);
    EXPECT_FALSE(ring.isAnimationRunning());

    ring.setStatus(ProgressRing::ProgressRingStatus::Running);
    EXPECT_TRUE(ring.isAnimationRunning());

    ring.setIsIndeterminate(false);
    EXPECT_FALSE(ring.isAnimationRunning());

    ring.setIsIndeterminate(true);
    EXPECT_TRUE(ring.isAnimationRunning());

    ring.setEnabled(false);
    QApplication::processEvents();
    EXPECT_FALSE(ring.isAnimationRunning());

    ring.setEnabled(true);
    QApplication::processEvents();
    EXPECT_TRUE(ring.isAnimationRunning());

    ring.hide();
    QApplication::processEvents();
    EXPECT_FALSE(ring.isAnimationRunning());

    ring.show();
    QApplication::processEvents();
    EXPECT_TRUE(ring.isAnimationRunning());

    ring.setIsActive(false);
    EXPECT_FALSE(ring.isAnimationRunning());
}

TEST_F(ProgressRingTest, NumberBoxValueDrivesDeterminateRing) {
    window->resize(360, 120);

    auto* root = new QHBoxLayout(window);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(28);

    auto* ring = new ProgressRing(window);
    ring->setRingSize(ProgressRing::ProgressRingSize::Medium);
    ring->setIsIndeterminate(false);
    ring->setIsActive(true);
    ring->setBackgroundVisible(true);

    auto* progressBox = new NumberBox(window);
    progressBox->setFixedWidth(150);
    progressBox->setRange(0, 100);
    progressBox->setSmallChange(1);
    progressBox->setLargeChange(10);
    progressBox->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);

    QObject::connect(progressBox, &NumberBox::valueChanged, ring, [ring](double value) {
        ring->setValue(static_cast<int>(value));
    });

    progressBox->setValue(32);
    EXPECT_EQ(ring->value(), 32);
    EXPECT_DOUBLE_EQ(ring->progressRatio(), 0.32);

    root->addWidget(ring, 0, Qt::AlignVCenter);
    root->addWidget(progressBox, 0, Qt::AlignVCenter);
    root->addStretch();

    window->show();
    QApplication::processEvents();

    auto* upButton = progressBox->findChild<RepeatButton*>("NumberBoxSpinUpButton");
    ASSERT_NE(upButton, nullptr);
    ASSERT_FALSE(upButton->isHidden());

    QSignalSpy ringValueSpy(ring, &ProgressRing::valueChanged);
    upButton->click();
    QApplication::processEvents();
    EXPECT_DOUBLE_EQ(progressBox->value(), 33);
    EXPECT_EQ(ring->value(), 33);
    EXPECT_DOUBLE_EQ(ring->progressRatio(), 0.33);
    EXPECT_EQ(ringValueSpy.count(), 1);

    progressBox->setValue(150);
    EXPECT_DOUBLE_EQ(progressBox->value(), 100);
    EXPECT_EQ(ring->value(), 100);
    EXPECT_DOUBLE_EQ(ring->progressRatio(), 1.0);

    progressBox->setValue(-10);
    EXPECT_DOUBLE_EQ(progressBox->value(), 0);
    EXPECT_EQ(ring->value(), 0);
    EXPECT_DOUBLE_EQ(ring->progressRatio(), 0.0);
}

// ─── Design-language × theme paint compatibility ────────────────────────────
//
// ProgressRing branches its paintEvent on the active design language: DesignFluent keeps the WinUI
// arc, DesignMaterial draws a rounded-cap arc + track + leading stop dot, DesignCupertino draws a
// spoke spinner (indeterminate) or a thin accent ring (determinate). Each must paint valid, non-empty
// content under both App themes, and a determinate ring at 0% must read visibly different from one at
// 100%. Design language + theme are GLOBAL singletons, so the fixture restores both in TearDown.
// zh_CN: ProgressRing 在 paintEvent 中按设计语言分支:DesignFluent 保持 WinUI 弧线,DesignMaterial 绘制
// 圆头弧线+轨道+前端停止点,DesignCupertino 绘制辐条转轮(不确定)或细 accent 环(确定)。每种都必须在两
// 种主题下绘制有效、非空内容,且确定态 0% 必须明显不同于 100%。设计语言与主题为全局单例,夹具在
// TearDown 中恢复二者。
class ProgressRingDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Design language + theme are GLOBAL — reset so later suites see defaults.
        // zh_CN: 设计语言与主题为全局状态;复位以保证后续套件看到默认值。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Build a DETERMINATE ring at the requested value, sized like a real medium ring, and grab it.
    // zh_CN: 以指定值构建确定态环,设定与真实中号环一致的尺寸并抓取。
    static QImage grabDeterminate(int value) {
        ProgressRing ring;
        ring.setIsIndeterminate(false);
        ring.setIsActive(true);
        ring.setBackgroundVisible(true);
        ring.setValue(value);
        ring.resize(48, 48);
        return ring.grab().toImage();
    }

    static bool hasPaintedContent(const QImage& img) {
        if (img.isNull()) return false;
        const QRgb bg = img.pixel(0, 0);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixel(x, y) != bg)
                    return true;
        return false;
    }

    static int differingPixels(const QImage& a, const QImage& b) {
        if (a.size() != b.size())
            return -1;
        int diff = 0;
        for (int y = 0; y < a.height(); ++y)
            for (int x = 0; x < a.width(); ++x)
                if (a.pixel(x, y) != b.pixel(x, y))
                    ++diff;
        return diff;
    }
};

TEST_F(ProgressRingDesignLanguageTest, AllLanguagesAndThemesPaintAndProgressIsVisible) {
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

            const QImage low = grabDeterminate(0);
            const QImage high = grabDeterminate(100);
            const QImage mid = grabDeterminate(60);

            // 1. Each grab paints a valid, non-empty image with content. zh_CN: 每次抓取都绘制有效、非空且有内容的图像。
            ASSERT_FALSE(low.isNull()) << ctx << "/0%";
            ASSERT_FALSE(high.isNull()) << ctx << "/100%";
            ASSERT_FALSE(mid.isNull()) << ctx << "/60%";
            EXPECT_GT(mid.width(), 0) << ctx;
            EXPECT_GT(mid.height(), 0) << ctx;
            EXPECT_TRUE(hasPaintedContent(mid)) << "60% painted nothing: " << ctx;
            EXPECT_TRUE(hasPaintedContent(high)) << "100% painted nothing: " << ctx;

            // 2. A determinate ring at 0% must read visibly different from one at 100%. zh_CN: 确定态 0% 必须明显不同于 100%。
            const int diff = differingPixels(low, high);
            ASSERT_GE(diff, 0) << ctx << " (size mismatch)";
            EXPECT_GT(diff, 0)
                << "determinate progress is indistinguishable between 0% and 100%: " << ctx;
        }
    }
}

// The macOS spoke spinner is a distinct code path (radial tapered lines with a fading alpha ramp)
// from the determinate ring; verify the INDETERMINATE Cupertino ring paints content without crashing.
// Headless timers don't advance, so it relies on the static fading frame. zh_CN: macOS 辐条转轮是与确定
// 环不同的代码路径(放射状渐细线段+渐隐 alpha);验证不确定态 Cupertino 环绘制出内容且不崩溃。无头环境
// 定时器不前进,故依赖静态渐隐帧。
TEST_F(ProgressRingDesignLanguageTest, CupertinoIndeterminateSpokeSpinnerPaints) {
    fluent::ThemeRegistry::instance().setDesignLanguage(fluent::FluentElement::DesignCupertino);

    for (auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        fluent::FluentElement::setTheme(theme);

        ProgressRing ring;
        ring.setIsIndeterminate(true);
        ring.setIsActive(true);
        ring.resize(48, 48);
        const QImage img = ring.grab().toImage();

        ASSERT_FALSE(img.isNull()) << "theme=" << theme;
        EXPECT_TRUE(hasPaintedContent(img)) << "Cupertino spoke spinner painted nothing: theme=" << theme;
    }
}

TEST_F(ProgressRingTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP() << "Skipping visual test in offscreen mode";
    }

    window->resize(900, 820);

    auto* root = new QVBoxLayout(window);
    root->setContentsMargins(24, 22, 24, 24);
    root->setSpacing(14);

    auto* header = new QHBoxLayout();
    auto* title = new QLabel("ProgressRing", window);
    QFont titleFont = title->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto* themeButton = new QPushButton("Toggle Light/Dark", window);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(themeButton);
    root->addLayout(header);

    auto* actions = new QHBoxLayout();
    actions->setSpacing(8);
    actions->addWidget(new QPushButton("Documentation", window));
    actions->addWidget(new QPushButton("Source", window));
    actions->addStretch();
    root->addLayout(actions);

    auto* description = new QLabel(
        "The ProgressRing has two different visual representations:\n"
        "Indeterminate - shows that a task is ongoing, but blocks user interaction.\n"
        "Determinate - shows how much progress has been made on a known amount of work.",
        window);
    description->setWordWrap(true);
    root->addWidget(description);

    QObject::connect(themeButton, &QPushButton::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
            ? fluent::FluentElement::Dark
            : fluent::FluentElement::Light);
    });

    auto makeSectionLabel = [this](const QString& text) {
        auto* label = new QLabel(text, window);
        QFont font = label->font();
        font.setBold(true);
        label->setFont(font);
        return label;
    };

    auto makePanel = [this](int minimumHeight) {
        auto* panel = new QFrame(window);
        panel->setFrameShape(QFrame::StyledPanel);
        panel->setMinimumHeight(minimumHeight);
        panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return panel;
    };

    root->addWidget(makeSectionLabel("An indeterminate progress ring."));

    auto* indeterminatePanel = makePanel(150);
    auto* indeterminateLayout = new QHBoxLayout(indeterminatePanel);
    indeterminateLayout->setContentsMargins(32, 20, 32, 20);
    indeterminateLayout->setSpacing(28);
    auto* indeterminateRing = new ProgressRing(indeterminatePanel);
    indeterminateRing->setRingSize(ProgressRing::ProgressRingSize::Large);
    indeterminateRing->setIsActive(true);
    indeterminateRing->setIsIndeterminate(true);
    indeterminateLayout->addWidget(indeterminateRing, 0, Qt::AlignLeft | Qt::AlignVCenter);
    indeterminateLayout->addStretch();
    root->addWidget(indeterminatePanel);

    root->addWidget(makeSectionLabel("A determinate progress ring."));

    auto* determinatePanel = makePanel(126);
    auto* determinateLayout = new QHBoxLayout(determinatePanel);
    determinateLayout->setContentsMargins(34, 18, 34, 18);
    determinateLayout->setSpacing(58);
    auto* determinateRing = new ProgressRing(determinatePanel);
    determinateRing->setRingSize(ProgressRing::ProgressRingSize::Large);
    determinateRing->setIsIndeterminate(false);
    determinateRing->setIsActive(true);
    determinateRing->setBackgroundVisible(false);

    auto* progressBox = new NumberBox(determinatePanel);
    progressBox->setHeader("Progress");
    progressBox->setFixedWidth(156);
    progressBox->setRange(0, 100);
    progressBox->setSmallChange(1);
    progressBox->setLargeChange(10);
    progressBox->setDisplayPrecision(0);
    progressBox->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);
    QObject::connect(progressBox, &NumberBox::valueChanged, determinateRing, [determinateRing](double value) {
        determinateRing->setValue(static_cast<int>(value));
    });
    progressBox->setValue(32);

    determinateLayout->addWidget(determinateRing, 0, Qt::AlignLeft | Qt::AlignVCenter);
    determinateLayout->addWidget(progressBox, 0, Qt::AlignVCenter);
    determinateLayout->addStretch();
    root->addWidget(determinatePanel);

    root->addWidget(makeSectionLabel("Representative states."));

    auto* variantsPanel = makePanel(176);
    auto* grid = new QGridLayout(variantsPanel);
    grid->setContentsMargins(26, 20, 26, 20);
    grid->setHorizontalSpacing(34);
    grid->setVerticalSpacing(16);
    root->addWidget(variantsPanel);

    auto addExample = [grid, variantsPanel](int row, int column, const QString& labelText,
                                            ProgressRing::ProgressRingSize size,
                                            bool indeterminate,
                                            ProgressRing::ProgressRingStatus status,
                                            int value,
                                            bool backgroundVisible) {
        auto* cell = new QWidget(variantsPanel);
        auto* cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(8);
        cellLayout->setAlignment(Qt::AlignHCenter);

        auto* ring = new ProgressRing(cell);
        ring->setRingSize(size);
        ring->setIsIndeterminate(indeterminate);
        ring->setStatus(status);
        ring->setValue(value);
        ring->setBackgroundVisible(backgroundVisible);
        ring->setIsActive(true);

        auto* label = new QLabel(labelText, cell);
        label->setAlignment(Qt::AlignCenter);
        cellLayout->addWidget(ring, 0, Qt::AlignHCenter);
        cellLayout->addWidget(label, 0, Qt::AlignHCenter);
        grid->addWidget(cell, row, column);
    };

    addExample(0, 0, "Small Indeterminate", ProgressRing::ProgressRingSize::Small, true, ProgressRing::ProgressRingStatus::Running, 0, false);
    addExample(0, 1, "Medium Indeterminate", ProgressRing::ProgressRingSize::Medium, true, ProgressRing::ProgressRingStatus::Running, 0, true);
    addExample(0, 2, "Large Indeterminate", ProgressRing::ProgressRingSize::Large, true, ProgressRing::ProgressRingStatus::Running, 0, true);
    addExample(1, 0, "Determinate 40%", ProgressRing::ProgressRingSize::Medium, false, ProgressRing::ProgressRingStatus::Running, 40, true);
    addExample(1, 1, "Paused", ProgressRing::ProgressRingSize::Medium, false, ProgressRing::ProgressRingStatus::Paused, 68, true);
    addExample(1, 2, "Error", ProgressRing::ProgressRingSize::Medium, false, ProgressRing::ProgressRingStatus::Error, 68, true);

    window->show();
    qApp->exec();
}
