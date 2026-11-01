#include <QAccessible>
#include <QApplication>
#include <QHBoxLayout>
#include <QSignalSpy>
#include <QTest>
#include <QVector>
#include <gtest/gtest.h>

#include <limits>
#include <type_traits>

#include "ControlSurfaceTestUtils.h"
#include "QtTestEnvironment.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/RangeSlider.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

using fluent::FluentElement;
using fluent::basicinput::Button;
using fluent::basicinput::RangeSlider;
using Thumb = fluent::basicinput::RangeSlider::Thumb;

namespace {

void pressMoveRelease(RangeSlider& slider, const QPoint& from, const QPoint& to)
{
    QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, from);
    // Send the drag directly: offscreen cursor relocation can coalesce moves.
    // zh_CN: 直接发送拖动事件，避免离屏光标重定位合并移动事件。
    QMouseEvent move(QEvent::MouseMove, QPointF(to), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&slider, &move);
    QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, to);
}

void sendMove(RangeSlider& slider, const QPoint& to)
{
    QMouseEvent move(QEvent::MouseMove, QPointF(to), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&slider, &move);
}

QColor composite(const QColor& background, const QColor& overlay)
{
    const qreal alpha = overlay.alphaF();
    return QColor::fromRgbF(overlay.redF() * alpha + background.redF() * (1.0 - alpha),
        overlay.greenF() * alpha + background.greenF() * (1.0 - alpha),
        overlay.blueF() * alpha + background.blueF() * (1.0 - alpha));
}

bool nearColor(const QColor& actual, const QColor& expected, int tolerance = 3)
{
    return qAbs(actual.red() - expected.red()) <= tolerance && qAbs(actual.green() - expected.green()) <= tolerance
        && qAbs(actual.blue() - expected.blue()) <= tolerance;
}

} // namespace

