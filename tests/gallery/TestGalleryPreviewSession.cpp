#include "view/preview/GalleryPreviewSession.h"
#include "view/preview/GalleryPreviewActions.h"

#include "components/basicinput/Button.h"
#include "components/status_info/ProgressBar.h"
#include "components/textfields/LineEdit.h"
#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <gtest/gtest.h>

namespace {

using fluent::gallery::GalleryPreviewSession;

QJsonObject nodeNamed(const QJsonObject& report, const QString& name)
{
    for (const auto& node : report.value("nodes").toArray()) {
        if (node.toObject().value("object_name").toString() == name)
            return node.toObject();
    }
    return {};
}

TEST(GalleryPreviewSessionTest, OperatesRealControlsAndResetsTypedData)
{
    QTemporaryDir directory;
    QWidget root;
    root.resize(400, 240);
    fluent::basicinput::Button button(QStringLiteral("Toggle"), &root);
    button.setObjectName("button");
    button.setCheckable(true);
    button.setGeometry(20, 20, 120, 36);
    fluent::textfields::LineEdit editor(&root);
    editor.setObjectName("editor");
    editor.setGeometry(20, 80, 250, 36);
    fluent::status_info::ProgressBar progress(&root);
    progress.setObjectName("progress");
    progress.setValue(20);
    root.show();
    QApplication::processEvents();
    GalleryPreviewSession session(&root);
    QString error;
    ASSERT_TRUE(session.start(directory.path(), error)) << error.toStdString();
    const auto report = session.execute("inspect");
    const auto buttonId = nodeNamed(report, "button").value("id");
    ASSERT_FALSE(buttonId.toString().isEmpty());
    EXPECT_FALSE(
        session.execute("action", {{"target", buttonId}, {"action", "click"}}).contains("error"));
    EXPECT_TRUE(button.isChecked());
    EXPECT_FALSE(session
                     .execute("action", {{"target", "editor"},
                                         {"action", "type_text"},
                                         {"text", QStringLiteral("Hello 你好 👋")}})
                     .contains("error"));
    EXPECT_EQ(editor.text(), QStringLiteral("Hello 你好 👋"));
    EXPECT_FALSE(
        session.execute("set_data", {{"target", "progress"}, {"property", "value"}, {"value", 75}})
            .contains("error"));
    EXPECT_EQ(progress.value(), 75);
    EXPECT_TRUE(
        session
            .execute("set_data", {{"target", "progress"}, {"property", "value"}, {"value", "bad"}})
            .contains("error"));
    EXPECT_TRUE(
        session.execute("set_data", {{"target", "progress"}, {"property", "width"}, {"value", 99}})
            .contains("error"));
    EXPECT_EQ(session.execute("reset_data").value("restored").toInt(), 1);
    EXPECT_EQ(progress.value(), 20);
    const auto png =
        QByteArray::fromBase64(session.execute("screenshot").value("data").toString().toLatin1());
    EXPECT_TRUE(png.startsWith("\x89PNG"));
}

TEST(GalleryPreviewSessionTest, RejectsStaleOrAmbiguousTargetsAndHidesPasswordText)
{
    QTemporaryDir directory;
    QWidget root;
    auto* editor = new fluent::textfields::LineEdit(&root);
    editor->setObjectName("password");
    editor->setEchoMode(QLineEdit::Password);
    editor->setText("secret");
    GalleryPreviewSession session(&root);
    QString error;
    ASSERT_TRUE(session.start(directory.path(), error));
    const auto report = session.execute("inspect");
    const auto password = nodeNamed(report, "password");
    EXPECT_FALSE(password.value("data").toObject().contains("text"));
    const auto id = password.value("id");
    delete editor;
    EXPECT_TRUE(session.execute("set_data", {{"target", id}, {"property", "text"}, {"value", "no"}})
                    .contains("error"));
    QWidget first(&root), second(&root);
    first.setObjectName("duplicate");
    second.setObjectName("duplicate");
    EXPECT_TRUE(session
                    .execute("set_data",
                             {{"target", "duplicate"}, {"property", "enabled"}, {"value", false}})
                    .contains("error"));
}

TEST(GalleryPreviewSessionTest, SupplementaryTextPreservesEditingContracts)
{
    QTemporaryDir directory;
    QWidget root;
    fluent::textfields::LineEdit editor(&root);
    editor.setObjectName("editor");
    editor.setText("original");
    root.show();
    GalleryPreviewSession session(&root);
    QString error;
    ASSERT_TRUE(session.start(directory.path(), error));
    editor.selectAll();
    QSignalSpy edits(&editor, &QLineEdit::textEdited);
    const auto typeText = [&](const QString& text) {
        return session.execute("action",
                               {{"target", "editor"}, {"action", "type_text"}, {"text", text}});
    };
    ASSERT_FALSE(typeText(QStringLiteral("👋")).contains("error"));
    EXPECT_EQ(editor.text(), QStringLiteral("👋"));
    EXPECT_EQ(edits.count(), 1);
    editor.undo();
    EXPECT_EQ(editor.text(), "original");

    editor.setReadOnly(true);
    ASSERT_FALSE(typeText(QStringLiteral("👋")).contains("error"));
    EXPECT_EQ(editor.text(), "original");
    editor.setReadOnly(false);
    editor.clear();
    editor.setMaxLength(2);
    ASSERT_FALSE(typeText(QStringLiteral("👋!")).contains("error"));
    EXPECT_EQ(editor.text(), QStringLiteral("👋"));
}

TEST(GalleryPreviewSessionTest, RestoresOnlyChangedCompatibleNamedState)
{
    QTemporaryDir firstDirectory, secondDirectory;
    QWidget firstRoot, secondRoot;
    fluent::textfields::LineEdit first(&firstRoot), second(&secondRoot);
    first.setObjectName("note");
    second.setObjectName("note");
    first.setText("old default");
    second.setText("new default");
    GalleryPreviewSession firstSession(&firstRoot), secondSession(&secondRoot);
    QString error;
    ASSERT_TRUE(firstSession.start(firstDirectory.path(), error));
    ASSERT_TRUE(secondSession.start(secondDirectory.path(), error));
    EXPECT_TRUE(firstSession.execute("capture_state").value("widgets").toArray().isEmpty());
    first.setText("user input");
    const auto state = firstSession.execute("capture_state");
    EXPECT_EQ(secondSession.execute("restore_state", state).value("restored").toInt(), 1);
    EXPECT_EQ(second.text(), "user input");
    EXPECT_TRUE(
        secondSession
            .execute("set_data",
                     {{"target", nodeNamed(firstSession.execute("inspect"), "note").value("id")},
                      {"property", "text"},
                      {"value", "stale"}})
            .contains("error"));
}

TEST(GalleryPreviewSessionTest, ServesFileRequestsAndRejectsOldSessionNonce)
{
    QTemporaryDir directory;
    QWidget root;
    GalleryPreviewSession session(&root), competing(&root);
    QString error;
    ASSERT_TRUE(session.start(directory.path(), error));
    EXPECT_FALSE(competing.start(directory.path(), error));
    for (const bool current : {true, false}) {
        QFile request(directory.filePath("requests/request.json"));
        ASSERT_TRUE(request.open(QIODevice::WriteOnly));
        request.write(
            QJsonDocument(QJsonObject{{"id", "request"},
                                      {"session_id", current ? session.sessionId() : "old"},
                                      {"method", "inspect"},
                                      {"parameters", QJsonObject{}}})
                .toJson());
        request.close();
        const auto responsePath = directory.filePath("responses/request.json");
        ASSERT_TRUE(QTest::qWaitFor([&] { return QFile::exists(responsePath); }, 2000));
        QFile response(responsePath);
        ASSERT_TRUE(response.open(QIODevice::ReadOnly));
        const auto result =
            QJsonDocument::fromJson(response.readAll()).object().value("result").toObject();
        EXPECT_EQ(result.contains("error"), !current);
        response.close();
        response.remove();
    }
}

TEST(GalleryPreviewSessionTest, ToleratesControlRemovalDuringInput)
{
    QTemporaryDir directory;
    QWidget root;
    auto* editor = new fluent::textfields::LineEdit(&root);
    editor->setObjectName("removeOnInput");
    QObject::connect(editor, &QLineEdit::textChanged, &root, [editor] { editor->deleteLater(); });
    root.show();
    GalleryPreviewSession session(&root);
    QString error;
    ASSERT_TRUE(session.start(directory.path(), error));
    const auto result = session.execute(
        "action", {{"target", "removeOnInput"}, {"action", "type_text"}, {"text", "x"}});
    EXPECT_TRUE(result.contains("error"));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(session
                    .execute("set_data",
                             {{"target", "removeOnInput"}, {"property", "text"}, {"value", "gone"}})
                    .contains("error"));
}

TEST(GalleryPreviewSessionTest, KeepsFailedResetValuesAndRetriesOnlyPendingFields)
{
    QTemporaryDir directory;
    QWidget root;
    fluent::textfields::LineEdit editor(&root);
    editor.setObjectName("editor");
    editor.setText("original");
    GalleryPreviewSession session(&root);
    QString error;
    ASSERT_TRUE(session.start(directory.path(), error));
    ASSERT_FALSE(session
                     .execute("set_data",
                              {{"target", "editor"}, {"property", "text"}, {"value", "temporary"}})
                     .contains("error"));
    ASSERT_FALSE(session
                     .execute("set_data", {{"target", "editor"},
                                           {"property", "placeholderText"},
                                           {"value", "temporary hint"}})
                     .contains("error"));
    editor.setEchoMode(QLineEdit::Password);

    const auto partial = session.execute("reset_data");
    EXPECT_TRUE(partial.contains("error"));
    EXPECT_EQ(partial.value("restored").toInt(), 1);
    EXPECT_EQ(partial.value("failed").toInt(), 1);
    EXPECT_EQ(partial.value("pending").toInt(), 1);
    EXPECT_TRUE(editor.placeholderText().isEmpty());
    EXPECT_EQ(editor.text(), "temporary");
    editor.setEchoMode(QLineEdit::Normal);
    const auto retry = session.execute("reset_data");
    EXPECT_FALSE(retry.contains("error"));
    EXPECT_EQ(retry.value("restored").toInt(), 1);
    EXPECT_EQ(retry.value("pending").toInt(), 0);
    EXPECT_EQ(editor.text(), "original");
    EXPECT_EQ(session.execute("reset_data").value("restored").toInt(), 0);
}

TEST(GalleryPreviewSessionTest, DoesNotClaimResetSuccessWhenTheSetterClampsTheValue)
{
    QTemporaryDir directory;
    QWidget root;
    fluent::status_info::ProgressBar progress(&root);
    progress.setObjectName("progress");
    progress.setValue(80);
    GalleryPreviewSession session(&root);
    QString error;
    ASSERT_TRUE(session.start(directory.path(), error));
    ASSERT_FALSE(
        session.execute("set_data", {{"target", "progress"}, {"property", "value"}, {"value", 20}})
            .contains("error"));
    progress.setMaximum(50);
    const auto partial = session.execute("reset_data");
    EXPECT_TRUE(partial.contains("error"));
    EXPECT_EQ(partial.value("restored").toInt(), 0);
    EXPECT_EQ(partial.value("pending").toInt(), 1);
    progress.setMaximum(100);
    EXPECT_EQ(session.execute("reset_data").value("restored").toInt(), 1);
    EXPECT_EQ(progress.value(), 80);
}

TEST(GalleryPreviewSessionTest, DoesNotSendReleaseAfterPressDeletesTheControl)
{
    QTemporaryDir directory;
    QWidget root;
    auto* button = new fluent::basicinput::Button("Remove", &root);
    button->setObjectName("button");
    int clicks = 0;
    QObject::connect(button, &QPushButton::pressed, button, &QObject::deleteLater);
    QObject::connect(button, &QPushButton::clicked, &root, [&] { ++clicks; });
    root.show();
    QApplication::processEvents();
    GalleryPreviewSession session(&root);
    QString error;
    ASSERT_TRUE(session.start(directory.path(), error));
    EXPECT_TRUE(
        session.execute("action", {{"target", "button"}, {"action", "click"}}).contains("error"));
    EXPECT_EQ(clicks, 0);
}

class RemoveOnEnter final : public QObject {
protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Enter) {
            delete watched;
            return true;
        }
        return false;
    }
};

TEST(GalleryPreviewSessionTest, LegacyMouseMoveStopsIfEnterRemovesItsTarget)
{
    QWidget root;
    auto* button = new fluent::basicinput::Button("Remove", &root);
    button->setObjectName("button");
    root.show();
    QApplication::processEvents();
    RemoveOnEnter filter;
    button->installEventFilter(&filter);
    const auto result = fluent::gallery::executeGalleryPreviewActions(
        &root,
        {{"schema_version", 1},
         {"steps", QJsonArray{QJsonObject{{"target", "button"}, {"action", "mouse_move"}}}}});
    EXPECT_FALSE(result.passed);
}

} // namespace
