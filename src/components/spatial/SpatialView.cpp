#include "SpatialView.h"
#include "compatibility/PaintCompat.h"
#include "compatibility/private/spatial/GraphicsViewPaintCompat_p.h"
#include "compatibility/InputEventCompat.h"
#include "SpatialRuntime.h"
#include "SpatialSurface_p.h"

#include <QApplication>
#include <QDynamicPropertyChangeEvent>
#include <QElapsedTimer>
#include <QGraphicsProxyWidget>
#include <QGraphicsEffect>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLPaintDevice>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPointer>
#include <QRegion>
#include <QShowEvent>
#include <QStackedLayout>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <algorithm>
#include <cmath>
#include <functional>
#include "components/foundation/MotionPolicy.h"
#include "components/layout/Card.h"
#include "components/scrolling/ScrollView.h"

namespace fluent::spatial {
namespace {
constexpr char kThemeOverride[] = "fluentThemeOverride";
void refreshContentTheme(QWidget* widget)
{
    if (auto* element = dynamic_cast<FluentElement*>(widget))
        element->onThemeUpdated();
    for (auto* child : widget->findChildren<QWidget*>())
        if (auto* element = dynamic_cast<FluentElement*>(child))
            element->onThemeUpdated();
}
bool finite(const QVector3D& v)
{
    return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
}
bool finite(const QPointF& p)
{
    return std::isfinite(p.x()) && std::isfinite(p.y());
}
class SpatialViewport final : public QOpenGLWidget {
public:
    explicit SpatialViewport(QGraphicsView* view) : m_view(view) {}
    bool paintAtDeviceScale()
    {
        return ::compatibility::detail::paintLowDprOpenGLViewport(this, [this] { paintGL(); });
    }

protected:
    bool event(QEvent* event) override
    {
        observeExposureEvent(event->type());
        return QOpenGLWidget::event(event);
    }
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == this && event->type() == QEvent::Paint && m_fullExposurePaint) {
            m_fullExposurePaint = false;
            return QOpenGLWidget::event(event);
        }
        observeExposureEvent(event->type());
        return QOpenGLWidget::eventFilter(watched, event);
    }
    void paintGL() override
    {
        // Qt may invoke paintGL() directly (e.g. widget capture) bypassing QGraphicsView::paintEvent(),
        // and ancestor scrolling changes clipping without a local move/resize; repaint the full FBO here.
        QOpenGLPaintDevice target(size() * devicePixelRatioF());
        fluentSetRenderTargetDevicePixelRatio(target, devicePixelRatioF());
        QPainter painter(&target);
        fluentApplyRenderTargetScale(painter, devicePixelRatioF());
        painter.setRenderHints(m_view->renderHints());
        painter.fillRect(rect(), m_view->backgroundBrush());
        // Device already scales by DPR; pass the logical viewport rect to render().
        m_view->render(&painter, QRectF(rect()), m_view->viewport()->rect());
    }

private:
    void observeExposureEvent(QEvent::Type type)
    {
        if (type == QEvent::ParentChange)
            m_rebuildAncestors = true;
        if (type == QEvent::Hide)
            m_exposed = {};
        if (type != QEvent::Move && type != QEvent::Resize && type != QEvent::Show && type != QEvent::Hide
            && type != QEvent::ParentChange)
            return;
        if (m_exposurePending)
            return;
        m_exposurePending = true;
        QTimer::singleShot(0, this, [this] {
            m_exposurePending = false;
            if (m_rebuildAncestors) {
                for (const auto& ancestor : m_ancestors)
                    if (ancestor)
                        ancestor->removeEventFilter(this);
                m_ancestors.clear();
                for (auto* ancestor = parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
                    m_ancestors.append(ancestor);
                    ancestor->installEventFilter(this);
                }
                m_rebuildAncestors = false;
            }
            QRect exposed = isVisible() ? rect() : QRect();
            for (const auto& ancestor : m_ancestors)
                if (ancestor)
                    exposed &= QRect(mapFrom(ancestor, QPoint()), ancestor->size());
            // Repaint when ancestor scrolling exposes previously clipped regions of the GL texture.
            const bool revealed = !QRegion(exposed).subtracted(m_exposed).isEmpty();
            m_exposed = exposed;
            if (revealed) {
                m_fullExposurePaint = true;
                update();
            }
        });
    }
    QGraphicsView* m_view;
    QList<QPointer<QWidget>> m_ancestors;
    QRect m_exposed;
    bool m_rebuildAncestors = true;
    bool m_exposurePending = false;
    bool m_fullExposurePaint = false;
};
class SpatialCanvas final : public QGraphicsView {
public:
    explicit SpatialCanvas(QWidget* parent) : QGraphicsView(parent)
    {
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setMouseTracking(true);
        setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    }
    std::function<void(QPointF)> pointer;
    std::function<void()> resized, nativeRequested;

protected:
    void paintEvent(QPaintEvent* event) override
    {
        auto* gl = dynamic_cast<SpatialViewport*>(viewport());
        if (gl && gl->paintAtDeviceScale())
            return;
        QGraphicsView::paintEvent(event);
    }

