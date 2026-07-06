cmake_minimum_required(VERSION 3.16)

# CMP0091: 启用 CMAKE_MSVC_RUNTIME_LIBRARY (NEW) — 必须在 project() 之前设置。
# Windows Qt 官方预编译包使用动态 CRT；项目保持 /MD 与 Qt 二进制一致。
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

project(FluentQT VERSION 1.2.1 LANGUAGES CXX C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(FLUENT_QT_IS_TOP_LEVEL ON)
else()
    set(FLUENT_QT_IS_TOP_LEVEL OFF)
endif()

option(BUILD_TESTING "Enable testing and build tests" ${FLUENT_QT_IS_TOP_LEVEL})
option(FLUENT_QT_BUILD_TESTS "Build Fluent-Qt test targets" ${FLUENT_QT_IS_TOP_LEVEL})
option(FLUENT_QT_BUILD_GALLERY "Build the Fluent-Qt Gallery application" ${FLUENT_QT_IS_TOP_LEVEL})
option(FLUENT_QT_INSTALL "Install Fluent-Qt library targets, headers, and CMake package files" ${FLUENT_QT_IS_TOP_LEVEL})
option(FLUENT_QT_ENABLE_GALLERY_PACKAGING "Enable CPack packaging for the Gallery application" ${FLUENT_QT_IS_TOP_LEVEL})

if(MSVC)
	add_definitions(-DUNICODE -D_UNICODE)
	add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(spdlog CONFIG REQUIRED)

# Enforce minimum supported Qt versions
if(QT_VERSION_MAJOR EQUAL 5 AND Qt5_VERSION VERSION_LESS 5.15)
	message(FATAL_ERROR "Qt 5.15 or newer is required (found ${Qt5_VERSION})")
endif()
if(QT_VERSION_MAJOR EQUAL 6 AND Qt6_VERSION VERSION_LESS 6.2)
	message(FATAL_ERROR "Qt 6.2 or newer is required (found ${Qt6_VERSION})")
endif()
message(STATUS "Building with Qt ${QT_VERSION_MAJOR} (${Qt${QT_VERSION_MAJOR}_VERSION})")

# MSVC CRT 必须与所链接的 Qt 二进制保持一致：
#   - Qt5 官方 Windows 预编译包使用动态 CRT (/MD, /MDd)，否则会出现
#     `__acrt_first_block == header` 等跨 CRT heap 断言/崩溃。
#   - Qt6 官方包同样用 /MD；保持 /MD 最安全，避免 CRT 不匹配。
# 如需 /MT 静态运行时，必须使用同样以 /MT 编译的 Qt 二进制（自行重编）。
if(MSVC)
	# 合法值：MultiThreadedDLL (Release/MD) | MultiThreadedDebugDLL (Debug/MDd)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# find files recurse
file(GLOB_RECURSE SRC CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
foreach(v ${SRC})
	message(STATUS ${v})
endforeach()
set(RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${SRC})

add_library(FluentQt STATIC ${SRC} ${RESOURCES})
add_library(FluentQt::FluentQt ALIAS FluentQt)
# Backward-compatible build-tree alias for existing local tests and examples.
add_library(fluent_qt_lib ALIAS FluentQt)
target_link_libraries(FluentQt
	PRIVATE
	spdlog::spdlog
	PUBLIC
	Qt${QT_VERSION_MAJOR}::Widgets
)
if(APPLE)
	find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
	find_library(OBJC_LIBRARY objc REQUIRED)
	target_link_libraries(FluentQt PUBLIC ${COREGRAPHICS_FRAMEWORK} ${OBJC_LIBRARY})
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

if(FLUENT_QT_INSTALL)
	install(TARGETS FluentQt
		EXPORT FluentQtTargets
		ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Development
		LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Development
		RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Development
		INCLUDES DESTINATION
			"${CMAKE_INSTALL_INCLUDEDIR}"
			"${CMAKE_INSTALL_INCLUDEDIR}/FluentQt")
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/FluentQt"
		DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
		COMPONENT Development
		FILES_MATCHING PATTERN "*.h")
	install(DIRECTORY
		"${CMAKE_CURRENT_SOURCE_DIR}/src/components"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/compatibility"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/design"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/utils"
		DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FluentQt"
		COMPONENT Development
		FILES_MATCHING
		PATTERN "*.h"
		PATTERN "private" EXCLUDE)

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
		DESTINATION "${FLUENT_QT_CMAKE_PACKAGE_DIR}"
		COMPONENT Development)
endif()

if(FLUENT_QT_BUILD_GALLERY)
	add_subdirectory(app)
endif()

# ut
if(BUILD_TESTING AND FLUENT_QT_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

if(FLUENT_QT_BUILD_GALLERY AND FLUENT_QT_ENABLE_GALLERY_PACKAGING)
	include(cmake/FluentQTPackaging.cmake)
endif()
