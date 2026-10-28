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
    // Internal allocation probe shared with the Python compositor; not installed API.
    static int currentPaintSampleBytes();
    // Internal capture helper shared with the Python compositor; not installed API.
    // Returns the backing-store dirty region of source's graphics effect pass in source
    // coordinates, or an empty region when the pass cannot be mapped (repaint everything).
    static QRegion graphicsEffectDirtyRegion(const QPainter* painter, const QWidget* source);
    // Paint textureWidget (a QOpenGLWidget or QRhiWidget) when the next backing-store pass
    // reaches it, without queuing it as a dirty texture widget. False means use update().
    static bool schedulePaintInBackingStorePass(QWidget* textureWidget);
    bool isReady() const;
    QString rendererName() const;
    QVariantMap statistics() const;
    QImage grabFramebuffer() const;
    void requestFrame();
    // Paint the surface in the next backing-store pass that reaches it, without queuing
    // it as a dirty texture widget. False means use requestFrame().
    bool scheduleFrameInBackingStorePass();
    void clearFrameCaches();
    void setLayerCount(int count);
    void setLayer(int index,
        const QRectF& source,
        const QTransform& transform,
        quint64 revision,
        qreal radius = 0,
        const QColor& material = Qt::transparent,
        const QColor& reflection = Qt::transparent,
        qreal progress = 0,
        qreal shadowOpacity = 0);
    // A layer whose texture still holds baseRevision repaints only damage, in layer
    // coordinates. An empty damage region, or any other cached revision, repaints the layer.
    void setLayerDamage(int index, quint64 baseRevision, const QRegion& damage);
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
