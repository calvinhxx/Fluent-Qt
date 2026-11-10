#include <gtest/gtest.h>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include "view/widgets/GalleryFeedbackTip.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/FontIcon.h"
#include "components/textfields/Label.h"
#include <QPropertyAnimation>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <FluentQt/Diagnostics.h>
#include "components/foundation/FluentElement.h"
#include "platform/GalleryPlatform.h"
#include "support/logging/Log.h"
#include "view/widgets/GalleryFeedbackButton.h"
#include "view/shell/GalleryNavigationPane.h"
#include "view/shell/GalleryTopNavigationPane.h"
#include "viewmodel/GalleryDiagnostics.h"
#include "view/shell/GalleryWindow.h"
#include "viewmodel/GallerySettings.h"

namespace {
class FeedbackUrlReceiver : public QObject {
    Q_OBJECT
public:
    FeedbackUrlReceiver() { QDesktopServices::setUrlHandler("https", this, "record"); }
    ~FeedbackUrlReceiver() override { QDesktopServices::unsetUrlHandler("https"); }
    QList<QUrl> opened;
public slots:
    void record(const QUrl& url) { opened.append(url); }
};

class RestoreFeedbackClipboard {
public:
    RestoreFeedbackClipboard() : m_saved(new QMimeData)
    {
        if (const auto* mime = QApplication::clipboard()->mimeData())
            for (const auto& format : mime->formats())
                m_saved->setData(format, mime->data(format));
    }
    ~RestoreFeedbackClipboard() { QApplication::clipboard()->setMimeData(m_saved); }

private:
    QMimeData* m_saved;
};

void saveFixture(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(bytes), bytes.size());
}
} // namespace

TEST(GalleryDiagnosticsTest, AutomaticHistorySurvivesButtonRecreationAndPersists)
{
    QTemporaryDir directory;
    QString id;
    {
        QWidget window;
        auto* service = new fluent::gallery::GalleryDiagnostics(&window, directory.path());
        id = service->sessionId();
        LOG_WARN("recorded-before-opening-feedback");
        auto* button = new fluent::gallery::GalleryFeedbackButton(&window);
        delete button;
        EXPECT_TRUE(service->session()->isRecording());
        button = new fluent::gallery::GalleryFeedbackButton(&window);
        EXPECT_TRUE(button->isEnabled());
        EXPECT_EQ(fluent::gallery::GalleryDiagnostics::ensure(button), service);
        const auto report = service->captureReport();
        EXPECT_FALSE(report.contains("_local_process"));
        EXPECT_TRUE(QJsonDocument::fromVariant(report).toJson().contains("recorded-before-opening-feedback"));
        QUrlQuery query(service->issueUrl(report));
        EXPECT_EQ(query.queryItemValue("template"), "bug_report.yml");
        EXPECT_TRUE(query.queryItemValue("evidence").contains(id));
        EXPECT_FALSE(service->issueUrl(report).toString().contains("recorded-before-opening-feedback"));
    }
    QFile stored(directory.filePath("session-" + id + ".json"));
    ASSERT_TRUE(stored.open(QIODevice::ReadOnly));
    const auto bytes = stored.readAll();
    EXPECT_TRUE(bytes.contains("recorded-before-opening-feedback"));
    EXPECT_TRUE(bytes.contains("_local_process"));
    QWidget next;
    fluent::gallery::GalleryDiagnostics service(&next, directory.path());
    EXPECT_FALSE(service.savedAttachments().value("previous-session.json").toByteArray().contains("_local_process"));
}

