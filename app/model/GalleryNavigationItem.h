#ifndef GALLERYNAVIGATIONITEM_H
#define GALLERYNAVIGATIONITEM_H

#include <QColor>
#include <QString>

namespace fluent::gallery {

struct GalleryNavigationItem {
    QString id;
    QString title;
    QString group;
    QString iconGlyph;
    QColor placeholderColor;
    int depth = 0;
    bool sectionHeader = false;
    bool expandable = false;
};

} // namespace fluent::gallery

#endif // GALLERYNAVIGATIONITEM_H
