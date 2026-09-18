#include "components/basicinput/FileDropZone.h"

#include <QAccessible>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QVariantAnimation>
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

#include "components/basicinput/Button.h"
#include "components/foundation/MotionPolicy.h"
#include "components/textfields/Label.h"

using fluent::basicinput::Button;
using fluent::basicinput::FileDropZone;
using fluent::textfields::Label;

static_assert(std::is_base_of<QWidget, FileDropZone>::value, "FileDropZone is a QWidget");
static_assert(std::is_base_of<fluent::FluentElement, FileDropZone>::value,
              "FileDropZone exposes Fluent tokens");
static_assert(std::is_base_of<fluent::QMLPlus, FileDropZone>::value,
              "FileDropZone supports Fluent composition");

namespace {
class FileDropZoneTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_previousMotion = fluent::MotionPolicy::instance().mode();
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    }

    void TearDown() override { fluent::MotionPolicy::instance().setMode(m_previousMotion); }

    static void show(FileDropZone& zone, int width = 640)
    {
        zone.resize(width, zone.heightForWidth(width));
        zone.show();
        QApplication::processEvents();
    }

    static bool enter(FileDropZone& zone, const QMimeData& mime,
                      Qt::DropActions actions = Qt::CopyAction | Qt::MoveAction)
    {
        QDragEnterEvent event(QPoint(40, 40), actions, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&zone, &event);
        return event.isAccepted();
    }

    static void leave(FileDropZone& zone)
    {
        QDragLeaveEvent event;
        QApplication::sendEvent(&zone, &event);
    }

    static QVariantAnimation* transition(FileDropZone& zone)
    {
        return zone.findChild<QVariantAnimation*>(QStringLiteral("FileDropZoneStateTransition"));
    }

    static Button* browse(FileDropZone& zone)
    {
        return zone.findChild<Button*>(QStringLiteral("FileDropZoneBrowse"));
    }

    fluent::MotionPolicy::Mode m_previousMotion = fluent::MotionPolicy::Mode::Full;
};
} // namespace

TEST_F(FileDropZoneTest, Contract_PropertyChangesAreNoOpsWhenUnchanged)
{
    FileDropZone zone;
    QSignalSpy title(&zone, &FileDropZone::titleChanged);
    QSignalSpy description(&zone, &FileDropZone::descriptionChanged);
    QSignalSpy hint(&zone, &FileDropZone::hintTextChanged);
    QSignalSpy browseText(&zone, &FileDropZone::browseTextChanged);
    QSignalSpy error(&zone, &FileDropZone::errorMessageChanged);
    zone.setTitle(QStringLiteral("Add references"));
    zone.setDescription(QStringLiteral("Choose local reference files"));
    zone.setHintText(QStringLiteral("PDF or PNG"));
    zone.setBrowseText(QStringLiteral("Choose references"));
    zone.setErrorMessage(QStringLiteral("Choose a smaller file"));
    zone.setTitle(zone.title());
    zone.setDescription(zone.description());
    zone.setHintText(zone.hintText());
    zone.setBrowseText(zone.browseText());
    zone.setErrorMessage(zone.errorMessage());
    EXPECT_EQ(title.count(), 1);
    EXPECT_EQ(description.count(), 1);
    EXPECT_EQ(hint.count(), 1);
    EXPECT_EQ(browseText.count(), 1);
    EXPECT_EQ(error.count(), 1);
    zone.clearError();
    zone.clearError();
    EXPECT_EQ(error.count(), 2);
    EXPECT_TRUE(zone.errorMessage().isEmpty());
}