TEST(RangeSliderContractTest, Contract_DefaultsKeepQSliderSemantics)
{
    static_assert(std::is_base_of<QWidget, RangeSlider>::value, "RangeSlider must remain a QWidget");
    static_assert(std::is_base_of<fluent::FluentElement, RangeSlider>::value, "RangeSlider must expose themes");
    static_assert(std::is_base_of<fluent::QMLPlus, RangeSlider>::value, "RangeSlider must expose QMLPlus");

    RangeSlider slider;
    EXPECT_EQ(slider.orientation(), Qt::Horizontal);
    EXPECT_EQ(slider.minimum(), 0);
    EXPECT_EQ(slider.maximum(), 99);
    EXPECT_EQ(slider.lowerValue(), 0);
    EXPECT_EQ(slider.upperValue(), 99);
    EXPECT_EQ(slider.minimumRange(), 0);
    EXPECT_EQ(slider.singleStep(), 1);
    EXPECT_EQ(slider.pageStep(), 10);
    EXPECT_EQ(slider.tickPosition(), QSlider::NoTicks);
    EXPECT_EQ(slider.tickInterval(), 0);
    EXPECT_TRUE(slider.hasTracking());
    EXPECT_EQ(slider.activeThumb(), Thumb::Lower);
    EXPECT_FALSE(slider.isSliderDown());
    EXPECT_EQ(slider.focusPolicy(), Qt::StrongFocus);
    EXPECT_EQ(slider.sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    EXPECT_EQ(slider.sizePolicy().verticalPolicy(), QSizePolicy::Fixed);
    EXPECT_EQ(slider.sizeHint(), QSize(160, 24));

    QSignalSpy orientationSpy(&slider, &RangeSlider::orientationChanged);
    slider.setOrientation(Qt::Vertical);
    EXPECT_EQ(orientationSpy.count(), 1);
    EXPECT_EQ(slider.sizeHint(), QSize(24, 160));
    EXPECT_EQ(slider.sizePolicy().verticalPolicy(), QSizePolicy::Expanding);
    EXPECT_EQ(slider.minimumSizeHint().height(), 2 * 12 + 2 * 20);

    RangeSlider vertical(Qt::Vertical);
    EXPECT_EQ(vertical.orientation(), Qt::Vertical);
    EXPECT_EQ(vertical.sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
}

TEST(RangeSliderContractTest, Contract_ValuesStayOrderedClampedAndApart)
{
    RangeSlider slider;
    slider.setRange(0, 100);

    slider.setValues(80, 20);
    EXPECT_EQ(slider.lowerValue(), 20);
    EXPECT_EQ(slider.upperValue(), 80);

    slider.setLowerValue(95);
    EXPECT_EQ(slider.lowerValue(), 80) << "the lower value never passes the upper value";
    slider.setValues(20, 80);
    slider.setUpperValue(5);
    EXPECT_EQ(slider.upperValue(), 20) << "the upper value never passes the lower value";

    slider.setValues(-10, 500);
    EXPECT_EQ(slider.lowerValue(), 0);
    EXPECT_EQ(slider.upperValue(), 100);

    slider.setValues(40, 50);
    slider.setMinimumRange(30);
    EXPECT_EQ(slider.lowerValue(), 40);
    EXPECT_EQ(slider.upperValue(), 70) << "a wider minimum range moves the upper value first";

    slider.setValues(90, 95);
    EXPECT_EQ(slider.lowerValue(), 70);
    EXPECT_EQ(slider.upperValue(), 100) << "at the maximum both values shift down together";

    slider.setLowerValue(90);
    EXPECT_EQ(slider.lowerValue(), 70);
    slider.setUpperValue(0);
    EXPECT_EQ(slider.upperValue(), 100);

    slider.setRange(0, 10);
    EXPECT_EQ(slider.lowerValue(), 0) << "a minimum range wider than the span is capped by it";
    EXPECT_EQ(slider.upperValue(), 10);

    slider.setMinimumRange(-5);
    EXPECT_EQ(slider.minimumRange(), 0);

    slider.setRange(50, 10);
    EXPECT_EQ(slider.minimum(), 50);
    EXPECT_EQ(slider.maximum(), 50);
    EXPECT_EQ(slider.lowerValue(), 50);
    EXPECT_EQ(slider.upperValue(), 50);

    slider.setMaximum(40);
    EXPECT_EQ(slider.minimum(), 40);
    EXPECT_EQ(slider.maximum(), 40);
    slider.setMinimum(60);
    EXPECT_EQ(slider.minimum(), 60);
    EXPECT_EQ(slider.maximum(), 60);

    slider.setSingleStep(-3);
    EXPECT_EQ(slider.singleStep(), 3);
    slider.setPageStep(-20);
    EXPECT_EQ(slider.pageStep(), 20);
    slider.setTickInterval(-1);
    EXPECT_EQ(slider.tickInterval(), 0);
}

TEST(RangeSliderContractTest, Contract_SettersAreNoOpsAndSignalsReadConsistentPairs)
{
    RangeSlider slider;
    slider.setRange(0, 100);
    slider.setValues(0, 100);

    QSignalSpy lowerSpy(&slider, &RangeSlider::lowerValueChanged);
    QSignalSpy upperSpy(&slider, &RangeSlider::upperValueChanged);
    QSignalSpy valuesSpy(&slider, &RangeSlider::valuesChanged);
    int inconsistentReads = 0;
    const auto checkPair = [&] {
        if (slider.lowerValue() > slider.upperValue())
            ++inconsistentReads;
    };
    QObject::connect(&slider, &RangeSlider::lowerValueChanged, &slider, checkPair);
    QObject::connect(&slider, &RangeSlider::upperValueChanged, &slider, checkPair);

    slider.setValues(60, 70);
    EXPECT_EQ(lowerSpy.count(), 1);
    EXPECT_EQ(upperSpy.count(), 1);
    ASSERT_EQ(valuesSpy.count(), 1);
    EXPECT_EQ(valuesSpy.at(0).at(0).toInt(), 60);
    EXPECT_EQ(valuesSpy.at(0).at(1).toInt(), 70);
    EXPECT_EQ(inconsistentReads, 0);

    slider.setValues(60, 70);
    slider.setLowerValue(60);
    slider.setUpperValue(70);
    EXPECT_EQ(valuesSpy.count(), 1);

    slider.setUpperValue(90);
    EXPECT_EQ(lowerSpy.count(), 1);
    EXPECT_EQ(upperSpy.count(), 2);
    EXPECT_EQ(valuesSpy.count(), 2);

    QSignalSpy rangeSpy(&slider, &RangeSlider::rangeChanged);
    QStringList order;
    QObject::connect(&slider, &RangeSlider::rangeChanged, &slider, [&] { order << QStringLiteral("range"); });
    QObject::connect(&slider, &RangeSlider::valuesChanged, &slider, [&] { order << QStringLiteral("values"); });
    slider.setRange(0, 50);
    EXPECT_EQ(rangeSpy.count(), 1);
    EXPECT_EQ(order, (QStringList{QStringLiteral("range"), QStringLiteral("values")}));
    EXPECT_EQ(slider.lowerValue(), 50);
    EXPECT_EQ(slider.upperValue(), 50);
    slider.setRange(0, 50);
    EXPECT_EQ(rangeSpy.count(), 1);

    QSignalSpy minimumRangeSpy(&slider, &RangeSlider::minimumRangeChanged);
    QSignalSpy singleSpy(&slider, &RangeSlider::singleStepChanged);
    QSignalSpy pageSpy(&slider, &RangeSlider::pageStepChanged);
    QSignalSpy tickPositionSpy(&slider, &RangeSlider::tickPositionChanged);
    QSignalSpy tickIntervalSpy(&slider, &RangeSlider::tickIntervalChanged);
    QSignalSpy trackingSpy(&slider, &RangeSlider::trackingChanged);
    QSignalSpy activeSpy(&slider, &RangeSlider::activeThumbChanged);
    for (int repeat = 0; repeat < 2; ++repeat) {
        slider.setMinimumRange(5);
        slider.setSingleStep(2);
        slider.setPageStep(15);
        slider.setTickPosition(QSlider::TicksBelow);
        slider.setTickInterval(5);
        slider.setTracking(false);
        slider.setActiveThumb(Thumb::Upper);
        slider.setOrientation(Qt::Horizontal);
    }
    EXPECT_EQ(minimumRangeSpy.count(), 1);
    EXPECT_EQ(singleSpy.count(), 1);
    EXPECT_EQ(pageSpy.count(), 1);
    EXPECT_EQ(tickPositionSpy.count(), 1);
    EXPECT_EQ(tickIntervalSpy.count(), 1);
    EXPECT_EQ(trackingSpy.count(), 1);
    EXPECT_EQ(activeSpy.count(), 1);
}

TEST(RangeSliderContractTest, Contract_KeyboardMovesActiveThumbAndTabVisitsBothThumbs)
{
    QWidget host;
    QHBoxLayout layout(&host);
    Button before(QStringLiteral("Before"));
    RangeSlider slider;
    Button after(QStringLiteral("After"));
    layout.addWidget(&before);
    layout.addWidget(&slider);
    layout.addWidget(&after);
    slider.setRange(0, 100);
    slider.setValues(20, 80);
    slider.setMinimumRange(10);
    ASSERT_TRUE(tests::support::showAndFocusWidget(&before));

    QTest::keyClick(&before, Qt::Key_Tab);
    ASSERT_TRUE(slider.hasFocus());
    EXPECT_EQ(slider.activeThumb(), Thumb::Lower);

    QTest::keyClick(&slider, Qt::Key_Right);
    EXPECT_EQ(slider.lowerValue(), 21);
    QTest::keyClick(&slider, Qt::Key_PageUp);
    EXPECT_EQ(slider.lowerValue(), 31);
    QTest::keyClick(&slider, Qt::Key_End);
    EXPECT_EQ(slider.lowerValue(), 70) << "End stops the lower thumb a minimum range below the upper one";
    QTest::keyClick(&slider, Qt::Key_Home);
    EXPECT_EQ(slider.lowerValue(), 0);
    EXPECT_EQ(slider.upperValue(), 80);

    QTest::keyClick(&slider, Qt::Key_Tab);
    ASSERT_TRUE(slider.hasFocus()) << "Tab moves to the upper thumb before leaving the control";
    EXPECT_EQ(slider.activeThumb(), Thumb::Upper);
    QTest::keyClick(&slider, Qt::Key_Down);
    EXPECT_EQ(slider.upperValue(), 79);
    QTest::keyClick(&slider, Qt::Key_Home);
    EXPECT_EQ(slider.upperValue(), 10);
    QTest::keyClick(&slider, Qt::Key_End);
    EXPECT_EQ(slider.upperValue(), 100);

    QTest::keyClick(&slider, Qt::Key_Tab);
    EXPECT_TRUE(after.hasFocus());
    QTest::keyClick(&after, Qt::Key_Backtab);
    ASSERT_TRUE(slider.hasFocus());
    EXPECT_EQ(slider.activeThumb(), Thumb::Upper);
    QTest::keyClick(&slider, Qt::Key_Backtab);
    ASSERT_TRUE(slider.hasFocus());
    EXPECT_EQ(slider.activeThumb(), Thumb::Lower);
    QTest::keyClick(&slider, Qt::Key_Backtab);
    EXPECT_TRUE(before.hasFocus());

    slider.setFocus(Qt::OtherFocusReason);
    slider.setLayoutDirection(Qt::RightToLeft);
    slider.setActiveThumb(Thumb::Lower);
    QTest::keyClick(&slider, Qt::Key_Left);
    EXPECT_EQ(slider.lowerValue(), 1) << "Left increases in right-to-left layouts, as in QSlider";
}

TEST(RangeSliderContractTest, Contract_PointerPicksNearestThumbAndSignalsExactlyOnce)
{
    RangeSlider slider;
    slider.setRange(0, 100);
    slider.setValues(20, 80);
    slider.resize(240, 40);
    slider.show();

    QSignalSpy pressedSpy(&slider, &RangeSlider::sliderPressed);
    QSignalSpy releasedSpy(&slider, &RangeSlider::sliderReleased);
    QSignalSpy movedSpy(&slider, &RangeSlider::sliderMoved);

    const auto point = [&](int value) {
        // Track pixels 12..228 map 0..100. zh_CN: 轨道像素 12..228 对应 0..100。
        return QPoint(12 + (216 * value + 50) / 100, 20);
    };

    QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, point(5));
    EXPECT_EQ(pressedSpy.count(), 1);
    EXPECT_TRUE(slider.isSliderDown());
    EXPECT_EQ(slider.activeThumb(), Thumb::Lower);
    EXPECT_EQ(slider.lowerValue(), 5);
    QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, point(5));
    EXPECT_EQ(releasedSpy.count(), 1);
    EXPECT_FALSE(slider.isSliderDown());

    QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, point(60));
    EXPECT_EQ(slider.activeThumb(), Thumb::Upper);
    EXPECT_EQ(slider.upperValue(), 60);
    EXPECT_EQ(slider.lowerValue(), 5);

    // Grabbing a thumb off-center keeps the offset instead of jumping.
    // zh_CN: 偏离中心抓住手柄时保持偏移，不会跳动。
    slider.setValues(20, 80);
    const QPoint grab = slider.thumbCenter(Thumb::Lower) + QPoint(4, 0);
    QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, grab);
    EXPECT_EQ(slider.lowerValue(), 20);
    EXPECT_EQ(slider.activeThumb(), Thumb::Lower);
    movedSpy.clear();
    sendMove(slider, grab + QPoint(point(30).x() - point(20).x(), 0));
    EXPECT_EQ(slider.lowerValue(), 30);
    EXPECT_EQ(movedSpy.count(), 1);
    sendMove(slider, slider.thumbCenter(Thumb::Upper) + QPoint(40, 0));
    EXPECT_EQ(slider.lowerValue(), 80) << "dragging past the upper thumb stops at it";
    EXPECT_EQ(slider.upperValue(), 80);
    QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, point(90));
    EXPECT_EQ(pressedSpy.count(), 3);
    EXPECT_EQ(releasedSpy.count(), 3);

    QTest::mouseClick(&slider, Qt::RightButton, Qt::NoModifier, point(50));
    EXPECT_EQ(pressedSpy.count(), 3);
}

