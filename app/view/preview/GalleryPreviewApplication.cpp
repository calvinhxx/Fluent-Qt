#include "GalleryPreviewApplication.h"

#include <FluentQt/Diagnostics.h>

#include <QApplication>
#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QLayout>
#include <QPalette>
#include <QPixmap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

#include <cstddef>
#include <cstdio>

#include "compatibility/QtCompat.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/scrolling/ScrollView.h"
#include "view/widgets/GallerySampleCard.h"
#include "view/widgets/GallerySampleCatalog.h"

namespace fluent::gallery {
namespace {

constexpr int kMinimumPreviewWidth = 320;
constexpr int kMinimumPreviewHeight = 240;
constexpr int kMaximumPreviewWidth = 3840;
constexpr int kMaximumPreviewHeight = 2160;
constexpr int kPreviewInset = 16;
constexpr int kPreviewReportSchemaVersion = 1;

void configurePreviewParser(QCommandLineParser &parser) {
  parser.setApplicationDescription(
      QStringLiteral("Render one real FluentQt Gallery sample without starting "
                     "the full Gallery shell."));
  parser.addHelpOption();
  parser.addOption(QCommandLineOption(
      QStringLiteral("preview"),
      QStringLiteral("Run the isolated Gallery sample preview host.")));
  parser.addOption(QCommandLineOption(
      QStringLiteral("route"), QStringLiteral("Gallery component route id."),
      QStringLiteral("route-id")));
  parser.addOption(QCommandLineOption(
      QStringLiteral("sample"),
      QStringLiteral(
          "Gallery sample id; defaults to the first sample for the route."),
      QStringLiteral("sample-id")));
  parser.addOption(QCommandLineOption(
      QStringLiteral("theme"),
      QStringLiteral("Preview theme: light, dark, or system."),
      QStringLiteral("theme"), QStringLiteral("light")));
  parser.addOption(QCommandLineOption(
      QStringLiteral("rtl"),
      QStringLiteral("Render using right-to-left layout direction.")));
  parser.addOption(QCommandLineOption(
      QStringLiteral("size"),
      QStringLiteral("Preview viewport size as WIDTHxHEIGHT."),
      QStringLiteral("size"), QStringLiteral("800x640")));
  parser.addOption(QCommandLineOption(
      QStringLiteral("snapshot"),
      QStringLiteral("Write the settled preview window to a PNG file."),
      QStringLiteral("path")));
  parser.addOption(QCommandLineOption(
      QStringLiteral("report"),
      QStringLiteral(
          "Write a versioned Inspector JSON report; use '-' for stdout."),
      QStringLiteral("path")));
  parser.addOption(QCommandLineOption(
      QStringLiteral("settle-ms"),
      QStringLiteral(
          "Milliseconds to wait before snapshot/report capture (0-10000)."),
      QStringLiteral("milliseconds"), QStringLiteral("250")));
  parser.addOption(QCommandLineOption(
      QStringLiteral("keep-open"),
      QStringLiteral(
          "Keep the preview open after writing requested artifacts.")));
}

bool parseViewportSize(const QString &text, QSize &size) {
  static const QRegularExpression expression(
      QStringLiteral("^([1-9][0-9]*)[xX]([1-9][0-9]*)$"));
  const QRegularExpressionMatch match = expression.match(text.trimmed());
  if (!match.hasMatch())
    return false;

  bool widthOk = false;
  bool heightOk = false;
  const int width = match.captured(1).toInt(&widthOk);
  const int height = match.captured(2).toInt(&heightOk);
  if (!widthOk || !heightOk || width < kMinimumPreviewWidth ||
      width > kMaximumPreviewWidth || height < kMinimumPreviewHeight ||
      height > kMaximumPreviewHeight) {
    return false;
  }
  size = QSize(width, height);
  return true;
}

GalleryPreviewTheme parseTheme(const QString &text, bool &ok) {
  const QString normalized = text.trimmed().toLower();
  ok = true;
  if (normalized == QStringLiteral("system"))
    return GalleryPreviewTheme::System;
  if (normalized == QStringLiteral("light"))
    return GalleryPreviewTheme::Light;
  if (normalized == QStringLiteral("dark"))
    return GalleryPreviewTheme::Dark;
  ok = false;
  return GalleryPreviewTheme::Light;
}

FluentElement::Theme systemTheme() {
  const FluentSystemColorScheme scheme = fluentSystemColorScheme();
  if (scheme == FluentSystemColorScheme::Dark)
    return FluentElement::Dark;
  if (scheme == FluentSystemColorScheme::Light)
    return FluentElement::Light;

  if (qApp) {
    const QPalette palette = qApp->palette();
    if (palette.color(QPalette::Window).lightness() <
        palette.color(QPalette::WindowText).lightness()) {
      return FluentElement::Dark;
    }
  }
  return FluentElement::Light;
}

FluentElement::Theme resolvedTheme(GalleryPreviewTheme theme) {
  if (theme == GalleryPreviewTheme::Dark)
    return FluentElement::Dark;
  if (theme == GalleryPreviewTheme::System)
    return systemTheme();
  return FluentElement::Light;
}

QString absoluteArtifactPath(const QString &path) {
  if (path.isEmpty() || path == QStringLiteral("-"))
    return path;
  return QFileInfo(path).absoluteFilePath();
}

bool ensureParentDirectory(const QString &path, QString &error) {
  const QFileInfo info(path);
  QDir directory = info.dir();
  if (directory.exists() || directory.mkpath(QStringLiteral(".")))
    return true;
  error = QStringLiteral("Could not create artifact directory: %1")
              .arg(directory.absolutePath());
  return false;
}

bool writeJson(const QString &path, const QJsonObject &object, QString &error) {
  const QByteArray payload =
      QJsonDocument(object).toJson(QJsonDocument::Indented);
  if (path == QStringLiteral("-")) {
    const std::size_t written =
        std::fwrite(payload.constData(), 1,
                    static_cast<std::size_t>(payload.size()), stdout);
    std::fflush(stdout);
    if (written == static_cast<std::size_t>(payload.size()))
      return true;
    error = QStringLiteral("Could not write preview JSON to stdout.");
    return false;
  }

  if (!ensureParentDirectory(path, error))
    return false;
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    error = QStringLiteral("Could not open JSON artifact %1: %2")
                .arg(path, file.errorString());
    return false;
  }
  if (file.write(payload) != payload.size()) {
    error = QStringLiteral("Could not write JSON artifact %1: %2")
                .arg(path, file.errorString());
    file.cancelWriting();
    return false;
  }
  if (!file.commit()) {
    error = QStringLiteral("Could not commit JSON artifact %1: %2")
                .arg(path, file.errorString());
    return false;
  }
  return true;
}

bool writeSnapshot(GalleryPreviewWindow *window, const QString &path,
                   QString &resolvedPath, QString &error) {
  resolvedPath = absoluteArtifactPath(path);
  if (!window || resolvedPath.isEmpty()) {
    error = QStringLiteral("Snapshot path or preview window is missing.");
    return false;
  }
  if (!ensureParentDirectory(resolvedPath, error))
    return false;

  const QPixmap snapshot = window->grab();
  if (snapshot.isNull()) {
    error = QStringLiteral("Preview window returned an empty snapshot.");
    return false;
  }
  QSaveFile file(resolvedPath);
  if (!file.open(QIODevice::WriteOnly)) {
    error = QStringLiteral("Could not open snapshot %1: %2")
                .arg(resolvedPath, file.errorString());
    return false;
  }
  if (!snapshot.save(&file, "PNG")) {
    error =
        QStringLiteral("Could not encode snapshot PNG: %1").arg(resolvedPath);
    file.cancelWriting();
    return false;
  }
  if (!file.commit()) {
    error = QStringLiteral("Could not commit snapshot %1: %2")
                .arg(resolvedPath, file.errorString());
    return false;
  }
  return true;
}

void writeStandardError(const QString &message) {
  const QByteArray local = message.toLocal8Bit();
  std::fprintf(stderr, "%s\n", local.constData());
}

} // namespace

