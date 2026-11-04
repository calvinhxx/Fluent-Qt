#include "components/basicinput/Slider.h"
#include "components/basicinput/Button.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "design/Spacing.h"
#include "ControlSurfaceTestUtils.h"
#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <gtest/gtest.h>
#include <limits>

using namespace fluent::basicinput;
using namespace fluent;

TEST(SliderContractTest, Contract_FocusAndResizedHandlesStayInsideTheSurface)
{
    const QColor background(17, 23, 31);
    for (const auto orientation : {Qt::Horizontal, Qt::Vertical}) {
        for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            QWidget host;
            QVBoxLayout layout(&host);
            Button before(QStringLiteral("Before"));
            Slider slider(orientation);
            slider.setLayoutDirection(direction);
            slider.setRange(0, 100);
            layout.addWidget(&before);
            layout.addWidget(&slider);
            host.show();
            QApplication::setActiveWindow(&host);
            for (const int size : {20, 36}) {
                slider.setHandleSize(size);
                QApplication::processEvents();
                const QSize minimum = slider.minimumSizeHint();
                EXPECT_GE(orientation == Qt::Horizontal ? minimum.height() : minimum.width(), size + 4);
                for (const int value : {0, 100}) {
                    slider.setValue(value);
                    for (const auto theme : {FluentElement::Light, FluentElement::Dark, FluentElement::HighContrast}) {
                        slider.setProperty("fluentThemeOverride", static_cast<int>(theme));
                        for (const qreal scale : {1.0, 1.25, 1.5, 2.0}) {
                            SCOPED_TRACE(::testing::Message()
                                << "orientation=" << orientation << " direction=" << direction << "size=" << size
                                << " value=" << value << " theme=" << theme << " scale=" << scale);
                            before.setFocus();
                            ASSERT_TRUE(before.hasFocus());
                            const QImage resting = testutils::renderControlSurface(slider, scale, background);
                            slider.setFocus(Qt::TabFocusReason);
                            ASSERT_TRUE(slider.hasFocus());
                            const QImage focused = testutils::renderControlSurface(slider, scale, background);
                            EXPECT_NE(resting, focused);
                            testutils::expectClearSurfacePerimeter(focused, background);
                            EXPECT_EQ(slider.value(), value);
                            before.setFocus();
                            slider.setFocus(Qt::MouseFocusReason);
                            EXPECT_EQ(resting, testutils::renderControlSurface(slider, scale, background));
                        }
                    }
                }
            }
            slider.setValue(50);
            QTest::keyClick(&slider, Qt::Key_Up);
            EXPECT_EQ(slider.value(), 51);
#ifdef Q_OS_MAC
            EXPECT_FALSE(slider.testAttribute(Qt::WA_MacShowFocusRect));
#endif
        }
    }
}

TEST(SliderContractTest, Contract_MinimumSizeRetainsDragTravel)
{
    for (const auto orientation : {Qt::Horizontal, Qt::Vertical}) {
        for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            for (const int handle : {20, 36}) {
                Slider slider(orientation);
                slider.setLayoutDirection(direction);
                slider.setHandleSize(handle);
                slider.setRange(0, 100);
                slider.resize(slider.minimumSizeHint());
                slider.show();
                const bool horizontal = orientation == Qt::Horizontal;
                const int length = horizontal ? slider.width() : slider.height();
                const auto point = [&](int position) {
                    return horizontal ? QPoint(position, slider.height() / 2) : QPoint(slider.width() / 2, position);
                };
                QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, point(0));
                const int start = slider.value();
                // Send the drag directly: offscreen cursor relocation can
                // coalesce moves when repeated cases reuse the same position.
                QMouseEvent move(QEvent::MouseMove, QPointF(point(length - 1)), Qt::NoButton, Qt::LeftButton,
                    Qt::NoModifier);
                QApplication::sendEvent(&slider, &move);
                QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, point(length - 1));
                EXPECT_NE(slider.value(), start);
                EXPECT_EQ(qMin(start, slider.value()), slider.minimum());
                EXPECT_EQ(qMax(start, slider.value()), slider.maximum());
            }
        }
    }
}

