#include "GalleryComponentPreview.h"
#include "GallerySpatialShowcase.h"
#include "platform/GalleryPlatform.h"
#include "view/widgets/GallerySampleCatalog.h"

#include <FluentQt/FluentQt.h>
#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>
#include <emscripten.h>
#include <utility>

namespace fluent::gallery::platform {
namespace {
QPointer<QWidget> previewWindow;
QPointer<QWidget> sampleContent;
QVBoxLayout* sampleLayout = nullptr;
QJsonObject selection;
int clicks = 0;
int revision = 0;
bool active = true;
int reportedHeight = -1;
bool geometryPending = false;
bool windowResized = false;

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
        });
        return 1;
    }
    return 0;
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
    setHostThemeChangedHandler(window, [window](HostTheme theme) { applyTheme(window, theme); });
    fluentQtGalleryComponentMotion(
        emscripten_run_script_int("window.matchMedia('(prefers-reduced-motion: reduce)').matches"));
    fluentQtGalleryComponentSelect();
    return window;
}
} // namespace fluent::gallery::platform
