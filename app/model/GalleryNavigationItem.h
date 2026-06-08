#ifndef GALLERYNAVIGATIONITEM_H
#define GALLERYNAVIGATIONITEM_H

#include <QColor>
#include <QString>

namespace fluent::gallery {

struct GalleryNavigationItem {
    enum class Kind {
        SectionHeader,
        RootRoute,
        CategoryRoute,
        ComponentRoute,
        FooterRoute
    };

    QString id;
    QString title;
    QString group;
    QString parentId;
    QString iconGlyph;
    QColor placeholderColor;
    Kind kind = Kind::RootRoute;
    int depth = 0;
    bool sectionHeader = false;
    bool expandable = false;
};

} // namespace fluent::gallery

#endif // GALLERYNAVIGATIONITEM_H
