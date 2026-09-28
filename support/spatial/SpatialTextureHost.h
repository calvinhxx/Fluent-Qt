#ifndef FLUENTQT_SPATIALTEXTUREHOST_P_H
#define FLUENTQT_SPATIALTEXTUREHOST_P_H

#include <QColor>
#include <QRegion>
#include <QTransform>
#include <QVariantMap>
#include <QWidget>
#include <memory>

class QPainter;

namespace fluent::windowing {
class Window;
}

namespace fluent::spatial {
// Internal texture compositor shared by native hosts and the Python Gallery.
// Not part of the installed API. Widgets paint only when their revision changes;
// perspective and material animation run on the GPU without recapturing widgets.
class SpatialTextureHost : public QWidget {
    Q_OBJECT
public:
    explicit SpatialTextureHost(QWidget* parent = nullptr);
    ~SpatialTextureHost() override;
    static bool isSupported();
    static bool prepareWindow(windowing::Window* window);
    static bool isPreferred();
    static QString preflightFailure();
    bool isReady() const;
    QString rendererName() const;
    QVariantMap statistics() const;
    QImage grabFramebuffer() const;
    void requestFrame();
    void clearFrameCaches();
    void setLayerCount(int count);
    void setLayer(int index, const QRectF& source, const QTransform& transform, quint64 revision,
                  qreal radius = 0, const QColor& material = Qt::transparent,
                  const QColor& reflection = Qt::transparent, qreal progress = 0,
                  qreal shadowOpacity = 0);
    void setSceneClip(const QRectF& clip);

signals:
    void initialized();
    void renderingFailed();
    void cacheUnavailable();

protected:
    virtual void synchronizeFrame();
    virtual void paintLayer(QPainter* painter, int index, const QRegion& region);
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
} // namespace fluent::spatial
#endif