    void resizeEvent(QResizeEvent* e) override
    {
        QGraphicsView::resizeEvent(e);
        if (resized)
            resized();
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (e->buttons() == Qt::NoButton && pointer)
            pointer({2.0 * fluentMousePos(e).x() / std::max(1, viewport()->width()) - 1,
                2.0 * fluentMousePos(e).y() / std::max(1, viewport()->height()) - 1});
        QGraphicsView::mouseMoveEvent(e);
    }
    void leaveEvent(QEvent* e) override
    {
        if (pointer)
            pointer({});
        QGraphicsView::leaveEvent(e);
    }
    bool event(QEvent* e) override
    {
        if (e->type() == QEvent::KeyPress && nativeRequested) {
            const int key = static_cast<QKeyEvent*>(e)->key();
            if (key == Qt::Key_Tab || key == Qt::Key_Backtab || key == Qt::Key_Escape) {
                nativeRequested();
                return true;
            }
        }
        return QGraphicsView::event(e);
    }
};
bool supported(QWidget* widget)
{
    if (widget->testAttribute(Qt::WA_NativeWindow) || widget->testAttribute(Qt::WA_PaintOnScreen)
        || qobject_cast<QOpenGLWidget*>(widget))
        return false;
    for (auto* child : widget->findChildren<QWidget*>())
        if (child->testAttribute(Qt::WA_NativeWindow) || child->testAttribute(Qt::WA_PaintOnScreen)
            || qobject_cast<QOpenGLWidget*>(child))
            return false;
    return true;
}
} // namespace

struct SpatialItem::Private {
    SpatialView* owner = nullptr;
    QPointer<QWidget> widget, originalParent;
    QPointer<SpatialSurfaceProxy> proxy;
    QMetaObject::Connection destroyedConnection;
    WidgetOwnership ownership = WidgetOwnership::Borrowed;
    QVector3D position, rotation;
    QPointF pivot{.5, .5};
    qreal scale = 1;
    qreal surfaceIntensity = 0, hoverLift = 0;
    bool visible = true, inheritsHostTheme = true;
};

struct SpatialView::Private {
    SpatialView* q;
    SpatialCanvas* canvas = nullptr;
    QGraphicsScene* scene = nullptr;
    scrolling::ScrollView* flat = nullptr;
    layout::Card* flatContent = nullptr;
    QVBoxLayout* flatLayout = nullptr;
    QStackedLayout* stack = nullptr;
    QList<SpatialItem*> items;
    QTimer* motion = nullptr;
    QTimer* modeChange = nullptr;
    QElapsedTimer clock;
    QPointF pointer, target, tilt{10, 16};
    qreal camera = 1000, zoom = 1;
    int response = 140, frameRate = 60;
    bool spatial = true, requestedSpatial = true, tracking = true, cache = true;
    bool destroying = false, projecting = false, geometryQueued = false, updatingTheme = false;
    bool countNotificationQueued = false;
    bool backendQueued = false, checkQueued = false, preflightChecked = false;
    RenderMode mode = RenderMode::Auto;
    Backend backend = Backend::Raster;
    QString renderer, reason, gpuFailure;
    QPointer<QOpenGLContext> checkedContext;
    QList<QPointer<QWidget>> ancestors;
    QList<QMetaObject::Connection> ancestorDestructionConnections;
    QList<const QObject*> destroyingAncestors;
    bool ancestorDestroying = false;
    bool ancestorsDirty = true, environmentRaster = true;

    void inspectEnvironment(QEvent::Type type)
    {
        if (destroying || ancestorDestroying)
            return;
        if (type == QEvent::ParentChange)
            ancestorsDirty = true;
        if (type != QEvent::Show && type != QEvent::Hide && type != QEvent::Move && type != QEvent::Resize
            && type != QEvent::ParentChange && type != QEvent::UpdateRequest && type != QEvent::LayoutRequest)
            return;
        if (ancestorsDirty) {
            for (const auto& connection : ancestorDestructionConnections)
                QObject::disconnect(connection);
            ancestorDestructionConnections.clear();
            for (const auto& ancestor : ancestors)
                if (ancestor)
                    ancestor->removeEventFilter(q);
            ancestors.clear();
            for (auto* ancestor = q->parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
                ancestors.append(ancestor);
                ancestor->installEventFilter(q);
                ancestorDestructionConnections.append(
                    QObject::connect(ancestor, &QObject::destroyed, q, [this](QObject* object) {
                        // Ancestor teardown has already broken the widget focus chain; never reparent cards back into it.
                        ancestorDestroying = true;
                        destroyingAncestors.append(object);
                        motion->stop();
                        modeChange->stop();
                    }));
            }
            ancestorsDirty = false;
        }
        QRect exposed = q->isVisible() ? q->rect() : QRect();
        bool composed = false;
        for (const auto& ancestor : ancestors) {
            if (!ancestor)
                continue;
            exposed &= QRect(q->mapFrom(ancestor, QPoint()), ancestor->size());
            composed |= ancestor->graphicsEffect() && ancestor->graphicsEffect()->isEnabled();
        }
        const bool raster = exposed.isEmpty() || composed;
        if (environmentRaster != raster) {
            environmentRaster = raster;
            queueBackend();
        }
    }

