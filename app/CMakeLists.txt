include("${CMAKE_CURRENT_SOURCE_DIR}/GalleryMetadata.cmake")

file(GLOB_RECURSE FLUENT_QT_GALLERY_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/model/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/model/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/view/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/view/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodel/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodel/*.cpp
)
set(FLUENT_QT_GALLERY_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/gallery_resources.qrc
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
    ${FLUENT_QT_GALLERY_SOURCES}
    ${FLUENT_QT_GALLERY_RESOURCES}
)

set(FLUENT_QT_GALLERY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
set(FLUENT_QT_GALLERY_SOURCES ${FLUENT_QT_GALLERY_SOURCES} PARENT_SCOPE)
set(FLUENT_QT_GALLERY_RESOURCES ${FLUENT_QT_GALLERY_RESOURCES} PARENT_SCOPE)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)

add_executable(${FLUENT_QT_GALLERY_EXECUTABLE_NAME}
    main.cpp
    ${FLUENT_QT_GALLERY_SOURCES}
    ${FLUENT_QT_GALLERY_RESOURCES}
)
target_link_libraries(${FLUENT_QT_GALLERY_EXECUTABLE_NAME}
    PRIVATE
    FluentQt::FluentQt
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)
target_include_directories(${FLUENT_QT_GALLERY_EXECUTABLE_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(${FLUENT_QT_GALLERY_EXECUTABLE_NAME} PRIVATE
    FLUENT_QT_GALLERY_VERSION="${PROJECT_VERSION}"
    FLUENT_QT_GALLERY_APP_ID="${FLUENT_QT_GALLERY_APP_ID}"
    FLUENT_QT_GALLERY_DISPLAY_NAME="${FLUENT_QT_GALLERY_DISPLAY_NAME}"
    FLUENT_QT_GALLERY_ORGANIZATION_NAME="${FLUENT_QT_GALLERY_ORGANIZATION_NAME}"
)

# Windows: embed the native icon and version metadata. Non-Debug builds use the
# GUI subsystem; Debug retains a console for development logging.
if(WIN32)
    target_link_libraries(${FLUENT_QT_GALLERY_EXECUTABLE_NAME} PRIVATE user32)
    set_target_properties(${FLUENT_QT_GALLERY_EXECUTABLE_NAME} PROPERTIES
        WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
    )

    set(FLUENT_QT_GALLERY_ICO
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/${FLUENT_QT_GALLERY_ICON_BASENAME}.ico")
    file(TO_CMAKE_PATH "${FLUENT_QT_GALLERY_ICO}" FLUENT_QT_GALLERY_ICO)
    set(FLUENT_QT_GALLERY_RC "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/app.rc.in"
        "${FLUENT_QT_GALLERY_RC}"
        @ONLY)
    set_source_files_properties("${FLUENT_QT_GALLERY_RC}" PROPERTIES
        OBJECT_DEPENDS "${FLUENT_QT_GALLERY_ICO}")
    target_sources(${FLUENT_QT_GALLERY_EXECUTABLE_NAME}
        PRIVATE "${FLUENT_QT_GALLERY_RC}")
endif()

# macOS: build a native application bundle using the same canonical identity.
if(APPLE)
    set(FLUENT_QT_GALLERY_ICNS
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/${FLUENT_QT_GALLERY_ICON_BASENAME}.icns")
    target_sources(${FLUENT_QT_GALLERY_EXECUTABLE_NAME}
        PRIVATE "${FLUENT_QT_GALLERY_ICNS}")
    set_source_files_properties("${FLUENT_QT_GALLERY_ICNS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    set_target_properties(${FLUENT_QT_GALLERY_EXECUTABLE_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "${FLUENT_QT_GALLERY_DISPLAY_NAME}"
        MACOSX_BUNDLE_BUNDLE_NAME "${FLUENT_QT_GALLERY_DISPLAY_NAME}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${FLUENT_QT_GALLERY_APP_ID}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "${FLUENT_QT_GALLERY_ICON_BASENAME}"
        MACOSX_BUNDLE_INFO_PLIST
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/Info.plist.in"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
endif()

include(GNUInstallDirs)
install(TARGETS ${FLUENT_QT_GALLERY_EXECUTABLE_NAME}
    COMPONENT GalleryRuntime
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# Linux: generate and install the freedesktop launcher from the shared identity.
if(UNIX AND NOT APPLE)
    set(FLUENT_QT_GALLERY_DESKTOP_FILE
        "${CMAKE_CURRENT_BINARY_DIR}/${FLUENT_QT_GALLERY_APP_ID}.desktop")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/app.desktop.in"
        "${FLUENT_QT_GALLERY_DESKTOP_FILE}"
        @ONLY)
    install(FILES "${FLUENT_QT_GALLERY_DESKTOP_FILE}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
        COMPONENT GalleryRuntime)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/app-icon.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/1024x1024/apps"
        RENAME "${FLUENT_QT_GALLERY_APP_ID}.png"
        COMPONENT GalleryRuntime)
endif()

# windeployqt installs Qt itself, but the default shared vcpkg triplet also
# needs the linked spdlog/fmt DLLs beside the executable.
if(WIN32)
    set(_fluent_qt_runtime_dlls "")
    foreach(_fluent_qt_dep spdlog::spdlog fmt::fmt)
        if(TARGET ${_fluent_qt_dep})
            get_target_property(_fluent_qt_dep_type ${_fluent_qt_dep} TYPE)
            if(_fluent_qt_dep_type STREQUAL "SHARED_LIBRARY")
                list(APPEND _fluent_qt_runtime_dlls "$<TARGET_FILE:${_fluent_qt_dep}>")
            endif()
        endif()
    endforeach()
    if(_fluent_qt_runtime_dlls)
        install(FILES ${_fluent_qt_runtime_dlls}
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT GalleryRuntime)
    endif()
endif()
