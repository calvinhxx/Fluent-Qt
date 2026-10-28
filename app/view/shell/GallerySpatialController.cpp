#include "GallerySpatialController.h"
#include "compatibility/QtCompat.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/overlay/OverlayPresentation.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include "components/foundation/overlay/OverlayWindow.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/StackContentHost.h"
#include "components/windowing/WindowBackdrop.h"
#include "components/windowing/Window.h"
#include "view/support/GalleryDepth.h"
#include "view/support/GalleryToast.h"
#include "GallerySplashScreen.h"
#include "viewmodel/GallerySettings.h"
#include "GallerySpatialRenderPolicy.h"
#include "GalleryGlyphPaintDevice.h"
#include "GalleryPanelSampler.h"
#include "GalleryParticleCompositor.h"
#include "components/spatial/SpatialRuntime.h"
#include "components/spatial/SpatialSession.h"
#include "support/spatial/SpatialTextureHost.h"
#include "platform/GalleryPlatform.h"
#ifdef FLUENT_QT_HAS_QML_BRIDGE
#include <FluentQt/QmlBridge.h>
#endif

#include <QApplication>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QDynamicPropertyChangeEvent>
#include <QFrame>
#include <QGraphicsEffect>
#include <QHash>
#include <QHelpEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLFunctions>
#include <QOffscreenSurface>
#include <QWindow>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPainterPath>
#include <QOpenGLFramebufferObject>
#include <QOpenGLPaintDevice>
#include <QOpenGLTexture>
#include <QPaintEngine>
#include <QMenu>
#include <QScreen>
#include <QProxyStyle>
#include <QStyleOption>
#include <QScopedValueRollback>
#include <QSurfaceFormat>
#include <utility>
#include <QTimer>
#include <QTouchEvent>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QtMath>
#include <functional>
#include <algorithm>

namespace fluent::gallery {
namespace {
constexpr qreal kSideRotation = 5.0;
constexpr qreal kTopRotation = 3.0;
constexpr qreal kPointerYaw = .65;
constexpr qreal kPointerPitch = .45;
constexpr char kDirectCompositionRequiredProperty[] = "fluentqtDirectCompositionRequired";
constexpr char kQmlFailurePendingProperty[] = "galleryQmlFailurePending";
// The navigation pixel dirtied to bring a backing-store pass to the GPU surface, with
// room for fractional device-pixel rounding.
constexpr QRect kSyntheticPassRect(0, 0, 2, 2);

bool requiresDirectComposition(const QWidget* root)
{
    if (!root)
        return false;
    if (root->property(kDirectCompositionRequiredProperty).toBool())
        return true;
    for (auto* child : root->findChildren<QWidget*>()) {
        if (child->property(kDirectCompositionRequiredProperty).toBool())
            return true;
    }
    return false;
}

class DirectCompositionObserver final : public QObject {
public:
    explicit DirectCompositionObserver(QObject* parent) : QObject(parent) { qApp->installEventFilter(this); }
    std::function<void(QWidget*, bool)> changed;
#ifdef Q_OS_WASM
    std::function<void(QObject*)> windowReady;
#endif

protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
#ifdef Q_OS_WASM
        if (windowReady && (event->type() == QEvent::Show || event->type() == QEvent::Expose))
            windowReady(object);
#endif
        const bool propertyChanged = event->type() == QEvent::DynamicPropertyChange
            && static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName() == kDirectCompositionRequiredProperty;
        if (!propertyChanged && event->type() != QEvent::ParentChange)
            return false;
        auto* widget = qobject_cast<QWidget*>(object);
        if (widget && changed && (propertyChanged || widget->property(kDirectCompositionRequiredProperty).toBool()))
            changed(widget, propertyChanged);
        return false;
    }
};

QString currentRendererName(QOpenGLContext* context)
{
    if (!context || QOpenGLContext::currentContext() != context)
        return {};
    const QString hostRenderer = platform::graphicsRendererOverride();
    if (!hostRenderer.isEmpty())
        return hostRenderer;
    return spatial::SpatialRuntime::currentRendererName();
}

QString sessionUnavailableReason()
{
    if (qEnvironmentVariableIntValue("FLUENT_QT_GALLERY_DISABLE_3D") != 0)
        return QObject::tr("3D is disabled for this session.");
    const QString graphicsFailure = qApp->property("fluentqtGalleryGraphicsBackendFailure").toString();
    if (!graphicsFailure.isEmpty())
        return graphicsFailure;
    if (!spatial::SpatialRuntime::supportsOpenGLDisplay())
        return QObject::tr("This display uses the 2D Gallery.");
    return {};
}

QString accelerationUnavailableReason()
{
    return spatial::SpatialRuntime::preflightFailure();
}

// Suppress backing-store painting and invalidate the GPU cache. During a GPU render pass,
// drawSource sends the widget tree directly to the OpenGL painter, without a CPU snapshot.
// zh_CN: 拦截后备存储绘制并使 GPU 缓存失效；实际绘制时直接输出到 OpenGL，避免整页 CPU 位图。
class SurfaceCapture final : public QGraphicsEffect {
public:
    // Receives the changed region in the captured widget's coordinates; empty means all of it.
    // zh_CN: 收到被捕获控件坐标中的变化区域；空区域表示整块变化。
    std::function<void(const QRegion&)> invalidated;
    QPointer<QWidget> target;
    bool rendering = false, composing = false;
    QPoint renderOrigin;
    const bool measuring = qEnvironmentVariableIntValue("FLUENT_QT_SPATIAL_BENCHMARK") != 0;
    qint64 captures = 0, captureNanoseconds = 0;

protected:
    void draw(QPainter* painter) override
    {
        if (rendering) {
            // Qt applies the root effect's target offset again in drawSource.
            // Cancel it here; descendants keep their strip-local clip coordinates.
            // zh_CN: 抵消 drawSource 重复应用的根偏移，子特效仍使用条带内裁剪坐标。
            painter->save();
            painter->translate(renderOrigin);
            drawSource(painter);
            painter->restore();
        } else if (!composing && invalidated) {
            invalidated(spatial::SpatialTextureHost::graphicsEffectDirtyRegion(painter, target));
        }
    }
};

struct ShellScene : FluentElement {
    struct Panel {
        QRectF source;
        QTransform transform;
    };
    quint64 navigationRevision = 0, contentRevision = 0;
    spatial_render::PanelDamage navigationDamage, contentDamage;
    QPixmap backdrop;
    Panel navigation, content;
    qreal progress = 0;
    QPointF pointerTilt;
    bool top = false;
    std::function<void()> themeChanged;
    std::function<void(QPainter&, bool, const QRegion&)> renderWidgets;
    // Whether an enabled graphics effect inside a panel intersects a panel-local area.
    std::function<bool(bool, const QRect&)> hasNestedEffects;
    QPointer<QWidget> contentRoot;
    spatial_render::GalleryParticleCompositor::RenderWidget renderForeground;
    void onThemeUpdated() override
    {
        if (themeChanged)
            themeChanged();
    }

    // Retire a panel texture; `dirty` is panel-local, and empty means the whole panel.
    // zh_CN: 使面板纹理失效；dirty 为面板局部坐标，空区域表示整块面板。
    void markDirty(bool isNavigation, const QRegion& dirty = {})
    {
        (isNavigation ? navigationDamage : contentDamage).add(dirty);
        ++(isNavigation ? navigationRevision : contentRevision);
    }
    void resetRevisions()
    {
        navigationRevision = contentRevision = 0;
        navigationDamage = {};
        contentDamage = {};
    }
    // Attribute a dirty region of the navigation view to the panels it covers. Both
    // textures are retired when Qt could not say what changed.
    // zh_CN: 将导航视图中的脏区分配给所覆盖的面板；Qt 无法给出变化范围时两块纹理都失效。
    void invalidate(const QRegion& dirty, const QRect& contentBounds)
    {
        // Revision zero means the panel was never captured; its first capture is whole.
        // zh_CN: 版本为零表示面板尚未捕获，首次捕获必须完整。
        if (dirty.isEmpty() || !navigationRevision || !contentRevision) {
            markDirty(true);
            markDirty(false);
            return;
        }
        const QRect navigationBounds = navigation.source.toAlignedRect();
        const QRegion navigationDirty = dirty & navigationBounds;
        const QRegion contentDirty = dirty & contentBounds;
        if (!navigationDirty.isEmpty())
            markDirty(true, navigationDirty.translated(-navigationBounds.topLeft()));
        if (!contentDirty.isEmpty())
            markDirty(false, contentDirty.translated(-contentBounds.topLeft()));
    }

    static QTransform
    project(const QRectF& rect, qreal angle, bool top, qreal progress, const QPointF& pointerTilt, bool navigation)
    {
        if (rect.isEmpty() || progress == 0)
            return {};
        const QPointF center = rect.center();
        // A long lens keeps text readable and avoids extreme foreshortening of the top rail.
        // zh_CN: 较长的焦距保持文字可读，避免顶部窄导航栏出现夸张的透视变形。
        const qreal camera = qMax(1400.0, qMax(rect.width(), rect.height()) * 6.0);
        QMatrix4x4 rotation;
        rotation.rotate(float(((top ? angle : 0) + pointerTilt.y()) * progress), 1, 0, 0);
        rotation.rotate(float(((top ? 0 : angle) + pointerTilt.x()) * progress), 0, 1, 0);
        QPolygonF original;
        original << rect.topLeft() << rect.topRight() << rect.bottomRight() << rect.bottomLeft();
        QPolygonF projected;
        for (const auto& corner : original) {
            const auto p = rotation.map(QVector3D(corner.x() - center.x(), corner.y() - center.y(), 0));
            const qreal scale = camera / (camera - p.z());
            projected << QPointF(p.x() * scale, p.y() * scale);
        }
        // Fit inside each existing layout region. This leaves a material-filled gutter and
        // keeps the outer edges, scrollbars and navigation footer inside the window.
        // zh_CN: 在原布局区域内留出材质间隙，外缘、滚动条与底部导航均不会越出窗口。
        const qreal horizontalMargin = (navigation ? (top ? 5.0 : qMin(3.0, rect.width() * 6.0 / rect.height())) : 6.0)
            * progress;
        // A thin rail must not shrink in both axes just to create a vertical gutter.
        // zh_CN: 顶部窄导航不能为了竖向留白而等比缩小整条导航及文字。
        const qreal verticalMargin = (navigation && top ? qMin(.5, rect.height() * 5.0 / rect.width()) : 6.0)
            * progress;
        const QRectF available = rect.adjusted(horizontalMargin, verticalMargin, -horizontalMargin, -verticalMargin);
        const QRectF bounds = projected.boundingRect();
        const qreal fit = qMin(available.width() / bounds.width(), available.height() / bounds.height());
        for (auto& point : projected)
            point = available.center() + (point - bounds.center()) * fit;
        QTransform result;
        QTransform::quadToQuad(original, projected, result);
        return result;
    }

    void layout(navigation::NavigationView* view)
    {
        top = view->effectiveDisplayMode() == navigation::NavigationView::DisplayMode::Top;
        content.source = view->contentHost()->geometry();
        // Compact navigation opens OVER the content. Follow the actual animated chrome,
        // not content.left(), which remains at the collapsed rail width.
        // zh_CN: 紧凑导航展开时覆盖正文，按正在动画中的窗格宽度投影，不能按正文左边界切分。
        qreal paneWidth = 0;
        for (auto* chrome : {view->headerChromeWidget(), view->mainChromeWidget(), view->footerChromeWidget()}) {
            if (chrome && !chrome->isHidden())
                paneWidth = qMax(paneWidth, qreal(chrome->geometry().right() + 1));
        }
        navigation.source =
            top ? QRectF(0, 0, view->width(), content.source.top()) : QRectF(0, 0, paneWidth, view->height());
        const qreal angle = top ? kTopRotation : kSideRotation;
        navigation.transform = project(navigation.source, angle, top, progress, pointerTilt, true);
        content.transform = project(content.source, -angle, top, progress, pointerTilt, false);
    }