    bool parentIsTearingDown(const QObject* parent) const
    {
        for (auto* ancestor = parent; ancestor; ancestor = ancestor->parent())
            if (destroyingAncestors.contains(ancestor))
                return true;
        return false;
    }

    void syncContentTheme(SpatialItem* item)
    {
        auto* widget = item->widget();
        if (!widget)
            return;
        // Propagate host theme to proxy roots (which lack a QWidget parent) unless explicitly overridden.
        if (item->d->inheritsHostTheme) {
            updatingTheme = true;
            widget->setProperty(kThemeOverride, int(q->effectiveTheme()));
            updatingTheme = false;
        }
        refreshContentTheme(widget);
    }

    bool allowsSpatial() const
    {
        return q->effectiveTheme() != FluentElement::HighContrast
            && MotionPolicy::instance().shouldAnimate(true, MotionPolicy::Kind::Continuous);
    }
    void settle()
    {
        motion->stop();
        pointer = target = {};
        for (auto* item : items)
            if (item->d->proxy)
                item->d->proxy->resetInteraction();
        q->updateProjection();
    }
    void pointAt(QPointF point)
    {
        if (!spatial || !tracking || !allowsSpatial() || !q->isVisible() || !q->window()->isActiveWindow()
            || scene->mouseGrabberItem())
            return;
        target = {std::clamp(point.x(), -1.0, 1.0), std::clamp(point.y(), -1.0, 1.0)};
        if (!response) {
            pointer = target;
            motion->stop();
            q->updateProjection();
        } else if (!motion->isActive() && (target - pointer).manhattanLength() > .001) {
            clock.start();
            motion->start();
        }
    }
    void advance()
    {
        if (!spatial || !tracking || !q->isVisible() || !q->window()->isActiveWindow() || !allowsSpatial()) {
            settle();
            return;
        }
        if (scene->mouseGrabberItem()) {
            motion->stop();
            target = pointer;
            return;
        }
        const qreal dt = std::clamp<qreal>(clock.restart(), 1, 100);
        const QPointF delta = target - pointer;
        if (!response || delta.manhattanLength() < .001) {
            pointer = target;
            motion->stop();
        } else
            pointer += delta * (1 - std::exp(-dt / response));
        q->updateProjection();
    }
    void embed(SpatialItem* item)
    {
        auto* widget = item->d->widget.data();
        if (!widget)
            return;
        flatLayout->removeWidget(widget);
        widget->setParent(nullptr);
        item->d->proxy = new SpatialSurfaceProxy;
        scene->addItem(item->d->proxy);
        item->d->proxy->setWidget(widget);
        item->d->proxy->moved = [this] { q->updateProjection(); };
        item->d->proxy->setPos(0, 0);
        item->d->proxy->setCacheMode(cache ? QGraphicsItem::ItemCoordinateCache : QGraphicsItem::NoCache);
        widget->setVisible(item->d->visible);
        syncContentTheme(item);
    }
    void unembed(SpatialItem* item)
    {
        if (!item->d->proxy)
            return;
        // Remove proxy from scene before detaching its widget to avoid Qt 6 accessing stale mouse state.
        scene->removeItem(item->d->proxy);
        item->d->proxy->setWidget(nullptr);
        delete item->d->proxy.data();
        item->d->proxy = nullptr;
    }
    void status(Backend active, const QString& why = {}, const QString& name = {})
    {
        if (active == backend && why == reason && name == renderer)
            return;
        backend = active;
        reason = why;
        renderer = name;
        emit q->rendererChanged();
    }
    void useRaster(const QString& why = {})
    {
        if (qobject_cast<QOpenGLWidget*>(canvas->viewport())) {
            auto* viewport = new QWidget;
            canvas->setViewport(viewport);
            viewport->setMouseTracking(true);
            q->updateProjection();
        }
        status(Backend::Raster, why);
    }
    void checkBackend()
    {
        if (destroying || ancestorDestroying)
            return;
        auto* gl = qobject_cast<QOpenGLWidget*>(canvas->viewport());
        auto* window = q->window()->windowHandle();
        if (!gl || !canvas->isVisible() || !window || !window->isExposed())
            return;
        if (!gl->isValid()) {
            gpuFailure = q->tr("OpenGL initialization failed.");
            useRaster(gpuFailure);
            return;
        }
        if (backend == Backend::OpenGL && checkedContext == gl->context())
            return;
        gl->makeCurrent();
        QString name;
        if (QOpenGLContext::currentContext() == gl->context()) {
            const auto* value = gl->context()->functions()->glGetString(GL_RENDERER);
            if (value)
                name = QString::fromLatin1(reinterpret_cast<const char*>(value));
        }
        gl->doneCurrent();
        if (!SpatialRuntime::isHardwareRenderer(name)) {
            gpuFailure = q->tr("A hardware OpenGL renderer is unavailable.");
            useRaster(gpuFailure);
            return;
        }
        checkedContext = gl->context();
        status(Backend::OpenGL, {}, name);
    }
    void queueCheck()
    {
        if (checkQueued)
            return;
        checkQueued = true;
        QTimer::singleShot(0, q, [this] {
            checkQueued = false;
            checkBackend();
        });
    }
    void queueBackend(int delay = 0)
    {
        if (backendQueued)
            return;
        backendQueued = true;
        QTimer::singleShot(delay, q, [this] {
            backendQueued = false;
            applyBackend();
        });
    }
    void applyBackend()
    {
        if (destroying || ancestorDestroying)
            return;
        if (scene->mouseGrabberItem() && q->isVisible()) {
            queueBackend(16);
            return;
        }
        inspectEnvironment(QEvent::LayoutRequest);
        if (!spatial || environmentRaster || mode == RenderMode::Raster
            || (mode == RenderMode::Auto
                && qEnvironmentVariable("FLUENT_QT_SPATIAL_RENDERER")
                        .compare(QStringLiteral("raster"), Qt::CaseInsensitive)
                    == 0)) {
            useRaster();
            return;
        }
        if (!SpatialRuntime::supportsOpenGLDisplay()) {
            useRaster(q->tr("This Qt platform uses software drawing."));
            return;
        }
        // Defer adding QOpenGLWidget while hidden so creating its surface doesn't disturb an active window.
        if (!q->isVisible())
            return;
        if (!gpuFailure.isEmpty()) {
            useRaster(gpuFailure);
            return;
        }
        if (!qobject_cast<QOpenGLWidget*>(canvas->viewport())) {
            // Run preflight probe before inserting QOpenGLWidget alters the host backing store; reuse across temporary raster fallbacks.
            if (!preflightChecked) {
                gpuFailure = SpatialRuntime::preflightFailure();
                preflightChecked = true;
                if (!gpuFailure.isEmpty()) {
                    useRaster(gpuFailure);
                    return;
                }
            }
            const bool focused = canvas->hasFocus() || canvas->viewport()->hasFocus();
            auto* gl = new SpatialViewport(canvas);
            gl->setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
            QObject::connect(gl, &QOpenGLWidget::frameSwapped, q, [this] { queueCheck(); });
            canvas->setViewport(gl);
            // Process the one-shot full-exposure flag before QGraphicsView's viewport filter.
            gl->installEventFilter(gl);
            gl->setMouseTracking(true);
            if (focused)
                canvas->setFocus(Qt::OtherFocusReason);
            status(Backend::Raster);
            q->updateProjection();
        }
        queueCheck();
    }
};

