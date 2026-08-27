#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>

#include <gtest/gtest.h>

#ifndef FLUENT_QT_GALLERY_PREVIEW_PATH
#define FLUENT_QT_GALLERY_PREVIEW_PATH ""
#endif

namespace {

TEST(GalleryPreviewCliTest, RendersTargetedSampleAndPrintsJsonReport) {
  QProcess process;
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("QT_QPA_PLATFORM"),
                     QStringLiteral("offscreen"));
  environment.insert(QStringLiteral("SPDLOG_LEVEL"), QStringLiteral("off"));
  process.setProcessEnvironment(environment);
  process.setProgram(QString::fromUtf8(FLUENT_QT_GALLERY_PREVIEW_PATH));
  process.setArguments({QStringLiteral("--preview"), QStringLiteral("--route"),
                        QStringLiteral("button"), QStringLiteral("--sample"),
                        QStringLiteral("button-styles"),
                        QStringLiteral("--theme"), QStringLiteral("dark"),
                        QStringLiteral("--rtl"), QStringLiteral("--size"),
                        QStringLiteral("720x540"),
                        QStringLiteral("--settle-ms"), QStringLiteral("0"),
                        QStringLiteral("--report"), QStringLiteral("-")});

  process.start();
  ASSERT_TRUE(process.waitForStarted(10000))
      << process.errorString().toStdString();
  ASSERT_TRUE(process.waitForFinished(30000))
      << process.errorString().toStdString();
  EXPECT_EQ(process.exitStatus(), QProcess::NormalExit);
  EXPECT_EQ(process.exitCode(), 0)
      << process.readAllStandardError().toStdString();

  QJsonParseError parseError;
  const QJsonDocument document =
      QJsonDocument::fromJson(process.readAllStandardOutput(), &parseError);
  ASSERT_EQ(parseError.error, QJsonParseError::NoError)
      << parseError.errorString().toStdString();
  ASSERT_TRUE(document.isObject());
  const QJsonObject root = document.object();
  EXPECT_EQ(root.value(QStringLiteral("schema_version")).toInt(), 1);
  EXPECT_EQ(root.value(QStringLiteral("status")).toString(),
            QStringLiteral("ok"));
  const QJsonObject selection =
      root.value(QStringLiteral("selection")).toObject();
  EXPECT_EQ(selection.value(QStringLiteral("route")).toString(),
            QStringLiteral("button"));
  EXPECT_EQ(selection.value(QStringLiteral("sample")).toString(),
            QStringLiteral("button-styles"));
  const QJsonObject scene = root.value(QStringLiteral("scene")).toObject();
  EXPECT_EQ(scene.value(QStringLiteral("theme")).toString(),
            QStringLiteral("dark"));
  EXPECT_EQ(scene.value(QStringLiteral("layout_direction")).toString(),
            QStringLiteral("rtl"));
  EXPECT_EQ(scene.value(QStringLiteral("actual_width")).toInt(), 720);
  EXPECT_EQ(scene.value(QStringLiteral("actual_height")).toInt(), 540);
  const QJsonObject reportEnvironment =
      root.value(QStringLiteral("environment")).toObject();
  EXPECT_EQ(
      reportEnvironment.value(QStringLiteral("platform_plugin")).toString(),
      QStringLiteral("offscreen"));
  EXPECT_GT(
      reportEnvironment.value(QStringLiteral("device_pixel_ratio")).toDouble(),
      0.0);
  EXPECT_EQ(root.value(QStringLiteral("quality_report"))
                .toObject()
                .value(QStringLiteral("schema_version"))
                .toInt(),
            1);
}

} // namespace
