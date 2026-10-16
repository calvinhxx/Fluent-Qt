#include "compatibility/private/quick/QuickCompat_p.h"
#include "QmlTextureLayer.h"
#include "QmlView.h"

#include <QApplication>
#include <QGraphicsEffect>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLTextureBlitter>
#include <QPointer>
#include <QRegion>
#include <QThread>
#include <QWindow>
#include <QtMath>
#include <limits>
#include <vector>

namespace fluent::qmlbridge {
namespace {
constexpr quint64 kUnsupportedBytes = std::numeric_limits<quint64>::max();

class CurrentContextScope {
public:
    CurrentContextScope()
        : context(QOpenGLContext::currentContext()), surface(context ? context->surface() : nullptr),
          surfaceOwner(!surface ? nullptr
                  : surface->surfaceClass() == QSurface::Window
                  ? static_cast<QObject*>(static_cast<QWindow*>(surface))
                  : static_cast<QObject*>(static_cast<QOffscreenSurface*>(surface)))
    {}
    ~CurrentContextScope()
    {
        if (context && context->isValid() && surfaceOwner) {
            if (QOpenGLContext::currentContext() == context && context->surface() == surface)
                return;
            if (context->makeCurrent(surface))
                return;
        }
        if (auto* current = QOpenGLContext::currentContext())
            current->doneCurrent();
    }

private:
    QPointer<QOpenGLContext> context;
    QSurface* surface;
    QPointer<QObject> surfaceOwner;
};

bool onGuiThread(const QObject* object)
{
    return qApp && object->thread() == qApp->thread() && QThread::currentThread() == qApp->thread();
}

QList<QmlView*> pageViews(QWidget* root)
{
    if (!root)
        return {};
    auto result = root->findChildren<QmlView*>();
    if (auto* view = qobject_cast<QmlView*>(root))
        result.prepend(view);
    return result;
}

QRegion visibleRegion(QWidget* widget, QWidget* root)
{
    if (!widget || !root || !root->isVisible() || !widget->isVisibleTo(root))
        return {};
    QRegion result = widget->visibleRegion().translated(widget->mapTo(root, QPoint()));
    for (auto* current = widget; current; current = current->parentWidget()) {
        const QPoint origin = current->mapTo(root, QPoint());
        result &= QRect(origin, current->size());
        if (!current->mask().isEmpty())
            result &= current->mask().translated(origin);
        if (current == root)
            break;
    }
    return result;
}

bool supportedHierarchy(QmlView* view, QWidget* root)
{
    if (!view || !root || (view != root && !root->isAncestorOf(view)))
        return false;
    const QRect bounds(view->mapTo(root, QPoint()), view->size());
    for (auto* current = static_cast<QWidget*>(view); current; current = current->parentWidget()) {
        if (current->graphicsEffect())
            return false;
        if (current == root)
            break;
        auto* parent = current->parentWidget();
        bool above = false;
        for (auto* child : parent->children()) {
            auto* sibling = qobject_cast<QWidget*>(child);
            if (sibling == current) {
                above = true;
                continue;
            }
            if (!above || !sibling || sibling->isWindow() || !view->isVisible() || !sibling->isVisible()
                || qobject_cast<QmlView*>(sibling))
                continue;
            // Transparent native siblings also need correct blending. Keep them
            // on Qt's direct path rather than painting QML over their foreground.
            // zh_CN: 透明兄弟控件也需要正确混合；留在 Qt 直接合成路径，不让 QML 覆盖其前景。
            if (QRect(sibling->mapTo(root, QPoint()), sibling->size()).intersects(bounds))
                return false;
        }
    }
    return true;
}

QSize targetSize(const QSizeF& logical, qreal dpr)
{
    if (logical.isEmpty() || !qIsFinite(dpr) || dpr <= 0 || dpr > 4 || !qIsFinite(logical.width())
        || !qIsFinite(logical.height()) || logical.width() * dpr > std::numeric_limits<int>::max()
        || logical.height() * dpr > std::numeric_limits<int>::max())
        return {};
    return QSize(qCeil(logical.width() * dpr), qCeil(logical.height() * dpr));
}

struct FramebufferState {
    explicit FramebufferState(QOpenGLContext* context) : context(context), gl(context->extraFunctions())
    {
        separate = context->format().majorVersion() >= 3;
        gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &draw);
        if (separate)
            gl->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        gl->glGetIntegerv(GL_VIEWPORT, viewport);
        scissorEnabled = gl->glIsEnabled(GL_SCISSOR_TEST);
        blendEnabled = gl->glIsEnabled(GL_BLEND);
        depthEnabled = gl->glIsEnabled(GL_DEPTH_TEST);
        stencilEnabled = gl->glIsEnabled(GL_STENCIL_TEST);
        cullEnabled = gl->glIsEnabled(GL_CULL_FACE);
        gl->glGetIntegerv(GL_BLEND_SRC_RGB, &blendSourceRgb);
        gl->glGetIntegerv(GL_BLEND_DST_RGB, &blendDestinationRgb);
        gl->glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSourceAlpha);
        gl->glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestinationAlpha);
        gl->glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb);
        gl->glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
        gl->glGetIntegerv(GL_SCISSOR_BOX, scissor);
        gl->glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    }
    ~FramebufferState()
    {
        if (!context || QOpenGLContext::currentContext() != context)
            return;
        if (separate) {
            gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(draw));
            gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(read));
        } else {
            gl->glBindFramebuffer(GL_FRAMEBUFFER, GLuint(draw));
        }
        gl->glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        gl->glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        gl->glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        gl->glBlendFuncSeparate(blendSourceRgb, blendDestinationRgb, blendSourceAlpha, blendDestinationAlpha);
        gl->glBlendEquationSeparate(blendEquationRgb, blendEquationAlpha);
        const auto restore = [this](GLenum state, bool enabled) {
            if (enabled)
                gl->glEnable(state);
            else
                gl->glDisable(state);
        };
        restore(GL_SCISSOR_TEST, scissorEnabled);
        restore(GL_BLEND, blendEnabled);
        restore(GL_DEPTH_TEST, depthEnabled);
        restore(GL_STENCIL_TEST, stencilEnabled);
        restore(GL_CULL_FACE, cullEnabled);
    }
    QPointer<QOpenGLContext> context;
    QOpenGLExtraFunctions* gl;
    GLint draw = 0, read = 0, viewport[4] = {}, scissor[4] = {};
    GLboolean colorMask[4] = {};
    GLint blendSourceRgb = 0, blendDestinationRgb = 0, blendSourceAlpha = 0;
    GLint blendDestinationAlpha = 0, blendEquationRgb = 0, blendEquationAlpha = 0;
    bool separate = false, scissorEnabled = false, blendEnabled = false;
    bool depthEnabled = false, stencilEnabled = false, cullEnabled = false;
};
} // namespace

