#include "compatibility/private/WidgetPlatformCompat_p.h"
#include "compatibility/private/quick/QuickCompat_p.h"
#include "compatibility/private/quick/QuickWidgetCompat_p.h"
#include "compatibility/private/PointerEventCompat_p.h"
#include "QmlView.h"

#include <QApplication>
#include <QHash>
#include <QFocusEvent>
#include <QInputMethodEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWidget>
#include <QShowEvent>
#include <QTimer>
#include <QResizeEvent>
#include <QSGRendererInterface>
#include <QScopedValueRollback>
#include <QRegularExpression>
#include <QThread>
#include <QVBoxLayout>
#include "QmlTheme.h"
#include "QmlTextureRenderer_p.h"

namespace fluent::qmlbridge {
namespace {

constexpr char kDirectCompositionRequiredProperty[] = "fluentqtDirectCompositionRequired";

bool onGuiThread(const QObject* object)
{
    const auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    return app && QThread::currentThread() == app->thread() && object->thread() == app->thread();
}
} // namespace

struct QmlView::Private {
    struct Binding {
        QPointer<QObject> object;
        QMetaObject::Connection destroyed;
    };
    QHash<QString, Binding> bindings;
    QPointer<QQuickWidget> quick;
    QPointer<QQuickWidget> pendingNativePresentation;
    QmlTheme* theme = nullptr;
    QUrl source;
    QStringList errors;
    QString sceneGraphError;
    Status status = Unloaded;
    bool active = false;
    bool busy = false;
    bool textureEnabled = false;
    bool textureTransition = false;
    quint64 textureRevision = 0;
    QString textureError;
    std::shared_ptr<QmlTextureRenderer> textureRenderer;
};

QmlView::QmlView(QWidget* parent) : QWidget(parent), d(std::make_shared<Private>())
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    d->theme = new QmlTheme(this);
    setAccessibleName(QStringLiteral("QML content"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    ::compatibility::detail::enableEmbeddedSceneTouch(this);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}
QmlView::~QmlView()
{
    const auto state = d;
    state->busy = true;
    state->active = false;
    const auto renderer = std::move(state->textureRenderer);
    if (renderer)
        renderer->detach();
    state->quick = nullptr;
    const auto scenes = findChildren<QQuickWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* quick : scenes) {
        disconnect(quick, nullptr, this, nullptr);
        delete quick;
    }
    if (property(kDirectCompositionRequiredProperty).toBool())
        setProperty(kDirectCompositionRequiredProperty, false);
}
QUrl QmlView::source() const
{
    return d->source;
}
QmlView::Status QmlView::status() const
{
    return d->status;
}
bool QmlView::isActive() const
{
    return d->active;
}
QStringList QmlView::errors() const
{
    return d->errors;
}
QString QmlView::lastError() const
{
    return d->errors.join(QLatin1Char('\n'));
}
QObject* QmlView::rootObject() const
{
    return d->quick ? d->quick->rootObject() : nullptr;
}
QObject* QmlView::themeObject() const
{
    return d->theme;
}
QSize QmlView::sizeHint() const
{
    return QSize(420, 300);
}

bool QmlView::reportError(const QString& message, QString* error)
{
    if (error)
        *error = message;
    const auto state = d;
    const QPointer<QQuickWidget> scene(state->quick);
    if (state->errors == QStringList{message})
        return false;
    state->errors = QStringList{message};
    QPointer<QmlView> lifetime(this);
    emit errorsChanged();
    if (lifetime && state->quick == scene && state->errors == QStringList{message})
        emit errorOccurred(message);
    return false;
}

bool QmlView::setContextObject(const QString& name, QObject* object)
{
    if (!onGuiThread(this))
        return false;
    if (d->busy || d->quick)
        return reportError(QStringLiteral("Unload the QML page before changing context objects."));
    static const QRegularExpression identifier(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    if (!identifier.match(name).hasMatch() || name == QStringLiteral("fluentTheme"))
        return reportError(QStringLiteral("Context names must be QML identifiers other than fluentTheme."));
    if (object && object->thread() != thread())
        return reportError(QStringLiteral("Context objects must belong to the GUI thread."));
    auto previous = d->bindings.find(name);
    if (previous != d->bindings.end()) {
        if (previous->object == object)
            return true;
        disconnect(previous->destroyed);
        d->bindings.erase(previous);
    }
    if (object) {
        QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);
        const auto connection = connect(object, &QObject::destroyed, this, [this, name] {
            if (!d)
                return;
            d->bindings.remove(name);
            unload();
        });
        d->bindings.insert(name, {object, connection});
    }
    return true;
}

bool QmlView::load(const QUrl& url, QString* error)
{
    const QUrl requestedSource(url);
    if (error)
        error->clear();
    if (!onGuiThread(this)) {
        if (error)
            *error = QStringLiteral("QML loading requires QApplication's GUI thread.");
        return false;
    }
    if (!d->active)
        return reportError(QStringLiteral("Activate the QML bridge before loading a page."), error);
    if (d->busy)
        return reportError(QStringLiteral("A QML page transition is already in progress."), error);
    if (!requestedSource.isValid() || requestedSource.isEmpty() || requestedSource.isRelative())
        return reportError(QStringLiteral("QML source must be a valid absolute URL."), error);
    if (d->source == requestedSource && d->status == Ready)
        return true;

    const auto state = d;
    QPointer<QmlView> lifetime(this);
    unload();
    if (!lifetime || !state->active || state->quick || state->busy)
        return false;
    state->busy = true;
    // Let an embedding host suspend incompatible capture effects before Quick exists.
    // zh_CN: 创建 Quick 前让嵌入宿主同步停用不兼容的捕获特效。
    setProperty(kDirectCompositionRequiredProperty, !state->textureEnabled);
    if (!lifetime)
        return false;
    if (!state->active || (!state->textureEnabled && !property(kDirectCompositionRequiredProperty).toBool())) {
        state->busy = false;
        return false;
    }
    auto* quick = new ::compatibility::detail::QuickWidget(this);
    state->quick = quick;
    quick->hide();
    quick->setObjectName(QStringLiteral("qmlQuickWidget"));
    quick->setResizeMode(QQuickWidget::SizeRootObjectToView);
    // Let the QML scene paint its background so rounded corners reveal the host.
    // zh_CN: 由 QML 场景绘制背景，让圆角外的区域显示宿主背景。
    quick->setClearColor(Qt::transparent);
    quick->setFocusPolicy(Qt::StrongFocus);
    quick->rootContext()->setContextProperty(QStringLiteral("fluentTheme"), state->theme);
    for (auto binding = state->bindings.cbegin(); binding != state->bindings.cend(); ++binding)
        quick->rootContext()->setContextProperty(binding.key(), binding->object.data());
    layout()->addWidget(quick);
    if (!state->textureEnabled)
        setFocusProxy(quick);
    const QPointer<QQuickWidget> scene(quick);
    connect(
        quick, &QQuickWidget::statusChanged, this,
        [this, scene] {
            if (scene && d->quick == scene)
                updateStatus();
        },
        Qt::QueuedConnection);
    connect(
        quick, &QQuickWidget::sceneGraphError, this,
        [this, scene](QQuickWindow::SceneGraphError, const QString& message) {
            if (!scene || d->quick != scene)
                return;
            const auto state = d;
            const bool changed = state->status != Error;
            d->sceneGraphError = message;
            d->status = Error;
            QPointer<QmlView> lifetime(this);
            reportError(message);
            if (lifetime && state->quick == scene && state->status == Error && state->sceneGraphError == message
                && changed)
                emit statusChanged(Error);
        },
        Qt::QueuedConnection);
    state->source = requestedSource;
    quick->setSource(requestedSource);
    if (!lifetime)
        return false;
    state->busy = false;
    if (state->quick != scene)
        return false;
    // A load callback may change the request while loading is busy. Apply it
    // before publishing this scene or allowing the layout to expose Quick.
    // zh_CN: 加载回调可能改变模式请求，在发布场景或布局显示 Quick 前应用最终请求。
    if (state->textureEnabled) {
        scene->hide();
        setFocusProxy(nullptr);
        if (property(kDirectCompositionRequiredProperty).toBool())
            setProperty(kDirectCompositionRequiredProperty, false);
    } else {
        setProperty(kDirectCompositionRequiredProperty, true);
        if (!lifetime || state->quick != scene)
            return false;
        setFocusProxy(scene);
        scheduleNativePresentation();
    }
    if (!lifetime || state->quick != scene)
        return false;
    emit sourceChanged(requestedSource);
    if (!lifetime || state->quick != scene)
        return false;
    updateStatus();
    if (!lifetime)
        return false;
    if (error && state->status == Error)
        *error = lastError();
    return state->active && state->quick == scene && state->status != Error;
}

void QmlView::updateStatus()
{
    if (!d->quick)
        return;
    const QPointer<QQuickWidget> scene(d->quick);
    Status next = Unloaded;
    switch (d->quick->status()) {
    case QQuickWidget::Ready:
        next = Ready;
        break;
    case QQuickWidget::Loading:
        next = Loading;
        break;
    case QQuickWidget::Error:
        next = Error;
        break;
    case QQuickWidget::Null:
        break;
    }
    QStringList messages;
    for (const auto& error : d->quick->errors())
        messages.append(error.toString());
    if (!d->sceneGraphError.isEmpty()) {
        next = Error;
        messages.append(d->sceneGraphError);
    }
    const auto state = d;
    const bool errorsChangedValue = state->errors != messages;
    const bool statusChangedValue = state->status != next;
    state->errors = messages;
    state->status = next;
    QPointer<QmlView> lifetime(this);
    if (next == Ready && state->textureEnabled)
        ensureTextureRenderer();
    if (!lifetime || state->quick != scene)
        return;
    if (errorsChangedValue)
        emit errorsChanged();
    if (!lifetime || state->status != next || state->quick != scene || state->errors != messages)
        return;
    if (statusChangedValue)
        emit statusChanged(next);
    if (lifetime && state->quick == scene && state->status == Ready)
        scheduleNativePresentation();
    if (lifetime && state->quick == scene && state->status == Error && state->errors == messages && !messages.isEmpty()
        && errorsChangedValue)
        emit errorOccurred(messages.join(QLatin1Char('\n')));
}

void QmlView::scheduleNativePresentation()
{
    const auto state = d;
    const QPointer<QQuickWidget> scene(state->quick);
    if (!scene || state->textureEnabled || state->status != Ready || !isVisible()
        || state->pendingNativePresentation == scene)
        return;
    state->pendingNativePresentation = scene;
    // Let the host finish its layout before revealing this particular scene.
    // zh_CN: 等宿主完成布局后再显示当前场景，过期加载不能重新显示旧 Quick 控件。
    QTimer::singleShot(0, this, [this, state, scene] {
        if (state->pendingNativePresentation == scene)
            state->pendingNativePresentation = nullptr;
        if (!scene || state->quick != scene || !state->active || state->textureEnabled || state->status != Ready
            || !isVisible())
            return;
        QPointer<QmlView> guard(this);
        if (layout())
            layout()->activate();
        if (!guard || state->quick != scene || state->textureEnabled)
            return;
        scene->setGeometry(contentsRect());
        if (!guard || !scene || state->quick != scene || state->textureEnabled)
            return;
        scene->show();
    });
}

void QmlView::unload()
{
    if (!d || !onGuiThread(this))
        return;
    const auto state = d;
    const bool wasBusy = state->busy;
    const bool hadSource = !state->source.isEmpty();
    const bool hadErrors = !state->errors.isEmpty();
    const bool hadStatus = state->status != Unloaded;
    state->source = QUrl();
    state->errors.clear();
    state->sceneGraphError.clear();
    state->status = Unloaded;
    state->busy = true;
    auto* quick = state->quick.data();
    state->quick = nullptr;
    QPointer<QmlView> lifetime(this);
    setFocusProxy(nullptr);
    const auto renderer = std::move(state->textureRenderer);
    if (renderer)
        renderer->detach();
    state->textureError.clear();
    if (quick) {
        disconnect(quick, nullptr, this, nullptr);
        layout()->removeWidget(quick);
        quick->hide();
        // A model signal can stop the plugin while its engine is executing QML.
        // zh_CN: 模型信号可能在引擎执行 QML 时停用插件，延迟销毁避免破坏 JS 栈。
        quick->deleteLater();
    }
    if (!lifetime)
        return;
    if (property(kDirectCompositionRequiredProperty).toBool())
        setProperty(kDirectCompositionRequiredProperty, false);
    if (!lifetime)
        return;
    state->busy = wasBusy;
    if (hadSource)
        emit sourceChanged(QUrl());
    if (!lifetime || state->quick)
        return;
    if (hadErrors)
        emit errorsChanged();
    if (!lifetime || state->quick)
        return;
    if (hadStatus)
        emit statusChanged(Unloaded);
}

bool QmlView::isTextureRenderingEnabled() const
{
    return d->textureEnabled;
}

bool QmlView::setTextureRenderingEnabled(bool enabled)
{
    if (!onGuiThread(this) || d->textureTransition)
        return false;
    if (d->textureEnabled == enabled)
        return true;
    // Software Quick has no shareable GPU target; retain its direct-widget path.
    // zh_CN: 软件 Quick 没有可共享的 GPU 目标，保留直接控件路径。
    if (enabled) {
        if (!::compatibility::detail::quickUsesOpenGL())
            return false;
        if (d->quick && d->quick->quickWindow()->rendererInterface()->graphicsApi() != QSGRendererInterface::OpenGL)
            return false;
    }
    const auto state = d;
    state->textureEnabled = enabled;
    QScopedValueRollback<bool> transition(state->textureTransition, true);
    state->textureError.clear();
    const QPointer<QQuickWidget> scene(state->quick);
    QPointer<QmlView> lifetime(this);
    if (!state->busy) {
        if (enabled) {
            if (scene) {
                setFocusProxy(nullptr);
                scene->hide();
                if (scene->status() == QQuickWidget::Ready)
                    ensureTextureRenderer();
            }
            if (!lifetime)
                return false;
            if (property(kDirectCompositionRequiredProperty).toBool())
                setProperty(kDirectCompositionRequiredProperty, false);
        } else {
            const auto renderer = std::move(state->textureRenderer);
            if (renderer)
                renderer->detach();
            if (!lifetime)
                return false;
            if (scene && state->quick == scene) {
                setProperty(kDirectCompositionRequiredProperty, true);
                if (!lifetime || state->quick != scene)
                    return false;
                setFocusProxy(scene);
                scheduleNativePresentation();
            }
        }
    }
    if (!lifetime)
        return false;
    state->textureTransition = false;
    emit textureRenderingEnabledChanged(enabled);
    if (lifetime && enabled && state->textureEnabled)
        emit textureUpdateRequested();
    return lifetime && state->textureEnabled == enabled;
}

bool QmlView::ensureTextureRenderer()
{
    const auto state = d;
    if (!state->textureEnabled || !state->quick || state->quick->status() != QQuickWidget::Ready)
        return false;
    if (state->textureRenderer)
        return true;
    const QPointer<QQuickWidget> scene(state->quick);
    QPointer<QmlView> lifetime(this);
    scene->hide();
    setFocusProxy(nullptr);
    const auto renderer = std::make_shared<QmlTextureRenderer>(this);
    state->textureRenderer = renderer;
    const bool attached = renderer->attach(scene);
    if (!lifetime || !state->textureEnabled || state->quick != scene || state->textureRenderer != renderer)
        return false;
    if (!attached) {
        state->textureRenderer.reset();
        state->textureError = QStringLiteral("The QML root could not enter texture presentation.");
        emit textureRenderingFailed(state->textureError);
        return false;
    }
    if (property(kDirectCompositionRequiredProperty).toBool())
        setProperty(kDirectCompositionRequiredProperty, false);
    if (!lifetime || state->textureRenderer != renderer)
        return false;
    emit textureUpdateRequested();
    return lifetime && state->textureRenderer == renderer;
}

bool QmlView::renderTexture(QOpenGLContext* context, qreal devicePixelRatio, quint64 maximumBytes)
{
    if (!onGuiThread(this) || !d->active || !d->textureEnabled || d->busy || !ensureTextureRenderer())
        return false;
    const auto state = d;
    const auto renderer = state->textureRenderer;
    const quint64 frames = renderer->frameCount();
    QPointer<QmlView> lifetime(this);
    const bool rendered = renderer->render(context, size(), devicePixelRatio, maximumBytes);
    if (!lifetime || !state->active || !state->textureEnabled || state->textureRenderer != renderer)
        return false;
    if (rendered) {
        state->textureError.clear();
        if (renderer->frameCount() != frames)
            ++state->textureRevision;
    } else {
        const QString error = renderer->lastError();
        if (!error.isEmpty() && state->textureError != error) {
            state->textureError = error;
            emit textureRenderingFailed(error);
        }
    }
    return lifetime && rendered && state->textureRenderer == renderer;
}

unsigned int QmlView::textureId() const
{
    return d->textureRenderer ? d->textureRenderer->textureId() : 0;
}
QSize QmlView::textureSize() const
{
    return d->textureRenderer ? d->textureRenderer->textureSize() : QSize();
}
quint64 QmlView::textureRevision() const
{
    return d->textureRevision;
}
QVariantMap QmlView::textureStatistics() const
{
    QVariantMap result = d->textureRenderer ? d->textureRenderer->statistics() : QVariantMap();
    result.insert(QStringLiteral("requested"), d->textureEnabled);
    result.insert(QStringLiteral("active"), textureId() != 0);
    result.insert(QStringLiteral("revision"), QVariant::fromValue(d->textureRevision));
    result.insert(QStringLiteral("lastError"), d->textureError);
    if (!result.contains(QStringLiteral("allocatedBytes")))
        result.insert(QStringLiteral("allocatedBytes"), QVariant::fromValue(quint64(0)));
    if (!result.contains(QStringLiteral("renderedFrames")))
        result.insert(QStringLiteral("renderedFrames"), QVariant::fromValue(quint64(0)));
    return result;
}
void QmlView::releaseTexture()
{
    if (!onGuiThread(this))
        return;
    const auto renderer = d->textureRenderer;
    if (renderer)
        renderer->release();
}

bool QmlView::event(QEvent* event)
{
    const auto renderer = d->textureRenderer;
    if (d->textureEnabled && renderer && renderer->window()) {
        const QPointer<QQuickWindow> scene(renderer->window());
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove: {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            QPointer<QmlView> lifetime(this);
            if (event->type() == QEvent::MouseButtonPress)
                setFocus(Qt::MouseFocusReason);
            if (!lifetime || !scene)
                return true;
            if (event->type() == QEvent::MouseButtonPress) {
                QFocusEvent focus(QEvent::FocusIn, Qt::MouseFocusReason);
                QCoreApplication::sendEvent(scene, &focus);
            }
            if (!lifetime || !scene)
                return true;
            auto mapped = ::compatibility::detail::mouseEventForLocalScene(mouse);
            const bool handled = QCoreApplication::sendEvent(scene, &mapped);
            event->setAccepted(mapped.isAccepted());
            return handled;
        }
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::ShortcutOverride:
        case QEvent::Wheel:
        case QEvent::Leave:
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
        case QEvent::FocusIn:
        case QEvent::FocusOut:
            return QCoreApplication::sendEvent(scene, event);
        case QEvent::InputMethod:
            return QCoreApplication::sendEvent(scene->focusObject(), event);
        default:
            break;
        }
    }
    return QWidget::event(event);
}