TEST(GalleryDiagnosticsTest, FeedbackCopiesPersistentZipAndReportsSaveFailure)
{
    RestoreFeedbackClipboard restoreClipboard;
    QTemporaryDir directory;
    QWidget context;
    bool completed = false;
    bool success = false;
    QString path;
    fluent::gallery::platform::prepareDiagnosticFeedback(&context, {{"session_id", "synthetic"}}, {}, {},
        directory.path(), [&](bool ok, const QString& detail) {
            completed = true;
            success = ok;
            path = detail;
        });
    ASSERT_TRUE(QTest::qWaitFor([&] { return completed; }, 5000));
    ASSERT_TRUE(success) << path.toStdString();
    EXPECT_EQ(QApplication::clipboard()->mimeData()->urls(), QList<QUrl>{QUrl::fromLocalFile(path)});
    QFile zip(path);
    ASSERT_TRUE(zip.open(QIODevice::ReadOnly));
    const auto bytes = zip.readAll();
    EXPECT_TRUE(bytes.startsWith("PK\x03\x04"));
    EXPECT_TRUE(bytes.contains("report.json"));
    EXPECT_TRUE(bytes.contains("crash_files_included"));
    const auto blocker = directory.filePath("not-a-directory");
    saveFixture(blocker, "occupied");
    completed = false;
    fluent::gallery::platform::prepareDiagnosticFeedback(&context, {}, {}, {}, blocker, [&](bool ok, const QString&) {
        completed = true;
        success = ok;
    });
    ASSERT_TRUE(QTest::qWaitFor([&] { return completed; }, 5000));
    EXPECT_FALSE(success);
    EXPECT_EQ(QApplication::clipboard()->mimeData()->urls(), QList<QUrl>{QUrl::fromLocalFile(path)});
}

TEST(GalleryDiagnosticsTest, CrashCollectionRequiresIdentityAndTimeAndSkipsLinks)
{
    QTemporaryDir directory;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    QVariantMap identity{{"pid", 48191}, {"executable", "/test/Gallery"}, {"started_ms", now - 10000},
        {"checkpoint_ms", now}};
    const QByteArray ips = "{\"app_name\":\"Gallery\"}\n{\"pid\":48191,\"procPath\":\"/test/Gallery\"}";
    saveFixture(directory.filePath("gallery.ips"), ips);
    saveFixture(directory.filePath("unrelated.ips"), "{\"pid\":48192,\"procPath\":\"/test/Gallery\"}");
    saveFixture(directory.filePath("other.ips"), "{\"pid\":48191,\"procPath\":\"/other/App\"}");
    saveFixture(directory.filePath("Gallery.48191.dmp"), "synthetic dump");
    saveFixture(directory.filePath("Gallery.48192.dmp"), "unrelated dump");
    QFile::link(directory.filePath("gallery.ips"), directory.filePath("linked.ips"));
    const auto files = fluent::gallery::platform::existingCrashAttachments({identity}, directory.path());
    EXPECT_EQ(files.size(), 2);
    EXPECT_TRUE(files.values().contains(ips));
    EXPECT_TRUE(files.values().contains(QByteArray("synthetic dump")));
    identity["started_ms"] = now - 1000000;
    identity["checkpoint_ms"] = now - 900000;
    EXPECT_TRUE(fluent::gallery::platform::existingCrashAttachments({identity}, directory.path()).isEmpty());
    EXPECT_TRUE(fluent::gallery::platform::existingCrashAttachments({}, directory.path()).isEmpty());
}

