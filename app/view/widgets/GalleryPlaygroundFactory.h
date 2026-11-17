#ifndef GALLERYPLAYGROUNDFACTORY_H
#define GALLERYPLAYGROUNDFACTORY_H

#include <QStringList>

class QObject;
class QWidget;

namespace fluent::gallery {
struct PlaygroundControl {
    QWidget* surface = nullptr;
    QObject* target = nullptr;
};

QStringList playgroundRoutes();
PlaygroundControl createPlaygroundControl(const QString& route, QWidget* parent);
} // namespace fluent::gallery
#endif