GalleryPreviewParseResult
parseGalleryPreviewArguments(const QStringList &arguments) {
  GalleryPreviewParseResult result;
  const bool requested = arguments.contains(QStringLiteral("--preview"));
  result.options.requested = requested;
  if (!requested)
    return result;

  QCommandLineParser parser;
  configurePreviewParser(parser);
  if (!parser.parse(arguments)) {
    result.error = parser.errorText();
    result.helpText = parser.helpText();
    return result;
  }
  result.helpText = parser.helpText();
  result.options.helpRequested = parser.isSet(QStringLiteral("help"));
  if (result.options.helpRequested)
    return result;

  result.options.routeId = parser.value(QStringLiteral("route")).trimmed();
  result.options.sampleId = parser.value(QStringLiteral("sample")).trimmed();
  if (result.options.routeId.isEmpty()) {
    result.error = QStringLiteral(
        "--route is required for Gallery preview.");
    return result;
  }

  bool themeOk = false;
  result.options.theme =
      parseTheme(parser.value(QStringLiteral("theme")), themeOk);
  if (!themeOk) {
    result.error = QStringLiteral("--theme must be light, dark, or system.");
    return result;
  }
  if (!parseViewportSize(parser.value(QStringLiteral("size")),
                         result.options.viewportSize)) {
    result.error = QStringLiteral(
        "--size must be WIDTHxHEIGHT within 320x240 and 3840x2160.");
    return result;
  }

  bool settleOk = false;
  result.options.settleMs =
      parser.value(QStringLiteral("settle-ms")).toInt(&settleOk);
  if (!settleOk || result.options.settleMs < 0 ||
      result.options.settleMs > 10000) {
    result.error =
        QStringLiteral("--settle-ms must be an integer from 0 to 10000.");
    return result;
  }
  result.options.rightToLeft = parser.isSet(QStringLiteral("rtl"));
  result.options.snapshotPath =
      parser.value(QStringLiteral("snapshot")).trimmed();
  result.options.reportPath = parser.value(QStringLiteral("report")).trimmed();
  result.options.keepOpen = parser.isSet(QStringLiteral("keep-open"));
  return result;
}