TEST(RangeSliderContractTest, Contract_CoincidentThumbsFollowTheDragDirection)
{
    for (const int direction : {1, -1}) {
        SCOPED_TRACE(::testing::Message() << "direction=" << direction);
        RangeSlider slider;
        slider.setRange(0, 100);
        slider.setValues(50, 50);
        slider.resize(240, 40);
        slider.show();

        const QPoint center = slider.thumbCenter(Thumb::Lower);
        QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, center);
        EXPECT_EQ(slider.lowerValue(), 50);
        EXPECT_EQ(slider.upperValue(), 50);
        sendMove(slider, center + QPoint(direction * 43, 0));
        QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, center + QPoint(direction * 43, 0));
        if (direction > 0) {
            EXPECT_EQ(slider.activeThumb(), Thumb::Upper);
            EXPECT_EQ(slider.lowerValue(), 50);
            EXPECT_EQ(slider.upperValue(), 70);
        } else {
            EXPECT_EQ(slider.activeThumb(), Thumb::Lower);
            EXPECT_EQ(slider.lowerValue(), 30);
            EXPECT_EQ(slider.upperValue(), 50);
        }
    }
}

TEST(RangeSliderContractTest, Contract_WithoutTrackingValuesCommitBeforeRelease)
{
    RangeSlider slider;
    slider.setRange(0, 100);
    slider.setValues(20, 80);
    slider.setTracking(false);
    slider.resize(240, 40);
    slider.show();

    QSignalSpy valuesSpy(&slider, &RangeSlider::valuesChanged);
    QSignalSpy movedSpy(&slider, &RangeSlider::sliderMoved);
    int upperAtRelease = -1;
    QObject::connect(&slider, &RangeSlider::sliderReleased, &slider, [&] { upperAtRelease = slider.upperValue(); });

    const QPoint start = slider.thumbCenter(Thumb::Upper);
    const QPoint end = start - QPoint(43, 0);
    QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, start);
    sendMove(slider, end);
    EXPECT_EQ(valuesSpy.count(), 0);
    EXPECT_GE(movedSpy.count(), 1);
    EXPECT_EQ(slider.upperValue(), 80);
    EXPECT_EQ(slider.upperPosition(), 60);

    QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, end);
    EXPECT_EQ(valuesSpy.count(), 1);
    EXPECT_EQ(slider.upperValue(), 60);
    EXPECT_EQ(upperAtRelease, 60) << "sliderReleased observes the committed value";
}