TEST_F(FileDropZoneTest, Contract_CopyDropPreservesUrlsAndDoesNotInspectFiles)
{
    FileDropZone zone;
    show(zone);
    const QList<QUrl> urls{QUrl::fromLocalFile(QStringLiteral("/not-on-disk/report.pdf")),
                           QUrl::fromLocalFile(QStringLiteral("/not-on-disk/设计稿.png"))};
    QMimeData mime;
    mime.setUrls(urls);
    QList<QUrl> delivered;
    bool activeDuringDelivery = true;
    int deliveries = 0;
    QObject::connect(&zone, &FileDropZone::filesDropped, &zone, [&](const QList<QUrl>& values) {
        delivered = values;
        activeDuringDelivery = zone.isDragActive();
        ++deliveries;
    });
    QSignalSpy active(&zone, &FileDropZone::dragActiveChanged);
    ASSERT_TRUE(enter(zone, mime));
    EXPECT_TRUE(zone.isDragActive());
    QDragMoveEvent move(QPoint(80, 100), Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton,
                        Qt::NoModifier);
    QApplication::sendEvent(&zone, &move);
    EXPECT_TRUE(move.isAccepted());
    EXPECT_EQ(move.dropAction(), Qt::CopyAction);
    QDropEvent drop(QPointF(80, 100), Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton,
                    Qt::NoModifier);
    QApplication::sendEvent(&zone, &drop);
    EXPECT_TRUE(drop.isAccepted());
    EXPECT_EQ(drop.dropAction(), Qt::CopyAction);
    EXPECT_EQ(deliveries, 1);
    EXPECT_EQ(delivered, urls);
    EXPECT_FALSE(activeDuringDelivery);
    EXPECT_FALSE(zone.isDragActive());
    ASSERT_EQ(active.count(), 2);
    EXPECT_TRUE(active.at(0).at(0).toBool());
    EXPECT_FALSE(active.at(1).at(0).toBool());
}

TEST_F(FileDropZoneTest, Contract_RejectsRemoteMixedTextAndMoveOnlyPayloads)
{
    FileDropZone zone;
    show(zone);
    QSignalSpy active(&zone, &FileDropZone::dragActiveChanged);
    QSignalSpy dropped(&zone, &FileDropZone::filesDropped);
    QMimeData mime;
    mime.setText(QStringLiteral("file:///not-a-file-drop.pdf"));
    EXPECT_FALSE(enter(zone, mime));
    mime.setUrls({QUrl(QStringLiteral("https://example.com/report.pdf"))});
    EXPECT_FALSE(enter(zone, mime));
    mime.setUrls({QUrl::fromLocalFile(QStringLiteral("/local.pdf")),
                  QUrl(QStringLiteral("https://example.com/report.pdf"))});
    EXPECT_FALSE(enter(zone, mime));
    mime.setUrls({QUrl::fromLocalFile(QStringLiteral("/local.pdf"))});
    EXPECT_FALSE(enter(zone, mime, Qt::MoveAction));
    EXPECT_FALSE(zone.isDragActive());
    EXPECT_EQ(active.count(), 0);
    EXPECT_EQ(dropped.count(), 0);
}

TEST_F(FileDropZoneTest, Contract_CancelRestoresPresentationWithoutClearingApplicationError)
{
    FileDropZone zone;
    zone.setErrorMessage(QStringLiteral("This file exceeds the workspace limit."));
    show(zone);
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(QStringLiteral("/retry.pdf"))});
    QSignalSpy dropped(&zone, &FileDropZone::filesDropped);
    QSignalSpy active(&zone, &FileDropZone::dragActiveChanged);
    ASSERT_TRUE(enter(zone, mime));
    EXPECT_TRUE(zone.isDragActive());
    leave(zone);
    EXPECT_FALSE(zone.isDragActive());
    EXPECT_EQ(dropped.count(), 0);
    EXPECT_EQ(active.count(), 2);
    EXPECT_EQ(zone.errorMessage(), QStringLiteral("This file exceeds the workspace limit."));
    auto* description = zone.findChild<Label*>(QStringLiteral("FileDropZoneDescription"));
    ASSERT_NE(description, nullptr);
    EXPECT_EQ(description->text(), zone.errorMessage());
}