GalleryPreviewSelection
resolveGalleryPreviewSelection(const QString &routeId,
                               const QString &sampleId) {
  GalleryPreviewSelection selection;
  const QVector<GallerySample> samples = gallerySamplesForRoute(routeId);
  for (const GallerySample &sample : samples)
    selection.availableSampleIds.append(sample.id);

  if (samples.isEmpty()) {
    selection.error =
        galleryContentEntry(routeId)
            ? QStringLiteral("Gallery route '%1' does not expose live samples.")
                  .arg(routeId)
            : QStringLiteral("Unknown Gallery route '%1'.").arg(routeId);
    return selection;
  }

  if (sampleId.isEmpty()) {
    selection.sample = samples.first();
    return selection;
  }
  for (const GallerySample &sample : samples) {
    if (sample.id == sampleId) {
      selection.sample = sample;
      return selection;
    }
  }
  selection.error =
      QStringLiteral("Unknown sample '%1' for route '%2'. Available: %3")
          .arg(sampleId, routeId,
               selection.availableSampleIds.join(QStringLiteral(", ")));
  return selection;
}

QString galleryPreviewThemeName(GalleryPreviewTheme theme) {
  if (theme == GalleryPreviewTheme::Dark)
    return QStringLiteral("dark");
  if (theme == GalleryPreviewTheme::System)
    return QStringLiteral("system");
  return QStringLiteral("light");
}

