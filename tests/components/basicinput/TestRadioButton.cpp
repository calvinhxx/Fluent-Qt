#include "components/basicinput/Button.h"
#include "components/basicinput/RadioButton.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "SelectionIndicatorTestUtils.h"
#include "ControlSurfaceTestUtils.h"
#include <QApplication>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QSignalSpy>
#include <QStyle>
#include <QTest>
#include <QtMath>
#include <QVBoxLayout>
#include <gtest/gtest.h>

using namespace fluent;
using namespace fluent::basicinput;

// ── FluentTestWindow ─────────────────────────────────────────────────────────

class RadioButtonTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

// ── Test Fixture ─────────────────────────────────────────────────────────────

class RadioButtonTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window = new RadioButtonTestWindow();
        window->setFixedSize(600, 600);
        window->setWindowTitle("Fluent RadioButton Test");
        window->onThemeUpdated();
    }

    void TearDown() override { delete window; }

    RadioButtonTestWindow* window = nullptr;
};

// ── Helper: RadioButton + QLabel row ─────────────────────────────────────────

static QHBoxLayout* makeRadioRow(RadioButton* rb, const QString& label, QWidget* parent)
{
    auto* row = new QHBoxLayout();
    row->setSpacing(8);
    row->addWidget(rb);
    row->addWidget(new QLabel(label, parent));
    row->addStretch();
    return row;
}

// ── Unit Tests ───────────────────────────────────────────────────────────────

TEST_F(RadioButtonTest, DefaultProperties)
{
    RadioButton rb(window);
    EXPECT_EQ(rb.circleSize(), 20);
    EXPECT_EQ(rb.textGap(), 8);
    EXPECT_TRUE(rb.text().isEmpty());
    EXPECT_FALSE(rb.isChecked());
    EXPECT_TRUE(rb.isEnabled());
}

TEST_F(RadioButtonTest, DefaultPropertiesWithText)
{
    RadioButton rb("Option", window);
    EXPECT_EQ(rb.text(), "Option");
    EXPECT_EQ(rb.circleSize(), 20);
    EXPECT_EQ(rb.textGap(), 8);
    EXPECT_FALSE(rb.isChecked());
}

TEST_F(RadioButtonTest, EmptyText)
{
    RadioButton rb("", window);
    EXPECT_TRUE(rb.text().isEmpty());
    // The circle keeps space for its antialiased edge and keyboard cue.
    QSize hint = rb.sizeHint();
    EXPECT_EQ(hint.width(), 28);
}