TEST(GalleryDiagnosticsTest, FeedbackEntryFitsExpandedCompactAndTopNavigation)
{
    fluent::gallery::GalleryNavigationItem settings;
    settings.id = "settings";
    settings.title = "Settings";
    settings.iconGlyph = Typography::Icons::Settings;
    settings.kind = fluent::gallery::GalleryNavigationItem::Kind::FooterRoute;
    fluent::gallery::GalleryNavigationPane footer({settings});
    fluent::gallery::GalleryTopNavigationPane top({settings});
    const auto original = fluent::FluentElement::currentTheme();
    for (const auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        fluent::FluentElement::setTheme(theme);
        for (bool compact : {false, true}) {
            footer.setCompact(compact);
            footer.resize(compact ? 48 : 280, footer.minimumHeight());
            footer.show();
            QTest::qWait(220);
            auto* button = footer.findChild<fluent::basicinput::Button*>("galleryFeedbackButton");
            ASSERT_NE(button, nullptr);
            EXPECT_TRUE(button->isVisible());
            EXPECT_TRUE(footer.rect().contains(button->geometry()));
            EXPECT_EQ(button->size(), QSize(24, 24));
            EXPECT_EQ(button->accessibleName(), "Report a problem");
            EXPECT_EQ(button->fluentLayout(), fluent::basicinput::Button::IconOnly);
            const auto folder = qEnvironmentVariable("FLUENTQT_DIAGNOSTIC_SNAPSHOT_DIR");
            if (!folder.isEmpty()) {
                QDir().mkpath(folder);
                EXPECT_TRUE(footer.grab().save(folder
                    + QStringLiteral("/footer-%1-%2.png")
                        .arg(theme == fluent::FluentElement::Light ? "light" : "dark")
                        .arg(compact ? "compact" : "expanded")));
            }
        }
        top.resize(top.sizeHint());
        top.show();
        QTest::qWait(50);
        auto* button = top.findChild<fluent::basicinput::Button*>("galleryFeedbackButton");
        ASSERT_NE(button, nullptr);
        EXPECT_TRUE(top.rect().contains(button->geometry()));
        EXPECT_TRUE(button->isVisible());
    }
    fluent::FluentElement::setTheme(original);
}

