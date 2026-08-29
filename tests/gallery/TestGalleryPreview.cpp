#include <FluentQt/Diagnostics.h>

#include <QApplication>
#include <QEvent>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>

#include <gtest/gtest.h>

#include "components/foundation/FluentElement.h"
#include "view/preview/GalleryPreviewActions.h"
#include "view/preview/GalleryPreviewApplication.h"
#include "view/widgets/GalleryCodeBlock.h"
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
       QStringLiteral("920x680"), QStringLiteral("--actions"),
       QStringLiteral("actions.json"), QStringLiteral("--snapshot"),
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
  EXPECT_EQ(result.options.actionsPath, QStringLiteral("actions.json"));
  EXPECT_EQ(result.options.snapshotPath, QStringLiteral("preview.png"));
  EXPECT_EQ(result.options.reportPath, QStringLiteral("-"));
  EXPECT_EQ(result.options.settleMs, 40);
}

TEST(GalleryPreviewTest, ExecutesInputAndStateInteractionStepsWithAssertions) {
  QWidget root;
  root.setObjectName(QStringLiteral("interactionRoot"));
  root.resize(320, 240);
  QPushButton button(QStringLiteral("Probe"), &root);
  button.setObjectName(QStringLiteral("probeButton"));
  button.setCheckable(true);
  button.setGeometry(40, 40, 120, 40);
  QWidget editorHost(&root);
  editorHost.setObjectName(QStringLiteral("editorHost"));
  editorHost.setGeometry(40, 100, 160, 40);
  QLineEdit editor(&editorHost);
  editor.setGeometry(0, 0, 150, 32);
  root.show();
  QApplication::processEvents();

  const QJsonObject script{
      {QStringLiteral("schema_version"), 1},
      {QStringLiteral("steps"),
       QJsonArray{
           QJsonObject{{QStringLiteral("id"), QStringLiteral("focus")},
                       {QStringLiteral("action"), QStringLiteral("focus")},
                       {QStringLiteral("target"),
                        QStringLiteral("probeButton")},
                       {QStringLiteral("expect"),
                        QJsonObject{{QStringLiteral("has_focus"), true}}}},
           QJsonObject{{QStringLiteral("id"), QStringLiteral("activate")},
                       {QStringLiteral("action"), QStringLiteral("click")},
                       {QStringLiteral("target"),
                        QStringLiteral("probeButton")},
                       {QStringLiteral("expect"),
                        QJsonObject{{QStringLiteral("checked"), true}}},
                       {QStringLiteral("observe"),
                        QJsonArray{QStringLiteral("enabled")}}},
           QJsonObject{{QStringLiteral("id"), QStringLiteral("disable")},
                       {QStringLiteral("action"),
                        QStringLiteral("set_property")},
                       {QStringLiteral("target"),
                        QStringLiteral("probeButton")},
                       {QStringLiteral("property"),
                        QStringLiteral("enabled")},
                       {QStringLiteral("value"), false},
                       {QStringLiteral("expect"),
                        QJsonObject{{QStringLiteral("enabled"), false}}}},
           QJsonObject{{QStringLiteral("id"),
                        QStringLiteral("focus-descendant")},
                       {QStringLiteral("action"), QStringLiteral("focus")},
                       {QStringLiteral("target"), QStringLiteral("editorHost")},
                       {QStringLiteral("descendant_class"),
                        QStringLiteral("QLineEdit")},
                       {QStringLiteral("expect"),
                        QJsonObject{{QStringLiteral("has_focus"), true}}}},
           QJsonObject{{QStringLiteral("id"), QStringLiteral("type")},
                       {QStringLiteral("action"),
                        QStringLiteral("type_text")},
                       {QStringLiteral("target"), QStringLiteral("@focus")},
                       {QStringLiteral("text"), QStringLiteral("42")},
                       {QStringLiteral("expect"),
                        QJsonObject{{QStringLiteral("text"),
                                     QStringLiteral("42")}}}}}}};

  const auto result = fluent::gallery::executeGalleryPreviewActions(
      &root, script, QStringLiteral("memory://interaction-test"));
  EXPECT_TRUE(result.passed)
      << QJsonDocument(result.report).toJson().toStdString();
  EXPECT_EQ(result.report.value(QStringLiteral("status")).toString(),
            QStringLiteral("pass"));
  EXPECT_EQ(result.report.value(QStringLiteral("summary"))
                .toObject()
                .value(QStringLiteral("passed"))
                .toInt(),
            5);
  EXPECT_TRUE(button.isChecked());
  EXPECT_FALSE(button.isEnabled());
  EXPECT_EQ(editor.text(), QStringLiteral("42"));

  root.close();
  QApplication::processEvents();
}

TEST(GalleryPreviewTest, RejectsMissingInteractionTargetsWithoutClaimingPass) {
  QWidget root;
  root.setObjectName(QStringLiteral("interactionRoot"));
  const QJsonObject script{
      {QStringLiteral("schema_version"), 1},
      {QStringLiteral("steps"),
       QJsonArray{QJsonObject{
           {QStringLiteral("action"), QStringLiteral("click")},
           {QStringLiteral("target"), QStringLiteral("missingWidget")}}}}};

  const auto result =
      fluent::gallery::executeGalleryPreviewActions(&root, script);
  EXPECT_FALSE(result.passed);
  EXPECT_EQ(result.report.value(QStringLiteral("status")).toString(),
            QStringLiteral("fail"));
  EXPECT_TRUE(result.report.value(QStringLiteral("steps"))
                  .toArray()
                  .first()
                  .toObject()
                  .value(QStringLiteral("message"))
                  .toString()
                  .contains(QStringLiteral("missingWidget")));
}