struct QmlTextureLayer::Private {
    struct Frame {
        QPointer<QmlView> view;
        QPointer<QObject> scene;
        unsigned int texture = 0;
        QRectF bounds;
        QRegion clip;
        quint64 revision = 0;
    };
    QPointer<QWidget> root;
    QPointer<QOpenGLContext> context;
    std::unique_ptr<QOffscreenSurface> cleanupSurface;
    QList<QPointer<QmlView>> managed;
    QList<QPointer<QmlView>> selected;
    std::vector<Frame> frames;
    std::vector<Frame> composedFrames;
    std::unique_ptr<QOpenGLFramebufferObject> target;
    std::unique_ptr<QOpenGLTextureBlitter> blitter;
    QMetaObject::Connection contextDestroyed;
    QRectF source;
    qreal dpr = 1;
    quint64 baseRevision = 0;
    unsigned int baseTexture = 0;
    quint64 compositions = 0;
    quint64 bytes = 0;
    bool busy = false;
    bool failed = false;
    bool closing = false;
    bool pendingRoot = false;
    bool pendingClear = false;
    bool pendingRelease = false;
    quint64 generation = 0;

    struct Operation {
        Operation(std::shared_ptr<Private> state, QmlTextureLayer* owner) : state(std::move(state)), owner(owner)
        {
            this->state->busy = true;
        }
        ~Operation()
        {
            state->busy = false;
            state->flushPending(owner);
        }
        std::shared_ptr<Private> state;
        QPointer<QmlTextureLayer> owner;
        CurrentContextScope restoreContext;
    };