TEST(SliderContractTest, Contract_DefaultMetricsAndQSliderSemantics)
{
    Slider slider(Qt::Horizontal);
    EXPECT_EQ(slider.handleSize(), 20);
    EXPECT_EQ(slider.trackHeight(), Spacing::XSmall);
    EXPECT_DOUBLE_EQ(slider.hoverRatio(), 0.0);
    EXPECT_DOUBLE_EQ(slider.pressRatio(), 0.0);

    slider.setRange(10, 90);
    slider.setValue(55);
    slider.setHandleSize(24);
    slider.setTrackHeight(6);
    slider.setHoverRatio(0.5);
    slider.setPressRatio(0.75);

    EXPECT_EQ(slider.minimum(), 10);
    EXPECT_EQ(slider.maximum(), 90);
    EXPECT_EQ(slider.value(), 55);
    EXPECT_EQ(slider.handleSize(), 24);
    EXPECT_EQ(slider.trackHeight(), 6);
    EXPECT_DOUBLE_EQ(slider.hoverRatio(), 0.5);
    EXPECT_DOUBLE_EQ(slider.pressRatio(), 0.75);
}

TEST(SliderContractTest, Contract_PointerInteractionPreservesInheritedSignalsExactlyOnce)
{
    Slider slider(Qt::Horizontal);
    slider.setRange(0, 100);
    slider.resize(240, 40);

    QSignalSpy pressedSpy(&slider, &QAbstractSlider::sliderPressed);
    QSignalSpy releasedSpy(&slider, &QAbstractSlider::sliderReleased);

    const QPoint center = slider.rect().center();
    QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, center);
    EXPECT_EQ(pressedSpy.count(), 1);
    EXPECT_EQ(releasedSpy.count(), 0);

    QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, center);
    EXPECT_EQ(pressedSpy.count(), 1);
    EXPECT_EQ(releasedSpy.count(), 1);
}

TEST(SliderContractTest, Contract_PointerAndFilledTrackRespectQtDirectionSemantics)
{
    struct DirectionCase {
        Qt::Orientation orientation;
        Qt::LayoutDirection direction;
        bool inverted;
        bool minimumAtStart;
    };
    const DirectionCase cases[] = {
        {Qt::Horizontal, Qt::LeftToRight, false, true},
        {Qt::Horizontal, Qt::LeftToRight, true, false},
        {Qt::Horizontal, Qt::RightToLeft, false, false},
        {Qt::Horizontal, Qt::RightToLeft, true, true},
        {Qt::Vertical, Qt::LeftToRight, false, false},
        {Qt::Vertical, Qt::LeftToRight, true, true},
        {Qt::Vertical, Qt::RightToLeft, false, false},
        {Qt::Vertical, Qt::RightToLeft, true, true},
    };
    for (const auto& item : cases) {
        SCOPED_TRACE(::testing::Message()
            << "orientation=" << item.orientation << ", direction=" << item.direction
            << ", inverted=" << item.inverted);
        Slider slider(item.orientation);
        QSlider reference(item.orientation);
        for (QSlider* target : {static_cast<QSlider*>(&slider), &reference}) {
            target->setRange(-50, 150);
            target->setLayoutDirection(item.direction);
            target->setInvertedAppearance(item.inverted);
        }
        const bool horizontal = item.orientation == Qt::Horizontal;
        slider.resize(horizontal ? QSize(240, 40) : QSize(40, 240));
        const auto point = [horizontal](int position) {
            return horizontal ? QPoint(position, 20) : QPoint(20, position);
        };

        QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, point(0));
        EXPECT_EQ(slider.value(), item.minimumAtStart ? -50 : 150);
        QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, point(239));
        EXPECT_EQ(slider.value(), item.minimumAtStart ? 150 : -50);
        QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, point(120));
        ASSERT_EQ(slider.value(), 50);

        slider.setHoverRatio(0.0);
        slider.setPressRatio(0.0);
        const QImage image = slider.grab().toImage();
        const auto sample = [&](int position) {
            const QPoint pixel = point(position) * image.devicePixelRatio();
            return image.pixelColor(pixel);
        };
        EXPECT_EQ(sample(item.minimumAtStart ? 60 : 180), slider.themeColors().accentDefault);
        EXPECT_NE(sample(item.minimumAtStart ? 180 : 60), slider.themeColors().accentDefault);

        reference.setValue(50);
        const Qt::Key key = horizontal ? Qt::Key_Right : Qt::Key_Up;
        QTest::keyClick(&slider, key);
        QTest::keyClick(&reference, key);
        EXPECT_EQ(slider.value(), reference.value());
    }
}

