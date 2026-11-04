#include "components/basicinput/Button.h"
#include "components/basicinput/RatingControl.h"
#include "components/foundation/FluentElement.h"
#include "ControlSurfaceTestUtils.h"
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QRegion>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <gtest/gtest.h>

using namespace fluent::basicinput;

TEST(RatingControlTest, DefaultPropertyValues)
{
    RatingControl rc;
    EXPECT_DOUBLE_EQ(rc.value(), -1.0);
    EXPECT_DOUBLE_EQ(rc.placeholderValue(), 0.0);
    EXPECT_TRUE(rc.caption().isEmpty());
    EXPECT_TRUE(rc.isClearEnabled());
    EXPECT_FALSE(rc.isReadOnly());
    EXPECT_EQ(rc.maxRating(), 5);
    EXPECT_EQ(rc.starSize(), 16);
    EXPECT_EQ(rc.focusPolicy(), Qt::StrongFocus);
}

// ── Value 属性 ───────────────────────────────────────────────────────────────

TEST(RatingControlTest, ValueEmitsOnlyWhenChanged)
{
    RatingControl rc;
    QSignalSpy spy(&rc, &RatingControl::valueChanged);
    rc.setValue(3.0);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_DOUBLE_EQ(spy.first().first().toDouble(), 3.0);
    EXPECT_DOUBLE_EQ(rc.value(), 3.0);
    rc.setValue(3.0);
    EXPECT_EQ(spy.count(), 1);
}

TEST(RatingControlTest, ValueClampedToMaxRating)
{
    RatingControl rc;
    rc.setMaxRating(5);
    rc.setValue(10.0);
    EXPECT_DOUBLE_EQ(rc.value(), 5.0);
}

TEST(RatingControlTest, ValueClampedToMinusOne)
{
    RatingControl rc;
    rc.setValue(-5.0);
    EXPECT_DOUBLE_EQ(rc.value(), -1.0);
}

TEST(RatingControlTest, HalfStarValue)
{
    RatingControl rc;
    rc.setValue(3.5);
    EXPECT_DOUBLE_EQ(rc.value(), 3.5);
}

// ── PlaceholderValue 属性 ────────────────────────────────────────────────────

TEST(RatingControlTest, SetPlaceholderValueEmitsSignal)
{
    RatingControl rc;
    QSignalSpy spy(&rc, &RatingControl::placeholderValueChanged);
    rc.setPlaceholderValue(2.5);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_DOUBLE_EQ(spy.first().first().toDouble(), 2.5);
}

TEST(RatingControlTest, PlaceholderValueClampedToZero)
{
    RatingControl rc;
    rc.setPlaceholderValue(-1.0);
    EXPECT_DOUBLE_EQ(rc.placeholderValue(), 0.0);
}

// ── Caption 属性 ─────────────────────────────────────────────────────────────

TEST(RatingControlTest, CaptionEmitsOnlyWhenChanged)
{
    RatingControl rc;
    QSignalSpy spy(&rc, &RatingControl::captionChanged);
    rc.setCaption("312 ratings");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(rc.caption(), "312 ratings");
    rc.setCaption("312 ratings");
    EXPECT_EQ(spy.count(), 1);
}

// ── IsClearEnabled 属性 ──────────────────────────────────────────────────────

TEST(RatingControlTest, ClearEnabledControlsRepeatedClicksOnTheSameStar)
{
    RatingControl rc;
    QSignalSpy spy(&rc, &RatingControl::isClearEnabledChanged);
    rc.setIsClearEnabled(false);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(rc.isClearEnabled());

    rc.resize(rc.sizeHint());
    QTest::mouseClick(&rc, Qt::LeftButton);
    const double selected = rc.value();
    ASSERT_GT(selected, 0.0);
    QTest::mouseClick(&rc, Qt::LeftButton);
    EXPECT_DOUBLE_EQ(rc.value(), selected);

    rc.setIsClearEnabled(true);
    QTest::mouseClick(&rc, Qt::LeftButton);
    EXPECT_DOUBLE_EQ(rc.value(), -1.0);
}

// ── IsReadOnly 属性 ──────────────────────────────────────────────────────────

TEST(RatingControlTest, SetIsReadOnlyEmitsSignal)
{
    RatingControl rc;
    QSignalSpy spy(&rc, &RatingControl::isReadOnlyChanged);
    rc.setIsReadOnly(true);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_TRUE(rc.isReadOnly());
}

