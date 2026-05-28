#include <gtest/gtest.h>

#include <QApplication>
#include <QPalette>
#include <QTimer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <cmath>
#include <limits>

#include "design/Spacing.h"
#include "design/Typography.h"
#include "view/foundation/QMLPlus.h"
#include "view/basicinput/Button.h"
#include "view/basicinput/RepeatButton.h"
#include "view/textfields/NumberBox.h"
#include "view/textfields/Label.h"

using namespace view;
using namespace view::basicinput;
using namespace view::textfields;

class NumberBoxTestWindow : public QWidget, public FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& colors = themeColors();
        QPalette pal = palette();
        pal.setColor(QPalette::Window, colors.bgCanvas);
        setPalette(pal);
        setAutoFillBackground(true);
    }
};

class NumberBoxTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<NumberBox::SpinButtonPlacementMode>("NumberBox::SpinButtonPlacementMode");
    }

    void SetUp() override {
        FluentElement::setTheme(FluentElement::Light);
        window = new NumberBoxTestWindow();
        window->setFixedSize(620, 520);
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
        FluentElement::setTheme(FluentElement::Light);
    }

    void showAndFocus(NumberBox* box) {
        window->show();
        box->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents();
    }

    void commit(NumberBox* box, const QString& text) {
        box->setText(text);
        showAndFocus(box);
        QTest::keyClick(box, Qt::Key_Return);
        QApplication::processEvents();
    }

    NumberBoxTestWindow* window = nullptr;
    AnchorLayout* layout = nullptr;
};

TEST_F(NumberBoxTest, DefaultsAndSizeHint) {
    NumberBox box(window);

    EXPECT_TRUE(std::isnan(box.value()));
    EXPECT_TRUE(std::isinf(box.minimum()));
    EXPECT_LT(box.minimum(), 0.0);
    EXPECT_TRUE(std::isinf(box.maximum()));
    EXPECT_GT(box.maximum(), 0.0);
    EXPECT_DOUBLE_EQ(box.smallChange(), 1.0);
    EXPECT_DOUBLE_EQ(box.largeChange(), 10.0);
    EXPECT_TRUE(box.header().isEmpty());
    EXPECT_FALSE(box.acceptsExpression());
    EXPECT_EQ(box.spinButtonPlacementMode(), NumberBox::SpinButtonPlacementMode::Hidden);
    EXPECT_EQ(box.spinButtonSize(), QSize(24, 14));
    EXPECT_EQ(box.inlineSpinButtonSize(), QSize(24, 20));
    EXPECT_EQ(box.spinButtonRightMargin(), 4);
    EXPECT_EQ(box.compactSpinButtonReservedWidth(), 14);
    EXPECT_EQ(box.spinButtonSpacing(), 2);
    EXPECT_EQ(box.spinButtonTextGap(), 2);
    EXPECT_EQ(box.spinButtonIconSize(), 10);
    EXPECT_EQ(box.displayPrecision(), -1);
    EXPECT_DOUBLE_EQ(box.formatStep(), 0.0);
    EXPECT_EQ(box.sizeHint().height(), 32);
    EXPECT_GE(box.minimumSizeHint().width(), 124);
}