    QPointF projectPoint(QPointF point) const
    {
        return (content.source.contains(point) ? content : navigation).transform.map(point);
    }
    const Panel* unproject(QPointF point, QPointF* result) const
    {
        for (const auto* panel : {&navigation, &content}) {
            const auto source = panel->transform.inverted().map(point);
            if (panel->source.contains(source)) {
                *result = source;
                return panel;
            }
        }
        return nullptr;
    }
    void paint(QPainter& painter, const std::function<void(QPainter&, const Panel&, bool)>& texture) const
    {
        if (!navigationRevision)
            return;
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        for (const auto* current : {&content, &navigation}) {
            const auto& panel = *current;
            if (panel.source.isEmpty())
                continue;
            const bool isNavigation = current == &navigation;
            const qreal radius = themeRadius().overlay * 1.5;
            const QRectF body = panel.source.adjusted(.5, .5, -.5, -.5);
            QPainterPath outline;
            outline.addRoundedRect(body, radius, radius);
            painter.save();
            // Leave the outer window edge clear for the native Mica/Acrylic compositor.
            // zh_CN: 保持窗口最外缘透明，让原生 Mica/Acrylic 合成器接管背景。
            if (progress > 0)
                painter.setClipRect(navigation.source.united(content.source).adjusted(1, 1, -1, -1), Qt::IntersectClip);
            painter.setTransform(panel.transform);
            // Keep the shadow outside the translucent pane so its interior does not turn muddy.
            // zh_CN: 柔影仅落在面板外侧，不叠入半透明材质内部，避免玻璃发灰。
            QPainterPath shadowArea;
            shadowArea.addRect(body.adjusted(-20, -20, 20, 24));
            shadowArea.addPath(outline);
            shadowArea.setFillRule(Qt::OddEvenFill);
            painter.save();
            painter.setClipPath(shadowArea, Qt::IntersectClip);
            overlay::paintLayeredShadow(painter, body.toAlignedRect(), radius, themeShadow(Elevation::High),
                (isNavigation ? .045 : .065) * progress, 10, 3);
            painter.restore();
            // Material, widget coverage and the thin reflection share one
            // projected edge in the sampler. Separate QPainter paths acquire
            // different subpixel coverage under a perspective transform.
            texture(painter, panel, isNavigation);
            painter.restore();
        }
    }
};

class RhiSurface final : public spatial::SpatialTextureHost {
public:
    RhiSurface(ShellScene* scene, QWidget* parent) : SpatialTextureHost(parent), m_scene(scene)
    {
        m_fallback.setSingleShot(true);
        m_fallback.setInterval(50);
        connect(&m_fallback, &QTimer::timeout, this, [this] {
            if (m_paintPending)
                requestFrame();
        });
    }
    std::function<void()> requestBackingStorePass;
    // Same backing-store pass policy as the OpenGL surface.
    // zh_CN: 与 OpenGL 表面相同的后备存储绘制策略。
    void requestFrameInPass(bool inPaintPass)
    {
        if (property("presenting").toBool() && (inPaintPass || requestBackingStorePass)
            && scheduleFrameInBackingStorePass()) {
            if (!m_paintPending && !inPaintPass)
                requestBackingStorePass();
            m_paintPending = true;
            // Arm once per pending frame; continuous requests must not postpone it.
            // zh_CN: 每个待绘帧只启动一次，连续请求不能推迟回退。
            if (!m_fallback.isActive())
                m_fallback.start();
            return;
        }
        requestFrame();
    }

protected:
    void synchronizeFrame() override
    {
        m_paintPending = false;
        m_fallback.stop();
        const bool presenting = property("presenting").toBool() && m_scene->navigationRevision;
        setLayerCount(presenting ? 3 : 1);
        const auto& backdrop = m_scene->backdrop;
        const QSizeF size = backdrop.isNull() ? QSizeF() : QSizeF(backdrop.size()) / backdrop.devicePixelRatioF();
        setLayer(0, QRectF(QPointF(), size), {}, quint64(backdrop.cacheKey()));
        setSceneClip(presenting && m_scene->progress > 0
                ? m_scene->navigation.source.united(m_scene->content.source).adjusted(1, 1, -1, -1)
                : QRectF());
        if (!presenting)
            return;
        const auto colors = m_scene->themeColors();
        const bool dark = m_scene->effectiveThemeUsesDarkAppearance();
        for (int i : {1, 2}) {
            const bool nav = i == 2;
            const auto& panel = nav ? m_scene->navigation : m_scene->content;
            QColor material = colors.bgLayerAlt;
            material.setAlphaF(nav ? (dark ? .32 : .34) : (dark ? .62 : .56));
            QColor reflection = colors.grey10;
            reflection.setAlphaF(dark ? .18 : .72);
            const quint64 revision = nav ? m_scene->navigationRevision : m_scene->contentRevision;
            setLayer(i, panel.source, panel.transform, revision, m_scene->themeRadius().overlay * 1.5, material,
                reflection, m_scene->progress, nav ? .06 : .085);
            const auto changes = (nav ? m_scene->navigationDamage : m_scene->contentDamage).take(revision);
            setLayerDamage(i, changes.base, changes.full ? QRegion() : changes.region);
        }
    }
    void paintLayer(QPainter* painter, int index, const QRegion& region) override
    {
        if (index == 0) {
            painter->drawPixmap(QPointF(), m_scene->backdrop);
        } else if (m_scene->renderWidgets) {
            const bool nav = index == 2;
            const QPoint origin = nav ? QPoint() : m_scene->content.source.topLeft().toPoint();
            // QWidget::render places the region's top-left at the painter origin.
            // zh_CN: QWidget::render 将区域左上角绘制在画笔原点。
            painter->translate(region.boundingRect().topLeft());
            m_scene->renderWidgets(*painter, nav, region.translated(-origin));
        }
    }

private:
    ShellScene* m_scene;
    QTimer m_fallback;
    bool m_paintPending = false;
};

void requestCanvasFrame(QWidget* canvas, bool withInput = false, bool inPaintPass = false);

using spatial_render::PanelSampler;

// Create the shared GL surface only after opting into 3D. Keep it hidden between
// later toggles to avoid repeatedly replacing the native window's backing store.
// zh_CN: 首次启用 3D 才创建共享 GL 表面；后续关闭时隐藏，避免反复重建原生窗口后备存储。
class GpuSurface final : public QOpenGLWidget {
public:
    GpuSurface(ShellScene* scene, QWidget* parent, spatial::SpatialSession* session)
        : QOpenGLWidget(parent), m_scene(scene), m_particles(this, session)
    {
#ifdef FLUENT_QT_HAS_QML_BRIDGE
        m_qml = new qmlbridge::QmlTextureLayer(this);
        connect(m_qml, &qmlbridge::QmlTextureLayer::frameRequested, this, [this] { requestFrame(false); });
        connect(m_qml, &qmlbridge::QmlTextureLayer::renderingFailed, this, [this](const QString&) {
            setProperty(kQmlFailurePendingProperty, true);
            if (qmlCompositionFailed)
                qmlCompositionFailed(m_qml->rootWidget());
        });
#endif
        connect(&m_particles, &spatial_render::GalleryParticleCompositor::frameRequested, this, [this] {
            requestFrame(false);
        });
        connect(&m_particles, &spatial_render::GalleryParticleCompositor::staticContentInvalidated, this, [this] {
            m_scene->markDirty(false);
            requestFrame(false);
        });
        m_fallback.setSingleShot(true);
        m_fallback.setInterval(50);
        connect(&m_fallback, &QTimer::timeout, this, [this] {
            // No backing-store pass reached this surface; queue it normally.
            // zh_CN: 没有后备存储绘制经过本表面，改用常规排队更新。
            if (m_paintPending)
                update();
        });
        m_clock.start();
        m_refine.setSingleShot(true);
        m_refine.setInterval(spatial_render::kRefineDelayMs);
        connect(&m_refine, &QTimer::timeout, this, [this] {
            // Repaint rows drawn at native density during continuous updates.
            // zh_CN: 连续更新停止后，按缓存密度重画原生密度期间绘制的行。
            bool refined = false;
            for (auto* cache : {&m_navigation, &m_content}) {
                if (cache->degraded.isEmpty() || !(cache->dpr > 0))
                    continue;
                QRegion logical;
                for (const QRect& rows : cache->degraded)
                    logical += QRectF(0, rows.top() / cache->dpr, cache->source.width(), rows.height() / cache->dpr)
                                   .toAlignedRect();
                // A refinement is not part of the continuous update that degraded it.
                cache->updatedAtMs = -1;
                m_scene->markDirty(cache == &m_navigation, logical);
                refined = true;
            }
            if (refined) {
                ++refinements;
                requestFrame(false);
            }
        });
        m_interactive = !measuring || qEnvironmentVariable("FLUENT_QT_SPATIAL_INTERACTIVE") != QLatin1String("0");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        // Keep the completed surface while an auxiliary frame is being prepared.
        // Successful frames still clear every pixel before drawing.
        // zh_CN: 准备附加纹理期间保留完整画面；成功帧仍先清空再全量绘制。
        setUpdateBehavior(PartialUpdate);
        auto surfaceFormat = format();
        int samples = 4;
        if (qEnvironmentVariableIntValue("FLUENT_QT_SPATIAL_BENCHMARK")) {
            bool ok = false;
            const int requested = qEnvironmentVariableIntValue("FLUENT_QT_SPATIAL_SAMPLES", &ok);
            if (ok && (requested == 0 || requested == 2 || requested == 4))
                samples = requested;
        }
        surfaceFormat.setSamples(samples);
        surfaceFormat.setAlphaBufferSize(8);
        setFormat(surfaceFormat);
    }

    ~GpuSurface() override
    {
        if (context())
            disconnect(context(), nullptr, this, nullptr);
        makeCurrent();
        releaseTextures();
        doneCurrent();
    }

    std::function<void()> initialized;
    std::function<void()> contextLost;
    std::function<void()> failed;
    std::function<void()> cacheUnavailable;
    std::function<void(QWidget*)> qmlCompositionFailed;
#ifdef FLUENT_QT_HAS_QML_BRIDGE
    qmlbridge::QmlTextureLayer* qmlLayer() const { return m_qml; }
    QRectF qmlSource() const
    {
        auto* root = m_qml->rootWidget();
        return root && m_scene->contentRoot
            ? QRectF(root->mapFrom(m_scene->contentRoot, QPoint()), m_scene->content.source.size())
            : QRectF();
    }
#endif
    qreal cacheDpr() const { return m_plan.dpr; }
    quint32 contentTextureId() const { return m_content.texture ? m_content.texture->texture() : 0; }
    int maxCacheDimension() const { return m_maxDimension; }
    int allocationRetries = 0;
    int surfaceSamples = 0;
    int paintSamples() const { return m_paintTarget ? m_paintTarget->format().samples() : 0; }
    int paintSampleBytes() const { return m_paintSampleBytes; }
    int glyphCacheBytes() const { return m_glyphImages.cost(); }
    qint64 estimatedCacheBytes() const
    {
        qint64 bytes = qMax(m_plan.estimatedBytes, backdropBytes()) + qint64(m_particles.allocatedBytes());
#ifdef FLUENT_QT_HAS_QML_BRIDGE
        bytes += m_qml->statistics().value("allocatedBytes").toLongLong();
#endif
        return bytes;
    }
    const spatial_render::GalleryParticleCompositor& particles() const { return m_particles; }
    qint64 backdropBytes() const
    {
        return m_backdrop.texture ? qint64(m_backdrop.size.width()) * m_backdrop.size.height() * 4 : 0;
    }
    int paintTargetHeight() const { return m_plan.paintSize.height(); }
    bool ready() const { return m_blitter && m_blitter->isCreated(); }
    qint64 cachedPixels() const
    {
        qint64 pixels = 0;
        for (const auto* cache : {&m_navigation, &m_content})
            if (cache->texture)
                pixels += qint64(cache->texture->width()) * cache->texture->height();
        return pixels;
    }

    void clearFrameCaches()
    {
        m_inputFrameRequested = false;
        m_hasPresentedFrame = false;
        m_refine.stop();
        makeCurrent();
        m_glyphImages.clear();
        m_particles.release();
#ifdef FLUENT_QT_HAS_QML_BRIDGE
        m_qml->clearFrameCaches();
#endif
        m_presentedContentTexture = 0;
        m_navigation = {};
        m_content = {};
        m_paintTarget.reset();
        m_resolveTarget.reset();
        m_backdrop = {};
        m_plan = {};
        m_maxExtraSampling = 2;
        m_cacheFailurePending = false;
        doneCurrent();
    }
    const bool measuring = qEnvironmentVariableIntValue("FLUENT_QT_SPATIAL_BENCHMARK") != 0;
    qint64 paints = 0, paintNanoseconds = 0;
    qint64 allocationNanoseconds = 0, firstPaintNanoseconds = 0;
    qint64 backdropUploads = 0;
    // Always-on counters: texture rows repainted, partial and native-density updates.
    // zh_CN: 常驻计数：重画的纹理行数、局部更新与原生密度更新次数。
    qint64 navigationPaintedRows = 0, contentPaintedRows = 0;
    qint64 partialUpdates = 0, interactiveUpdates = 0, refinements = 0;
    qreal updateCostMs() const { return m_slowFrames.costMs; }
    int slowFrames() const { return m_slowFrames.streak; }
    // Reports, once per surface, that even native-density updates stay too slow.
    // zh_CN: 原生密度更新仍持续过慢时通知一次。
    std::function<void()> slowRendering;

    // Raster-dirties a pixel beneath this surface so a backing-store pass reaches it.
    std::function<void()> requestBackingStorePass;

    // While presenting, paint inside a backing-store pass instead of queuing this texture
    // widget: Qt repaints everything beneath a queued texture widget whenever another
    // widget is dirty in the same pass, which would retire both panel textures. A pass
    // already in progress (inPaintPass) reaches this surface after the panels beneath it.
    // zh_CN: 呈现时在后备存储绘制中完成本表面绘制，而不是排队纹理控件；排队时只要同一次绘制
    // 还有其他脏控件，Qt 就会重绘其下方全部区域并作废两块面板纹理。进行中的绘制会在面板之后经过本表面。
    void requestFrame(bool withInput, bool inPaintPass = false)
    {
        if (m_inputFrameRequested)
            return;
        m_inputFrameRequested = withInput;
        const bool passAvailable = inPaintPass || requestBackingStorePass;
        if (property("presenting").toBool() && passAvailable
            && spatial::SpatialTextureHost::schedulePaintInBackingStorePass(this)) {
            if (!m_paintPending && !inPaintPass)
                requestBackingStorePass();
            m_paintPending = true;
            // Arm once per pending frame; continuous requests must not postpone it.
            // zh_CN: 每个待绘帧只启动一次，连续请求不能推迟回退。
            if (!m_fallback.isActive())
                m_fallback.start();
            return;
        }
        update();
    }

protected:
    void resizeGL(int, int) override { m_hasPresentedFrame = false; }