TEST(RatingControlTest, ReadOnlyCursorIsArrow)
{
    RatingControl rc;
    rc.setIsReadOnly(true);
    EXPECT_EQ(rc.cursor().shape(), Qt::ArrowCursor);
}

TEST(RatingControlTest, KeyboardNavigationSupportsHalfStepsBoundsAndRtl)
{
    RatingControl rc;
    rc.resize(rc.sizeHint());
    rc.show();
    QApplication::processEvents();
    rc.setFocus();
    rc.setValue(2.0);

    QTest::keyClick(&rc, Qt::Key_Right);
    EXPECT_DOUBLE_EQ(rc.value(), 2.5);
    QTest::keyClick(&rc, Qt::Key_Left);
    EXPECT_DOUBLE_EQ(rc.value(), 2.0);
    QTest::keyClick(&rc, Qt::Key_Up);
    EXPECT_DOUBLE_EQ(rc.value(), 2.5);
    QTest::keyClick(&rc, Qt::Key_Down);
    EXPECT_DOUBLE_EQ(rc.value(), 2.0);
    QTest::keyClick(&rc, Qt::Key_End);
    EXPECT_DOUBLE_EQ(rc.value(), 5.0);
    QTest::keyClick(&rc, Qt::Key_Home);
    EXPECT_DOUBLE_EQ(rc.value(), -1.0);

    rc.setIsClearEnabled(false);
    QTest::keyClick(&rc, Qt::Key_Home);
    EXPECT_DOUBLE_EQ(rc.value(), 0.5);

    rc.setLayoutDirection(Qt::RightToLeft);
    rc.setValue(2.0);
    QTest::keyClick(&rc, Qt::Key_Right);
    EXPECT_DOUBLE_EQ(rc.value(), 1.5);
    QTest::keyClick(&rc, Qt::Key_Left);
    EXPECT_DOUBLE_EQ(rc.value(), 2.0);

    rc.setIsReadOnly(true);
    QTest::keyClick(&rc, Qt::Key_Left);
    EXPECT_DOUBLE_EQ(rc.value(), 2.0);
}

TEST(RatingControlTest, MouseActivationTakesFocus)
{
    QWidget host;
    RatingControl rc(&host);
    rc.setGeometry(20, 20, rc.sizeHint().width(), rc.sizeHint().height());
    host.show();
    rc.show();
    QApplication::processEvents();

    QTest::mouseClick(&rc, Qt::LeftButton, Qt::NoModifier, rc.rect().center());

    EXPECT_TRUE(rc.hasFocus());
}

// ── MaxRating 属性 ───────────────────────────────────────────────────────────

TEST(RatingControlTest, SetMaxRatingEmitsSignal)
{
    RatingControl rc;
    QSignalSpy spy(&rc, &RatingControl::maxRatingChanged);
    rc.setMaxRating(10);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(rc.maxRating(), 10);
}

TEST(RatingControlTest, MaxRatingClampsExistingValue)
{
    RatingControl rc;
    rc.setValue(5.0);
    rc.setMaxRating(3);
    EXPECT_DOUBLE_EQ(rc.value(), 3.0);
}

TEST(RatingControlTest, MaxRatingMinIsOne)
{
    RatingControl rc;
    rc.setMaxRating(0);
    EXPECT_EQ(rc.maxRating(), 1);
}

// ── StarSize 属性 ────────────────────────────────────────────────────────────

TEST(RatingControlTest, SetStarSizeEmitsSignal)
{
    RatingControl rc;
    QSignalSpy spy(&rc, &RatingControl::starSizeChanged);
    rc.setStarSize(32);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(rc.starSize(), 32);
}

// ── SizeHint ─────────────────────────────────────────────────────────────────

TEST(RatingControlTest, SizeHintReflectsMaxRating)
{
    RatingControl rc;
    QSize hint5 = rc.sizeHint();
    rc.setMaxRating(10);
    QSize hint10 = rc.sizeHint();
    EXPECT_GT(hint10.width(), hint5.width());
}

TEST(RatingControlTest, SizeHintIncludesCaption)
{
    RatingControl rc;
    QSize hintNoCaption = rc.sizeHint();
    rc.setCaption("312 ratings");
    QSize hintWithCaption = rc.sizeHint();
    EXPECT_GT(hintWithCaption.width(), hintNoCaption.width());
}

