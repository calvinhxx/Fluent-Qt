#include <gtest/gtest.h>

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QVector>

#include <algorithm>

#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
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

#if QT_CONFIG(accessibility)

struct AccessibleEventRecord {
    QObject* object = nullptr;
    QAccessible::Event type = QAccessible::InvalidEvent;
    QString announcement;
    int politeness = -1;
};

QVector<AccessibleEventRecord> g_accessibleEvents;

void captureAccessibleEvent(QAccessibleEvent* event)
{
    if (!event)
        return;

    AccessibleEventRecord record;
    record.object = event->object();
    record.type = event->type();
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (event->type() == QAccessible::Announcement) {
        auto* announcement =
            static_cast<QAccessibleAnnouncementEvent*>(event);
        record.announcement = announcement->message();
        record.politeness =
            static_cast<int>(announcement->politeness());
    }
#endif
    g_accessibleEvents.append(record);
}

struct ScopedAccessibleEventCapture {
    ScopedAccessibleEventCapture()
    {
        previous = QAccessible::installUpdateHandler(
            captureAccessibleEvent);
        eventDeliveryActive = QAccessible::isActive();
        g_accessibleEvents.clear();
    }

    ~ScopedAccessibleEventCapture()
    {
        QAccessible::installUpdateHandler(previous);
        g_accessibleEvents.clear();
    }

    QAccessible::UpdateHandler previous = nullptr;
    bool eventDeliveryActive = false;
};

#endif

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
    EXPECT_EQ(toast.action(), nullptr);
    EXPECT_FALSE(toast.isPauseOnHoverEnabled());
    EXPECT_TRUE(toast.updateKey().isEmpty());
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

TEST(ToastTest, Contract_PresentAnnouncesAccessibleContent)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    QWidget host;
    host.resize(640, 480);
    host.show();

    Toast toast(&host);
    toast.setTitle(QStringLiteral("Sync complete"));
    toast.setMessage(QStringLiteral("12 files are available"));
    toast.setDuration(0);
    toast.setAnimationEnabled(false);

    ScopedAccessibleEventCapture capture;
    ASSERT_TRUE(toast.present(&host));
    EXPECT_EQ(
        toast.accessibleName(),
        QStringLiteral("Sync complete: 12 files are available"));

    if (capture.eventDeliveryActive) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        const auto match = std::find_if(
            g_accessibleEvents.cbegin(),
            g_accessibleEvents.cend(),
            [&toast](const AccessibleEventRecord& record) {
                return record.object == &toast
                    && record.type == QAccessible::Announcement;
            });
        ASSERT_NE(match, g_accessibleEvents.cend());
        EXPECT_EQ(
            match->announcement,
            QStringLiteral(
                "Sync complete: 12 files are available"));
        EXPECT_EQ(
            match->politeness,
            static_cast<int>(
                QAccessible::AnnouncementPoliteness::Polite));
#else
        EXPECT_TRUE(std::any_of(
            g_accessibleEvents.cbegin(),
            g_accessibleEvents.cend(),
            [&toast](const AccessibleEventRecord& record) {
                return record.object == &toast
                    && record.type == QAccessible::Alert;
            }));
#endif
    }
#endif
}

