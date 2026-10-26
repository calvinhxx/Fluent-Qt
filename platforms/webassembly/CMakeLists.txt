if(NOT EMSCRIPTEN)
    message(FATAL_ERROR
        "FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT requires an Emscripten toolchain")
endif()
if(NOT QT_VERSION_MAJOR EQUAL 6 OR NOT Qt6_VERSION VERSION_EQUAL 6.9.3)
    message(FATAL_ERROR
        "WebAssembly support requires the reviewed Qt 6.9.3 SDK (found Qt ${QT_VERSION_MAJOR} ${Qt${QT_VERSION_MAJOR}_VERSION})")
endif()

include("${PROJECT_SOURCE_DIR}/cmake/FluentQtWasmAccessibility.cmake")
fluent_qt_configure_wasm_accessibility_overlay(
    "${PROJECT_SOURCE_DIR}/tools/wasm/qt_accessibility_overlay.py"
    "${PROJECT_SOURCE_DIR}/src/compatibility/private/wasm/qt-6.9.3"
    "${PROJECT_SOURCE_DIR}/cmake/wasm-accessibility")

add_library(FluentQtWasmRuntime STATIC
    "${PROJECT_SOURCE_DIR}/include/FluentQt/WebAssembly.h"
    Runtime.cpp)
add_library(FluentQt::WebAssembly ALIAS FluentQtWasmRuntime)
set_target_properties(FluentQtWasmRuntime PROPERTIES
    EXPORT_NAME WebAssembly)
fluent_qt_configure_cpp_target(FluentQtWasmRuntime)
target_link_libraries(FluentQtWasmRuntime
    PUBLIC FluentQt::FluentQt)

# Asyncify is deliberately opt-in because it increases binary size and CPU
# overhead. Only browser executables with synchronous nested loops link it.
add_library(FluentQtWasmAsyncify INTERFACE)
add_library(FluentQt::WasmAsyncify ALIAS FluentQtWasmAsyncify)
set_target_properties(FluentQtWasmAsyncify PROPERTIES
    EXPORT_NAME WasmAsyncify)
target_link_options(FluentQtWasmAsyncify INTERFACE -sASYNCIFY -O3)
target_compile_definitions(FluentQtWasmAsyncify
    INTERFACE FLUENT_QT_WASM_ASYNCIFY=1)
