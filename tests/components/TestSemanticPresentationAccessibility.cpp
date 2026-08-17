#include <gtest/gtest.h>

#include <QAccessible>
#include <QApplication>
#include <QDesktopServices>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>
#include <QVector>

#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/HyperlinkButton.h"
#include "components/status_info/InfoBar.h"
#include "components/status_info/Shimmer.h"

using fluent::basicinput::Button;
using fluent::basicinput::HyperlinkButton;
using fluent::status_info::InfoBar;
using fluent::status_info::Shimmer;

namespace {

const QString& dismissAction()
{
    static const QString action = QStringLiteral("dismiss");
    return action;
}

void showAndProcess(QWidget& widget, const QSize& size)
{
    widget.resize(size);
    widget.show();
    QApplication::processEvents();
}

#if QT_CONFIG(accessibility)

struct AccessibleEventRecord {
    QObject* object = nullptr;
    QAccessible::Event type = QAccessible::InvalidEvent;
    QAccessible::State changedState{};
    QString announcement;
    FluentAccessibleAnnouncementPoliteness politeness =
        FluentAccessibleAnnouncementPoliteness::Unspecified;
};

QVector<AccessibleEventRecord> g_accessibilityEvents;

void captureAccessibilityEvent(QAccessibleEvent* event)
{
    if (!event)
        return;
    AccessibleEventRecord record;
    record.object = event->object();
    record.type = event->type();
    if (event->type() == QAccessible::StateChanged) {
        record.changedState =
            static_cast<QAccessibleStateChangeEvent*>(event)
                ->changedStates();
    }
    if (event->type() == fluentAccessibleAnnouncementEventType()) {
        record.announcement =
            fluentAccessibleAnnouncementMessage(event);
        record.politeness =
            fluentAccessibleAnnouncementPoliteness(event);
    }
    g_accessibilityEvents.append(record);
}

class ScopedAccessibilityEventCapture {
public:
    enum class StateField { Traversed, Invisible, Animated, Busy };

    ScopedAccessibilityEventCapture()
        : m_previous(
              QAccessible::installUpdateHandler(captureAccessibilityEvent))
    {
        g_accessibilityEvents.clear();
    }

    ~ScopedAccessibilityEventCapture()
    {
        QAccessible::installUpdateHandler(m_previous);
        g_accessibilityEvents.clear();
    }

    int count(QObject* object, QAccessible::Event type) const
    {
        int result = 0;
        for (const AccessibleEventRecord& event : g_accessibilityEvents) {
            if (event.object == object && event.type == type)
                ++result;
        }
        return result;
    }

    int countState(QObject* object, StateField field) const
    {
        int result = 0;
        for (const AccessibleEventRecord& event : g_accessibilityEvents) {
            bool changed = false;
            switch (field) {
            case StateField::Traversed:
                changed = event.changedState.traversed;
                break;
            case StateField::Invisible:
                changed = event.changedState.invisible;
                break;
            case StateField::Animated:
                changed = event.changedState.animated;
                break;
            case StateField::Busy:
                changed = event.changedState.busy;
                break;
            }
            if (event.object == object
                && event.type == QAccessible::StateChanged
                && changed) {
                ++result;
            }
        }
        return result;
    }

    QVector<AccessibleEventRecord> announcements(QObject* object) const
    {
        QVector<AccessibleEventRecord> result;
        for (const AccessibleEventRecord& event : g_accessibilityEvents) {
            if (event.object == object
                && event.type == fluentAccessibleAnnouncementEventType()) {
                result.append(event);
            }
        }
        return result;
    }