// ── Disabled 状态 ────────────────────────────────────────────────────────────

TEST(RatingControlTest, DisabledState)
{
    RatingControl rc;
    rc.setValue(3.0);
    rc.setEnabled(false);
    EXPECT_FALSE(rc.isEnabled());
    EXPECT_DOUBLE_EQ(rc.value(), 3.0);
}

TEST(RatingControlTest, Contract_SelectedStarHasSolidCenterAndEmptyStarDoesNot)
{
    RatingControl rating;
    rating.setMaxRating(2);
    rating.setStarSize(28);
    rating.setValue(1.0);
    rating.resize(rating.sizeHint());

    const QColor background(17, 23, 31);
    QImage image(rating.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(background);
    QPainter painter(&image);
    rating.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    painter.end();

    const QColor selectedCenter = image.pixelColor(image.width() / 4, image.height() / 2);
    const QColor emptyCenter = image.pixelColor(image.width() * 3 / 4, image.height() / 2);
    EXPECT_NE(selectedCenter, background);
    EXPECT_EQ(emptyCenter, background);
}

TEST(RatingControlTest, Contract_LegacyFixedSizeKeepsStarsCaptionAndHitTargets)
{
    const QColor background(17, 23, 31);
    for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        for (const QString caption : {QString(), QStringLiteral("Rating")}) {
            RatingControl compact, padded;
            for (auto* rating : {&compact, &padded}) {
                rating->setLayoutDirection(direction);
                rating->setCaption(caption);
                rating->setValue(2.5);
            }
            const int inset = padded.themeSpacing().gap.tight;
            compact.setFixedSize(padded.sizeHint() - QSize(inset * 2, inset * 2));
            padded.resize(padded.sizeHint());
            for (const qreal scale : {1.0, 1.25, 1.5, 2.0}) {
                const QImage actual = fluent::testutils::renderControlSurface(compact, scale, background);
                const QImage reference = fluent::testutils::renderControlSurface(padded, scale, background);
                EXPECT_EQ(actual,
                    reference.copy(qRound(inset * scale), qRound(inset * scale), actual.width(), actual.height()));
            }
            const int leading = direction == Qt::RightToLeft ? compact.width() - 2 : 2;
            QTest::mouseClick(&compact, Qt::LeftButton, Qt::NoModifier, QPoint(leading, compact.height() / 2));
            EXPECT_DOUBLE_EQ(compact.value(), 0.5);
        }
    }
}

TEST(RatingControlTest, Contract_FocusPreservesStarInkAndEdgesAtEveryScale)
{
    const QColor background(17, 23, 31);
    for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        for (const int size : {16, 28, 40}) {
            QWidget host;
            QVBoxLayout layout(&host);
            Button before(QStringLiteral("Before"));
            RatingControl rating;
            rating.setLayoutDirection(direction);
            rating.setStarSize(size);
            rating.setValue(2.5);
            layout.addWidget(&before);
            layout.addWidget(&rating);
            host.adjustSize();
            host.show();
            QApplication::setActiveWindow(&host);
            for (const auto theme :
                {fluent::FluentElement::Light, fluent::FluentElement::Dark, fluent::FluentElement::HighContrast}) {
                rating.setProperty("fluentThemeOverride", static_cast<int>(theme));
                for (const qreal scale : {1.0, 1.25, 1.5, 2.0}) {
                    SCOPED_TRACE(::testing::Message()
                        << "size=" << size << " direction=" << direction << " theme=" << theme << " scale=" << scale);
                    before.setFocus();
                    ASSERT_TRUE(before.hasFocus());
                    const QImage resting = fluent::testutils::renderControlSurface(rating, scale, background);
                    rating.setFocus(Qt::TabFocusReason);
                    ASSERT_TRUE(rating.hasFocus());
                    const QImage focused = fluent::testutils::renderControlSurface(rating, scale, background);
                    fluent::testutils::expectClearSurfacePerimeter(resting, background);
                    EXPECT_NE(resting, focused);
                    for (int y = 0; y < resting.height(); ++y) {
                        for (int x = 0; x < resting.width(); ++x) {
                            if (resting.pixelColor(x, y) != background)
                                EXPECT_EQ(resting.pixel(x, y), focused.pixel(x, y)) << "x=" << x << " y=" << y;
                        }
                    }
                }
            }
        }
    }
}