TEST_F(NumberBoxTest, PropertySignalsAndNaNDedupe) {
    NumberBox box(window);
    QSignalSpy valueSpy(&box, &NumberBox::valueChanged);
    QSignalSpy minimumSpy(&box, &NumberBox::minimumChanged);
    QSignalSpy maximumSpy(&box, &NumberBox::maximumChanged);
    QSignalSpy smallSpy(&box, &NumberBox::smallChangeChanged);
    QSignalSpy largeSpy(&box, &NumberBox::largeChangeChanged);
    QSignalSpy modeSpy(&box, &NumberBox::spinButtonPlacementModeChanged);

    box.setValue(42.5);
    ASSERT_EQ(valueSpy.count(), 1);
    EXPECT_DOUBLE_EQ(box.value(), 42.5);
    EXPECT_EQ(box.text(), "42.5");

    box.setValue(42.5);
    EXPECT_EQ(valueSpy.count(), 1);

    box.setValue(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(valueSpy.count(), 2);
    EXPECT_TRUE(std::isnan(box.value()));
    EXPECT_TRUE(box.text().isEmpty());

    box.setValue(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(valueSpy.count(), 2);

    box.setMinimum(10);
    box.setMaximum(100);
    box.setSmallChange(2);
    box.setLargeChange(20);
    box.setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);
    EXPECT_EQ(minimumSpy.count(), 1);
    EXPECT_EQ(maximumSpy.count(), 1);
    EXPECT_EQ(smallSpy.count(), 1);
    EXPECT_EQ(largeSpy.count(), 1);
    EXPECT_EQ(modeSpy.count(), 1);
}

TEST_F(NumberBoxTest, RangeClampAndInvalidSteps) {
    NumberBox box(window);
    box.setMinimum(10);
    box.setValue(5);
    EXPECT_DOUBLE_EQ(box.value(), 10);
    EXPECT_EQ(box.text(), "10");

    box.setMaximum(100);
    box.setValue(120);
    EXPECT_DOUBLE_EQ(box.value(), 100);
    EXPECT_EQ(box.text(), "100");

    box.setRange(50, 20);
    EXPECT_DOUBLE_EQ(box.minimum(), 50);
    EXPECT_DOUBLE_EQ(box.maximum(), 50);
    EXPECT_DOUBLE_EQ(box.value(), 50);

    box.setSmallChange(4);
    box.setLargeChange(40);
    box.setSmallChange(0);
    box.setLargeChange(-1);
    EXPECT_DOUBLE_EQ(box.smallChange(), 4);
    EXPECT_DOUBLE_EQ(box.largeChange(), 40);
}

TEST_F(NumberBoxTest, ParsesNumbersAndExpressions) {
    auto* box = new NumberBox(window);
    box->setFixedWidth(220);
    layout->addWidget(box);

    commit(box, "60");
    EXPECT_DOUBLE_EQ(box->value(), 60);
    EXPECT_EQ(box->text(), "60");

    commit(box, "");
    EXPECT_TRUE(std::isnan(box->value()));
    EXPECT_TRUE(box->text().isEmpty());

    commit(box, "1 + 2^2");
    EXPECT_TRUE(std::isnan(box->value()));
    EXPECT_EQ(box->text(), "1 + 2^2");

    box->setAcceptsExpression(true);
    commit(box, "1 + 2^2");
    EXPECT_DOUBLE_EQ(box->value(), 5);
    EXPECT_EQ(box->text(), "5");

    commit(box, "(2 + 3) * -4 / 2");
    EXPECT_DOUBLE_EQ(box->value(), -10);
    EXPECT_EQ(box->text(), "-10");

    commit(box, "4 / 0");
    EXPECT_TRUE(std::isnan(box->value()));
    EXPECT_EQ(box->text(), "4 / 0");

    commit(box, "(1 + 2");
    EXPECT_TRUE(std::isnan(box->value()));
    EXPECT_EQ(box->text(), "(1 + 2");
}

TEST_F(NumberBoxTest, KeyboardAndSpinnerStep) {
    auto* box = new NumberBox(window);
    box->setFixedWidth(220);
    box->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);
    box->setValue(10);
    layout->addWidget(box);
    showAndFocus(box);

    QTest::keyClick(box, Qt::Key_Up);
    EXPECT_DOUBLE_EQ(box->value(), 11);
    QTest::keyClick(box, Qt::Key_Down);
    EXPECT_DOUBLE_EQ(box->value(), 10);
    QTest::keyClick(box, Qt::Key_PageUp);
    EXPECT_DOUBLE_EQ(box->value(), 20);
    QTest::keyClick(box, Qt::Key_PageDown);
    EXPECT_DOUBLE_EQ(box->value(), 10);

    auto* upButton = box->findChild<RepeatButton*>("NumberBoxSpinUpButton");
    auto* downButton = box->findChild<RepeatButton*>("NumberBoxSpinDownButton");
    ASSERT_NE(upButton, nullptr);
    ASSERT_NE(downButton, nullptr);
    ASSERT_FALSE(upButton->isHidden());
    ASSERT_FALSE(downButton->isHidden());

    upButton->click();
    EXPECT_DOUBLE_EQ(box->value(), 11);
    downButton->click();
    EXPECT_DOUBLE_EQ(box->value(), 10);
    EXPECT_TRUE(box->hasFocus());
}

