#include "SpatialTextureBackend_p.h"
#include "compatibility/private/spatial/SpatialRenderCompat_p.h"

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QLoggingCategory>
#include <QMatrix4x4>
#include <QPainter>
#include <QWidget>
#include <utility>
#include <QTimer>
#include <QtMath>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#ifdef FLUENT_QT_HAS_RHI
#include <QRhiWidget>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

static void initializeSpatialShaders()
{
    Q_INIT_RESOURCE(spatial_rhi);
}
#endif

namespace compatibility::detail {
namespace {
#ifdef FLUENT_QT_HAS_RHI
Q_LOGGING_CATEGORY(lcSpatialRenderer, "fluentqt.spatial.renderer")
#endif
constexpr qint64 kBudget = 192 * 1024 * 1024;
struct Layer {
    QRectF source;
    QTransform transform;
    quint64 revision = 0;
    qreal radius = 0, progress = 0, shadowOpacity = 0;
    QColor material, reflection;
};
#ifdef FLUENT_QT_HAS_RHI
constexpr int kUniformBytes = 144;
bool hardwareDevice(QRhi* rhi, const std::function<bool(const QString&)>& isHardwareRenderer)
{
    return rhi && rhi->driverInfo().deviceType != QRhiDriverInfo::CpuDevice
        && isHardwareRenderer(QString::fromUtf8(rhi->driverInfo().deviceName));
}
std::unique_ptr<QRhi> probeDevice()
{
#if defined(Q_OS_WIN)
    QRhiD3D11InitParams params;
    return std::unique_ptr<QRhi>(QRhi::create(QRhi::D3D11, &params));
#elif defined(Q_OS_MACOS)
    QRhiMetalInitParams params;
    return std::unique_ptr<QRhi>(QRhi::create(QRhi::Metal, &params));
#else
    return {};
#endif
}
QShader shader(const char* name)
{
    QFile file(QString::fromLatin1(name));
    return file.open(QIODevice::ReadOnly) ? QShader::fromSerialized(file.readAll()) : QShader();
}
#endif
} // namespace

struct SpatialTextureBackend::Private {
    QWidget* owner;
    SpatialTextureCallbacks callbacks;
    std::vector<Layer> layers;
    QRectF clip;
    bool ready = false, failurePending = false;
    QString renderer, backend;
    qreal dpr = 0;
    qint64 captures = 0, uploads = 0, frames = 0, estimatedBytes = 0;
    int maxDimension = 0;
#ifdef FLUENT_QT_HAS_RHI
    struct Cache {
        std::unique_ptr<QRhiTexture> texture;
        std::unique_ptr<QRhiBuffer> uniform;
        std::unique_ptr<QRhiShaderResourceBindings> bindings;
        QRectF source;
        quint64 revision = 0;
        qreal dpr = 0;
    };
    class Surface final : public QRhiWidget {
    public:
        explicit Surface(Private* state) : QRhiWidget(nullptr), d(state)
        {
#if defined(Q_OS_WIN)
            setApi(Api::Direct3D11);
#elif defined(Q_OS_MACOS)
            setApi(Api::Metal);
#endif
            setAttribute(Qt::WA_TransparentForMouseEvents);
            setAttribute(Qt::WA_NoSystemBackground);
            setFocusPolicy(Qt::NoFocus);
            connect(this, &QRhiWidget::renderFailed, d->owner, [this] { fail(false); });
            // Reparent only after QRhiWidget has installed its RHI configuration.
            // QWidget's constructor runs before that configuration exists and
            // cannot enable RHI on an already exposed native parent.
            setParent(state->owner);
        }
        ~Surface() override { releaseResources(); }
        void clearCaches()
        {
            caches.clear();
            d->dpr = 0;
            d->estimatedBytes = 0;
            d->failurePending = false;
        }