    void clear() { g_accessibilityEvents.clear(); }

private:
    QAccessible::UpdateHandler m_previous = nullptr;
};

QAccessibleInterface* accessible(QWidget* widget)
{
    return widget ? QAccessible::queryAccessibleInterface(widget) : nullptr;
}

class UrlCapture final : public QObject {
    Q_OBJECT

public:
    QUrl lastUrl;
    int count = 0;

public slots:
    void handleUrl(const QUrl& url)
    {
        lastUrl = url;
        ++count;
    }
};

class ScopedUrlHandler {
public:
    explicit ScopedUrlHandler(UrlCapture* capture)
    {
        QDesktopServices::setUrlHandler(
            QStringLiteral("fluentqt-test"), capture, "handleUrl");
    }

    ~ScopedUrlHandler()
    {
        QDesktopServices::unsetUrlHandler(
            QStringLiteral("fluentqt-test"));
    }
};

#endif

} // namespace

TEST(SemanticPresentationAccessibilityTest, Contract_AccessibilityHyperlinkExposesLinkTargetActionAndVisitedState)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    UrlCapture capture;
    ScopedUrlHandler handler(&capture);
    HyperlinkButton link(
        QStringLiteral("&Documentation"),
        QUrl(QStringLiteral("fluentqt-test://docs/getting-started")));
    showAndProcess(link, QSize(180, 36));

    QAccessibleInterface* root = accessible(&link);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::Link);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Documentation"));
    EXPECT_EQ(root->text(QAccessible::Value),
              QStringLiteral("fluentqt-test://docs/getting-started"));
    EXPECT_TRUE(root->state().linked);
    EXPECT_FALSE(root->state().traversed);
    ASSERT_NE(root->hyperlinkInterface(), nullptr);
    EXPECT_EQ(root->hyperlinkInterface()->anchor(),
              QStringLiteral("Documentation"));
    EXPECT_EQ(root->hyperlinkInterface()->anchorTarget(),
              QStringLiteral("fluentqt-test://docs/getting-started"));
    EXPECT_EQ(root->hyperlinkInterface()->startIndex(), 0);
    EXPECT_EQ(root->hyperlinkInterface()->endIndex(), 13);
    EXPECT_TRUE(root->hyperlinkInterface()->isValid());

    QAccessibleActionInterface* actions = root->actionInterface();
    ASSERT_NE(actions, nullptr);
    EXPECT_EQ(actions->actionNames(),
              QStringList{QAccessibleActionInterface::pressAction()});
    EXPECT_EQ(actions->keyBindingsForAction(
                  QAccessibleActionInterface::pressAction()),
              (QStringList{QStringLiteral("Space"),
                           QStringLiteral("Enter")}));

    ScopedAccessibilityEventCapture events;
    actions->doAction(QAccessibleActionInterface::pressAction());
    EXPECT_EQ(capture.count, 1);
    EXPECT_EQ(capture.lastUrl,
              QUrl(QStringLiteral(
                  "fluentqt-test://docs/getting-started")));
    EXPECT_TRUE(root->state().traversed);
    EXPECT_EQ(events.countState(
                  &link,
                  ScopedAccessibilityEventCapture::StateField::Traversed),
              1);

    actions->doAction(QAccessibleActionInterface::pressAction());
    EXPECT_EQ(capture.count, 2);
    EXPECT_EQ(events.countState(
                  &link,
                  ScopedAccessibilityEventCapture::StateField::Traversed),
              1);

    events.clear();
    link.setUrl(QUrl(QStringLiteral("fluentqt-test://docs/api")));
    EXPECT_FALSE(root->state().traversed);
    EXPECT_EQ(root->hyperlinkInterface()->anchorTarget(),
              QStringLiteral("fluentqt-test://docs/api"));
    EXPECT_EQ(events.countState(
                  &link,
                  ScopedAccessibilityEventCapture::StateField::Traversed),
              1);
    EXPECT_EQ(events.count(&link, QAccessible::ValueChanged), 1);
    link.setUrl(QUrl(QStringLiteral("fluentqt-test://docs/api")));
    EXPECT_EQ(events.count(&link, QAccessible::ValueChanged), 1);

    link.setAccessibleName(QStringLiteral("Documentation center"));
    link.setText(QStringLiteral("Changed"));
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Documentation center"));
#endif
}