GalleryPreviewWindow::GalleryPreviewWindow(const GalleryPreviewOptions &options,
                                           const GallerySample &sample,
                                           QWidget *parent)
    : QWidget(parent), m_routeId(options.routeId), m_sampleId(sample.id) {
  setObjectName(QStringLiteral("galleryPreviewWindow"));
  setProperty("galleryPreviewRouteId", m_routeId);
  setProperty("galleryPreviewSampleId", m_sampleId);
  setAccessibleName(QStringLiteral("FluentQt Gallery sample preview"));
  setWindowTitle(
      QStringLiteral("%1 · %2 · FluentQt Preview").arg(m_routeId, m_sampleId));
  setMinimumSize(kMinimumPreviewWidth, kMinimumPreviewHeight);
  resize(options.viewportSize);
  setLayoutDirection(options.rightToLeft ? Qt::RightToLeft : Qt::LeftToRight);
  setAutoFillBackground(true);

  auto *outerLayout = new QVBoxLayout(this);
  outerLayout->setContentsMargins(0, 0, 0, 0);
  outerLayout->setSpacing(0);

  auto *scrollView = new fluent::scrolling::ScrollView(this);
  scrollView->setObjectName(QStringLiteral("galleryPreviewScrollView"));
  scrollView->setAccessibleName(
      QStringLiteral("Gallery sample preview canvas"));
  scrollView->setProperty("scrollChainingEnabled", false);
  scrollView->setWidgetResizable(true);
  scrollView->setHorizontalScrollMode(
      fluent::scrolling::ScrollView::ScrollMode::Disabled);
  scrollView->setHorizontalScrollBarVisibility(
      fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);
  scrollView->setVerticalScrollBarVisibility(
      fluent::scrolling::ScrollView::ScrollBarVisibility::Auto);

  m_canvas = new QWidget(scrollView);
  m_canvas->setObjectName(QStringLiteral("galleryPreviewCanvas"));
  m_canvas->setAutoFillBackground(true);
  auto *canvasLayout = new QVBoxLayout(m_canvas);
  canvasLayout->setContentsMargins(kPreviewInset, kPreviewInset, kPreviewInset,
                                   kPreviewInset);
  canvasLayout->setSpacing(0);
  canvasLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

  m_sampleCard = new GallerySampleCard(m_routeId, sample, m_canvas);
  m_sampleCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  canvasLayout->addWidget(m_sampleCard, 0, Qt::AlignTop);
  canvasLayout->addStretch(1);

  scrollView->setWidget(m_canvas);
  outerLayout->addWidget(scrollView);
  applyPalette();
}

void GalleryPreviewWindow::onThemeUpdated() {
  applyPalette();
  update();
}

void GalleryPreviewWindow::applyPalette() {
  const auto &colors = themeColorsRef();
  QPalette windowPalette = palette();
  windowPalette.setColor(QPalette::Window, colors.bgCanvas);
  windowPalette.setColor(QPalette::Base, colors.bgCanvas);
  setPalette(windowPalette);
  if (m_canvas) {
    QPalette canvasPalette = m_canvas->palette();
    canvasPalette.setColor(QPalette::Window, colors.bgCanvas);
    canvasPalette.setColor(QPalette::Base, colors.bgCanvas);
    m_canvas->setPalette(canvasPalette);
    m_canvas->update();
  }
}

