#include "GallerySpatialController.h"
#include "GallerySpatialRenderPolicy.h"
#include "platform/GalleryPlatform.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QElapsedTimer>
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
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QPaintEngine>
#include <QProxyStyle>
#include <QStyleOption>
#include <QScopedValueRollback>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QtMath>
#include <functional>

#include "compatibility/QtCompat.h"
#include "components/foundation/MotionPolicy.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include "components/foundation/overlay/OverlayWindow.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/StackContentHost.h"
#include "components/windowing/WindowBackdrop.h"
#include "view/support/GalleryDepth.h"
#include "GallerySplashScreen.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {
namespace {
constexpr qreal kSideRotation = 5.0;
constexpr qreal kTopRotation = 3.0;
constexpr qreal kPointerYaw = .65;
constexpr qreal kPointerPitch = .45;

bool isSoftwareRenderer(const QString& renderer)
{
    const auto name = renderer.toLower();
    return name.isEmpty() || name.contains(QStringLiteral("llvmpipe")) ||
           name.contains(QStringLiteral("softpipe")) ||
           name.contains(QStringLiteral("swiftshader")) ||
           name.contains(QStringLiteral("software")) ||
           name.contains(QStringLiteral("basic render driver")) ||
           name.contains(QStringLiteral("warp")) || name.contains(QStringLiteral("gdi generic"));
}

QString currentRendererName(QOpenGLContext* context)
{
    if (!context || QOpenGLContext::currentContext() != context)
        return {};
    const QString hostRenderer = platform::graphicsRendererOverride();
    if (!hostRenderer.isEmpty())
        return hostRenderer;
    const auto* name = context->functions()->glGetString(GL_RENDERER);
    return name ? QString::fromLatin1(reinterpret_cast<const char*>(name)) : QString();
}

QString sessionUnavailableReason()
{
    if (qEnvironmentVariableIntValue("FLUENT_QT_GALLERY_DISABLE_3D") != 0)
        return QObject::tr("3D is disabled for this session.");
    const auto platform = QGuiApplication::platformName();
    if (platform == "offscreen" || platform == "minimal" || platform == "vnc")
        return QObject::tr("This display uses the 2D Gallery.");
    return {};
}

QString accelerationUnavailableReason()
{
    if (!platform::capabilities().probesOffscreenOpenGL)
        return {};
    QOpenGLContext probe;
    if (!probe.create())
        return QObject::tr("3D acceleration is unavailable. Using the 2D Gallery.");
    QOffscreenSurface surface;
    surface.setFormat(probe.format());
    surface.create();
    if (!surface.isValid() || !probe.makeCurrent(&surface))
        return QObject::tr("3D acceleration is unavailable. Using the 2D Gallery.");
    const QString renderer = currentRendererName(&probe);
    probe.doneCurrent();
    if (isSoftwareRenderer(renderer))
        return QObject::tr("Hardware acceleration is unavailable. Using the 2D Gallery.");
    return {};
}

QColor translucent(QColor color, qreal opacity)
{
    color.setAlphaF(qBound(0.0, opacity, 1.0));
    return color;
}

// Cocoa draws some native controls through CGContext, which cannot target an OpenGL
// painter. Rasterize only those style primitives; Fluent painting and text stay on GPU.
// zh_CN: Cocoa 的原生控件需要 CGContext；只为原生样式图元提供小位图，Fluent 绘制仍走 GPU。
class GpuCompatibleNativeStyle final : public QProxyStyle {
public:
    explicit GpuCompatibleNativeStyle(QStyle* base) : QProxyStyle(base)
    {
        setObjectName(base->objectName());
        setProperty("galleryGpuCompatibleStyle", true);
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                       const QWidget* widget = nullptr) const override
    {
        // QMacStyle delegates PE_Widget to QCommonStyle, where it paints nothing.
        if (isGpu(painter) && element == PE_Widget)
            return;
        paintNative(option, painter, [=](const QStyleOption* local, QPainter* target) {
            QProxyStyle::drawPrimitive(element, local, target, widget);
        });
    }

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                     const QWidget* widget = nullptr) const override
    {
        if (isGpu(painter) && element == CE_ShapedFrame) {
            const auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option);
            if (frame && frame->frameShape == QFrame::NoFrame)
                return;
        }
        paintNative(option, painter, [=](const QStyleOption* local, QPainter* target) {
            QProxyStyle::drawControl(element, local, target, widget);
        });
    }

    void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option,
                            QPainter* painter, const QWidget* widget = nullptr) const override
    {
        paintNative(option, painter, [=](const QStyleOption* local, QPainter* target) {
            QProxyStyle::drawComplexControl(control, static_cast<const QStyleOptionComplex*>(local),
                                            target, widget);
        });
    }

private:
    static bool isGpu(QPainter* painter)
    {
        return painter && painter->paintEngine()->type() == QPaintEngine::OpenGL2;
    }

    template <typename Paint>
    static void paintNative(const QStyleOption* option, QPainter* painter, Paint paint)
    {
        if (!isGpu(painter)) {
            paint(option, painter);
            return;
        }
        // Cocoa's NSView drawing ignores painter translations. Localize the option
        // itself for common primitives, preserving the concrete option's fields.
        // Other option types retain their original origin and native behavior.
        if (const auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option))
            paintLocal(*frame, painter, paint);
        else if (const auto* button = qstyleoption_cast<const QStyleOptionButton*>(option))
            paintLocal(*button, painter, paint);
        else if (option->type == QStyleOption::SO_Default)
            paintLocal(*option, painter, paint);
        else
            paintImage(
                option, painter,
                QRect(0, 0, qMax(1, option->rect.right() + 5), qMax(1, option->rect.bottom() + 5)),
                paint);
    }

    template <typename Option, typename Paint>
    static void paintLocal(Option option, QPainter* painter, Paint paint)
    {
        const QRect bounds = option.rect.adjusted(-4, -4, 4, 4);
        option.rect.translate(-bounds.topLeft());
        paintImage(&option, painter, bounds, paint);
    }

    template <typename Paint>
    static void paintImage(const QStyleOption* option, QPainter* painter, const QRect& bounds,
                           Paint paint)
    {
        const qreal dpr = painter->device()->devicePixelRatioF();
        QImage image(QSize(qCeil(bounds.width() * dpr), qCeil(bounds.height() * dpr)),
                     QImage::Format_ARGB32_Premultiplied);
        if (image.isNull())
            return;
        image.setDevicePixelRatio(dpr);
        image.fill(Qt::transparent);
        if (qEnvironmentVariableIntValue("FLUENT_QT_SPATIAL_BENCHMARK"))
            qApp->style()->setProperty("galleryLastNativeRasterPixels",
                                       qint64(image.width()) * image.height());
        QPainter native(&image);
        native.setFont(painter->font());
        native.setPen(painter->pen());
        native.setRenderHints(painter->renderHints());
        paint(option, &native);
        native.end();
        painter->drawImage(bounds.topLeft(), image);
    }
};

