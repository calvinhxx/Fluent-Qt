#ifndef GALLERYNAVIGATIONVIEWMODEL_H
#define GALLERYNAVIGATIONVIEWMODEL_H

#include <QString>
#include <QStringList>
#include <QVector>

#include "model/GalleryNavigationItem.h"

namespace fluent::gallery {

class GalleryNavigationViewModel {
public:
    GalleryNavigationViewModel();

    const QVector<GalleryNavigationItem>& items() const { return m_items; }
    QVector<GalleryNavigationItem> mainPaneItems() const;
    QVector<GalleryNavigationItem> footerPaneItems() const;
    QStringList navigationEntryIds() const;
    QStringList visibleTitles() const;
    QString defaultRouteId() const;
    const GalleryNavigationItem* itemById(const QString& id) const;
    QString parentRouteId(const QString& id) const;

private:
    QVector<GalleryNavigationItem> m_items;
};

} // namespace fluent::gallery

#endif // GALLERYNAVIGATIONVIEWMODEL_H
