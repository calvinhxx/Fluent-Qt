#include <FluentQt/Diagnostics.h>

#include <QApplication>
#include <QEvent>
#include <QEventLoop>
#include <QJsonObject>

#include <gtest/gtest.h>

#include "components/foundation/FluentElement.h"
#include "view/preview/GalleryPreviewApplication.h"
#include "view/widgets/GallerySampleCard.h"

namespace {

using fluent::gallery::GalleryPreviewOptions;
using fluent::gallery::GalleryPreviewTheme;
using fluent::gallery::GalleryPreviewWindow;
using fluent::gallery::parseGalleryPreviewArguments;
using fluent::gallery::resolveGalleryPreviewSelection;

TEST(GalleryPreviewTest, DoesNotClaimNormalGalleryStartup) {
  const auto noFlags = parseGalleryPreviewArguments(
      {QStringLiteral("fluent_qt_gallery")});
  EXPECT_TRUE(noFlags.isValid());
  EXPECT_FALSE(noFlags.options.requested);
  EXPECT_FALSE(noFlags.options.helpRequested);

  const auto unrelatedArguments = parseGalleryPreviewArguments(
      {QStringLiteral("fluent_qt_gallery"), QStringLiteral("--style"),
       QStringLiteral("fusion"), QStringLiteral("--route"),
       QStringLiteral("button")});
  EXPECT_TRUE(unrelatedArguments.isValid());
  EXPECT_FALSE(unrelatedArguments.options.requested);
  EXPECT_TRUE(unrelatedArguments.error.isEmpty());
}

TEST(GalleryPreviewTest, ParsesDeterministicSceneAndArtifactArguments) {
  const auto result = parseGalleryPreviewArguments(
      {QStringLiteral("fluent_qt_gallery"), QStringLiteral("--preview"),
       QStringLiteral("--route"), QStringLiteral("button"),
       QStringLiteral("--sample"), QStringLiteral("button-styles"),
       QStringLiteral("--theme"), QStringLiteral("dark"),
       QStringLiteral("--rtl"), QStringLiteral("--size"),
       QStringLiteral("920x680"), QStringLiteral("--snapshot"),
       QStringLiteral("preview.png"), QStringLiteral("--report"),
       QStringLiteral("-"), QStringLiteral("--settle-ms"),
       QStringLiteral("40")});

  ASSERT_TRUE(result.isValid()) << result.error.toStdString();
  EXPECT_TRUE(result.options.requested);
  EXPECT_EQ(result.options.routeId, QStringLiteral("button"));
  EXPECT_EQ(result.options.sampleId, QStringLiteral("button-styles"));
  EXPECT_EQ(result.options.theme, GalleryPreviewTheme::Dark);
  EXPECT_TRUE(result.options.rightToLeft);
  EXPECT_EQ(result.options.viewportSize, QSize(920, 680));
  EXPECT_EQ(result.options.snapshotPath, QStringLiteral("preview.png"));
  EXPECT_EQ(result.options.reportPath, QStringLiteral("-"));
  EXPECT_EQ(result.options.settleMs, 40);
}

TEST(GalleryPreviewTest, RejectsInvalidThemeSizeAndMissingRoute) {
  const auto missingRoute = parseGalleryPreviewArguments(
      {QStringLiteral("fluent_qt_gallery"), QStringLiteral("--preview")});
  EXPECT_FALSE(missingRoute.isValid());
  EXPECT_TRUE(missingRoute.error.contains(QStringLiteral("--route")));

  const auto invalidTheme = parseGalleryPreviewArguments(
      {QStringLiteral("fluent_qt_gallery"), QStringLiteral("--preview"),
       QStringLiteral("--route"), QStringLiteral("button"),
       QStringLiteral("--theme"), QStringLiteral("sepia")});
  EXPECT_FALSE(invalidTheme.isValid());
  EXPECT_TRUE(invalidTheme.error.contains(QStringLiteral("--theme")));

  const auto invalidSize = parseGalleryPreviewArguments(
      {QStringLiteral("fluent_qt_gallery"), QStringLiteral("--preview"),
       QStringLiteral("--route"), QStringLiteral("button"),
       QStringLiteral("--size"), QStringLiteral("200x100")});
  EXPECT_FALSE(invalidSize.isValid());
  EXPECT_TRUE(invalidSize.error.contains(QStringLiteral("--size")));
}

TEST(GalleryPreviewTest, ResolvesDefaultAndNamedSamplesWithActionableErrors) {
  const auto defaultSelection =
      resolveGalleryPreviewSelection(QStringLiteral("button"), QString());
  ASSERT_TRUE(defaultSelection.isValid())
      << defaultSelection.error.toStdString();
  EXPECT_EQ(defaultSelection.sample.id, QStringLiteral("button-styles"));
  EXPECT_TRUE(defaultSelection.availableSampleIds.contains(
      QStringLiteral("button-sizes")));

  const auto namedSelection = resolveGalleryPreviewSelection(
      QStringLiteral("button"), QStringLiteral("button-sizes"));
  ASSERT_TRUE(namedSelection.isValid()) << namedSelection.error.toStdString();
  EXPECT_EQ(namedSelection.sample.id, QStringLiteral("button-sizes"));

  const auto missingSelection = resolveGalleryPreviewSelection(
      QStringLiteral("button"), QStringLiteral("missing-sample"));
  EXPECT_FALSE(missingSelection.isValid());
  EXPECT_TRUE(missingSelection.error.contains(QStringLiteral("button-styles")));
}

TEST(GalleryPreviewTest, HostsOneRealCardAndBuildsVersionedInspectorReport) {
  const auto selection = resolveGalleryPreviewSelection(
      QStringLiteral("button"), QStringLiteral("button-styles"));
  ASSERT_TRUE(selection.isValid()) << selection.error.toStdString();

  const auto previousTheme = fluent::FluentElement::currentTheme();
  const auto previousDirection = qApp->layoutDirection();
  fluent::FluentElement::setTheme(fluent::FluentElement::Dark);

  GalleryPreviewOptions options;
  options.requested = true;
  options.routeId = QStringLiteral("button");
  options.sampleId = QStringLiteral("button-styles");
  options.theme = GalleryPreviewTheme::Dark;
  options.rightToLeft = true;
  options.viewportSize = QSize(720, 540);

  GalleryPreviewWindow window(options, selection.sample);
  window.show();
  QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
  QApplication::processEvents(QEventLoop::AllEvents, 50);

  ASSERT_NE(window.sampleCard(), nullptr);
  EXPECT_EQ(window.size(), options.viewportSize);
  EXPECT_EQ(window.layoutDirection(), Qt::RightToLeft);
  EXPECT_EQ(window.sampleCard()->sampleId(), QStringLiteral("button-styles"));
  EXPECT_NE(window.sampleCard()->previewWidget(), nullptr);

  const QJsonObject report =
      fluent::gallery::galleryPreviewReport(&window, options);
  EXPECT_EQ(report.value(QStringLiteral("schema_version")).toInt(), 1);
  EXPECT_EQ(report.value(QStringLiteral("status")).toString(),
            QStringLiteral("ok"));
  const QJsonObject selectionObject =
      report.value(QStringLiteral("selection")).toObject();
  EXPECT_EQ(selectionObject.value(QStringLiteral("route")).toString(),
            QStringLiteral("button"));
  EXPECT_EQ(selectionObject.value(QStringLiteral("sample")).toString(),
            QStringLiteral("button-styles"));
  const QJsonObject quality =
      report.value(QStringLiteral("quality_report")).toObject();
  EXPECT_EQ(quality.value(QStringLiteral("schema_version")).toInt(),
            fluent::diagnostics::Inspector::ReportSchemaVersion);
  const QJsonObject environment =
      report.value(QStringLiteral("environment")).toObject();
  EXPECT_FALSE(
      environment.value(QStringLiteral("qt_version")).toString().isEmpty());
  EXPECT_GT(environment.value(QStringLiteral("device_pixel_ratio")).toDouble(),
            0.0);

  window.close();
  QApplication::processEvents();
  qApp->setLayoutDirection(previousDirection);
  fluent::FluentElement::setTheme(previousTheme);
}

} // namespace
