#pragma once

#include <QColor>
#include <QImage>
#include <QRegion>
#include <QSurface>
#include <QTransform>
#include <QVariantMap>
#include <functional>
#include <memory>
#include <optional>

class QPainter;
class QWidget;

namespace compatibility::detail {

// The application supplies scene content and renderer policy; this adapter owns
// the optional Qt RHI surface and its version/platform-specific fallback.
// zh_CN: 应用提供场景内容与渲染器策略；适配器管理可选 Qt RHI 表面及版本/平台回退。
struct SpatialTextureCallbacks {
    std::function<void()> synchronizeFrame;
    std::function<void(QPainter*, int, const QRegion&)> paintLayer;
    std::function<void()> initialized;
    std::function<void()> renderingFailed;
    std::function<void()> cacheUnavailable;
    std::function<bool(const QString&)> isHardwareRenderer;
};

class SpatialTextureBackend final {
public:
    SpatialTextureBackend(QWidget* owner, SpatialTextureCallbacks callbacks);
    ~SpatialTextureBackend();
    static bool isSupported();
    static bool isPreferred();
    static std::optional<QSurface::SurfaceType> preferredSurfaceType();
    static bool hasHardwareDevice(const std::function<bool(const QString&)>& isHardwareRenderer);
    bool isReady() const;
    QString rendererName() const;
    QVariantMap statistics() const;
    QImage grabFramebuffer() const;
    void requestFrame();
    void clearFrameCaches();
    void setLayerCount(int count);
    void setLayer(int index,
        const QRectF& source,
        const QTransform& transform,
        quint64 revision,
        qreal radius,
        const QColor& material,
        const QColor& reflection,
        qreal progress,
        qreal shadowOpacity);
    void setSceneClip(const QRectF& clip);
    void resize(const QRect& rect);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace compatibility::detail