    protected:
        void releaseResources() override
        {
            pipeline.reset();
            clearCaches();
            sampler.reset();
            vertices.reset();
            referenceBindings.reset();
            referenceUniform.reset();
            referenceTexture.reset();
            d->ready = false;
        }
        void initialize(QRhiCommandBuffer*) override
        {
            releaseResources();
            if (!hardwareDevice(rhi(), d->callbacks.isHardwareRenderer)) {
                fail(false);
                return;
            }
            d->renderer = QString::fromUtf8(rhi()->driverInfo().deviceName);
            d->backend = QString::fromLatin1(rhi()->backendName());
            d->maxDimension = rhi()->resourceLimit(QRhi::TextureSizeMax);
            vertices.reset(rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 8 * sizeof(float)));
            sampler.reset(rhi()->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
            referenceUniform.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBytes));
            referenceTexture.reset(rhi()->newTexture(QRhiTexture::RGBA8, QSize(1, 1)));
            if (!vertices->create() || !sampler->create() || !referenceUniform->create()
                || !referenceTexture->create()) {
                fail(false);
                return;
            }
            referenceBindings = bindings(referenceUniform.get(), referenceTexture.get());
            if (!referenceBindings) {
                fail(false);
                return;
            }
            pipeline.reset(rhi()->newGraphicsPipeline());
            QRhiGraphicsPipeline::TargetBlend blend;
            blend.enable = true;
            blend.srcColor = QRhiGraphicsPipeline::One;
            blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            blend.srcAlpha = QRhiGraphicsPipeline::One;
            blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            pipeline->setTargetBlends({blend});
            pipeline->setFlags(QRhiGraphicsPipeline::UsesScissor);
            pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
            pipeline->setShaderStages({{QRhiShaderStage::Vertex, shader(":/fluentqt/spatial/panel.vert.qsb")},
                {QRhiShaderStage::Fragment, shader(":/fluentqt/spatial/panel.frag.qsb")}});
            QRhiVertexInputLayout input;
            input.setBindings({{2 * sizeof(float)}});
            input.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0}});
            pipeline->setVertexInputLayout(input);
            pipeline->setShaderResourceBindings(referenceBindings.get());
            pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
            pipeline->setSampleCount(renderTarget()->sampleCount());
            if (!pipeline->create()) {
                fail(false);
                return;
            }
            vertexUploadPending = true;
            d->ready = true;
            qCInfo(lcSpatialRenderer) << "Hardware renderer ready:" << d->backend << d->renderer;
            d->callbacks.initialized();
        }
        void render(QRhiCommandBuffer* cb) override
        {
            if (!d->ready || rhi()->isDeviceLost()) {
                fail(false);
                return;
            }
            d->callbacks.synchronizeFrame();
            if (!plan()) {
                fail(true);
                return;
            }
            auto* updates = rhi()->nextResourceUpdateBatch();
            if (vertexUploadPending) {
                const float points[] = {0, 0, 1, 0, 0, 1, 1, 1};
                updates->uploadStaticBuffer(vertices.get(), points);
                vertexUploadPending = false;
            }
            caches.resize(d->layers.size());
            bool valid = true;
            for (size_t i = 0; i < caches.size(); ++i) {
                const auto& layer = d->layers[i];
                if (layer.source.isEmpty()) {
                    caches[i] = {};
                    continue;
                }
                if (!updateCache(int(i), updates)) {
                    valid = false;
                    break;
                }
                const auto values = uniforms(layer, caches[i].texture->pixelSize());
                updates->updateDynamicBuffer(caches[i].uniform.get(), 0, kUniformBytes, values.data());
            }
            if (!valid) {
                updates->release();
                fail(true);
                return;
            }
            const QSize pixels = renderTarget()->pixelSize();
            cb->beginPass(renderTarget(), Qt::transparent, {1.0f, 0}, updates);
            cb->setGraphicsPipeline(pipeline.get());
            cb->setViewport({0, 0, float(pixels.width()), float(pixels.height())});
            const QRhiCommandBuffer::VertexInput vertex(vertices.get(), 0);
            cb->setVertexInput(0, 1, &vertex);
            for (size_t i = 0; i < caches.size(); ++i) {
                if (!caches[i].texture)
                    continue;
                QRect clip(QPoint(), pixels);
                if (i > 0 && !d->clip.isEmpty()) {
                    const qreal scale = devicePixelRatioF();
                    clip &= QRect(qFloor(d->clip.x() * scale), pixels.height() - qCeil(d->clip.bottom() * scale),
                        qCeil(d->clip.width() * scale), qCeil(d->clip.height() * scale));
                }
                if (clip.isEmpty())
                    continue;
                cb->setScissor({clip.x(), clip.y(), clip.width(), clip.height()});
                cb->setShaderResources(caches[i].bindings.get());
                cb->draw(4);
            }
            cb->endPass();
            ++d->frames;
        }

    private:
        Private* d;
        std::vector<Cache> caches;
        std::unique_ptr<QRhiBuffer> vertices, referenceUniform;
        std::unique_ptr<QRhiTexture> referenceTexture;
        std::unique_ptr<QRhiSampler> sampler;
        std::unique_ptr<QRhiShaderResourceBindings> referenceBindings;
        std::unique_ptr<QRhiGraphicsPipeline> pipeline;
        bool vertexUploadPending = true;
        void fail(bool budget)
        {
            if (d->failurePending)
                return;
            d->failurePending = true;
            QTimer::singleShot(0, d->owner, budget ? d->callbacks.cacheUnavailable : d->callbacks.renderingFailed);
        }
        std::unique_ptr<QRhiShaderResourceBindings> bindings(QRhiBuffer* uniform, QRhiTexture* texture)
        {
            std::unique_ptr<QRhiShaderResourceBindings> result(rhi()->newShaderResourceBindings());
            result->setBindings(
                {QRhiShaderResourceBinding::uniformBuffer(0,
                     QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, uniform),
                    QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, texture,
                        sampler.get())});
            if (!result->create())
                result.reset();
            return result;
        }
        bool plan()
        {
            // Include the CPU capture, queued upload, GPU textures and final
            // color/depth targets. Never reduce below the display's native DPR.
            const QSize output = renderTarget()->pixelSize();
            const qint64 targetBytes = qint64(output.width()) * output.height() * 12;
            const qreal native = devicePixelRatioF();
            for (qreal extra : {2.0, 1.75, 1.5, 1.25, 1.0}) {
                qint64 bytes = targetBytes;
                bool fits = true;
                for (const auto& layer : d->layers) {
                    if (layer.source.isEmpty())
                        continue;
                    const qreal w = std::ceil(layer.source.width() * native * extra);
                    const qreal h = std::ceil(layer.source.height() * native * extra);
                    if (!std::isfinite(w) || !std::isfinite(h) || w > d->maxDimension || h > d->maxDimension) {
                        fits = false;
                        break;
                    }
                    bytes += qint64(w) * qint64(h) * 12;
                }
                if (fits && bytes <= kBudget) {
                    d->dpr = native * extra;
                    d->estimatedBytes = bytes;
                    return true;
                }
            }
            return false;
        }
        bool updateCache(int index, QRhiResourceUpdateBatch* updates)
        {
            auto& cache = caches[size_t(index)];
            const auto& layer = d->layers[size_t(index)];
            const QSize size(qCeil(layer.source.width() * d->dpr), qCeil(layer.source.height() * d->dpr));
            const bool allocate = !cache.texture || cache.texture->pixelSize() != size;
            if (allocate) {
                cache = {};
                cache.texture.reset(rhi()->newTexture(QRhiTexture::RGBA8, size));
                cache.uniform.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBytes));
                if (!cache.texture->create() || !cache.uniform->create())
                    return false;
                cache.bindings = bindings(cache.uniform.get(), cache.texture.get());
                if (!cache.bindings)
                    return false;
            }
            if (!allocate && cache.source == layer.source && cache.revision == layer.revision && cache.dpr == d->dpr)
                return true;
            QImage image(size, QImage::Format_RGBA8888_Premultiplied);
            if (image.isNull())
                return false;
            image.setDevicePixelRatio(d->dpr);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            painter.setRenderHints(
                QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
            painter.translate(-layer.source.topLeft());
            painter.setClipRect(layer.source);
            d->callbacks.paintLayer(&painter, index, QRegion(layer.source.toAlignedRect()));
            painter.end();
            updates->uploadTexture(cache.texture.get(), image);
            cache.source = layer.source;
            cache.revision = layer.revision;
            cache.dpr = d->dpr;
            ++d->captures;
            ++d->uploads;
            return true;
        }
        std::array<float, kUniformBytes / sizeof(float)> uniforms(const Layer& layer, const QSize& size)
        {
            std::array<float, kUniformBytes / sizeof(float)> values{};
            const bool decorated = layer.radius > 0;
            const qreal padding = decorated ? 20 : 0;
            QMatrix4x4 projection;
            projection.ortho(0.f, float(width()), float(height()), 0.f, -1.f, 1.f);
            QMatrix4x4 quad;
            quad.translate(layer.source.x() - padding, layer.source.y() - padding);
            quad.scale(layer.source.width() + 2 * padding, layer.source.height() + 2 * padding);
            const auto matrix = rhi()->clipSpaceCorrMatrix() * projection * QMatrix4x4(layer.transform) * quad;
            std::memcpy(values.data(), matrix.constData(), 64);
            values[16] = float(size.width());
            values[17] = float(size.height());
            values[18] = float(layer.source.width());
            values[19] = float(layer.source.height());
            values[20] = float(layer.radius);
            values[21] = float(layer.progress);
            values[22] = float(padding);
            values[23] = decorated ? 1.f : 0.f;
            const auto color = [&values](int offset, const QColor& c) {
                values[size_t(offset)] = float(c.redF());
                values[size_t(offset + 1)] = float(c.greenF());
                values[size_t(offset + 2)] = float(c.blueF());
                values[size_t(offset + 3)] = float(c.alphaF());
            };
            color(24, layer.material);
            color(28, layer.reflection);
            values[35] = float(layer.shadowOpacity);
            return values;
        }
    };
    Surface* surface = nullptr;