TEST(GalleryDiagnosticsTest, ReadyTipWaitsForUserAndOpensPrefilledIssue)
{
    RestoreFeedbackClipboard restoreClipboard;
    FeedbackUrlReceiver browser;
    QTemporaryDir directory;
    QWidget root;
    root.resize(800, 480);
    auto* anchor = new fluent::gallery::GalleryFeedbackButton(&root);
    anchor->move(24, root.height() - 48);
    root.show();
    fluent::gallery::GalleryDiagnostics service(&root, directory.path());
    service.reportProblem(anchor);
    service.reportProblem(anchor);
    auto ready = [&]() {
        auto* toast = dynamic_cast<fluent::gallery::GalleryFeedbackTip*>(
            root.findChild<fluent::dialogs_flyouts::TeachingTip*>("galleryFeedbackTip"));
        return toast && toast->title() == "Diagnostic ZIP ready" ? toast : nullptr;
    };
    ASSERT_TRUE(QTest::qWaitFor([&] { return ready() != nullptr; }, 5000));
    auto* toast = ready();
    ASSERT_TRUE(browser.opened.isEmpty());
    EXPECT_TRUE(toast->isLightDismissEnabled());
    EXPECT_EQ(toast->target(), anchor);
    EXPECT_EQ(toast->preferredPlacement(), fluent::dialogs_flyouts::TeachingTip::RightBottom);
    const QString shortcut = QKeySequence(QKeySequence::Paste).toString(QKeySequence::NativeText);
    EXPECT_EQ(toast->message(),
        QStringLiteral("Press %1 on GitHub to attach it.")
            .arg(shortcut.isEmpty() ? QStringLiteral("Ctrl+V") : shortcut));
    EXPECT_EQ(toast->artifactName(), "fluentqt-report.zip");
    EXPECT_EQ(toast->artifactState(), "Copied to clipboard");
    ASSERT_NE(toast->actionButton(), nullptr);
    EXPECT_EQ(toast->actionButton()->text(), "Open GitHub issue");
    auto* icon = toast->findChild<fluent::FontIcon*>("galleryFeedbackStatusIcon");
    ASSERT_NE(icon, nullptr);
    EXPECT_EQ(icon->iconSize(), 16);
    const auto originalTheme = fluent::FluentElement::currentTheme();
    const auto captureDirectory = qEnvironmentVariable("FLUENTQT_DIAGNOSTIC_SNAPSHOT_DIR");
    for (const auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        fluent::FluentElement::setTheme(theme);
        for (int width : {800, 360, 280}) {
            root.resize(width, 480);
            QTest::qWait(220);
            auto* card = toast->contentHost();
            auto* badge = toast->findChild<QWidget*>("galleryFeedbackStatusBadge");
            auto* artifact = toast->findChild<QWidget*>("galleryFeedbackArtifact");
            auto* artifactSize = toast->findChild<fluent::textfields::Label*>("galleryFeedbackArtifactSize");
            ASSERT_NE(card, nullptr);
            ASSERT_NE(badge, nullptr);
            ASSERT_NE(artifact, nullptr);
            ASSERT_NE(artifactSize, nullptr);
            EXPECT_TRUE(artifact->isVisible());
            EXPECT_FALSE(artifactSize->text().isEmpty());
            EXPECT_LE(card->width(), 244);
            EXPECT_EQ(card->height(), 140);
            EXPECT_EQ(badge->size(), QSize(24, 24));
            EXPECT_EQ(artifact->height(), 40);
            EXPECT_FALSE(toast->isTailVisible());
            auto* title = toast->findChild<fluent::textfields::Label*>("galleryFeedbackTitle");
            auto* message = toast->findChild<fluent::textfields::Label*>("galleryFeedbackMessage");
            ASSERT_NE(title, nullptr);
            ASSERT_NE(message, nullptr);
            EXPECT_EQ(title->font().pixelSize(), 13);
            EXPECT_EQ(title->font().weight(), QFont::DemiBold);
            EXPECT_EQ(message->font().pixelSize(), 12);
            EXPECT_EQ(toast->actionButton()->font().pixelSize(), 12);
            EXPECT_EQ(toast->actionButton()->font().weight(), QFont::DemiBold);
            EXPECT_EQ(toast->actionButton()->height(), 24);
            EXPECT_EQ(toast->actionButton()->fluentStyle(), fluent::basicinput::Button::Subtle);
            EXPECT_EQ(toast->actionButton()->textColorRole(), fluent::basicinput::Button::TextColorRole::Accent);
            EXPECT_EQ(toast->actionButton()->fluentLayout(), fluent::basicinput::Button::IconAfter);
            EXPECT_EQ(badge->mapTo(card, QPoint()).x(), artifact->mapTo(card, QPoint()).x());
            EXPECT_EQ(artifact->mapTo(card, QPoint()).x(), message->mapTo(card, QPoint()).x());
            EXPECT_EQ(toast->actionButton()->mapTo(card, QPoint()).x() + 8, message->mapTo(card, QPoint()).x());
            EXPECT_TRUE(root.rect().contains(QRect(card->mapTo(&root, QPoint()), card->size())));
            EXPECT_EQ(icon->glyph(), "ic_fluent_checkmark_circle_16_regular");
            if (!captureDirectory.isEmpty())
                EXPECT_TRUE(toast->grab().save(captureDirectory
                    + QStringLiteral("/cpp-tip-%1-%2.png")
                        .arg(theme == fluent::FluentElement::Light ? "light" : "dark")
                        .arg(width)));
        }
    }
    fluent::FluentElement::setTheme(originalTheme);
    auto* button = toast->findChild<fluent::basicinput::Button*>("galleryFeedbackAction");
    ASSERT_NE(button, nullptr);
    button->click();
    ASSERT_EQ(browser.opened.size(), 1);
    EXPECT_EQ(QUrlQuery(browser.opened.first()).queryItemValue("template"), "bug_report.yml");
    EXPECT_TRUE(QUrlQuery(browser.opened.first()).queryItemValue("evidence").contains(service.sessionId()));
}

