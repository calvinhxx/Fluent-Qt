#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QPointer>
#include <QSignalSpy>

#include "components/foundation/FontIcon.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/status_info/Toast.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

using fluent::status_info::Toast;

namespace {

void flushDeferredDeletes()
{
    QCoreApplication::sendPostedEvents(
        nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

struct ScopedMaximumVisible {
    explicit ScopedMaximumVisible(int count)
        : previous(Toast::maximumVisible())
    {
        Toast::setMaximumVisible(count);
    }

    ~ScopedMaximumVisible()
    {
        Toast::setMaximumVisible(previous);
    }

    int previous = 3;
};

} // namespace

TEST(ToastTest, Contract_DefaultsAndNoOpSetters)
{
    Toast toast;
    EXPECT_TRUE(toast.title().isEmpty());
    EXPECT_TRUE(toast.message().isEmpty());
    EXPECT_EQ(toast.severity(), Toast::Informational);
    EXPECT_EQ(toast.placement(), Toast::Top);
    EXPECT_EQ(toast.placementMargins(), QMargins(16, 16, 16, 16));
    EXPECT_EQ(toast.duration(), 2200);
    EXPECT_TRUE(toast.isAnimationEnabled());
    EXPECT_FALSE(toast.isOpen());
    EXPECT_GE(Toast::maximumVisible(), 1);

    QSignalSpy messageSpy(&toast, &Toast::messageChanged);
    QSignalSpy severitySpy(&toast, &Toast::severityChanged);
    toast.setMessage(QStringLiteral("Saved"));
    toast.setMessage(QStringLiteral("Saved"));
    toast.setSeverity(Toast::Success);
    toast.setSeverity(Toast::Success);
    EXPECT_EQ(messageSpy.count(), 1);
    EXPECT_EQ(severitySpy.count(), 1);

    toast.setDuration(-1);
    EXPECT_EQ(toast.duration(), 0);
}

TEST(ToastTest, Contract_ShortMessageDoesNotWrap)
{
    QWidget host;
    host.resize(800, 600);

    Toast toast(&host);
    toast.setMessage(QStringLiteral("Connection is unstable"));
    toast.setDuration(0);
    toast.setAnimationEnabled(false);
    ASSERT_TRUE(toast.present(&host));

    auto* message = toast.findChild<fluent::textfields::Label*>(
        QStringLiteral("fluentToastMessage"));
    ASSERT_NE(message, nullptr);
    EXPECT_FALSE(message->wordWrap());
    EXPECT_EQ(message->text().count(QLatin1Char('\n')), 0);

    const QRect card =
        fluent::overlay::visibleCardGeometry(toast.geometry());
    EXPECT_LT(card.height(), 72);
}

TEST(ToastTest, Contract_PresentUsesTopLevelAndTracksResize)
{
    QWidget host;
    host.resize(800, 600);
    host.show();
    QApplication::processEvents();
    QWidget anchor(&host);
    anchor.show();

    Toast toast(&anchor);
    toast.setMessage(QStringLiteral("Saved"));
    toast.setDuration(0);
    toast.setAnimationEnabled(false);
    QSignalSpy openSpy(&toast, &Toast::isOpenChanged);
    ASSERT_TRUE(toast.present(&anchor));
    ASSERT_TRUE(toast.present(&anchor));

    EXPECT_TRUE(toast.isOpen());
    EXPECT_EQ(openSpy.count(), 1);
    EXPECT_EQ(toast.parentWidget(), &host);
    const QRect surface = fluent::overlay::overlaySurfaceRect(&host);
    const QRect card = fluent::overlay::visibleCardGeometry(toast.geometry());
    EXPECT_EQ(card.top(), surface.top() + 16);
    EXPECT_NEAR(card.center().x(), surface.center().x(), 1);

    const QPoint oldPosition = toast.pos();
    host.resize(1000, 700);
    QCoreApplication::processEvents();
    EXPECT_NE(toast.pos(), oldPosition);

    toast.dismiss();
    EXPECT_FALSE(toast.isOpen());
    EXPECT_FALSE(toast.isVisible());
}

TEST(ToastTest, Contract_ToastDoesNotBlockPointerHitTesting)
{
    QWidget host;
    host.resize(640, 480);
    QWidget target(&host);
    target.setGeometry(host.rect());
    target.show();
    host.show();
    QApplication::processEvents();

    Toast toast(&host);
    toast.setMessage(QStringLiteral("Saved"));
    toast.setDuration(0);
    toast.setAnimationEnabled(false);
    ASSERT_TRUE(toast.present(&host));
    QApplication::processEvents();

    ASSERT_TRUE(toast.testAttribute(Qt::WA_TransparentForMouseEvents));
    const QPoint toastCenter =
        toast.mapTo(&host, toast.rect().center());
    ASSERT_TRUE(toast.geometry().contains(toastCenter));
    EXPECT_EQ(host.childAt(toastCenter), &target);
}

TEST(ToastTest, Contract_CornerPlacementAndNormalizedMargins)
{
    QWidget host;
    host.resize(640, 480);

    Toast toast(&host);
    toast.setMessage(QStringLiteral("Done"));
    toast.setDuration(0);
    toast.setAnimationEnabled(false);
    toast.setPlacement(Toast::BottomEnd);
    toast.setPlacementMargins(QMargins(-1, -2, 24, 28));
    EXPECT_EQ(toast.placementMargins(), QMargins(0, 0, 24, 28));
    ASSERT_TRUE(toast.present(&host));

    const QRect surface = fluent::overlay::overlaySurfaceRect(&host);
    const QRect card = fluent::overlay::visibleCardGeometry(toast.geometry());
    EXPECT_EQ(card.bottom(), surface.bottom() - 28);
    EXPECT_EQ(card.right(), surface.right() - 24);

    toast.setPlacement(Toast::TopStart);
    const QRect topStart =
        fluent::overlay::visibleCardGeometry(toast.geometry());
    EXPECT_EQ(topStart.top(), surface.top());
    EXPECT_EQ(topStart.left(), surface.left());
}

TEST(ToastTest, Contract_SeverityUsesFontIcon)
{
    Toast toast;
    toast.setSeverity(Toast::Success);
    auto* icon = toast.findChild<fluent::FontIcon*>();
    ASSERT_NE(icon, nullptr);
    EXPECT_EQ(icon->glyph(), Typography::Icons::Success);

    toast.setSeverity(Toast::Error);
    EXPECT_EQ(icon->glyph(), Typography::Icons::ErrorIcon);
}

TEST(ToastTest, Contract_ManagedToastsStackUntilMaximumVisible)
{
    ScopedMaximumVisible scoped(2);
    QWidget host;
    host.resize(640, 480);

    QPointer<Toast> first =
        Toast::showToast(
            &host, QStringLiteral("First"), Toast::Informational, 0);
    QPointer<Toast> second =
        Toast::showToast(
            &host, QStringLiteral("Second"), Toast::Success, 0);
    ASSERT_FALSE(first.isNull());
    ASSERT_FALSE(second.isNull());
    EXPECT_TRUE(first->isOpen());
    EXPECT_TRUE(second->isOpen());

    const QRect firstCard =
        fluent::overlay::visibleCardGeometry(first->geometry());
    const QRect secondCard =
        fluent::overlay::visibleCardGeometry(second->geometry());
    EXPECT_LT(firstCard.top(), secondCard.top());

    QPointer<Toast> third =
        Toast::showToast(
            &host, QStringLiteral("Third"), Toast::Warning, 0);
    flushDeferredDeletes();
    EXPECT_TRUE(first.isNull());
    ASSERT_FALSE(second.isNull());
    ASSERT_FALSE(third.isNull());
    EXPECT_TRUE(second->isOpen());
    EXPECT_TRUE(third->isOpen());

    second->setAnimationEnabled(false);
    third->setAnimationEnabled(false);
    second->dismiss();
    third->dismiss();
    flushDeferredDeletes();
    EXPECT_TRUE(second.isNull());
    EXPECT_TRUE(third.isNull());
}

TEST(ToastTest, Contract_StackOffsetsFollowPlacementDirection)
{
    ScopedMaximumVisible scoped(3);
    QWidget host;
    host.resize(800, 600);

    QPointer<Toast> topA = Toast::showToast(
        &host,
        QStringLiteral("Top A"),
        Toast::Informational,
        0,
        Toast::TopEnd);
    QPointer<Toast> topB = Toast::showToast(
        &host,
        QStringLiteral("Top B"),
        Toast::Success,
        0,
        Toast::TopEnd);
    QPointer<Toast> bottomA = Toast::showToast(
        &host,
        QStringLiteral("Bottom A"),
        Toast::Warning,
        0,
        Toast::BottomStart);
    QPointer<Toast> bottomB = Toast::showToast(
        &host,
        QStringLiteral("Bottom B"),
        Toast::Error,
        0,
        Toast::BottomStart);
    ASSERT_FALSE(topA.isNull());
    ASSERT_FALSE(topB.isNull());
    ASSERT_FALSE(bottomA.isNull());
    ASSERT_FALSE(bottomB.isNull());

    const QRect surface = fluent::overlay::overlaySurfaceRect(&host);
    const QRect topACard =
        fluent::overlay::visibleCardGeometry(topA->geometry());
    const QRect topBCard =
        fluent::overlay::visibleCardGeometry(topB->geometry());
    const QRect bottomACard =
        fluent::overlay::visibleCardGeometry(bottomA->geometry());
    const QRect bottomBCard =
        fluent::overlay::visibleCardGeometry(bottomB->geometry());

    EXPECT_EQ(topACard.right(), surface.right() - 16);
    EXPECT_EQ(topBCard.right(), surface.right() - 16);
    EXPECT_LT(topACard.top(), topBCard.top());

    EXPECT_EQ(bottomACard.left(), surface.left() + 16);
    EXPECT_EQ(bottomBCard.left(), surface.left() + 16);
    EXPECT_GT(bottomACard.bottom(), bottomBCard.bottom());
}
