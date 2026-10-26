cmake_minimum_required(VERSION 3.25)
project(FluentQtWasmAccessibilityOverlay LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.9.3 EXACT REQUIRED COMPONENTS Core Gui OpenGL)
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "This private sub-build requires the pinned Emscripten toolchain")
endif()
foreach(variable FLUENT_QT_OVERLAY_PREPARED FLUENT_QT_OVERLAY_SOURCE_CACHE FLUENT_QT_OVERLAY_ARCHIVES
        FLUENT_QT_OVERLAY_TOOL FLUENT_QT_OVERLAY_MANIFEST FLUENT_QT_OVERLAY_SDK
        FLUENT_QT_OVERLAY_PYTHON FLUENT_QT_OVERLAY_AR)
    if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
        message(FATAL_ERROR "Missing overlay sub-build input: ${variable}")
    endif()
endforeach()
add_library(cache_overlay OBJECT
    "${FLUENT_QT_OVERLAY_PREPARED}/qaccessiblecache.cpp"
    "${FLUENT_QT_OVERLAY_PREPARED}/qaccessiblecache_p.h")
set_target_properties(cache_overlay PROPERTIES AUTOMOC ON)
target_compile_definitions(cache_overlay PRIVATE QT_BUILD_GUI_LIB QT_NO_DEBUG)
target_link_libraries(cache_overlay PRIVATE Qt6::GuiPrivate Qt6::CorePrivate)
target_include_directories(cache_overlay PRIVATE "${FLUENT_QT_OVERLAY_PREPARED}")
add_library(wasm_overlay OBJECT "${FLUENT_QT_OVERLAY_PREPARED}/qwasmaccessibility.cpp")
target_compile_definitions(wasm_overlay PRIVATE QT_STATICPLUGIN QT_NO_DEBUG)
target_link_libraries(wasm_overlay PRIVATE Qt6::GuiPrivate Qt6::CorePrivate Qt6::OpenGLPrivate)
target_include_directories(wasm_overlay PRIVATE "${FLUENT_QT_OVERLAY_SOURCE_CACHE}/src/plugins/platforms/wasm")
foreach(target cache_overlay wasm_overlay)
    target_compile_options(${target} PRIVATE -fexceptions -fvisibility=hidden -fvisibility-inlines-hidden)
endforeach()
set(_gui "${FLUENT_QT_OVERLAY_ARCHIVES}/libQt6Gui.a")
set(_platform "${FLUENT_QT_OVERLAY_ARCHIVES}/libqwasm.a")
set(_record "${FLUENT_QT_OVERLAY_ARCHIVES}/overlay.json")
add_custom_target(fluent_qt_overlay_archives ALL
    COMMAND "${FLUENT_QT_OVERLAY_PYTHON}" "${FLUENT_QT_OVERLAY_TOOL}"
        --manifest "${FLUENT_QT_OVERLAY_MANIFEST}" archives
        --sdk "${FLUENT_QT_OVERLAY_SDK}" --cache "${FLUENT_QT_OVERLAY_SOURCE_CACHE}"
        --object-dir "${CMAKE_CURRENT_BINARY_DIR}"
        --output "${FLUENT_QT_OVERLAY_ARCHIVES}" --ar "${FLUENT_QT_OVERLAY_AR}"
    DEPENDS cache_overlay wasm_overlay $<TARGET_OBJECTS:cache_overlay> $<TARGET_OBJECTS:wasm_overlay>
        "${FLUENT_QT_OVERLAY_TOOL}" "${FLUENT_QT_OVERLAY_MANIFEST}"
    BYPRODUCTS "${_gui}" "${_platform}" "${_record}"
    VERBATIM)