TEST(RangeSliderContractTest, Contract_DirectionFollowsQSliderSemantics)
{
    RangeSlider horizontal;
    horizontal.setRange(0, 100);
    horizontal.setValues(20, 80);
    horizontal.resize(240, 40);
    EXPECT_LT(horizontal.thumbCenter(Thumb::Lower).x(), horizontal.thumbCenter(Thumb::Upper).x());
    horizontal.setLayoutDirection(Qt::RightToLeft);
    EXPECT_GT(horizontal.thumbCenter(Thumb::Lower).x(), horizontal.thumbCenter(Thumb::Upper).x());
    QTest::mouseClick(&horizontal, Qt::LeftButton, Qt::NoModifier, QPoint(12, 20));
    EXPECT_EQ(horizontal.upperValue(), 100) << "the leading edge holds the maximum in right-to-left layouts";

    RangeSlider vertical(Qt::Vertical);
    vertical.setRange(0, 100);
    vertical.setValues(20, 80);
    vertical.resize(40, 240);
    EXPECT_GT(vertical.thumbCenter(Thumb::Lower).y(), vertical.thumbCenter(Thumb::Upper).y());
    QTest::mouseClick(&vertical, Qt::LeftButton, Qt::NoModifier, QPoint(20, 227));
    EXPECT_EQ(vertical.lowerValue(), 0) << "vertical sliders place the minimum at the bottom";
}

