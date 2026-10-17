cmake_minimum_required(VERSION 3.16)

# Enable CMake's MSVC runtime abstraction before targets are created. The
# caller's toolchain, including a vcpkg triplet, remains responsible for
# selecting dynamic or static CRT linkage through CMAKE_MSVC_RUNTIME_LIBRARY.
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

project(FluentQt VERSION 1.10.1 LANGUAGES CXX C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/FluentQtInstallHeaders.cmake)
include(cmake/FluentQtSanitizers.cmake)
include(cmake/FluentQtTargetOptions.cmake)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(FLUENT_QT_IS_TOP_LEVEL ON)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
else()
    set(FLUENT_QT_IS_TOP_LEVEL OFF)
endif()

option(BUILD_TESTING "Enable testing and build tests" OFF)
option(FLUENT_QT_BUILD_TESTS "Build Fluent-Qt test targets" OFF)
option(FLUENT_QT_BUILD_GALLERY "Build the Fluent-Qt Gallery application" OFF)
option(FLUENT_QT_BUILD_SPATIAL "Build the optional Spatial module (requires Qt OpenGL support)" OFF)
option(FLUENT_QT_BUILD_PLUGINS "Build the optional plugin interfaces and lifecycle manager" OFF)
option(FLUENT_QT_BUILD_QML_BRIDGE "Build the optional QML host plugin (requires Qt QuickWidgets)" OFF)
option(FLUENT_QT_BUILD_EXAMPLES "Build Fluent-Qt executable examples" ${FLUENT_QT_IS_TOP_LEVEL})
option(FLUENT_QT_BUILD_PYSIDE6_BINDINGS "Build the optional PySide6 bindings" OFF)
option(FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT "Build the optional WebAssembly adapter targets" OFF)
option(FLUENT_QT_INSTALL "Install Fluent-Qt library targets, headers, and CMake package files" ${FLUENT_QT_IS_TOP_LEVEL})
option(FLUENT_QT_ENABLE_GALLERY_PACKAGING "Enable CPack packaging for the Gallery application" OFF)
option(FLUENT_QT_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer for FluentQt tests" OFF)
option(FLUENT_QT_ENABLE_WARNINGS "Enable strict compiler warnings for the FluentQt library" ${FLUENT_QT_IS_TOP_LEVEL})

if(BUILD_TESTING AND
   (FLUENT_QT_BUILD_TESTS OR FLUENT_QT_BUILD_PYSIDE6_BINDINGS))
    enable_testing()
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# Enforce minimum supported Qt versions
if(QT_VERSION_MAJOR EQUAL 5 AND Qt5_VERSION VERSION_LESS 5.15)
    message(FATAL_ERROR "Qt 5.15 or newer is required (found ${Qt5_VERSION})")
endif()
if(QT_VERSION_MAJOR EQUAL 6 AND Qt6_VERSION VERSION_LESS 6.2)
    message(FATAL_ERROR "Qt 6.2 or newer is required (found ${Qt6_VERSION})")
endif()
message(STATUS "Building with Qt ${QT_VERSION_MAJOR} (${Qt${QT_VERSION_MAJOR}_VERSION})")

# Keep CONFIGURE_DEPENDS here: the source tree is the canonical implementation
# inventory, while cmake/FluentQtInstallHeaders.cmake remains the explicit public
# header allowlist. Sorting keeps IDE and generated build files deterministic.
file(GLOB_RECURSE SRC CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(SORT SRC)
set(FLUENT_QT_SPATIAL_SOURCES ${SRC})
list(FILTER FLUENT_QT_SPATIAL_SOURCES INCLUDE REGEX "/(components/spatial|compatibility/private/spatial)/")
set(FLUENT_QT_SPATIAL_RENDERER_SOURCES ${FLUENT_QT_SPATIAL_SOURCES})
list(FILTER FLUENT_QT_SPATIAL_RENDERER_SOURCES INCLUDE REGEX "/compatibility/private/spatial/renderer/")
list(FILTER FLUENT_QT_SPATIAL_SOURCES EXCLUDE REGEX "/compatibility/private/spatial/renderer/")
list(FILTER SRC EXCLUDE REGEX "/(components/spatial|compatibility/private/spatial)/")
set(FLUENT_QT_QML_BRIDGE_SOURCES ${SRC})
list(FILTER FLUENT_QT_QML_BRIDGE_SOURCES INCLUDE REGEX "/(components/qmlbridge|compatibility/private/quick)/")
list(FILTER SRC EXCLUDE REGEX "/(components/qmlbridge|compatibility/private/quick)/")
set(FLUENT_QT_PLUGIN_SOURCES ${SRC})
list(FILTER FLUENT_QT_PLUGIN_SOURCES INCLUDE REGEX "/components/foundation/plugins/")
list(FILTER SRC EXCLUDE REGEX "/components/foundation/plugins/")
set(FLUENT_QT_OPTIONAL_RESOURCE_ENTRIES "")
if(EMSCRIPTEN AND FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT)
    set(FLUENT_QT_OPTIONAL_RESOURCE_ENTRIES
        "        <file alias=\"res/fonts/FluentQtUISimplifiedChinese-Regular.otf\">${CMAKE_CURRENT_SOURCE_DIR}/res/fonts/FluentQtUISimplifiedChinese-Regular.otf</file>")
endif()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/resources.qrc"
    @ONLY)
set(RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/generated/resources.qrc)
if(APPLE)
    include(cmake/FluentQtApplePlatformDependencies.cmake)
endif()

if(FLUENT_QT_BUILD_SPATIAL OR FLUENT_QT_BUILD_QML_BRIDGE)
    set(FLUENT_QT_BUILD_PLUGINS ON)
endif()
include(cmake/FluentQtPlugins.cmake)
include(cmake/FluentQtSpatial.cmake)
include(cmake/FluentQtQmlBridge.cmake)
# Imported Qt targets must be visible to sibling app, bindings, and tests.
if(FLUENT_QT_BUILD_SPATIAL)
    fluent_qt_find_spatial_dependencies()
endif()
if(FLUENT_QT_BUILD_QML_BRIDGE)
    fluent_qt_find_qml_bridge_dependencies()
endif()

if(FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT)
    add_subdirectory(platforms/webassembly)
endif()

add_subdirectory(uilib)
if(FLUENT_QT_BUILD_PLUGINS)
    add_subdirectory(plugin)
endif()

if(FLUENT_QT_BUILD_PYSIDE6_BINDINGS)
    if(NOT QT_VERSION_MAJOR EQUAL 6)
        message(FATAL_ERROR
            "FLUENT_QT_BUILD_PYSIDE6_BINDINGS requires Qt 6.2 or newer")
    endif()
    set_target_properties(FluentQt PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_subdirectory(bindings/pyside6)
endif()

add_custom_target(fluent_qt_source_package
    COMMAND ${CMAKE_COMMAND}
        -D "FLUENT_QT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        -D "FLUENT_QT_SOURCE_PACKAGE_DIR=${CMAKE_CURRENT_BINARY_DIR}/packages"
        -D "FLUENT_QT_SOURCE_PACKAGE_VERSION=${PROJECT_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CreateSourcePackage.cmake"
    COMMENT "Create the FluentQt library source package")

if(FLUENT_QT_INSTALL)
    set(_fluent_qt_install_targets FluentQt)
    if(TARGET FluentQtWasmRuntime)
        list(APPEND _fluent_qt_install_targets FluentQtWasmRuntime)
    endif()
    if(TARGET FluentQtWasmAsyncify)
        list(APPEND _fluent_qt_install_targets FluentQtWasmAsyncify)
    endif()
    install(TARGETS ${_fluent_qt_install_targets}
        EXPORT FluentQtTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Development
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Development
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Development
        INCLUDES DESTINATION
            "${CMAKE_INSTALL_INCLUDEDIR}"
            "${CMAKE_INSTALL_INCLUDEDIR}/FluentQt")
    fluent_qt_install_headers("${CMAKE_CURRENT_SOURCE_DIR}")

    set(FLUENT_QT_CMAKE_PACKAGE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/FluentQt")
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FluentQtConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/FluentQtConfig.cmake"
        INSTALL_DESTINATION "${FLUENT_QT_CMAKE_PACKAGE_DIR}")
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/FluentQtConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion)
    install(EXPORT FluentQtTargets
        NAMESPACE FluentQt::
        DESTINATION "${FLUENT_QT_CMAKE_PACKAGE_DIR}"
        COMPONENT Development)
    if(TARGET FluentQtSpatial)
        install(TARGETS FluentQtSpatial EXPORT FluentQtSpatialTargets
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Development)
        install(EXPORT FluentQtSpatialTargets NAMESPACE FluentQt::
            DESTINATION "${FLUENT_QT_CMAKE_PACKAGE_DIR}" COMPONENT Development)
    endif()
    if(TARGET FluentQtPlugins)
        install(TARGETS FluentQtPlugins EXPORT FluentQtPluginsTargets
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Development)
        install(EXPORT FluentQtPluginsTargets NAMESPACE FluentQt::
            DESTINATION "${FLUENT_QT_CMAKE_PACKAGE_DIR}" COMPONENT Development)
    endif()
    if(TARGET FluentQtQmlBridge)
        install(TARGETS FluentQtQmlBridge EXPORT FluentQtQmlBridgeTargets
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Development)
        install(EXPORT FluentQtQmlBridgeTargets NAMESPACE FluentQt::
            DESTINATION "${FLUENT_QT_CMAKE_PACKAGE_DIR}" COMPONENT Development)
    endif()
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/FluentQtConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/FluentQtConfigVersion.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FluentQtApplePlatformDependencies.cmake"
        DESTINATION "${FLUENT_QT_CMAKE_PACKAGE_DIR}"
        COMPONENT Development)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/TRADEMARKS.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/FluentQt"
        COMPONENT Development)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/inter/LICENSE.txt"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/FluentQt/third_party/inter"
        COMPONENT Development)
    if(EMSCRIPTEN AND FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT)
        install(FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/noto-sans-sc/LICENSE.txt"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/FluentQt/third_party/noto-sans-sc"
            COMPONENT Development)
    endif()
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/icons/fluentui-system-icons/LICENSE.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/icons/fluentui-system-icons/NOTICE.txt"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/FluentQt/third_party/fluentui-system-icons"
        COMPONENT Development)

    set(_fluent_qt_ai_doc_root "${CMAKE_INSTALL_DATADIR}/doc/FluentQt")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/llms.txt"
        DESTINATION "${_fluent_qt_ai_doc_root}"
        COMPONENT Development)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/docs/ai"
        DESTINATION "${_fluent_qt_ai_doc_root}/docs"
        COMPONENT Development)
    install(PROGRAMS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/ai/query_ai_catalog.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/ai/evaluate_ai_catalog.py"
        DESTINATION "${_fluent_qt_ai_doc_root}/tools/ai"
        COMPONENT Development)
    install(PROGRAMS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/onboarding/fluentqt"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/onboarding/fluentqt_create.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/onboarding/fluentqt_doctor.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/onboarding/fluentqt_trial.py"
        DESTINATION "${_fluent_qt_ai_doc_root}/tools/onboarding"
        COMPONENT Development)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tools/onboarding/"
        DESTINATION "${_fluent_qt_ai_doc_root}/tools/onboarding"
        COMPONENT Development
        PATTERN "fluentqt" EXCLUDE
        PATTERN "fluentqt_create.py" EXCLUDE
        PATTERN "fluentqt_doctor.py" EXCLUDE
        PATTERN "fluentqt_trial.py" EXCLUDE
        PATTERN "test_*.py" EXCLUDE
        PATTERN "__pycache__" EXCLUDE
        PATTERN "*.pyc" EXCLUDE
        PATTERN "*.pyo" EXCLUDE)
    install(DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}/.agents/skills/build-fluentqt-gui"
        DESTINATION "${_fluent_qt_ai_doc_root}/.agents/skills"
        COMPONENT Development
        PATTERN "__pycache__" EXCLUDE
        PATTERN "*.pyc" EXCLUDE
        PATTERN "*.pyo" EXCLUDE)
endif()

if(FLUENT_QT_BUILD_GALLERY OR (BUILD_TESTING AND FLUENT_QT_BUILD_TESTS))
    if(NOT EMSCRIPTEN)
        find_package(spdlog CONFIG REQUIRED)
    endif()
    add_subdirectory(app)
endif()

if(EMSCRIPTEN AND TARGET fluent_qt_gallery)
    get_property(_fluent_qt_gallery_examples GLOBAL PROPERTY FLUENT_QT_GALLERY_EXAMPLE_TARGETS)
    if(_fluent_qt_gallery_examples)
        add_dependencies(fluent_qt_gallery ${_fluent_qt_gallery_examples})
    endif()
endif()

# ut
if(BUILD_TESTING AND FLUENT_QT_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(FLUENT_QT_BUILD_GALLERY AND FLUENT_QT_ENABLE_GALLERY_PACKAGING AND NOT EMSCRIPTEN)
    include(cmake/FluentQTPackaging.cmake)
endif()
