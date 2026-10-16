#include "compatibility/private/quick/QuickRenderCompat_p.h"
#include "QmlTextureRenderer_p.h"
#include "QmlView.h"

#include <QOffscreenSurface>
#include <QLoggingCategory>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QPointer>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QScopedValueRollback>
#include <QtMath>
#include <cmath>

namespace fluent::qmlbridge {
namespace {
Q_LOGGING_CATEGORY(lcQmlTexture, "fluentqt.qmlbridge.texture")

class TargetScope {
public:
    explicit TargetScope(QOpenGLContext* context) : m_context(context), m_gl(context->functions())
    {
        m_gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_framebuffer);
        m_gl->glGetIntegerv(GL_VIEWPORT, m_viewport);
    }
    ~TargetScope()
    {
        if (m_context && QOpenGLContext::currentContext() == m_context) {
            m_gl->glBindFramebuffer(GL_FRAMEBUFFER, GLuint(m_framebuffer));
            m_gl->glViewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
        }
    }

private:
    QPointer<QOpenGLContext> m_context;
    QOpenGLFunctions* m_gl;
    GLint m_framebuffer = 0;
    GLint m_viewport[4] = {};
};
} // namespace

struct QmlTextureRenderer::Private {
    QPointer<QmlView> view;
    QPointer<QQuickWidget> quick;
    QPointer<QQuickItem> root;
    QPointer<QOpenGLContext> context;
    QMetaObject::Connection contextDestroyed;
    std::unique_ptr<::compatibility::detail::QuickRenderControl> control;
    std::unique_ptr<QQuickWindow> window;
    std::unique_ptr<QOpenGLFramebufferObject> target;
    std::unique_ptr<QOffscreenSurface> cleanupSurface;
    QSize pixels;
    qreal dpr = 0;
    quint64 bytes = 0;
    quint64 frames = 0;
    QString error;
    bool initialized = false;
    bool dirty = true;
    bool rendering = false;
    bool releasing = false;
    bool releasePending = false;
    bool detached = false;
};

QmlTextureRenderer::QmlTextureRenderer(QmlView* view) : d(std::make_unique<Private>())
{
    d->view = view;
}
QmlTextureRenderer::~QmlTextureRenderer()
{
    d->detached = true;
    releaseNow(true);
}

void QmlTextureRenderer::createWindow()
{
    if (d->window || d->detached || !d->view)
        return;
    d->control = std::make_unique<::compatibility::detail::QuickRenderControl>(d->view);
    d->window = std::make_unique<QQuickWindow>(d->control.get());
    d->window->setColor(d->quick ? d->quick->quickWindow()->color() : Qt::transparent);
    d->window->setObjectName(QStringLiteral("qmlTextureWindow"));
    connect(d->control.get(), &QQuickRenderControl::renderRequested, this, [this] { markDirty(); });
    connect(d->control.get(), &QQuickRenderControl::sceneChanged, this, [this] { markDirty(); });
    connect(d->window.get(), &QQuickWindow::focusObjectChanged, d->view, [view = d->view](QObject* focused) {
        if (!view)
            return;
        const auto* item = qobject_cast<QQuickItem*>(focused);
        view->setAttribute(Qt::WA_InputMethodEnabled, item && item->inputMethodQuery(Qt::ImEnabled).toBool());
    });
    if (d->root)
        d->root->setParentItem(d->window->contentItem());
}

bool QmlTextureRenderer::attach(QQuickWidget* quick)
{
    if (!quick || !quick->rootObject() || d->detached)
        return false;
    d->quick = quick;
    d->root = quick->rootObject();
    // Window-attached QML callbacks can unload while setParentItem() is running.
    // Keep its destination alive until that public operation has returned.
    // zh_CN: 窗口关联的 QML 回调可能在视觉重父过程中卸载，须保活目标至操作返回。
    QScopedValueRollback<bool> attaching(d->rendering, true);
    createWindow();
    d->rendering = false;
    if (d->releasePending || d->detached)
        releaseNow(d->detached);
    return d->root && d->window && d->root->window() == d->window.get() && !d->detached;
}

void QmlTextureRenderer::markDirty()
{
    if (d->detached || d->releasing)
        return;
    const bool alreadyDirty = d->dirty;
    d->dirty = true;
    if (!alreadyDirty && d->view)
        emit d->view->textureUpdateRequested();
}

void QmlTextureRenderer::detach()
{
    d->detached = true;
    if (d->rendering) {
        d->releasePending = true;
        return;
    }
    releaseNow(true);
}

void QmlTextureRenderer::release()
{
    if (d->rendering) {
        d->releasePending = true;
        return;
    }
    releaseNow(false);
}