TEST(SemanticPresentationAccessibilityTest, Contract_AccessibilityInfoBarExposesNotificationContentAndDismiss)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    QWidget window;
    showAndProcess(window, QSize(720, 240));
    InfoBar bar(&window);
    bar.setTitle(QStringLiteral("Sync"));
    bar.setMessage(QStringLiteral("All changes saved."));
    bar.setGeometry(20, 20, 600, 64);
    bar.show();
    QApplication::processEvents();

    QAccessibleInterface* root = accessible(&bar);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::Notification);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Sync: All changes saved."));
    EXPECT_EQ(root->text(QAccessible::Description),
              QStringLiteral("Informational"));
    EXPECT_TRUE(root->state().active);
    EXPECT_FALSE(root->state().focusable);
    ASSERT_NE(root->actionInterface(), nullptr);
    EXPECT_EQ(root->actionInterface()->actionNames(),
              QStringList{dismissAction()});

    auto* closeButton =
        bar.findChild<Button*>(QStringLiteral("InfoBarCloseButton"));
    ASSERT_NE(closeButton, nullptr);
    EXPECT_EQ(closeButton->accessibleName(),
              QStringLiteral("Dismiss notification"));
    EXPECT_EQ(root->childCount(), 1);
    ASSERT_NE(root->child(0), nullptr);
    EXPECT_EQ(root->child(0)->object(), closeButton);

    QSignalSpy closedSpy(&bar, &InfoBar::closed);
    root->actionInterface()->doAction(dismissAction());
    EXPECT_FALSE(bar.isOpen());
    EXPECT_EQ(closedSpy.count(), 1);
    EXPECT_TRUE(root->state().invisible);
    EXPECT_TRUE(root->actionInterface()->actionNames().isEmpty());
#endif
}

TEST(SemanticPresentationAccessibilityTest, Contract_AccessibilityInfoBarUpdatesAnnounceSeverityAndSuppressNoOps)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    InfoBar bar;
    bar.setTitle(QStringLiteral("Connection"));
    bar.setMessage(QStringLiteral("Online"));
    showAndProcess(bar, bar.sizeHint());
    ASSERT_NE(accessible(&bar), nullptr);

    ScopedAccessibilityEventCapture events;
    bar.setMessage(QStringLiteral("Connection lost"));
    bar.setMessage(QStringLiteral("Connection lost"));
    EXPECT_EQ(events.count(&bar, QAccessible::NameChanged), 1);
    auto announcements = events.announcements(&bar);
    ASSERT_EQ(announcements.size(), 1);
    if (fluentAccessibleAnnouncementSupportsDetails()) {
        EXPECT_EQ(announcements.first().announcement,
                  QStringLiteral(
                      "Informational: Connection: Connection lost"));
        EXPECT_EQ(announcements.first().politeness,
                  FluentAccessibleAnnouncementPoliteness::Polite);
    }

    events.clear();
    bar.setSeverity(InfoBar::Error);
    bar.setSeverity(InfoBar::Error);
    EXPECT_EQ(accessible(&bar)->text(QAccessible::Description),
              QStringLiteral("Error"));
    EXPECT_EQ(events.count(&bar, QAccessible::DescriptionChanged), 1);
    announcements = events.announcements(&bar);
    ASSERT_EQ(announcements.size(), 1);
    if (fluentAccessibleAnnouncementSupportsDetails()) {
        EXPECT_EQ(announcements.first().announcement,
                  QStringLiteral("Error: Connection: Connection lost"));
        EXPECT_EQ(announcements.first().politeness,
                  FluentAccessibleAnnouncementPoliteness::Assertive);
    }

    events.clear();
    bar.setIsOpen(false);
    EXPECT_EQ(events.countState(
                  &bar,
                  ScopedAccessibilityEventCapture::StateField::Invisible),
              1);
    EXPECT_EQ(events.count(&bar, QAccessible::ActionChanged), 1);
    events.clear();
    bar.setMessage(QStringLiteral("Still offline"));
    EXPECT_TRUE(events.announcements(&bar).isEmpty());
    bar.setIsOpen(true);
    announcements = events.announcements(&bar);
    ASSERT_EQ(announcements.size(), 1);

    bar.setAccessibleName(QStringLiteral("Network status"));
    events.clear();
    bar.setTitle(QStringLiteral("Network"));
    EXPECT_EQ(accessible(&bar)->text(QAccessible::Name),
              QStringLiteral("Network status"));
    EXPECT_EQ(events.count(&bar, QAccessible::NameChanged), 0);