SpatialItem::SpatialItem(SpatialView* view) : QObject(view), d(new Private)
{
    d->owner = view;
}
SpatialItem::~SpatialItem()
{
    if (d->owner)
        d->owner->detachItem(this, true);
}
QWidget* SpatialItem::widget() const
{
    return d->widget;
}
QVector3D SpatialItem::position() const
{
    return d->position;
}
void SpatialItem::setPosition(const QVector3D& value)
{
    if (!finite(value) || value == d->position)
        return;
    d->position = value;
    if (d->owner)
        d->owner->updateProjection();
    emit positionChanged(value);
}
QVector3D SpatialItem::rotation() const
{
    return d->rotation;
}
void SpatialItem::setRotation(const QVector3D& value)
{
    if (!finite(value))
        return;
    const QVector3D angle(std::remainder(value.x(), 360.f), std::remainder(value.y(), 360.f),
        std::remainder(value.z(), 360.f));
    if (angle == d->rotation)
        return;
    d->rotation = angle;
    if (d->owner)
        d->owner->updateProjection();
    emit rotationChanged(angle);
}
qreal SpatialItem::scale() const
{
    return d->scale;
}
void SpatialItem::setScale(qreal value)
{
    if (!std::isfinite(value))
        return;
    value = std::clamp<qreal>(value, .05, 8);
    if (value == d->scale)
        return;
    d->scale = value;
    if (d->owner)
        d->owner->updateProjection();
    emit scaleChanged(value);
}
qreal SpatialItem::surfaceIntensity() const
{
    return d->surfaceIntensity;
}
void SpatialItem::setSurfaceIntensity(qreal value)
{
    if (!std::isfinite(value))
        return;
    value = std::clamp<qreal>(value, 0, 1);
    if (value == d->surfaceIntensity)
        return;
    d->surfaceIntensity = value;
    if (d->owner)
        d->owner->updateProjection();
    emit surfaceIntensityChanged(value);
}
qreal SpatialItem::hoverLift() const
{
    return d->hoverLift;
}
void SpatialItem::setHoverLift(qreal value)
{
    if (!std::isfinite(value))
        return;
    value = std::clamp<qreal>(value, 0, 16);
    if (value == d->hoverLift)
        return;
    d->hoverLift = value;
    if (d->owner)
        d->owner->updateProjection();
    emit hoverLiftChanged(value);
}
QPointF SpatialItem::pivot() const
{
    return d->pivot;
}
void SpatialItem::setPivot(const QPointF& value)
{
    if (!finite(value))
        return;
    const QPointF point(std::clamp(value.x(), 0.0, 1.0), std::clamp(value.y(), 0.0, 1.0));
    if (point == d->pivot)
        return;
    d->pivot = point;
    if (d->owner)
        d->owner->updateProjection();
    emit pivotChanged(point);
}
QPolygonF SpatialItem::projectedPolygon() const
{
    if (!d->owner || !d->widget || !d->visible)
        return {};
    auto* owner = d->owner;
    if (!owner->isSpatialEnabled()) {
        return QPolygonF(QRectF(d->widget->mapTo(owner, QPoint()), d->widget->size()));
    }
    if (!d->proxy || !d->proxy->isVisible())
        return {};
    const auto scenePolygon = d->proxy->mapToScene(d->proxy->boundingRect());
    QPolygonF result(owner->d->canvas->mapFromScene(scenePolygon));
    result.translate(owner->d->canvas->viewport()->mapTo(owner, QPoint()));
    return result;
}
bool SpatialItem::isVisible() const
{
    return d->visible;
}
void SpatialItem::setVisible(bool visible)
{
    if (visible == d->visible)
        return;
    d->visible = visible;
    if (d->owner) {
        if (d->owner->isSpatialEnabled())
            d->owner->updateProjection();
        else if (d->widget)
            d->widget->setVisible(visible);
    }
    emit visibleChanged(visible);
}

