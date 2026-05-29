#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QButtonGroup>
#include <QSignalSpy>
#include "components/basicinput/RadioButton.h"
#include "components/basicinput/Button.h"
#include "components/foundation/FluentElement.h"

using namespace fluent;
using namespace fluent::basicinput;

// ── FluentTestWindow ─────────────────────────────────────────────────────────

class RadioButtonTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

// ── Test Fixture ─────────────────────────────────────────────────────────────

class RadioButtonTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new RadioButtonTestWindow();
        window->setFixedSize(600, 600);
        window->setWindowTitle("Fluent RadioButton Test");
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    RadioButtonTestWindow* window = nullptr;
};

// ── Helper: RadioButton + QLabel row ─────────────────────────────────────────

static QHBoxLayout* makeRadioRow(RadioButton* rb, const QString& label, QWidget* parent) {
    auto* row = new QHBoxLayout();
    row->setSpacing(8);
    row->addWidget(rb);
    row->addWidget(new QLabel(label, parent));
    row->addStretch();
    return row;
}

// ── Unit Tests ───────────────────────────────────────────────────────────────

TEST_F(RadioButtonTest, DefaultProperties) {
    RadioButton rb(window);
    EXPECT_EQ(rb.circleSize(), 20);
    EXPECT_EQ(rb.textGap(), 8);
    EXPECT_TRUE(rb.text().isEmpty());
    EXPECT_FALSE(rb.isChecked());
    EXPECT_TRUE(rb.isEnabled());
}

TEST_F(RadioButtonTest, DefaultPropertiesWithText) {
    RadioButton rb("Option", window);
    EXPECT_EQ(rb.text(), "Option");
    EXPECT_EQ(rb.circleSize(), 20);
    EXPECT_EQ(rb.textGap(), 8);
    EXPECT_FALSE(rb.isChecked());
}

TEST_F(RadioButtonTest, EmptyText) {
    RadioButton rb("", window);
    EXPECT_TRUE(rb.text().isEmpty());
    // sizeHint with empty text = circle only
    QSize hint = rb.sizeHint();
    EXPECT_EQ(hint.width(), 20);
}