void QmlTextureRenderer::releaseNow(bool restore)
{
    if (d->releasing)
        return;
    QScopedValueRollback<bool> releasing(d->releasing, true);
    disconnect(d->contextDestroyed);
    const QPointer<QOpenGLContext> previous(QOpenGLContext::currentContext());
    QSurface* previousSurface = previous ? previous->surface() : nullptr;
    const QPointer<QObject> previousSurfaceOwner(!previousSurface ? nullptr
            : previousSurface->surfaceClass() == QSurface::Window
            ? static_cast<QObject*>(static_cast<QWindow*>(previousSurface))
            : static_cast<QObject*>(static_cast<QOffscreenSurface*>(previousSurface)));
    const QPointer<QOpenGLContext> owner(d->context);
    if (owner && owner != previous && owner->isValid() && d->cleanupSurface)
        owner->makeCurrent(d->cleanupSurface.get());
    const QPointer<QQuickItem> root(d->root);
    const QPointer<QQuickWidget> quick(d->quick);
    if (root && d->window && root->window() == d->window.get())
        root->setParentItem(quick ? quick->quickWindow()->contentItem() : nullptr);
    if (d->initialized && d->control)
        d->control->invalidate();
    d->initialized = false;
    if (d->window) {
        ::compatibility::detail::resetQuickRenderTarget(d->window.get());
    }
    d->target.reset();
    // The window owns render-target resources backed by the control's QRhi.
    // Destroy those first, then the QRhi, while its borrowed context is alive.
    // zh_CN: 先清理窗口拥有的渲染目标，再销毁 QRhi，二者均须早于借用上下文失效。
    d->window.reset();
    d->control.reset();
    d->context.clear();
    d->cleanupSurface.reset();
    d->pixels = QSize();
    d->bytes = 0;
    d->dpr = 0;
    d->dirty = true;
    d->releasePending = false;
    if (!restore && !d->detached)
        createWindow();
    // The saved surface can be this renderer's retired cleanup surface.
    // zh_CN: 保存的表面可能就是刚被本渲染器销毁的清理表面，恢复前须确认其仍存活。
    if (previous && previous->isValid() && previousSurfaceOwner)
        previous->makeCurrent(previousSurface);
    else if (auto* current = QOpenGLContext::currentContext())
        current->doneCurrent();
}