SpatialView::SpatialView(QWidget* parent) : QWidget(parent), d(new Private)
{
    d->q = this;
    d->spatial = d->requestedSpatial = d->allowsSpatial();
    d->stack = new QStackedLayout(this);
    d->stack->setContentsMargins(0, 0, 0, 0);
    d->canvas = new SpatialCanvas(this);
    d->canvas->setObjectName(QStringLiteral("spatialCanvas"));
    d->scene = new QGraphicsScene(d->canvas);
    d->canvas->setScene(d->scene);
    d->flat = new scrolling::ScrollView(this);
    d->flat->setFrameShape(QFrame::NoFrame);
    d->flatContent = new layout::Card;
    d->flatContent->setAppearance(layout::Card::Canvas);
    d->flatContent->setBorderVisible(false);
    d->flatLayout = new QVBoxLayout(d->flatContent);
    d->flatLayout->setContentsMargins(24, 24, 24, 24);
    d->flatLayout->setSpacing(20);
    d->flatLayout->addStretch();
    d->flat->setWidgetResizable(true);
    d->flat->setWidget(d->flatContent);
    d->stack->addWidget(d->canvas);
    d->stack->addWidget(d->flat);
    d->stack->setCurrentWidget(d->spatial ? static_cast<QWidget*>(d->canvas) : d->flat);
    setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(d->spatial ? static_cast<QWidget*>(d->canvas) : d->flat);
    d->motion = new QTimer(this);
    d->motion->setObjectName(QStringLiteral("spatialMotionTimer"));
    d->motion->setTimerType(Qt::PreciseTimer);
    d->motion->setInterval(17);
    connect(d->motion, &QTimer::timeout, this, [this] { d->advance(); });
    d->modeChange = new QTimer(this);
    d->modeChange->setObjectName(QStringLiteral("spatialModeChangeTimer"));
    d->modeChange->setSingleShot(true);
    connect(d->modeChange, &QTimer::timeout, this, [this] { setSpatialEnabled(d->requestedSpatial); });
    d->canvas->pointer = [this](QPointF p) { d->pointAt(p); };
    d->canvas->resized = [this] {
        updateProjection();
        d->queueCheck();
    };
    d->canvas->nativeRequested = [this] {
        setSpatialEnabled(false);
        for (auto* item : d->items) {
            if (!item->widget())
                continue;
            auto children = item->widget()->findChildren<QWidget*>();
            children.prepend(item->widget());
            for (auto* child : children)
                if (child->isEnabled() && child->focusPolicy() != Qt::NoFocus && child->isVisible()) {
                    child->setFocus(Qt::TabFocusReason);
                    return;
                }
        }
    };
    connect(&MotionPolicy::instance(), &MotionPolicy::modeChanged, this, [this] {
        if (!d->allowsSpatial())
            setSpatialEnabled(false);
    });
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this] { d->settle(); });
    onThemeUpdated();
    d->applyBackend();
    d->inspectEnvironment(QEvent::ParentChange);
}
SpatialView::~SpatialView()
{
    d->destroying = true;
    for (const auto& connection : d->ancestorDestructionConnections)
        disconnect(connection);
    d->motion->stop();
    d->modeChange->stop();
    d->canvas->pointer = {};
    d->canvas->resized = {};
    d->canvas->nativeRequested = {};
    while (!d->items.isEmpty())
        delete d->items.last();
}
bool SpatialView::isHostTearingDown() const
{
    return d->destroying || d->ancestorDestroying;
}
SpatialItem* SpatialView::addWidget(QWidget* widget, WidgetOwnership ownership)
{
    if (!widget)
        return nullptr;
    for (auto* item : d->items)
        if (item->widget() == widget)
            return item;
    if (widget == this || widget->isAncestorOf(this) || isAncestorOf(widget))
        return nullptr;
    if (widget->graphicsProxyWidget() || !supported(widget)
        || (ownership != WidgetOwnership::Borrowed && ownership != WidgetOwnership::Reparented
            && ownership != WidgetOwnership::Owned))
        return nullptr;
    auto* item = new SpatialItem(this);
    item->d->widget = widget;
    item->d->originalParent = widget->parentWidget();
    item->d->ownership = ownership;
    item->d->inheritsHostTheme = !widget->property(kThemeOverride).isValid();
    if (!widget->testAttribute(Qt::WA_Resized) && widget->sizeHint().isValid())
        widget->resize(widget->sizeHint());
    d->items.append(item);
    widget->installEventFilter(this);
    item->d->destroyedConnection = connect(widget, &QObject::destroyed, this, [this, item](QObject* object) {
        if (!d->destroying && d->items.removeOne(item)) {
            disconnect(item->d->destroyedConnection);
            object->removeEventFilter(this);
            item->d->owner = nullptr;
            item->d->widget.clear();
            // Focus chain is broken during widget destruction; let Qt unbind before emitting itemCountChanged().
            if (auto* proxy = item->d->proxy.data())
                proxy->moved = {};
            item->deleteLater();
            if (!d->countNotificationQueued) {
                d->countNotificationQueued = true;
                QTimer::singleShot(0, this, [this] {
                    if (!d->countNotificationQueued)
                        return;
                    d->countNotificationQueued = false;
                    emit itemCountChanged(itemCount());
                });
            }
        }
    });
    if (d->spatial)
        d->embed(item);
    else {
        d->flatLayout->insertWidget(d->flatLayout->count() - 1, widget, 0, Qt::AlignHCenter);
        widget->show();
        d->syncContentTheme(item);
    }
    updateProjection();
    QPointer<SpatialItem> itemGuard(item);
    d->countNotificationQueued = false;
    emit itemCountChanged(itemCount());
    return itemGuard.data();
}
QWidget* SpatialView::detachItem(SpatialItem* item, bool applyOwnership)
{
    if (!item || item->d->owner != this || !d->items.removeOne(item))
        return nullptr;
    auto* widget = item->d->widget.data();
    item->d->owner = nullptr;
    // Unparent item early so an itemCountChanged() handler that deletes the view won't double-delete it.
    item->setParent(nullptr);
    if (widget) {
        widget->removeEventFilter(this);
        disconnect(item->d->destroyedConnection);
    }
    d->unembed(item);
    if (widget) {
        d->flatLayout->removeWidget(widget);
        widget->hide();
        widget->setParent(nullptr);
        if (applyOwnership && item->d->ownership == WidgetOwnership::Owned) {
            delete widget;
            widget = nullptr;
        } else if (applyOwnership && item->d->ownership == WidgetOwnership::Reparented) {
            if (d->parentIsTearingDown(item->d->originalParent)) {
                // Original parent is also tearing down; delete directly instead of attaching to a broken focus chain.
                delete widget;
                widget = nullptr;
            } else {
                widget->setParent(item->d->originalParent);
            }
        }
        if (widget) {
            if (item->d->inheritsHostTheme)
                widget->setProperty(kThemeOverride, QVariant());
            refreshContentTheme(widget);
        }
    }
    if (!d->destroying) {
        d->countNotificationQueued = false;
        emit itemCountChanged(itemCount());
    }
    return widget;
}
QWidget* SpatialView::takeWidget(SpatialItem* item)
{
    if (!item || item->d->owner != this)
        return nullptr;
    QPointer<SpatialItem> itemGuard(item);
    QPointer<QWidget> widget(item->widget());
    detachItem(item, false);
    delete itemGuard.data();
    return widget.data();
}
void SpatialView::releaseItem(SpatialItem* item)
{
    if (item && item->d->owner == this)
        delete item;
}
QList<SpatialItem*> SpatialView::items() const
{
    return d->items;
}
int SpatialView::itemCount() const
{
    return d->items.size();
}
bool SpatialView::isSpatialEnabled() const
{
    return d->spatial;
}
void SpatialView::setSpatialEnabled(bool enabled)
{
    if (isHostTearingDown())
        return;
    enabled = enabled && d->allowsSpatial();
    d->requestedSpatial = enabled;
    d->modeChange->stop();
    if (enabled == d->spatial)
        return;
    if (d->scene->mouseGrabberItem() && isVisible()) {
        // Coalesce pending mode switches while a mouse grab is active; hidden views won't get a release event.
        d->modeChange->start(16);
        return;
    }
    d->motion->stop();
    d->pointer = d->target = {};
    d->spatial = enabled;
    for (auto* item : d->items) {
        if (enabled)
            d->embed(item);
        else {
            d->unembed(item);
            if (item->widget()) {
                d->flatLayout->insertWidget(d->flatLayout->count() - 1, item->widget(), 0, Qt::AlignHCenter);
                item->widget()->setVisible(item->d->visible);
            }
        }
    }
    d->stack->setCurrentWidget(enabled ? static_cast<QWidget*>(d->canvas) : d->flat);
    setFocusProxy(enabled ? static_cast<QWidget*>(d->canvas) : d->flat);
    d->queueBackend();
    updateProjection();
    emit spatialEnabledChanged(enabled);
}
qreal SpatialView::cameraDistance() const
{
    return d->camera;
}
void SpatialView::setCameraDistance(qreal value)
{
    if (!std::isfinite(value))
        return;
    value = std::clamp<qreal>(value, 100, 10000);
    if (value == d->camera)
        return;
    d->camera = value;
    updateProjection();
    emit cameraDistanceChanged(value);
}
qreal SpatialView::zoom() const
{
    return d->zoom;
}
void SpatialView::setZoom(qreal value)
{
    if (!std::isfinite(value))
        return;
    value = std::clamp<qreal>(value, .1, 4);
    if (value == d->zoom)
        return;
    d->zoom = value;
    updateProjection();
    emit zoomChanged(value);
}
bool SpatialView::isPointerTrackingEnabled() const
{
    return d->tracking;
}
void SpatialView::setPointerTrackingEnabled(bool enabled)
{
    if (enabled == d->tracking)
        return;
    d->tracking = enabled;
    if (!enabled)
        d->settle();
    emit pointerTrackingEnabledChanged(enabled);
}
QPointF SpatialView::maximumTilt() const
{
    return d->tilt;
}
void SpatialView::setMaximumTilt(const QPointF& value)
{
    if (!finite(value))
        return;
    const QPointF tilt(std::clamp(value.x(), 0.0, 45.0), std::clamp(value.y(), 0.0, 45.0));
    if (tilt == d->tilt)
        return;
    d->tilt = tilt;
    updateProjection();
    emit maximumTiltChanged(tilt);
}
int SpatialView::responseTime() const
{
    return d->response;
}
void SpatialView::setResponseTime(int value)
{
    value = std::clamp(value, 0, 1000);
    if (value == d->response)
        return;
    d->response = value;
    emit responseTimeChanged(value);
}
int SpatialView::maximumFrameRate() const
{
    return d->frameRate;
}
void SpatialView::setMaximumFrameRate(int value)
{
    value = std::clamp(value, 15, 120);
    if (value == d->frameRate)
        return;
    d->frameRate = value;
    d->motion->setInterval((1000 + value - 1) / value);
    emit maximumFrameRateChanged(value);
}
bool SpatialView::isCacheEnabled() const
{
    return d->cache;
}
void SpatialView::setCacheEnabled(bool enabled)
{
    if (enabled == d->cache)
        return;
    d->cache = enabled;
    for (auto* item : d->items)
        if (item->d->proxy)
            item->d->proxy->setCacheMode(enabled ? QGraphicsItem::ItemCoordinateCache : QGraphicsItem::NoCache);
    emit cacheEnabledChanged(enabled);
}
SpatialView::RenderMode SpatialView::renderMode() const
{
    return d->mode;
}
void SpatialView::setRenderMode(RenderMode mode)
{
    if (isHostTearingDown() || (mode != RenderMode::Auto && mode != RenderMode::Raster && mode != RenderMode::OpenGL)
        || mode == d->mode)
        return;
    d->mode = mode;
    d->gpuFailure.clear();
    d->preflightChecked = false;
    d->queueBackend();
    emit renderModeChanged(mode);
}
SpatialView::Backend SpatialView::activeBackend() const
{
    return d->backend;
}
QString SpatialView::rendererName() const
{
    return d->renderer;
}
QString SpatialView::fallbackReason() const
{
    return d->reason;
}
void SpatialView::updateProjection()
{
    if (isHostTearingDown() || d->projecting || !d->canvas)
        return;
    d->projecting = true;
    const QSize size = d->canvas->viewport()->size();
    d->scene->setSceneRect(-size.width() / 2.0, -size.height() / 2.0, size.width(), size.height());
    QMatrix4x4 observer;
    observer.rotate(float(d->pointer.x() * d->tilt.y()), 0, 1, 0);
    observer.rotate(float(-d->pointer.y() * d->tilt.x()), 1, 0, 0);
    for (auto* item : d->items) {
        if (!item->d->proxy || !item->widget())
            continue;
        const QRectF rect(QPointF(), item->widget()->size());
        const auto* theme = dynamic_cast<const FluentElement*>(item->widget());
        item->d->proxy->configure(item->d->surfaceIntensity, item->d->hoverLift, theme ? *theme : *this);
        const QPointF pivot(rect.width() * item->d->pivot.x(), rect.height() * item->d->pivot.y());
        QMatrix4x4 model = observer;
        model.translate(item->d->position);
        model.translate(0, -float(item->d->proxy->liftOffset()), 0);
        model.rotate(item->d->rotation.z(), 0, 0, 1);
        model.rotate(item->d->rotation.y(), 0, 1, 0);
        model.rotate(item->d->rotation.x(), 1, 0, 0);
        model.scale(float(item->d->scale));
        QPolygonF source;
        source << rect.topLeft() << rect.topRight() << rect.bottomRight() << rect.bottomLeft();
        QPolygonF projected;
        bool valid = !rect.isEmpty();
        for (const QPointF& corner : source) {
            const QVector3D p = model.map(QVector3D(float(corner.x() - pivot.x()), float(corner.y() - pivot.y()), 0));
            const qreal distance = d->camera - p.z();
            if (!finite(p) || distance <= 1) {
                valid = false;
                break;
            }
            const qreal factor = d->zoom * d->camera / distance;
            projected.append(QPointF(p.x() * factor, p.y() * factor));
        }
        QTransform transform;
        valid = valid && QTransform::quadToQuad(source, projected, transform) && transform.isInvertible();
        item->d->proxy->setVisible(valid && item->d->visible);
        if (valid) {
            item->d->proxy->setTransform(transform);
            item->d->proxy->setZValue(
                model.map(QVector3D(float(rect.center().x() - pivot.x()), float(rect.center().y() - pivot.y()), 0))
                    .z());
        }
    }
    d->projecting = false;
}
bool SpatialView::eventFilter(QObject* watched, QEvent* event)
{
    d->inspectEnvironment(event->type());
    if (!d->destroying && !d->updatingTheme && event->type() == QEvent::DynamicPropertyChange
        && static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName() == kThemeOverride) {
        for (auto* item : d->items)
            if (item->widget() == watched) {
                item->d->inheritsHostTheme = !watched->property(kThemeOverride).isValid();
                d->syncContentTheme(item);
                break;
            }
    }
    if (!d->destroying && !d->projecting && event->type() == QEvent::Resize && !d->geometryQueued) {
        d->geometryQueued = true;
        QTimer::singleShot(0, this, [this] {
            d->geometryQueued = false;
            updateProjection();
        });
    }
    return QWidget::eventFilter(watched, event);
}
bool SpatialView::event(QEvent* event)
{
    const QPointer<SpatialView> guard(this);
    const bool handled = QWidget::event(event);
    if (!guard)
        return handled;
    if (d && d->canvas)
        d->inspectEnvironment(event->type());
    return handled;
}
void SpatialView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // Defer backend switch so replacing the viewport doesn't mutate QWidget's child list mid-show.
    d->queueBackend();
}
void SpatialView::hideEvent(QHideEvent* event)
{
    d->settle();
    if (auto* grabber = d->scene->mouseGrabberItem())
        grabber->ungrabMouse();
    // Apply pending mode change after hide traversal finishes rather than waiting for an unreachable release.
    if (d->requestedSpatial != d->spatial)
        d->modeChange->start(0);
    else
        d->modeChange->stop();
    d->queueBackend();
    QWidget::hideEvent(event);
}
void SpatialView::onThemeUpdated()
{
    if (!d || !d->canvas)
        return;
    auto palette = this->palette();
    palette.setColor(QPalette::Window, themeColorsRef().bgCanvas);
    palette.setColor(QPalette::Base, themeColorsRef().bgCanvas);
    setPalette(palette);
    setAutoFillBackground(true);
    // Force Fluent canvas color so styled-host palette repolishing doesn't overwrite it.
    d->flatContent->onThemeUpdated();
    d->flat->viewport()->setPalette(palette);
    d->canvas->setBackgroundBrush(themeColorsRef().bgCanvas);
    for (auto* item : d->items)
        d->syncContentTheme(item);
    updateProjection();
    if (!d->allowsSpatial())
        setSpatialEnabled(false);
}
QSize SpatialView::sizeHint() const
{
    return {720, 480};
}
} // namespace fluent::spatial
