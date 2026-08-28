#ifndef GALLERYPREVIEWAPPLICATION_H
#define GALLERYPREVIEWAPPLICATION_H

#include <QJsonObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "model/GalleryContentCatalog.h"

class QApplication;

namespace fluent::gallery {

class GallerySampleCard;

enum class GalleryPreviewTheme { System, Light, Dark };

/**
 * @brief Parsed, development-only Gallery preview command-line contract.
 * zh_CN: 解析后的开发期 Gallery 预览命令行契约。
 */
struct GalleryPreviewOptions {
  bool requested = false;
  bool helpRequested = false;
  QString routeId;
  QString sampleId;
  GalleryPreviewTheme theme = GalleryPreviewTheme::Light;
  bool rightToLeft = false;
  QSize viewportSize{800, 640};
  QString actionsPath;
  QString snapshotPath;
  QString reportPath;
  int settleMs = 250;
  bool keepOpen = false;
};

struct GalleryPreviewParseResult {
  GalleryPreviewOptions options;
  QString error;
  QString helpText;

  bool isValid() const { return error.isEmpty(); }
};

struct GalleryPreviewSelection {
  GallerySample sample;
  QStringList availableSampleIds;
  QString error;

  bool isValid() const { return error.isEmpty(); }
};

/** @brief Parses preview arguments without changing normal Gallery startup. */
GalleryPreviewParseResult
parseGalleryPreviewArguments(const QStringList &arguments);

/** @brief Resolves one real Gallery sample factory for a preview request. */
GalleryPreviewSelection resolveGalleryPreviewSelection(const QString &routeId,
                                                       const QString &sampleId);

QString galleryPreviewThemeName(GalleryPreviewTheme theme);

/**
 * @brief Minimal deterministic host for one real Gallery sample card.
 * zh_CN: 用于单个真实 Gallery 示例卡片的最小确定性宿主。
 */
class GalleryPreviewWindow final : public QWidget,
                                   public fluent::FluentElement,
                                   public fluent::QMLPlus {
public:
  GalleryPreviewWindow(const GalleryPreviewOptions &options,
                       const GallerySample &sample, QWidget *parent = nullptr);

  GallerySampleCard *sampleCard() const { return m_sampleCard; }
  QString routeId() const { return m_routeId; }
  QString sampleId() const { return m_sampleId; }

  void onThemeUpdated() override;

private:
  void applyPalette();

  QString m_routeId;
  QString m_sampleId;
  QWidget *m_canvas = nullptr;
  GallerySampleCard *m_sampleCard = nullptr;
};

/** @brief Builds the versioned machine-readable preview and Inspector report.
 */
QJsonObject galleryPreviewReport(GalleryPreviewWindow *window,
                                 const GalleryPreviewOptions &options,
                                 const QString &snapshotPath = QString(),
                                 const QString &snapshotError = QString(),
                                 const QJsonObject &interactionReport =
                                     QJsonObject());

/** @brief Runs the isolated preview path inside an initialized QApplication. */
int runGalleryPreviewApplication(QApplication &app,
                                 const GalleryPreviewOptions &options);

} // namespace fluent::gallery

#endif // GALLERYPREVIEWAPPLICATION_H