TEST_F(NumberBoxTest, SpinButtonsDisableAtRangeEdges) {
    auto* box = new NumberBox(window);
    box->resize(240, box->sizeHint().height());
    box->setRange(0, 2);
    box->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);
    layout->addWidget(box);

    auto* upButton = box->findChild<RepeatButton*>("NumberBoxSpinUpButton");
    auto* downButton = box->findChild<RepeatButton*>("NumberBoxSpinDownButton");
    ASSERT_NE(upButton, nullptr);
    ASSERT_NE(downButton, nullptr);

    box->setValue(0);
    EXPECT_TRUE(upButton->isEnabled());
    EXPECT_FALSE(downButton->isEnabled());

    upButton->click();
    EXPECT_DOUBLE_EQ(box->value(), 1);
    EXPECT_TRUE(upButton->isEnabled());
    EXPECT_TRUE(downButton->isEnabled());

    upButton->click();
    EXPECT_DOUBLE_EQ(box->value(), 2);
    EXPECT_FALSE(upButton->isEnabled());
    EXPECT_TRUE(downButton->isEnabled());

    downButton->click();
    EXPECT_DOUBLE_EQ(box->value(), 1);
    EXPECT_TRUE(upButton->isEnabled());
    EXPECT_TRUE(downButton->isEnabled());

    box->setRange(0, 1);
    EXPECT_FALSE(upButton->isEnabled());
    EXPECT_TRUE(downButton->isEnabled());

    box->setRange(1, 1);
    EXPECT_FALSE(upButton->isEnabled());
    EXPECT_FALSE(downButton->isEnabled());

    box->setValue(std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(upButton->isEnabled());
    EXPECT_TRUE(downButton->isEnabled());

    box->setRange(0, 2);
    box->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Compact);
    box->setValue(2);
    EXPECT_FALSE(upButton->isEnabled());
    EXPECT_TRUE(downButton->isEnabled());

    box->setValue(0);
    EXPECT_TRUE(upButton->isEnabled());
    EXPECT_FALSE(downButton->isEnabled());
}

TEST_F(NumberBoxTest, NaNStepStartUsesZeroOrNearestBoundary) {
    NumberBox box(window);
    box.setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);

    box.setValue(std::numeric_limits<double>::quiet_NaN());
    QTest::keyClick(&box, Qt::Key_Up);
    EXPECT_DOUBLE_EQ(box.value(), 1);

    box.setRange(10, 20);
    box.setValue(std::numeric_limits<double>::quiet_NaN());
    QTest::keyClick(&box, Qt::Key_Down);
    EXPECT_DOUBLE_EQ(box.value(), 10);
}

TEST_F(NumberBoxTest, SpinButtonPlacementAndMargins) {
    auto* box = new NumberBox(window);
    box->resize(240, box->sizeHint().height());
    layout->addWidget(box);
    auto* upButton = box->findChild<RepeatButton*>("NumberBoxSpinUpButton");
    auto* downButton = box->findChild<RepeatButton*>("NumberBoxSpinDownButton");
    ASSERT_NE(upButton, nullptr);
    ASSERT_NE(downButton, nullptr);

    EXPECT_TRUE(upButton->isHidden());
    const int hiddenMargin = box->contentMargins().right();

    box->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);
    EXPECT_FALSE(upButton->isHidden());
    EXPECT_FALSE(downButton->isHidden());
    EXPECT_GT(box->contentMargins().right(), hiddenMargin);

    box->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Compact);
    box->clearFocus();
    QApplication::processEvents();
    EXPECT_FALSE(upButton->isHidden());
    EXPECT_FALSE(downButton->isHidden());
    EXPECT_GT(box->contentMargins().right(), hiddenMargin);

    box->setEnabled(false);
    QApplication::processEvents();
    EXPECT_FALSE(upButton->isEnabled());
    EXPECT_FALSE(downButton->isEnabled());
}