    void flushPending(const QPointer<QmlTextureLayer>& owner)
    {
        if (busy || (!pendingRoot && !pendingClear && !pendingRelease && !closing))
            return;
        const CurrentContextScope restoreContext;
        busy = true;
        const bool rootChanged = pendingRoot;
        const bool releaseRequested = pendingRelease || closing || !owner;
        pendingRoot = pendingClear = pendingRelease = false;
        frames.clear();
        if (rootChanged || releaseRequested)
            selected.clear();
        clearGpu();
        // Take a guarded copy: a QML window-change callback may change the page,
        // request release, or delete this layer while GPU targets are retired.
        // zh_CN: QML 窗口变更回调可切页、释放或销毁本层，清理期间须使用受保护的快照。
        const auto views = managed;
        for (const auto& view : views)
            if (view)
                view->releaseTexture();
        if (releaseRequested || pendingRelease || closing || !owner) {
            managed.clear();
            selected.clear();
            for (const auto& view : views) {
                if (!view)
                    continue;
                if (owner)
                    QObject::disconnect(view, nullptr, owner, nullptr);
                view->setTextureRenderingEnabled(false);
            }
            failed = false;
        }
        // Coalesce callback requests. Every old GPU target is already gone and
        // no new frame can be allocated while busy, so another drain is needless.
        // zh_CN: 合并回调中的请求；旧 GPU 目标已全部清空，busy 期间无法分配新帧，无须循环排空。
        if (rootChanged || pendingRoot) {
            selected.clear();
            failed = false;
        }
        pendingRoot = pendingClear = pendingRelease = false;
        busy = false;
    }

    void clearGpu()
    {
        auto* previous = QOpenGLContext::currentContext();
        auto* previousSurface = previous ? previous->surface() : nullptr;
        auto* owner = context.data();
        const bool switched = owner && owner != previous && owner->isValid() && cleanupSurface
            && owner->makeCurrent(cleanupSurface.get());
        target.reset();
        blitter.reset();
        if (switched) {
            if (previous && previous->isValid() && previousSurface)
                previous->makeCurrent(previousSurface);
            else
                owner->doneCurrent();
        }
        composedFrames.clear();
        baseRevision = 0;
        baseTexture = 0;
        bytes = 0;
    }
};