TEST_F(FileDropZoneTest, Contract_DisableHideAndDeactivateEndDragAndStopMotion)
{
    FileDropZone zone;
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(QStringLiteral("/item.pdf"))});
    show(zone);
    ASSERT_TRUE(enter(zone, mime));
    zone.setEnabled(false);
    EXPECT_FALSE(zone.isDragActive());
    EXPECT_EQ(transition(zone)->state(), QAbstractAnimation::Stopped);
    EXPECT_FALSE(enter(zone, mime));
    zone.setEnabled(true);
    ASSERT_TRUE(enter(zone, mime));
    zone.hide();
    EXPECT_FALSE(zone.isDragActive());
    EXPECT_EQ(transition(zone)->state(), QAbstractAnimation::Stopped);
    show(zone);
    ASSERT_TRUE(enter(zone, mime));
    QEvent deactivate(QEvent::WindowDeactivate);
    QApplication::sendEvent(&zone, &deactivate);
    EXPECT_FALSE(zone.isDragActive());
    EXPECT_EQ(transition(zone)->state(), QAbstractAnimation::Stopped);
}

TEST_F(FileDropZoneTest, Contract_BrowseUsesNativeButtonAndKeyboardExactlyOnce)
{
    FileDropZone zone;
    show(zone);
    Button* button = browse(zone);
    ASSERT_NE(button, nullptr);
    QSignalSpy requested(&zone, &FileDropZone::browseRequested);
    QTest::mouseClick(button, Qt::LeftButton);
    EXPECT_EQ(requested.count(), 1);
    QTest::keyClick(button, Qt::Key_Space);
    EXPECT_EQ(requested.count(), 2);
    QTest::keyClick(button, Qt::Key_Return);
    EXPECT_EQ(requested.count(), 3);
    QTest::keyClick(button, Qt::Key_Enter);
    EXPECT_EQ(requested.count(), 4);
    zone.setEnabled(false);
    QTest::mouseClick(button, Qt::LeftButton);
    QTest::keyClick(button, Qt::Key_Return);
    EXPECT_EQ(requested.count(), 4);
    EXPECT_EQ(zone.focusProxy(), button);
    EXPECT_EQ(zone.findChildren<Button*>().size(), 1);
}

TEST_F(FileDropZoneTest, Contract_DropStateCallbackMayDeleteOrDisableTheSurface)
{
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(QStringLiteral("/item.pdf"))});
    QPointer<FileDropZone> deletedZone = new FileDropZone;
    show(*deletedZone);
    ASSERT_TRUE(enter(*deletedZone, mime));
    int deliveries = 0;
    QObject::connect(deletedZone, &FileDropZone::filesDropped, [&] { ++deliveries; });
    QObject::connect(deletedZone, &FileDropZone::dragActiveChanged, [deletedZone](bool active) {
        if (!active)
            delete deletedZone.data();
    });
    QDropEvent deletionDrop(QPointF(40, 40), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(deletedZone, &deletionDrop);
    EXPECT_TRUE(deletedZone.isNull());
    EXPECT_EQ(deliveries, 0);
    EXPECT_FALSE(deletionDrop.isAccepted());

    FileDropZone disabledZone;
    show(disabledZone);
    ASSERT_TRUE(enter(disabledZone, mime));
    QObject::connect(&disabledZone, &FileDropZone::filesDropped, [&] { ++deliveries; });
    QObject::connect(&disabledZone, &FileDropZone::dragActiveChanged, &disabledZone,
                     [&](bool active) {
                         if (!active)
                             disabledZone.setEnabled(false);
                     });
    QDropEvent disabledDrop(QPointF(40, 40), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&disabledZone, &disabledDrop);
    EXPECT_FALSE(disabledDrop.isAccepted());
    EXPECT_EQ(deliveries, 0);
}

TEST_F(FileDropZoneTest, Contract_DeactivationCallbackMayDeleteTheSurface)
{
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(QStringLiteral("/item.pdf"))});
    QPointer<FileDropZone> zone = new FileDropZone;
    show(*zone);
    ASSERT_TRUE(enter(*zone, mime));
    QObject::connect(zone, &FileDropZone::dragActiveChanged, [zone](bool active) {
        if (!active)
            delete zone.data();
    });
    QEvent deactivate(QEvent::WindowDeactivate);
    QApplication::sendEvent(zone, &deactivate);
    EXPECT_TRUE(zone.isNull());
}