TEST_F(NumberBoxTest, InlineSpinButtonsAreLaidOutHorizontally) {
    auto* box = new NumberBox(window);
    box->resize(260, box->sizeHint().height());
    box->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);
    layout->addWidget(box);

    auto* upButton = box->findChild<RepeatButton*>("NumberBoxSpinUpButton");
    auto* downButton = box->findChild<RepeatButton*>("NumberBoxSpinDownButton");
    ASSERT_NE(upButton, nullptr);
    ASSERT_NE(downButton, nullptr);

    EXPECT_FALSE(upButton->isHidden());
    EXPECT_FALSE(downButton->isHidden());
    EXPECT_EQ(upButton->fluentStyle(), Button::Standard);
    EXPECT_EQ(downButton->fluentStyle(), Button::Standard);
    EXPECT_EQ(upButton->size(), box->inlineSpinButtonSize());
    EXPECT_EQ(downButton->size(), box->inlineSpinButtonSize());
    EXPECT_GT(upButton->height(), box->spinButtonSize().height());
    EXPECT_EQ(upButton->y(), downButton->y());
    EXPECT_EQ(upButton->x(), downButton->x() + downButton->width() + box->spinButtonSpacing());
    EXPECT_EQ(upButton->geometry().right(), box->width() - box->spinButtonRightMargin() - 1);

    const int expectedMargin = ::Spacing::Padding::TextFieldHorizontal + box->spinButtonRightMargin()
        + box->inlineSpinButtonSize().width() * 2 + box->spinButtonSpacing() + box->spinButtonTextGap();
    EXPECT_EQ(box->contentMargins().right(), expectedMargin);
}

TEST_F(NumberBoxTest, CompactSpinButtonsAreVisibleByDefault) {
    auto* box = new NumberBox(window);
    box->resize(260, box->sizeHint().height());
    box->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Compact);
    layout->addWidget(box);

    auto* upButton = box->findChild<RepeatButton*>("NumberBoxSpinUpButton");
    auto* downButton = box->findChild<RepeatButton*>("NumberBoxSpinDownButton");
    ASSERT_NE(upButton, nullptr);
    ASSERT_NE(downButton, nullptr);

    EXPECT_FALSE(upButton->isHidden());
    EXPECT_FALSE(downButton->isHidden());
    EXPECT_EQ(upButton->fluentStyle(), Button::Standard);
    EXPECT_EQ(downButton->fluentStyle(), Button::Standard);
    EXPECT_EQ(upButton->size(), box->spinButtonSize());
    EXPECT_EQ(downButton->size(), box->spinButtonSize());
    EXPECT_EQ(upButton->x(), downButton->x());
    EXPECT_EQ(downButton->y(), upButton->y() + upButton->height());
    EXPECT_GT(upButton->y(), 0);
    EXPECT_LT(downButton->geometry().bottom(), box->height() - 1);

    const int expectedMargin = ::Spacing::Padding::TextFieldHorizontal + box->spinButtonRightMargin()
        + box->spinButtonSize().width() + box->spinButtonTextGap();
    EXPECT_EQ(box->contentMargins().right(), expectedMargin);

    showAndFocus(box);
    box->clearFocus();
    QApplication::processEvents();
    EXPECT_FALSE(upButton->isHidden());
    EXPECT_FALSE(downButton->isHidden());
}