TEST(SliderContractTest, Contract_FullIntegerRangePaintAndPointerMappingRemainStable)
{
    const int low = std::numeric_limits<int>::min();
    const int high = std::numeric_limits<int>::max();
    for (const int length : {240, 4200}) {
        SCOPED_TRACE(length);
        Slider slider(Qt::Horizontal);
        slider.setRange(low, high);
        slider.resize(length, 40);
        Slider normalized(Qt::Horizontal);
        normalized.setRange(0, 100);
        normalized.resize(length, 40);

        const int values[] = {low, 0, high};
        const int percentages[] = {0, 50, 100};
        for (int index = 0; index < 3; ++index) {
            slider.setValue(values[index]);
            normalized.setValue(percentages[index]);
            EXPECT_EQ(slider.grab().toImage(), normalized.grab().toImage());
        }

        QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(0, 20));
        EXPECT_EQ(slider.value(), low);
        QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(length / 2, 20));
        EXPECT_GE(slider.value(), -1);
        EXPECT_LE(slider.value(), 1);
        QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(length - 1, 20));
        EXPECT_EQ(slider.value(), high);
    }
}

TEST(SliderContractTest, Contract_AutomaticTicksHandleZeroStepsAndFullIntegerRanges)
{
    Slider slider(Qt::Horizontal);
    slider.resize(240, 40);
    slider.setRange(0, 100);
    slider.setValue(50);
    slider.setHoverRatio(1.0);
    slider.setTickPosition(QSlider::TicksBelow);
    slider.setTickInterval(10);
    const QImage pageStepTicks = slider.grab().toImage();
    slider.setTickInterval(0);
    EXPECT_EQ(slider.grab().toImage(), pageStepTicks);

    slider.setRange(0, 2);
    slider.setSingleStep(0);
    slider.setPageStep(0);
    const QImage zeroStepTicks = slider.grab().toImage();
    slider.setTickPosition(QSlider::NoTicks);
    EXPECT_NE(slider.grab().toImage(), zeroStepTicks);

    slider.setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    slider.setValue(0);
    const QImage noTicks = slider.grab().toImage();
    slider.setTickPosition(QSlider::TicksBelow);
    slider.setTickInterval(std::numeric_limits<int>::max());
    EXPECT_NE(slider.grab().toImage(), noTicks);
    slider.setTickInterval(1);
    EXPECT_EQ(slider.grab().toImage(), noTicks);
}

TEST(SliderContractTest, Contract_LightAndDarkRangeExtremesPaintDistinctly)
{
    const FluentElement::Theme themes[]{FluentElement::Light, FluentElement::Dark};
    for (const auto theme : themes) {
        FluentElement::setTheme(theme);
        Slider slider(Qt::Horizontal);
        slider.setRange(0, 100);
        slider.resize(240, 40);

        slider.setValue(0);
        const QImage minimum = slider.grab().toImage();
        slider.setValue(100);
        const QImage maximum = slider.grab().toImage();

        ASSERT_FALSE(minimum.isNull()) << "theme=" << theme;
        ASSERT_EQ(maximum.size(), minimum.size()) << "theme=" << theme;
        EXPECT_NE(maximum, minimum) << "theme=" << theme;
    }

    ThemeRegistry::instance().resetToDefaults();
    FluentElement::setTheme(FluentElement::Light);
}

TEST(SliderContractTest, Contract_UnfilledTrackUsesStrongControlFill)
{
    // The shared slider painter draws the unfilled track with the strong control
    // fill so the value stays readable on light surfaces.
    // zh_CN: 共享滑块绘制器用强控件填充绘制未填充轨道，使数值在浅色表面上依然清晰。
    const QColor background(250, 250, 250);
    for (const auto theme : {FluentElement::Light, FluentElement::Dark, FluentElement::HighContrast}) {
        SCOPED_TRACE(::testing::Message() << "theme=" << theme);
        Slider slider(Qt::Horizontal);
        slider.setProperty("fluentThemeOverride", static_cast<int>(theme));
        slider.setRange(0, 100);
        slider.setValue(20);
        slider.resize(240, 40);
        const QImage image = testutils::renderControlSurface(slider, 1.0, background);
        const QColor track = slider.themeColors().strokeStrong;
        const qreal alpha = track.alphaF();
        const QColor expected = QColor::fromRgbF(track.redF() * alpha + background.redF() * (1.0 - alpha),
            track.greenF() * alpha + background.greenF() * (1.0 - alpha),
            track.blueF() * alpha + background.blueF() * (1.0 - alpha));
        const QColor actual = image.pixelColor(210, 20);
        EXPECT_LE(qAbs(actual.red() - expected.red()), 3);
        EXPECT_LE(qAbs(actual.green() - expected.green()), 3);
        EXPECT_LE(qAbs(actual.blue() - expected.blue()), 3);
        EXPECT_EQ(image.pixelColor(30, 20), slider.themeColors().accentDefault);
    }
}