QmlTextureLayer::QmlTextureLayer(QObject* parent) : QObject(parent), d(std::make_shared<Private>()) {}
QmlTextureLayer::~QmlTextureLayer()
{
    const auto state = d;
    state->closing = state->pendingRelease = state->pendingClear = true;
    ++state->generation;
    state->flushPending(QPointer<QmlTextureLayer>());
}
QWidget* QmlTextureLayer::rootWidget() const
{
    return d->root;
}
void QmlTextureLayer::setRootWidget(QWidget* root)
{
    if (!onGuiThread(this) || d->root == root || (root && root->thread() != thread()))
        return;
    const auto state = d;
    const QPointer<QmlTextureLayer> lifetime(this);
    state->root = root;
    ++state->generation;
    state->pendingRoot = state->pendingClear = true;
    state->flushPending(lifetime);
    if (lifetime)
        emit frameRequested();
}
bool QmlTextureLayer::synchronize()
{
    const auto state = d;
    if (!onGuiThread(this) || state->busy || state->failed)
        return false;
    const QPointer<QWidget> root(state->root);
    const quint64 generation = state->generation;
    QList<QPointer<QmlView>> views;
    for (auto* view : pageViews(root))
        views.append(view);
    state->selected.clear();
    for (const auto& view : views) {
        state->selected.append(view);
        if (!supportedHierarchy(view, root))
            return false;
    }
    QPointer<QmlTextureLayer> lifetime(this);
    Private::Operation operation(state, this);
    bool ready = true;
    for (const QPointer<QmlView>& view : views) {
        if (!view)
            continue;
        bool managed = false;
        for (const auto& previous : state->managed)
            managed |= previous == view;
        if (!managed) {
            state->managed.append(view);
            connect(view, &QmlView::textureUpdateRequested, this, [this, guarded = view] {
                if (guarded && d->root && (guarded == d->root || d->root->isAncestorOf(guarded))
                    && !visibleRegion(guarded, d->root).isEmpty())
                    emit frameRequested();
            });
            connect(view, &QmlView::textureRenderingFailed, this, [this](const QString& error) { fail(error); });
            connect(view, &QObject::destroyed, this, &QmlTextureLayer::frameRequested);
        }
        const bool enabled = view->setTextureRenderingEnabled(true);
        if (!lifetime || state->generation != generation || state->root != root)
            return false;
        const bool unloaded = view && view->status() == QmlView::Unloaded && !view->rootObject();
        ready &= unloaded || (enabled && view && !view->property("fluentqtDirectCompositionRequired").toBool());
    }
    return ready;
}

quint64 QmlTextureLayer::requiredBytes(const QRectF& source, qreal dpr) const
{
    if (!d->root)
        return 0;
    quint64 bytes = 0;
    for (const auto& view : d->selected) {
        if (!view)
            continue;
        if (view->status() != QmlView::Ready || visibleRegion(view, d->root).isEmpty())
            continue;
        const QSize pixels = targetSize(view->size(), dpr);
        if (pixels.isEmpty() || !supportedHierarchy(view, d->root))
            return kUnsupportedBytes;
        if (!::compatibility::detail::quickSupportsIndependentTargetDpr()
            && qAbs(dpr - view->devicePixelRatioF()) > .001)
            return kUnsupportedBytes;
        const quint64 allocation = quint64(pixels.width()) * quint64(pixels.height()) * 12;
        if (allocation > kUnsupportedBytes - bytes)
            return kUnsupportedBytes;
        bytes += allocation;
    }
    if (!bytes)
        return 0;
    const QSize panel = targetSize(source.size(), dpr);
    if (panel.isEmpty())
        return kUnsupportedBytes;
    const quint64 allocation = quint64(panel.width()) * quint64(panel.height()) * 4;
    return allocation > kUnsupportedBytes - bytes ? kUnsupportedBytes : bytes + allocation;
}

bool QmlTextureLayer::fail(const QString& message)
{
    if (d->failed)
        return false;
    d->failed = true;
    emit renderingFailed(message);
    return false;
}

