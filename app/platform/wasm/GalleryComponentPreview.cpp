#include "GalleryComponentPreview.h"
#include "GallerySpatialShowcase.h"
#include "platform/GalleryPlatform.h"
#include "view/widgets/GallerySampleCatalog.h"
#include "view/widgets/GalleryComponentPlayground.h"
#include "view/widgets/GalleryPlaygroundFactory.h"

#include <FluentQt/FluentQt.h>
#include <QAbstractButton>
#include <QBuffer>
#include <cmath>
#include <QApplication>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QWindow>
#include <QDialog>
#include <QEvent>
#include <QInputMethod>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <emscripten.h>
#include <utility>

namespace fluent::gallery::platform {
namespace {
QPointer<QWidget> previewWindow;
QPointer<QWidget> sampleContent;
QVBoxLayout* sampleLayout = nullptr;
QJsonObject selection;
QPointer<GalleryComponentPlayground> playground;
int playgroundRequestId = 0;
QPointer<QWidget> playgroundHost;
QString playgroundSelection;
int clicks = 0;
int revision = 0;
bool active = true;
int reportedHeight = -1;
bool geometryPending = false;
bool windowResized = false;
bool interactiveRuntime = false;
QPointer<QWidget> playgroundBrowserFocus;
QJsonObject lastPlaygroundViewport;
int playgroundViewportRevision = 0;
int paintedViewportRevision = -1;

QJsonObject playgroundViewport(bool relayout)
{
    auto viewport = playground->interactiveViewport(playgroundHost, relayout);
    if (viewport != lastPlaygroundViewport) {
        lastPlaygroundViewport = viewport;
        ++playgroundViewportRevision;
    }
    viewport["geometryRevision"] = playgroundViewportRevision;
    return viewport;
}

void publishPlayground(const QString& error = {})
{
    if (!playground)
        return;
    QJsonObject liveViewport;
    if (interactiveRuntime) {
        playgroundHost->setGeometry(previewWindow->rect());
        liveViewport = playgroundViewport(true);
    }
    auto state = playground->snapshot();
    state["route"] = selection.value("route");
    state["requestId"] = playgroundRequestId;
    state["selection"] = playgroundSelection;
    state["example"] = selection;
    if (interactiveRuntime) {
        state["preview"] = liveViewport;
    } else {
        auto preview = playground->renderPreview(previewWindow->devicePixelRatioF());
        state = playground->snapshot();
        state["route"] = selection.value("route");
        state["requestId"] = playgroundRequestId;
        state["selection"] = playgroundSelection;
        state["example"] = selection;
        QByteArray pixels;
        QBuffer buffer(&pixels);
        buffer.open(QIODevice::WriteOnly);
        preview.save(&buffer, "PNG");
        state["preview"] = QJsonObject{
            {"url", QStringLiteral("data:image/png;base64,") + QString::fromLatin1(pixels.toBase64())},
            {"width", preview.width() / preview.devicePixelRatio()},
            {"height", preview.height() / preview.devicePixelRatio()}};
    }
    if (!error.isEmpty())
        state["error"] = error;
    const auto json = QJsonDocument(state).toJson(QJsonDocument::Compact);
    // clang-format off
    EM_ASM({
        const json = UTF8ToString($0);
        document.documentElement.dataset.fluentQtPlaygroundState = json;
        window.fluentQtPlaygroundChanged?.(json);
    }, json.constData());
    // clang-format on
}

void preparePlaygroundTeardown()
{
    if (playgroundHost) {
        if (QWidget* focused = qApp ? QApplication::focusWidget() : nullptr;
            focused && (focused == playgroundHost || playgroundHost->findChildren<QWidget*>().contains(focused))) {
            focused->clearFocus();
        }
        for (auto* child : playgroundHost->findChildren<QWidget*>()) {
            if (child->isWindow())
                child->hide();
        }
        // Hand focusWindow back to the persistent desktop surface before deleting child windows so
        // QWasmWindowTreeNode::shutdown() does not re-activate a window mid-teardown.
        // zh_CN: 在销毁子窗口前将 focusWindow 切回持久桌面宿主，避免 QWasmWindowTreeNode::shutdown() 在析构过程中重新激活窗口。
        if (QWidget* surface = previewWindow ? previewWindow->window() : nullptr;
            surface && surface->windowHandle() && QGuiApplication::focusWindow() != surface->windowHandle()) {
            surface->activateWindow();
            surface->windowHandle()->requestActivate();
        }
    }
}

void closePlayground()
{
    preparePlaygroundTeardown();
    if (previewWindow && previewWindow->property("fluentOverlaySurfaceRect").isValid())
        previewWindow->setProperty("fluentOverlaySurfaceRect", QVariant());
    if (playgroundHost)
        playgroundHost->hide();
    delete playgroundHost.data();
    playground = nullptr;
    playgroundSelection.clear();
    playgroundRequestId = 0;
    // clang-format off
    EM_ASM({ delete document.documentElement.dataset.fluentQtPlaygroundState; });
    // clang-format on
}

bool createLiveFixture()
{
    playgroundHost = new QWidget(previewWindow);
    playgroundHost->setGeometry(previewWindow->rect());
    playgroundHost->setProperty("fluentPlaygroundFillParent", true);
    playgroundHost->show();
    playground = GalleryComponentPlayground::createInteractive(selection.value("route").toString(), playgroundHost);
    if (!playground)
        closePlayground();
    return playground != nullptr;
}

QList<QWidget*> pickableControls()
{
    if (!sampleContent)
        return {};
    const QString route = selection.value("route").toString();
    if (route == QStringLiteral("button") && selection.value("sample") != QStringLiteral("button-styles")
        && selection.value("sample") != QStringLiteral("button-interaction-state"))
        return {};
    auto candidates = sampleContent->findChildren<QWidget*>();
    candidates.prepend(sampleContent);
    QList<QWidget*> result;
    for (auto* candidate : candidates)
        if (GalleryComponentPlayground::accepts(route, candidate))
            result.append(candidate);
    if (result.isEmpty() && playgroundRoutes().contains(route))
        result.append(sampleContent);
    return result;
}

QRegion exposedRegion(const QWidget* widget)
{
    if (!widget || !widget->isVisible())
        return {};
    // Containers whose viewport children cover 100% of their rect have an empty own visibleRegion().
    QRegion region = widget->visibleRegion();
    for (const auto* child : widget->findChildren<const QWidget*>()) {
        if (child->isVisible() && !child->visibleRegion().isEmpty()) {
            const QPoint offset = child->mapTo(widget, QPoint());
            region |= child->visibleRegion().translated(offset) & widget->rect();
        }
    }
    return region;
}

void publishPickerTargets()
{
    if (!previewWindow)
        return;
    QJsonArray targets;
    const auto buttons = pickableControls();
    for (int index = 0; index < buttons.size(); ++index) {
        auto* button = buttons[index];
        const QRegion exposed = exposedRegion(button);
        if (exposed.isEmpty())
            continue;
        const QPoint origin = button->mapTo(previewWindow, QPoint());
        const QRect visible = exposed.boundingRect().translated(origin);
        targets.append(QJsonObject{{"id", index},
            {"label",
                button->property("text").toString().isEmpty()
                    ? selection.value("route").toString()
                    : button->property("text").toString()},
            {"x", visible.x()}, {"y", visible.y()}, {"width", visible.width()}, {"height", visible.height()}});
    }
    const auto json = QJsonDocument(
        QJsonObject{{"example", selection}, {"targets", targets}, {"width", previewWindow->width()},
            {"height", previewWindow->height()}})
                          .toJson(QJsonDocument::Compact);
    // clang-format off
    EM_ASM({ window.fluentQtPickerChanged?.(UTF8ToString($0)); }, json.constData());
    // clang-format on
}

void reportPreferredHeight()
{
    // A hidden sample collapses its layout; keep the last height while offscreen.
    // zh_CN: 隐藏的示例会让布局塌缩；离开视口时保留上次的高度。
    if (!previewWindow || !sampleLayout || !sampleContent || !active)
        return;
    const int width = previewWindow->width();
    const int height = sampleLayout->hasHeightForWidth()
        ? sampleLayout->totalHeightForWidth(width)
        : sampleLayout->totalSizeHint().height();
    if (height <= 0 || height == reportedHeight)
        return;
    reportedHeight = height;
    // clang-format off
    EM_ASM({ window.fluentQtComponentResized?.($0); }, height);
    // clang-format on
}

void publishState()
{
    if (!previewWindow || !sampleContent)
        return;
    QJsonArray controls;
    for (auto* button : sampleContent->findChildren<QAbstractButton*>()) {
        const QPoint origin = button->mapTo(previewWindow, QPoint());
        QJsonObject control{{"text", button->text()}, {"name", button->objectName()}, {"enabled", button->isEnabled()},
            {"checked", button->isChecked()}, {"x", origin.x()}, {"y", origin.y()}, {"width", button->width()},
            {"height", button->height()}};
        if (auto* check = qobject_cast<QCheckBox*>(button))
            control["checkState"] = static_cast<int>(check->checkState());
        controls.append(control);
    }
    QJsonObject state = selection;
    state["revision"] = ++revision;
    state["controls"] = controls;
    QJsonArray focusTargets;
    auto widgets = sampleContent->findChildren<QWidget*>();
    widgets.prepend(sampleContent);
    for (auto* widget : widgets) {
        if (!widget->isVisible() || !widget->isEnabled() || widget->focusPolicy() == Qt::NoFocus)
            continue;
        const QPoint origin = widget->mapTo(previewWindow, QPoint());
        const QRect visible = widget->visibleRegion().boundingRect();
        focusTargets.append(QJsonObject{{"class", QString::fromLatin1(widget->metaObject()->className())},
            {"name", widget->objectName()}, {"focused", widget->hasFocus()}, {"x", origin.x()}, {"y", origin.y()},
            {"width", widget->width()}, {"height", widget->height()},
            {"fullyVisible", visible.contains(widget->rect())}});
    }
    state["focusTargets"] = focusTargets;
    state["clicks"] = clicks;
    state["contentWidth"] = sampleContent->width();
    state["contentHeight"] = sampleContent->height();
    state["widgetCount"] = static_cast<int>(sampleContent->findChildren<QWidget*>().size());
    QJsonArray dialogs;
    // The browser desktop owns same-window overlays alongside the preview.
    // zh_CN: 浏览器桌面在预览窗口旁承载同窗口浮层。
    for (auto* dialog : previewWindow->window()->findChildren<QDialog*>()) {
        auto* hit = dialog->window()->childAt(dialog->mapTo(dialog->window(), dialog->rect().center()));
        dialogs.append(QJsonObject{{"visible", dialog->isVisible()}, {"open", dialog->property("isOpen").toBool()},
            {"exposed", hit && (hit == dialog || dialog->isAncestorOf(hit))}, {"x", dialog->x()}, {"y", dialog->y()},
            {"width", dialog->width()}, {"height", dialog->height()}, {"title", dialog->property("title").toString()}});
    }
    state["dialogs"] = dialogs;
#if defined(FLUENT_QT_HAS_SPATIAL)
    if (auto* view = sampleContent->findChild<spatial::SpatialView*>("spatialPreviewView")) {
        state["spatial"] = QJsonObject{{"enabled", view->isSpatialEnabled()},
            {"backend", view->activeBackend() == spatial::SpatialView::Backend::OpenGL ? "webgl" : "raster"},
            {"distance", view->cameraDistance()}, {"zoom", view->zoom()}};
    }
#endif
    const QByteArray json = QJsonDocument(state).toJson(QJsonDocument::Compact);
    // clang-format off
    EM_ASM({ document.documentElement.dataset.fluentQtComponentState = UTF8ToString($0); }, json.constData());
    // clang-format on
}

void settleGeometry()
{
    geometryPending = false;
    reportPreferredHeight();
    publishPickerTargets();
    if (std::exchange(windowResized, false))
        publishState();
}

void scheduleGeometry(bool resized = false)
{
    windowResized = windowResized || resized;
    if (geometryPending || !previewWindow)
        return;
    geometryPending = true;
    QTimer::singleShot(0, previewWindow, settleGeometry);
}

// Layout and host width changes can change the needed height; a host resize also
// moves every control, so the published geometry is refreshed once per turn.
// zh_CN: 布局和宿主宽度变化会改变所需高度；宿主尺寸变化还会移动控件，因此每轮刷新一次几何信息。
class PreviewGeometryObserver final : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize)
            scheduleGeometry(event->type() == QEvent::Resize && watched == previewWindow);
        return QObject::eventFilter(watched, event);
    }
};