TEST(RangeSliderContractTest, Contract_FullIntegerRangeMapsWithoutOverflow)
{
    const int low = std::numeric_limits<int>::min();
    const int high = std::numeric_limits<int>::max();
    RangeSlider slider;
    slider.setRange(low, high);
    slider.setValues(low, high);
    slider.setPageStep(high);
    slider.resize(4200, 40);
    slider.show();

    EXPECT_EQ(slider.thumbCenter(Thumb::Lower).x(), 12);
    EXPECT_EQ(slider.thumbCenter(Thumb::Upper).x(), 4200 - 12);

    slider.setActiveThumb(Thumb::Upper);
    QTest::keyClick(&slider, Qt::Key_PageUp);
    EXPECT_EQ(slider.upperValue(), high);
    slider.setActiveThumb(Thumb::Lower);
    QTest::keyClick(&slider, Qt::Key_PageDown);
    EXPECT_EQ(slider.lowerValue(), low);
    QTest::keyClick(&slider, Qt::Key_End);
    EXPECT_EQ(slider.lowerValue(), high);

    slider.setValues(low, high);
    QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(2100, 20));
    EXPECT_GT(slider.lowerValue(), low);
    EXPECT_LT(slider.lowerValue(), high);
}

TEST(RangeSliderContractTest, Contract_PaintsSelectedRangeAndStrongTrackInEveryTheme)
{
    const QColor background(250, 250, 250);
    for (const auto theme : {FluentElement::Light, FluentElement::Dark, FluentElement::HighContrast}) {
        SCOPED_TRACE(::testing::Message() << "theme=" << theme);
        RangeSlider slider;
        slider.setProperty("fluentThemeOverride", static_cast<int>(theme));
        slider.setRange(0, 100);
        slider.setValues(20, 80);
        slider.resize(240, 40);

        const QImage image = fluent::testutils::renderControlSurface(slider, 1.0, background);
        const auto& colors = slider.themeColors();
        EXPECT_EQ(image.pixelColor(120, 20), colors.accentDefault) << "the range between the thumbs is filled";
        EXPECT_TRUE(nearColor(image.pixelColor(30, 20), composite(background, colors.strokeStrong)))
            << "the track below the range uses the strong control fill";
        EXPECT_TRUE(nearColor(image.pixelColor(210, 20), composite(background, colors.strokeStrong)))
            << "the track above the range uses the strong control fill";
        fluent::testutils::expectClearSurfacePerimeter(image, background);
    }
}

