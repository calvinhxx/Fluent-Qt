#include "components/basicinput/Button.h"
#include "components/basicinput/RadioButton.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "SelectionIndicatorTestUtils.h"
#include "ControlSurfaceTestUtils.h"
#include <QApplication>
#include <QButtonGroup>
#include <QImage>
#include <QSignalSpy>
#include <QStyle>
#include <QTest>
#include <QtMath>
#include <QVBoxLayout>
#include <gtest/gtest.h>

using namespace fluent;
using namespace fluent::basicinput;

TEST(RadioButtonTest, DefaultProperties)
{
    RadioButton rb(nullptr);
    EXPECT_EQ(rb.circleSize(), 20);
    EXPECT_EQ(rb.textGap(), 8);
    EXPECT_TRUE(rb.text().isEmpty());
    EXPECT_FALSE(rb.isChecked());
    EXPECT_TRUE(rb.isEnabled());
}

TEST(RadioButtonTest, DefaultPropertiesWithText)
{
    RadioButton rb("Option");
    EXPECT_EQ(rb.text(), "Option");
    EXPECT_EQ(rb.circleSize(), 20);
    EXPECT_EQ(rb.textGap(), 8);
    EXPECT_FALSE(rb.isChecked());
}

TEST(RadioButtonTest, EmptyText)
{
    RadioButton rb("");
    EXPECT_TRUE(rb.text().isEmpty());
    // The circle keeps space for its antialiased edge and keyboard cue.
    QSize hint = rb.sizeHint();
    EXPECT_EQ(hint.width(), 28);
}

TEST(RadioButtonTest, SetCircleSize)
{
    RadioButton rb(nullptr);
    QSignalSpy spy(&rb, &RadioButton::circleSizeChanged);
    rb.setCircleSize(24);
    EXPECT_EQ(rb.circleSize(), 24);
    EXPECT_EQ(spy.count(), 1);

    // Same value → no signal
    rb.setCircleSize(24);
    EXPECT_EQ(spy.count(), 1);
}

TEST(RadioButtonTest, SetTextGap)
{
    RadioButton rb("Test");
    QSignalSpy spy(&rb, &RadioButton::textGapChanged);
    rb.setTextGap(12);
    EXPECT_EQ(rb.textGap(), 12);
    EXPECT_EQ(spy.count(), 1);

    // Same value → no signal
    rb.setTextGap(12);
    EXPECT_EQ(spy.count(), 1);
}

TEST(RadioButtonTest, SetTextFont)
{
    RadioButton rb("Test");
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

TEST(RadioButtonTest, SizeHintWithText)
{
    RadioButton rb("Hello World");
    QSize hint = rb.sizeHint();
    // With text: width > circleSize, height >= circleSize
    EXPECT_GT(hint.width(), 20);
    EXPECT_GE(hint.height(), 20);
}

TEST(RadioButtonTest, SizeHintNoText)
{
    RadioButton rb(nullptr);
    QSize hint = rb.sizeHint();
    EXPECT_EQ(hint, QSize(28, 28));
}

TEST(RadioButtonTest, Contract_LegacyFixedSizeKeepsTheWholeIndicatorAndText)
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

TEST(RadioButtonTest, CheckedState)
{
    QWidget host;
    RadioButton rb1("A", &host);
    RadioButton rb2("B", &host);
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

TEST(RadioButtonTest, DisabledState)
{
    RadioButton rb("Disabled");
    rb.setEnabled(false);
    EXPECT_FALSE(rb.isEnabled());
    rb.setChecked(true);
    EXPECT_TRUE(rb.isChecked());
}

TEST(RadioButtonTest, PaintDoesNotCrash)
{
    QWidget host;
    RadioButton rb("Paint test", &host);
    host.show();
    rb.show();
    rb.repaint();

    rb.setChecked(true);
    rb.repaint();

    rb.setEnabled(false);
    rb.repaint();
}

TEST(RadioButtonTest, PaintEmptyTextDoesNotCrash)
{
    QWidget host;
    RadioButton rb(&host);
    host.show();
    rb.show();
    rb.repaint();
    rb.setChecked(true);
    rb.repaint();
}

TEST(RadioButtonTest, ThemeUpdateDoesNotCrash)
{
    RadioButton rb("Theme");
    rb.onThemeUpdated();
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    rb.onThemeUpdated();
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
}

TEST(RadioButtonTest, Contract_LightAndDarkCheckedStatePaintsDistinctly)
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

TEST(RadioButtonTest, Contract_UncheckedOutlineRemainsVisibleWithoutHover)
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

TEST(RadioButtonTest, Contract_SelectedCircleDoesNotTouchWidgetClipAtFractionalScale)
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

TEST(RadioButtonTest, Contract_KeyboardCueDoesNotOverpaintSelection)
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