bool QmlTextureRenderer::render(QOpenGLContext* context, const QSize& logicalSize, qreal dpr, quint64 budget)
{
    const auto fail = [this](const QString& error) {
        d->error = error;
        release();
        return false;
    };
    if (d->rendering || d->releasing || d->detached || !d->root || !d->quick)
        return false;
    if (!context || !context->isValid() || QOpenGLContext::currentContext() != context)
        return fail(QStringLiteral("Texture rendering requires the caller's current OpenGL context."));
    if (logicalSize.isEmpty() || !std::isfinite(dpr) || dpr <= 0 || dpr > 4)
        return fail(QStringLiteral("Texture size and device pixel ratio must be valid."));
    createWindow();
    if (!d->window || !d->root || d->detached)
        return false;
    if (d->window->rendererInterface()->graphicsApi() != QSGRendererInterface::OpenGL)
        return fail(QStringLiteral("Texture rendering requires Qt Quick's OpenGL backend."));
    if (d->view && !::compatibility::detail::quickSupportsIndependentTargetDpr()
        && !qFuzzyCompare(dpr, d->view->devicePixelRatioF()))
        return fail(QStringLiteral("This Qt version requires the presenting window's device pixel ratio."));
    auto* gl = context->functions();
    // The context is borrowed from a compositor. Errors already pending here
    // belong to its preceding work, not to this QML frame.
    // zh_CN: 上下文由合成器借入，先单独报告此前遗留的错误，避免误判本次 QML 绘制。
    constexpr int maximumInheritedErrors = 16;
    for (int count = 0; count < maximumInheritedErrors; ++count) {
        const GLenum error = gl->glGetError();
        if (error == GL_NO_ERROR)
            break;
        qCWarning(lcQmlTexture) << "OpenGL error preceding QML rendering:" << error;
        if (count + 1 == maximumInheritedErrors)
            return fail(QStringLiteral("The borrowed OpenGL context keeps reporting errors before QML rendering."));
    }
    TargetScope targetScope(context);
    GLint limit = 0;
    gl->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
    if (logicalSize.width() * dpr > limit || logicalSize.height() * dpr > limit)
        return fail(QStringLiteral("QML texture exceeds the OpenGL texture size limit."));
    const QSize pixels(qCeil(logicalSize.width() * dpr), qCeil(logicalSize.height() * dpr));
    // RGBA8 + a conservative separate depth/stencil allocation; QML images are separate.
    const quint64 bytes = quint64(pixels.width()) * quint64(pixels.height()) * 12;
    if (bytes > budget)
        return fail(QStringLiteral("QML texture exceeds the compositor's byte budget."));
    if (d->context && d->context != context) {
        releaseNow(false);
        if (!d->root || d->detached || QOpenGLContext::currentContext() != context)
            return false;
    }
    d->control->targetDpr = dpr;
    QScopedValueRollback<bool> rendering(d->rendering, true);
    d->control->renderPhase = true;
    if (!d->initialized) {
        d->context = context;
        // Quick creates its own fallback surface when importing the context.
        // Its EGL configuration must match the compositor's actual format.
        // zh_CN: Quick 导入上下文时会创建备用表面，EGL 配置须匹配合成器的实际格式。
        d->window->setFormat(context->format());
        d->cleanupSurface = std::make_unique<QOffscreenSurface>();
        d->cleanupSurface->setFormat(context->format());
        d->cleanupSurface->create();
        d->contextDestroyed =
            connect(context, &QOpenGLContext::aboutToBeDestroyed, this, [this] { release(); }, Qt::DirectConnection);
        d->initialized = ::compatibility::detail::initializeQuickOpenGLRendering(d->window.get(), d->control.get(),
            context);
        if (!d->initialized) {
            d->error = QStringLiteral("Qt Quick could not initialize the OpenGL texture renderer.");
            d->releasePending = true;
        }
    }
    if (d->initialized && !d->releasePending && !d->detached && d->root) {
        d->window->setColor(d->quick->quickWindow()->color());
        d->window->setGeometry(0, 0, logicalSize.width(), logicalSize.height());
        d->window->contentItem()->setSize(logicalSize);
        if (!d->releasePending && !d->detached && d->root)
            d->root->setSize(logicalSize);
        if (!d->releasePending && !d->detached && d->root
            && (!d->target || d->pixels != pixels || !qFuzzyCompare(d->dpr, dpr))) {
            ::compatibility::detail::resetQuickRenderTarget(d->window.get());
            d->target.reset();
            const auto format = ::compatibility::detail::quickFramebufferFormat();
            d->target = std::make_unique<QOpenGLFramebufferObject>(pixels, format);
            if (!d->target->isValid()) {
                d->error = QStringLiteral("QML texture target allocation failed.");
                d->releasePending = true;
            } else {
                d->pixels = pixels;
                d->dpr = dpr;
                d->bytes = bytes;
                d->dirty = true;
                ::compatibility::detail::setQuickRenderTarget(d->window.get(), d->target.get(), dpr);
            }
        }
    }
    if (d->initialized && !d->releasePending && !d->detached && d->root && d->dirty) {
        d->control->polishItems();
        if (!d->releasePending && !d->detached && d->root) {
            // Geometry/binding changes during polish belong to this sync. Only
            // requests raised during sync/render need another frame afterwards.
            // zh_CN: polish 中的几何/绑定变更由本次同步消费，仅同步/绘制中新增请求需保留。
            d->dirty = false;
            ::compatibility::detail::beginQuickFrame(d->control.get());
            d->control->sync();
            if (!d->releasePending && !d->detached && d->root)
                d->control->render();
            ::compatibility::detail::endQuickFrame(d->window.get(), d->control.get());
            if (!d->releasePending && !d->detached && d->root) {
                if (gl->glGetError() != GL_NO_ERROR || !gl->glIsTexture(d->target->texture())) {
                    d->error = QStringLiteral("OpenGL could not render the QML texture.");
                    d->releasePending = true;
                } else {
                    ++d->frames;
                    d->error.clear();
                }
            }
        }
    }
    d->control->renderPhase = false;
    d->rendering = false;
    if (d->releasePending || d->detached)
        releaseNow(d->detached);
    return textureId() != 0 && !d->detached;
}

QQuickWindow* QmlTextureRenderer::window() const
{
    return d->window.get();
}
unsigned int QmlTextureRenderer::textureId() const
{
    return d->target && !d->releasePending && !d->detached ? d->target->texture() : 0;
}
QSize QmlTextureRenderer::textureSize() const
{
    return textureId() ? d->pixels : QSize();
}
quint64 QmlTextureRenderer::frameCount() const
{
    return d->frames;
}
QString QmlTextureRenderer::lastError() const
{
    return d->error;
}
QVariantMap QmlTextureRenderer::statistics() const
{
    return {{QStringLiteral("active"), textureId() != 0}, {QStringLiteral("dirty"), d->dirty},
        {QStringLiteral("allocatedBytes"), QVariant::fromValue(d->bytes)},
        {QStringLiteral("renderedFrames"), QVariant::fromValue(d->frames)},
        {QStringLiteral("pixelWidth"), textureSize().width()}, {QStringLiteral("pixelHeight"), textureSize().height()},
        {QStringLiteral("devicePixelRatio"), d->dpr}, {QStringLiteral("lastError"), d->error},
        {QStringLiteral("backend"), QStringLiteral("opengl")}};
}
} // namespace fluent::qmlbridge