    void initializeGL() override
    {
        m_glyphImages.clear();
        m_hasPresentedFrame = false;
        m_surfaceSamplesKnown = false;
        auto* gl = context()->functions();
        m_maxDimension = spatial::SpatialRuntime::maximumTextureDimension();
        // A driver may round the requested sample count up. Query a tiny target
        // before planning large allocations so the memory bound remains accurate.
        QOpenGLFramebufferObjectFormat sampleFormat;
        sampleFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        sampleFormat.setInternalTextureFormat(GL_RGBA8);
        sampleFormat.setSamples(spatial_render::kPaintSamples);
        {
            QOpenGLFramebufferObject sampleProbe(QSize(1, 1), sampleFormat);
            m_paintSamples = sampleProbe.isValid() ? sampleProbe.format().samples() : 0;
            if (sampleProbe.isValid() && sampleProbe.bind()) {
                m_paintSampleBytes = spatial::SpatialTextureHost::currentPaintSampleBytes();
                sampleProbe.release();
            }
        }
        // Retain the old conservative plan only for same-device performance comparisons.
        // zh_CN: 旧保守估算仅用于同设备性能对照。
        if (measuring && qEnvironmentVariableIsSet("FLUENT_QT_SPATIAL_CONSERVATIVE_CACHE"))
            m_paintSampleBytes = 12;
        m_blitter = std::make_unique<PanelSampler>();
        const bool reference = measuring && qgetenv("FLUENT_QT_SPATIAL_SAMPLER") == "monotone";
        m_blitter->create(
            reference ? PanelSampler::Reconstruction::Monotone : PanelSampler::Reconstruction::BoundedCubic);
        connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this] {
            makeCurrent();
            releaseTextures();
            doneCurrent();
            if (contextLost)
                contextLost();
        });
        if (initialized)
            initialized();
    }

    void paintGL() override
    {
        m_inputFrameRequested = false;
        m_paintPending = false;
        m_fallback.stop();
        QElapsedTimer clock;
        if (measuring)
            clock.start();
        auto* gl = context()->functions();
        // The final FBO keeps the context's sample format across paints/resizes.
        // Avoid a synchronous driver query on every pointer-animation frame.
        if (!m_surfaceSamplesKnown) {
            gl->glGetIntegerv(GL_SAMPLES, &surfaceSamples);
            m_surfaceSamplesKnown = true;
        }
        // Retain the native-sized backdrop independently of Qt's pixmap texture
        // cache. Some ES sharing contexts report NPOT support inconsistently and
        // otherwise rescale/reupload this unchanged image on every motion frame.
        // zh_CN: 独立保留原始密度背景纹理，避免 ES 共享上下文反复缩放和上传同一背景。
        const auto& backdrop = m_scene->backdrop;
        if (m_backdrop.key != backdrop.cacheKey() || m_backdrop.size != backdrop.size()
            || m_backdrop.dpr != backdrop.devicePixelRatioF() || m_backdrop.context != context())
            m_backdrop = {};
        if (property("presenting").toBool()) {
            if (!prepareCaches()) {
                if (!m_cacheFailurePending && cacheUnavailable) {
                    m_cacheFailurePending = true;
                    cacheUnavailable();
                }
                return;
            }
            const auto remaining = qMax<qint64>(0, spatial_render::kCacheBudgetBytes - m_plan.estimatedBytes);
#ifdef FLUENT_QT_HAS_QML_BRIDGE
            const auto qmlBytes = m_qml->requiredBytes(qmlSource(), m_plan.dpr);
#else
            const quint64 qmlBytes = 0;
#endif
            const bool acceleratedParticles = m_particles.prepare(m_scene->contentRoot, m_scene->contentRevision,
                m_plan.sizes[1], devicePixelRatioF(), m_plan.dpr,
                quint64(qMax<qint64>(0, remaining - qint64(qmlBytes))),
                !measuring || !qEnvironmentVariableIsSet("FLUENT_QT_SPATIAL_PARTICLES_CPU"));
#ifdef FLUENT_QT_HAS_QML_BRIDGE
            const QPointer<GpuSurface> lifetime(this);
            const bool qmlReady = m_qml->prepareFrame(context(), qmlSource(), m_plan.dpr,
                quint64(qMax<qint64>(0, remaining - qint64(m_particles.allocatedBytes()))));
            if (!lifetime)
                return;
            if (!property("presenting").toBool()) {
                bindSurfaceTarget();
                return;
            }
            if (!qmlReady && m_hasPresentedFrame) {
                bindSurfaceTarget();
                return;
            }
            // A first/resized FBO has no previous frame. Finish its Widgets and
            // particle panels while native QML restoration waits for the event loop.
            // zh_CN: 首帧或缩放后无旧画面，先完成普通控件与粒子面板，回调退出后恢复原生 QML。
#endif
            const bool valid = ready()
                && updateTexture(m_content, m_scene->contentRevision, m_scene->content.source, false)
                && updateTexture(m_navigation, m_scene->navigationRevision, m_scene->navigation.source, true);
            if (!valid) {
                if (failed)
                    failed();
                return;
            }
            m_presentedContentTexture = m_content.texture ? m_content.texture->texture() : 0;
            if (acceleratedParticles) {
                const auto revision = m_scene->contentRevision;
                m_presentedContentTexture = m_particles.compose(m_presentedContentTexture, revision,
                    m_paintTarget.get(), m_resolveTarget.get(), m_scene->renderForeground);
                // A composition failure restores CPU painting synchronously. Do
                // not display the particle-free static cache for that frame.
                // zh_CN: 合成失败同步恢复 CPU 绘制，本帧不能显示省略粒子的静态缓存。
                if (revision != m_scene->contentRevision && !m_particles.activeLayerCount()) {
                    if (!updateTexture(m_content, m_scene->contentRevision, m_scene->content.source, false)) {
                        if (failed)
                            failed();
                        return;
                    }
                    m_presentedContentTexture = m_content.texture ? m_content.texture->texture() : 0;
                }
            }
            finishTextureFrame();
#ifdef FLUENT_QT_HAS_QML_BRIDGE
            if (qmlReady) {
                const auto qmlRevision = m_scene->contentRevision + m_particles.compositionCount();
                const auto texture = m_qml->compose(m_presentedContentTexture, m_plan.sizes[1], qmlRevision);
                if (!lifetime)
                    return;
                if (!property("presenting").toBool()) {
                    bindSurfaceTarget();
                    return;
                }
                if (texture)
                    m_presentedContentTexture = texture;
                else if (m_hasPresentedFrame) {
                    bindSurfaceTarget();
                    return;
                }
            }
#endif
        }
        if (!updateBackdrop()) {
            if (failed)
                failed();
            return;
        }
        bindSurfaceTarget();
        // Do not touch the displayed FBO until frame preparation succeeds or a
        // first-frame fallback is ready. QPainter can leave scissor/color masks.
        // zh_CN: 准备完成或首帧回退可用后才清屏，同时清除 QPainter 遗留的裁剪和颜色掩码。
        gl->glDisable(GL_SCISSOR_TEST);
        gl->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        gl->glClearColor(0, 0, 0, 0);
        gl->glClear(GL_COLOR_BUFFER_BIT);
        QPainter painter(this);
        if (m_backdrop.texture) {
            painter.beginNativePainting();
            gl->glEnable(GL_BLEND);
            gl->glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            QMatrix4x4 projection;
            projection.ortho(0.f, float(width()), float(height()), 0.f, -1.f, 1.f);
            const QSizeF logicalSize = QSizeF(m_backdrop.size) / m_backdrop.dpr;
            QMatrix4x4 quad;
            quad.translate(logicalSize.width() / 2, logicalSize.height() / 2);
            // Uploaded QImage rows start at the top, unlike the panel FBOs.
            quad.scale(logicalSize.width() / 2, logicalSize.height() / 2);
            m_blitter->blit(m_backdrop.texture->textureId(), m_backdrop.size, projection * quad);
            painter.endNativePainting();
        }
        if (property("presenting").toBool()) {
            const auto colors = m_scene->themeColors();
            const bool dark = m_scene->effectiveThemeUsesDarkAppearance();
            m_scene->paint(painter, [this, &colors, dark](QPainter& p, const ShellScene::Panel& panel, bool nav) {
                auto& cache = nav ? m_navigation : m_content;
                if (!cache.texture)
                    return;
                p.beginNativePainting();
                auto* gl = context()->functions();
                gl->glEnable(GL_BLEND);
                gl->glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                QMatrix4x4 projection;
                projection.ortho(0.f, float(width()), float(height()), 0.f, -1.f, 1.f);
                QMatrix4x4 quad;
                quad.translate(panel.source.center().x(), panel.source.center().y());
                quad.scale(panel.source.width() / 2, -panel.source.height() / 2);
                const PanelSampler::Appearance appearance = {panel.source.size(), m_scene->themeRadius().overlay * 1.5,
                    m_scene->progress, colors.bgLayerAlt, colors.grey10, nav ? (dark ? .32 : .34) : (dark ? .62 : .56),
                    dark ? .18 : .72};
                m_blitter->blit(
                    !nav && m_presentedContentTexture ? m_presentedContentTexture : cache.texture->texture(),
                    cache.texture->size(), projection * QMatrix4x4(panel.transform) * quad, &appearance);
                p.endNativePainting();
            });
        }
        painter.end();
        m_hasPresentedFrame = property("presenting").toBool();
        if (measuring) {
            if (!firstPaintNanoseconds && property("presenting").toBool())
                firstPaintNanoseconds = clock.nsecsElapsed();
            ++paints;
            paintNanoseconds += clock.nsecsElapsed();
        }
    }

private:
#ifdef FLUENT_QT_HAS_QML_BRIDGE
    qmlbridge::QmlTextureLayer* m_qml = nullptr;
