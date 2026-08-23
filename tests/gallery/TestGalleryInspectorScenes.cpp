#include <gtest/gtest.h>

#include <functional>

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTest>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/windowing/Window.h"
#include "utils/private/Inspector_p.h"
#include "view/pages/GalleryContentPage.h"
#include "view/shell/GalleryWindow.h"
#include "viewmodel/GallerySettings.h"

namespace {

using fluent::diagnostics::Inspector;
using fluent::gallery::GalleryContentPage;
using fluent::gallery::GalleryWindow;

constexpr auto GalleryApplicationId = "fluent_qt_gallery";

bool waitUntil(const std::function<bool()>& condition, int timeoutMs = 7000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
        QTest::qWait(20);
    }
    return condition();
}

TEST(GalleryInspectorScenesTest, ManifestDrivenInspectorAcceptance)
{
    auto& settings = fluent::gallery::GallerySettings::instance();
    settings.setIntroCompleted(true);
    const QVariant previousAutomatedProperty = qApp->property("fluentqtGalleryAutomated");
    struct RestoreApplicationProperty final {
        QVariant value;
        ~RestoreApplicationProperty() { qApp->setProperty("fluentqtGalleryAutomated", value); }
    } restoreApplicationProperty { previousAutomatedProperty };
    qApp->setProperty("fluentqtGalleryAutomated", true);

    QFile manifestFile(QString::fromUtf8(FLUENT_QT_APPLICATION_SCENES_PATH));
    ASSERT_TRUE(manifestFile.open(QIODevice::ReadOnly));
    QJsonParseError parseError;
    const QJsonDocument manifestDocument
        = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    ASSERT_EQ(parseError.error, QJsonParseError::NoError);
    ASSERT_TRUE(manifestDocument.isObject());
    const QJsonArray scenes = manifestDocument.object().value(QStringLiteral("scenes")).toArray();
    ASSERT_FALSE(scenes.isEmpty());

    GalleryWindow window;
    window.setBackdropEffect(fluent::windowing::BackdropEffect::Solid);
    window.resize(1180, 760);
    window.show();
    ASSERT_TRUE(waitUntil([&window]() {
        return window.findChild<QWidget*>(QStringLiteral("gallerySplashScreen")) == nullptr;
    }));

    int automatedSceneCount = 0;
    for (const QJsonValue& sceneValue : scenes) {
        const QJsonObject scene = sceneValue.toObject();
        if (scene.value(QStringLiteral("application")).toString()
                != QString::fromLatin1(GalleryApplicationId)
            || scene.value(QStringLiteral("automation")).toString()
                != QStringLiteral("automated")) {
            continue;
        }
        ++automatedSceneCount;
        const QString sceneId = scene.value(QStringLiteral("id")).toString();
        const QString route = scene.value(QStringLiteral("route")).toString();
        const QJsonObject viewport = scene.value(QStringLiteral("viewport")).toObject();
        const QSize viewportSize(viewport.value(QStringLiteral("width")).toInt(),
            viewport.value(QStringLiteral("height")).toInt());
        const bool dark = scene.value(QStringLiteral("theme")).toString() == QStringLiteral("dark");
        const auto theme = dark ? fluent::FluentElement::Dark : fluent::FluentElement::Light;

        SCOPED_TRACE(sceneId.toStdString());
        window.resize(viewportSize);
        fluent::FluentElement::setTheme(theme);
        ASSERT_TRUE(window.selectRoute(route));
        ASSERT_TRUE(waitUntil([&window, &route]() {
            return window.currentRouteId() == route && window.currentContentPage() != nullptr;
        }));
        QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        const int settleMs = scene.value(QStringLiteral("settle_ms")).toInt();
        if (settleMs > 0)
            QTest::qWait(settleMs);

        GalleryContentPage* page = window.currentContentPage();
        ASSERT_NE(page, nullptr);
        const auto findings = fluent::diagnostics::inspectFindings(page);
        const QJsonObject budget = scene.value(QStringLiteral("inspector")).toObject();
        const int maxFindings = budget.value(QStringLiteral("max_findings")).toInt();
        QSet<QString> allowedCodes;
        for (const QJsonValue& code : budget.value(QStringLiteral("allowed_codes")).toArray())
            allowedCodes.insert(code.toString());
        const std::string report
            = QJsonDocument(Inspector::report(page)).toJson(QJsonDocument::Compact).toStdString();

        EXPECT_LE(findings.size(), maxFindings) << report;
        for (const auto& finding : findings)
            EXPECT_TRUE(allowedCodes.contains(finding.code)) << report;
    }
    EXPECT_GT(automatedSceneCount, 0);
}

} // namespace