TEST_F(FileDropZoneTest, Contract_AccessibilityUsesNativeButtonAndPreservesCallerText)
{
    FileDropZone zone;
    zone.setTitle(QStringLiteral("Reference files"));
    zone.setDescription(QStringLiteral("Choose reference documents"));
    zone.setHintText(QStringLiteral("PDF or PNG"));
    EXPECT_EQ(zone.accessibleName(), zone.title());
    EXPECT_TRUE(zone.accessibleDescription().contains(zone.description()));
    EXPECT_TRUE(zone.accessibleDescription().contains(zone.hintText()));
    auto* buttonInterface = QAccessible::queryAccessibleInterface(browse(zone));
    ASSERT_NE(buttonInterface, nullptr);
    EXPECT_EQ(buttonInterface->role(), QAccessible::Button);
    EXPECT_EQ(buttonInterface->text(QAccessible::Name), zone.browseText());
    EXPECT_NE(buttonInterface->actionInterface(), nullptr);
    zone.setAccessibleName(QStringLiteral("Attachment entry"));
    zone.setAccessibleDescription(QStringLiteral("Workspace attachments"));
    zone.setTitle(QStringLiteral("A new title"));
    zone.setErrorMessage(QStringLiteral("Too large"));
    EXPECT_EQ(zone.accessibleName(), QStringLiteral("Attachment entry"));
    EXPECT_EQ(zone.accessibleDescription(), QStringLiteral("Workspace attachments"));
    zone.setEnabled(false);
    EXPECT_TRUE(buttonInterface->state().disabled);
}

TEST_F(FileDropZoneTest, Contract_LongFeedbackWrapsWithoutElisionAtNarrowWidth)
{
    FileDropZone zone;
    zone.setTitle(QStringLiteral("Add references to this workspace"));
    zone.setErrorMessage(QStringLiteral("The selected archive is larger than the limit for this "
                                        "workspace. Choose a smaller file to continue."));
    zone.setHintText(QStringLiteral("Your application checks each file before adding it."));
    const int wideHeight = zone.heightForWidth(640);
    const int narrowHeight = zone.heightForWidth(260);
    EXPECT_GT(narrowHeight, wideHeight);
    show(zone, 260);
    for (Label* label : zone.findChildren<Label*>()) {
        if (label->isHidden())
            continue;
        EXPECT_TRUE(label->wordWrap());
        EXPECT_EQ(label->textElideMode(), Qt::ElideNone);
        EXPECT_EQ(label->textFormat(), Qt::PlainText);
        EXPECT_TRUE(zone.rect().contains(label->geometry()));
        EXPECT_GE(label->height(), label->heightForWidth(label->width()));
    }
    EXPECT_TRUE(zone.rect().contains(browse(zone)->geometry()));
}

TEST_F(FileDropZoneTest, Contract_ErrorTextUsesCriticalTokenAcrossThemes)
{
    FileDropZone zone;
    zone.setErrorMessage(QStringLiteral("Choose a smaller file."));
    auto* description = zone.findChild<Label*>(QStringLiteral("FileDropZoneDescription"));
    ASSERT_NE(description, nullptr);
    for (const auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark,
                             fluent::FluentElement::HighContrast}) {
        SCOPED_TRACE(static_cast<int>(theme));
        zone.setProperty("fluentThemeOverride", static_cast<int>(theme));
        zone.onThemeUpdated();
        const QColor expected = zone.themeColorsRef().systemCritical;
        EXPECT_EQ(description->textColorRole(), Label::TextColorRole::Primary);
        EXPECT_EQ(description->themeColorsRef().textPrimary, expected);
        EXPECT_TRUE(description->styleSheet().contains(QStringLiteral("rgba(%1, %2, %3, %4)")
                                                           .arg(expected.red())
                                                           .arg(expected.green())
                                                           .arg(expected.blue())
                                                           .arg(expected.alpha())));
    }
}