void prepareNativeStyle()
{
    auto* style = qApp->style();
    if (QGuiApplication::platformName() == QLatin1String("cocoa") &&
        style->objectName().contains(QLatin1String("mac"), Qt::CaseInsensitive) &&
        !style->property("galleryGpuCompatibleStyle").toBool())
        qApp->setStyle(new GpuCompatibleNativeStyle(style));
}

// Suppress backing-store painting and invalidate the GPU cache. During a GPU render pass,
// drawSource sends the widget tree directly to the OpenGL painter, without a CPU snapshot.
// zh_CN: 拦截后备存储绘制并使 GPU 缓存失效；实际绘制时直接输出到 OpenGL，避免整页 CPU 位图。
class SurfaceCapture final : public QGraphicsEffect {
public:
    std::function<void()> invalidated;
    bool rendering = false, composing = false;
    const bool measuring = qEnvironmentVariableIntValue("FLUENT_QT_SPATIAL_BENCHMARK") != 0;
    qint64 captures = 0, captureNanoseconds = 0;

protected:
    void draw(QPainter* painter) override
    {
        if (rendering) {
            drawSource(painter);
        } else if (!composing && invalidated) {
            invalidated();
        }
    }
};

struct ShellScene : FluentElement {
    struct Panel {
        QRectF source;
        QTransform transform;
    };
    quint64 navigationRevision = 0, contentRevision = 0;
    QPixmap backdrop;
    Panel navigation, content;
    qreal progress = 0;
    QPointF pointerTilt;
    bool top = false;
    std::function<void()> themeChanged;
    std::function<void(QPainter&, bool, const QRegion&)> renderWidgets;
    void onThemeUpdated() override
    {
        if (themeChanged)
            themeChanged();
    }

    static QTransform project(const QRectF& rect, qreal angle, bool top, qreal progress,
                              const QPointF& pointerTilt, bool navigation)
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
        QPolygonF original{rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()};
        QPolygonF projected;
        for (const auto& corner : original) {
            const auto p =
                rotation.map(QVector3D(corner.x() - center.x(), corner.y() - center.y(), 0));
            const qreal scale = camera / (camera - p.z());
            projected << QPointF(p.x() * scale, p.y() * scale);
        }
        // Fit inside each existing layout region. This leaves a material-filled gutter and
        // keeps the outer edges, scrollbars and navigation footer inside the window.
        // zh_CN: 在原布局区域内留出材质间隙，外缘、滚动条与底部导航均不会越出窗口。
        const qreal horizontalMargin =
            (navigation ? (top ? 5.0 : qMin(3.0, rect.width() * 6.0 / rect.height())) : 6.0) *
            progress;
        // A thin rail must not shrink in both axes just to create a vertical gutter.
        // zh_CN: 顶部窄导航不能为了竖向留白而等比缩小整条导航及文字。
        const qreal verticalMargin =
            (navigation && top ? qMin(.5, rect.height() * 5.0 / rect.width()) : 6.0) * progress;
        const QRectF available =
            rect.adjusted(horizontalMargin, verticalMargin, -horizontalMargin, -verticalMargin);
        const QRectF bounds = projected.boundingRect();
        const qreal fit =
            qMin(available.width() / bounds.width(), available.height() / bounds.height());
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
        for (auto* chrome :
             {view->headerChromeWidget(), view->mainChromeWidget(), view->footerChromeWidget()}) {
            if (chrome && !chrome->isHidden())
                paneWidth = qMax(paneWidth, qreal(chrome->geometry().right() + 1));
        }
        navigation.source = top ? QRectF(0, 0, view->width(), content.source.top())
                                : QRectF(0, 0, paneWidth, view->height());
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
    void paint(QPainter& painter,
               const std::function<void(QPainter&, const Panel&, bool)>& texture) const
    {
        if (!navigationRevision)
            return;
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        const auto colors = themeColors();
        const bool dark = effectiveThemeUsesDarkAppearance();
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
                painter.setClipRect(navigation.source.united(content.source).adjusted(1, 1, -1, -1),
                                    Qt::IntersectClip);
            painter.setTransform(panel.transform);
            // Keep the shadow outside the translucent pane so its interior does not turn muddy.
            // zh_CN: 柔影仅落在面板外侧，不叠入半透明材质内部，避免玻璃发灰。
            QPainterPath shadowArea;
            shadowArea.addRect(body.adjusted(-20, -20, 20, 24));
            shadowArea.addPath(outline);
            shadowArea.setFillRule(Qt::OddEvenFill);
            painter.save();
            painter.setClipPath(shadowArea, Qt::IntersectClip);
            overlay::paintLayeredShadow(painter, body.toAlignedRect(), radius,
                                        themeShadow(Elevation::High),
                                        (isNavigation ? .045 : .065) * progress, 10, 3);
            painter.restore();
            QLinearGradient material(body.topLeft(), body.bottomRight());
            const qreal opacity = isNavigation ? (dark ? .32 : .34) : (dark ? .62 : .56);
            material.setColorAt(0, translucent(colors.bgLayerAlt, opacity * progress));
            material.setColorAt(1, translucent(colors.bgLayerAlt, (opacity - .12) * progress));
            painter.fillPath(outline, material);
            texture(painter, panel, isNavigation);
            if (progress > 0) {
                // One directional reflection instead of a dark frame around every pane.
                // zh_CN: 用单向反光表达薄边，不再给面板围一圈深色边框。
                QLinearGradient rim(body.topLeft(), body.bottomRight());
                rim.setColorAt(0, translucent(colors.grey10, (dark ? .18 : .72) * progress));
                rim.setColorAt(.35, translucent(colors.grey10, .12 * progress));
                rim.setColorAt(1, translucent(colors.grey10, 0));
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(rim, 1));
                painter.drawPath(outline);
            }
            painter.restore();
        }
    }
};