TEST(RangeSliderContractTest, Contract_HoverFollowsAThumbMovedAwayFromTheCursor)
{
    const QColor background(250, 250, 250);
    const auto prepare = [](RangeSlider& slider) {
        slider.setRange(0, 100);
        slider.setValues(20, 60);
        slider.setActiveThumb(Thumb::Upper);
        slider.resize(240, 40);
        // Keep the real cursor away from the control on desktop runs.
        // zh_CN: 桌面运行时让真实光标远离控件。
        slider.move(-10000, -10000);
    };
    RangeSlider hovered;
    RangeSlider reference;
    prepare(hovered);
    prepare(reference);

    const QPoint center = hovered.thumbCenter(Thumb::Upper);
    QMouseEvent move(QEvent::MouseMove, QPointF(center), QPointF(hovered.mapToGlobal(center)), Qt::NoButton,
        Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&hovered, &move);
    QTest::qWait(400);
    ASSERT_NE(fluent::testutils::renderControlSurface(hovered, 1.0, background),
        fluent::testutils::renderControlSurface(reference, 1.0, background))
        << "the hovered thumb grows";

    QTest::keyClick(&hovered, Qt::Key_End);
    QTest::keyClick(&reference, Qt::Key_End);
    ASSERT_EQ(hovered.upperValue(), 100);
    QTest::qWait(400);
    EXPECT_EQ(fluent::testutils::renderControlSurface(hovered, 1.0, background),
        fluent::testutils::renderControlSurface(reference, 1.0, background))
        << "the thumb that left the pointer drops its hover state";
}