#endif
    struct TextureCache {
        std::unique_ptr<QOpenGLFramebufferObject> texture;
        quint64 revision = 0;
        QRectF source;
        qreal dpr = 0;
        // Texture rows last painted at native density, as one-pixel-wide rectangles.
        QRegion degraded;
        qint64 updatedAtMs = -1;
    };
    TextureCache m_navigation, m_content;
    struct BackdropCache {
        std::unique_ptr<QOpenGLTexture> texture;
        qint64 key = 0;
        QSize size;
        qreal dpr = 0;
        QPointer<QOpenGLContext> context;
    };
    BackdropCache m_backdrop;
    std::unique_ptr<PanelSampler> m_blitter;
    std::unique_ptr<QOpenGLFramebufferObject> m_paintTarget;
    std::unique_ptr<QOpenGLFramebufferObject> m_resolveTarget;
    ShellScene* m_scene;
    spatial_render::GalleryParticleCompositor m_particles;
    spatial_render::GlyphImageCache m_glyphImages;
    GLuint m_presentedContentTexture = 0;
    spatial_render::CachePlan m_plan;
    int m_maxDimension = 0;
    int m_paintSamples = spatial_render::kPaintSamples;
    int m_paintSampleBytes = 12;
    qreal m_maxExtraSampling = 2;
    bool m_cacheFailurePending = false;
    bool m_surfaceSamplesKnown = false;
    bool m_hasPresentedFrame = false;
    bool m_inputFrameRequested = false;
    bool m_interactive = true;
    bool m_paintPending = false;
    bool m_frameInteractive = false;
    qreal m_frameUpdateMs = 0;
    spatial_render::SlowFrameTracker m_slowFrames;
    QElapsedTimer m_clock;
    QTimer m_refine;
    QTimer m_fallback;

    void bindSurfaceTarget()
    {
        auto* gl = context()->functions();
        gl->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
        gl->glViewport(0, 0, qRound(width() * devicePixelRatioF()), qRound(height() * devicePixelRatioF()));
    }

    bool prepareCaches()
    {
        if (m_paintSamples <= 1)
            return false;
        const std::array<QSizeF, 2> panels = {m_scene->navigation.source.size(), m_scene->content.source.size()};
        const auto backdrop = qint64(m_scene->backdrop.width()) * m_scene->backdrop.height() * 4;
        const auto glyphScratch =
            spatial_render::needsNativeGlyphCoverage(devicePixelRatioF()) ? spatial_render::kGlyphWorkingSetBytes : 0;
        if (!glyphScratch)
            m_glyphImages.clear();
        qreal extraSampling = spatial_render::preferredExtraSampling(devicePixelRatioF());
        if (measuring) {
            bool ok = false;
            const qreal requested = qEnvironmentVariable("FLUENT_QT_SPATIAL_EXTRA_SAMPLING").toDouble(&ok);
            if (ok && requested >= 1 && requested <= 2)
                extraSampling = requested;
        }
        while (true) {
            auto plan = spatial_render::planCaches(panels, devicePixelRatioF(), m_maxDimension,
                qMin(m_maxExtraSampling, extraSampling), spatial_render::kCacheBudgetBytes, m_paintSamples, backdrop,
                glyphScratch, 0, m_paintSampleBytes);
            if (!plan.valid())
                return false;
            for (qreal extra : {2., 1.75, 1.5, 1.25, 1.}) {
                if (devicePixelRatioF() * extra > plan.dpr)
                    continue;
                const auto candidate = spatial_render::planCaches(panels, devicePixelRatioF(), m_maxDimension, extra,
                    spatial_render::kCacheBudgetBytes, m_paintSamples, backdrop, glyphScratch, 0, m_paintSampleBytes);
                if (!candidate.valid() || candidate.dpr != devicePixelRatioF() * extra)
                    continue;
                const quint64 particleBytes = m_particles.requiredBytes(m_scene->contentRoot, m_scene->contentRevision,
                    candidate.sizes[1], devicePixelRatioF(), candidate.dpr,
                    !measuring || !qEnvironmentVariableIsSet("FLUENT_QT_SPATIAL_PARTICLES_CPU"));
#ifdef FLUENT_QT_HAS_QML_BRIDGE
                const quint64 qmlBytes = m_qml->requiredBytes(qmlSource(), candidate.dpr);
#else
                const quint64 qmlBytes = 0;
#endif
                if (qmlBytes >= quint64(spatial_render::kCacheBudgetBytes))
                    continue;
                const quint64 auxiliaryBytes = particleBytes + qmlBytes;
                if (!auxiliaryBytes)
                    break;
                if (auxiliaryBytes >= quint64(spatial_render::kCacheBudgetBytes))
                    continue;
                const auto joint = spatial_render::planCaches(panels, devicePixelRatioF(), m_maxDimension, extra,
                    spatial_render::kCacheBudgetBytes, m_paintSamples, backdrop, glyphScratch, qint64(auxiliaryBytes),
                    m_paintSampleBytes);
                // Choose the highest joint level, always at least native density.
                // Re-estimate auxiliary textures at each sampling candidate.
                if (joint.valid() && joint.dpr == candidate.dpr) {
                    plan = joint;
                    break;
                }
            }
            if (plan.sizes == m_plan.sizes && plan.dpr == m_plan.dpr && plan.paintSize == m_plan.paintSize
                && plan.backdropBytes == m_plan.backdropBytes && plan.glyphScratchBytes == m_plan.glyphScratchBytes)
                return true;
            // Release both old targets before allocating replacements: window resizing
            // must not transiently retain two complete sets of high-DPI caches.
            m_particles.release();
#ifdef FLUENT_QT_HAS_QML_BRIDGE
            m_qml->clearFrameCaches();
#endif
            m_presentedContentTexture = 0;
            m_navigation = {};
            m_content = {};
            m_paintTarget.reset();
            m_resolveTarget.reset();
            m_plan = {};
            QElapsedTimer allocationClock;
            if (measuring)
                allocationClock.start();
            bool allocated = true;
            QOpenGLFramebufferObjectFormat paintFormat;
            paintFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            paintFormat.setInternalTextureFormat(GL_RGBA8);
            paintFormat.setSamples(m_paintSamples);
            m_paintTarget = std::make_unique<QOpenGLFramebufferObject>(plan.paintSize, paintFormat);
            QOpenGLFramebufferObjectFormat textureFormat;
            textureFormat.setAttachment(QOpenGLFramebufferObject::NoAttachment);
            textureFormat.setInternalTextureFormat(GL_RGBA8);
            m_resolveTarget = std::make_unique<QOpenGLFramebufferObject>(plan.paintSize, textureFormat);
            allocated = m_paintTarget->isValid() && m_resolveTarget->isValid();
            const std::array<TextureCache*, 2> caches = {&m_navigation, &m_content};
            for (size_t i = 0; i < caches.size(); ++i) {
                if (!allocated)
                    break;
                if (plan.sizes[i].isEmpty())
                    continue;
                auto& texture = caches[i]->texture;
                texture = std::make_unique<QOpenGLFramebufferObject>(plan.sizes[i], textureFormat);
                if (!texture->isValid()) {
                    allocated = false;
                    break;
                }
                auto* gl = context()->functions();
                gl->glBindTexture(GL_TEXTURE_2D, texture->texture());
                gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                gl->glBindTexture(GL_TEXTURE_2D, 0);
            }
            if (measuring)
                allocationNanoseconds += allocationClock.nsecsElapsed();
            if (allocated) {
                m_plan = plan;
                return true;
            }
            m_navigation = {};
            m_content = {};
            ++allocationRetries;
            m_maxExtraSampling = plan.dpr / devicePixelRatioF() - .25;
        }
    }

    void releaseTextures()
    {
        m_hasPresentedFrame = false;
        m_refine.stop();
        m_glyphImages.clear();
        m_particles.release();
#ifdef FLUENT_QT_HAS_QML_BRIDGE
        m_qml->clearFrameCaches();
#endif
        m_presentedContentTexture = 0;
        m_navigation = {};
        m_content = {};
        m_paintTarget.reset();
        m_resolveTarget.reset();
        m_backdrop = {};
        m_plan = {};
        m_maxExtraSampling = 2;
        m_cacheFailurePending = false;
        m_blitter.reset();
    }

    bool updateBackdrop()
    {
        const auto& pixmap = m_scene->backdrop;
        if (pixmap.isNull() || m_backdrop.texture)
            return true;
        const QSize size = pixmap.size();
        const qint64 bytes = qint64(size.width()) * size.height() * 4;
        if (!ready() || size.width() > m_maxDimension || size.height() > m_maxDimension
            || bytes > spatial_render::kCacheBudgetBytes)
            return false;
        const QImage image = pixmap.toImage().convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        if (image.isNull())
            return false;
        auto texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
        if (!texture->create())
            return false;
        texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
        texture->setSize(size.width(), size.height());
        texture->setMipLevels(1);
        texture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        texture->setWrapMode(QOpenGLTexture::ClampToEdge);
        texture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
        if (!texture->isStorageAllocated())
            return false;
        texture->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, image.constBits());
        if (context()->functions()->glGetError() != GL_NO_ERROR)
            return false;
        m_backdrop.texture = std::move(texture);
        m_backdrop.key = pixmap.cacheKey();
        m_backdrop.size = size;
        m_backdrop.dpr = pixmap.devicePixelRatioF();
        m_backdrop.context = context();
        ++backdropUploads;
        return true;
    }

    bool updateTexture(TextureCache& cache, quint64 revision, const QRectF& source, bool navigation)
    {
        if (!revision || source.isEmpty())
            return true;
        const qreal dpr = m_plan.dpr;
        if (cache.revision == revision && cache.source == source && cache.dpr == dpr)
            return true;
        if (!cache.texture || !cache.texture->isValid())
            return false;
        QElapsedTimer clock;
        clock.start();
        const QSize pixels = cache.texture->size();
        const int height = pixels.height();
        // Damage applies only to the texture state it was accumulated against.
        // zh_CN: 脏区只适用于其累积时所对应的纹理版本。
        const auto changes = (navigation ? m_scene->navigationDamage : m_scene->contentDamage).take(revision);
        const bool reusable = cache.revision && cache.revision == changes.base && cache.source == source
            && cache.dpr == dpr && !changes.full;
        std::vector<spatial_render::RowBand> bands;
        if (reusable)
            bands = spatial_render::damageRows(changes.region, dpr, height);
        const bool known = reusable && !bands.empty();
        const bool partial = known && spatial_render::coveredRows(bands) * 10 < height * 6;
        if (!partial)
            bands = {{0, height}};
        // Continuous large updates with a reported extent (scrolling, dragging, expanding)
        // paint at native density and are refined at cache density after they pause.
        // Unknown changes such as startup, theme or page switches keep full density.
        // zh_CN: 范围已知的连续大面积更新（滚动、拖动、展开）按原生密度绘制，停止后按缓存密度补全；
        // 启动、主题或换页等范围未知的变化保持完整密度。
        const qreal nativeDpr = devicePixelRatioF();
        const qint64 now = m_clock.elapsed();
        const bool slow = updateCostMs() > 16;
        const qreal share =
            slow ? spatial_render::kInteractiveDamageShare / 3 : spatial_render::kInteractiveDamageShare;
        const int interval = slow ? 2 * spatial_render::kInteractiveIntervalMs : spatial_render::kInteractiveIntervalMs;
        const int quantum = spatial_render::sharedRowQuantum(nativeDpr, dpr);
        const bool continuous = cache.updatedAtMs >= 0 && now - cache.updatedAtMs < interval;
        bool interactive = m_interactive && known && quantum && dpr > nativeDpr && continuous
            && spatial_render::coveredRows(bands) >= share * height;
        cache.updatedAtMs = now;
        // Qt caches nested effect pixmaps in device pixels and invalidates them only when the
        // painter transform changes, so a density switch would replay them at stale positions.
        // zh_CN: Qt 按设备像素缓存嵌套特效图像，且仅在画笔变换改变时失效；切换密度会让其按旧位置重放。
        if (interactive && m_scene->hasNestedEffects) {
            const auto first = bands.front().top, last = bands.back().bottom;
            interactive = !m_scene->hasNestedEffects(navigation,
                QRectF(0, first / dpr, source.width(), (last - first) / dpr).toAlignedRect());
        }
        if (interactive && partial)
            bands = spatial_render::damageRows(changes.region, dpr, height, qRound(quantum * dpr));
        for (const auto& band : bands) {
            if (!paintRows(cache, source, navigation, band, interactive ? nativeDpr : dpr, quantum))
                return false;
            const QRegion rows(0, band.top, 1, band.bottom - band.top);
            cache.degraded = interactive ? cache.degraded.united(rows) : cache.degraded.subtracted(rows);
            (navigation ? navigationPaintedRows : contentPaintedRows) += band.bottom - band.top;
        }
        if (!partial)
            cache.degraded = interactive ? QRegion(0, 0, 1, height) : QRegion();
        partialUpdates += partial;
        interactiveUpdates += interactive;
        if (interactive)
            m_refine.start();
        m_frameUpdateMs += clock.nsecsElapsed() / 1e6;
        m_frameInteractive |= interactive;
        cache.revision = revision;
        cache.source = source;
        cache.dpr = dpr;
        return true;
    }

    void finishTextureFrame()
    {
        if (!(m_frameUpdateMs > 0))
            return;
        const bool report = m_slowFrames.record(m_frameUpdateMs, m_frameInteractive);
        m_frameUpdateMs = 0;
        m_frameInteractive = false;
        // Sustained slow native-density frames: let the application suggest 2D once.
        // zh_CN: 原生密度帧持续过慢时，交由应用提示一次 2D。
        if (report && slowRendering) {
            QTimer::singleShot(0, this, [this] {
                if (slowRendering)
                    slowRendering();
            });
        }
    }

    // Paint texture rows [band.top, band.bottom) at paintDpr. A lower density is scaled
    // into the texture; quantum logical rows land on whole pixels at both densities.
    // zh_CN: 按 paintDpr 绘制纹理行 [top, bottom)；低密度结果缩放写入纹理，quantum 保证两种密度行网格对齐。
    bool paintRows(TextureCache& cache,
        const QRectF& source,
        bool navigation,
        spatial_render::RowBand band,
        qreal paintDpr,
        int quantum)
    {
        const qreal dpr = m_plan.dpr;
        const QSize pixels = cache.texture->size();
        const bool scaled = paintDpr != dpr;
        const int textureStep = scaled ? qRound(quantum * dpr) : 1;
        const int paintStep = scaled ? qRound(quantum * paintDpr) : 1;
        const int paintHeight = scaled ? qCeil(source.height() * paintDpr) : pixels.height();
        const int paintWidth = scaled ? qMin(pixels.width(), qCeil(source.width() * paintDpr)) : pixels.width();
        const auto toPaint = [&](int row) {
            return row >= pixels.height() ? paintHeight : qMin(paintHeight, row / textureStep * paintStep);
        };
        const auto toTexture = [&](int row) {
            return row >= paintHeight ? pixels.height() : qMin(pixels.height(), row / paintStep * textureStep);
        };
        // QWidget clips in logical pixels. Guard a full logical pixel so
        // rounded clip coordinates and MSAA coverage stay outside the copied strip.
        const int guard = qMax(1, qCeil(paintDpr));
        const auto strips = spatial_render::planStrips(toPaint(band.top), toPaint(band.bottom), paintHeight,
            m_plan.paintSize.height(), guard, paintStep);
        if (strips.empty())
            return band.top >= band.bottom;
        auto* gl = context()->functions();
        for (const auto& strip : strips) {
            const int paintTop = strip.paintTop;
            const int paintRows = strip.paintBottom - strip.paintTop;
            m_paintTarget->bind();
            // Both panels reuse a large target. Clear only this strip, including
            // its guard pixels; unused target rows/columns are never resolved.
            // zh_CN: 两个面板复用大画布，仅清空当前条带及保护像素，未使用的区域不会参与解析。
            gl->glEnable(GL_SCISSOR_TEST);
            gl->glScissor(0, 0, paintWidth, paintRows);
            gl->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            gl->glClearColor(0, 0, 0, 0);
            gl->glStencilMask(~0u);
            gl->glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            gl->glDisable(GL_SCISSOR_TEST);
            QOpenGLPaintDevice device(QSize(paintWidth, paintRows));
            device.setDevicePixelRatio(paintDpr);
            spatial_render::GalleryGlyphPaintDevice glyphDevice(device, devicePixelRatioF(),
                QPointF(0, paintTop / paintDpr), &m_glyphImages);
            QPainter painter(spatial_render::needsNativeGlyphCoverage(devicePixelRatioF())
                    ? static_cast<QPaintDevice*>(&glyphDevice)
                    : &device);
            painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
            const QRectF stripRect(0, paintTop / paintDpr, source.width(), paintRows / paintDpr);
            painter.setClipRect(QRectF(0, 0, source.width(), paintRows / paintDpr));
            const QPointF origin = navigation ? source.topLeft() : QPointF();
            const QRect sourceRect = stripRect.translated(origin).toAlignedRect();
            // QWidget must own the integer offset for nested effect clips; only
            // the fractional pixel alignment belongs in the painter transform.
            // zh_CN: 整数偏移交给 QWidget 以正确裁剪子特效，画笔仅处理亚像素对齐。
            painter.translate(QPointF(sourceRect.topLeft()) - origin - stripRect.topLeft());
            // Supply the dirty strip to QWidget::render so unrelated children are skipped.
            // zh_CN: 按条带指定绘制区域，跳过未覆盖的子控件，复用同一抗锯齿画布。
            m_scene->renderWidgets(painter, navigation, sourceRect);
            painter.end();
            gl->glDisable(GL_SCISSOR_TEST);
            // WebGL/GLES requires identical rectangles and formats for MSAA resolve.
            // Resolve first, then move the guarded strip into the panel texture.
            const QRect painted(0, 0, paintWidth, paintRows);
            QOpenGLFramebufferObject::blitFramebuffer(m_resolveTarget.get(), painted, m_paintTarget.get(), painted);
            // GL framebuffer coordinates run upwards; widget coordinates run downwards.
            const int textureTop = toTexture(strip.top);
            const int textureBottom = toTexture(strip.top + strip.height);
            QOpenGLFramebufferObject::blitFramebuffer(cache.texture.get(),
                QRect(0, pixels.height() - textureBottom, pixels.width(), textureBottom - textureTop),
                m_resolveTarget.get(), QRect(0, strip.paintBottom - strip.top - strip.height, paintWidth, strip.height),
                GL_COLOR_BUFFER_BIT, scaled ? GL_LINEAR : GL_NEAREST);
        }
        return true;
    }
};

void requestCanvasFrame(QWidget* canvas, bool withInput, bool inPaintPass)
{
    if (auto* rhi = dynamic_cast<RhiSurface*>(canvas))
        rhi->requestFrameInPass(inPaintPass);
    else if (auto* host = dynamic_cast<spatial::SpatialTextureHost*>(canvas))
        host->requestFrame();
    else if (auto* gl = dynamic_cast<GpuSurface*>(canvas))
        gl->requestFrame(withInput, inPaintPass);
    else if (canvas)
        canvas->update();
}
} // namespace