// Sample a projected pixel's footprint rather than one point in a supersampled
// texture. Four bilinear taps retain thin strokes without mipmap upsampling blur.
class PanelSampler {
public:
    bool create()
    {
        auto* context = QOpenGLContext::currentContext();
        const bool es = context->isOpenGLES();
        // A Qt WebAssembly sharing wrapper can still report the requested ES 2
        // format while its current browser context is already WebGL 2 / ES 3.
        const QByteArray glVersion(
            reinterpret_cast<const char*>(context->functions()->glGetString(GL_VERSION)));
        const bool es3 = context->format().majorVersion() >= 3 ||
                         glVersion.startsWith("OpenGL ES 3.") || glVersion.contains("WebGL 2.");
        const bool modern = es ? es3 : context->format().profile() == QSurfaceFormat::CoreProfile;
        const QByteArray version =
            modern ? (es ? "#version 300 es\n" : "#version 150\n")
                   : (es ? "#extension GL_OES_standard_derivatives : enable\n" : "");
        const QByteArray precision = es ? "precision highp float;\n" : "";
        const QByteArray vertex =
            (modern ? "in vec2 position; out vec2 uv;\n"
                    : "attribute highp vec2 position; varying highp vec2 uv;\n") +
            QByteArray("uniform mat4 target; void main() {\n"
                       "uv = (position + 1.0) * 0.5;\n"
                       "gl_Position = target * vec4(position, 0.0, 1.0); }\n");
        const QByteArray fragment =
            (modern ? "in vec2 uv; out vec4 color;\n#define SAMPLE texture\n#define OUTPUT color\n"
                    : "varying highp vec2 uv;\n#define SAMPLE texture2D\n#define OUTPUT "
                      "gl_FragColor\n") +
            QByteArray(
                "uniform sampler2D source; uniform vec2 sourceSize;\n"
                "void main() {\n"
                "vec2 dx = dFdx(uv), dy = dFdy(uv);\n"
                "vec2 span = clamp(vec2(length(dx * sourceSize), length(dy * sourceSize)) - 1.0, "
                "0.0, 1.0);\n"
                "dx *= 0.25 * span.x; dy *= 0.25 * span.y;\n"
                "OUTPUT = 0.25 * (SAMPLE(source, uv - dx - dy) + SAMPLE(source, uv + dx - dy)\n"
                " + SAMPLE(source, uv - dx + dy) + SAMPLE(source, uv + dx + dy)); }\n");
        if (!m_program.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                               version + precision + vertex) ||
            !m_program.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                               version + precision + fragment))
            return false;
        m_program.bindAttributeLocation("position", 0);
        if (!m_program.link() || !m_vertices.create())
            return false;
        m_vao.create();
        QOpenGLVertexArrayObject::Binder vao(&m_vao);
        m_vertices.bind();
        const GLfloat vertices[] = {-1, -1, 1, -1, -1, 1, 1, 1};
        m_vertices.allocate(vertices, sizeof(vertices));
        m_vertices.release();
        return true;
    }

    bool isCreated() const { return m_program.isLinked() && m_vertices.isCreated(); }
    void blit(GLuint texture, const QSize& size, const QMatrix4x4& target)
    {
        auto* gl = QOpenGLContext::currentContext()->functions();
        QOpenGLVertexArrayObject::Binder vao(&m_vao);
        m_program.bind();
        m_vertices.bind();
        m_program.enableAttributeArray(0);
        m_program.setAttributeBuffer(0, GL_FLOAT, 0, 2);
        m_program.setUniformValue("target", target);
        m_program.setUniformValue("source", 0);
        m_program.setUniformValue("sourceSize", QVector2D(size.width(), size.height()));
        gl->glActiveTexture(GL_TEXTURE0);
        gl->glBindTexture(GL_TEXTURE_2D, texture);
        gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        gl->glBindTexture(GL_TEXTURE_2D, 0);
        m_program.disableAttributeArray(0);
        m_vertices.release();
        m_program.release();
    }

private:
    QOpenGLShaderProgram m_program;
    QOpenGLBuffer m_vertices;
    QOpenGLVertexArrayObject m_vao;
};

// Create the shared GL surface only after opting into 3D. Keep it hidden between
// later toggles to avoid repeatedly replacing the native window's backing store.
// zh_CN: 首次启用 3D 才创建共享 GL 表面；后续关闭时隐藏，避免反复重建原生窗口后备存储。
class GpuSurface final : public QOpenGLWidget {
public:
    GpuSurface(ShellScene* scene, QWidget* parent) : QOpenGLWidget(parent), m_scene(scene)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        setUpdateBehavior(NoPartialUpdate);
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
    qreal cacheDpr() const { return m_plan.dpr; }
    int maxCacheDimension() const { return m_maxDimension; }
    int allocationRetries = 0;
    int surfaceSamples = 0;
    int paintSamples() const { return m_paintTarget ? m_paintTarget->format().samples() : 0; }
    qint64 estimatedCacheBytes() const { return m_plan.estimatedBytes; }
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
        makeCurrent();
        m_navigation = {};
        m_content = {};
        m_paintTarget.reset();
        m_resolveTarget.reset();
        m_plan = {};
        m_maxExtraSampling = 2;
        m_cacheFailurePending = false;
        doneCurrent();
    }
    const bool measuring = qEnvironmentVariableIntValue("FLUENT_QT_SPATIAL_BENCHMARK") != 0;
    qint64 paints = 0, paintNanoseconds = 0;
    qint64 allocationNanoseconds = 0, firstPaintNanoseconds = 0;