#endif
};

SpatialTextureBackend::SpatialTextureBackend(QWidget* owner, SpatialTextureCallbacks callbacks) : d(new Private)
{
    d->owner = owner;
    d->callbacks = std::move(callbacks);
#ifdef FLUENT_QT_HAS_RHI
    initializeSpatialShaders();
    d->surface = new Private::Surface(d.get());
#endif
}
SpatialTextureBackend::~SpatialTextureBackend()
{
#ifdef FLUENT_QT_HAS_RHI
    delete d->surface;
#endif
}
bool SpatialTextureBackend::isSupported()
{
#if defined(FLUENT_QT_HAS_RHI) && (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
    return spatialRenderCapabilities().openGLDisplay;
#else
    return false;
#endif
}
bool SpatialTextureBackend::isPreferred()
{
    if (!isSupported())
        return false;
    const auto requested = qgetenv("FLUENT_QT_SPATIAL_BACKEND").toLower();
    if (requested == "opengl")
        return false;
#if defined(Q_OS_WIN) && defined(Q_PROCESSOR_ARM_64)
    return true;
#else
    return requested == "rhi";
#endif
}
std::optional<QSurface::SurfaceType> SpatialTextureBackend::preferredSurfaceType()
{
#if defined(FLUENT_QT_HAS_RHI) && defined(Q_OS_WIN)
    return QSurface::Direct3DSurface;
#else
    // Metal's layer and RHI backing store must be created together. This path
    // is used only for native backend validation on macOS.
    return std::nullopt;
#endif
}
bool SpatialTextureBackend::hasHardwareDevice(const std::function<bool(const QString&)>& isHardwareRenderer)
{
#ifdef FLUENT_QT_HAS_RHI
    if (isSupported()) {
        const auto probe = probeDevice();
        return hardwareDevice(probe.get(), isHardwareRenderer);
    }
#else
    Q_UNUSED(isHardwareRenderer);
#endif
    return false;
}
bool SpatialTextureBackend::isReady() const
{
    return d->ready;
}
QString SpatialTextureBackend::rendererName() const
{
    return d->renderer;
}
QVariantMap SpatialTextureBackend::statistics() const
{
    return {{"backend", d->backend}, {"renderer", d->renderer}, {"paints", d->frames}, {"captures", d->captures},
        {"uploads", d->uploads}, {"cacheDpr", d->dpr}, {"cacheEstimatedBytes", d->estimatedBytes},
        {"cacheBudgetBytes", kBudget}, {"maxCacheDimension", d->maxDimension}};
}
QImage SpatialTextureBackend::grabFramebuffer() const
{
#ifdef FLUENT_QT_HAS_RHI
    return d->surface->grabFramebuffer();
#else
    return {};
#endif
}
void SpatialTextureBackend::requestFrame()
{
#ifdef FLUENT_QT_HAS_RHI
    d->surface->update();
#endif
}
void SpatialTextureBackend::clearFrameCaches()
{
#ifdef FLUENT_QT_HAS_RHI
    d->surface->clearCaches();
#endif
}
void SpatialTextureBackend::setLayerCount(int count)
{
    d->layers.resize(size_t(qBound(0, count, 64)));
}
void SpatialTextureBackend::setLayer(int index,
    const QRectF& source,
    const QTransform& transform,
    quint64 revision,
    qreal radius,
    const QColor& material,
    const QColor& reflection,
    qreal progress,
    qreal shadowOpacity)
{
    if (index < 0 || size_t(index) >= d->layers.size())
        return;
    d->layers[size_t(index)] = {source, transform, revision, radius, progress, shadowOpacity, material, reflection};
}
void SpatialTextureBackend::setSceneClip(const QRectF& clip)
{
    d->clip = clip;
}
void SpatialTextureBackend::resize(const QRect& rect)
{
#ifdef FLUENT_QT_HAS_RHI
    d->surface->setGeometry(rect);
#else
    Q_UNUSED(rect);
#endif
}
} // namespace compatibility::detail
