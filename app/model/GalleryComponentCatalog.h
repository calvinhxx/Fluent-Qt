#ifndef GALLERYCOMPONENTCATALOG_H
#define GALLERYCOMPONENTCATALOG_H

#include <QString>
#include <QVector>

namespace fluent::gallery {

struct GalleryComponentEntry {
    QString id;
    QString title;
    QString iconGlyph;
};

struct GalleryComponentCategory {
    QString id;
    QString title;
    QString sourceDirectory;
    QString iconGlyph;
    QVector<GalleryComponentEntry> components;
};

const QVector<GalleryComponentCategory>& galleryComponentCatalog();

} // namespace fluent::gallery

#endif // GALLERYCOMPONENTCATALOG_H