protected:
    void initializeGL() override
    {
        auto* gl = context()->functions();
        GLint textureLimit = 0, renderbufferLimit = 0, viewportLimits[2] = {};
        gl->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &textureLimit);
        gl->glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &renderbufferLimit);
        gl->glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewportLimits);
        m_maxDimension =
            qMin(qMin(textureLimit, renderbufferLimit), qMin(viewportLimits[0], viewportLimits[1]));
        // A driver may round the requested sample count up. Query a tiny target
        // before planning large allocations so the memory bound remains accurate.
        QOpenGLFramebufferObjectFormat sampleFormat;
        sampleFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        sampleFormat.setInternalTextureFormat(GL_RGBA8);
        sampleFormat.setSamples(spatial_render::kPaintSamples);
        {
            QOpenGLFramebufferObject sampleProbe(QSize(1, 1), sampleFormat);
            m_paintSamples = sampleProbe.isValid() ? sampleProbe.format().samples() : 0;
        }
        m_blitter = std::make_unique<PanelSampler>();
        m_blitter->create();
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
        QElapsedTimer clock;
        if (measuring)
            clock.start();
        auto* gl = context()->functions();
        gl->glGetIntegerv(GL_SAMPLES, &surfaceSamples);
        // QPainter can leave a scissor/color mask behind. Clear the whole FBO before
        // drawing a new translucent frame, otherwise animated foregrounds leave trails.
        // zh_CN: QPainter 可能遗留裁剪或颜色掩码，清理整个 FBO，避免透明动效留下上一帧轨迹。
        gl->glDisable(GL_SCISSOR_TEST);
        gl->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        gl->glClearColor(0, 0, 0, 0);
        gl->glClear(GL_COLOR_BUFFER_BIT);
        if (property("presenting").toBool()) {
            if (!prepareCaches()) {
                if (!m_cacheFailurePending && cacheUnavailable) {
                    m_cacheFailurePending = true;
                    cacheUnavailable();
                }
                return;
            }
            const bool valid = ready() &&
                               updateTexture(m_content, m_scene->contentRevision,
                                             m_scene->content.source, false) &&
                               updateTexture(m_navigation, m_scene->navigationRevision,
                                             m_scene->navigation.source, true);
            if (!valid) {
                if (failed)
                    failed();
                return;
            }
            gl->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
            gl->glViewport(0, 0, qRound(width() * devicePixelRatioF()),
                           qRound(height() * devicePixelRatioF()));
        }
        QPainter painter(this);
        if (!m_scene->backdrop.isNull())
            painter.drawPixmap(0, 0, m_scene->backdrop);
        if (property("presenting").toBool()) {
            m_scene->paint(painter, [this](QPainter& p, const ShellScene::Panel& panel, bool nav) {
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
                m_blitter->blit(cache.texture->texture(), cache.texture->size(),
                                projection * QMatrix4x4(panel.transform) * quad);
                p.endNativePainting();
            });
        }
        painter.end();
        if (measuring) {
            if (!firstPaintNanoseconds && property("presenting").toBool())
                firstPaintNanoseconds = clock.nsecsElapsed();
            ++paints;
            paintNanoseconds += clock.nsecsElapsed();
        }
    }