struct GallerySpatialController::Private {
    QVariantMap initializationTimings;
    QObject* owner = nullptr;
    QPointer<QWidget> window;
    QPointer<spatial::SpatialSession> session;
    QPointer<navigation::NavigationView> navigation;
    QPointer<QWidget> canvas;
    QPointer<SurfaceCapture> capture;
    QPointer<SurfaceCapture> contentCapture;
    DirectCompositionObserver* compositionObserver = nullptr;
    QPointer<QWidget> grabbed, hovered;
    struct NativePopupAnchor {
        QPointer<QWidget> source;
        QPoint point;
        QPoint offset;
    };
    QHash<QWidget*, NativePopupAnchor> nativePopupAnchors;
    ShellScene scene;
    QVariantAnimation* motion = nullptr;
    QVariantAnimation* pointerMotion = nullptr;
    bool forwarding = false;
    bool syncQueued = false;
    bool target = false;
    bool backdropDirty = true;
    bool rendererInitialized = false;
    bool rendererReady = false;
    bool rendererFailed = false;
    QTimer* rendererTimeout = nullptr;
    bool nativeNoSystemBackground = false;
    bool filtering = false;
    bool directCompositionRequired = false;
    bool compositionRefreshQueued = false;
    bool composingRequirements = false;
    bool unusedComposing = false, unusedContentComposing = false;
    bool syntheticPassPending = false;
    QPointer<QWidget> qmlFailedPage;

    bool canInitializeRenderer() const
    {
        return !directCompositionRequired && session && session->isActive()
            && GallerySettings::instance().spatialModeEnabled() && canvas && canvas->isVisible()
            && !canvas->size().isEmpty() && window && window->window()->windowHandle()
            && window->window()->windowHandle()->isExposed();
    }

    bool blocksPendingQmlInput(QObject* watched, QEvent* event) const
    {
        if (!canvas->property(kQmlFailurePendingProperty).toBool() || !canvas->property("presenting").toBool())
            return false;
        auto* widget = qobject_cast<QWidget*>(watched);
        const auto inSource = [this](QWidget* candidate) {
            return candidate && candidate->window() == window->window()
                && (candidate == navigation || navigation->isAncestorOf(candidate));
        };
        const bool source = inSource(widget);
        const bool top = watched == window || watched == window->window()
            || watched == window->window()->windowHandle();
        if (!source && !top)
            return false;
        QPoint global;
        switch (event->type()) {
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::ShortcutOverride:
        case QEvent::InputMethod:
            return source || inSource(QApplication::focusWidget());
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
            global = fluentMouseGlobalPos(static_cast<QMouseEvent*>(event));
            break;
        case QEvent::Wheel:
            global = static_cast<QWheelEvent*>(event)->globalPosition().toPoint();
            break;
        case QEvent::ContextMenu:
            global = static_cast<QContextMenuEvent*>(event)->globalPos();
            break;
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::TouchCancel: {
            const auto* touch = static_cast<QTouchEvent*>(event);
            if (!fluentFirstTouchGlobalPos(touch, global))
                return source;
            break;
        }
        default:
            return false;
        }
        return (source || navigation->rect().contains(navigation->mapFromGlobal(global))) && !nativeOverlayAt(global);
    }

    void setFiltering(bool active)
    {
        if (filtering == active)
            return;
        filtering = active;
        if (active)
            qApp->installEventFilter(owner);
        else
            qApp->removeEventFilter(owner);
    }

    QWidget* firstOverlay() const
    {
        // Direct children are in stacking order. Keep the compositor in the content
        // layer, underneath the lowest visible scrim or same-window overlay.
        // zh_CN: 直接子级按层叠顺序排列；合成面保持在内容层，低于最底部的可见遮罩或浮层。
        for (QObject* child : window->window()->children()) {
            auto* widget = qobject_cast<QWidget*>(child);
            if (widget && widget->isVisible() && !widget->isWindow()
                && (qobject_cast<overlay::OverlayScrim*>(widget)
                    || widget->property(overlay::kOverlaySurfaceProperty).toBool()))
                return widget;
        }
        return nullptr;
    }

    bool nativeOverlayAt(const QPoint& global) const
    {
        if (!firstOverlay())
            return false;
        // Hit-test the native stack before unprojecting. childAt respects masks and
        // mouse transparency, so dim-only scrims still allow background input.
        // zh_CN: 先命中原生层叠；childAt 遵守遮罩与鼠标透明属性，纯调暗遮罩仍允许背景交互。
        auto* top = window->window();
        for (auto* hit = top->childAt(top->mapFromGlobal(global)); hit && hit != top; hit = hit->parentWidget()) {
            if (qobject_cast<overlay::OverlayScrim*>(hit) || hit->property(overlay::kOverlaySurfaceProperty).toBool())
                return true;
        }
        return false;
    }

    void raisePresentation()
    {
        auto* overlay = firstOverlay();
        if (overlay && overlay->parentWidget() == canvas->parentWidget())
            canvas->stackUnder(overlay);
        else
            canvas->raise();
    }

    void followPointer(const QPointF& tilt, bool animated = true)
    {
        if (animated && pointerMotion->state() == QAbstractAnimation::Running) {
            // Retarget the running timeline. Restarting on each pointer event can
            // starve animation ticks, especially in the browser event loop.
            // zh_CN: 更新运行中动画的目标，避免高频鼠标事件不断重启动画而阻塞帧推进。
            pointerMotion->setEndValue(tilt);
            return;
        }
        pointerMotion->stop();
        if (!animated) {
            scene.pointerTilt = tilt;
            sync();
        } else if (QLineF(scene.pointerTilt, tilt).length() > .001) {
            pointerMotion->setStartValue(scene.pointerTilt);
            pointerMotion->setEndValue(tilt);
            pointerMotion->start();
        }
    }

    bool belongsToContent(const QWidget* widget) const
    {
        auto* host = navigation->contentHost();
        return widget == host || host->isAncestorOf(widget);
    }

    void syncNativePopups()
    {
        const auto popups = nativePopupAnchors.keys();
        for (auto* popup : popups) {
            if (!nativePopupAnchors.contains(popup))
                continue;
            const auto anchor = nativePopupAnchors.value(popup);
            if (!anchor.source || !popup->isVisible() || anchor.source->window() != window->window()) {
                continue;
            }
            QScopedValueRollback<bool> forward(forwarding, true);
            const QPoint presented = overlay::presentation::mapToGlobal(anchor.source, anchor.point);
            QPoint position = presented + anchor.offset;
            auto* screen = QGuiApplication::screenAt(presented);
            if (!screen)
                screen = popup->screen();
            if (screen) {
                // QMenu contains its initial popup, but QWidget::move does not.
                // Keep the chosen placement while following a moving panel/window.
                // zh_CN: QMenu 仅在首次弹出时约束屏幕；跟随面板或窗口移动时保留方向并重新限界。
                const QRect available = screen->availableGeometry();
                const QRect frame = popup->frameGeometry();
                const QPoint frameOffset = frame.topLeft() - popup->pos();
                const QPoint framePosition = position + frameOffset;
                position =
                    QPoint(qBound(available.left(), framePosition.x(),
                               qMax(available.left(), available.right() - frame.width() + 1)),
                        qBound(available.top(), framePosition.y(),
                            qMax(available.top(), available.bottom() - frame.height() + 1)))
                    - frameOffset;
            }
            popup->move(position);
        }
    }

    void publishPresentationTransforms(bool enabled)
    {
        if (!navigation)
            return;
        auto* host = navigation->contentHost();
        if (enabled) {
            const QPoint origin = host->mapTo(navigation, QPoint());
            const QTransform contentTransform = QTransform::fromTranslate(origin.x(), origin.y())
                * scene.content.transform * QTransform::fromTranslate(-origin.x(), -origin.y());
            overlay::presentation::setTransform(navigation, scene.navigation.transform);
            overlay::presentation::setTransform(host, contentTransform);
        } else {
            overlay::presentation::clearTransform(navigation);
            overlay::presentation::clearTransform(host);
        }
        syncNativePopups();
    }

    void prepareBackdrop()
    {
        if (!navigation || !canvas)
            return;
        const QRect bounds(navigation->mapTo(window, QPoint()), navigation->size());
        backdropDirty |= canvas->geometry() != bounds;
        backdropDirty |= !scene.backdrop.isNull() && scene.backdrop.devicePixelRatioF() != window->devicePixelRatioF();
        canvas->setGeometry(bounds);
        // A visible GL surface replaces the raster backdrop even while the splash
        // owns the foreground. Keep that backdrop through the splash's fade-out.
        // zh_CN: 可见 GL 表面在启动页淡出时也需要窗口背景，避免露出浏览器底色。
        if (backdropDirty) {
            backdropDirty = false;
            scene.backdrop = {};
            if (!windowing::windowBackdropRequiresTransparentClear(window)) {
                const qreal dpr = window->devicePixelRatioF();
                scene.backdrop = QPixmap(bounds.size() * dpr);
                scene.backdrop.setDevicePixelRatio(dpr);
                scene.backdrop.fill(Qt::transparent);
                QPainter painter(&scene.backdrop);
                painter.translate(-bounds.topLeft());
                // The backdrop excludes both panels; rendering it is not a panel change.
                // zh_CN: 背景不含两块面板，渲染背景不应使面板纹理失效。
                QScopedValueRollback<bool> navigationComposing(capture ? capture->composing : unusedComposing, true);
                QScopedValueRollback<bool>
                    contentComposing(contentCapture ? contentCapture->composing : unusedContentComposing, true);
                window->render(&painter, QPoint(), QRegion(), QWidget::RenderFlags());
            }
        }
    }
    void sync()
    {
        if (!navigation || !canvas || canvas->isHidden())
            return;
        prepareBackdrop();
        if (!canvas->property("presenting").toBool()) {
            requestCanvasFrame(canvas);
            return;
        }
        scene.layout(navigation);
        publishPresentationTransforms(true);
        canvas->setProperty("galleryRotationAxis", scene.top ? "X" : "Y");
        const qreal angle = scene.top ? kTopRotation : kSideRotation;
        canvas->setProperty("galleryNavigationRotation", angle * scene.progress);
        canvas->setProperty("galleryContentRotation", -angle * scene.progress);
        canvas->setProperty("galleryPointerTilt", scene.pointerTilt);
        canvas->setProperty("galleryDepthProgress", scene.progress);
        requestCanvasFrame(canvas);
    }
    void settle()
    {
        motion->stop();
        pointerMotion->stop();
        scene.pointerTilt = {};
        scene.progress = target ? 1 : 0;
        contentCapture->setEnabled(target);
        capture->setEnabled(target);
        canvas->setProperty("presenting", target);
        if (target) {
            navigation->setAttribute(Qt::WA_NoSystemBackground);
            raisePresentation();
        } else {
            publishPresentationTransforms(false);
            navigation->setAttribute(Qt::WA_NoSystemBackground, nativeNoSystemBackground);
            canvas->hide();
            if (auto* rhi = dynamic_cast<RhiSurface*>(canvas.data()))
                rhi->clearFrameCaches();
            else
                static_cast<GpuSurface*>(canvas.data())->clearFrameCaches();
            setFiltering(false);
            canvas->setProperty("galleryDepthProgress", 0.0);
        }
        QEvent compositionChanged(depth::changeEvent());
        QCoreApplication::sendEvent(navigation, &compositionChanged);
        if (!target) {
            scene.resetRevisions();
            scene.backdrop = {};
            backdropDirty = true;
            grabbed = nullptr;
            hovered = nullptr;
        }
        sync();
        navigation->update();
        window->update();
    }
    void hover(QWidget* target, const QPoint& source)
    {
        if (hovered == target)
            return;
        if (hovered) {
            QEvent leave(QEvent::Leave);
            QApplication::sendEvent(hovered, &leave);
        }
        hovered = target;
        if (target) {
            const QPoint local = target->mapFrom(navigation, source);
            FLUENT_MAKE_ENTER_EVENT_AT(enter, local, target->mapTo(window, local), target->mapToGlobal(local));
            QApplication::sendEvent(target, &enter);
        }
    }
};

void GallerySpatialController::prepareApplicationStyle()
{
    spatial::SpatialSession session;
    session.activate();
}