TEST(GalleryDiagnosticsTest, FailedPreparationStaysLocalAndCanRetry)
{
    RestoreFeedbackClipboard restoreClipboard;
    FeedbackUrlReceiver browser;
    QTemporaryDir directory;
    const auto blocker = directory.filePath("blocked");
    saveFixture(blocker, "occupied");
    QWidget root;
    root.resize(500, 360);
    auto* anchor = new fluent::gallery::GalleryFeedbackButton(&root);
    anchor->move(24, root.height() - 48);
    root.show();
    fluent::gallery::GalleryDiagnostics service(&root, blocker);
    service.reportProblem(anchor);
    auto* toast = dynamic_cast<fluent::gallery::GalleryFeedbackTip*>(
        root.findChild<fluent::dialogs_flyouts::TeachingTip*>("galleryFeedbackTip"));
    ASSERT_NE(toast, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return toast->title() == "Couldn’t save report"; }, 5000));
    EXPECT_TRUE(browser.opened.isEmpty());
    ASSERT_NE(toast->actionButton(), nullptr);
    EXPECT_EQ(toast->actionButton()->text(), "Try again");
    ASSERT_TRUE(QFile::remove(blocker));
    auto* retry = toast->findChild<fluent::basicinput::Button*>("galleryFeedbackAction");
    ASSERT_NE(retry, nullptr);
    retry->click();
    ASSERT_TRUE(QTest::qWaitFor([&] { return toast->isOpen() && toast->title() == "Diagnostic ZIP ready"; }, 5000));
    EXPECT_TRUE(browser.opened.isEmpty());
}

