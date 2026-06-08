#ifndef GALLERYNAVIGATIONSTATE_H
#define GALLERYNAVIGATIONSTATE_H

#include <QObject>
#include <QString>

namespace fluent::gallery {

class GalleryNavigationState : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString selectedRouteId READ selectedRouteId WRITE setSelectedRouteId NOTIFY selectedRouteIdChanged)

public:
    explicit GalleryNavigationState(QObject* parent = nullptr);

    QString selectedRouteId() const { return m_selectedRouteId; }

public slots:
    void setSelectedRouteId(const QString& routeId);

signals:
    void selectedRouteIdChanged(const QString& routeId);

private:
    QString m_selectedRouteId;
};

} // namespace fluent::gallery

#endif // GALLERYNAVIGATIONSTATE_H