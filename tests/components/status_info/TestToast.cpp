#include <gtest/gtest.h>

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QPointer>
#include <QPropertyAnimation>
#include <QSignalSpy>
#include <QTest>
#include <QVector>
#include <QVariantAnimation>

#include <algorithm>

#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/foundation/FontIcon.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/layout/Card.h"
#include "components/status_info/Toast.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

using fluent::status_info::Toast;

namespace {

class MotionPolicyScope final {
public:
    explicit MotionPolicyScope(fluent::MotionPolicy::Mode mode)
        : m_previous(fluent::MotionPolicy::instance().mode())
    {
        fluent::MotionPolicy::instance().setMode(mode);
    }

    ~MotionPolicyScope() { fluent::MotionPolicy::instance().setMode(m_previous); }

private:
    fluent::MotionPolicy::Mode m_previous;
};

void flushDeferredDeletes()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

struct ScopedMaximumVisible {
    explicit ScopedMaximumVisible(int count) : previous(Toast::maximumVisible())
    {
        Toast::setMaximumVisible(count);
    }

    ~ScopedMaximumVisible() { Toast::setMaximumVisible(previous); }

    int previous = 3;
};

#if QT_CONFIG(accessibility)

struct AccessibleEventRecord {
    QObject* object = nullptr;
    QAccessible::Event type = QAccessible::InvalidEvent;
    QString announcement;
    FluentAccessibleAnnouncementPoliteness politeness =
        FluentAccessibleAnnouncementPoliteness::Unspecified;
};

QVector<AccessibleEventRecord> g_accessibleEvents;

void captureAccessibleEvent(QAccessibleEvent* event)
{
    if (!event)
        return;

    AccessibleEventRecord record;
    record.object = event->object();
    record.type = event->type();
    record.announcement = fluentAccessibleAnnouncementMessage(event);
    record.politeness = fluentAccessibleAnnouncementPoliteness(event);
    g_accessibleEvents.append(record);
}

struct ScopedAccessibleEventCapture {
    ScopedAccessibleEventCapture()
    {
        previous = QAccessible::installUpdateHandler(captureAccessibleEvent);
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
    EXPECT_FALSE(toast.isClosable());
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

TEST(ToastTest, Contract_MotionPolicyDisabledSettlesPresentationAndDismissal)
{
    MotionPolicyScope policyScope(fluent::MotionPolicy::Mode::Disabled);
    QWidget host;
    host.resize(640, 480);
    host.show();

    Toast toast(&host);
    toast.setMessage(QStringLiteral("Saved"));
    toast.setDuration(0);
    QSignalSpy dismissedSpy(&toast, &Toast::dismissed);

    ASSERT_TRUE(toast.present(&host));
    EXPECT_TRUE(toast.isOpen());
    EXPECT_TRUE(toast.isVisible());
    EXPECT_DOUBLE_EQ(toast.toastProgress(), 1.0);

    toast.dismiss();

    EXPECT_FALSE(toast.isOpen());
    EXPECT_FALSE(toast.isVisible());
    EXPECT_DOUBLE_EQ(toast.toastProgress(), 0.0);
    EXPECT_EQ(dismissedSpy.count(), 1);
}

TEST(ToastTest, Contract_DisablingLocalAnimationSettlesActivePresentation)
{
    MotionPolicyScope policyScope(fluent::MotionPolicy::Mode::Full);
    QWidget host;
    host.resize(640, 480);
    host.show();

    Toast toast(&host);
    toast.setMessage(QStringLiteral("Saved"));
    toast.setDuration(0);
    ASSERT_TRUE(toast.present(&host));

    toast.setAnimationEnabled(false);

    EXPECT_TRUE(toast.isOpen());
    EXPECT_TRUE(toast.isVisible());
    EXPECT_DOUBLE_EQ(toast.toastProgress(), 1.0);

    toast.dismiss();
    EXPECT_FALSE(toast.isOpen());
    EXPECT_FALSE(toast.isVisible());
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
    EXPECT_EQ(toast.accessibleName(), QStringLiteral("Sync complete: 12 files are available"));

    if (capture.eventDeliveryActive) {
        const auto match =
            std::find_if(g_accessibleEvents.cbegin(), g_accessibleEvents.cend(),
                         [&toast](const AccessibleEventRecord& record) {
                             return record.object == &toast &&
                                    record.type == fluentAccessibleAnnouncementEventType();
                         });
        ASSERT_NE(match, g_accessibleEvents.cend());
        if (fluentAccessibleAnnouncementSupportsDetails()) {
            EXPECT_EQ(match->announcement, QStringLiteral("Sync complete: 12 files are available"));
            EXPECT_EQ(match->politeness, FluentAccessibleAnnouncementPoliteness::Polite);
        }
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
    EXPECT_EQ(toast.accessibleName(), QStringLiteral("Custom announcement"));
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

    auto* message =
        toast.findChild<fluent::textfields::Label*>(QStringLiteral("fluentToastMessage"));
    ASSERT_NE(message, nullptr);
    EXPECT_FALSE(message->wordWrap());
    EXPECT_EQ(message->text().count(QLatin1Char('\n')), 0);
    EXPECT_EQ(message->fluentTypography(), Typography::FontRole::BodyStrong);

    const QRect card = fluent::overlay::visibleCardGeometry(toast.geometry());
    EXPECT_GE(card.width(), 220);
    EXPECT_LT(card.width(), 300);
    EXPECT_EQ(card.height(), 52);
}

TEST(ToastTest, Contract_ContentChangesCanShrinkBackToCompactSize)
{
    QWidget host;
    host.resize(800, 600);
    Toast toast(&host);
    toast.setMessage(QStringLiteral("Saved"));
    toast.setDuration(0);
    toast.setAnimationEnabled(false);
    ASSERT_TRUE(toast.present(&host));
    const QSize compact = toast.size();

    QAction retry(QStringLiteral("Try again"), &host);
    toast.setTitle(QStringLiteral("Upload interrupted"));
    toast.setMessage(QStringLiteral("Your connection was lost. Try again to resume the file."));
    toast.setAction(&retry);
    toast.setClosable(true);
    EXPECT_GT(toast.width(), compact.width());
    EXPECT_GT(toast.height(), compact.height());

    toast.setTitle(QString());
    toast.setAction(nullptr);
    toast.setClosable(false);
    toast.setMessage(QStringLiteral("Saved"));
    EXPECT_EQ(toast.size(), compact);
}

TEST(ToastTest, Contract_ContentResizeKeepsCornerVisibleDuringReflow)
{
    MotionPolicyScope policyScope(fluent::MotionPolicy::Mode::Full);
    QWidget host;
    host.resize(640, 480);
    host.show();
    QApplication::processEvents();

    for (const auto placement : {Toast::TopEnd, Toast::BottomEnd}) {
        Toast toast(&host);
        toast.setPlacement(placement);
        toast.setDuration(0);
        toast.setMessage(QStringLiteral("Saved"));
        toast.setClosable(true);
        toast.setAnimationEnabled(false);
        ASSERT_TRUE(toast.present(&host));
        toast.setAnimationEnabled(true);
        for (const QString& message :
             {QStringLiteral("Your changes are saved. This longer confirmation wraps onto several "
                             "lines while the close button stays inside the window."),
              QStringLiteral("Saved")}) {
            toast.setMessage(message);
            const QRect card = fluent::overlay::visibleCardGeometry(toast.geometry());
            EXPECT_TRUE(host.rect().contains(card));
            EXPECT_EQ(card.right(), host.rect().right() - toast.placementMargins().right());
            if (placement == Toast::TopEnd)
                EXPECT_EQ(card.top(), toast.placementMargins().top());
            else
                EXPECT_EQ(card.bottom(), host.rect().bottom() - toast.placementMargins().bottom());
        }
    }
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
    QObject::connect(toast, &Toast::isOpenChanged, &host, [toast](bool open) {
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
    const QPoint toastCenter = toast.mapTo(&host, toast.rect().center());
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
    QObject::connect(&toast, &Toast::actionChanged, &host,
                     [&actionChangedCount](QAction*) { ++actionChangedCount; });
    toast.setAction(&action);
    EXPECT_EQ(toast.action(), &action);
    EXPECT_EQ(action.parent(), &host);
    EXPECT_EQ(actionChangedCount, 1);
    EXPECT_FALSE(toast.testAttribute(Qt::WA_TransparentForMouseEvents));

    auto* button =
        toast.findChild<fluent::basicinput::Button*>(QStringLiteral("fluentToastAction"));
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text(), QStringLiteral("Retry"));
    EXPECT_EQ(button->accessibleName(), QStringLiteral("Retry"));

    int triggerCount = 0;
    int reasonCount = 0;
    Toast::DismissReason reason = Toast::Programmatic;
    QObject::connect(&action, &QAction::triggered, &host, [&triggerCount]() { ++triggerCount; });
    QObject::connect(&action, &QAction::triggered, &toast, &Toast::dismiss);
    QObject::connect(&toast, &Toast::dismissedWithReason, &host,
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
    EXPECT_TRUE(toast.testAttribute(Qt::WA_TransparentForMouseEvents));
}

TEST(ToastTest, Contract_ActionReusesFluentButtonWithLocalNeutralStyling)
{
    for (const auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        SCOPED_TRACE(theme == fluent::FluentElement::Light ? "Light" : "Dark");
        QWidget host;
        host.setProperty(fluent::overlay::themeOverridePropertyName(), int(theme));
        fluent::basicinput::Button sibling(&host);
        const QColor siblingFill = sibling.themeColorsRef().controlDefault;
        Toast toast(&host);
        QAction retry(QStringLiteral("Try again"), &host);
        toast.setMessage(QStringLiteral("Upload interrupted"));
        toast.setAction(&retry);
        toast.onThemeUpdated();

        auto* button =
            toast.findChild<fluent::basicinput::Button*>(QStringLiteral("fluentToastAction"));
        ASSERT_NE(button, nullptr);
        EXPECT_EQ(button->fluentStyle(), fluent::basicinput::Button::Standard);
        EXPECT_FALSE(button->themeOverrides().isEmpty());
        EXPECT_EQ(button->fontRole(), Typography::FontRole::BodyStrong);
        EXPECT_TRUE(button->hasFocusVisual());
        EXPECT_EQ(button->themeColorsRef().textPrimary, toast.themeColorsRef().textPrimary);
        EXPECT_TRUE(sibling.themeOverrides().isEmpty());
        EXPECT_EQ(sibling.themeColorsRef().controlDefault, siblingFill);
    }
}

TEST(ToastTest, Contract_CloseButtonIsOptionalAndReportsOneDismissal)
{
    QWidget host;
    host.resize(640, 480);
    host.show();
    Toast toast(&host);
    toast.setMessage(QStringLiteral("Changes saved"));
    toast.setDuration(0);
    toast.setAnimationEnabled(false);

    QSignalSpy closableSpy(&toast, &Toast::closableChanged);
    QSignalSpy reasonSpy(&toast, &Toast::dismissedWithReason);
    toast.setClosable(true);
    toast.setClosable(true);
    EXPECT_EQ(closableSpy.count(), 1);
    EXPECT_FALSE(toast.testAttribute(Qt::WA_TransparentForMouseEvents));
    ASSERT_TRUE(toast.present(&host));

    auto* close = toast.findChild<fluent::basicinput::Button*>(QStringLiteral("fluentToastClose"));
    ASSERT_NE(close, nullptr);
    EXPECT_TRUE(close->isVisible());
    EXPECT_EQ(close->size(), QSize(24, 24));
    EXPECT_EQ(fluent::overlay::visibleCardGeometry(toast.geometry()).height(), 52);
    const auto expectTopCorner = [&toast, close]() {
        const QRect card = fluent::overlay::visibleCardRect(toast.rect());
        const QRect button(close->mapTo(&toast, QPoint()), close->size());
        EXPECT_EQ(button.top() - card.top(), 8);
        EXPECT_EQ(card.right() - button.right(), 8);
    };
    expectTopCorner();
    toast.setMessage(
        QStringLiteral("Your changes are saved. This longer confirmation wraps "
                       "onto several lines while the close button stays in its corner."));
    QApplication::processEvents();
    EXPECT_GT(fluent::overlay::visibleCardGeometry(toast.geometry()).height(), 52);
    expectTopCorner();
    toast.setMessage(QStringLiteral("Changes saved"));
    QApplication::processEvents();
    EXPECT_EQ(fluent::overlay::visibleCardGeometry(toast.geometry()).height(), 52);
    expectTopCorner();
    EXPECT_FALSE(close->accessibleName().isEmpty());
    close->setFocus(Qt::TabFocusReason);
    QTest::keyClick(close, Qt::Key_Space);
    ASSERT_EQ(reasonSpy.count(), 1);
    EXPECT_EQ(reasonSpy.at(0).at(0).value<Toast::DismissReason>(), Toast::CloseButton);
    EXPECT_FALSE(toast.isOpen());
    toast.dismiss();
    EXPECT_EQ(reasonSpy.count(), 1);

    toast.setClosable(false);
    EXPECT_TRUE(toast.testAttribute(Qt::WA_TransparentForMouseEvents));
}

TEST(ToastTest, Contract_NarrowHostWrapsFullTextWithActionBelow)
{
    QWidget host;
    host.resize(320, 640);
    host.show();
    QAction action(QStringLiteral("Try again"), &host);
    Toast toast(&host);
    toast.setTitle(QStringLiteral("The connection was interrupted while uploading your files"));
    toast.setMessage(
        QStringLiteral("Your connection was lost. Try again to resume product-shot.png."));
    toast.setClosable(true);
    toast.setAction(&action);
    toast.setDuration(0);
    toast.setAnimationEnabled(false);
    ASSERT_TRUE(toast.present(&host));
    QApplication::processEvents();

    auto* title = toast.findChild<fluent::textfields::Label*>(QStringLiteral("fluentToastTitle"));
    auto* message =
        toast.findChild<fluent::textfields::Label*>(QStringLiteral("fluentToastMessage"));
    auto* button =
        toast.findChild<fluent::basicinput::Button*>(QStringLiteral("fluentToastAction"));
    auto* close = toast.findChild<fluent::basicinput::Button*>(QStringLiteral("fluentToastClose"));
    ASSERT_NE(title, nullptr);
    ASSERT_NE(message, nullptr);
    ASSERT_NE(button, nullptr);
    ASSERT_NE(close, nullptr);
    const auto geometryInToast = [&toast](const QWidget* widget) {
        return QRect(widget->mapTo(&toast, QPoint()), widget->size());
    };
    EXPECT_EQ(title->text(), toast.title());
    EXPECT_EQ(message->text(), toast.message());
    EXPECT_EQ(title->fluentTypography(), Typography::FontRole::BodyStrong);
    EXPECT_EQ(message->fluentTypography(), Typography::FontRole::Body);
    EXPECT_EQ(message->textColorRole(), fluent::textfields::Label::TextColorRole::Secondary);
    EXPECT_TRUE(title->wordWrap());
    EXPECT_TRUE(message->wordWrap());
    EXPECT_FALSE(title->isTextElided());
    EXPECT_FALSE(message->isTextElided());
    EXPECT_GE(title->height(), title->heightForWidth(title->width()));
    EXPECT_GE(message->height(), message->heightForWidth(message->width()));
    EXPECT_EQ(button->height(), 32);
    EXPECT_EQ(button->text(), action.text());
    EXPECT_GE(button->width(), button->sizeHint().width());
    EXPECT_GE(geometryInToast(button).top() - geometryInToast(message).bottom() - 1, 8);
    EXPECT_EQ(geometryInToast(button).left(), geometryInToast(message).left());
    EXPECT_LT(geometryInToast(message).right(), geometryInToast(close).left());
    const QRect localCard = fluent::overlay::visibleCardRect(toast.rect());
    EXPECT_EQ(geometryInToast(close).top() - localCard.top(), 8);
    EXPECT_EQ(localCard.right() - geometryInToast(close).right(), 8);
    const QRect card = fluent::overlay::visibleCardGeometry(toast.geometry());
    EXPECT_EQ(card.width(), 288);
    EXPECT_GE(card.left(), 16);
    EXPECT_LE(card.right(), host.width() - 17);

    host.resize(800, 640);
    QApplication::processEvents();
    EXPECT_LE(fluent::overlay::visibleCardGeometry(toast.geometry()).width(), 380);
    EXPECT_GT(fluent::overlay::visibleCardGeometry(toast.geometry()).width(), card.width());
    EXPECT_EQ(title->text(), toast.title());

    host.setLayoutDirection(Qt::RightToLeft);
    QApplication::processEvents();
    const QRect rtlCard = fluent::overlay::visibleCardRect(toast.rect());
    EXPECT_EQ(geometryInToast(close).top() - rtlCard.top(), 8);
    EXPECT_EQ(geometryInToast(close).left() - rtlCard.left(), 8);
    EXPECT_LT(geometryInToast(close).right(), geometryInToast(message).left());
    EXPECT_EQ(geometryInToast(button).right(), geometryInToast(message).right());
    EXPECT_GE(geometryInToast(button).top() - geometryInToast(message).bottom() - 1, 8);
}

TEST(ToastTest, Contract_NarrowHostKeepsLongActionCaptionReadable)
{
    QWidget host;
    host.resize(320, 640);
    host.show();
    QAction action(QStringLiteral("Review connection settings and resume the upload"), &host);
    Toast toast(&host);
    toast.setTitle(QStringLiteral("Upload paused"));
    toast.setMessage(QStringLiteral("Check your connection before continuing."));
    toast.setClosable(true);
    toast.setAction(&action);
    toast.setDuration(0);
    toast.setAnimationEnabled(false);
    ASSERT_TRUE(toast.present(&host));
    QApplication::processEvents();

    auto* message =
        toast.findChild<fluent::textfields::Label*>(QStringLiteral("fluentToastMessage"));
    auto* button =
        toast.findChild<fluent::basicinput::Button*>(QStringLiteral("fluentToastAction"));
    ASSERT_NE(message, nullptr);
    ASSERT_NE(button, nullptr);
    QString displayedCaption = button->text();
    displayedCaption.replace(QLatin1Char('\n'), QLatin1Char(' '));
    EXPECT_EQ(displayedCaption, action.text());
    EXPECT_EQ(button->accessibleName(), action.text());
    const auto lines = button->text().split(QLatin1Char('\n'));
    ASSERT_GT(lines.size(), 1);
    for (const QString& line : lines)
        EXPECT_LE(button->fontMetrics().horizontalAdvance(line), button->width());
    EXPECT_GT(button->height(), 32);
    EXPECT_GE(button->height(), button->fontMetrics().lineSpacing() * lines.size());

    const QRect buttonRect(button->mapTo(&toast, QPoint()), button->size());
    const QRect messageRect(message->mapTo(&toast, QPoint()), message->size());
    EXPECT_GT(buttonRect.top(), messageRect.bottom());
    EXPECT_TRUE(fluent::overlay::visibleCardRect(toast.rect()).contains(buttonRect));

    host.resize(800, 640);
    QApplication::processEvents();
    displayedCaption = button->text();
    displayedCaption.replace(QLatin1Char('\n'), QLatin1Char(' '));
    EXPECT_EQ(displayedCaption, action.text());
    EXPECT_LE(fluent::overlay::visibleCardGeometry(toast.geometry()).width(), 380);
}

TEST(ToastTest, Contract_StackReflowsContinuouslyAndLocalDisableSettlesPosition)
{
    MotionPolicyScope policyScope(fluent::MotionPolicy::Mode::Full);
    QWidget host;
    host.resize(640, 480);
    host.show();
    Toast first(&host);
    Toast second(&host);
    for (Toast* toast : {&first, &second}) {
        toast->setMessage(QStringLiteral("Changes saved"));
        toast->setDuration(0);
        toast->setAnimationEnabled(false);
        ASSERT_TRUE(toast->present(&host));
    }
    second.setAnimationEnabled(true);
    const QPoint before = second.pos();
    first.dismiss();
    EXPECT_EQ(second.pos(), before);
    auto* animation =
        second.findChild<QVariantAnimation*>(QStringLiteral("fluentToastPositionAnimation"));
    ASSERT_NE(animation, nullptr);
    ASSERT_EQ(animation->state(), QAbstractAnimation::Running);
    animation->setCurrentTime(animation->duration() / 2);
    EXPECT_LT(second.y(), before.y());
    EXPECT_GT(fluent::overlay::visibleCardGeometry(second.geometry()).top(), 16);
    second.setAnimationEnabled(false);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
    EXPECT_EQ(fluent::overlay::visibleCardGeometry(second.geometry()).top(), 16);
}

TEST(ToastTest, Contract_ReversingDismissalKeepsPresentationContinuous)
{
    MotionPolicyScope policyScope(fluent::MotionPolicy::Mode::Full);
    QWidget host;
    host.resize(640, 480);
    host.show();
    Toast toast(&host);
    toast.setMessage(QStringLiteral("Changes saved"));
    toast.setDuration(0);
    QSignalSpy dismissedSpy(&toast, &Toast::dismissed);
    ASSERT_TRUE(toast.present(&host));
    auto* animation =
        toast.findChild<QPropertyAnimation*>(QStringLiteral("fluentToastPresentationAnimation"));
    ASSERT_NE(animation, nullptr);
    animation->setCurrentTime(animation->duration() / 2);
    const qreal visibleProgress = toast.toastProgress();
    const QPoint visiblePosition = toast.pos();
    toast.dismiss();
    EXPECT_DOUBLE_EQ(toast.toastProgress(), visibleProgress);
    EXPECT_EQ(toast.pos(), visiblePosition);
    animation->setCurrentTime(animation->duration() / 2);
    const qreal dismissProgress = toast.toastProgress();
    const QPoint dismissPosition = toast.pos();
    EXPECT_EQ(dismissPosition, visiblePosition);
    ASSERT_TRUE(toast.present(&host));
    EXPECT_DOUBLE_EQ(toast.toastProgress(), dismissProgress);
    EXPECT_EQ(toast.pos(), dismissPosition);
    animation->setCurrentTime(animation->duration());
    EXPECT_DOUBLE_EQ(toast.toastProgress(), 1.0);
    EXPECT_EQ(dismissedSpy.count(), 0);
    auto* opacity = qobject_cast<QGraphicsOpacityEffect*>(toast.graphicsEffect());
    ASSERT_NE(opacity, nullptr);
    EXPECT_FALSE(opacity->isEnabled());
    toast.setAnimationEnabled(false);
    toast.dismiss();
    EXPECT_EQ(dismissedSpy.count(), 1);
}

TEST(ToastTest, Contract_ReducedMotionFadesWithoutTranslationAndHiddenMotionSettles)
{
    MotionPolicyScope policyScope(fluent::MotionPolicy::Mode::Reduced);
    QWidget host;
    host.resize(640, 480);
    host.show();
    Toast toast(&host);
    toast.setMessage(QStringLiteral("Changes saved"));
    toast.setDuration(0);
    ASSERT_TRUE(toast.present(&host));
    auto* animation =
        toast.findChild<QPropertyAnimation*>(QStringLiteral("fluentToastPresentationAnimation"));
    ASSERT_NE(animation, nullptr);
    EXPECT_LE(animation->duration(), 50);
    EXPECT_EQ(fluent::overlay::visibleCardGeometry(toast.geometry()).top(), 16);
    host.hide();
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
    EXPECT_DOUBLE_EQ(toast.toastProgress(), 1.0);
    host.show();
    EXPECT_TRUE(toast.isVisible());
    EXPECT_EQ(fluent::overlay::visibleCardGeometry(toast.geometry()).top(), 16);
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
    ASSERT_FALSE(toast.testAttribute(Qt::WA_TransparentForMouseEvents));
    ASSERT_TRUE(toast.present(&host));

    QTest::qWait(20);
    FLUENT_MAKE_ENTER_EVENT(enter, 4, 4);
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
    QObject::connect(&toast, &Toast::dismissedWithReason, &host,
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
    const QRect topStart = fluent::overlay::visibleCardGeometry(toast.geometry());
    EXPECT_EQ(topStart.top(), surface.top());
    EXPECT_EQ(topStart.left(), surface.left());
}

TEST(ToastTest, Contract_SeverityUsesFontIcon)
{
    Toast toast;
    toast.setSeverity(Toast::Success);
    auto* badge = toast.findChild<fluent::layout::Card*>(QStringLiteral("fluentToastStatusBadge"));
    ASSERT_NE(badge, nullptr);
    EXPECT_EQ(badge->size(), QSize(28, 28));
    auto* icon = badge->findChild<fluent::FontIcon*>(QStringLiteral("fluentToastIcon"));
    ASSERT_NE(icon, nullptr);
    EXPECT_EQ(icon->size(), QSize(16, 16));
    const QString successGlyph =
        Typography::Icons::glyph(QStringLiteral("ic_fluent_checkmark_circle_16_regular"));
    ASSERT_FALSE(successGlyph.isEmpty());
    EXPECT_EQ(icon->glyph(), successGlyph);

    toast.setSeverity(Toast::Error);
    EXPECT_EQ(icon->glyph(), Typography::Icons::ErrorIcon);
}

TEST(ToastTest, Contract_ManagedToastsStackUntilMaximumVisible)
{
    ScopedMaximumVisible scoped(2);
    QWidget host;
    host.resize(640, 480);

    QPointer<Toast> first =
        Toast::showToast(&host, QStringLiteral("First"), Toast::Informational, 0);
    QPointer<Toast> second = Toast::showToast(&host, QStringLiteral("Second"), Toast::Success, 0);
    ASSERT_FALSE(first.isNull());
    ASSERT_FALSE(second.isNull());
    EXPECT_TRUE(first->isOpen());
    EXPECT_TRUE(second->isOpen());

    const QRect firstCard = fluent::overlay::visibleCardGeometry(first->geometry());
    const QRect secondCard = fluent::overlay::visibleCardGeometry(second->geometry());
    EXPECT_LT(firstCard.top(), secondCard.top());

    int firstDismissCount = 0;
    Toast::DismissReason firstReason = Toast::Programmatic;
    QObject::connect(first.data(), &Toast::dismissedWithReason, &host,
                     [&firstDismissCount, &firstReason](Toast::DismissReason reason) {
                         ++firstDismissCount;
                         firstReason = reason;
                     });

    QPointer<Toast> third = Toast::showToast(&host, QStringLiteral("Third"), Toast::Warning, 0);
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

    QPointer<Toast> first =
        Toast::showOrUpdateToast(&firstHost, QStringLiteral("sync"), QStringLiteral("Uploading"),
                                 Toast::Informational, 0, Toast::TopEnd);
    ASSERT_FALSE(first.isNull());
    EXPECT_EQ(first->updateKey(), QStringLiteral("sync"));

    QSignalSpy updatedSpy(first.data(), &Toast::updated);
    Toast* updated = Toast::showOrUpdateToast(&firstHost, QStringLiteral("sync"),
                                              QStringLiteral("Upload complete"), Toast::Success, 0,
                                              Toast::TopEnd);
    ASSERT_EQ(updated, first.data());
    EXPECT_EQ(updatedSpy.count(), 1);
    EXPECT_EQ(first->message(), QStringLiteral("Upload complete"));
    EXPECT_EQ(first->severity(), Toast::Success);

    QPointer<Toast> otherPlacement = Toast::showOrUpdateToast(&firstHost, QStringLiteral("sync"),
                                                              QStringLiteral("Bottom status"),
                                                              Toast::Warning, 0, Toast::BottomEnd);
    QPointer<Toast> otherHost = Toast::showOrUpdateToast(&secondHost, QStringLiteral("sync"),
                                                         QStringLiteral("Other window"),
                                                         Toast::Informational, 0, Toast::TopEnd);
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

    QPointer<Toast> topA =
        Toast::showToast(&host, QStringLiteral("Top A"), Toast::Informational, 0, Toast::TopEnd);
    QPointer<Toast> topB =
        Toast::showToast(&host, QStringLiteral("Top B"), Toast::Success, 0, Toast::TopEnd);
    QPointer<Toast> bottomA =
        Toast::showToast(&host, QStringLiteral("Bottom A"), Toast::Warning, 0, Toast::BottomStart);
    QPointer<Toast> bottomB =
        Toast::showToast(&host, QStringLiteral("Bottom B"), Toast::Error, 0, Toast::BottomStart);
    ASSERT_FALSE(topA.isNull());
    ASSERT_FALSE(topB.isNull());
    ASSERT_FALSE(bottomA.isNull());
    ASSERT_FALSE(bottomB.isNull());

    const QRect surface = fluent::overlay::overlaySurfaceRect(&host);
    const QRect topACard = fluent::overlay::visibleCardGeometry(topA->geometry());
    const QRect topBCard = fluent::overlay::visibleCardGeometry(topB->geometry());
    const QRect bottomACard = fluent::overlay::visibleCardGeometry(bottomA->geometry());
    const QRect bottomBCard = fluent::overlay::visibleCardGeometry(bottomB->geometry());

    EXPECT_EQ(topACard.right(), surface.right() - 16);
    EXPECT_EQ(topBCard.right(), surface.right() - 16);
    EXPECT_LT(topACard.top(), topBCard.top());

    EXPECT_EQ(bottomACard.left(), surface.left() + 16);
    EXPECT_EQ(bottomBCard.left(), surface.left() + 16);
    EXPECT_GT(bottomACard.bottom(), bottomBCard.bottom());
}
