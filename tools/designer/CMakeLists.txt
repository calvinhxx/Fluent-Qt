cmake_minimum_required(VERSION 3.21)
project(FluentQtDesignerPreview LANGUAGES CXX)

set(FLUENTQT_DESIGNER_EXTENSIONS "" CACHE STRING "Optional form components: Particles;Spatial;QmlBridge")
find_package(FluentQt CONFIG REQUIRED COMPONENTS ${FLUENTQT_DESIGNER_EXTENSIONS})
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    generate_native.py fluentqt_designer_catalog.py widgets.json)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_native.py"
    "${generated}" ${FLUENTQT_DESIGNER_EXTENSIONS} COMMAND_ERROR_IS_FATAL ANY)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
add_library(fluentqt_form_widgets STATIC "${generated}/fluentqt_designer.h"
    DesignerSurfaces.h DesignerSurfaces.cpp DesignerForms.cpp)
set_target_properties(fluentqt_form_widgets PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(fluentqt_form_widgets PUBLIC "${generated}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(fluentqt_form_widgets PUBLIC FluentQt::FluentQt)
foreach(extension IN LISTS FLUENTQT_DESIGNER_EXTENSIONS)
    target_link_libraries(fluentqt_form_widgets PUBLIC FluentQt::${extension})
endforeach()

option(FLUENTQT_BUILD_DESIGNER_PLUGIN "Build the widget box plugin" ON)
if(FLUENTQT_BUILD_DESIGNER_PLUGIN)
    if(TARGET Qt6::Widgets)
        set(qt_package Qt6)
    else()
        set(qt_package Qt5)
    endif()
    find_package(${qt_package} REQUIRED COMPONENTS UiPlugin Designer)
    add_library(fluentqt_designer_plugin MODULE WidgetCollection.cpp DesignerPageExtension.h "${generated}/widget_icons.qrc"
        templates/fluentqt_form_templates.qrc)
    if("Spatial" IN_LIST FLUENTQT_DESIGNER_EXTENSIONS)
        target_sources(fluentqt_designer_plugin PRIVATE templates/fluentqt_spatial_template.qrc)
    endif()
    target_link_libraries(fluentqt_designer_plugin PRIVATE fluentqt_form_widgets ${qt_package}::UiPlugin ${qt_package}::Designer)
    set_target_properties(fluentqt_designer_plugin PROPERTIES
        AUTORCC ON
        LIBRARY_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/plugins/designer>"
        RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/plugins/designer>")
endif()

if(PROJECT_IS_TOP_LEVEL)
    add_executable(fluentqt_form_preview preview.cpp preview.ui)
    set_target_properties(fluentqt_form_preview PROPERTIES AUTOUIC ON
        AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/templates")
    target_link_libraries(fluentqt_form_preview PRIVATE fluentqt_form_widgets)
endif()