void QmlView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    scheduleNativePresentation();
}

void QmlView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (d->textureRenderer)
        d->textureRenderer->markDirty();
}

QVariant QmlView::inputMethodQuery(Qt::InputMethodQuery query) const
{
    const auto renderer = d->textureRenderer;
    if (d->textureEnabled && renderer && renderer->window()) {
        const auto* item = qobject_cast<QQuickItem*>(renderer->window()->focusObject());
        if (item) {
            QVariant value = item->inputMethodQuery(query);
            if (value.canConvert<QRectF>()
                && (query == Qt::ImCursorRectangle || query == Qt::ImAnchorRectangle
                    || query == Qt::ImInputItemClipRectangle))
                return item->mapRectToScene(value.toRectF());
            if (value.canConvert<QPointF>() && (query == Qt::ImCursorPosition || query == Qt::ImAnchorPosition))
                return item->mapToScene(value.toPointF());
            return value;
        }
    }
    return QWidget::inputMethodQuery(query);
}

void QmlView::setActive(bool active)
{
    if (d->active == active)
        return;
    d->active = active;
    QPointer<QmlView> lifetime(this);
    if (!active)
        unload();
    if (lifetime)
        emit activeChanged(active);
}
void QmlView::onThemeUpdated()
{
    if (!d)
        return;
    if (d->quick)
        d->quick->setClearColor(Qt::transparent);
    if (d->textureRenderer)
        d->textureRenderer->markDirty();
    emit d->theme->changed();
}
} // namespace fluent::qmlbridge
