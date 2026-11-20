source_group(TREE ${PROJECT_SOURCE_DIR}/src FILES ${SRC})
set_source_files_properties(${PROJECT_SOURCE_DIR}/src/utils/DiagnosticSession.cpp PROPERTIES
    COMPILE_DEFINITIONS "FLUENTQT_DIAGNOSTIC_VERSION=\"${PROJECT_VERSION}\"")

set(icon_index "${CMAKE_CURRENT_BINARY_DIR}/generated/FluentQtIconIndex.inc")
set(icon_catalog "${PROJECT_SOURCE_DIR}/res/icons/FluentQtIcons.json")
set(icon_aliases "${PROJECT_SOURCE_DIR}/res/icons/FluentQtIconAliases.json")
add_custom_command(OUTPUT "${icon_index}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND "${CMAKE_COMMAND}" "-Dcatalog=${icon_catalog}" "-Daliases=${icon_aliases}" "-Doutput=${icon_index}"
        -P "${PROJECT_SOURCE_DIR}/cmake/GenerateIconIndex.cmake"
    DEPENDS "${icon_catalog}" "${icon_aliases}" "${PROJECT_SOURCE_DIR}/cmake/GenerateIconIndex.cmake"
    VERBATIM)
add_library(FluentQt STATIC ${SRC} ${RESOURCES} "${icon_index}")
target_include_directories(FluentQt PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_library(FluentQt::FluentQt ALIAS FluentQt)
# Backward-compatible build-tree alias for existing local tests and examples.
add_library(fluent_qt_lib ALIAS FluentQt)
fluent_qt_configure_cpp_target(FluentQt)
fluent_qt_enable_project_warnings(FluentQt)
fluent_qt_enable_sanitizers(FluentQt)
target_link_libraries(FluentQt
    PUBLIC
    Qt${QT_VERSION_MAJOR}::Widgets
)
if(QT_VERSION_MAJOR EQUAL 5)
    # Consumers compile the public UTF-8 headers too. Match Qt 6's MSVC opt-out.
    target_compile_options(FluentQt INTERFACE
        "$<$<AND:$<COMPILE_LANG_AND_ID:CXX,MSVC>,$<NOT:$<BOOL:$<TARGET_PROPERTY:QT_NO_UTF8_SOURCE>>>>:/utf-8>")
endif()
if(APPLE)
    target_link_libraries(FluentQt PUBLIC FluentQt::ApplePlatformDependencies)
endif()
target_include_directories(FluentQt
    PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/FluentQt>
)
set_target_properties(FluentQt PROPERTIES
    EXPORT_NAME FluentQt
    OUTPUT_NAME FluentQt
    ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    AUTOGEN_BUILD_DIR "${PROJECT_BINARY_DIR}/FluentQt_autogen"
)

if(FLUENT_QT_BUILD_EXAMPLES)
    include("${PROJECT_SOURCE_DIR}/cmake/FluentQtHelloWorldExamples.cmake")
    fluent_qt_add_hello_world_example(fluentqt_hello_world FluentQt::FluentQt
        HelloWorldWindow.cpp "${PROJECT_BINARY_DIR}/examples/hello_world")
endif()