TEST_F(FileDropZoneTest, Contract_ErrorTextPreservesCriticalOverrideAlphaAcrossThemes)
{
    FileDropZone zone;
    ASSERT_TRUE(
        zone.setThemeOverrides({{"light", QJsonObject{{"systemCritical", "#12345678"}}},
                                {"dark", QJsonObject{{"systemCritical", "#A1B2C3D4"}}},
                                {"contrast", QJsonObject{{"systemCritical", "#F1E2D3C4"}}}}));
    zone.setErrorMessage(QStringLiteral("Choose a smaller file."));
    auto* description = zone.findChild<Label*>(QStringLiteral("FileDropZoneDescription"));
    ASSERT_NE(description, nullptr);
    const std::pair<fluent::FluentElement::Theme, QColor> cases[]{
        {fluent::FluentElement::Light, QColor(0x12, 0x34, 0x56, 0x78)},
        {fluent::FluentElement::Dark, QColor(0xA1, 0xB2, 0xC3, 0xD4)},
        {fluent::FluentElement::HighContrast, QColor(0xF1, 0xE2, 0xD3, 0xC4)}};
    for (const auto& testCase : cases) {
        SCOPED_TRACE(static_cast<int>(testCase.first));
        zone.setProperty("fluentThemeOverride", static_cast<int>(testCase.first));
        zone.onThemeUpdated();
        EXPECT_EQ(zone.themeColorsRef().systemCritical, testCase.second);
        EXPECT_EQ(description->themeColorsRef().textPrimary, testCase.second);
        EXPECT_TRUE(description->styleSheet().contains(QStringLiteral("rgba(%1, %2, %3, %4)")
                                                           .arg(testCase.second.red())
                                                           .arg(testCase.second.green())
                                                           .arg(testCase.second.blue())
                                                           .arg(testCase.second.alpha())));
    }
    zone.clearError();
    EXPECT_EQ(description->textColorRole(), Label::TextColorRole::Secondary);
    EXPECT_TRUE(description->themeOverrides().isEmpty());
}

TEST_F(FileDropZoneTest, Contract_MotionIsFiniteInterruptibleAndDoesNotRelayoutContent)
{
    FileDropZone zone;
    show(zone);
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(QStringLiteral("/item.pdf"))});
    ASSERT_TRUE(enter(zone, mime));
    auto* animation = transition(zone);
    ASSERT_NE(animation, nullptr);
    ASSERT_EQ(animation->state(), QAbstractAnimation::Running);
    const QRect titleGeometry =
        zone.findChild<Label*>(QStringLiteral("FileDropZoneTitle"))->geometry();
    const QRect buttonGeometry = browse(zone)->geometry();
    animation->setCurrentTime(animation->duration() / 2);
    const qreal enteringOpacity = browse(zone)->contentOpacity();
    EXPECT_GT(enteringOpacity, 0.0);
    EXPECT_LT(enteringOpacity, 1.0);
    EXPECT_EQ(zone.findChild<Label*>(QStringLiteral("FileDropZoneTitle"))->geometry(),
              titleGeometry);
    EXPECT_EQ(browse(zone)->geometry(), buttonGeometry);
    leave(zone);
    EXPECT_NEAR(browse(zone)->contentOpacity(), enteringOpacity, 0.001);
    animation->setCurrentTime(animation->duration());
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
    EXPECT_DOUBLE_EQ(browse(zone)->contentOpacity(), 1.0);
}

TEST_F(FileDropZoneTest, Contract_MotionPolicyAndHighContrastSettleTransitions)
{
    FileDropZone zone;
    show(zone);
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(QStringLiteral("/item.pdf"))});
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Reduced);
    ASSERT_TRUE(enter(zone, mime));
    EXPECT_LE(transition(zone)->duration(), 50);
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    EXPECT_EQ(transition(zone)->state(), QAbstractAnimation::Stopped);
    EXPECT_DOUBLE_EQ(browse(zone)->contentOpacity(), 0.0);
    leave(zone);
    EXPECT_EQ(transition(zone)->state(), QAbstractAnimation::Stopped);
    EXPECT_DOUBLE_EQ(browse(zone)->contentOpacity(), 1.0);
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    zone.setProperty("fluentThemeOverride", int(fluent::FluentElement::HighContrast));
    zone.onThemeUpdated();
    ASSERT_TRUE(enter(zone, mime));
    EXPECT_EQ(transition(zone)->state(), QAbstractAnimation::Stopped);
    EXPECT_DOUBLE_EQ(browse(zone)->contentOpacity(), 0.0);
}
