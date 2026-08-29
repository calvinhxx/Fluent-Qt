#include "GalleryPreviewApplication.h"

#include <FluentQt/Diagnostics.h>

#include <QApplication>
#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QEvent>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLayout>
#include <QLocale>
#include <QPalette>
#include <QPixmap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QSizePolicy>
#include <QStyle>
#include <QSysInfo>
#include <QTimer>
#include <QVBoxLayout>

#include <cstddef>
#include <cstdio>

#include "compatibility/QtCompat.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/scrolling/ScrollView.h"
#include "view/preview/GalleryPreviewActions.h"
#include "view/widgets/GallerySampleCard.h"
#include "view/widgets/GallerySampleCatalog.h"

namespace fluent::gallery {
namespace {

QString fileSha256(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return {};
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!file.atEnd())
    hash.addData(file.read(1024 * 1024));
  return QString::fromLatin1(hash.result().toHex());
}

constexpr int kMinimumPreviewWidth = 320;
constexpr int kMinimumPreviewHeight = 240;
constexpr int kMaximumPreviewWidth = 3840;
constexpr int kMaximumPreviewHeight = 2160;
constexpr int kPreviewInset = 16;
constexpr int kPreviewReportSchemaVersion = 2;

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
      QStringLiteral("actions"),
      QStringLiteral("Execute a versioned JSON interaction script before "
                     "capturing artifacts."),
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

QString widgetSegment(QWidget *widget) {
  if (!widget->objectName().isEmpty())
    return widget->objectName();

  const QString className =
      QString::fromLatin1(widget->metaObject()->className());
  int index = 0;
  if (QWidget *parent = widget->parentWidget()) {
    const auto siblings =
        parent->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *sibling : siblings) {
      if (sibling == widget)
        break;
      if (!sibling->objectName().startsWith(QStringLiteral("qt_")) &&
          QString::fromLatin1(sibling->metaObject()->className()) == className) {
        ++index;
      }
    }
  }
  return QStringLiteral("%1[%2]").arg(className).arg(index);
}

QString widgetPath(QWidget *widget, QWidget *root) {
  QStringList segments;
  for (QWidget *current = widget; current;
       current = current->parentWidget()) {
    if (!current->objectName().startsWith(QStringLiteral("qt_")))
      segments.prepend(widgetSegment(current));
    if (current == root)
      break;
  }
  return segments.join(QLatin1Char('/'));
}

QJsonObject rectObject(const QRect &rect) {
  return {{QStringLiteral("x"), rect.x()},
          {QStringLiteral("y"), rect.y()},
          {QStringLiteral("width"), rect.width()},
          {QStringLiteral("height"), rect.height()}};
}

QJsonObject sizeObject(const QSize &size) {
  return {{QStringLiteral("width"), size.width()},
          {QStringLiteral("height"), size.height()}};
}

QRect visibleWidgetRect(QWidget *widget, QWidget *root, const QRect &rect) {
  QRect visibleRect = rect.intersected(root->rect());
  for (QWidget *ancestor = widget ? widget->parentWidget() : nullptr;
       ancestor; ancestor = ancestor->parentWidget()) {
    const QRect ancestorRect(ancestor->mapTo(root, QPoint(0, 0)),
                             ancestor->size());
    visibleRect = visibleRect.intersected(ancestorRect);
    if (ancestor == root)
      break;
  }
  return visibleRect;
}

