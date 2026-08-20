cmake_minimum_required(VERSION 3.16)

# Enable CMake's MSVC runtime abstraction before targets are created. The
# caller's toolchain, including a vcpkg triplet, remains responsible for
# selecting dynamic or static CRT linkage through CMAKE_MSVC_RUNTIME_LIBRARY.
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

project(FluentQt VERSION 1.7.1 LANGUAGES CXX C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/FluentQtInstallHeaders.cmake)
include(cmake/FluentQtSanitizers.cmake)
include(cmake/FluentQtTargetOptions.cmake)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(FLUENT_QT_IS_TOP_LEVEL ON)
else()
    set(FLUENT_QT_IS_TOP_LEVEL OFF)
endif()

option(BUILD_TESTING "Enable testing and build tests" OFF)
option(FLUENT_QT_BUILD_TESTS "Build Fluent-Qt test targets" OFF)
option(FLUENT_QT_BUILD_GALLERY "Build the Fluent-Qt Gallery application" OFF)
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
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${SRC})

add_library(FluentQt STATIC ${SRC} ${RESOURCES})
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
if(APPLE)
	include(cmake/FluentQtApplePlatformDependencies.cmake)
	target_link_libraries(FluentQt PUBLIC FluentQt::ApplePlatformDependencies)
endif()
target_include_directories(FluentQt
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/FluentQt>
)
set_target_properties(FluentQt PROPERTIES
	EXPORT_NAME FluentQt
	OUTPUT_NAME FluentQt
)

if(FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT)
	add_subdirectory(platforms/webassembly)
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
	install(DIRECTORY
		"${CMAKE_CURRENT_SOURCE_DIR}/.agents/skills/build-fluentqt-gui"
		DESTINATION "${_fluent_qt_ai_doc_root}/.agents/skills"
		COMPONENT Development
		PATTERN "__pycache__" EXCLUDE
		PATTERN "*.pyc" EXCLUDE
		PATTERN "*.pyo" EXCLUDE)
endif()

if(FLUENT_QT_BUILD_GALLERY OR (BUILD_TESTING AND FLUENT_QT_BUILD_TESTS))
	if(EMSCRIPTEN)
		if(NOT FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT)
			message(FATAL_ERROR
				"Gallery logging requires FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT")
		endif()
		set(_fluent_qt_logging_source
			"${CMAKE_CURRENT_SOURCE_DIR}/platforms/webassembly/LogBackend.cpp")
	else()
		find_package(spdlog CONFIG REQUIRED)
		set(_fluent_qt_logging_source
			"${CMAKE_CURRENT_SOURCE_DIR}/support/logging/Log.cpp")
	endif()
	add_library(FluentQtLoggingSupport STATIC
		"${_fluent_qt_logging_source}"
		"${CMAKE_CURRENT_SOURCE_DIR}/support/logging/Log.h")
	fluent_qt_configure_cpp_target(FluentQtLoggingSupport)
	target_include_directories(FluentQtLoggingSupport
		PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
	target_link_libraries(FluentQtLoggingSupport
		PUBLIC Qt${QT_VERSION_MAJOR}::Core)
	if(NOT EMSCRIPTEN)
		target_link_libraries(FluentQtLoggingSupport PRIVATE spdlog::spdlog)
	endif()
	fluent_qt_enable_sanitizers(FluentQtLoggingSupport)
endif()

if(FLUENT_QT_BUILD_GALLERY)
	add_subdirectory(app)
endif()

if(FLUENT_QT_BUILD_EXAMPLES)
	add_subdirectory(examples/hello_world)
endif()

if(EMSCRIPTEN AND TARGET fluent_qt_gallery AND TARGET fluentqt_hello_world)
	add_dependencies(fluent_qt_gallery fluentqt_hello_world)
endif()

# ut
if(BUILD_TESTING AND FLUENT_QT_BUILD_TESTS)
	add_subdirectory(tests)
endif()

if(FLUENT_QT_BUILD_GALLERY AND FLUENT_QT_ENABLE_GALLERY_PACKAGING AND NOT EMSCRIPTEN)
	include(cmake/FluentQTPackaging.cmake)
endif()
