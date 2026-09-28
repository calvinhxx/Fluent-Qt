#include <FluentQt/FluentQt.h>
#include <components/foundation/overlay/OverlayPresentation.h>

#include <QApplication>
#include <QAction>
#include <QLocale>

bool verifyOverlayWindowHeaders(QWidget* child, QWidget* top);

// Compile/link fixture for external add_subdirectory consumers.
// CI builds this target but does not start its event loop.
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    fluent::initializeResources();

    auto theme = fluent::ThemeRegistry::instance().snapshot();
    theme.fontScale = 1.0;
    fluent::ThemeRegistry::instance().applySnapshot(theme);

    fluent::collections::ListView list;
    list.setSelectionMode(fluent::collections::SelectionMode::Single);
    list.setFontRole(Typography::FontRole::Body);

    fluent::scrolling::ScrollView scrollView;
    scrollView.setContentWidget(
        new fluent::textfields::Label(QStringLiteral("External source consumer")),
        fluent::WidgetOwnership::Owned);

    fluent::date_time::CalendarView calendar;
    calendar.setLocale(QLocale::English);
    calendar.resetFirstDayOfWeek();

    fluent::basicinput::Button button(QStringLiteral("FluentQt external integration"));
    QWidget overlayAnchor(&button);
    if (!verifyOverlayWindowHeaders(&overlayAnchor, &button))
        return 6;
    const QPoint nativeAnchor = button.mapToGlobal(QPoint(3, 5));
    fluent::overlay::presentation::setTransform(&button, QTransform::fromTranslate(7, 11));
    if (!fluent::overlay::presentation::hasTransform(&button) ||
        fluent::overlay::presentation::mapToGlobal(&button, QPoint(3, 5)) !=
            nativeAnchor + QPoint(7, 11))
        return 1;
    fluent::overlay::presentation::clearTransform(&button);
    if (fluent::overlay::presentation::mapToGlobal(&button, QPoint(3, 5)) != nativeAnchor)
        return 2;
    fluent::menus_toolbars::CommandBar commandBar;
    QAction command(QStringLiteral("External command"));
    commandBar.addPrimaryAction(&command);
    fluent::menus_toolbars::CommandBarFlyout commandFlyout(&button);
    commandFlyout.addSecondaryAction(&command);
    fluent::basicinput::CompoundButton compoundButton(QStringLiteral("Install update"), &button);
    compoundButton.setSecondaryText(QStringLiteral("Downloads and restarts the application"));
    fluent::layout::Accordion accordion;
    fluent::layout::ParticleBackdrop particles;
    if (particles.isGpuAccelerationEnabled())
        return 3;
    particles.setGpuAccelerationEnabled(true);
    if (!particles.isGpuAccelerationEnabled())
        return 4;
    fluent::status_info::Avatar avatar(QStringLiteral("Ada Lovelace"));
#ifdef FLUENT_QT_HAS_SPATIAL
    fluent::spatial::SpatialRuntime::prepareApplication();
    if (!fluent::spatial::SpatialRuntime::isHardwareRenderer(QStringLiteral("Test GPU")) ||
        fluent::spatial::SpatialRuntime::isHardwareRenderer(QStringLiteral("llvmpipe")))
        return 5;
    fluent::spatial::SpatialView spatial;
    spatial.setRenderMode(fluent::spatial::SpatialView::RenderMode::Raster);
    auto* card = new fluent::layout::Card;
    spatial.addWidget(card, fluent::WidgetOwnership::Owned)->setRotation(QVector3D(0, 10, 0));
#endif
    return 0;
}