QJsonObject previewGeometryReport(QWidget *root) {
  QJsonArray widgets;
  if (!root) {
    return {{QStringLiteral("schema_version"), 1},
            {QStringLiteral("tool"),
             QStringLiteral("FluentQt Named Widget Geometry")},
            {QStringLiteral("widgets"), widgets}};
  }

  QVector<QWidget *> candidates{root};
  const auto descendants = root->findChildren<QWidget *>();
  candidates.reserve(descendants.size() + 1);
  for (QWidget *widget : descendants)
    candidates.append(widget);

  for (QWidget *widget : candidates) {
    if (!widget || widget->objectName().startsWith(QStringLiteral("qt_")) ||
        (widget != root && !widget->isVisibleTo(root))) {
      continue;
    }
    const QRect rect(widget->mapTo(root, QPoint(0, 0)), widget->size());
    const QRect visibleRect = visibleWidgetRect(widget, root, rect);
    widgets.append(QJsonObject{
        {QStringLiteral("path"), widgetPath(widget, root)},
        {QStringLiteral("class"),
         QString::fromLatin1(widget->metaObject()->className())},
        {QStringLiteral("object_name"), widget->objectName()},
        {QStringLiteral("stable"), !widget->objectName().isEmpty()},
        {QStringLiteral("rect"), rectObject(rect)},
        {QStringLiteral("visible_rect"), rectObject(visibleRect)},
        {QStringLiteral("minimum_size"), sizeObject(widget->minimumSize())},
        {QStringLiteral("maximum_size"), sizeObject(widget->maximumSize())},
        {QStringLiteral("size_hint"), sizeObject(widget->sizeHint())},
        {QStringLiteral("enabled"), widget->isEnabled()},
        {QStringLiteral("has_focus"), widget->hasFocus()},
        {QStringLiteral("clipped"), visibleRect != rect},
        {QStringLiteral("layout_direction"),
         widget->layoutDirection() == Qt::RightToLeft ? QStringLiteral("rtl")
                                                       : QStringLiteral("ltr")},
        {QStringLiteral("accessible_name"), widget->accessibleName()}});
  }

  return {{QStringLiteral("schema_version"), 1},
          {QStringLiteral("tool"),
           QStringLiteral("FluentQt Named Widget Geometry")},
          {QStringLiteral("root_size"), sizeObject(root->size())},
          {QStringLiteral("widget_count"), widgets.size()},
          {QStringLiteral("widgets"), widgets}};
}

