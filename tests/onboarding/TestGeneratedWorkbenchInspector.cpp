#include <gtest/gtest.h>

#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTest>
#include <QWidget>

#include "application/WorkspaceController.h"
#include "application/WorkspaceRepository.h"
#include "ui/shell/MainWindow.h"
#include "ui/theme/BrandTheme.h"
#include "utils/private/Inspector_p.h"

namespace {

constexpr auto WorkbenchApplicationId = "fluentqt_cpp_workbench";

class RepresentativeWorkspaceRepository final
    : public fluentqt_inspector_workbench::application::WorkspaceRepository {
public:
    fluentqt_inspector_workbench::domain::Workspace currentWorkspace() const override
    {
        return { "FluentQt Workbench", "/projects/fluentqt-workbench" };
    }
};

class EmptyWorkspaceRepository final
    : public fluentqt_inspector_workbench::application::WorkspaceRepository {
public:
    fluentqt_inspector_workbench::domain::Workspace currentWorkspace() const override
    {
        return {};
    }
};

TEST(WorkbenchInspectorTest, Contract_ApplicationScenesPass)
{
    QFile manifestFile(QString::fromUtf8(FLUENT_QT_APPLICATION_SCENES_PATH));
    ASSERT_TRUE(manifestFile.open(QIODevice::ReadOnly));
    QJsonParseError parseError;
    const QJsonDocument manifestDocument
        = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    ASSERT_EQ(parseError.error, QJsonParseError::NoError);
    ASSERT_TRUE(manifestDocument.isObject());

    const QJsonArray scenes = manifestDocument.object().value(QStringLiteral("scenes")).toArray();
    int automatedSceneCount = 0;
    for (const QJsonValue& sceneValue : scenes) {
        const QJsonObject scene = sceneValue.toObject();
        if (scene.value(QStringLiteral("application")).toString()
                != QString::fromLatin1(WorkbenchApplicationId)
            || scene.value(QStringLiteral("automation")).toString()
                != QStringLiteral("automated")) {
            continue;
        }

        ++automatedSceneCount;
        const QString sceneId = scene.value(QStringLiteral("id")).toString();
        const QJsonObject viewport = scene.value(QStringLiteral("viewport")).toObject();
        const QSize viewportSize(viewport.value(QStringLiteral("width")).toInt(),
            viewport.value(QStringLiteral("height")).toInt());
        const bool dark = scene.value(QStringLiteral("theme")).toString() == QStringLiteral("dark");
        SCOPED_TRACE(sceneId.toStdString());

        bool empty = false;
        for (const QJsonValue& coverage : scene.value(QStringLiteral("coverage")).toArray())
            empty = empty || coverage.toString() == QStringLiteral("empty");
        RepresentativeWorkspaceRepository representativeRepository;
        EmptyWorkspaceRepository emptyRepository;
        fluentqt_inspector_workbench::application::WorkspaceRepository* repository
            = &representativeRepository;
        if (empty)
            repository = &emptyRepository;
        fluentqt_inspector_workbench::application::WorkspaceController controller(*repository);
        if (dark)
            controller.toggleTheme();
        fluentqt_inspector_workbench::ui::theme::applyBrandTheme(dark);
        fluentqt_inspector_workbench::ui::MainWindow window(controller);
        window.setBackdropEffect(fluent::windowing::BackdropEffect::Solid);
        window.resize(viewportSize);
        window.show();
        QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        const int settleMs = scene.value(QStringLiteral("settle_ms")).toInt();
        if (settleMs > 0)
            QTest::qWait(settleMs);

        QWidget* content = window.contentWidget();
        ASSERT_NE(content, nullptr);
        const auto findings = fluent::diagnostics::inspectFindings(content);
        const QJsonObject budget = scene.value(QStringLiteral("inspector")).toObject();
        const int maxFindings = budget.value(QStringLiteral("max_findings")).toInt();
        QSet<QString> allowedCodes;
        for (const QJsonValue& code : budget.value(QStringLiteral("allowed_codes")).toArray())
            allowedCodes.insert(code.toString());
        const std::string report = QJsonDocument(fluent::diagnostics::Inspector::report(content))
                                       .toJson(QJsonDocument::Compact)
                                       .toStdString();

        EXPECT_LE(findings.size(), maxFindings) << report;
        for (const auto& finding : findings)
            EXPECT_TRUE(allowedCodes.contains(finding.code)) << report;
        window.close();
        QApplication::processEvents();
    }

    EXPECT_EQ(automatedSceneCount, 3);
}

} // namespace
