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

void applyShowcaseTheme(QWidget* window, HostTheme host)
{
    const auto theme = host == HostTheme::HighContrast ? FluentElement::HighContrast
                       : host == HostTheme::Dark       ? FluentElement::Dark
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
        MotionPolicy::instance().setMode(reduced ? MotionPolicy::Mode::Reduced
                                                 : MotionPolicy::Mode::Full);
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
    setHostThemeChangedHandler(window,
                               [window](HostTheme theme) { applyShowcaseTheme(window, theme); });

#if defined(FLUENT_QT_HAS_SPATIAL)
    spatial::SpatialRuntime::prepareApplication();
    auto* toolbar = new QHBoxLayout;
    auto* mode = new basicinput::ToggleSwitch(content);
    mode->setObjectName(QStringLiteral("showcaseSpatialMode"));
    mode->setAccessibleName(QStringLiteral("3D scene"));
    mode->setOnContent(QStringLiteral("3D scene"));
    mode->setOffContent(QStringLiteral("3D scene"));
    toolbar->addWidget(mode);
    toolbar->addStretch();
    auto* status = new textfields::Label(content);
    status->setTextColorRole(textfields::Label::TextColorRole::Secondary);
    toolbar->addWidget(status);
    column->addLayout(toolbar);

    for (const auto& sample : gallerySamplesForRoute(QStringLiteral("spatial-view"))) {
        if (sample.id != QStringLiteral("spatial-view-scene") || !sample.createPreview)
            continue;
        auto* preview = sample.createPreview(content);
        column->addWidget(preview);
        auto* view = preview->findChild<spatial::SpatialView*>("spatialPreviewView");
        if (!view)
            break;
        const auto publish = [view, mode, status] {
            const QSignalBlocker blocker(mode);
            mode->setIsOn(view->isSpatialEnabled());
            mode->setEnabled(MotionPolicy::instance().mode() == MotionPolicy::Mode::Full &&
                             FluentElement::currentTheme() != FluentElement::HighContrast);
            const bool gpu = view->activeBackend() == spatial::SpatialView::Backend::OpenGL;
            status->setText(!view->isSpatialEnabled() ? QStringLiteral("2D · Qt Widgets")
                            : gpu                     ? QStringLiteral("3D · WebGL")
                                                      : QStringLiteral("3D · Raster fallback"));
            status->setToolTip(view->fallbackReason());
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
        QObject::connect(mode, &basicinput::ToggleSwitch::toggled, view,
                         &spatial::SpatialView::setSpatialEnabled);
        QObject::connect(view, &spatial::SpatialView::spatialEnabledChanged, status, publish);
        QObject::connect(view, &spatial::SpatialView::rendererChanged, status, publish);
        QObject::connect(view, &spatial::SpatialView::cameraDistanceChanged, status, publish);
        QObject::connect(view, &spatial::SpatialView::zoomChanged, status, publish);
        QObject::connect(&MotionPolicy::instance(), &MotionPolicy::modeChanged, status, publish);
        setHostThemeChangedHandler(window, [window, publish](HostTheme theme) {
            applyShowcaseTheme(window, theme);
            publish();
        });
        view->setSpatialEnabled(true);
        publish();
        break;
    }
#else
    column->addWidget(
        new textfields::Label(QStringLiteral("This build does not include Spatial."), content));
#endif
    column->addStretch();
    return window;
}
} // namespace fluent::gallery::platform