QJsonObject previewEnvironment(QWidget *window) {
  const QScreen *screen = window ? window->screen() : QGuiApplication::primaryScreen();
  const QFont font = qApp ? qApp->font() : QFont();
  QJsonObject scaleEnvironment;
  const QStringList scaleVariables{
      QStringLiteral("QT_SCALE_FACTOR"),
      QStringLiteral("QT_SCREEN_SCALE_FACTORS"),
      QStringLiteral("QT_FONT_DPI"),
      QStringLiteral("QT_AUTO_SCREEN_SCALE_FACTOR"),
      QStringLiteral("QT_ENABLE_HIGHDPI_SCALING")};
  for (const QString &name : scaleVariables) {
    const QByteArray key = name.toLatin1();
    scaleEnvironment.insert(name, qEnvironmentVariable(key.constData()));
  }

  const QJsonObject system{
      {QStringLiteral("product_type"), QSysInfo::productType()},
      {QStringLiteral("product_version"), QSysInfo::productVersion()},
      {QStringLiteral("kernel_type"), QSysInfo::kernelType()},
      {QStringLiteral("kernel_version"), QSysInfo::kernelVersion()},
      {QStringLiteral("cpu_architecture"), QSysInfo::currentCpuArchitecture()}};
  const QJsonObject fontObject{
      {QStringLiteral("family"), font.family()},
      {QStringLiteral("style_name"), font.styleName()},
      {QStringLiteral("point_size"), font.pointSizeF()},
      {QStringLiteral("pixel_size"), font.pixelSize()},
      {QStringLiteral("weight"), font.weight()},
      {QStringLiteral("italic"), font.italic()}};
  QJsonObject screenObject;
  if (screen) {
    screenObject = {
        {QStringLiteral("name"), screen->name()},
        {QStringLiteral("manufacturer"), screen->manufacturer()},
        {QStringLiteral("model"), screen->model()},
        {QStringLiteral("serial_number"), screen->serialNumber()},
        {QStringLiteral("depth"), screen->depth()},
        {QStringLiteral("geometry"), rectObject(screen->geometry())},
        {QStringLiteral("available_geometry"),
         rectObject(screen->availableGeometry())},
        {QStringLiteral("physical_dpi_x"),
         screen->physicalDotsPerInchX()},
        {QStringLiteral("physical_dpi_y"),
         screen->physicalDotsPerInchY()}};
  }

  return {
      {QStringLiteral("fingerprint_schema_version"), 1},
      {QStringLiteral("qt_version"), QString::fromLatin1(qVersion())},
      {QStringLiteral("platform_plugin"), QGuiApplication::platformName()},
      {QStringLiteral("style"),
       qApp && qApp->style() ? qApp->style()->objectName() : QString()},
      {QStringLiteral("device_pixel_ratio"),
       window ? window->devicePixelRatioF() : 0.0},
      {QStringLiteral("logical_dpi_x"),
       screen ? screen->logicalDotsPerInchX() : 0.0},
      {QStringLiteral("logical_dpi_y"),
       screen ? screen->logicalDotsPerInchY() : 0.0},
      {QStringLiteral("locale"), QLocale().name()},
      {QStringLiteral("font"), fontObject},
      {QStringLiteral("screen"), screenObject},
      {QStringLiteral("system"), system},
      {QStringLiteral("scale_environment"), scaleEnvironment}};
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
  result.options.actionsPath =
      parser.value(QStringLiteral("actions")).trimmed();
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

  // The compiled preview runs through the desktop adapter and must mirror the
  // installed C++ Gallery's C++-only source presentation. Passing the route id
  // would request the WebAssembly-only bilingual catalog, which is deliberately
  // absent from desktop binaries and produces a misleading resource warning.
  m_sampleCard = new GallerySampleCard(sample, m_canvas);
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
                                 const QString &snapshotError,
                                 const QJsonObject &interactionReport) {
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
                   {QStringLiteral("sha256"),
                    snapshotWritten ? fileSha256(snapshotPath) : QString()},
                   {QStringLiteral("error"), snapshotError}}}};

  const QJsonObject resolvedInteractionReport =
      interactionReport.isEmpty() ? galleryPreviewActionsNotRequested()
                                  : interactionReport;
  const bool interactionFailed =
      resolvedInteractionReport.value(QStringLiteral("requested")).toBool() &&
      resolvedInteractionReport.value(QStringLiteral("status")).toString() !=
          QStringLiteral("pass");

  return {
      {QStringLiteral("schema_version"), kPreviewReportSchemaVersion},
      {QStringLiteral("tool"), QStringLiteral("FluentQt Gallery Preview")},
      {QStringLiteral("status"),
       !snapshotError.isEmpty()
           ? QStringLiteral("artifact-error")
           : interactionFailed ? QStringLiteral("interaction-error")
                               : QStringLiteral("ok")},
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
      {QStringLiteral("environment"), previewEnvironment(window)},
      {QStringLiteral("artifacts"), artifacts},
      {QStringLiteral("interaction_report"), resolvedInteractionReport},
      {QStringLiteral("geometry_report"), previewGeometryReport(window)},
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
                            !options.reportPath.isEmpty() ||
                            !options.actionsPath.isEmpty();
  int artifactExitCode = 0;
  if (hasArtifacts) {
    QTimer::singleShot(options.settleMs, &window, [&]() {
      QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
      QApplication::processEvents(QEventLoop::AllEvents, 50);

      const GalleryPreviewActionResult actionResult =
          runGalleryPreviewActions(&window, options.actionsPath);
      if (!actionResult.passed) {
        artifactExitCode = 6;
        writeStandardError(QStringLiteral("Gallery preview interactions failed."));
      }

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
            galleryPreviewReport(&window, options, snapshotPath, snapshotError,
                                 actionResult.report);
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
