#include "GallerySpatialShowcase.h"
#include "platform/GalleryPlatform.h"
#include "view/widgets/GallerySampleCatalog.h"

#include <FluentQt/FluentQt.h>
#include <QHBoxLayout>
#include <QPointer>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <emscripten.h>

namespace fluent::gallery::platform {
namespace {
QPointer<QWidget> showcaseContent;

class SpatialModeSwitch final : public basicinput::ToggleSwitch {
public:
    using ToggleSwitch::ToggleSwitch;
    std::function<void()> refresh;
    void onThemeUpdated() override
    {
        ToggleSwitch::onThemeUpdated();
        if (refresh)
            refresh();
    }
};

void applyShowcaseTheme(QWidget* window, HostTheme host)
{
    const auto theme = host == HostTheme::HighContrast ? FluentElement::HighContrast
        : host == HostTheme::Dark
        ? FluentElement::Dark
        : FluentElement::Light;
    FluentElement::setTheme(theme);
    auto palette = window->palette();
    palette.setColor(QPalette::Window, ThemeRegistry::instance().colors(theme).bgCanvas);
    window->setPalette(palette);
}
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void fluentQtGalleryShowcaseActive(int active)
{
    if (showcaseContent)
        showcaseContent->setVisible(active != 0);
}

extern "C" EMSCRIPTEN_KEEPALIVE void fluentQtGalleryShowcaseMotion(int reduced)
{
    if (showcaseContent)
        MotionPolicy::instance().setMode(reduced ? MotionPolicy::Mode::Reduced : MotionPolicy::Mode::Full);
}

QWidget* createSpatialPreviewControls(QWidget* preview, QWidget* parent, const std::function<void()>& stateChanged)
{
#if defined(FLUENT_QT_HAS_SPATIAL)
    const QPointer<spatial::SpatialView> view(preview->findChild<spatial::SpatialView*>("spatialPreviewView"));
    const QPointer<spatial::SpatialSession> session(preview->findChild<spatial::SpatialSession*>());
    if (!view)
        return nullptr;
    auto* controls = new QWidget(parent);
    auto* row = new QHBoxLayout(controls);
    row->setContentsMargins(0, 0, 0, 0);
    auto* mode = new SpatialModeSwitch(controls);
    mode->setObjectName(QStringLiteral("showcaseSpatialMode"));
    mode->setAccessibleName(QStringLiteral("3D scene"));
    mode->setOnContent(QStringLiteral("3D scene"));
    mode->setOffContent(QStringLiteral("3D scene"));
    row->addWidget(mode);
    row->addStretch();
    auto* status = new textfields::Label(controls);
    status->setTextColorRole(textfields::Label::TextColorRole::Secondary);
    row->addWidget(status);
    const auto synchronize = [view, session, mode, status, stateChanged] {
        if (!view)
            return;
        const QSignalBlocker blocker(mode);
        mode->setIsOn(view->isSpatialEnabled());
        mode->setEnabled(session && session->isActive() && MotionPolicy::instance().mode() == MotionPolicy::Mode::Full
            && FluentElement::currentTheme() != FluentElement::HighContrast);
        const bool gpu = view->activeBackend() == spatial::SpatialView::Backend::OpenGL;
        status->setText(!view->isSpatialEnabled() ? QStringLiteral("2D · Qt Widgets")
                : gpu                             ? QStringLiteral("3D · WebGL")
                                                  : QStringLiteral("3D · Raster fallback"));
        status->setToolTip(view->fallbackReason());
        if (stateChanged)
            stateChanged();
    };
    QObject::connect(mode, &basicinput::ToggleSwitch::toggled, controls, [view, session](bool enabled) {
        if (view)
            view->setSpatialEnabled(enabled && session && session->isActive());
    });
    if (session) {
        QObject::connect(session, &spatial::SpatialSession::activeChanged, controls, synchronize);
        QObject::connect(session, &QObject::destroyed, controls, synchronize);
    }
    QObject::connect(view, &spatial::SpatialView::spatialEnabledChanged, controls, synchronize);
    QObject::connect(view, &spatial::SpatialView::rendererChanged, controls, synchronize);
    QObject::connect(view, &spatial::SpatialView::cameraDistanceChanged, controls, synchronize);
    QObject::connect(view, &spatial::SpatialView::zoomChanged, controls, synchronize);
    QObject::connect(&MotionPolicy::instance(), &MotionPolicy::modeChanged, controls, synchronize);
    mode->refresh = synchronize;
    // Isolated hosts own this mode; the sample binding must not load Gallery preferences.
    // zh_CN: 独立宿主管理当前模式，示例绑定不读取 Gallery 偏好。
    view->setSpatialEnabled(session && session->isActive());
    synchronize();
    return controls;
#else
    Q_UNUSED(preview);
    Q_UNUSED(parent);
    Q_UNUSED(stateChanged);
    return nullptr;
#endif
}

QWidget* createSpatialShowcase()
{
    auto* window = new QWidget;
    window->setObjectName(QStringLiteral("gallerySpatialShowcase"));
    window->setWindowTitle(QStringLiteral("FluentQt Spatial · C++ WebAssembly"));
    window->setAutoFillBackground(true);
    // The existing sample binding recognizes isolated previews and does not read or
    // write the full Gallery's 3D preference. The component owns its local mode.
    // zh_CN: 复用独立预览约定，不读写完整 Gallery 的 3D 偏好，由组件管理本地模式。
    window->setProperty("galleryPreviewRouteId", QStringLiteral("spatial-view"));
    auto* layout = new QVBoxLayout(window);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new scrolling::ScrollView(window);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollMode(scrolling::ScrollView::ScrollMode::Disabled);
    layout->addWidget(scroll);
    auto* content = new QWidget;
    showcaseContent = content;
    auto* column = new QVBoxLayout(content);
    column->setContentsMargins(20, 16, 20, 16);
    column->setSpacing(12);
    scroll->setWidget(content);
    applyShowcaseTheme(window, hostTheme());
    fluentQtGalleryShowcaseMotion(
        emscripten_run_script_int("window.matchMedia('(prefers-reduced-motion: reduce)').matches"));
    setHostThemeChangedHandler(window, [window](HostTheme theme) { applyShowcaseTheme(window, theme); });

#if defined(FLUENT_QT_HAS_SPATIAL)
    for (const auto& sample : gallerySamplesForRoute(QStringLiteral("spatial-view"))) {
        if (sample.id != QStringLiteral("spatial-view-scene") || !sample.createPreview)
            continue;
        auto* preview = sample.createPreview(content);
        column->addWidget(preview);
        auto* view = preview->findChild<spatial::SpatialView*>("spatialPreviewView");
        if (!view)
            break;
        const auto publish = [view] {
            const bool gpu = view->activeBackend() == spatial::SpatialView::Backend::OpenGL;
            // Publish observable component state, not a second implementation of the demo.
            // zh_CN: 仅发布组件的真实状态，不在浏览器层模拟示例行为。
            // clang-format off
            EM_ASM({
                const data = document.documentElement.dataset;
                data.fluentQtShowcase = 'spatial';
                data.fluentQtShowcaseBackend = $0 ? 'webgl' : 'raster';
                data.fluentQtShowcaseSpatial = String(Boolean($1));
                data.fluentQtShowcaseDistance = String($2);
                data.fluentQtShowcaseZoom = String($3);
            }, gpu, view->isSpatialEnabled(), view->cameraDistance(), view->zoom());
            // clang-format on
        };
        if (auto* controls = createSpatialPreviewControls(preview, content, publish))
            column->insertWidget(0, controls);
        break;
    }
#else
    column->addWidget(new textfields::Label(QStringLiteral("This build does not include Spatial."), content));
#endif
    column->addStretch();
    return window;
}
} // namespace fluent::gallery::platform