TEST(GalleryDiagnosticsTest, TipTracksAnchorAndAdaptsToEdgesAndMotionPolicy)
{
    using Tip = fluent::gallery::GalleryFeedbackTip;
    auto& motion = fluent::MotionPolicy::instance();
    const auto original = motion.mode();
    QWidget root;
    root.resize(800, 480);
    root.setAutoFillBackground(true);
    auto palette = root.palette();
    palette.setColor(QPalette::Window, QColor("#102030"));
    root.setPalette(palette);
    QWidget parent(&root);
    parent.setGeometry(10, 300, 150, 150);
    fluent::gallery::GalleryFeedbackButton anchor(&parent);
    anchor.move(20, 80);
    root.show();
    Tip tip(&root);
    tip.setFeedback("Diagnostic ZIP ready", "Press ⌘V on GitHub to attach it.", false, false);
    auto checkPlacement = [&](Tip::PreferredPlacement placement) {
        const QRect buttonRect(anchor.mapTo(&root, QPoint()), anchor.size());
        const QRect card(tip.contentHost()->mapTo(&root, QPoint()), tip.contentHost()->size());
        EXPECT_FALSE(tip.isTailVisible());
        EXPECT_TRUE(root.rect().contains(card));
        if (placement == Tip::Right || placement == Tip::Left)
            EXPECT_LE(qAbs(card.center().y() - buttonRect.center().y()), 1);
        if (placement == Tip::Right || placement == Tip::RightBottom)
            EXPECT_EQ(card.left() - buttonRect.right(), 12);
        else if (placement == Tip::Left || placement == Tip::LeftBottom)
            EXPECT_EQ(buttonRect.left() - card.right(), 13);
        else if (placement == Tip::Top || placement == Tip::TopLeft || placement == Tip::TopRight)
            EXPECT_EQ(buttonRect.top() - card.bottom(), 13);
        else
            EXPECT_EQ(card.top() - buttonRect.bottom(), 12);
        if (placement == Tip::TopRight || placement == Tip::BottomRight)
            EXPECT_EQ(card.right(), buttonRect.right());
        if (placement == Tip::RightBottom)
            EXPECT_EQ(card.bottom(), buttonRect.bottom());
    };
    motion.setMode(fluent::MotionPolicy::Mode::Full);
    tip.setFeedback("Preparing report ZIP…", "Packaging logs for clipboard…", true, false);
    tip.showFor(&anchor);
    EXPECT_LT(tip.popupProgress(), 1.0);
    for (auto* animation : tip.findChildren<QPropertyAnimation*>(QString(), Qt::FindDirectChildrenOnly))
        if (animation->targetObject() == &tip && animation->propertyName() == "popupProgress")
            EXPECT_EQ(animation->duration(), 220);
    EXPECT_LT(tip.placementMargin(), 12);
    ASSERT_TRUE(QTest::qWaitFor([&] { return tip.popupProgress() >= 1.0; }));
    EXPECT_EQ(tip.placementMargin(), 12);
    EXPECT_EQ(tip.cardSize(), QSize(244, 66));
    EXPECT_EQ(tip.preferredPlacement(), Tip::Right);
    checkPlacement(Tip::Right);
    // Smoothly morphs height in place when the ZIP artifact and action arrive while open.
    tip.setFeedback("Diagnostic ZIP ready", "Press ⌘V on GitHub to attach it.", false, false);
    tip.setArtifact("fluentqt-report.zip", 233675, false);
    tip.setAction("Open GitHub issue", "ic_fluent_open_16_regular", &tip, [] {});
    EXPECT_EQ(tip.targetCardSize(), QSize(244, 140));
    ASSERT_TRUE(QTest::qWaitFor([&] { return tip.cardSize() == QSize(244, 140); }, 1000));
    const QPoint before = tip.pos();
    parent.move(30, 300);
    QTest::qWait(30);
    EXPECT_EQ(tip.x() - before.x(), 20);
    parent.move(640, 0);
    QTest::qWait(30);
    EXPECT_EQ(tip.preferredPlacement(), Tip::Left);
    checkPlacement(Tip::Left);
    // Expanded navigation footer aligns right edge inside the 260px pane instead of straddling its divider.
    parent.move(208, 360);
    QTest::qWait(30);
    EXPECT_EQ(tip.preferredPlacement(), Tip::TopRight);
    checkPlacement(Tip::TopRight);
    // Compact rail footer opens cleanly to the right of the 48px rail.
    parent.move(10, 360);
    QTest::qWait(30);
    EXPECT_EQ(tip.preferredPlacement(), Tip::RightBottom);
    checkPlacement(Tip::RightBottom);
    // Top navigation bar drops down right-aligned below the top-right button.
    parent.move(620, -68);
    QTest::qWait(30);
    EXPECT_EQ(tip.preferredPlacement(), Tip::BottomRight);
    checkPlacement(Tip::BottomRight);
    root.resize(320, 480);
    parent.move(130, 300);
    QTest::qWait(30);
    EXPECT_EQ(tip.preferredPlacement(), Tip::Top);
    checkPlacement(Tip::Top);
    EXPECT_TRUE(root.rect().contains(QRect(tip.contentHost()->mapTo(&root, QPoint()), tip.contentHost()->size())));
    anchor.hide();
    EXPECT_FALSE(tip.isOpen());
    QTest::qWait(300);
    anchor.show();
    for (auto mode : {fluent::MotionPolicy::Mode::Reduced, fluent::MotionPolicy::Mode::Disabled}) {
        motion.setMode(mode);
        tip.showFor(&anchor);
        EXPECT_EQ(tip.placementMargin(), 12);
        ASSERT_TRUE(QTest::qWaitFor([&] { return tip.popupProgress() >= 1.0; }));
        tip.close();
        QTest::qWait(150);
    }
    motion.setMode(original);
}