GallerySpatialController::GallerySpatialController(QWidget* window, navigation::NavigationView* navigation)
    : QObject(window), d(new Private)
{
    setObjectName(QStringLiteral("gallerySpatialController"));
    d->owner = this;
    d->window = window;
    d->navigation = navigation;
    d->scene.contentRoot = navigation->contentHost();
    d->scene.themeChanged = [this] {
        d->backdropDirty = true;
        // Material tokens change immediately. Retire both widget textures in
        // the same transaction instead of waiting for a later raster update.
        // zh_CN: 材质 token 立即改变，同步作废两块控件纹理，避免新背景混用旧主题内容。
        d->scene.markDirty(true);
        d->scene.markDirty(false);
        d->sync();
    };
    d->nativeNoSystemBackground = navigation->testAttribute(Qt::WA_NoSystemBackground);
    GallerySettings::instance().beginSpatialAvailabilityCheck();
    d->rendererTimeout = new QTimer(this);
    d->rendererTimeout->setSingleShot(true);
    d->rendererTimeout->setInterval(5000);
    connect(d->rendererTimeout, &QTimer::timeout, this, [this] {
        checkRenderer();
        if (!d->rendererReady && d->canInitializeRenderer())
            disableSpatial(tr("3D could not start. Using the 2D Gallery."));
    });
    d->motion = new QVariantAnimation(this);
    d->motion->setObjectName(QStringLiteral("galleryAssemblyAnimation"));
    d->motion->setEasingCurve(QEasingCurve::InOutCubic);
    connect(d->motion, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        d->scene.progress = value.toReal();
        d->sync();
    });
    connect(d->motion, &QVariantAnimation::finished, this, [this] { d->settle(); });
    d->pointerMotion = new QVariantAnimation(this);
    d->pointerMotion->setObjectName(QStringLiteral("galleryPointerAnimation"));
    d->pointerMotion->setDuration(180);
    d->pointerMotion->setEasingCurve(QEasingCurve::OutCubic);
    connect(d->pointerMotion, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        d->scene.pointerTilt = value.toPointF();
        d->sync();
    });
    const auto stopObserving = [this] {
        // QWidget emits destroyed before deleting its children, while Qt 5 may
        // still keep its QPointer alive. Child sessions must not refresh a host
        // whose derived members have already been destroyed.
        // zh_CN: QWidget 在删除子对象前发出 destroyed，此时 Qt 5 的 QPointer
        // 可能仍非空；子会话卸载不能再访问已析构的导航与页面数据。
        d->navigation = nullptr;
        d->scene.contentRoot = nullptr;
        d->rendererFailed = true;
        d->target = false;
        d->rendererTimeout->stop();
        d->motion->stop();
        d->pointerMotion->stop();
        d->setFiltering(false);
        if (d->compositionObserver)
            qApp->removeEventFilter(d->compositionObserver);
        if (d->canvas)
            d->canvas->setProperty("presenting", false);
        if (d->capture)
            d->capture->invalidated = {};
        if (d->contentCapture)
            d->contentCapture->invalidated = {};
        d->scene.renderWidgets = {};
        d->scene.renderForeground = {};
        d->scene.hasNestedEffects = {};
    };
    connect(window, &QObject::destroyed, this, [this, stopObserving] {
        d->window = nullptr;
        stopObserving();
    });
    connect(navigation, &QObject::destroyed, this, stopObserving);
    connect(navigation->contentHost(), &QObject::destroyed, this, stopObserving);
    auto& settings = GallerySettings::instance();
    connect(&settings, &GallerySettings::spatialModeEnabledChanged, this, &GallerySpatialController::applyMode);
    const auto refresh = [this] {
        d->backdropDirty = true;
        applyMode(GallerySettings::instance().spatialModeEnabled());
    };
    connect(&settings, &GallerySettings::themeModeChanged, this, refresh);
    connect(&MotionPolicy::instance(), &MotionPolicy::modeChanged, this, refresh);
    connect(&settings, &GallerySettings::windowEffectChanged, this, [this] {
        d->backdropDirty = true;
        cancelTransition();
    });
    if (auto* fluentWindow = qobject_cast<windowing::Window*>(window)) {
        connect(fluentWindow, &windowing::Window::backdropStateChanged, this, [this] {
            // Consume the resolved paint state before the native material is
            // removed. A Settings notification arrives after that transaction.
            // zh_CN: 在原生材质移除前同步实际绘制状态；Settings 通知晚于这次交接。
            d->backdropDirty = true;
            d->scene.markDirty(true);
            d->scene.markDirty(false);
            d->sync();
        });
    }
    d->session = new spatial::SpatialSession(window, this);
    d->session->setObjectName(QStringLiteral("gallerySpatialSession"));
    QString activationError;
    if (!d->session->activate(&activationError)) {
        disableSpatial(activationError);
        return;
    }
    connect(d->session, &spatial::SpatialSession::activeChanged, this, [this](bool active) {
        applyMode(active && GallerySettings::instance().spatialModeEnabled());
        if (!active)
            cancelTransition();
    });
    connect(d->session, &QObject::destroyed, this, [this] {
        applyMode(false);
        cancelTransition();
    });
    const QString unavailableReason = sessionUnavailableReason();
    if (!unavailableReason.isEmpty()) {
        disableSpatial(unavailableReason);
        return;
    }
    const auto refreshComposition = [this] { applyMode(GallerySettings::instance().spatialModeEnabled()); };
    connect(navigation->contentHost(), &navigation::StackContentHost::currentIndexChanged, this, refreshComposition);
    d->compositionObserver = new DirectCompositionObserver(this);
#ifdef Q_OS_WASM
    d->compositionObserver->windowReady = [this, refreshComposition](QObject* object) {
        if (d->canvas || d->rendererFailed || d->compositionRefreshQueued
            || !GallerySettings::instance().spatialModeEnabled())
            return;
        auto* top = d->window->window();
        if (object != d->window && object != top && object != top->windowHandle())
            return;
        d->compositionRefreshQueued = true;
        QTimer::singleShot(0, this, [this, refreshComposition] {
            d->compositionRefreshQueued = false;
            refreshComposition();
        });
    };
#endif
    d->compositionObserver->changed = [this, refreshComposition](QWidget* widget, bool propertyChanged) {
        auto* host = d->navigation ? d->navigation->contentHost() : nullptr;
        auto* page = host ? host->pageWidget(host->currentIndex()) : nullptr;
        if (propertyChanged && (!page || (page != widget && !page->isAncestorOf(widget))))
            return;
        if (!d->composingRequirements && propertyChanged
            && widget->property(kDirectCompositionRequiredProperty).toBool()) {
            // No fade-out: the ancestor effects must be gone before Quick is constructed.
            // zh_CN: 不做退出动画，须在创建 Quick 前移除祖先捕获特效。
            refreshComposition();
        } else if (!d->compositionRefreshQueued) {
            d->compositionRefreshQueued = true;
            QTimer::singleShot(0, this, [this, refreshComposition] {
                d->compositionRefreshQueued = false;
                refreshComposition();
            });
        }
    };
    // The splash keeps ownership of its capture effect through the logo handoff.
    // zh_CN: 启动图标交接结束前，内容缓存特效仍由 splash 管理。
    if (auto* splash = window->findChild<GallerySplashScreen*>()) {
        connect(splash, &QObject::destroyed, this, [this] {
            QTimer::singleShot(0, this, &GallerySpatialController::startPresentation);
        });
    }
    applyMode(settings.spatialModeEnabled());
}

void GallerySpatialController::ensureRenderer()
{
    if (d->rendererFailed || !d->session || !d->session->isActive())
        return;
#ifdef Q_OS_WASM
    // The browser runtime reparents Gallery into its desktop surface. Creating
    // GL resources before that surface is exposed leaves them in an offscreen
    // WebGL context that cannot share textures with the final backing store.
    // zh_CN: 浏览器会将 Gallery 放入桌面表面；须等该表面显示后再创建 GL 资源，
    // 避免资源留在无法与最终后备存储共享纹理的离屏 WebGL 上下文中。
    auto* top = d->window->window();
    if (!d->window->isVisible() || !top->windowHandle() || !top->windowHandle()->isExposed())
        return;
#endif
    if (!d->canvas) {
        const bool measuring = qEnvironmentVariableIntValue("FLUENT_QT_SPATIAL_BENCHMARK");
        QElapsedTimer clock;
        if (measuring)
            clock.start();
        bool useRhi = spatial::SpatialTextureHost::isPreferred();
        QString reason = qApp->property("fluentqtGalleryGraphicsBackendFailure").toString();
        if (reason.isEmpty()) {
            reason = useRhi ? spatial::SpatialTextureHost::preflightFailure() : accelerationUnavailableReason();
            if (useRhi && !reason.isEmpty()) {
                bool allowOpenGL = true;
#ifdef FLUENT_QT_HAS_QML_BRIDGE
                allowOpenGL = qmlbridge::QmlSession::isGraphicsApiCompatible(
                    qmlbridge::QmlSession::GraphicsApi::OpenGL);
#endif
                if (allowOpenGL) {
                    useRhi = false;
                    reason = accelerationUnavailableReason();
                }
            }
        }
        if (measuring) {
            d->initializationTimings["probeMs"] = clock.nsecsElapsed() / 1e6;
            clock.restart();
        }
        if (!reason.isEmpty()) {
            disableSpatial(reason);
            return;
        }
        if (measuring)
            d->initializationTimings["nativeStyleMs"] = clock.nsecsElapsed() / 1e6;
        const auto initialized = [this] {
            d->rendererInitialized = true;
            QTimer::singleShot(0, this, &GallerySpatialController::checkRenderer);
        };
        const auto failed = [this] {
            QTimer::singleShot(0, this, [this] {
                disableSpatial(tr("3D rendering is unavailable. Using the 2D Gallery."));
            });
        };
        const auto cacheUnavailable = [this] {
            QTimer::singleShot(0, this, &GallerySpatialController::releaseOversizedPresentation);
        };
        if (useRhi) {
            auto* surface = new RhiSurface(&d->scene, d->window);
            d->canvas = surface;
            connect(surface, &RhiSurface::initialized, this, initialized);
            connect(surface, &RhiSurface::renderingFailed, this, failed);
            connect(surface, &RhiSurface::cacheUnavailable, this, cacheUnavailable);
            surface->requestBackingStorePass = [this] {
                if (!d->navigation)
                    return;
                d->syntheticPassPending = true;
                d->navigation->update(QRect(QPoint(), QSize(1, 1)));
            };
        } else {
            auto* surface = new GpuSurface(&d->scene, d->window, d->session);
            d->canvas = surface;
            surface->initialized = initialized;
            surface->failed = failed;
            surface->cacheUnavailable = cacheUnavailable;
            surface->qmlCompositionFailed = [this](QWidget* failedPage) {
                const QPointer<QWidget> guardedPage(failedPage);
                QTimer::singleShot(0, this, [this, guardedPage] {
                    d->qmlFailedPage = guardedPage;
                    applyMode(GallerySettings::instance().spatialModeEnabled());
                    if (d->canvas)
                        d->canvas->setProperty(kQmlFailurePendingProperty, false);
                });
            };
            surface->contextLost = [this] {
                d->rendererInitialized = d->rendererReady = false;
                QTimer::singleShot(0, this, &GallerySpatialController::checkRenderer);
            };
            surface->requestBackingStorePass = [this] {
                if (!d->navigation)
                    return;
                d->syntheticPassPending = true;
                d->navigation->update(QRect(QPoint(), QSize(1, 1)));
            };
            surface->slowRendering = [this] {
                if (d->window && d->target)
                    showGalleryToast(d->window,
                        tr("3D is running slowly on this device. Turn off 3D Gallery in Settings for smoother "
                           "scrolling."),
                        status_info::Toast::Warning);
            };
        }
        d->canvas->setObjectName(QStringLiteral("gallerySpatialSurface"));
        d->canvas->setProperty("galleryGpuComposition", true);
        d->canvas->lower();
        d->navigation->setMouseTracking(true);
        d->window->setMouseTracking(true);
    }
    d->setFiltering(true);
    d->canvas->setGeometry(QRect(d->navigation->mapTo(d->window, QPoint()), d->navigation->size()));
    // Showing the native child can initialize and paint it synchronously.
    // Prepare the existing material snapshot before that first GL frame.
    // zh_CN: 原生子窗口显示可能同步初始化并绘制；先准备现有材质快照供首帧使用。
    d->prepareBackdrop();
    QElapsedTimer showClock;
    const bool measuring = qEnvironmentVariableIntValue("FLUENT_QT_SPATIAL_BENCHMARK");
    if (measuring)
        showClock.start();
    d->canvas->show();
    if (measuring)
        d->initializationTimings["surfaceShowMs"] = showClock.nsecsElapsed() / 1e6;
    d->sync();
    startPresentation();
}

void GallerySpatialController::checkRenderer()
{
    if (d->rendererFailed || d->rendererReady)
        return;
    if (!d->canInitializeRenderer()) {
        d->rendererTimeout->stop();
        return;
    }
    // isValid() is also false before initializeGL, including a zero-sized surface.
    // Bound a real visible initialization failure without rejecting deferred startup.
    // zh_CN: 初始化前 isValid() 同样为 false；等待回调，仅对可见表面的初始化设置超时。
    if (!d->rendererTimeout->isActive())
        d->rendererTimeout->start();
    if (!d->rendererInitialized)
        return;
    QString renderer;
    if (auto* rhi = dynamic_cast<RhiSurface*>(d->canvas.data())) {
        if (!rhi->isReady()) {
            disableSpatial(tr("3D could not start. Using the 2D Gallery."));
            return;
        }
        renderer = rhi->rendererName();
    } else {
        auto* surface = static_cast<GpuSurface*>(d->canvas.data());
        if (!surface->isValid() || !surface->ready()) {
            disableSpatial(tr("3D could not start. Using the 2D Gallery."));
            return;
        }
        surface->makeCurrent();
        renderer = currentRendererName(surface->context());
        surface->doneCurrent();
    }
    if (!spatial::SpatialRuntime::isHardwareRenderer(renderer)) {
        disableSpatial(tr("Hardware acceleration is unavailable. Using the 2D Gallery."));
        return;
    }
    d->rendererReady = true;
    d->rendererTimeout->stop();
    GallerySettings::instance().setSpatialAvailability(true);
    if (!d->window->findChild<GallerySplashScreen*>())
        QTimer::singleShot(0, this, &GallerySpatialController::startPresentation);
}

void GallerySpatialController::releaseOversizedPresentation()
{
    // A size/allocation limit is recoverable, not evidence that the GPU is unsupported.
    // Turning 3D on again after resizing or freeing resources retries the allocation.
    GallerySettings::instance().setSpatialModeEnabled(false);
    cancelTransition();
}

void GallerySpatialController::disableSpatial(const QString& reason)
{
    d->publishPresentationTransforms(false);
    if (d->rendererFailed)
        return;
    d->rendererFailed = true;
    d->rendererTimeout->stop();
    d->setFiltering(false);
    d->rendererReady = false;
    d->target = false;
    d->motion->stop();
    d->pointerMotion->stop();
    d->scene.progress = 0;
    d->scene.pointerTilt = {};
    if (d->contentCapture) {
        d->contentCapture->invalidated = {};
        d->navigation->contentHost()->setGraphicsEffect(nullptr);
    }
    if (d->capture) {
        d->capture->invalidated = {};
        d->navigation->setGraphicsEffect(nullptr);
    }
#ifdef FLUENT_QT_HAS_QML_BRIDGE
    if (auto* surface = dynamic_cast<GpuSurface*>(d->canvas.data()))
        surface->qmlLayer()->release();
#endif
    d->scene.resetRevisions();
    d->scene.backdrop = {};
    d->grabbed = nullptr;
    d->hovered = nullptr;
    if (d->canvas) {
        if (auto* surface = dynamic_cast<GpuSurface*>(d->canvas.data())) {
            surface->initialized = {};
            surface->contextLost = {};
            surface->failed = {};
            surface->cacheUnavailable = {};
            surface->slowRendering = {};
            surface->requestBackingStorePass = {};
        }
        d->canvas->disconnect(this);
        d->canvas->hide();
        d->canvas->deleteLater();
        d->canvas = nullptr;
    }
    d->navigation->setAttribute(Qt::WA_NoSystemBackground, d->nativeNoSystemBackground);
    depth::setEnabled(d->window, false);
    GallerySettings::instance().setSpatialAvailability(false, reason);
    d->navigation->update();
    d->window->update();
}