TEST(RangeSliderContractTest, Contract_KeyboardFocusCueStaysInsideTheSurface)
{
    const QColor background(17, 23, 31);
    for (const auto orientation : {Qt::Horizontal, Qt::Vertical}) {
        for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            QWidget host;
            QHBoxLayout layout(&host);
            Button before(QStringLiteral("Before"));
            RangeSlider slider(orientation);
            slider.setLayoutDirection(direction);
            slider.setRange(0, 100);
            slider.setValues(0, 100);
            layout.addWidget(&before);
            layout.addWidget(&slider);
            ASSERT_TRUE(tests::support::showAndFocusWidget(&before));
            for (const qreal scale : {1.0, 1.5, 2.0}) {
                SCOPED_TRACE(::testing::Message()
                    << "orientation=" << orientation << " direction=" << direction << " scale=" << scale);
                before.setFocus();
                const QImage resting = fluent::testutils::renderControlSurface(slider, scale, background);
                slider.setFocus(Qt::TabFocusReason);
                ASSERT_TRUE(slider.hasFocus());
                const QImage focused = fluent::testutils::renderControlSurface(slider, scale, background);
                EXPECT_NE(resting, focused);
                fluent::testutils::expectClearSurfacePerimeter(focused, background);
                before.setFocus();
                slider.setFocus(Qt::MouseFocusReason);
                EXPECT_EQ(resting, fluent::testutils::renderControlSurface(slider, scale, background));
            }
        }
    }
}

#if QT_CONFIG(accessibility)

namespace {

struct ThumbEvent {
    QAccessibleInterface* item = nullptr;
    QAccessible::Event type = QAccessible::InvalidEvent;
};

QVector<ThumbEvent> g_events;

void captureThumbEvent(QAccessibleEvent* event)
{
    if (event)
        g_events.append(ThumbEvent{event->accessibleInterface(), event->type()});
}

int countEvents(QAccessibleInterface* item, QAccessible::Event type)
{
    int count = 0;
    for (const ThumbEvent& event : g_events)
        count += event.item == item && event.type == type;
    return count;
}

} // namespace

