#include <gtest/gtest.h>

#include <QApplication>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QSignalSpy>
#include <QSizePolicy>
#include <QTest>
#include <QVBoxLayout>

#include <cmath>
#include <limits>

#include "components/foundation/FluentElement.h"
#include "components/basicinput/RepeatButton.h"
#include "components/status_info/ProgressBar.h"
#include "components/textfields/NumberBox.h"

using namespace fluent::status_info;
using fluent::basicinput::RepeatButton;
using fluent::textfields::NumberBox;

class ProgressBarTestWindow : public QWidget, public fluent::FluentElement {
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

class ProgressBarTest : public ::testing::Test {
protected:
    void SetUp() override {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        window = new ProgressBarTestWindow();
        window->setWindowTitle("ProgressBar Visual Test");
        window->resize(760, 520);
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
        window = nullptr;
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    QImage renderBar(ProgressBar& bar) {
        bar.resize(bar.sizeHint());
        QImage image(bar.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        bar.render(&image);
        return image;
    }

    ProgressBarTestWindow* window = nullptr;
};

TEST_F(ProgressBarTest, DefaultPropertyValues) {
    ProgressBar bar;
    EXPECT_FALSE(bar.isIndeterminate());
    EXPECT_DOUBLE_EQ(bar.minimum(), 0.0);
    EXPECT_DOUBLE_EQ(bar.maximum(), 100.0);
    EXPECT_DOUBLE_EQ(bar.value(), 0.0);
    EXPECT_FALSE(bar.showPaused());
    EXPECT_FALSE(bar.showError());
    EXPECT_EQ(bar.barWidth(), 220);
    EXPECT_DOUBLE_EQ(bar.trackThickness(), 3.0);
    EXPECT_TRUE(bar.railVisible());
    EXPECT_EQ(bar.progressText(), "0");
    EXPECT_FALSE(bar.isAnimationRunning());
}

TEST_F(ProgressBarTest, PropertySignalsAndSameValueNoSignal) {
    ProgressBar bar;

    QSignalSpy indeterminateSpy(&bar, &ProgressBar::isIndeterminateChanged);
    bar.setIsIndeterminate(true);
    EXPECT_EQ(indeterminateSpy.count(), 1);
    bar.setIsIndeterminate(true);
    EXPECT_EQ(indeterminateSpy.count(), 1);

    QSignalSpy pausedSpy(&bar, &ProgressBar::showPausedChanged);
    bar.setShowPaused(true);
    EXPECT_EQ(pausedSpy.count(), 1);
    bar.setShowPaused(true);
    EXPECT_EQ(pausedSpy.count(), 1);

    QSignalSpy errorSpy(&bar, &ProgressBar::showErrorChanged);
    bar.setShowError(true);
    EXPECT_EQ(errorSpy.count(), 1);
    bar.setShowError(true);
    EXPECT_EQ(errorSpy.count(), 1);

    QSignalSpy widthSpy(&bar, &ProgressBar::barWidthChanged);
    bar.setBarWidth(130);
    EXPECT_EQ(widthSpy.count(), 1);
    bar.setBarWidth(130);
    EXPECT_EQ(widthSpy.count(), 1);

    QSignalSpy thicknessSpy(&bar, &ProgressBar::trackThicknessChanged);
    bar.setTrackThickness(4.5);
    EXPECT_EQ(thicknessSpy.count(), 1);
    bar.setTrackThickness(4.5);
    EXPECT_EQ(thicknessSpy.count(), 1);

    QSignalSpy railSpy(&bar, &ProgressBar::railVisibleChanged);
    bar.setRailVisible(false);
    EXPECT_EQ(railSpy.count(), 1);
    bar.setRailVisible(false);
    EXPECT_EQ(railSpy.count(), 1);
}

TEST_F(ProgressBarTest, RangeAndValueClamp) {
    ProgressBar bar;
    QSignalSpy valueSpy(&bar, &ProgressBar::valueChanged);

    bar.setValue(140);
    EXPECT_DOUBLE_EQ(bar.value(), 100.0);
    EXPECT_EQ(valueSpy.count(), 1);

    bar.setValue(-20);
    EXPECT_DOUBLE_EQ(bar.value(), 0.0);
    EXPECT_EQ(valueSpy.count(), 2);

    QSignalSpy minSpy(&bar, &ProgressBar::minimumChanged);
    QSignalSpy maxSpy(&bar, &ProgressBar::maximumChanged);
    bar.setRange(20, 10);
    EXPECT_DOUBLE_EQ(bar.minimum(), 20.0);
    EXPECT_DOUBLE_EQ(bar.maximum(), 21.0);
    EXPECT_DOUBLE_EQ(bar.value(), 20.0);
    EXPECT_EQ(minSpy.count(), 1);
    EXPECT_EQ(maxSpy.count(), 1);
    EXPECT_EQ(valueSpy.count(), 3);
}

TEST_F(ProgressBarTest, ProgressRatioAndText) {
    ProgressBar bar;
    bar.setValue(40);

    EXPECT_DOUBLE_EQ(bar.progressRatio(), 0.4);
    EXPECT_EQ(bar.progressText(), "40");

    bar.setRange(0, 200);
    bar.setValue(66.6);
    EXPECT_DOUBLE_EQ(bar.progressRatio(), 0.333);
    EXPECT_EQ(bar.progressText(), "67");
}

TEST_F(ProgressBarTest, SizeAndThicknessValidation) {
    ProgressBar bar;
    EXPECT_EQ(bar.sizeHint(), QSize(220, 32));
    EXPECT_EQ(bar.minimumSizeHint().height(), 32);
    EXPECT_GT(bar.minimumSizeHint().width(), 0);

    QSignalSpy widthSpy(&bar, &ProgressBar::barWidthChanged);
    bar.setBarWidth(130);
    EXPECT_EQ(bar.sizeHint(), QSize(130, 32));
    EXPECT_EQ(widthSpy.count(), 1);

    bar.setBarWidth(0);
    bar.setBarWidth(-1);
    EXPECT_EQ(bar.barWidth(), 130);
    EXPECT_EQ(widthSpy.count(), 1);

    QSignalSpy thicknessSpy(&bar, &ProgressBar::trackThicknessChanged);
    bar.setTrackThickness(5.0);
    EXPECT_DOUBLE_EQ(bar.trackThickness(), 5.0);
    EXPECT_EQ(thicknessSpy.count(), 1);

    bar.setTrackThickness(0.0);
    bar.setTrackThickness(-2.0);
    bar.setTrackThickness(std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(bar.trackThickness(), 5.0);
    EXPECT_EQ(thicknessSpy.count(), 1);
}

TEST_F(ProgressBarTest, RailVisibleSignal) {
    ProgressBar bar;
    QSignalSpy spy(&bar, &ProgressBar::railVisibleChanged);

    bar.setRailVisible(false);
    EXPECT_FALSE(bar.railVisible());
    EXPECT_EQ(spy.count(), 1);

    bar.setRailVisible(false);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ProgressBarTest, NonFiniteValueFallsBackToMinimum) {
    ProgressBar bar;
    bar.setValue(55);
    EXPECT_DOUBLE_EQ(bar.value(), 55.0);

    bar.setValue(std::numeric_limits<double>::quiet_NaN());
    EXPECT_DOUBLE_EQ(bar.value(), 0.0);
    EXPECT_DOUBLE_EQ(bar.progressRatio(), 0.0);

    bar.setValue(72);
    bar.setValue(std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(bar.value(), 0.0);
}

TEST_F(ProgressBarTest, ErrorRenderingHasPriorityOverPaused) {
    ProgressBar errorOnly;
    errorOnly.setValue(68);
    errorOnly.setShowError(true);

    ProgressBar pausedAndError;
    pausedAndError.setValue(68);
    pausedAndError.setShowPaused(true);
    pausedAndError.setShowError(true);

    ProgressBar pausedOnly;
    pausedOnly.setValue(68);
    pausedOnly.setShowPaused(true);

    EXPECT_EQ(renderBar(errorOnly), renderBar(pausedAndError));
    EXPECT_NE(renderBar(errorOnly), renderBar(pausedOnly));
}

TEST_F(ProgressBarTest, ThemeChangeUpdatesRendering) {
    ProgressBar bar;
    bar.setValue(50);

    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    bar.onThemeUpdated();
    const QImage light = renderBar(bar);

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    bar.onThemeUpdated();
    const QImage dark = renderBar(bar);

    EXPECT_NE(light, dark);
}

TEST_F(ProgressBarTest, AnimationLifecycle) {
    ProgressBar bar;
    bar.setIsIndeterminate(true);
    bar.show();
    QApplication::processEvents();
    EXPECT_TRUE(bar.isAnimationRunning());

    bar.setShowPaused(true);
    EXPECT_FALSE(bar.isAnimationRunning());

    bar.setShowPaused(false);
    EXPECT_TRUE(bar.isAnimationRunning());

    bar.setShowError(true);
    EXPECT_FALSE(bar.isAnimationRunning());

    bar.setShowError(false);
    EXPECT_TRUE(bar.isAnimationRunning());

    bar.setEnabled(false);
    QApplication::processEvents();
    EXPECT_FALSE(bar.isAnimationRunning());

    bar.setEnabled(true);
    QApplication::processEvents();
    EXPECT_TRUE(bar.isAnimationRunning());

    bar.hide();
    QApplication::processEvents();
    EXPECT_FALSE(bar.isAnimationRunning());

    bar.show();
    QApplication::processEvents();
    EXPECT_TRUE(bar.isAnimationRunning());

    bar.setIsIndeterminate(false);
    EXPECT_FALSE(bar.isAnimationRunning());
}

TEST_F(ProgressBarTest, NumberBoxValueDrivesDeterminateBar) {
    window->resize(420, 120);

    auto* root = new QHBoxLayout(window);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(18);

    auto* bar = new ProgressBar(window);
    bar->setBarWidth(130);

    auto* progressBox = new NumberBox(window);
    progressBox->setFixedWidth(150);
    progressBox->setRange(0, 100);
    progressBox->setSmallChange(1);
    progressBox->setLargeChange(10);
    progressBox->setDisplayPrecision(0);
    progressBox->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);

    QObject::connect(progressBox, &NumberBox::valueChanged, bar, [bar](double value) {
        bar->setValue(std::isfinite(value) ? value : 0.0);
    });

    progressBox->setValue(1);
    EXPECT_DOUBLE_EQ(bar->value(), 1.0);
    EXPECT_EQ(bar->progressText(), "1");

    progressBox->setValue(2);
    EXPECT_DOUBLE_EQ(bar->value(), 2.0);
    EXPECT_EQ(bar->progressText(), "2");

    progressBox->setValue(3);
    EXPECT_DOUBLE_EQ(bar->value(), 3.0);
    EXPECT_EQ(bar->progressText(), "3");

    progressBox->setValue(std::numeric_limits<double>::quiet_NaN());
    EXPECT_DOUBLE_EQ(bar->value(), 0.0);
    EXPECT_EQ(bar->progressText(), "0");

    root->addWidget(bar, 0, Qt::AlignVCenter);
    root->addWidget(progressBox, 0, Qt::AlignVCenter);
    root->addStretch();

    window->show();
    QApplication::processEvents();

    auto* upButton = progressBox->findChild<RepeatButton*>("NumberBoxSpinUpButton");
    ASSERT_NE(upButton, nullptr);
    ASSERT_FALSE(upButton->isHidden());

    QSignalSpy barValueSpy(bar, &ProgressBar::valueChanged);
    upButton->click();
    QApplication::processEvents();
    EXPECT_DOUBLE_EQ(progressBox->value(), 1.0);
    EXPECT_DOUBLE_EQ(bar->value(), 1.0);
    EXPECT_EQ(bar->progressText(), "1");
    EXPECT_EQ(barValueSpy.count(), 1);
}

TEST_F(ProgressBarTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP() << "Skipping visual test in offscreen mode";
    }

    window->resize(900, 760);

    auto* root = new QVBoxLayout(window);
    root->setContentsMargins(24, 22, 24, 24);
    root->setSpacing(14);

    auto* header = new QHBoxLayout();
    auto* title = new QLabel("ProgressBar", window);
    QFont titleFont = title->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto* themeButton = new QPushButton("Toggle Light/Dark", window);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(themeButton);
    root->addLayout(header);

    auto* description = new QLabel(
        "Indeterminate shows an ongoing operation. Determinate shows progress for a known amount of work.",
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

    root->addWidget(makeSectionLabel("An indeterminate progress bar."));

    auto* indeterminatePanel = makePanel(150);
    auto* indeterminateLayout = new QHBoxLayout(indeterminatePanel);
    indeterminateLayout->setContentsMargins(32, 20, 32, 20);
    auto* indeterminateBar = new ProgressBar(indeterminatePanel);
    indeterminateBar->setBarWidth(130);
    indeterminateBar->setIsIndeterminate(true);
    indeterminateLayout->addWidget(indeterminateBar, 0, Qt::AlignLeft | Qt::AlignVCenter);
    indeterminateLayout->addStretch();
    root->addWidget(indeterminatePanel);

    root->addWidget(makeSectionLabel("A determinate progress bar."));

    auto* determinatePanel = makePanel(126);
    auto* determinateLayout = new QHBoxLayout(determinatePanel);
    determinateLayout->setContentsMargins(34, 18, 34, 18);
    determinateLayout->setSpacing(26);
    auto* determinateBar = new ProgressBar(determinatePanel);
    determinateBar->setBarWidth(130);

    auto* progressBox = new NumberBox(determinatePanel);
    progressBox->setHeader("Progress");
    progressBox->setFixedWidth(156);
    progressBox->setRange(0, 100);
    progressBox->setSmallChange(1);
    progressBox->setLargeChange(10);
    progressBox->setDisplayPrecision(0);
    progressBox->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);
    QObject::connect(progressBox, &NumberBox::valueChanged, determinateBar, [determinateBar](double value) {
        determinateBar->setValue(std::isfinite(value) ? value : 0.0);
    });
    progressBox->setValue(32);

    determinateLayout->addWidget(determinateBar, 0, Qt::AlignLeft | Qt::AlignVCenter);
    determinateLayout->addWidget(progressBox, 0, Qt::AlignVCenter);
    determinateLayout->addStretch();
    root->addWidget(determinatePanel);

    root->addWidget(makeSectionLabel("Representative states."));

    auto* variantsPanel = makePanel(260);
    auto* grid = new QGridLayout(variantsPanel);
    grid->setContentsMargins(26, 20, 26, 20);
    grid->setHorizontalSpacing(28);
    grid->setVerticalSpacing(18);
    root->addWidget(variantsPanel);

    auto addExample = [grid, variantsPanel](int row, int column, const QString& labelText,
                                            bool indeterminate,
                                            bool paused,
                                            bool error,
                                            double value,
                                            int width,
                                            bool railVisible,
                                            bool enabled) {
        auto* cell = new QWidget(variantsPanel);
        auto* cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(8);
        cellLayout->setAlignment(Qt::AlignHCenter);

        auto* bar = new ProgressBar(cell);
        bar->setIsIndeterminate(indeterminate);
        bar->setShowPaused(paused);
        bar->setShowError(error);
        bar->setValue(value);
        bar->setBarWidth(width);
        bar->setRailVisible(railVisible);
        bar->setEnabled(enabled);

        auto* label = new QLabel(labelText, cell);
        label->setAlignment(Qt::AlignCenter);
        cellLayout->addWidget(bar, 0, Qt::AlignHCenter);
        cellLayout->addWidget(label, 0, Qt::AlignHCenter);
        grid->addWidget(cell, row, column);
    };

    addExample(0, 0, "Determinate 68%", false, false, false, 68, 220, true, true);
    addExample(0, 1, "Paused", false, true, false, 68, 220, true, true);
    addExample(0, 2, "Error", false, false, true, 68, 220, true, true);
    addExample(1, 0, "Indeterminate", true, false, false, 0, 220, true, true);
    addExample(1, 1, "Rail hidden", false, false, false, 42, 220, false, true);
    addExample(1, 2, "Disabled", false, false, false, 68, 220, true, false);

    window->show();
    qApp->exec();
}