bool QmlTextureLayer::prepareFrame(QOpenGLContext* context, const QRectF& source, qreal dpr, quint64 maximumBytes)
{
    const auto state = d;
    if (!onGuiThread(this) || state->busy || state->failed || !context || QOpenGLContext::currentContext() != context)
        return false;
    const QPointer<QmlTextureLayer> lifetime(this);
    const QPointer<QWidget> root(state->root);
    const auto selected = state->selected;
    const quint64 generation = state->generation;
    Private::Operation operation(state, this);
    const auto current = [&] {
        return lifetime && state->generation == generation && state->root == root
            && QOpenGLContext::currentContext() == context;
    };
    const quint64 required = requiredBytes(source, dpr);
    if (required == kUnsupportedBytes || required > maximumBytes)
        return fail(QStringLiteral("QML panel textures exceed the supported size or byte limit."));
    if (!required) {
        if (!state->frames.empty() || state->target || state->bytes)
            state->pendingClear = true;
        return current();
    }
    FramebufferState restore(context);
    if (state->context != context) {
        state->clearGpu();
        state->frames.clear();
        if (state->context) {
            const auto managed = state->managed;
            for (const auto& view : managed) {
                if (view)
                    view->releaseTexture();
                if (!current())
                    return false;
            }
        }
        disconnect(state->contextDestroyed);
        state->context = context;
        state->cleanupSurface = std::make_unique<QOffscreenSurface>();
        state->cleanupSurface->setFormat(context->format());
        state->cleanupSurface->create();
        state->contextDestroyed = connect(
            context, &QOpenGLContext::aboutToBeDestroyed, this,
            [this] {
                const auto state = d;
                clearFrameCaches();
                state->context.clear();
                state->cleanupSurface.reset();
            },
            Qt::DirectConnection);
    }
    state->frames.clear();
    if (state->source != source || state->dpr != dpr) {
        state->clearGpu();
        state->source = source;
        state->dpr = dpr;
    }
    for (const auto& view : selected) {
        if (!current())
            return false;
        if (!view || view->status() != QmlView::Ready)
            continue;
        const QRegion clip = visibleRegion(view, root) & source.toAlignedRect();
        if (clip.isEmpty()) {
            view->releaseTexture();
            if (!current())
                return false;
            continue;
        }
        if (!view->isTextureRenderingEnabled() || !view->renderTexture(context, dpr, maximumBytes)) {
            if (!current())
                return false;
            if (!view || view->status() != QmlView::Ready)
                continue;
            return fail(QStringLiteral("The QML scene could not render into the panel context."));
        }
        if (!current())
            return false;
        if (!view) {
            return false;
        }
        if (!root || (view != root && !root->isAncestorOf(view))) {
            state->pendingClear = true;
            ++state->generation;
            return false;
        }
        state->frames.push_back({view, view->rootObject(), view->textureId(),
            QRectF(view->mapTo(root, QPoint()), view->size()), clip, view->textureRevision()});
    }
    state->bytes = required;
    if (state->frames.empty())
        state->clearGpu();
    return current() && !state->failed;
}