TEST(RangeSliderContractTest, Contract_AccessibilityExposesOneValueChildPerThumb)
{
    QWidget host;
    QHBoxLayout layout(&host);
    Button before(QStringLiteral("Before"));
    RangeSlider slider;
    slider.setAccessibleName(QStringLiteral("Price"));
    slider.setRange(0, 100);
    slider.setValues(20, 80);
    slider.setMinimumRange(5);
    slider.setSingleStep(2);
    layout.addWidget(&before);
    layout.addWidget(&slider);
    ASSERT_TRUE(tests::support::showAndFocusWidget(&before));

    QAccessibleInterface* root = QAccessible::queryAccessibleInterface(&slider);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::Grouping);
    EXPECT_EQ(root->text(QAccessible::Name), QStringLiteral("Price"));
    ASSERT_EQ(root->childCount(), 2);

    QAccessibleInterface* lower = root->child(0);
    QAccessibleInterface* upper = root->child(1);
    ASSERT_NE(lower, nullptr);
    ASSERT_NE(upper, nullptr);
    EXPECT_EQ(root->child(0), lower) << "children are cached";
    EXPECT_EQ(root->indexOfChild(upper), 1);
    EXPECT_EQ(lower->parent(), root);
    EXPECT_EQ(lower->role(), QAccessible::Slider);
    EXPECT_EQ(upper->role(), QAccessible::Slider);
    EXPECT_FALSE(lower->text(QAccessible::Name).isEmpty());
    EXPECT_NE(lower->text(QAccessible::Name), upper->text(QAccessible::Name));
    EXPECT_TRUE(lower->rect().contains(slider.mapToGlobal(slider.thumbCenter(Thumb::Lower))));

    QAccessibleValueInterface* lowerValue = lower->valueInterface();
    QAccessibleValueInterface* upperValue = upper->valueInterface();
    ASSERT_NE(lowerValue, nullptr);
    ASSERT_NE(upperValue, nullptr);
    EXPECT_EQ(lowerValue->currentValue().toInt(), 20);
    EXPECT_EQ(lowerValue->minimumValue().toInt(), 0);
    EXPECT_EQ(lowerValue->maximumValue().toInt(), 75) << "the lower bound follows the upper thumb";
    EXPECT_EQ(upperValue->minimumValue().toInt(), 25) << "the upper bound follows the lower thumb";
    EXPECT_EQ(upperValue->maximumValue().toInt(), 100);
    EXPECT_EQ(lowerValue->minimumStepSize().toInt(), 2);

    lowerValue->setCurrentValue(90);
    EXPECT_EQ(slider.lowerValue(), 75);
    upper->actionInterface()->doAction(QAccessibleActionInterface::increaseAction());
    EXPECT_EQ(slider.upperValue(), 82);
    EXPECT_TRUE(upper->actionInterface()->actionNames().contains(QAccessibleActionInterface::decreaseAction()));

    slider.setFocus(Qt::TabFocusReason);
    ASSERT_TRUE(slider.hasFocus());
    EXPECT_EQ(root->focusChild(), lower);
    EXPECT_TRUE(lower->state().focused);
    EXPECT_FALSE(upper->state().focused);
    QTest::keyClick(&slider, Qt::Key_Tab);
    EXPECT_EQ(root->focusChild(), upper);
    EXPECT_TRUE(upper->state().focused);

    slider.setEnabled(false);
    EXPECT_TRUE(lower->state().disabled);
    lowerValue->setCurrentValue(10);
    EXPECT_EQ(slider.lowerValue(), 75) << "disabled thumbs ignore assistive value changes";
}

TEST(RangeSliderContractTest, Contract_AccessibilityAnnouncesValueAndFocusOnTheThumb)
{
    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    QWidget host;
    QHBoxLayout layout(&host);
    Button before(QStringLiteral("Before"));
    RangeSlider slider;
    slider.setRange(0, 100);
    slider.setValues(20, 80);
    layout.addWidget(&before);
    layout.addWidget(&slider);
    ASSERT_TRUE(tests::support::showAndFocusWidget(&before));

    QAccessibleInterface* root = QAccessible::queryAccessibleInterface(&slider);
    ASSERT_NE(root, nullptr);
    QAccessibleInterface* lower = root->child(0);
    QAccessibleInterface* upper = root->child(1);

    QAccessible::UpdateHandler previous = QAccessible::installUpdateHandler(captureThumbEvent);
    g_events.clear();
    slider.setLowerValue(30);
    slider.setLowerValue(30);
    EXPECT_EQ(countEvents(lower, QAccessible::ValueChanged), 1);
    EXPECT_EQ(countEvents(upper, QAccessible::ValueChanged), 0);

    g_events.clear();
    slider.setFocus(Qt::TabFocusReason);
    EXPECT_GE(countEvents(lower, QAccessible::Focus), 1);
    QTest::keyClick(&slider, Qt::Key_Tab);
    EXPECT_EQ(countEvents(upper, QAccessible::Focus), 1);
    QAccessible::installUpdateHandler(previous);
    g_events.clear();
}

#endif // QT_CONFIG(accessibility)
