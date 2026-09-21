#include "SpatialView.h"
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

protected:
    bool event(QEvent* event) override
    {
        observeExposureEvent(event->type());
        return QOpenGLWidget::event(event);
    }
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        observeExposureEvent(event->type());
        return QOpenGLWidget::eventFilter(watched, event);
    }
    void paintGL() override
    {
        // Qt can repaint the FBO directly (for example during a widget capture),
        // bypassing QGraphicsView's viewport paint event. A plain QOpenGLWidget
        // clears the frame in that path and leaves the live preview blank.
        // zh_CN: Qt 可能直接重绘 FBO，绕过 QGraphicsView 的视口事件；空 paintGL 会清空预览。
        QPainter painter(this);
        m_view->render(&painter);
    }

private:
    void observeExposureEvent(QEvent::Type type)
    {
        if (type == QEvent::ParentChange)
            m_rebuildAncestors = true;
        if (type == QEvent::Hide)
            m_exposed = {};
        if (type != QEvent::Move && type != QEvent::Resize && type != QEvent::Show &&
            type != QEvent::Hide && type != QEvent::ParentChange)
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
                for (auto* ancestor = parentWidget(); ancestor;
                     ancestor = ancestor->parentWidget()) {
                    m_ancestors.append(ancestor);
                    ancestor->installEventFilter(this);
                }
                m_rebuildAncestors = false;
            }
            QRect exposed = isVisible() ? rect() : QRect();
            for (const auto& ancestor : m_ancestors)
                if (ancestor)
                    exposed &= QRect(mapFrom(ancestor, QPoint()), ancestor->size());
            // Ancestor scrolling changes clipping without moving this widget locally.
            // QGraphicsView may leave the newly exposed part of its GL texture unpainted.
            // zh_CN: 祖先滚动只改变裁剪，不触发本控件移动；新露出的 GL 区域需要主动重绘。
            const bool revealed = !QRegion(exposed).subtracted(m_exposed).isEmpty();
            m_exposed = exposed;
            if (revealed)
                update();
        });
    }
    QGraphicsView* m_view;
    QList<QPointer<QWidget>> m_ancestors;
    QRect m_exposed;
    bool m_rebuildAncestors = true;
    bool m_exposurePending = false;
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
    if (widget->testAttribute(Qt::WA_NativeWindow) || widget->testAttribute(Qt::WA_PaintOnScreen) ||
        qobject_cast<QOpenGLWidget*>(widget))
        return false;
    for (auto* child : widget->findChildren<QWidget*>())
        if (child->testAttribute(Qt::WA_NativeWindow) ||
            child->testAttribute(Qt::WA_PaintOnScreen) || qobject_cast<QOpenGLWidget*>(child))
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
    QElapsedTimer clock;
    QPointF pointer, target, tilt{10, 16};
    qreal camera = 1000, zoom = 1;
    int response = 140, frameRate = 60;
    bool spatial = true, tracking = true, cache = true;
    bool destroying = false, projecting = false, geometryQueued = false, updatingTheme = false;
    bool backendQueued = false, checkQueued = false, gpuFailed = false;
    RenderMode mode = RenderMode::Auto;
    Backend backend = Backend::Raster;
    QString renderer, reason;
    QPointer<QOpenGLContext> checkedContext;
    QList<QPointer<QWidget>> ancestors;
    bool ancestorsDirty = true, environmentRaster = true;

    void inspectEnvironment(QEvent::Type type)
    {
        if (destroying)
            return;
        if (type == QEvent::ParentChange)
            ancestorsDirty = true;
        if (type != QEvent::Show && type != QEvent::Hide && type != QEvent::Move &&
            type != QEvent::Resize && type != QEvent::ParentChange &&
            type != QEvent::UpdateRequest && type != QEvent::LayoutRequest)
            return;
        if (ancestorsDirty) {
            for (const auto& ancestor : ancestors)
                if (ancestor)
                    ancestor->removeEventFilter(q);
            ancestors.clear();
            for (auto* ancestor = q->parentWidget(); ancestor;
                 ancestor = ancestor->parentWidget()) {
                ancestors.append(ancestor);
                ancestor->installEventFilter(q);
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

    void syncContentTheme(SpatialItem* item)
    {
        auto* widget = item->widget();
        if (!widget)
            return;
        // Embedded roots have no QWidget parent. Bridge the host theme without
        // replacing an explicit content override; remove this bridge on release.
        // zh_CN: 代理根控件没有 QWidget 父对象；补充主题继承，保留显式覆盖，释放时移除。
        if (item->d->inheritsHostTheme) {
            updatingTheme = true;
            widget->setProperty(kThemeOverride, int(q->effectiveTheme()));
            updatingTheme = false;
        }
        refreshContentTheme(widget);
    }

    bool allowsSpatial() const
    {
        return q->effectiveTheme() != FluentElement::HighContrast &&
               MotionPolicy::instance().shouldAnimate(true, MotionPolicy::Kind::Continuous);
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
        if (!spatial || !tracking || !allowsSpatial() || !q->isVisible() ||
            !q->window()->isActiveWindow() || scene->mouseGrabberItem())
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
        if (!spatial || !tracking || !q->isVisible() || !q->window()->isActiveWindow() ||
            !allowsSpatial()) {
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
        item->d->proxy->setCacheMode(cache ? QGraphicsItem::ItemCoordinateCache
                                           : QGraphicsItem::NoCache);
        widget->setVisible(item->d->visible);
        syncContentTheme(item);
    }
    void unembed(SpatialItem* item)
    {
        if (!item->d->proxy)
            return;
        // Detach before clearing a proxy cursor. Qt 6 otherwise consults its
        // last mouse event even when the view has only received keyboard input.
        // zh_CN: 先分离再清理代理光标，避免 Qt 6 在纯键盘操作时访问未初始化的鼠标事件。
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
        auto* gl = qobject_cast<QOpenGLWidget*>(canvas->viewport());
        auto* window = q->window()->windowHandle();
        if (!gl || !canvas->isVisible() || !window || !window->isExposed())
            return;
        if (!gl->isValid()) {
            gpuFailed = true;
            useRaster(q->tr("OpenGL initialization failed."));
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
        const auto lower = name.toLower();
        if (name.isEmpty() || lower.contains(QStringLiteral("llvmpipe")) ||
            lower.contains(QStringLiteral("softpipe")) ||
            lower.contains(QStringLiteral("swiftshader")) ||
            lower.contains(QStringLiteral("software")) ||
            lower.contains(QStringLiteral("basic render driver")) ||
            lower.contains(QStringLiteral("warp")) ||
            lower.contains(QStringLiteral("gdi generic"))) {
            gpuFailed = true;
            useRaster(q->tr("A hardware OpenGL renderer is unavailable."));
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
        if (scene->mouseGrabberItem() && q->isVisible()) {
            queueBackend(16);
            return;
        }
        const auto platform = QGuiApplication::platformName();
        inspectEnvironment(QEvent::LayoutRequest);
        if (!spatial || environmentRaster || mode == RenderMode::Raster ||
            (mode == RenderMode::Auto &&
             qEnvironmentVariable("FLUENT_QT_SPATIAL_RENDERER")
                     .compare(QStringLiteral("raster"), Qt::CaseInsensitive) == 0)) {
            useRaster();
            return;
        }
        if (platform == QStringLiteral("offscreen") || platform == QStringLiteral("minimal") ||
            platform == QStringLiteral("vnc")) {
            useRaster(q->tr("This Qt platform uses software drawing."));
            return;
        }
        // Adding the first OpenGL viewport can recreate its top-level native surface.
        // Hidden pages must not disturb a visible host while they are being prepared.
        // zh_CN: 首个 OpenGL 视口可能重建顶层原生表面；隐藏页面预热时不能影响已显示的宿主。
        if (!q->isVisible())
            return;
        if (gpuFailed)
            return;
        if (!qobject_cast<QOpenGLWidget*>(canvas->viewport())) {
            const bool focused = canvas->hasFocus() || canvas->viewport()->hasFocus();
            auto* gl = new SpatialViewport(canvas);
            gl->setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
            QObject::connect(gl, &QOpenGLWidget::frameSwapped, q, [this] { queueCheck(); });
            canvas->setViewport(gl);
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
    d->spatial = d->allowsSpatial();
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
                if (child->isEnabled() && child->focusPolicy() != Qt::NoFocus &&
                    child->isVisible()) {
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
}
SpatialView::~SpatialView()
{
    d->destroying = true;
    d->motion->stop();
    d->canvas->pointer = {};
    d->canvas->resized = {};
    d->canvas->nativeRequested = {};
    while (!d->items.isEmpty())
        delete d->items.last();
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
    if (widget->graphicsProxyWidget() || !supported(widget) ||
        (ownership != WidgetOwnership::Borrowed && ownership != WidgetOwnership::Reparented &&
         ownership != WidgetOwnership::Owned))
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
    item->d->destroyedConnection = connect(widget, &QObject::destroyed, this, [this, item] {
        if (!d->destroying && d->items.removeOne(item)) {
            item->d->owner = nullptr;
            item->deleteLater();
            emit itemCountChanged(itemCount());
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
    emit itemCountChanged(itemCount());
    return item;
}
QWidget* SpatialView::detachItem(SpatialItem* item, bool applyOwnership)
{
    if (!item || item->d->owner != this || !d->items.removeOne(item))
        return nullptr;
    auto* widget = item->d->widget.data();
    item->d->owner = nullptr;
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
        } else if (applyOwnership && item->d->ownership == WidgetOwnership::Reparented)
            widget->setParent(item->d->originalParent);
        if (widget) {
            if (item->d->inheritsHostTheme)
                widget->setProperty(kThemeOverride, QVariant());
            refreshContentTheme(widget);
        }
    }
    if (!d->destroying)
        emit itemCountChanged(itemCount());
    return widget;
}
QWidget* SpatialView::takeWidget(SpatialItem* item)
{
    if (!item || item->d->owner != this)
        return nullptr;
    auto* widget = detachItem(item, false);
    delete item;
    return widget;
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
    enabled = enabled && d->allowsSpatial();
    if (enabled == d->spatial)
        return;
    if (d->scene->mouseGrabberItem()) {
        QTimer::singleShot(16, this, [this, enabled] { setSpatialEnabled(enabled); });
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
                d->flatLayout->insertWidget(d->flatLayout->count() - 1, item->widget(), 0,
                                            Qt::AlignHCenter);
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
            item->d->proxy->setCacheMode(enabled ? QGraphicsItem::ItemCoordinateCache
                                                 : QGraphicsItem::NoCache);
    emit cacheEnabledChanged(enabled);
}
SpatialView::RenderMode SpatialView::renderMode() const
{
    return d->mode;
}
void SpatialView::setRenderMode(RenderMode mode)
{
    if ((mode != RenderMode::Auto && mode != RenderMode::Raster && mode != RenderMode::OpenGL) ||
        mode == d->mode)
        return;
    d->mode = mode;
    d->gpuFailed = false;
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
    if (d->destroying || d->projecting || !d->canvas)
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
        item->d->proxy->configure(item->d->surfaceIntensity, item->d->hoverLift,
                                  theme ? *theme : *this);
        const QPointF pivot(rect.width() * item->d->pivot.x(), rect.height() * item->d->pivot.y());
        QMatrix4x4 model = observer;
        model.translate(item->d->position);
        model.translate(0, -float(item->d->proxy->liftOffset()), 0);
        model.rotate(item->d->rotation.z(), 0, 0, 1);
        model.rotate(item->d->rotation.y(), 0, 1, 0);
        model.rotate(item->d->rotation.x(), 1, 0, 0);
        model.scale(float(item->d->scale));
        const QPolygonF source{rect.topLeft(), rect.topRight(), rect.bottomRight(),
                               rect.bottomLeft()};
        QPolygonF projected;
        bool valid = !rect.isEmpty();
        for (const QPointF& corner : source) {
            const QVector3D p = model.map(
                QVector3D(float(corner.x() - pivot.x()), float(corner.y() - pivot.y()), 0));
            const qreal distance = d->camera - p.z();
            if (!finite(p) || distance <= 1) {
                valid = false;
                break;
            }
            const qreal factor = d->zoom * d->camera / distance;
            projected.append(QPointF(p.x() * factor, p.y() * factor));
        }
        QTransform transform;
        valid = valid && QTransform::quadToQuad(source, projected, transform) &&
                transform.isInvertible();
        item->d->proxy->setVisible(valid && item->d->visible);
        if (valid) {
            item->d->proxy->setTransform(transform);
            item->d->proxy->setZValue(model
                                          .map(QVector3D(float(rect.center().x() - pivot.x()),
                                                         float(rect.center().y() - pivot.y()), 0))
                                          .z());
        }
    }
    d->projecting = false;
}
bool SpatialView::eventFilter(QObject* watched, QEvent* event)
{
    d->inspectEnvironment(event->type());
    if (!d->destroying && !d->updatingTheme && event->type() == QEvent::DynamicPropertyChange &&
        static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName() == kThemeOverride) {
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
    const bool handled = QWidget::event(event);
    if (d && d->canvas)
        d->inspectEnvironment(event->type());
    return handled;
}
void SpatialView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // Replacing children during QWidget's show traversal can invalidate that traversal.
    // zh_CN: 等待 QWidget 的显示遍历结束，避免替换视口使正在遍历的子控件失效。
    d->queueBackend();
}
void SpatialView::hideEvent(QHideEvent* event)
{
    d->settle();
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
    // Use the Fluent canvas surface inside styled hosts too; native palettes may be repolished.
    // zh_CN: 在带样式的宿主内仍使用 Fluent 画布，避免原生调色板被重新 polish 后覆盖。
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