TEST_F(NumberBoxTest, ConfigurableSpinButtonMetrics) {
    auto* box = new NumberBox(window);
    box->resize(260, box->sizeHint().height());
    box->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);
    layout->addWidget(box);

    auto* upButton = box->findChild<RepeatButton*>("NumberBoxSpinUpButton");
    auto* downButton = box->findChild<RepeatButton*>("NumberBoxSpinDownButton");
    ASSERT_NE(upButton, nullptr);
    ASSERT_NE(downButton, nullptr);

    QSignalSpy sizeSpy(box, &NumberBox::spinButtonSizeChanged);
    QSignalSpy inlineSizeSpy(box, &NumberBox::inlineSpinButtonSizeChanged);
    QSignalSpy marginSpy(box, &NumberBox::spinButtonRightMarginChanged);
    QSignalSpy reservedSpy(box, &NumberBox::compactSpinButtonReservedWidthChanged);
    QSignalSpy spacingSpy(box, &NumberBox::spinButtonSpacingChanged);
    QSignalSpy gapSpy(box, &NumberBox::spinButtonTextGapChanged);
    QSignalSpy iconSpy(box, &NumberBox::spinButtonIconSizeChanged);

    box->setInlineSpinButtonSize(QSize(28, 20));
    EXPECT_EQ(inlineSizeSpy.count(), 1);
    EXPECT_EQ(box->inlineSpinButtonSize(), QSize(28, 20));
    EXPECT_EQ(upButton->size(), QSize(28, 20));
    EXPECT_EQ(downButton->size(), QSize(28, 20));
    EXPECT_EQ(upButton->y(), downButton->y());
    EXPECT_EQ(upButton->x(), downButton->x() + downButton->width() + box->spinButtonSpacing());

    int expectedMargin = ::Spacing::Padding::TextFieldHorizontal + box->spinButtonRightMargin()
        + box->inlineSpinButtonSize().width() * 2 + box->spinButtonSpacing() + box->spinButtonTextGap();
    EXPECT_EQ(box->contentMargins().right(), expectedMargin);

    box->setSpinButtonSize(QSize(28, 12));
    EXPECT_EQ(sizeSpy.count(), 1);
    EXPECT_EQ(box->spinButtonSize(), QSize(28, 12));
    EXPECT_EQ(upButton->size(), QSize(28, 20));
    EXPECT_EQ(downButton->size(), QSize(28, 20));

    box->setSpinButtonRightMargin(8);
    box->setSpinButtonSpacing(4);
    box->setSpinButtonTextGap(6);
    EXPECT_EQ(marginSpy.count(), 1);
    EXPECT_EQ(spacingSpy.count(), 1);
    EXPECT_EQ(gapSpy.count(), 1);
    expectedMargin = ::Spacing::Padding::TextFieldHorizontal + 8 + 28 * 2 + 4 + 6;
    EXPECT_EQ(box->contentMargins().right(), expectedMargin);

    box->setSpinButtonIconSize(12);
    EXPECT_EQ(iconSpy.count(), 1);
    EXPECT_EQ(box->spinButtonIconSize(), 12);

    box->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Compact);
    box->clearFocus();
    EXPECT_EQ(upButton->size(), QSize(28, 12));
    EXPECT_EQ(downButton->size(), QSize(28, 12));
    box->setCompactSpinButtonReservedWidth(20);
    EXPECT_EQ(reservedSpy.count(), 1);
    EXPECT_EQ(box->compactSpinButtonReservedWidth(), 20);
    expectedMargin = ::Spacing::Padding::TextFieldHorizontal + 8 + 28 + 6;
    EXPECT_EQ(box->contentMargins().right(), expectedMargin);

    box->setSpinButtonSize(QSize(0, 12));
    box->setInlineSpinButtonSize(QSize(0, 20));
    box->setSpinButtonIconSize(0);
    EXPECT_EQ(sizeSpy.count(), 1);
    EXPECT_EQ(inlineSizeSpy.count(), 1);
    EXPECT_EQ(iconSpy.count(), 1);
}

