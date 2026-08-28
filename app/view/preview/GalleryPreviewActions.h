#ifndef GALLERYPREVIEWACTIONS_H
#define GALLERYPREVIEWACTIONS_H

#include <QJsonObject>
#include <QString>

class QWidget;

namespace fluent::gallery {

struct GalleryPreviewActionResult {
  QJsonObject report;
  bool passed = false;
};

GalleryPreviewActionResult
executeGalleryPreviewActions(QWidget *root, const QJsonObject &script,
                             const QString &sourcePath = QString());

GalleryPreviewActionResult runGalleryPreviewActions(QWidget *root,
                                                     const QString &path);

QJsonObject galleryPreviewActionsNotRequested();

} // namespace fluent::gallery

#endif // GALLERYPREVIEWACTIONS_H