unsigned int QmlTextureLayer::compose(unsigned int baseTexture, const QSize& pixels, quint64 baseRevision)
{
    const auto state = d;
    auto* context = state->context.data();
    if (!onGuiThread(this) || state->busy || state->failed || !baseTexture)
        return 0;
    if (state->frames.empty())
        return baseTexture;
    if (!context || QOpenGLContext::currentContext() != context)
        return 0;
    const QPointer<QmlTextureLayer> lifetime(this);
    const QPointer<QWidget> root(state->root);
    const quint64 generation = state->generation;
    const auto frames = state->frames;
    Private::Operation operation(state, this);
    const auto current = [&] {
        return lifetime && state->generation == generation && state->root == root
            && QOpenGLContext::currentContext() == context;
    };
    if (pixels != targetSize(state->source.size(), state->dpr)) {
        fail(QStringLiteral("The QML composition target does not match the panel pixel size."));
        return 0;
    }
    bool unchanged = state->target && state->target->size() == pixels && state->baseTexture == baseTexture
        && state->baseRevision == baseRevision && state->composedFrames.size() == frames.size();
    for (size_t i = 0; unchanged && i < frames.size(); ++i) {
        const auto& before = state->composedFrames[i];
        const auto& after = frames[i];
        unchanged = before.view == after.view && before.scene == after.scene && before.texture == after.texture
            && before.bounds == after.bounds && before.clip == after.clip && before.revision == after.revision;
    }
    if (unchanged)
        return state->target->texture();
    FramebufferState restore(context);
    auto* gl = context->extraFunctions();
    if (!state->target || state->target->size() != pixels) {
        state->target.reset();
        QOpenGLFramebufferObjectFormat format;
        format.setInternalTextureFormat(GL_RGBA8);
        state->target = std::make_unique<QOpenGLFramebufferObject>(pixels, format);
        if (!state->target->isValid()) {
            fail(QStringLiteral("Could not allocate the QML panel composition target."));
            return 0;
        }
    }
    if (!state->blitter) {
        state->blitter = std::make_unique<QOpenGLTextureBlitter>();
        if (!state->blitter->create()) {
            fail(QStringLiteral("Could not initialize the QML texture compositor."));
            return 0;
        }
    }
    state->target->bind();
    gl->glViewport(0, 0, pixels.width(), pixels.height());
    gl->glDisable(GL_SCISSOR_TEST);
    gl->glDisable(GL_DEPTH_TEST);
    gl->glDisable(GL_STENCIL_TEST);
    gl->glDisable(GL_CULL_FACE);
    gl->glDisable(GL_BLEND);
    gl->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state->blitter->bind();
    const QRect viewport(QPoint(), pixels);
    state->blitter->blit(baseTexture, QOpenGLTextureBlitter::targetTransform(QRectF(viewport), viewport),
        QOpenGLTextureBlitter::OriginBottomLeft);
    gl->glEnable(GL_BLEND);
    gl->glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    gl->glBlendEquation(GL_FUNC_ADD);
    for (const auto& frame : frames) {
        if (!current())
            return 0;
        if (!frame.view || !frame.view->textureId())
            continue;
        const QRectF target((frame.bounds.x() - state->source.x()) * state->dpr,
            (frame.bounds.y() - state->source.y()) * state->dpr, frame.bounds.width() * state->dpr,
            frame.bounds.height() * state->dpr);
        QRegion pixelClip;
        for (const QRect& clip : frame.clip) {
            const int left = qMax(0, qFloor((clip.x() - state->source.x()) * state->dpr));
            const int top = qMax(0, qFloor((clip.y() - state->source.y()) * state->dpr));
            const int right = qMin(pixels.width(), qCeil((clip.x() + clip.width() - state->source.x()) * state->dpr));
            const int bottom = qMin(pixels.height(),
                qCeil((clip.y() + clip.height() - state->source.y()) * state->dpr));
            pixelClip |= QRect(left, top, right - left, bottom - top);
        }
        gl->glEnable(GL_SCISSOR_TEST);
        const auto transform = QOpenGLTextureBlitter::targetTransform(target, viewport);
        for (const QRect& clip : pixelClip) {
            gl->glScissor(clip.x(), pixels.height() - clip.y() - clip.height(), clip.width(), clip.height());
            state->blitter->blit(frame.view->textureId(), transform, QOpenGLTextureBlitter::OriginBottomLeft);
        }
    }
    state->blitter->release();
    if (!current())
        return 0;
    state->baseTexture = baseTexture;
    state->baseRevision = baseRevision;
    state->composedFrames = frames;
    ++state->compositions;
    return state->target->texture();
}

unsigned int QmlTextureLayer::textureId() const
{
    return d->target ? d->target->texture() : 0;
}
QSize QmlTextureLayer::textureSize() const
{
    return d->target ? d->target->size() : QSize();
}
QVariantMap QmlTextureLayer::statistics() const
{
    quint64 frames = 0;
    int views = 0;
    for (const auto& view : d->managed) {
        if (view) {
            ++views;
            frames += view->textureStatistics().value(QStringLiteral("renderedFrames")).toULongLong();
        }
    }
    return {{QStringLiteral("views"), views}, {QStringLiteral("visibleViews"), int(d->frames.size())},
        {QStringLiteral("renderedFrames"), frames},
        {QStringLiteral("compositions"), QVariant::fromValue(d->compositions)},
        {QStringLiteral("allocatedBytes"), QVariant::fromValue(d->bytes)}};
}
void QmlTextureLayer::clearFrameCaches()
{
    if (!onGuiThread(this))
        return;
    const auto state = d;
    ++state->generation;
    state->pendingClear = true;
    state->flushPending(QPointer<QmlTextureLayer>(this));
}
void QmlTextureLayer::release()
{
    const auto state = d;
    if (!onGuiThread(this))
        return;
    ++state->generation;
    state->pendingRelease = state->pendingClear = true;
    state->flushPending(QPointer<QmlTextureLayer>(this));
}
} // namespace fluent::qmlbridge
