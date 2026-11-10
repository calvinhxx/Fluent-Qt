#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTest>
#include <QWidget>
#include <thread>
#include <FluentQt/Diagnostics.h>

namespace {
using fluent::diagnostics::DiagnosticSession;
QByteArray json(const DiagnosticSession& session)
{
    return QJsonDocument::fromVariant(session.report()).toJson();
}
} // namespace

TEST(DiagnosticSessionTest, RecordsWithoutChangingEditingOrReadingPrivateText)
{
    QWidget root;
    QLineEdit editor(&root);
    editor.setObjectName(QStringLiteral("private-customer-name"));
    editor.setText(QStringLiteral("secret-password"));
    root.show();
    DiagnosticSession session;
    QSignalSpy changes(&session, &DiagnosticSession::recordingChanged);
    ASSERT_TRUE(session.start(&root));
    EXPECT_FALSE(session.start(&root, true));
    QTest::keyClick(&editor, Qt::Key_A);
    QTest::keyClick(&editor, Qt::Key_Left);
    session.recordMessage(QtWarningMsg, QStringLiteral("fluentqt.editing"), QStringLiteral("private-log-body"));
    session.stop();
    session.stop();
    EXPECT_EQ(changes.count(), 2);
    EXPECT_FALSE(session.isRecording());
    EXPECT_FALSE(session.report().value("events").toList().isEmpty());
    EXPECT_TRUE(editor.text().contains('a'));
    const auto encoded = json(session);
    for (const auto& forbidden : {"private-customer-name", "secret-password", "private-log-body"})
        EXPECT_FALSE(encoded.contains(forbidden));
    EXPECT_TRUE(encoded.contains("navigation_key"));
    EXPECT_TRUE(encoded.contains("fluentqt.editing"));
}

TEST(DiagnosticSessionTest, BoundedConcurrentLogsAndLifetimeSafeSink)
{
    QWidget root;
    auto session = std::make_unique<DiagnosticSession>();
    ASSERT_TRUE(session->start(&root, true));
    const auto sink = session->messageSink();
    std::thread first([&] {
        for (int i = 0; i < 600; ++i)
            sink(QtWarningMsg, "test", QString(2000, 'x'));
    });
    std::thread second([&] {
        for (int i = 0; i < 600; ++i)
            sink(QtInfoMsg, "test", "small");
    });
    first.join();
    second.join();
    session->stop();
    const auto report = session->report();
    const auto events = report.value("events").toList();
    EXPECT_EQ(events.size(), 512);
    EXPECT_EQ(report.value("dropped_events").toULongLong(), 688u);
    for (const auto& event : events)
        EXPECT_LE(event.toMap().value("message").toString().size(), 512);
    EXPECT_LT(json(*session).size(), 1024 * 1024);
    const auto frozen = json(*session);
    sink(QtWarningMsg, "test", "after-stop");
    EXPECT_EQ(json(*session), frozen);
    session.reset();
    sink(QtWarningMsg, "test", "after-destruction");
}

TEST(DiagnosticSessionTest, RootDestructionStopsAndClearDiscardsConsentAndData)
{
    auto root = std::make_unique<QWidget>();
    DiagnosticSession session;
    ASSERT_TRUE(session.start(root.get(), true));
    session.recordMessage(QtWarningMsg, "test", "captured");
    root.reset();
    EXPECT_FALSE(session.isRecording());
    EXPECT_TRUE(session.report().value("final").toMap().value("root_destroyed").toBool());
    EXPECT_TRUE(json(session).contains("captured"));
    session.clear();
    EXPECT_TRUE(session.report().value("events").toList().isEmpty());
    EXPECT_TRUE(session.report().value("environment").toMap().isEmpty());
    QWidget next;
    ASSERT_TRUE(session.start(&next));
    session.recordMessage(QtInfoMsg, "test", "private-new-session");
    session.stop();
    EXPECT_FALSE(json(session).contains("private-new-session"));
}

TEST(DiagnosticSessionTest, LargeTreeSnapshotIsBoundedAndOutsideEventsAreIgnored)
{
    QWidget root;
    QWidget outside;
    for (int i = 0; i < 400; ++i)
        new QWidget(&root);
    DiagnosticSession session;
    ASSERT_TRUE(session.start(&root));
    outside.resize(333, 222);
    session.stop();
    for (const auto& key : {"initial", "final"}) {
        const auto snapshot = session.report().value(key).toMap();
        EXPECT_EQ(snapshot.value("widgets").toList().size(), 256);
        EXPECT_TRUE(snapshot.value("truncated").toBool());
    }
    EXPECT_TRUE(session.report().value("events").toList().isEmpty());
}

TEST(DiagnosticSessionTest, ContinuousCaptureKeepsHistoryAndCapturesCurrentGeometry)
{
    QWidget root;
    DiagnosticSession session;
    ASSERT_TRUE(session.start(&root, true, 0));
    session.recordMessage(QtWarningMsg, "test", "before-feedback");
    root.resize(720, 400);
    const auto report = session.captureReport();
    EXPECT_TRUE(session.isRecording());
    EXPECT_EQ(report.value("limits").toMap().value("session_ms").toInt(), 0);
    EXPECT_EQ(
        report.value("final")
            .toMap()
            .value("widgets")
            .toList()
            .first()
            .toMap()
            .value("rect")
            .toMap()
            .value("width")
            .toInt(),
        720);
    EXPECT_TRUE(QJsonDocument::fromVariant(report).toJson().contains("before-feedback"));
    session.recordMessage(QtWarningMsg, "test", "after-feedback");
    EXPECT_TRUE(json(session).contains("after-feedback"));
}

TEST(DiagnosticSessionTest, BundleRejectsUnsafeNamesAndOversizedAttachments)
{
    QVariantMap report{{"schema_version", 1}};
    EXPECT_TRUE(DiagnosticSession::createBundle(report, {{"../secret", QByteArray("x")}}).isEmpty());
    EXPECT_TRUE(DiagnosticSession::createBundle(report, {{"report.json", QByteArray("x")}}).isEmpty());
    EXPECT_TRUE(DiagnosticSession::createBundle(report, {{"dump.dmp", QByteArray(21 * 1024 * 1024, 'x')}}).isEmpty());
    EXPECT_TRUE(DiagnosticSession::createBundle(report, {{"dump.dmp", QString("not bytes")}}).isEmpty());
    EXPECT_TRUE(DiagnosticSession::createBundle(report).startsWith("PK\003\004"));
}