TEST(GalleryDiagnosticsTest, PreparingSpinnerStaysStoppedWhenDismissed)
{
    auto& motion = fluent::MotionPolicy::instance();
    const auto original = motion.mode();
    QWidget root;
    root.resize(800, 480);
    fluent::gallery::GalleryFeedbackButton anchor(&root);
    anchor.move(24, 420);
    root.show();
    fluent::gallery::GalleryFeedbackTip tip(&root);
    auto* icon = tip.findChild<fluent::FontIcon*>("galleryFeedbackStatusIcon");
    ASSERT_NE(icon, nullptr);
    QPropertyAnimation* spinner = nullptr;
    for (auto* animation : tip.findChildren<QPropertyAnimation*>())
        if (animation->targetObject() == icon && animation->propertyName() == "rotation")
            spinner = animation;
    ASSERT_NE(spinner, nullptr);

    motion.setMode(fluent::MotionPolicy::Mode::Full);
    tip.setFeedback("Preparing report ZIP…", "Packaging logs for clipboard…", true, false);
    EXPECT_EQ(spinner->state(), QAbstractAnimation::Stopped);
    tip.showFor(&anchor);
    EXPECT_EQ(spinner->state(), QAbstractAnimation::Running);
    tip.close();
    EXPECT_EQ(spinner->state(), QAbstractAnimation::Stopped);
    // A policy change during the closing transition must not restart it either.
    motion.setMode(fluent::MotionPolicy::Mode::Reduced);
    motion.setMode(fluent::MotionPolicy::Mode::Full);
    EXPECT_EQ(spinner->state(), QAbstractAnimation::Stopped);
    EXPECT_TRUE(QTest::qWaitFor([&] { return !tip.isVisible(); }));
    motion.setMode(fluent::MotionPolicy::Mode::Disabled);
    motion.setMode(fluent::MotionPolicy::Mode::Full);
    EXPECT_EQ(spinner->state(), QAbstractAnimation::Stopped);
    EXPECT_DOUBLE_EQ(icon->rotation(), 0.0);
    tip.showFor(&anchor);
    EXPECT_EQ(spinner->state(), QAbstractAnimation::Running);
    tip.setFeedback("Diagnostic ZIP ready", "Attach it on GitHub.", false, false);
    EXPECT_EQ(spinner->state(), QAbstractAnimation::Stopped);
    motion.setMode(original);
}

TEST(GalleryDiagnosticsTest, ArtifactReportsDownloadRequestWithoutClaimingItWasSaved)
{
    fluent::gallery::GalleryFeedbackTip tip;
    tip.setArtifact("fluentqt-report.zip", 0, true);
    EXPECT_EQ(tip.artifactState(), "Download started");
    tip.setArtifact("fluentqt-report.zip", 123, false);
    EXPECT_EQ(tip.artifactState(), "Copied to clipboard");
}

TEST(GalleryDiagnosticsTest, BackgroundCompletionDoesNotReopenDismissedTip)
{
    RestoreFeedbackClipboard restoreClipboard;
    QTemporaryDir directory;
    QWidget root;
    root.resize(800, 480);
    fluent::gallery::GalleryFeedbackButton anchor(&root);
    anchor.move(24, 420);
    root.show();
    fluent::gallery::GalleryDiagnostics service(&root, directory.path());
    service.reportProblem(&anchor);
    auto* tip = root.findChild<fluent::dialogs_flyouts::TeachingTip*>("galleryFeedbackTip");
    ASSERT_NE(tip, nullptr);
    tip->close();
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            const auto* mime = QApplication::clipboard()->mimeData();
            return mime && !mime->urls().isEmpty() && mime->urls().first().toLocalFile().startsWith(directory.path());
        },
        5000));
    EXPECT_FALSE(tip->isOpen());
}