// Clip and scale the owning browser frame before Qt commits the corresponding paint.
// zh_CN: Qt 提交当前帧绘制前同步浏览器裁切；属性回读仍由原有状态通道负责。
class PlaygroundPaintObserver final : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() != QEvent::Paint || !playground || !playgroundHost)
            return false;
        const auto* widget = qobject_cast<QWidget*>(watched);
        if (!widget)
            return false;
        bool owned = false;
        for (auto* owner = watched; owner; owner = owner->parent())
            owned = owned || owner == previewWindow;
        if (!owned)
            return false;
        const auto viewport = playgroundViewport(false);
        if (paintedViewportRevision == playgroundViewportRevision)
            return false;
        paintedViewportRevision = playgroundViewportRevision;
        const auto json = QJsonDocument(viewport).toJson(QJsonDocument::Compact);
        // clang-format off
        EM_ASM({ window.fluentQtPlaygroundViewportChanged?.(UTF8ToString($0)); }, json.constData());
        // clang-format on
        return false;
    }
};

void applyTheme(QWidget* window, HostTheme host)
{
    const auto theme = host == HostTheme::HighContrast ? FluentElement::HighContrast
        : host == HostTheme::Dark
        ? FluentElement::Dark
        : FluentElement::Light;
    FluentElement::setTheme(theme);
    auto palette = window->palette();
    palette.setColor(QPalette::Window, ThemeRegistry::instance().colors(theme).bgLayer);
    window->setPalette(palette);
}
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE int fluentQtGalleryComponentSelect()
{
    if (!previewWindow || !sampleLayout)
        return 0;
    const char* raw = emscripten_run_script_string("JSON.stringify(window.fluentQtComponentRequest || {})");
    const QJsonObject request = QJsonDocument::fromJson(raw ? QByteArray(raw) : QByteArray()).object();
    if (sampleContent && request == selection) {
        publishState();
        publishPlayground();
        return 1;
    }
    const QString route = request.value("route").toString();
    const QString sampleId = request.value("sample").toString();
    const QString state = request.value("state").toString();
    // The canonical catalog owns the available routes and their preview factories.
    // zh_CN: 可用路由和预览工厂由 Gallery 的统一目录维护。
    for (const auto& sample : gallerySamplesForRoute(route)) {
        if (sample.id != sampleId || !sample.createPreview)
            continue;
        closePlayground();
        delete sampleContent.data();
        selection = request;
        clicks = 0;
        previewWindow->setProperty("galleryPreviewRouteId", route);
        sampleContent = sample.createPreview(sampleLayout->parentWidget());
        if (!sampleContent)
            return 0;
        sampleContent->setVisible(active);
        sampleLayout->addWidget(sampleContent, 0, sample.fillAvailableWidth ? Qt::AlignVCenter : Qt::AlignCenter);
        if (auto* column = qobject_cast<QVBoxLayout*>(sampleContent->layout())) {
            if (auto* controls = createSpatialPreviewControls(sampleContent, sampleContent, publishState))
                column->insertWidget(0, controls);
        }
        if (auto* check = route == QStringLiteral("checkbox")
                ? sampleContent->findChild<basicinput::CheckBox*>(QStringLiteral("checkboxOption"))
                : nullptr) {
            check->setEnabled(state != QStringLiteral("disabled"));
            if (state == QStringLiteral("checked"))
                check->setCheckState(Qt::Checked);
            else if (state == QStringLiteral("unchecked") || state == QStringLiteral("focus"))
                check->setCheckState(Qt::Unchecked);
        }
        for (auto* button : sampleContent->findChildren<QAbstractButton*>()) {
            QObject::connect(button, &QAbstractButton::clicked, sampleContent, [] {
                ++clicks;
                publishState();
            });
        }
        const QPointer<QWidget> current(sampleContent);
        QTimer::singleShot(0, sampleContent, [current, state] {
            if (!current)
                return;
            if (state == QStringLiteral("focus")) {
                if (auto* check = current->findChild<basicinput::CheckBox*>(QStringLiteral("checkboxOption")))
                    check->setFocus(Qt::TabFocusReason);
                else if (auto* button = qobject_cast<basicinput::Button*>(current.data())) {
                    button->setFocusVisual(true);
                    button->setFocus(Qt::TabFocusReason);
                }
            }
            if (state == QStringLiteral("states")) {
                if (auto* button = current->findChild<basicinput::Button*>(QStringLiteral("buttonStateFocus"))) {
                    button->setFocus(Qt::TabFocusReason);
                    button->setFocusVisual(true);
                }
            }
            publishState();
            publishPickerTargets();
        });
        return 1;
    }
    return 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int fluentQtGalleryPlaygroundPick()
{
    if (!previewWindow || (!interactiveRuntime && !sampleContent))
        return 0;
    const char* raw = emscripten_run_script_string("JSON.stringify(window.fluentQtPlaygroundRequest || {})");
    const auto request = QJsonDocument::fromJson(raw ? QByteArray(raw) : QByteArray()).object();
    if (request.value("example").toObject() != selection)
        return 0;
    const QString operation = request.value("operation").toString();
    if (operation == QStringLiteral("targets")) {
        publishPickerTargets();
        return 1;
    }
    if (operation == QStringLiteral("close")) {
        closePlayground();
        return 1;
    }
    if (operation != QStringLiteral("open") || request.value("selection").toString().isEmpty())
        return 0;
    if (interactiveRuntime) {
        if (!playground || !playgroundSelection.isEmpty()) {
            closePlayground();
            if (!createLiveFixture())
                return 0;
        }
        QString error;
        if (!playground->initializeInteractive(request.value("initialValues").toObject(), &error)) {
            closePlayground();
            return 0;
        }
        playgroundSelection = request.value("selection").toString();
        playgroundRequestId = request.value("requestId").toInt();
        QObject::connect(playground, &GalleryComponentPlayground::stateChanged, playgroundHost, [] {
            publishPlayground();
        });
        publishPlayground();
        playgroundRequestId = 0;
        return 1;
    }
    const auto buttons = pickableControls();
    QWidget* source = nullptr;
    if (request.contains("target")) {
        const auto id = request.value("target");
        const int index = id.toInt(-1);
        if (id.isDouble() && id.toDouble() == index && index >= 0 && index < buttons.size())
            source = buttons[index];
    } else {
        const auto x = request.value("x"), y = request.value("y");
        if (!x.isDouble() || !y.isDouble() || !std::isfinite(x.toDouble()) || !std::isfinite(y.toDouble()))
            return 0;
        const QPoint point(qRound(x.toDouble()), qRound(y.toDouble()));
        if (!previewWindow->rect().contains(point))
            return 0;
        for (QWidget* hit = previewWindow->childAt(point); hit && hit != previewWindow; hit = hit->parentWidget()) {
            if (buttons.contains(hit)) {
                source = hit;
                break;
            }
        }
    }
    if (exposedRegion(source).isEmpty())
        return 0;
    closePlayground();
    // Hidden, parent-owned content never enters the sample layout or its input path.
    // zh_CN: 隐藏且归父窗口所有的预览不加入示例布局，也不介入示例输入。
    playgroundHost = new QWidget(previewWindow, Qt::Window);
    playgroundHost->hide();
    const auto route = selection.value("route").toString();
    playground = GalleryComponentPlayground::accepts(route, source)
        ? GalleryComponentPlayground::fromWidget(route, source, playgroundHost)
        : GalleryComponentPlayground::create(route, playgroundHost);
    if (!playground) {
        closePlayground();
        return 0;
    }
    playgroundSelection = request.value("selection").toString();
    playgroundRequestId = request.value("requestId").toInt();
    QObject::connect(playground, &GalleryComponentPlayground::stateChanged, playgroundHost, [] {
        publishPlayground();
    });
    publishPlayground();
    playgroundRequestId = 0;
    return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int fluentQtGalleryPlaygroundApply()
{
    if (!playground)
        return 0;
    const char* raw = emscripten_run_script_string("JSON.stringify(window.fluentQtPlaygroundRequest || {})");
    const auto request = QJsonDocument::fromJson(raw ? QByteArray(raw) : QByteArray()).object();
    if (request.value("example").toObject() != selection || request.value("selection") != playgroundSelection
        || request.value("requestId").toInt() <= 0)
        return 0;
    if (request.value("operation") == QStringLiteral("reset") && !request.contains("property")) {
        const QSignalBlocker blocked(playground);
        preparePlaygroundTeardown();
    }
    playgroundRequestId = request.value("requestId").toInt();
    QString error;
    const bool accepted = playground->apply(request, &error);
    if (!accepted || request.value("operation") == QStringLiteral("activate"))
        publishPlayground(error);
    playgroundRequestId = 0;
    return accepted ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE void fluentQtGalleryPlaygroundBrowserFocus(int focused, int missingKeyTarget)
{
    if (!interactiveRuntime || !playgroundHost || !playground)
        return;
    // The enclosing HTML editor and the isolated Qt canvas share browser focus.
    // zh_CN: 外层 HTML 属性编辑器与独立 Qt 画布共享浏览器焦点，不能各自保留一个活动焦点。
    if (!focused) {
        auto* current = QApplication::focusWidget();
        if (!current || (current != playgroundHost && !playgroundHost->isAncestorOf(current)))
            return;
        playgroundBrowserFocus = current;
        current->clearFocus();
    } else if (!QApplication::focusWidget()) {
        if (playgroundBrowserFocus && playgroundHost->isAncestorOf(playgroundBrowserFocus)
            && playgroundBrowserFocus->isVisible() && playgroundBrowserFocus->isEnabled()) {
            playgroundBrowserFocus->setFocus(Qt::OtherFocusReason);
        } else {
            for (auto* widget : playgroundHost->findChildren<QWidget*>()) {
                if (widget->isVisible() && widget->isEnabled() && (widget->focusPolicy() & Qt::TabFocus)) {
                    widget->setFocus(Qt::TabFocusReason);
                    break;
                }
            }
        }
    } else {
        // Returning to the same editor can leave Qt focused while the browser sends keys to the document body.
        if (missingKeyTarget && QInputMethod::queryFocusObject(Qt::ImEnabled, {}).toBool())
            QGuiApplication::inputMethod()->show();
        return;
    }
    publishPlayground();
}

extern "C" EMSCRIPTEN_KEEPALIVE int fluentQtGalleryPlaygroundTabBoundary(int backward, int force)
{
    if (!interactiveRuntime || !playgroundHost)
        return 0;
    QWidget* focused = QApplication::focusWidget();
    if (!focused)
        return 1;
    if (!force && (QApplication::activePopupWidget() || focused->window()->isModal()))
        return 0;
    if (!force) {
        if (auto* edit = qobject_cast<QTextEdit*>(focused); edit && !edit->tabChangesFocus())
            return 0;
        if (auto* edit = qobject_cast<QPlainTextEdit*>(focused); edit && !edit->tabChangesFocus())
            return 0;
    }
    QWidget* candidate = focused;
    do {
        candidate = backward ? candidate->previousInFocusChain() : candidate->nextInFocusChain();
        if (candidate == focused || candidate == previewWindow || candidate == playgroundHost)
            return 1;
        if (candidate->isVisible() && candidate->isEnabled() && (candidate->focusPolicy() & Qt::TabFocus))
            return 0;
    } while (candidate != focused);
    return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int fluentQtGalleryPlaygroundHasOverlay()
{
    return interactiveRuntime && playground && playgroundHost
        && playground->interactiveViewport(playgroundHost).value("hasOpenOverlay").toBool();
}

extern "C" EMSCRIPTEN_KEEPALIVE void fluentQtGalleryComponentActive(int visible)
{
    active = visible != 0;
    if (sampleContent)
        sampleContent->setVisible(active);
    scheduleGeometry();
}

extern "C" EMSCRIPTEN_KEEPALIVE void fluentQtGalleryComponentMotion(int reduced)
{
    MotionPolicy::instance().setMode(reduced ? MotionPolicy::Mode::Reduced : MotionPolicy::Mode::Full);
}

QWidget* createComponentPreview()
{
    interactiveRuntime = emscripten_run_script_int("window.fluentQtInteractivePlayground === true");
    if (interactiveRuntime) {
        auto* window = new QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint);
        previewWindow = window;
        window->setAttribute(Qt::WA_TranslucentBackground);
        window->setAutoFillBackground(false);
        const char* raw = emscripten_run_script_string("JSON.stringify(window.fluentQtComponentRequest || {})");
        selection = QJsonDocument::fromJson(raw ? QByteArray(raw) : QByteArray()).object();
        setHostThemeChangedHandler(window, [window](HostTheme theme) {
            applyTheme(window, theme);
            publishPlayground();
        });
        applyTheme(window, hostTheme());
        // Let Qt select the Quick backing store before the browser window is shown.
        if (selection.value("route") == QStringLiteral("qml-view"))
            createLiveFixture();
        qApp->installEventFilter(new PlaygroundPaintObserver(window));
        return window;
    }
    auto* window = new QWidget(nullptr, Qt::FramelessWindowHint);
    previewWindow = window;
    window->setObjectName(QStringLiteral("galleryComponentPreview"));
    window->setAutoFillBackground(true);
    // Publish settled focus geometry for the isolated preview's browser checks.
    // zh_CN: 为独立预览的浏览器检查记录布局稳定后的焦点几何。
    QObject::connect(qApp, &QApplication::focusChanged, window, [window](QWidget*, QWidget*) {
        QTimer::singleShot(0, window, [] { publishState(); });
    });
    auto* layout = new QVBoxLayout(window);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new scrolling::ScrollView(window);
    scroll->setWidgetResizable(true);
    layout->addWidget(scroll);
    auto* stage = new QWidget;
    sampleLayout = new QVBoxLayout(stage);
    sampleLayout->setContentsMargins(16, 16, 16, 16);
    sampleLayout->setSizeConstraint(QLayout::SetMinimumSize);
    scroll->setWidget(stage);
    auto* geometryObserver = new PreviewGeometryObserver(window);
    stage->installEventFilter(geometryObserver);
    window->installEventFilter(geometryObserver);
    applyTheme(window, hostTheme());
    setHostThemeChangedHandler(window, [window](HostTheme theme) {
        applyTheme(window, theme);
        publishPlayground();
    });
    fluentQtGalleryComponentMotion(
        emscripten_run_script_int("window.matchMedia('(prefers-reduced-motion: reduce)').matches"));
    fluentQtGalleryComponentSelect();
    return window;
}
} // namespace fluent::gallery::platform