TEST(GalleryPreviewTest, RejectsEmptyInteractionScriptWithoutClaimingPass) {
  QWidget root;
  const QJsonObject script{
      {QStringLiteral("schema_version"), 1},
      {QStringLiteral("steps"), QJsonArray{}}};

  const auto result =
      fluent::gallery::executeGalleryPreviewActions(&root, script);
  EXPECT_FALSE(result.passed);
  EXPECT_EQ(result.report.value(QStringLiteral("status")).toString(),
            QStringLiteral("fail"));
  EXPECT_TRUE(result.report.value(QStringLiteral("error"))
                  .toString()
                  .contains(QStringLiteral("must not be empty")));
}

TEST(GalleryPreviewTest, RejectsEmptyTypedTextWithoutClaimingInput) {
  QWidget root;
  QLineEdit editor(&root);
  editor.setObjectName(QStringLiteral("editor"));
  const QJsonObject script{
      {QStringLiteral("schema_version"), 1},
      {QStringLiteral("steps"),
       QJsonArray{QJsonObject{
           {QStringLiteral("action"), QStringLiteral("type_text")},
           {QStringLiteral("target"), QStringLiteral("editor")},
           {QStringLiteral("text"), QString()}}}}};

  const auto result =
      fluent::gallery::executeGalleryPreviewActions(&root, script);
  EXPECT_FALSE(result.passed);
  EXPECT_TRUE(result.report.value(QStringLiteral("steps"))
                  .toArray()
                  .first()
                  .toObject()
                  .value(QStringLiteral("message"))
                  .toString()
                  .contains(QStringLiteral("non-empty text")));
}

TEST(GalleryPreviewTest, RejectsUnsafeInteractionDelayWithoutWaiting) {
  QWidget root;
  const QJsonObject script{
      {QStringLiteral("schema_version"), 1},
      {QStringLiteral("steps"),
       QJsonArray{QJsonObject{
           {QStringLiteral("action"), QStringLiteral("wait")},
           {QStringLiteral("milliseconds"), 0},
           {QStringLiteral("after_ms"), 10001}}}}};

  const auto result =
      fluent::gallery::executeGalleryPreviewActions(&root, script);
  EXPECT_FALSE(result.passed);
  EXPECT_TRUE(result.report.value(QStringLiteral("steps"))
                  .toArray()
                  .first()
                  .toObject()
                  .value(QStringLiteral("message"))
                  .toString()
                  .contains(QStringLiteral("after_ms")));

  const QJsonObject fractionalWaitScript{
      {QStringLiteral("schema_version"), 1},
      {QStringLiteral("steps"),
       QJsonArray{QJsonObject{
           {QStringLiteral("action"), QStringLiteral("wait")},
           {QStringLiteral("milliseconds"), 0.5}}}}};
  const auto fractionalWaitResult =
      fluent::gallery::executeGalleryPreviewActions(&root,
                                                    fractionalWaitScript);
  EXPECT_FALSE(fractionalWaitResult.passed);
  EXPECT_TRUE(fractionalWaitResult.report.value(QStringLiteral("steps"))
                  .toArray()
                  .first()
                  .toObject()
                  .value(QStringLiteral("message"))
                  .toString()
                  .contains(QStringLiteral("milliseconds")));
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
  ASSERT_NE(window.sampleCard()->codeBlock(), nullptr);
  EXPECT_FALSE(window.sampleCard()->codeBlock()->hasPythonCode());
  EXPECT_EQ(window.sampleCard()->codeBlock()->languageSelector(), nullptr);

  const QJsonObject report =
      fluent::gallery::galleryPreviewReport(&window, options);
  EXPECT_EQ(report.value(QStringLiteral("schema_version")).toInt(), 2);
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
  EXPECT_EQ(environment.value(QStringLiteral("fingerprint_schema_version"))
                .toInt(),
            1);
  EXPECT_FALSE(
      environment.value(QStringLiteral("qt_version")).toString().isEmpty());
  EXPECT_GT(environment.value(QStringLiteral("device_pixel_ratio")).toDouble(),
            0.0);
  EXPECT_GT(environment.value(QStringLiteral("logical_dpi_x")).toDouble(),
            0.0);
  EXPECT_FALSE(environment.value(QStringLiteral("font"))
                   .toObject()
                   .value(QStringLiteral("family"))
                   .toString()
                   .isEmpty());
  EXPECT_FALSE(environment.value(QStringLiteral("system"))
                   .toObject()
                   .value(QStringLiteral("cpu_architecture"))
                   .toString()
                   .isEmpty());
  EXPECT_GT(environment.value(QStringLiteral("screen"))
                .toObject()
                .value(QStringLiteral("depth"))
                .toInt(),
            0);

  const QJsonObject geometry =
      report.value(QStringLiteral("geometry_report")).toObject();
  EXPECT_EQ(geometry.value(QStringLiteral("schema_version")).toInt(), 1);
  EXPECT_GT(geometry.value(QStringLiteral("widget_count")).toInt(), 0);
  bool foundSampleCard = false;
  bool foundPreviewWidget = false;
  for (const QJsonValue &value :
       geometry.value(QStringLiteral("widgets")).toArray()) {
    const QString objectName =
        value.toObject().value(QStringLiteral("object_name")).toString();
    foundSampleCard |= objectName == QStringLiteral("gallerySampleCard");
    foundPreviewWidget |=
        objectName == QStringLiteral("gallerySamplePreviewWidget");
  }
  EXPECT_TRUE(foundSampleCard);
  EXPECT_TRUE(foundPreviewWidget);

  window.close();
  QApplication::processEvents();
  qApp->setLayoutDirection(previousDirection);
  fluent::FluentElement::setTheme(previousTheme);
}

} // namespace
