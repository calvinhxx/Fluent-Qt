cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(FluentQtPluginExtension VERSION 0.1.0 LANGUAGES CXX)
endif()

if(NOT TARGET FluentQt::Plugins)
    find_package(FluentQt CONFIG REQUIRED COMPONENTS Plugins)
endif()
if(EMSCRIPTEN AND NOT TARGET FluentQt::WebAssembly)
    message(FATAL_ERROR
        "The installed FluentQt SDK must enable FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT for this browser example")
endif()

set(_fluentqt_plugin_extension_sources
    main.cpp
    PluginContractProbe.h
    PluginContractProbe.cpp
    PluginExtensionApplication.h
    PluginExtensionWindow.h
    PluginExtensionWindow.cpp)
if(EMSCRIPTEN)
    list(APPEND _fluentqt_plugin_extension_sources
        platform/wasm/PluginExtensionApplication.cpp)
else()
    list(APPEND _fluentqt_plugin_extension_sources
        platform/desktop/PluginExtensionApplication.cpp)
endif()

if(COMMAND qt_add_executable)
    qt_add_executable(fluentqt_plugin_extension ${_fluentqt_plugin_extension_sources})
else()
    add_executable(fluentqt_plugin_extension ${_fluentqt_plugin_extension_sources})
endif()

if(COMMAND fluent_qt_configure_cpp_target)
    fluent_qt_configure_cpp_target(fluentqt_plugin_extension)
else()
    set_target_properties(fluentqt_plugin_extension PROPERTIES
        AUTOMOC ON
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
endif()

target_link_libraries(fluentqt_plugin_extension PRIVATE FluentQt::Plugins)
target_include_directories(fluentqt_plugin_extension PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

if(EMSCRIPTEN)
    target_link_libraries(fluentqt_plugin_extension PRIVATE FluentQt::WebAssembly)
    configure_file(platform/wasm/index.html.in "${CMAKE_CURRENT_BINARY_DIR}/index.html" @ONLY)
    if(TARGET fluent_qt_gallery)
        set(_fluentqt_plugin_extension_destination "${CMAKE_BINARY_DIR}/app/plugin-extension")
        file(MAKE_DIRECTORY "${_fluentqt_plugin_extension_destination}")
        configure_file(platform/wasm/index.html.in
            "${_fluentqt_plugin_extension_destination}/index.html" @ONLY)
        add_custom_command(TARGET fluentqt_plugin_extension POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_fluentqt_plugin_extension_destination}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_BINARY_DIR}/index.html"
                "${CMAKE_CURRENT_BINARY_DIR}/fluentqt_plugin_extension.js"
                "${CMAKE_CURRENT_BINARY_DIR}/fluentqt_plugin_extension.wasm"
                "${CMAKE_CURRENT_BINARY_DIR}/qtloader.js"
                "${CMAKE_CURRENT_BINARY_DIR}/qtlogo.svg"
                "${_fluentqt_plugin_extension_destination}"
            COMMENT "Stage the optional plugin consumer beside the Gallery")
        add_dependencies(fluent_qt_gallery fluentqt_plugin_extension)
    endif()
endif()
