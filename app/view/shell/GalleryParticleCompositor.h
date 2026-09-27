#ifndef GALLERYPARTICLECOMPOSITOR_H
#define GALLERYPARTICLECOMPOSITOR_H

#ifdef FLUENT_QT_HAS_SPATIAL
#include <QObject>
#include <QRegion>
#include <QSize>
#include <functional>
#include <memory>

class QPainter;
class QOpenGLFramebufferObject;
class QWidget;

namespace fluent::gallery::spatial_render {

// Gallery-only prototype. Existing static panel pixels retain their foreground;
// the GPU insertion pass adds a particle layer underneath that foreground once.
class GalleryParticleCompositor final : public QObject {
    Q_OBJECT
public:
    using RenderWidget = std::function<void(QPainter&, QWidget*, const QRegion&)>;
    explicit GalleryParticleCompositor(QObject* parent = nullptr);
    ~GalleryParticleCompositor() override;

    bool prepare(QWidget* content, quint64 contentRevision, const QSize& panelPixels,
                 qreal nativeDpr, qreal cacheDpr, quint64 maximumBytes, bool enabled);
    unsigned int compose(unsigned int staticTexture, quint64 contentRevision,
                         QOpenGLFramebufferObject* paintTarget,
                         QOpenGLFramebufferObject* resolveTarget, const RenderWidget& renderWidget);
    void release();
    int activeLayerCount() const;
    quint64 allocatedBytes() const;
    quint64 particleFrameCount() const;
    quint64 foregroundCaptureCount() const;
    quint64 compositionCount() const;
    quint64 sourceScanCount() const;

signals:
    void frameRequested();
    void staticContentInvalidated();

private:
    void releaseGpu(bool notify = true);
    struct Private;
    std::unique_ptr<Private> d;
};
} // namespace fluent::gallery::spatial_render
#endif
#endif