void GallerySpatialController::startPresentation()
{
    if (!d->session || !d->session->isActive() || !d->navigation || !d->canvas || d->rendererFailed
        || d->directCompositionRequired || !GallerySettings::instance().spatialModeEnabled()
        || d->window->findChild<GallerySplashScreen*>())
        return;
    if (d->capture && d->canvas->property("presenting").toBool())
        return;
    checkRenderer();
    if (!d->rendererReady)
        return;
    if (d->capture) {
        applyMode(true);
        return;
    }
    d->navigation->setAttribute(Qt::WA_NoSystemBackground);
    d->contentCapture = new SurfaceCapture;
    d->contentCapture->setEnabled(false);
    d->capture = new SurfaceCapture;
    d->capture->setEnabled(false);
    d->contentCapture->target = d->navigation->contentHost();
    d->capture->target = d->navigation;
    d->contentCapture->invalidated = [this](const QRegion& dirty) {
        d->scene.markDirty(false, dirty);
        requestCanvasFrame(d->canvas, false, true);
    };
    // The content host is a descendant of the navigation view, so Qt reports every
    // change through this outer effect. Attribute it by region instead of retiring both.
    // zh_CN: 正文宿主是导航视图的后代，Qt 通过外层特效报告所有变化；按区域分配，避免两块纹理同时作废。
    d->capture->invalidated = [this](const QRegion& dirty) {
        // The pixel dirtied only to bring a backing-store pass to the surface is not content.
        // zh_CN: 仅为让后备存储绘制经过表面而标脏的像素不是内容变化。
        const bool synthetic = std::exchange(d->syntheticPassPending, false) && !dirty.isEmpty()
            && kSyntheticPassRect.contains(dirty.boundingRect());
        if (!synthetic) {
            d->scene.layout(d->navigation);
            auto* host = d->navigation->contentHost();
            d->scene.invalidate(dirty, QRect(host->mapTo(d->navigation, QPoint()), host->size()));
        }
        requestCanvasFrame(d->canvas, false, true);
    };
    d->scene.renderWidgets = [this](QPainter& painter, bool navigation, const QRegion& region) {
        QScopedValueRollback<bool> navComposing(d->capture->composing, true);
        QScopedValueRollback<bool> contentComposing(d->contentCapture->composing, true);
        auto* capture = navigation ? d->capture.data() : d->contentCapture.data();
        auto* widget = navigation
            ? static_cast<QWidget*>(d->navigation.data())
            : static_cast<QWidget*>(d->navigation->contentHost());
        QScopedValueRollback<bool> rendering(capture->rendering, true);
        QScopedValueRollback<QPoint> renderOrigin(capture->renderOrigin, region.boundingRect().topLeft());
        QElapsedTimer clock;
        if (capture->measuring)
            clock.start();
        // Preserve transparent hosts instead of forcing a palette window background.
        widget->render(&painter, QPoint(), region, QWidget::DrawChildren);
        if (capture->measuring) {
            ++capture->captures;
            capture->captureNanoseconds += clock.nsecsElapsed();
        }
    };
    d->scene.hasNestedEffects = [this](bool navigation, const QRect& area) {
        QWidget* root = navigation ? static_cast<QWidget*>(d->navigation.data()) : d->navigation->contentHost();
        const auto children = root->findChildren<QWidget*>();
        return std::any_of(children.cbegin(), children.cend(), [this, root, &area](QWidget* child) {
            const auto* effect = child->graphicsEffect();
            return effect && effect->isEnabled() && effect != d->capture && effect != d->contentCapture
                && child->isVisibleTo(root) && QRect(child->mapTo(root, QPoint()), child->size()).intersects(area);
        });
    };
    d->scene.renderForeground = [this](QPainter& painter, QWidget* widget, const QRegion& region) {
        QScopedValueRollback<bool> navComposing(d->capture->composing, true);
        QScopedValueRollback<bool> contentComposing(d->contentCapture->composing, true);
        QScopedValueRollback<bool> navRendering(d->capture->rendering, true);
        QScopedValueRollback<bool> contentRendering(d->contentCapture->rendering, true);
        widget->render(&painter, region.boundingRect().topLeft(), region, QWidget::DrawChildren);
    };
    d->navigation->contentHost()->setGraphicsEffect(d->contentCapture);
    d->navigation->setGraphicsEffect(d->capture);
    applyMode(GallerySettings::instance().spatialModeEnabled());
}
GallerySpatialController::~GallerySpatialController()
{
    delete d->compositionObserver;
    d->compositionObserver = nullptr;
    if (d->session) {
        disconnect(d->session, nullptr, this, nullptr);
        d->session->deactivate();
    }
    if (d->window)
        depth::setEnabled(d->window, false);
    d->target = false;
    cancelTransition();
    qApp->removeEventFilter(this);
    d->nativePopupAnchors.clear();
    d->publishPresentationTransforms(false);
    if (d->capture) {
        d->capture->invalidated = {};
        if (d->navigation)
            d->navigation->setGraphicsEffect(nullptr);
    }
    if (d->contentCapture) {
        d->contentCapture->invalidated = {};
        if (d->navigation)
            d->navigation->contentHost()->setGraphicsEffect(nullptr);
    }
#ifdef FLUENT_QT_HAS_QML_BRIDGE
    if (auto* surface = dynamic_cast<GpuSurface*>(d->canvas.data()))
        surface->qmlLayer()->release();
#endif
    delete d->canvas;
}

bool GallerySpatialController::transitionRunning() const
{
    return d->motion->state() == QAbstractAnimation::Running;
}
QVariantMap GallerySpatialController::renderingStatistics() const
{
    QVariantMap result = d->initializationTimings;
    result["pluginActive"] = d->session && d->session->isActive();
    result["directCompositionRequired"] = d->directCompositionRequired;
    for (const auto& entry : {qMakePair(QStringLiteral("navigation"), d->capture.data()),
             qMakePair(QStringLiteral("content"), d->contentCapture.data())}) {
        result[entry.first + "Captures"] = entry.second ? entry.second->captures : 0;
        result[entry.first + "CaptureMs"] = entry.second ? entry.second->captureNanoseconds / 1e6 : 0;
    }
    if (auto* rhi = dynamic_cast<RhiSurface*>(d->canvas.data())) {
        const auto stats = rhi->statistics();
        for (auto it = stats.cbegin(); it != stats.cend(); ++it)
            result[it.key()] = it.value();
        return result;
    }
    auto* surface = static_cast<GpuSurface*>(d->canvas.data());
    result["paints"] = surface ? surface->paints : 0;
    result["navigationPaintedRows"] = surface ? surface->navigationPaintedRows : 0;
    result["contentPaintedRows"] = surface ? surface->contentPaintedRows : 0;
    result["partialUpdates"] = surface ? surface->partialUpdates : 0;
    result["interactiveUpdates"] = surface ? surface->interactiveUpdates : 0;
    result["refinements"] = surface ? surface->refinements : 0;
    result["updateCostMs"] = surface ? surface->updateCostMs() : 0;
    result["slowFrames"] = surface ? surface->slowFrames() : 0;
    result["paintMs"] = surface ? surface->paintNanoseconds / 1e6 : 0;
    result["allocationMs"] = surface ? surface->allocationNanoseconds / 1e6 : 0;
    result["firstPaintMs"] = surface ? surface->firstPaintNanoseconds / 1e6 : 0;
    result["cachedPixels"] = surface ? surface->cachedPixels() : 0;
    result["cacheDpr"] = surface ? surface->cacheDpr() : 0;
    result["contentTextureId"] = surface ? surface->contentTextureId() : 0;
    result["cacheEstimatedBytes"] = surface ? surface->estimatedCacheBytes() : 0;
    result["glyphCacheBytes"] = surface ? surface->glyphCacheBytes() : 0;
    result["backdropTextureBytes"] = surface ? surface->backdropBytes() : 0;
    result["backdropUploads"] = surface ? surface->backdropUploads : 0;
    result["paintSamples"] = surface ? surface->paintSamples() : 0;
    result["paintSampleBytes"] = surface ? surface->paintSampleBytes() : 0;
    result["paintTargetHeight"] = surface ? surface->paintTargetHeight() : 0;
    result["cacheBudgetBytes"] = spatial_render::kCacheBudgetBytes;
    result["maxCacheDimension"] = surface ? surface->maxCacheDimension() : 0;
    result["allocationRetries"] = surface ? surface->allocationRetries : 0;
    result["surfaceSamples"] = surface ? surface->surfaceSamples : 0;
    result["particleLayers"] = surface ? surface->particles().activeLayerCount() : 0;
    result["particleFrames"] = surface ? surface->particles().particleFrameCount() : 0;
    result["particleBytes"] = surface ? surface->particles().allocatedBytes() : 0;
    result["particleForegroundCaptures"] = surface ? surface->particles().foregroundCaptureCount() : 0;
    result["particleCompositions"] = surface ? surface->particles().compositionCount() : 0;
    result["particleSourceScans"] = surface ? surface->particles().sourceScanCount() : 0;
#ifdef FLUENT_QT_HAS_QML_BRIDGE
    result["qmlTextures"] = surface ? surface->qmlLayer()->statistics() : QVariantMap();
#endif
    return result;
}
void GallerySpatialController::cancelTransition()
{
    if (d->navigation && d->canvas && d->capture)
        d->settle();
}
QPoint GallerySpatialController::projectedPosition(const QWidget* widget, const QPoint& point) const
{
    if (d->navigation && d->capture && d->capture->isEnabled()
        && (widget == d->navigation || d->navigation->isAncestorOf(widget))) {
        const QPoint local = widget->mapTo(d->navigation, point);
        const QPointF presented = widget == d->navigation
            ? d->scene.projectPoint(local)
            : (d->belongsToContent(widget) ? d->scene.content : d->scene.navigation).transform.map(QPointF(local));
        return presented.toPoint() + d->navigation->mapTo(d->window, QPoint());
    }
    return widget->mapTo(d->window, point);
}
void GallerySpatialController::applyMode(bool enabled)
{
    if (!d->window || !d->navigation)
        return;
    if (d->composingRequirements)
        return;
    QScopedValueRollback<bool> compositionGuard(d->composingRequirements, true);
    auto* host = d->navigation->contentHost();
    auto* page = host->pageWidget(host->currentIndex());
    const auto suspend = [this, host] {
        d->rendererTimeout->stop();
        d->target = false;
        depth::setEnabled(d->window, false);
        if (d->capture) {
            d->settle();
            d->contentCapture->invalidated = {};
            d->capture->invalidated = {};
            host->setGraphicsEffect(nullptr);
            d->navigation->setGraphicsEffect(nullptr);
            d->scene.renderWidgets = {};
            d->scene.renderForeground = {};
            d->scene.hasNestedEffects = {};
        } else {
            d->motion->stop();
            d->pointerMotion->stop();
            d->scene.progress = 0;
            d->scene.pointerTilt = {};
            d->publishPresentationTransforms(false);
            if (d->canvas)
                d->canvas->hide();
            d->setFiltering(false);
        }
    };
    const bool requested = enabled && d->session && d->session->isActive();
    if (!requested)
        d->qmlFailedPage = nullptr;
    enabled = requested && MotionPolicy::instance().mode() == MotionPolicy::Mode::Full
        && FluentElement::currentTheme() != FluentElement::HighContrast;
    bool adapted = true;
#ifdef FLUENT_QT_HAS_QML_BRIDGE
    auto qmlViews = page ? page->findChildren<qmlbridge::QmlView*>() : QList<qmlbridge::QmlView*>();
    if (auto* view = qobject_cast<qmlbridge::QmlView*>(page))
        qmlViews.prepend(view);
    const bool hasQml = std::any_of(qmlViews.cbegin(), qmlViews.cend(), [](qmlbridge::QmlView* view) {
        return view->status() == qmlbridge::QmlView::Loading || view->rootObject()
            || view->property(kDirectCompositionRequiredProperty).toBool();
    });
    if (enabled && hasQml && !d->canvas && !d->rendererFailed) {
        // Initialize the GL canvas without capture effects. The new QML views
        // must surrender native composition before startPresentation can capture.
        // zh_CN: 先初始化不带捕获特效的 GL 画布，QML 视图接管原生合成后才能启用整页捕获。
        d->directCompositionRequired = true;
        ensureRenderer();
    }
    auto* surface = dynamic_cast<GpuSurface*>(d->canvas.data());
    if (!enabled && surface && surface->qmlLayer()->statistics().value("views").toInt()) {
        suspend();
        surface->qmlLayer()->release();
        d->directCompositionRequired = requiresDirectComposition(page);
        return;
    }
    if (enabled && hasQml) {
        adapted = surface && d->qmlFailedPage != page;
        if (adapted) {
            surface->qmlLayer()->setRootWidget(page);
            adapted = surface->qmlLayer()->synchronize();
        }
    } else if (surface) {
        surface->qmlLayer()->setRootWidget(page);
    }
#endif
    d->directCompositionRequired = !adapted || requiresDirectComposition(page);
    if (d->directCompositionRequired) {
        suspend();
#ifdef FLUENT_QT_HAS_QML_BRIDGE
        bool pending = false;
        if (page)
            for (auto* view : page->findChildren<qmlbridge::QmlView*>())
                pending |= view->status() == qmlbridge::QmlView::Loading
                    || (view->isTextureRenderingEnabled() && view->status() == qmlbridge::QmlView::Unloaded);
        if (surface && !pending)
            surface->qmlLayer()->release();
#endif
        return;
    }
    d->composingRequirements = false;
    if (enabled && (!d->capture || !d->rendererReady)) {
        ensureRenderer();
        return;
    }
    if (!d->capture) {
        d->rendererTimeout->stop();
        if (d->canvas)
            d->canvas->hide();
        d->setFiltering(false);
        return;
    }
    if (enabled) {
        d->setFiltering(true);
        d->canvas->show();
        d->navigation->setAttribute(Qt::WA_NoSystemBackground);
    }
    d->motion->stop();
    d->pointerMotion->stop();
    d->scene.pointerTilt = {};
    depth::setEnabled(d->window, enabled && d->rendererReady);
    d->target = depth::enabled(d->window);
    const bool animate = d->window->isVisible() && !d->window->findChild<QWidget*>("gallerySplashScreen")
        && MotionPolicy::instance().mode() == MotionPolicy::Mode::Full
        && FluentElement::currentTheme() != FluentElement::HighContrast;
    if (!animate || qFuzzyCompare(d->scene.progress + 1, (d->target ? 1.0 : 0.0) + 1)) {
        d->settle();
        return;
    }
    d->contentCapture->setEnabled(true);
    d->capture->setEnabled(true);
    d->canvas->setProperty("presenting", true);
    d->raisePresentation();
    d->sync();
    d->motion->setDuration(qMax(1, qRound(420 * qAbs((d->target ? 1.0 : 0.0) - d->scene.progress))));
    d->motion->setStartValue(d->scene.progress);
    d->motion->setEndValue(d->target ? 1.0 : 0.0);
    d->motion->start();
}