TEST_F(RadioButtonTest, SetCircleSize) {
    RadioButton rb(window);
    QSignalSpy spy(&rb, &RadioButton::circleSizeChanged);
    rb.setCircleSize(24);
    EXPECT_EQ(rb.circleSize(), 24);
    EXPECT_EQ(spy.count(), 1);

    // Same value → no signal
    rb.setCircleSize(24);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(RadioButtonTest, SetTextGap) {
    RadioButton rb("Test", window);
    QSignalSpy spy(&rb, &RadioButton::textGapChanged);
    rb.setTextGap(12);
    EXPECT_EQ(rb.textGap(), 12);
    EXPECT_EQ(spy.count(), 1);

    // Same value → no signal
    rb.setTextGap(12);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(RadioButtonTest, SetTextFont) {
    RadioButton rb("Test", window);
    QSignalSpy spy(&rb, &RadioButton::textFontChanged);
    QFont customFont("Arial", 16);
    rb.setTextFont(customFont);
    EXPECT_EQ(rb.textFont().family(), customFont.family());
    EXPECT_EQ(rb.textFont().pointSize(), 16);
    EXPECT_EQ(spy.count(), 1);

    // Same value → no signal
    rb.setTextFont(customFont);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(RadioButtonTest, SizeHintWithText) {
    RadioButton rb("Hello World", window);
    QSize hint = rb.sizeHint();
    // With text: width > circleSize, height >= circleSize
    EXPECT_GT(hint.width(), 20);
    EXPECT_GE(hint.height(), 20);
}

TEST_F(RadioButtonTest, SizeHintNoText) {
    RadioButton rb(window);
    QSize hint = rb.sizeHint();
    // No text: width == circleSize
    EXPECT_EQ(hint.width(), 20);
}

TEST_F(RadioButtonTest, CheckedState) {
    RadioButton rb1("A", window);
    RadioButton rb2("B", window);
    QButtonGroup group;
    group.addButton(&rb1);
    group.addButton(&rb2);

    rb1.setChecked(true);
    EXPECT_TRUE(rb1.isChecked());
    EXPECT_FALSE(rb2.isChecked());

    rb2.setChecked(true);
    EXPECT_FALSE(rb1.isChecked());
    EXPECT_TRUE(rb2.isChecked());
}

TEST_F(RadioButtonTest, DisabledState) {
    RadioButton rb("Disabled", window);
    rb.setEnabled(false);
    EXPECT_FALSE(rb.isEnabled());
    rb.setChecked(true);
    EXPECT_TRUE(rb.isChecked());
}

TEST_F(RadioButtonTest, PaintDoesNotCrash) {
    RadioButton rb("Paint test", window);
    window->show();
    rb.show();
    rb.repaint();

    rb.setChecked(true);
    rb.repaint();

    rb.setEnabled(false);
    rb.repaint();
}

TEST_F(RadioButtonTest, PaintEmptyTextDoesNotCrash) {
    RadioButton rb(window);
    window->show();
    rb.show();
    rb.repaint();
    rb.setChecked(true);
    rb.repaint();
}

TEST_F(RadioButtonTest, ThemeUpdateDoesNotCrash) {
    RadioButton rb("Theme", window);
    rb.onThemeUpdated();
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    rb.onThemeUpdated();
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
}

// ── VisualCheck ──────────────────────────────────────────────────────────────

TEST_F(RadioButtonTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") &&
        qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP() << "Skipping visual test in offscreen mode";
    }

    auto* layout = new QVBoxLayout(window);
    layout->setContentsMargins(40, 30, 40, 30);
    layout->setSpacing(12);

    // ── Section 1: Basic RadioButtons group ──
    layout->addWidget(new QLabel("1. A group of RadioButton controls:", window));

    auto* group1 = new QButtonGroup(window);
    auto* outputLabel = new QLabel("Output: Select an option.", window);

    auto* rb1 = new RadioButton("Option 1", window);
    auto* rb2 = new RadioButton("Option 2", window);
    auto* rb3 = new RadioButton("Option 3", window);
    group1->addButton(rb1, 0);
    group1->addButton(rb2, 1);
    group1->addButton(rb3, 2);

    layout->addWidget(rb1);
    layout->addWidget(rb2);
    layout->addWidget(rb3);
    layout->addWidget(outputLabel);

    QObject::connect(group1,
                     qOverload<QAbstractButton*>(&QButtonGroup::buttonClicked),
                     [outputLabel](QAbstractButton* btn) {
        outputLabel->setText(QString("Output: You selected %1").arg(btn->text()));
    });

    layout->addSpacing(10);

    // ── Section 2: Two groups controlling a colored box ──
    layout->addWidget(new QLabel("2. Two RadioButton groups with color selection:", window));

    auto makeColorGroup = [&](const QString& header,
                              const QStringList& items) -> QButtonGroup* {
        layout->addWidget(new QLabel(header, window));
        auto* grp = new QButtonGroup(window);
        auto* row = new QHBoxLayout();
        for (int i = 0; i < items.size(); ++i) {
            auto* rb = new RadioButton(items[i], window);
            grp->addButton(rb, i);
            row->addWidget(rb);
            row->addSpacing(12);
            if (i == 0) rb->setChecked(true);
        }
        row->addStretch();
        layout->addLayout(row);
        return grp;
    };

    auto* bgGroup = makeColorGroup("Background:", {"Green", "Yellow", "White"});
    auto* bdGroup = makeColorGroup("Border:", {"Green", "Yellow", "White"});

    auto* colorBox = new QWidget(window);
    colorBox->setFixedHeight(50);
    colorBox->setStyleSheet("background-color: green; border: 4px solid green;");
    layout->addWidget(colorBox);

    auto syncColors = [colorBox, bgGroup, bdGroup]() {
        auto colorFor = [](int idx) -> QString {
            switch (idx) {
            case 0: return "green";
            case 1: return "yellow";
            default: return "white";
            }
        };
        colorBox->setStyleSheet(
            QString("background-color: %1; border: 4px solid %2;")
                .arg(colorFor(bgGroup->checkedId()), colorFor(bdGroup->checkedId())));
    };
    QObject::connect(bgGroup, qOverload<QAbstractButton*>(&QButtonGroup::buttonClicked), syncColors);
    QObject::connect(bdGroup, qOverload<QAbstractButton*>(&QButtonGroup::buttonClicked), syncColors);

    layout->addSpacing(10);

    // ── Section 3: Disabled states ──
    layout->addWidget(new QLabel("3. Disabled states:", window));
    auto* disabledRow = new QHBoxLayout();
    auto* dOff = new RadioButton("Disabled Off", window);
    dOff->setEnabled(false);
    auto* dOn = new RadioButton("Disabled On", window);
    dOn->setChecked(true);
    dOn->setEnabled(false);
    disabledRow->addWidget(dOff);
    disabledRow->addSpacing(20);
    disabledRow->addWidget(dOn);
    disabledRow->addStretch();
    layout->addLayout(disabledRow);

    layout->addSpacing(10);

    // ── Section 4: Empty text (circle-only) ──
    layout->addWidget(new QLabel("4. Empty text (circle-only indicators):", window));
    auto* emptyGroup = new QButtonGroup(window);
    auto* emptyRow = new QHBoxLayout();
    for (int i = 0; i < 3; ++i) {
        auto* rb = new RadioButton(window);  // no text
        emptyGroup->addButton(rb, i);
        emptyRow->addWidget(rb);
        emptyRow->addSpacing(8);
        if (i == 0) rb->setChecked(true);
    }
    emptyRow->addStretch();
    layout->addLayout(emptyRow);

    layout->addStretch();

    // ── Theme Switcher ──
    auto* themeBtn = new Button("Switch Theme", window);
    themeBtn->setFixedSize(120, 32);
    layout->addWidget(themeBtn, 0, Qt::AlignRight);
    QObject::connect(themeBtn, &Button::clicked, []() {
        fluent::FluentElement::setTheme(
            fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                ? fluent::FluentElement::Dark
                : fluent::FluentElement::Light);
    });

    window->show();
    qApp->exec();
}