QJsonObject galleryPreviewReport(GalleryPreviewWindow *window,
                                 const GalleryPreviewOptions &options,
                                 const QString &snapshotPath,
                                 const QString &snapshotError) {
  const bool snapshotRequested = !options.snapshotPath.isEmpty();
  const bool snapshotWritten =
      snapshotRequested && snapshotError.isEmpty() && !snapshotPath.isEmpty();
  const QString resolvedThemeName =
      FluentElement::currentTheme() == FluentElement::Dark
          ? QStringLiteral("dark")
          : QStringLiteral("light");
  QJsonObject artifacts{
      {QStringLiteral("snapshot"),
       QJsonObject{{QStringLiteral("requested"), snapshotRequested},
                   {QStringLiteral("written"), snapshotWritten},
                   {QStringLiteral("path"), snapshotPath},
                   {QStringLiteral("error"), snapshotError}}}};

  return {
      {QStringLiteral("schema_version"), kPreviewReportSchemaVersion},
      {QStringLiteral("tool"), QStringLiteral("FluentQt Gallery Preview")},
      {QStringLiteral("status"), snapshotError.isEmpty()
                                     ? QStringLiteral("ok")
                                     : QStringLiteral("artifact-error")},
      {QStringLiteral("selection"),
       QJsonObject{{QStringLiteral("route"), options.routeId},
                   {QStringLiteral("sample"),
                    window ? window->sampleId() : QString()}}},
      {QStringLiteral("scene"),
       QJsonObject{
           {QStringLiteral("requested_theme"),
            galleryPreviewThemeName(options.theme)},
           {QStringLiteral("theme"), resolvedThemeName},
           {QStringLiteral("layout_direction"), options.rightToLeft
                                                    ? QStringLiteral("rtl")
                                                    : QStringLiteral("ltr")},
           {QStringLiteral("settle_ms"), options.settleMs},
           {QStringLiteral("requested_width"), options.viewportSize.width()},
           {QStringLiteral("requested_height"), options.viewportSize.height()},
           {QStringLiteral("actual_width"), window ? window->width() : 0},
           {QStringLiteral("actual_height"), window ? window->height() : 0}}},
      {QStringLiteral("environment"),
       QJsonObject{
           {QStringLiteral("qt_version"), QString::fromLatin1(qVersion())},
           {QStringLiteral("platform_plugin"), QGuiApplication::platformName()},
           {QStringLiteral("device_pixel_ratio"),
            window ? window->devicePixelRatioF() : 0.0}}},
      {QStringLiteral("artifacts"), artifacts},
      {QStringLiteral("quality_report"),
       window ? diagnostics::Inspector::report(window) : QJsonObject{}}};
}

int runGalleryPreviewApplication(QApplication &app,
                                 const GalleryPreviewOptions &options) {
  const GalleryPreviewSelection selection =
      resolveGalleryPreviewSelection(options.routeId, options.sampleId);
  if (!selection.isValid()) {
    writeStandardError(selection.error);
    return 3;
  }
  ThemeRegistry::instance().resetToDefaults();
  FluentElement::setTheme(resolvedTheme(options.theme));
  app.setLayoutDirection(options.rightToLeft ? Qt::RightToLeft
                                             : Qt::LeftToRight);
  app.setQuitOnLastWindowClosed(true);

  GalleryPreviewWindow window(options, selection.sample);
  window.show();

  const bool hasArtifacts = !options.snapshotPath.isEmpty() ||
                            !options.reportPath.isEmpty();
  int artifactExitCode = 0;
  if (hasArtifacts) {
    QTimer::singleShot(options.settleMs, &window, [&]() {
      QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
      QApplication::processEvents(QEventLoop::AllEvents, 50);

      QString snapshotPath;
      QString snapshotError;
      if (!options.snapshotPath.isEmpty() &&
          !writeSnapshot(&window, options.snapshotPath, snapshotPath,
                         snapshotError)) {
        artifactExitCode = 4;
        writeStandardError(snapshotError);
      }

      if (!options.reportPath.isEmpty()) {
        const QJsonObject report =
            galleryPreviewReport(&window, options, snapshotPath, snapshotError);
        QString reportError;
        const QString reportPath = absoluteArtifactPath(options.reportPath);
        if (!writeJson(reportPath, report, reportError)) {
          artifactExitCode = 5;
          writeStandardError(reportError);
        }
      }

      if (!options.keepOpen)
        app.exit(artifactExitCode);
    });
  }

  const int eventLoopExitCode = app.exec();
  return artifactExitCode != 0 ? artifactExitCode : eventLoopExitCode;
}

} // namespace fluent::gallery