bool GallerySpatialController::eventFilter(QObject* watched, QEvent* event)
{
    if (!d->window || !d->navigation || !d->canvas)
        return false;
    if (d->blocksPendingQmlInput(watched, event)) {
        event->accept();
        return true;
    }
    const bool nativeMenuShow = event->type() == QEvent::Show && qobject_cast<QMenu*>(watched);
    if (d->forwarding && !nativeMenuShow)
        return false;
    if (event->type() == QEvent::MouseMove && d->hovered && d->capture && d->capture->isEnabled()
        && watched == d->window->window()->windowHandle()) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->buttons() == Qt::NoButton) {
            const QPoint global = fluentMouseGlobalPos(mouse);
            const QPoint presented = d->navigation->mapFromGlobal(global);
            QPointF source;
            const auto* hit = d->scene.unproject(presented, &source);
            auto* target = hit && d->navigation->rect().contains(presented) && !d->nativeOverlayAt(global)
                ? d->navigation->childAt(source.toPoint())
                : nullptr;
            if (target != d->hovered) {
                // Clear the old projected hover before Qt dispatches the native
                // QWidget move and arms its tooltip timer. A later synthetic Leave
                // would cancel that timer; our forwarded move is not spontaneous.
                // zh_CN: 在 Qt 分发原生移动并启动提示计时前清理旧悬停，避免合成 Leave 取消提示。
                const QPointer<GallerySpatialController> guard(this);
                const QPointer<QObject> receiver(watched);
                const QPointer<QWidget> previous = d->hovered;
                d->hovered = nullptr;
                d->forwarding = true;
                QEvent leave(QEvent::Leave);
                QApplication::sendEvent(previous, &leave);
                if (!guard)
                    return true;
                d->forwarding = false;
                if (!receiver)
                    return true;
            }
        }
    }
    if (watched == d->window && !d->rendererReady
        && (event->type() == QEvent::Show || event->type() == QEvent::UpdateRequest))
        QTimer::singleShot(0, this, &GallerySpatialController::checkRenderer);
    auto* widget = qobject_cast<QWidget*>(watched);
    if (!widget)
        return false;
    const bool mouse = event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseButtonDblClick;
    // Nested Qt hover events can overwrite a native mouse event's shared global
    // point. Its receiver-local position stays intact; snapshot it before routing.
    // zh_CN: Qt 嵌套悬停事件可能改写原生鼠标的共享全局点，先由未变的接收者局部坐标取快照。
    const QPoint mouseGlobal = mouse
        ? (event->spontaneous() ? widget->mapToGlobal(fluentMousePos(static_cast<QMouseEvent*>(event)))
                                : fluentMouseGlobalPos(static_cast<QMouseEvent*>(event)))
        : QPoint();
    // A native popup can receive the release of the press that opened it. Do not
    // keep routing later clicks/wheels to that original opener.
    // zh_CN: 打开原生弹窗后，释放事件可能由弹窗接收；不能把后续输入一直发给打开按钮。
    if ((event->type() == QEvent::MouseButtonRelease && widget->window() != d->window->window())
        || (event->type() == QEvent::MouseMove && static_cast<QMouseEvent*>(event)->buttons() == Qt::NoButton)
        || (event->type() == QEvent::Wheel && static_cast<QWheelEvent*>(event)->buttons() == Qt::NoButton))
        d->grabbed = nullptr;
    const bool inSource = widget == d->navigation || d->navigation->isAncestorOf(widget);
    if (!inSource && widget != d->window && event->type() == QEvent::MouseButtonRelease)
        d->grabbed = nullptr;
    if (widget == d->window && event->type() == QEvent::ActivationChange)
        d->backdropDirty = true;
    if ((widget == d->window || widget == d->navigation || widget == d->navigation->contentHost()
            || widget == d->navigation->mainChromeWidget() || widget == d->navigation->footerChromeWidget()
            || widget == d->navigation->headerChromeWidget())
        && (event->type() == QEvent::Resize || event->type() == QEvent::Move || event->type() == QEvent::Show
            || event->type() == QEvent::LayoutRequest || event->type() == QEvent::ActivationChange)) {
        if (event->type() == QEvent::Resize && transitionRunning())
            cancelTransition();
        if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
            d->pointerMotion->stop();
            d->scene.pointerTilt = {};
        }
        if (!d->syncQueued) {
            d->syncQueued = true;
            QTimer::singleShot(0, this, [this] {
                d->syncQueued = false;
                d->sync();
            });
        }
    }
    if (!d->capture || !d->capture->isEnabled())
        return false;
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::Wheel
        || (event->type() == QEvent::Show
            && (widget->isWindow() || qobject_cast<overlay::OverlayScrim*>(widget)
                || widget->property(overlay::kOverlaySurfaceProperty).toBool())))
        d->pointerMotion->stop();
    if (widget == d->window && (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::Hide))
        d->followPointer({}, false);
    // A host-delivered move inside the scene is still over its projected control.
    // Clearing hover here would synthesize leave/enter and recapture it every frame.
    // zh_CN: 宿主转发的场景内移动仍命中投影控件，不能逐帧清空悬停并触发重绘。
    const bool outsideSceneMove = !inSource && widget != d->canvas && widget->window() == d->window->window()
        && event->type() == QEvent::MouseMove
        && (widget != d->window || !d->navigation->rect().contains(d->navigation->mapFromGlobal(mouseGlobal)));
    if ((widget == d->window && event->type() == QEvent::Leave) || outsideSceneMove) {
        QScopedValueRollback<bool> forward(d->forwarding, true);
        d->hover(nullptr, {});
        if (!d->grabbed && !d->firstOverlay())
            d->followPointer({});
    }
    if (widget == d->window && event->type() == QEvent::KeyPress && transitionRunning())
        cancelTransition();
    if (widget == d->window && (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::Hide)
        && transitionRunning())
        cancelTransition();
    // Popups remain native surfaces. Position their anchor on the presented panel while
    // leaving popup input and keyboard/IME handling to Qt.
    // zh_CN: 弹出层仍是原生控件，锚点映射到显示面板；弹窗输入与键盘/输入法仍交给 Qt。
    if (qobject_cast<QMenu*>(widget) && event->type() == QEvent::Show) {
        if (d->nativePopupAnchors.contains(widget))
            d->nativePopupAnchors[widget].source = nullptr;
        const auto declaration = overlay::presentation::menuAnchor(qobject_cast<QMenu*>(widget));
        auto* declaredSource = declaration.source.data();
        QWidget* source = declaredSource ? declaredSource : widget->parentWidget();
        // Child menus and foreign windows already live in native coordinates.
        // zh_CN: 子菜单和其他窗口已经使用原生坐标，不能再次投影。
        if (source && source->window() == d->window->window() && overlay::presentation::hasTransform(source)) {
            const QPoint point = declaredSource ? declaration.point : source->mapFromGlobal(widget->pos());
            const QPoint presented = overlay::presentation::mapToGlobal(source, point);
            if (!declaredSource) {
                QScopedValueRollback<bool> forward(d->forwarding, true);
                widget->move(presented);
            }
            if (!d->nativePopupAnchors.contains(widget))
                connect(widget, &QObject::destroyed, this, [this, widget] { d->nativePopupAnchors.remove(widget); });
            d->nativePopupAnchors.insert(widget, {source, point, widget->pos() - presented});
            const QPointer<QWidget> popup(widget);
            QTimer::singleShot(0, this, [this, popup] {
                if (popup && popup->isVisible() && d->nativePopupAnchors.contains(popup)) {
                    auto& anchor = d->nativePopupAnchors[popup];
                    if (anchor.source)
                        anchor.offset = popup->pos() - overlay::presentation::mapToGlobal(anchor.source, anchor.point);
                }
            });
        }
    }
    if (widget->window() != d->window->window() || (!inSource && widget != d->window))
        return false;
    if (inSource
        && (event->type() == QEvent::Enter || event->type() == QEvent::Leave || event->type() == QEvent::HoverEnter
            || event->type() == QEvent::HoverLeave || event->type() == QEvent::HoverMove))
        return true;
    const bool wheel = event->type() == QEvent::Wheel;
    const bool contextMenu = event->type() == QEvent::ContextMenu;
    const bool toolTip = event->type() == QEvent::ToolTip;
    if (!mouse && !wheel && !contextMenu && !toolTip)
        return false;
    // Qt's tooltip timer can retain that overwritten global point as well.
    // zh_CN: Qt 提示计时器也可能保留被覆盖的全局点，提示事件使用接收者局部坐标恢复。
    const QPoint global = mouse ? mouseGlobal
        : contextMenu           ? static_cast<QContextMenuEvent*>(event)->globalPos()
        : toolTip
        ? widget->mapToGlobal(static_cast<QHelpEvent*>(event)->pos())
        : static_cast<QWheelEvent*>(event)->globalPosition().toPoint();
    if (d->nativeOverlayAt(global)) {
        // Ignored input from an overlay label can bubble to the window. It must
        // never be reinterpreted as an interaction with the projected background.
        // zh_CN: 浮层文字未处理的输入可能冒泡到窗口，但不能再被解释为投影背景上的交互。
        QScopedValueRollback<bool> forward(d->forwarding, true);
        d->grabbed = nullptr;
        d->hover(nullptr, {});
        return true;
    }
    const QPoint presented = d->navigation->mapFromGlobal(global);
    if (!inSource && !d->navigation->rect().contains(presented))
        return false;
    if (event->type() == QEvent::MouseMove && static_cast<QMouseEvent*>(event)->buttons() == Qt::NoButton
        && !transitionRunning() && !QApplication::activePopupWidget()) {
        if (!d->firstOverlay()) {
            const qreal x = qBound(-1.0, 2.0 * presented.x() / d->navigation->width() - 1, 1.0);
            const qreal y = qBound(-1.0, 2.0 * presented.y() / d->navigation->height() - 1, 1.0);
            d->followPointer(QPointF(x * kPointerYaw, -y * kPointerPitch));
        }
    }
    QPointF source;
    const auto* hit = d->scene.unproject(presented, &source);
    const bool floatingPane = !d->scene.top && d->scene.navigation.source.right() > d->scene.content.source.left();
    if (!d->grabbed && floatingPane && hit != &d->scene.navigation && mouse
        && event->type() == QEvent::MouseButtonPress) {
        // Dismiss at the displayed drawer boundary, consuming the outside press as in 2D.
        // zh_CN: 在投影后的窗格边界外按下时轻关闭，并和 2D 一样吞掉本次按下。
        d->navigation->setPaneOpen(false);
        return true;
    }
    if (d->grabbed) {
        const auto& panel = d->belongsToContent(d->grabbed) ? d->scene.content : d->scene.navigation;
        source = panel.transform.inverted().map(presented);
    }
    QScopedValueRollback<bool> forward(d->forwarding, true);
    QWidget* target = d->grabbed ? d->grabbed.data() : hit ? d->navigation->childAt(source.toPoint()) : nullptr;
    if (!target) {
        d->hover(nullptr, {});
        return true;
    }
    const QPoint local = target->mapFrom(d->navigation, source.toPoint());
    const QPoint sourceGlobal = d->navigation->mapToGlobal(source.toPoint());
    if (mouse) {
        auto* input = static_cast<QMouseEvent*>(event);
        d->hover(target, source.toPoint());
        if (event->type() == QEvent::MouseButtonPress)
            d->grabbed = target;
        QMouseEvent forwarded(event->type(), local, target->mapTo(d->window, local), sourceGlobal, input->button(),
            input->buttons(), input->modifiers(), input->source());
        // Queue composition alongside the drag's source update, before Qt starts
        // painting. The capture then joins this frame instead of requesting another.
        // zh_CN: 拖动时提前合并合成与源更新，捕获结果进入本帧，避免额外提交一帧旧画面。
        if (input->buttons() != Qt::NoButton || input->button() != Qt::NoButton)
            requestCanvasFrame(d->canvas, true);
        QApplication::sendEvent(target, &forwarded);
        if (event->type() == QEvent::MouseButtonRelease)
            d->grabbed = nullptr;
    } else if (wheel) {
        auto* input = static_cast<QWheelEvent*>(event);
        // Qt deliberately does not bubble synthetic wheel events. Walk the widget
        // parents so scrolling over a label/card still reaches its scroll viewport.
        // zh_CN: Qt 不会冒泡合成滚轮事件，逐级转发让文字、卡片上的滚动也能到达滚动视口。
        for (QPointer<QWidget> receiver = target; receiver;) {
            const QPoint position = receiver->mapFromGlobal(sourceGlobal);
            QWheelEvent forwarded(position, sourceGlobal, input->pixelDelta(), input->angleDelta(), input->buttons(),
                input->modifiers(), input->phase(), input->inverted(), input->source());
            forwarded.setTimestamp(input->timestamp());
            forwarded.ignore();
            QApplication::sendEvent(receiver, &forwarded);
            if (!receiver || forwarded.isAccepted() || receiver->isWindow()
                || receiver->testAttribute(Qt::WA_NoMousePropagation))
                break;
            receiver = receiver->parentWidget();
        }
    } else if (toolTip) {
        // Hover help follows the same projected hit target as pointer input.
        // zh_CN: 悬停提示与指针输入使用相同的投影命中目标。
        QHelpEvent forwarded(QEvent::ToolTip, local, sourceGlobal);
        QApplication::sendEvent(target, &forwarded);
    } else {
        auto* input = static_cast<QContextMenuEvent*>(event);
        QContextMenuEvent forwarded(input->reason(), local, sourceGlobal, input->modifiers());
        QApplication::sendEvent(target, &forwarded);
    }
    return true;
}
} // namespace fluent::gallery