TEST(ToastTest, Contract_AccessibleNameTracksContentUnlessCallerOverrides)
{
    Toast toast;
    toast.setMessage(QStringLiteral("Saved"));
    EXPECT_EQ(toast.accessibleName(), QStringLiteral("Saved"));

    toast.setMessage(QStringLiteral("Published"));
    EXPECT_EQ(toast.accessibleName(), QStringLiteral("Published"));

    toast.setAccessibleName(QStringLiteral("Custom announcement"));
    toast.setTitle(QStringLiteral("Release"));
    toast.setMessage(QStringLiteral("Ready"));
    EXPECT_EQ(
        toast.accessibleName(),
        QStringLiteral("Custom announcement"));
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

TEST(ToastTest, Contract_OpenStateHandlerCanSynchronouslyDeleteToast)
{
    QWidget host;
    host.resize(640, 480);
    host.show();
    QWidget anchor(&host);
    anchor.show();

    auto* toast = new Toast(&anchor);
    toast->setMessage(QStringLiteral("Saved"));
    toast->setDuration(0);
    toast->setAnimationEnabled(false);
    QPointer<Toast> guard(toast);
    QObject::connect(toast, &Toast::isOpenChanged, &host,
                     [toast](bool open) {
                         if (open)
                             delete toast;
                     });

    EXPECT_FALSE(toast->present(&anchor));
    EXPECT_TRUE(guard.isNull());
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

TEST(ToastTest, Contract_ActionIsBorrowedAndReportsDismissReason)
{
    QWidget host;
    host.resize(640, 480);
    host.show();
    QApplication::processEvents();

    QAction action(QStringLiteral("&Retry"), &host);
    Toast toast(&host);
    toast.setMessage(QStringLiteral("Upload failed"));
    toast.setDuration(0);
    toast.setAnimationEnabled(false);

    int actionChangedCount = 0;
    QObject::connect(
        &toast,
        &Toast::actionChanged,
        &host,
        [&actionChangedCount](QAction*) {
        ++actionChangedCount;
    });
    toast.setAction(&action);
    EXPECT_EQ(toast.action(), &action);
    EXPECT_EQ(action.parent(), &host);
    EXPECT_EQ(actionChangedCount, 1);
    EXPECT_FALSE(
        toast.testAttribute(Qt::WA_TransparentForMouseEvents));

    auto* button =
        toast.findChild<fluent::basicinput::Button*>(
            QStringLiteral("fluentToastAction"));
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text(), QStringLiteral("Retry"));
    EXPECT_EQ(button->accessibleName(), QStringLiteral("Retry"));

    int triggerCount = 0;
    int reasonCount = 0;
    Toast::DismissReason reason = Toast::Programmatic;
    QObject::connect(
        &action,
        &QAction::triggered,
        &host,
        [&triggerCount]() {
        ++triggerCount;
    });
    QObject::connect(
        &action,
        &QAction::triggered,
        &toast,
        &Toast::dismiss);
    QObject::connect(
        &toast,
        &Toast::dismissedWithReason,
        &host,
        [&reasonCount, &reason](Toast::DismissReason value) {
        ++reasonCount;
        reason = value;
    });

    ASSERT_TRUE(toast.present(&host));
    QTest::mouseClick(button, Qt::LeftButton);
    EXPECT_EQ(triggerCount, 1);
    EXPECT_EQ(reasonCount, 1);
    EXPECT_EQ(reason, Toast::ActionInvoked);
    EXPECT_FALSE(toast.isOpen());

    toast.setAction(nullptr);
    EXPECT_EQ(actionChangedCount, 2);
    EXPECT_TRUE(
        toast.testAttribute(Qt::WA_TransparentForMouseEvents));
}

TEST(ToastTest, Contract_HoverPausePreservesRemainingDuration)
{
    QWidget host;
    host.resize(640, 480);
    host.show();

    Toast toast(&host);
    toast.setMessage(QStringLiteral("Hover to inspect"));
    toast.setDuration(90);
    toast.setAnimationEnabled(false);
    toast.setPauseOnHoverEnabled(true);
    ASSERT_FALSE(
        toast.testAttribute(Qt::WA_TransparentForMouseEvents));
    ASSERT_TRUE(toast.present(&host));

    QTest::qWait(20);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    FluentEnterEvent enter(
        QPointF(4, 4),
        QPointF(4, 4),
        QPointF(toast.mapToGlobal(QPoint(4, 4))));
#else
    FluentEnterEvent enter(QEvent::Enter);
#endif
    QCoreApplication::sendEvent(&toast, &enter);
    QTest::qWait(120);
    EXPECT_TRUE(toast.isOpen());

    QEvent leave(QEvent::Leave);
    QCoreApplication::sendEvent(&toast, &leave);
    QTRY_VERIFY_WITH_TIMEOUT(!toast.isOpen(), 500);
}

TEST(ToastTest, Contract_TimeoutReportsDismissReason)
{
    QWidget host;
    host.resize(640, 480);

    Toast toast(&host);
    toast.setMessage(QStringLiteral("Saved"));
    toast.setDuration(20);
    toast.setAnimationEnabled(false);

    Toast::DismissReason reason = Toast::Programmatic;
    int reasonCount = 0;
    QObject::connect(
        &toast,
        &Toast::dismissedWithReason,
        &host,
        [&reasonCount, &reason](Toast::DismissReason value) {
        ++reasonCount;
        reason = value;
    });
    ASSERT_TRUE(toast.present(&host));
    QTRY_VERIFY_WITH_TIMEOUT(!toast.isOpen(), 500);
    EXPECT_EQ(reasonCount, 1);
    EXPECT_EQ(reason, Toast::TimedOut);
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

    int firstDismissCount = 0;
    Toast::DismissReason firstReason = Toast::Programmatic;
    QObject::connect(
        first.data(),
        &Toast::dismissedWithReason,
        &host,
        [&firstDismissCount, &firstReason](
            Toast::DismissReason reason) {
        ++firstDismissCount;
        firstReason = reason;
    });

    QPointer<Toast> third =
        Toast::showToast(
            &host, QStringLiteral("Third"), Toast::Warning, 0);
    flushDeferredDeletes();
    EXPECT_TRUE(first.isNull());
    ASSERT_FALSE(second.isNull());
    ASSERT_FALSE(third.isNull());
    EXPECT_TRUE(second->isOpen());
    EXPECT_TRUE(third->isOpen());
    EXPECT_EQ(firstDismissCount, 1);
    EXPECT_EQ(firstReason, Toast::Evicted);

    second->setAnimationEnabled(false);
    third->setAnimationEnabled(false);
    second->dismiss();
    third->dismiss();
    flushDeferredDeletes();
    EXPECT_TRUE(second.isNull());
    EXPECT_TRUE(third.isNull());
}

TEST(ToastTest, Contract_UpdateKeyRefreshesInPlaceWithinStackScope)
{
    ScopedMaximumVisible scoped(2);
    QWidget firstHost;
    firstHost.resize(640, 480);
    QWidget secondHost;
    secondHost.resize(640, 480);

    QPointer<Toast> first = Toast::showOrUpdateToast(
        &firstHost,
        QStringLiteral("sync"),
        QStringLiteral("Uploading"),
        Toast::Informational,
        0,
        Toast::TopEnd);
    ASSERT_FALSE(first.isNull());
    EXPECT_EQ(first->updateKey(), QStringLiteral("sync"));

    QSignalSpy updatedSpy(first.data(), &Toast::updated);
    Toast* updated = Toast::showOrUpdateToast(
        &firstHost,
        QStringLiteral("sync"),
        QStringLiteral("Upload complete"),
        Toast::Success,
        0,
        Toast::TopEnd);
    ASSERT_EQ(updated, first.data());
    EXPECT_EQ(updatedSpy.count(), 1);
    EXPECT_EQ(first->message(), QStringLiteral("Upload complete"));
    EXPECT_EQ(first->severity(), Toast::Success);

    QPointer<Toast> otherPlacement = Toast::showOrUpdateToast(
        &firstHost,
        QStringLiteral("sync"),
        QStringLiteral("Bottom status"),
        Toast::Warning,
        0,
        Toast::BottomEnd);
    QPointer<Toast> otherHost = Toast::showOrUpdateToast(
        &secondHost,
        QStringLiteral("sync"),
        QStringLiteral("Other window"),
        Toast::Informational,
        0,
        Toast::TopEnd);
    ASSERT_FALSE(otherPlacement.isNull());
    ASSERT_FALSE(otherHost.isNull());
    EXPECT_NE(otherPlacement.data(), first.data());
    EXPECT_NE(otherHost.data(), first.data());

    first->setAnimationEnabled(false);
    otherPlacement->setAnimationEnabled(false);
    otherHost->setAnimationEnabled(false);
    first->dismiss();
    otherPlacement->dismiss();
    otherHost->dismiss();
    flushDeferredDeletes();
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