private:
    struct TextureCache {
        std::unique_ptr<QOpenGLFramebufferObject> texture;
        quint64 revision = 0;
        QRectF source;
        qreal dpr = 0;
    };
    TextureCache m_navigation, m_content;
    std::unique_ptr<PanelSampler> m_blitter;
    std::unique_ptr<QOpenGLFramebufferObject> m_paintTarget;
    std::unique_ptr<QOpenGLFramebufferObject> m_resolveTarget;
    ShellScene* m_scene;
    spatial_render::CachePlan m_plan;
    int m_maxDimension = 0;
    int m_paintSamples = spatial_render::kPaintSamples;
    qreal m_maxExtraSampling = 2;
    bool m_cacheFailurePending = false;

    bool prepareCaches()
    {
        if (m_paintSamples <= 1)
            return false;
        const std::array<QSizeF, 2> panels = {m_scene->navigation.source.size(),
                                              m_scene->content.source.size()};
        while (true) {
            const auto plan = spatial_render::planCaches(
                panels, devicePixelRatioF(), m_maxDimension, m_maxExtraSampling,
                spatial_render::kCacheBudgetBytes, m_paintSamples);
            if (!plan.valid())
                return false;
            if (plan.sizes == m_plan.sizes && plan.dpr == m_plan.dpr)
                return true;
            // Release both old targets before allocating replacements: window resizing
            // must not transiently retain two complete sets of high-DPI caches.
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
            m_resolveTarget =
                std::make_unique<QOpenGLFramebufferObject>(plan.paintSize, textureFormat);
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
        m_navigation = {};
        m_content = {};
        m_paintTarget.reset();
        m_resolveTarget.reset();
        m_plan = {};
        m_maxExtraSampling = 2;
        m_cacheFailurePending = false;
        m_blitter.reset();
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
        const QSize pixels = cache.texture->size();
        QPainterPath outside;
        outside.addRect(QRectF(QPointF(), source.size()));
        outside.addRoundedRect(QRectF(QPointF(), source.size()).adjusted(.5, .5, -.5, -.5),
                               m_scene->themeRadius().overlay * 1.5,
                               m_scene->themeRadius().overlay * 1.5);
        outside.setFillRule(Qt::OddEvenFill);
        const int guard = qMax(1, qCeil(dpr));
        const int stride = pixels.height() <= m_plan.paintSize.height()
                               ? pixels.height()
                               : qMax(1, m_plan.paintSize.height() - 2 * guard);
        for (int top = 0; top < pixels.height(); top += stride) {
            const int height = qMin(stride, pixels.height() - top);
            // QWidget clips in logical pixels. Guard a full logical pixel so
            // rounded clip coordinates and MSAA coverage stay outside the copied strip.
            const int paintTop = qMax(0, top - guard);
            const int paintBottom = qMin(pixels.height(), top + height + guard);
            const int paintHeight = paintBottom - paintTop;
            m_paintTarget->bind();
            auto* gl = context()->functions();
            gl->glDisable(GL_SCISSOR_TEST);
            gl->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            gl->glClearColor(0, 0, 0, 0);
            gl->glStencilMask(~0u);
            gl->glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            QOpenGLPaintDevice device(QSize(pixels.width(), paintHeight));
            device.setDevicePixelRatio(dpr);
            QPainter painter(&device);
            painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
            painter.translate(0, -paintTop / dpr);
            const QRectF strip(0, paintTop / dpr, source.width(), paintHeight / dpr);
            painter.setClipRect(strip);
            const QPointF origin = navigation ? source.topLeft() : QPointF();
            painter.translate(-origin);
            // Supply the dirty strip to QWidget::render so unrelated children are skipped.
            // zh_CN: 按条带指定绘制区域，跳过未覆盖的子控件，复用同一抗锯齿画布。
            m_scene->renderWidgets(painter, navigation, strip.translated(origin).toAlignedRect());
            painter.translate(origin);
            painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
            painter.fillPath(outside, Qt::black);
            painter.end();
            gl->glDisable(GL_SCISSOR_TEST);
            // WebGL/GLES requires identical rectangles and formats for MSAA resolve.
            // Resolve first, then move the guarded strip into the panel texture.
            QOpenGLFramebufferObject::blitFramebuffer(m_resolveTarget.get(), m_paintTarget.get());
            // GL framebuffer coordinates run upwards; widget coordinates run downwards.
            QOpenGLFramebufferObject::blitFramebuffer(
                cache.texture.get(),
                QRect(0, pixels.height() - top - height, pixels.width(), height),
                m_resolveTarget.get(),
                QRect(0, paintBottom - top - height, pixels.width(), height));
        }
        cache.revision = revision;
        cache.source = source;
        cache.dpr = dpr;
        return true;
    }
};
} // namespace

struct GallerySpatialController::Private {
    QVariantMap initializationTimings;
    QObject* owner = nullptr;
    QPointer<QWidget> window;
    QPointer<navigation::NavigationView> navigation;
    QPointer<QWidget> canvas;
    QPointer<SurfaceCapture> capture;
    QPointer<SurfaceCapture> contentCapture;
    QPointer<QWidget> grabbed, hovered;
    QHash<QWidget*, QPoint> popupPositions;
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

    bool canInitializeRenderer() const
    {
        return GallerySettings::instance().spatialModeEnabled() && canvas && canvas->isVisible() &&
               !canvas->size().isEmpty() && window && window->window()->windowHandle() &&
               window->window()->windowHandle()->isExposed();
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
        for (QObject* child : window->children()) {
            auto* widget = qobject_cast<QWidget*>(child);
            if (widget && widget->isVisible() && !widget->isWindow() &&
                (qobject_cast<overlay::OverlayScrim*>(widget) ||
                 widget->property(overlay::kOverlaySurfaceProperty).toBool()))
                return widget;
        }
        return nullptr;
    }

    void raisePresentation()
    {
        if (auto* overlay = firstOverlay())
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

    void sync()
    {
        if (!navigation || !canvas || canvas->isHidden())
            return;
        const QRect bounds(navigation->mapTo(window, QPoint()), navigation->size());
        backdropDirty |= canvas->geometry() != bounds;
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
                window->render(&painter, QPoint(), QRegion(), QWidget::RenderFlags());
            }
        }
        if (!canvas->property("presenting").toBool()) {
            canvas->update();
            return;
        }
        scene.layout(navigation);
        canvas->setProperty("galleryRotationAxis", scene.top ? "X" : "Y");
        const qreal angle = scene.top ? kTopRotation : kSideRotation;
        canvas->setProperty("galleryNavigationRotation", angle * scene.progress);
        canvas->setProperty("galleryContentRotation", -angle * scene.progress);
        canvas->setProperty("galleryPointerTilt", scene.pointerTilt);
        canvas->setProperty("galleryDepthProgress", scene.progress);
        canvas->update();
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
            navigation->setAttribute(Qt::WA_NoSystemBackground, nativeNoSystemBackground);
            canvas->hide();
            static_cast<GpuSurface*>(canvas.data())->clearFrameCaches();
            setFiltering(false);
            canvas->setProperty("galleryDepthProgress", 0.0);
        }
        QEvent compositionChanged(depth::changeEvent());
        QCoreApplication::sendEvent(navigation, &compositionChanged);
        if (!target) {
            scene.navigationRevision = 0;
            scene.contentRevision = 0;
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QEnterEvent enter(local, target->mapTo(window, local), target->mapToGlobal(local));
#else
            QEvent enter(QEvent::Enter);
#endif
            QApplication::sendEvent(target, &enter);
        }
    }
};

void GallerySpatialController::prepareApplicationStyle()
{
    prepareNativeStyle();
}