TEST_F(NumberBoxTest, FormattingPrecisionAndFormatStep) {
    auto* box = new NumberBox(window);
    box->setFixedWidth(220);
    layout->addWidget(box);

    box->setDisplayPrecision(2);
    box->setValue(3);
    EXPECT_DOUBLE_EQ(box->value(), 3);
    EXPECT_EQ(box->text(), "3.00");

    box->setFormatStep(0.25);
    commit(box, "1.13");
    EXPECT_DOUBLE_EQ(box->value(), 1.25);
    EXPECT_EQ(box->text(), "1.25");

    box->setFormatStep(-1);
    EXPECT_DOUBLE_EQ(box->formatStep(), 0.0);
}

TEST_F(NumberBoxTest, HeaderSizeHint) {
    NumberBox box(window);
    EXPECT_EQ(box.sizeHint().height(), 32);
    box.setHeader("Amount");
    EXPECT_EQ(box.sizeHint().height(), 60);
    EXPECT_EQ(box.minimumSizeHint().height(), 60);
}

TEST_F(NumberBoxTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;
    window->setFixedSize(620, 560);

    auto* title = new Label("NumberBox", window);
    title->setFluentTypography(Typography::FontRole::Subtitle);
    title->anchors()->top = {window, Edge::Top, 28};
    title->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(title);

    auto* equation = new NumberBox(window);
    equation->setHeader("Equation");
    equation->setPlaceholderText("1 + 2^2");
    equation->setAcceptsExpression(true);
    equation->anchors()->top = {title, Edge::Bottom, 18};
    equation->anchors()->left = {window, Edge::Left, 40};
    equation->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(equation);

    auto* inlineSpinner = new NumberBox(window);
    inlineSpinner->setHeader("Inline spinner");
    inlineSpinner->setValue(50);
    inlineSpinner->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);
    inlineSpinner->anchors()->top = {equation, Edge::Bottom, 20};
    inlineSpinner->anchors()->left = {window, Edge::Left, 40};
    inlineSpinner->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(inlineSpinner);

    auto* compactSpinner = new NumberBox(window);
    compactSpinner->setHeader("Compact spinner");
    compactSpinner->setValue(12);
    compactSpinner->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Compact);
    compactSpinner->anchors()->top = {inlineSpinner, Edge::Bottom, 20};
    compactSpinner->anchors()->left = {window, Edge::Left, 40};
    compactSpinner->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(compactSpinner);

    auto* amount = new NumberBox(window);
    amount->setHeader("Formatted amount");
    amount->setDisplayPrecision(2);
    amount->setFormatStep(0.25);
    amount->setValue(1.13);
    amount->anchors()->top = {compactSpinner, Edge::Bottom, 20};
    amount->anchors()->left = {window, Edge::Left, 40};
    amount->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(amount);

    auto* disabled = new NumberBox(window);
    disabled->setHeader("Disabled");
    disabled->setValue(100);
    disabled->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);
    disabled->setEnabled(false);
    disabled->anchors()->top = {amount, Edge::Bottom, 20};
    disabled->anchors()->left = {window, Edge::Left, 40};
    disabled->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(disabled);

    auto* themeButton = new Button("Switch Theme", window);
    themeButton->setFluentStyle(Button::Accent);
    themeButton->setFixedSize(120, 32);
    themeButton->anchors()->bottom = {window, Edge::Bottom, -28};
    themeButton->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(themeButton);

    QObject::connect(themeButton, &Button::clicked, []() {
        FluentElement::setTheme(FluentElement::currentTheme() == FluentElement::Light
            ? FluentElement::Dark : FluentElement::Light);
    });

    window->show();
    QTimer::singleShot(0, [equation]() {
        equation->setFocus(Qt::OtherFocusReason);
        QTest::keyClicks(equation, "1 + 2^2");
    });
    qApp->exec();
}