TEST_F(RadioButtonTest, SetCircleSize)
{
    RadioButton rb(window);
    QSignalSpy spy(&rb, &RadioButton::circleSizeChanged);
    rb.setCircleSize(24);
    EXPECT_EQ(rb.circleSize(), 24);
    EXPECT_EQ(spy.count(), 1);

    // Same value → no signal
    rb.setCircleSize(24);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(RadioButtonTest, SetTextGap)
{
    RadioButton rb("Test", window);
    QSignalSpy spy(&rb, &RadioButton::textGapChanged);
    rb.setTextGap(12);
    EXPECT_EQ(rb.textGap(), 12);
    EXPECT_EQ(spy.count(), 1);

    // Same value → no signal
    rb.setTextGap(12);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(RadioButtonTest, SetTextFont)
{
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

TEST_F(RadioButtonTest, SizeHintWithText)
{
    RadioButton rb("Hello World", window);
    QSize hint = rb.sizeHint();
    // With text: width > circleSize, height >= circleSize
    EXPECT_GT(hint.width(), 20);
    EXPECT_GE(hint.height(), 20);
}

TEST_F(RadioButtonTest, SizeHintNoText)
{
    RadioButton rb(window);
    QSize hint = rb.sizeHint();
    EXPECT_EQ(hint, QSize(28, 28));
}

TEST_F(RadioButtonTest, Contract_LegacyFixedSizeKeepsTheWholeIndicatorAndText)
{
    const QColor background(17, 23, 31);
    for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        for (const QString text : {QString(), QStringLiteral("Option")}) {
            RadioButton compact(text), padded(text);
            for (auto* radio : {&compact, &padded}) {
                radio->setLayoutDirection(direction);
                radio->setChecked(true);
                radio->setCheckProgress(1.0);
            }
            const QFontMetrics metrics(compact.textFont());
            const QSize legacy(compact.circleSize()
                    + (text.isEmpty() ? 0 : compact.textGap() + metrics.horizontalAdvance(text)),
                qMax(compact.circleSize(), metrics.height()));
            compact.setFixedSize(legacy);
            padded.resize(padded.sizeHint());
            const int inset = padded.themeSpacing().gap.tight;
            for (const qreal scale : {1.0, 1.25, 1.5, 2.0}) {
                const QImage actual = testutils::renderControlSurface(compact, scale, background);
                const QImage reference = testutils::renderControlSurface(padded, scale, background);
                EXPECT_EQ(actual,
                    reference.copy(qRound(inset * scale), qRound(inset * scale), actual.width(), actual.height()));
            }
        }
    }
}

TEST_F(RadioButtonTest, CheckedState)
{
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

TEST_F(RadioButtonTest, DisabledState)
{
    RadioButton rb("Disabled", window);
    rb.setEnabled(false);
    EXPECT_FALSE(rb.isEnabled());
    rb.setChecked(true);
    EXPECT_TRUE(rb.isChecked());
}

TEST_F(RadioButtonTest, PaintDoesNotCrash)
{
    RadioButton rb("Paint test", window);
    window->show();
    rb.show();
    rb.repaint();

    rb.setChecked(true);
    rb.repaint();

    rb.setEnabled(false);
    rb.repaint();
}

TEST_F(RadioButtonTest, PaintEmptyTextDoesNotCrash)
{
    RadioButton rb(window);
    window->show();
    rb.show();
    rb.repaint();
    rb.setChecked(true);
    rb.repaint();
}

TEST_F(RadioButtonTest, ThemeUpdateDoesNotCrash)
{
    RadioButton rb("Theme", window);
    rb.onThemeUpdated();
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    rb.onThemeUpdated();
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
}

TEST_F(RadioButtonTest, Contract_LightAndDarkCheckedStatePaintsDistinctly)
{
    const FluentElement::Theme themes[]{FluentElement::Light, FluentElement::Dark};
    for (const auto theme : themes) {
        FluentElement::setTheme(theme);

        auto grabState = [](bool checked) {
            RadioButton radio(QStringLiteral("Option"));
            radio.setChecked(checked);
            radio.resize(radio.sizeHint().expandedTo(QSize(200, 32)));
            return radio.grab().toImage();
        };

        const QImage unchecked = grabState(false);
        const QImage checked = grabState(true);
        ASSERT_FALSE(unchecked.isNull()) << "theme=" << theme;
        ASSERT_EQ(checked.size(), unchecked.size()) << "theme=" << theme;
        EXPECT_NE(checked, unchecked) << "theme=" << theme;
    }

    ThemeRegistry::instance().resetToDefaults();
    FluentElement::setTheme(FluentElement::Light);
}

TEST_F(RadioButtonTest, Contract_UncheckedOutlineRemainsVisibleWithoutHover)
{
    for (const auto theme : {FluentElement::Light, FluentElement::Dark, FluentElement::HighContrast}) {
        SCOPED_TRACE(::testing::Message() << "theme=" << theme);
        RadioButton radio(QStringLiteral(""));
        radio.setProperty("fluentThemeOverride", static_cast<int>(theme));
        radio.resize(48, 32);
        const QRect indicator(radio.themeSpacing().gap.tight, (radio.height() - radio.circleSize()) / 2,
            radio.circleSize(), radio.circleSize());
        testutils::expectVisibleUncheckedOutline(radio, indicator, radio.themeColors().bgLayer,
            theme == FluentElement::HighContrast);
    }
}

TEST_F(RadioButtonTest, Contract_SelectedCircleDoesNotTouchWidgetClipAtFractionalScale)
{
    for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        for (const auto theme : {FluentElement::Light, FluentElement::Dark, FluentElement::HighContrast}) {
            RadioButton radio(QStringLiteral(""));
            radio.setLayoutDirection(direction);
            radio.setProperty("fluentThemeOverride", static_cast<int>(theme));
            radio.setChecked(true);
            const QColor background = radio.themeColors().bgLayer;
            QPalette palette = radio.palette();
            palette.setColor(radio.backgroundRole(), background);
            radio.setPalette(palette);
            radio.setAutoFillBackground(true);
            for (const int diameter : {14, 20, 28}) {
                radio.setCircleSize(diameter);
                radio.resize(radio.sizeHint());
                for (const qreal scale : {1.0, 1.25, 1.5, 2.0}) {
                    SCOPED_TRACE(::testing::Message()
                        << "direction=" << direction << " theme=" << theme << " diameter=" << diameter
                        << " scale=" << scale);
                    QImage image(QSize(qCeil(radio.width() * scale), qCeil(radio.height() * scale)),
                        QImage::Format_ARGB32_Premultiplied);
                    image.setDevicePixelRatio(scale);
                    image.fill(background);
                    radio.render(&image);
                    for (int x = 0; x < image.width(); ++x) {
                        EXPECT_EQ(image.pixelColor(x, 0), background);
                        EXPECT_EQ(image.pixelColor(x, image.height() - 1), background);
                    }
                    for (int y = 0; y < image.height(); ++y) {
                        EXPECT_EQ(image.pixelColor(0, y), background);
                        EXPECT_EQ(image.pixelColor(image.width() - 1, y), background);
                    }
                }
            }
        }
    }
}

TEST_F(RadioButtonTest, Contract_KeyboardCueDoesNotOverpaintSelection)
{
    for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        for (const auto theme : {FluentElement::Light, FluentElement::Dark, FluentElement::HighContrast}) {
            SCOPED_TRACE(::testing::Message() << "direction=" << direction << " theme=" << theme);
            QWidget host;
            auto* layout = new QVBoxLayout(&host);
            auto* before = new Button(QStringLiteral("Before"), &host);
            auto* radio = new RadioButton(QStringLiteral("Option"), &host);
            radio->setLayoutDirection(direction);
            radio->setProperty("fluentThemeOverride", static_cast<int>(theme));
            radio->setChecked(true);
            layout->addWidget(before);
            layout->addWidget(radio);
            layout->addWidget(new RadioButton(QStringLiteral("Other"), &host));
            host.show();
            QApplication::setActiveWindow(&host);
            before->setFocus();
            ASSERT_TRUE(QTest::qWaitFor([before] { return before->hasFocus(); }, 1000));
            radio->setAttribute(Qt::WA_UnderMouse, false);
            const QImage resting = radio->grab().toImage();
            QTest::keyClick(before, Qt::Key_Tab);
            ASSERT_TRUE(radio->hasFocus());
#ifdef Q_OS_MAC
            EXPECT_FALSE(radio->testAttribute(Qt::WA_MacShowFocusRect));
#endif
            const QImage focused = radio->grab().toImage();
            const QRect logical(radio->themeSpacing().gap.tight, (radio->height() - radio->circleSize()) / 2,
                radio->circleSize(), radio->circleSize());
            const QRect indicator = QStyle::visualRect(direction, radio->rect(), logical);
            const qreal scale = focused.devicePixelRatio();
            const QRect pixels(qRound(indicator.x() * scale), qRound(indicator.y() * scale),
                qRound(indicator.width() * scale), qRound(indicator.height() * scale));
            EXPECT_EQ(resting.copy(pixels), focused.copy(pixels));
            EXPECT_NE(resting, focused);
            QTest::mouseClick(radio, Qt::LeftButton, Qt::NoModifier, radio->rect().center());
            EXPECT_TRUE(radio->isChecked());
            ASSERT_TRUE(QTest::qWaitFor([radio] { return qFuzzyCompare(radio->checkProgress(), 1.0); }, 1000));
            const QImage mouse = radio->grab().toImage();
            EXPECT_EQ(resting, mouse);
        }
    }
}

TEST_F(RadioButtonTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
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

    QObject::connect(group1, qOverload<QAbstractButton*>(&QButtonGroup::buttonClicked),
        [outputLabel](QAbstractButton* btn) {
            outputLabel->setText(QString("Output: You selected %1").arg(btn->text()));
        });

    layout->addSpacing(10);

    // ── Section 2: Two groups controlling a colored box ──
    layout->addWidget(new QLabel("2. Two RadioButton groups with color selection:", window));

    auto makeColorGroup = [&](const QString& header, const QStringList& items) -> QButtonGroup* {
        layout->addWidget(new QLabel(header, window));
        auto* grp = new QButtonGroup(window);
        auto* row = new QHBoxLayout();
        for (int i = 0; i < items.size(); ++i) {
            auto* rb = new RadioButton(items[i], window);
            grp->addButton(rb, i);
            row->addWidget(rb);
            row->addSpacing(12);
            if (i == 0)
                rb->setChecked(true);
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
            case 0:
                return "green";
            case 1:
                return "yellow";
            default:
                return "white";
            }
        };
        colorBox->setStyleSheet(QString("background-color: %1; border: 4px solid %2;")
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
        auto* rb = new RadioButton(window); // no text
        emptyGroup->addButton(rb, i);
        emptyRow->addWidget(rb);
        emptyRow->addSpacing(8);
        if (i == 0)
            rb->setChecked(true);
    }
    emptyRow->addStretch();
    layout->addLayout(emptyRow);

    layout->addStretch();

    // ── Theme Switcher ──
    auto* themeBtn = new Button("Switch Theme", window);
    themeBtn->setFixedSize(120, 32);
    layout->addWidget(themeBtn, 0, Qt::AlignRight);
    QObject::connect(themeBtn, &Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                ? fluent::FluentElement::Dark
                : fluent::FluentElement::Light);
    });

    window->show();
    qApp->exec();
}