GallerySpatialController::GallerySpatialController(QWidget* window,
                                                   navigation::NavigationView* navigation)
    : QObject(window), d(new Private)
{
    setObjectName(QStringLiteral("gallerySpatialController"));
    d->owner = this;
    d->window = window;
    d->navigation = navigation;
    d->scene.themeChanged = [this] {
        d->backdropDirty = true;
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
    connect(d->pointerMotion, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) {
                d->scene.pointerTilt = value.toPointF();
                d->sync();
            });
    auto& settings = GallerySettings::instance();
    connect(&settings, &GallerySettings::spatialModeEnabledChanged, this,
            &GallerySpatialController::applyMode);
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
    const QString unavailableReason = sessionUnavailableReason();
    if (!unavailableReason.isEmpty()) {
        disableSpatial(unavailableReason);
        return;
    }
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
    if (d->rendererFailed)
        return;
    if (!d->canvas) {
        const bool measuring = qEnvironmentVariableIntValue("FLUENT_QT_SPATIAL_BENCHMARK");
        QElapsedTimer clock;
        if (measuring)
            clock.start();
        const QString reason = accelerationUnavailableReason();
        if (measuring) {
            d->initializationTimings["probeMs"] = clock.nsecsElapsed() / 1e6;
            clock.restart();
        }
        if (!reason.isEmpty()) {
            disableSpatial(reason);
            return;
        }
        prepareNativeStyle();
        if (measuring)
            d->initializationTimings["nativeStyleMs"] = clock.nsecsElapsed() / 1e6;
        auto* surface = new GpuSurface(&d->scene, d->window);
        d->canvas = surface;
        surface->setObjectName(QStringLiteral("gallerySpatialSurface"));
        surface->setProperty("galleryGpuComposition", true);
        surface->initialized = [this] {
            d->rendererInitialized = true;
            QTimer::singleShot(0, this, &GallerySpatialController::checkRenderer);
        };
        surface->contextLost = [this] {
            // Reparenting can destroy a context before its replacement is initialized.
            // zh_CN: 更换父窗口时，旧上下文销毁后替代上下文可能尚未初始化。
            d->rendererInitialized = d->rendererReady = false;
            QTimer::singleShot(0, this, &GallerySpatialController::checkRenderer);
        };
        surface->cacheUnavailable = [this] {
            QTimer::singleShot(0, this, &GallerySpatialController::releaseOversizedPresentation);
        };
        surface->failed = [this] {
            QTimer::singleShot(0, this, [this] {
                disableSpatial(tr("3D rendering is unavailable. Using the 2D Gallery."));
            });
        };
        surface->lower();
        d->navigation->setMouseTracking(true);
        d->window->setMouseTracking(true);
    }
    d->setFiltering(true);
    d->canvas->setGeometry(QRect(d->navigation->mapTo(d->window, QPoint()), d->navigation->size()));
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
    auto* surface = static_cast<GpuSurface*>(d->canvas.data());
    if (!surface->isValid() || !surface->ready()) {
        disableSpatial(tr("3D could not start. Using the 2D Gallery."));
        return;
    }
    surface->makeCurrent();
    const QString renderer = currentRendererName(surface->context());
    surface->doneCurrent();
    if (isSoftwareRenderer(renderer)) {
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
    d->scene.navigationRevision = 0;
    d->scene.contentRevision = 0;
    d->scene.backdrop = {};
    d->grabbed = nullptr;
    d->hovered = nullptr;
    if (auto* surface = static_cast<GpuSurface*>(d->canvas.data())) {
        surface->initialized = {};
        surface->contextLost = {};
        surface->failed = {};
        surface->hide();
        surface->deleteLater();
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
    if (!d->navigation || !d->canvas || d->rendererFailed ||
        !GallerySettings::instance().spatialModeEnabled() ||
        d->window->findChild<GallerySplashScreen*>())
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
    d->contentCapture->invalidated = [this] {
        ++d->scene.contentRevision;
        d->canvas->update();
    };
    d->capture->invalidated = [this] {
        ++d->scene.navigationRevision;
        ++d->scene.contentRevision;
        d->scene.layout(d->navigation);
        d->canvas->update();
    };
    d->scene.renderWidgets = [this](QPainter& painter, bool navigation, const QRegion& region) {
        QScopedValueRollback<bool> navComposing(d->capture->composing, true);
        QScopedValueRollback<bool> contentComposing(d->contentCapture->composing, true);
        auto* capture = navigation ? d->capture.data() : d->contentCapture.data();
        auto* widget = navigation ? static_cast<QWidget*>(d->navigation.data())
                                  : static_cast<QWidget*>(d->navigation->contentHost());
        QScopedValueRollback<bool> rendering(capture->rendering, true);
        QElapsedTimer clock;
        if (capture->measuring)
            clock.start();
        // Preserve transparent hosts instead of forcing a palette window background.
        widget->render(&painter, region.boundingRect().topLeft(), region, QWidget::DrawChildren);
        if (capture->measuring) {
            ++capture->captures;
            capture->captureNanoseconds += clock.nsecsElapsed();
        }
    };
    d->navigation->contentHost()->setGraphicsEffect(d->contentCapture);
    d->navigation->setGraphicsEffect(d->capture);
    applyMode(GallerySettings::instance().spatialModeEnabled());
}
GallerySpatialController::~GallerySpatialController()
{
    qApp->removeEventFilter(this);
    if (d->capture) {
        d->capture->invalidated = {};
        d->capture->setEnabled(false);
    }
    if (d->contentCapture) {
        d->contentCapture->invalidated = {};
        d->contentCapture->setEnabled(false);
    }
    delete d->canvas;
}

bool GallerySpatialController::transitionRunning() const
{
    return d->motion->state() == QAbstractAnimation::Running;
}
QVariantMap GallerySpatialController::renderingStatistics() const
{
    QVariantMap result = d->initializationTimings;
    for (const auto& entry : {qMakePair(QStringLiteral("navigation"), d->capture.data()),
                              qMakePair(QStringLiteral("content"), d->contentCapture.data())}) {
        result[entry.first + "Captures"] = entry.second ? entry.second->captures : 0;
        result[entry.first + "CaptureMs"] =
            entry.second ? entry.second->captureNanoseconds / 1e6 : 0;
    }
    auto* surface = static_cast<GpuSurface*>(d->canvas.data());
    result["paints"] = surface ? surface->paints : 0;
    result["paintMs"] = surface ? surface->paintNanoseconds / 1e6 : 0;
    result["allocationMs"] = surface ? surface->allocationNanoseconds / 1e6 : 0;
    result["firstPaintMs"] = surface ? surface->firstPaintNanoseconds / 1e6 : 0;
    result["cachedPixels"] = surface ? surface->cachedPixels() : 0;
    result["cacheDpr"] = surface ? surface->cacheDpr() : 0;
    result["cacheEstimatedBytes"] = surface ? surface->estimatedCacheBytes() : 0;
    result["paintSamples"] = surface ? surface->paintSamples() : 0;
    result["paintTargetHeight"] = surface ? surface->paintTargetHeight() : 0;
    result["cacheBudgetBytes"] = spatial_render::kCacheBudgetBytes;
    result["maxCacheDimension"] = surface ? surface->maxCacheDimension() : 0;
    result["allocationRetries"] = surface ? surface->allocationRetries : 0;
    result["surfaceSamples"] = surface ? surface->surfaceSamples : 0;
    return result;
}
void GallerySpatialController::cancelTransition()
{
    if (d->navigation && d->canvas && d->capture)
        d->settle();
}
QPoint GallerySpatialController::projectedPosition(const QWidget* widget, const QPoint& point) const
{
    if (d->capture && d->capture->isEnabled() &&
        (widget == d->navigation || d->navigation->isAncestorOf(widget))) {
        const QPoint local = widget->mapTo(d->navigation, point);
        const QPointF presented =
            widget == d->navigation
                ? d->scene.projectPoint(local)
                : (d->belongsToContent(widget) ? d->scene.content : d->scene.navigation)
                      .transform.map(QPointF(local));
        return presented.toPoint() + d->navigation->mapTo(d->window, QPoint());
    }
    return widget->mapTo(d->window, point);
}
void GallerySpatialController::applyMode(bool enabled)
{
    if (!d->window || !d->navigation)
        return;
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
    const bool animate = d->window->isVisible() &&
                         !d->window->findChild<QWidget*>("gallerySplashScreen") &&
                         MotionPolicy::instance().mode() == MotionPolicy::Mode::Full &&
                         FluentElement::currentTheme() != FluentElement::HighContrast;
    if (!animate || qFuzzyCompare(d->scene.progress + 1, (d->target ? 1.0 : 0.0) + 1)) {
        d->settle();
        return;
    }
    d->contentCapture->setEnabled(true);
    d->capture->setEnabled(true);
    d->canvas->setProperty("presenting", true);
    d->raisePresentation();
    d->sync();
    d->motion->setDuration(
        qMax(1, qRound(420 * qAbs((d->target ? 1.0 : 0.0) - d->scene.progress))));
    d->motion->setStartValue(d->scene.progress);
    d->motion->setEndValue(d->target ? 1.0 : 0.0);
    d->motion->start();
}

bool GallerySpatialController::eventFilter(QObject* watched, QEvent* event)
{
    if (d->forwarding || !d->window || !d->navigation || !d->canvas)
        return false;
    if (watched == d->window && !d->rendererReady &&
        (event->type() == QEvent::Show || event->type() == QEvent::UpdateRequest))
        QTimer::singleShot(0, this, &GallerySpatialController::checkRenderer);
    auto* widget = qobject_cast<QWidget*>(watched);
    if (!widget)
        return false;
    // A native popup can receive the release of the press that opened it. Do not
    // keep routing later clicks/wheels to that original opener.
    // zh_CN: 打开原生弹窗后，释放事件可能由弹窗接收；不能把后续输入一直发给打开按钮。
    if ((event->type() == QEvent::MouseButtonRelease && widget->window() != d->window->window()) ||
        (event->type() == QEvent::MouseMove &&
         static_cast<QMouseEvent*>(event)->buttons() == Qt::NoButton) ||
        (event->type() == QEvent::Wheel &&
         static_cast<QWheelEvent*>(event)->buttons() == Qt::NoButton))
        d->grabbed = nullptr;
    const bool inSource = widget == d->navigation || d->navigation->isAncestorOf(widget);
    if (!inSource && widget != d->window && event->type() == QEvent::MouseButtonRelease)
        d->grabbed = nullptr;
    if (widget == d->window && event->type() == QEvent::ActivationChange)
        d->backdropDirty = true;
    if ((widget == d->window || widget == d->navigation || widget == d->navigation->contentHost() ||
         widget == d->navigation->mainChromeWidget() ||
         widget == d->navigation->footerChromeWidget() ||
         widget == d->navigation->headerChromeWidget()) &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Move ||
         event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest ||
         event->type() == QEvent::ActivationChange)) {
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
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::Wheel ||
        (event->type() == QEvent::Show &&
         (widget->isWindow() || qobject_cast<overlay::OverlayScrim*>(widget) ||
          widget->property(overlay::kOverlaySurfaceProperty).toBool())))
        d->pointerMotion->stop();
    if (widget == d->window &&
        (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::Hide))
        d->followPointer({}, false);
    if (event->type() == QEvent::Show || event->type() == QEvent::Move) {
        auto* flyout = qobject_cast<dialogs_flyouts::Flyout*>(widget);
        auto* anchor = flyout ? flyout->anchor() : nullptr;
        if (anchor && d->navigation->isAncestorOf(anchor) && widget->parentWidget() == d->window &&
            (!d->popupPositions.contains(widget) ||
             d->popupPositions.value(widget) != widget->pos())) {
            const QRect logical(anchor->mapTo(d->window, QPoint()), anchor->size());
            const QPolygon presentedCorners{projectedPosition(anchor, anchor->rect().topLeft()),
                                            projectedPosition(anchor, anchor->rect().topRight()),
                                            projectedPosition(anchor, anchor->rect().bottomRight()),
                                            projectedPosition(anchor, anchor->rect().bottomLeft())};
            const QRect presented = presentedCorners.boundingRect();
            const QRect card = overlay::visibleCardGeometry(widget->geometry());
            QPoint delta = presented.center() - logical.center();
            if (card.bottom() <= logical.top())
                delta.setY(presented.top() - logical.top());
            else if (card.top() >= logical.bottom())
                delta.setY(presented.bottom() - logical.bottom());
            QPoint position = widget->pos() + delta;
            const int shadow = overlay::defaultShadowMargin();
            position.setX(
                qBound(-shadow, position.x(), d->window->width() + shadow - widget->width()));
            position.setY(
                qBound(-shadow, position.y(), d->window->height() + shadow - widget->height()));
            if (!d->popupPositions.contains(widget))
                connect(widget, &QObject::destroyed, this,
                        [this, widget] { d->popupPositions.remove(widget); });
            d->popupPositions.insert(widget, position);
            QScopedValueRollback<bool> forward(d->forwarding, true);
            widget->move(position);
        }
    }
    // A host-delivered move inside the scene is still over its projected control.
    // Clearing hover here would synthesize leave/enter and recapture it every frame.
    // zh_CN: 宿主转发的场景内移动仍命中投影控件，不能逐帧清空悬停并触发重绘。
    const bool outsideSceneMove =
        !inSource && widget != d->canvas && widget->window() == d->window->window() &&
        event->type() == QEvent::MouseMove &&
        (widget != d->window || !d->navigation->rect().contains(d->navigation->mapFromGlobal(
                                    fluentMouseGlobalPos(static_cast<QMouseEvent*>(event)))));
    if ((widget == d->window && event->type() == QEvent::Leave) || outsideSceneMove) {
        QScopedValueRollback<bool> forward(d->forwarding, true);
        d->hover(nullptr, {});
        if (!d->grabbed && !d->firstOverlay())
            d->followPointer({});
    }
    if (widget == d->window && event->type() == QEvent::KeyPress && transitionRunning())
        cancelTransition();
    if (widget == d->window &&
        (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::Hide) &&
        transitionRunning())
        cancelTransition();
    // Popups remain native surfaces. Position their anchor on the presented panel while
    // leaving popup input and keyboard/IME handling to Qt.
    // zh_CN: 弹出层仍是原生控件，锚点映射到显示面板；弹窗输入与键盘/输入法仍交给 Qt。
    if (inSource && widget->isWindow() && widget != d->window && event->type() == QEvent::Show) {
        const QPoint source = d->navigation->mapFromGlobal(widget->pos());
        const QPoint mapped = d->scene.projectPoint(source).toPoint();
        widget->move(d->navigation->mapToGlobal(mapped));
    }
    if (widget->window() != d->window->window() || (!inSource && widget != d->window))
        return false;
    if (inSource && (event->type() == QEvent::Enter || event->type() == QEvent::Leave ||
                     event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave ||
                     event->type() == QEvent::HoverMove))
        return true;
    const bool mouse =
        event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseButtonDblClick;
    const bool wheel = event->type() == QEvent::Wheel;
    const bool contextMenu = event->type() == QEvent::ContextMenu;
    const bool toolTip = event->type() == QEvent::ToolTip;
    if (!mouse && !wheel && !contextMenu && !toolTip)
        return false;
    const QPoint global = mouse         ? fluentMouseGlobalPos(static_cast<QMouseEvent*>(event))
                          : contextMenu ? static_cast<QContextMenuEvent*>(event)->globalPos()
                          : toolTip     ? static_cast<QHelpEvent*>(event)->globalPos()
                                    : static_cast<QWheelEvent*>(event)->globalPosition().toPoint();
    const QPoint presented = d->navigation->mapFromGlobal(global);
    if (!inSource && !d->navigation->rect().contains(presented))
        return false;
    if (event->type() == QEvent::MouseMove &&
        static_cast<QMouseEvent*>(event)->buttons() == Qt::NoButton && !transitionRunning() &&
        !QApplication::activePopupWidget()) {
        if (!d->firstOverlay()) {
            const qreal x = qBound(-1.0, 2.0 * presented.x() / d->navigation->width() - 1, 1.0);
            const qreal y = qBound(-1.0, 2.0 * presented.y() / d->navigation->height() - 1, 1.0);
            d->followPointer(QPointF(x * kPointerYaw, -y * kPointerPitch));
        }
    }
    QPointF source;
    const auto* hit = d->scene.unproject(presented, &source);
    const bool floatingPane =
        !d->scene.top && d->scene.navigation.source.right() > d->scene.content.source.left();
    if (!d->grabbed && floatingPane && hit != &d->scene.navigation && mouse &&
        event->type() == QEvent::MouseButtonPress) {
        // Dismiss at the displayed drawer boundary, consuming the outside press as in 2D.
        // zh_CN: 在投影后的窗格边界外按下时轻关闭，并和 2D 一样吞掉本次按下。
        d->navigation->setPaneOpen(false);
        return true;
    }
    if (d->grabbed) {
        const auto& panel =
            d->belongsToContent(d->grabbed) ? d->scene.content : d->scene.navigation;
        source = panel.transform.inverted().map(presented);
    }
    QScopedValueRollback<bool> forward(d->forwarding, true);
    QWidget* target = d->grabbed ? d->grabbed.data()
                      : hit      ? d->navigation->childAt(source.toPoint())
                                 : nullptr;
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
        QMouseEvent forwarded(event->type(), local, target->mapTo(d->window, local), sourceGlobal,
                              input->button(), input->buttons(), input->modifiers(),
                              input->source());
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
            QWheelEvent forwarded(position, sourceGlobal, input->pixelDelta(), input->angleDelta(),
                                  input->buttons(), input->modifiers(), input->phase(),
                                  input->inverted(), input->source());
            forwarded.setTimestamp(input->timestamp());
            forwarded.ignore();
            QApplication::sendEvent(receiver, &forwarded);
            if (!receiver || forwarded.isAccepted() || receiver->isWindow() ||
                receiver->testAttribute(Qt::WA_NoMousePropagation))
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