TEST(GalleryDiagnosticsTest, FeedbackEntryWorksInRealGalleryAcrossThemes)
{
    RestoreFeedbackClipboard restoreClipboard;
    FeedbackUrlReceiver browser;
    auto& settings = fluent::gallery::GallerySettings::instance();
    const bool intro = settings.introCompleted();
    const bool spatial = settings.spatialModeEnabled();
    settings.setSpatialModeEnabled(false);
    settings.setIntroCompleted(true);
    const auto originalTheme = fluent::FluentElement::currentTheme();
    const auto originalMode = settings.themeMode();
    fluent::gallery::GalleryWindow window;
    window.setBackdropEffect(fluent::windowing::BackdropEffect::Solid);
    window.resize(1180, 850);
    ASSERT_TRUE(window.selectRoute("settings"));
    window.show();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.findChild<QWidget*>("gallerySplashScreen"); }, 10000));
    fluent::basicinput::Button* button = nullptr;
    for (auto* candidate : window.findChildren<fluent::basicinput::Button*>("galleryFeedbackButton"))
        if (candidate->isVisible()) {
            button = candidate;
            break;
        }
    ASSERT_NE(button, nullptr);
    button->click();
    auto ready = [&]() {
        auto* tip = dynamic_cast<fluent::gallery::GalleryFeedbackTip*>(
            window.findChild<fluent::dialogs_flyouts::TeachingTip*>("galleryFeedbackTip"));
        return tip && tip->title() == "Diagnostic ZIP ready" ? tip : nullptr;
    };
    ASSERT_TRUE(QTest::qWaitFor([&] { return ready() != nullptr; }, 5000));
    auto* tip = ready();
    EXPECT_EQ(tip->target(), button);
    EXPECT_EQ(tip->artifactName(), "fluentqt-report.zip");
    EXPECT_EQ(tip->artifactState(), "Copied to clipboard");
    EXPECT_TRUE(browser.opened.isEmpty());
    const QString directory = qEnvironmentVariable("FLUENTQT_DIAGNOSTIC_SNAPSHOT_DIR");
    for (const auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        settings.setThemeMode(theme == fluent::FluentElement::Light
                ? fluent::gallery::GallerySettings::ThemeMode::Light
                : fluent::gallery::GallerySettings::ThemeMode::Dark);
        QTest::qWait(240);
        EXPECT_TRUE(tip->isOpen());
        EXPECT_EQ(tip->effectiveThemeUsesDarkAppearance(), theme == fluent::FluentElement::Dark);
        EXPECT_EQ(tip->cardSize(), QSize(244, 140));
        EXPECT_EQ(tip->preferredPlacement(), fluent::dialogs_flyouts::TeachingTip::TopRight);
        const QRect card(tip->contentHost()->mapTo(&window, QPoint()), tip->cardSize());
        EXPECT_TRUE(window.rect().contains(card));
        EXPECT_LT(card.right(), 260);
        if (!directory.isEmpty()) {
            QDir().mkpath(directory);
            const QString mode = theme == fluent::FluentElement::Light ? "light" : "dark";
            EXPECT_TRUE(window.grab().save(directory + "/cpp-gallery-" + mode + ".png"));
            const QRect anchor(button->mapTo(&window, QPoint()), button->size());
            QRect crop = card.united(anchor).adjusted(-24, -24, 32, 24).intersected(window.rect());
            crop.setLeft(0);
            EXPECT_TRUE(window.grab(crop).save(directory + "/cpp-detail-" + mode + ".png"));
        }
    }
    window.resize(760, 850);
    QTest::qWait(300);
    button = nullptr;
    for (auto* candidate : window.findChildren<fluent::basicinput::Button*>("galleryFeedbackButton"))
        if (candidate->isVisible()) {
            button = candidate;
            break;
        }
    ASSERT_NE(button, nullptr);
    button->click();
    ASSERT_TRUE(QTest::qWaitFor([&] { return tip->isOpen() && tip->cardSize() == QSize(244, 140); }, 5000));
    QTest::qWait(240);
    const QRect compactCard(tip->contentHost()->mapTo(&window, QPoint()), tip->cardSize());
    EXPECT_EQ(tip->preferredPlacement(), fluent::dialogs_flyouts::TeachingTip::RightBottom);
    EXPECT_GE(compactCard.left(), 55);
    if (!directory.isEmpty())
        EXPECT_TRUE(window.grab().save(directory + "/cpp-gallery-compact-dark.png"));
    window.close();
    settings.setIntroCompleted(intro);
    settings.setSpatialModeEnabled(spatial);
    settings.setThemeMode(originalMode);
    fluent::FluentElement::setTheme(originalTheme);
}

#include "TestGalleryDiagnostics.moc"
