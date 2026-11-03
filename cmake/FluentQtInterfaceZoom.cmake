include_guard(GLOBAL)
get_filename_component(_fluent_qt_zoom_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set_property(GLOBAL PROPERTY FLUENT_QT_INTERFACE_ZOOM_ROOT "${_fluent_qt_zoom_root}")

option(FLUENT_QT_INTERFACE_ZOOM_LIVE "Enable live Gallery zoom when Qt private headers are available" ON)
option(FLUENT_QT_REQUIRE_INTERFACE_ZOOM "Require the live zoom backend in this Gallery/binding build" OFF)

function(fluent_qt_add_interface_zoom_support target)
    cmake_parse_arguments(zoom "RESTART_ONLY;REQUIRED" "" "" ${ARGN})
    get_property(_zoom_root GLOBAL PROPERTY FLUENT_QT_INTERFACE_ZOOM_ROOT)
    set(_zoom_repaint_support FALSE)
    target_sources(${target} PRIVATE
        "${_zoom_root}/support/interfacezoom/InterfaceZoom.cpp"
        "${_zoom_root}/support/interfacezoom/InterfaceZoom.h"
        "${_zoom_root}/src/compatibility/private/InterfaceZoomRepaintCompat.cpp")
    target_include_directories(${target} PRIVATE "${_zoom_root}")
    if(APPLE)
        target_link_libraries(${target} PRIVATE "-lobjc")
    endif()
    if(NOT EMSCRIPTEN)
        if(QT_VERSION_MAJOR EQUAL 6 AND FLUENT_QT_INTERFACE_ZOOM_LIVE AND NOT zoom_RESTART_ONLY)
            find_package(Qt6 QUIET OPTIONAL_COMPONENTS WidgetsPrivate)
        endif()
        fluent_qt_spatial_private_headers_available(_zoom_widgets_private
            MODULE Widgets DEPENDENCIES Core Gui
            HEADERS private/qwidget_p.h private/qwidgetrepaintmanager_p.h private/qgraphicseffect_p.h)
        if(_zoom_widgets_private AND FLUENT_QT_INTERFACE_ZOOM_LIVE AND NOT zoom_RESTART_ONLY)
            include(CheckCXXSourceCompiles)
            include(CMakePushCheckState)
            cmake_push_check_state(RESET)
            set(CMAKE_CXX_STANDARD 17)
            set(CMAKE_CXX_STANDARD_REQUIRED ON)
            set(CMAKE_REQUIRED_LIBRARIES Qt${QT_VERSION_MAJOR}::WidgetsPrivate)
            set(CMAKE_REQUIRED_INCLUDES "${_zoom_root}")
            get_target_property(_zoom_widgets_includes Qt${QT_VERSION_MAJOR}::WidgetsPrivate INTERFACE_INCLUDE_DIRECTORIES)
            file(SHA256 "${_zoom_root}/src/compatibility/private/InterfaceZoomRepaintQueues_p.h" _zoom_queue_probe_hash)
            file(SHA256 "${_zoom_root}/cmake/FluentQtInterfaceZoom.cmake" _zoom_repaint_probe_hash)
            file(SHA256 "${_zoom_root}/src/compatibility/private/InterfaceZoomLowDprCompat_p.h" _zoom_low_dpr_probe_hash)
            set(_zoom_repaint_kit "${_zoom_widgets_includes};cxx17;${_zoom_queue_probe_hash};${_zoom_repaint_probe_hash};${_zoom_low_dpr_probe_hash}")
            if(NOT "${FLUENT_QT_INTERFACE_ZOOM_REPAINT_KIT}" STREQUAL "${_zoom_repaint_kit}")
                unset(FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES CACHE)
                set(FLUENT_QT_INTERFACE_ZOOM_REPAINT_KIT "${_zoom_repaint_kit}" CACHE INTERNAL "Qt kit used by the repaint capability probe" FORCE)
            endif()
            check_cxx_source_compiles("#include \"src/compatibility/private/InterfaceZoomLowDprCompat_p.h\"
                int main(int argc, char**) {
                    auto *widget = reinterpret_cast<QWidget*>(quintptr(argc));
                    auto *manager = QWidgetPrivate::get(widget)->maybeRepaintManager();
                    QPoint offset;
                    QWidgetPrivate::get(widget)->redirected(&offset);
                    QWidgetPrivate::get(widget)->sharedPainter();
                    compatibility::detail::interfaceZoomTexturesLocked(widget);
                    const auto damage = compatibility::detail::pendingInterfaceZoomRasterDamage(manager, widget);
                    compatibility::detail::invalidateInterfaceZoomRasterDamage(widget, damage);
                    compatibility::detail::lowDprWidgetSnapshot(widget, damage.boundingRect());
                    compatibility::detail::repaintLowDprRegion(widget, damage);
                    return damage.rectCount();
                }" FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES)
            cmake_pop_check_state()
            if(FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES)
                set(_zoom_repaint_support TRUE)
                target_compile_definitions(${target} PRIVATE FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES=1)
                target_link_libraries(${target} PRIVATE Qt${QT_VERSION_MAJOR}::WidgetsPrivate)
            endif()
        endif()
    endif()
    set(_zoom_private FALSE)
    if(NOT EMSCRIPTEN AND FLUENT_QT_INTERFACE_ZOOM_LIVE AND NOT zoom_RESTART_ONLY)
        if(QT_VERSION_MAJOR EQUAL 6)
            find_package(Qt6 QUIET OPTIONAL_COMPONENTS GuiPrivate)
        endif()
        fluent_qt_spatial_private_headers_available(_zoom_private
            MODULE Gui DEPENDENCIES Core
            HEADERS private/qhighdpiscaling_p.h qpa/qplatformwindow.h qpa/qwindowsysteminterface.h)
    endif()
    if(NOT _zoom_private OR NOT _zoom_repaint_support)
        if((zoom_REQUIRED OR FLUENT_QT_REQUIRE_INTERFACE_ZOOM) AND NOT zoom_RESTART_ONLY)
            message(FATAL_ERROR
                "${target}: desktop Gallery requires live zoom with matching Qt Core/Gui/Widgets private development headers and repaint interfaces (including effective DPR below 1). "
                "Keep FLUENT_QT_INTERFACE_ZOOM_LIVE=ON and install the private headers for the exact Qt kit being used. "
                "Debian/Ubuntu packages are qtbase5-private-dev (Qt 5) or qt6-base-private-dev (Qt 6). "
                "For a library-only build, disable FLUENT_QT_BUILD_GALLERY and FLUENT_QT_BUILD_PYSIDE6_GALLERY; the Widgets library does not require private headers.")
        endif()
        message(STATUS "${target}: interface zoom uses Qt startup scaling (live zoom private interfaces unavailable)")
        return()
    endif()
    include(CheckCXXSourceCompiles)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_LIBRARIES Qt${QT_VERSION_MAJOR}::GuiPrivate)
    get_target_property(_zoom_private_includes Qt${QT_VERSION_MAJOR}::GuiPrivate INTERFACE_INCLUDE_DIRECTORIES)
    set(_zoom_probe_kit "${QT_VERSION_MAJOR};${_zoom_private_includes}")
    if(NOT "${FLUENT_QT_INTERFACE_ZOOM_PROBE_KIT}" STREQUAL "${_zoom_probe_kit}")
        unset(FLUENT_QT_INTERFACE_ZOOM_HAS_DPR_EVENT CACHE)
        set(FLUENT_QT_INTERFACE_ZOOM_PROBE_KIT "${_zoom_probe_kit}" CACHE INTERNAL "Qt kit used by the zoom capability probe" FORCE)
    endif()
    check_cxx_source_compiles("#include <qpa/qwindowsysteminterface.h>
        int main() { QWindowSystemInterface::handleWindowDevicePixelRatioChanged<QWindowSystemInterface::SynchronousDelivery>(nullptr); }"
        FLUENT_QT_INTERFACE_ZOOM_HAS_DPR_EVENT)
    cmake_pop_check_state()
    target_link_libraries(${target} PRIVATE Qt${QT_VERSION_MAJOR}::GuiPrivate)
    target_compile_definitions(${target} PRIVATE FLUENT_QT_HAS_INTERFACE_ZOOM=1)
    if(FLUENT_QT_INTERFACE_ZOOM_HAS_DPR_EVENT)
        target_compile_definitions(${target} PRIVATE FLUENT_QT_INTERFACE_ZOOM_HAS_DPR_EVENT=1)
    endif()
    message(STATUS "${target}: live interface zoom enabled")
endfunction()