#endif
}

TEST(SemanticPresentationAccessibilityTest, Contract_AccessibilityInfoBarRetainsHostedActionAndTracksDismissAvailability)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    InfoBar bar;
    bar.setTitle(QStringLiteral("Update available"));
    auto* install = new Button(QStringLiteral("Install"));
    install->setAccessibleName(QStringLiteral("Install update"));
    bar.setActionWidget(install);
    showAndProcess(bar, bar.sizeHint());

    QAccessibleInterface* root = accessible(&bar);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->childCount(), 2);
    EXPECT_EQ(root->child(0)->object(), install);
    EXPECT_EQ(root->child(1)->object(),
              bar.findChild<Button*>(
                  QStringLiteral("InfoBarCloseButton")));

    ScopedAccessibilityEventCapture events;
    bar.setIsClosable(false);
    bar.setIsClosable(false);
    EXPECT_TRUE(root->actionInterface()->actionNames().isEmpty());
    EXPECT_EQ(root->childCount(), 1);
    EXPECT_EQ(root->child(0)->object(), install);
    EXPECT_EQ(events.count(&bar, QAccessible::ActionChanged), 1);
    EXPECT_EQ(events.count(&bar, QAccessible::ObjectReorder), 1);
#endif
}

TEST(SemanticPresentationAccessibilityTest, Contract_AccessibilityShimmerExposesBusyStateWithoutFrameAnnouncements)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    Shimmer shimmer;
    showAndProcess(shimmer, QSize(240, 72));

    QAccessibleInterface* root = accessible(&shimmer);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::Animation);
    EXPECT_EQ(root->text(QAccessible::Name), QStringLiteral("Loading"));
    EXPECT_EQ(root->text(QAccessible::Description),
              QStringLiteral("Content is loading"));
    EXPECT_TRUE(root->state().busy);
    EXPECT_TRUE(root->state().animated);
    EXPECT_FALSE(root->state().focusable);
    EXPECT_TRUE(root->actionInterface()->actionNames().isEmpty());

    ScopedAccessibilityEventCapture events;
    shimmer.setShimmerProgress(0.25);
    QTest::qWait(48);
    EXPECT_TRUE(g_accessibilityEvents.isEmpty());

    shimmer.setAnimationEnabled(false);
    shimmer.setAnimationEnabled(false);
    EXPECT_TRUE(root->state().busy);
    EXPECT_FALSE(root->state().animated);
    EXPECT_EQ(events.countState(
                  &shimmer,
                  ScopedAccessibilityEventCapture::StateField::Animated),
              1);

    events.clear();
    shimmer.setActive(false);
    shimmer.setActive(false);
    EXPECT_FALSE(root->state().busy);
    EXPECT_FALSE(root->state().animated);
    EXPECT_TRUE(root->state().invisible);
    EXPECT_EQ(events.countState(
                  &shimmer,
                  ScopedAccessibilityEventCapture::StateField::Busy),
              1);
    EXPECT_EQ(events.countState(
                  &shimmer,
                  ScopedAccessibilityEventCapture::StateField::Invisible),
              1);

    shimmer.setAccessibleName(QStringLiteral("Loading account"));
    shimmer.setActive(true);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Loading account"));
    EXPECT_TRUE(root->state().busy);
#endif
}

#include "TestSemanticPresentationAccessibility.moc"
