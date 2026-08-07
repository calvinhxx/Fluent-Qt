include_guard(GLOBAL)

option(FLUENT_QT_BUILD_PYSIDE6_GALLERY
    "Build and package the standalone FluentQt PySide6 Gallery"
    OFF)

if(NOT FLUENT_QT_BUILD_PYSIDE6_GALLERY)
    add_custom_target(fluentqt_pyside6_wheels
        DEPENDS fluentqt_pyside6_wheel)
    return()
endif()

set(FLUENTQT_PYSIDE6_GALLERY_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/gallery/src/fluentqt_gallery")
if(NOT IS_DIRECTORY "${FLUENTQT_PYSIDE6_GALLERY_SOURCE_DIR}")
    message(FATAL_ERROR
        "FLUENT_QT_BUILD_PYSIDE6_GALLERY requires the complete FluentQt "
        "repository. The library-only source package intentionally excludes "
        "the standalone Gallery application.")
endif()
set(FLUENTQT_PYSIDE6_GALLERY_PACKAGE_DIR
    "${PROJECT_BINARY_DIR}/python/fluentqt_gallery")
set(FLUENTQT_PYSIDE6_GALLERY_FILES
    __init__.py
    __main__.py
    application_controller.py
    app.py
    catalog.py
    foundation_pages.py
    identity.py
    intro_tour.py
    metrics.py
    native_samples.py
    native_samples_basic.py
    native_samples_collections.py
    native_samples_dialogs.py
    native_samples_navigation.py
    native_samples_scrolling.py
    native_samples_status.py
    native_samples_text_window.py
    samples.py
    settings.py
    single_instance.py
    update_checker.py
    visual.py
    window.py
    window_placement.py)

file(MAKE_DIRECTORY "${FLUENTQT_PYSIDE6_GALLERY_PACKAGE_DIR}")
set(FLUENTQT_PYSIDE6_GALLERY_SOURCE_PATHS)
foreach(FLUENTQT_PYSIDE6_GALLERY_FILE
        IN LISTS FLUENTQT_PYSIDE6_GALLERY_FILES)
    set(FLUENTQT_PYSIDE6_GALLERY_SOURCE
        "${FLUENTQT_PYSIDE6_GALLERY_SOURCE_DIR}/${FLUENTQT_PYSIDE6_GALLERY_FILE}")
    list(APPEND FLUENTQT_PYSIDE6_GALLERY_SOURCE_PATHS
        "${FLUENTQT_PYSIDE6_GALLERY_SOURCE}")
    configure_file(
        "${FLUENTQT_PYSIDE6_GALLERY_SOURCE}"
        "${FLUENTQT_PYSIDE6_GALLERY_PACKAGE_DIR}/${FLUENTQT_PYSIDE6_GALLERY_FILE}"
        COPYONLY)
endforeach()

# The standalone Gallery owns its application artwork.  The native C++ app is
# still the canonical source so parity assets cannot silently diverge.
set(FLUENTQT_PYSIDE6_GALLERY_ASSET_SOURCE_DIR
    "${PROJECT_SOURCE_DIR}/app/assets")
set(FLUENTQT_PYSIDE6_GALLERY_ASSET_PACKAGE_DIR
    "${FLUENTQT_PYSIDE6_GALLERY_PACKAGE_DIR}/assets")
file(GLOB_RECURSE FLUENTQT_PYSIDE6_GALLERY_ASSET_FILES
    CONFIGURE_DEPENDS
    "${FLUENTQT_PYSIDE6_GALLERY_ASSET_SOURCE_DIR}/control_images/*.png"
    "${FLUENTQT_PYSIDE6_GALLERY_ASSET_SOURCE_DIR}/home_header_tiles/*.png")
list(APPEND FLUENTQT_PYSIDE6_GALLERY_ASSET_FILES
    "${FLUENTQT_PYSIDE6_GALLERY_ASSET_SOURCE_DIR}/app-icon.png")
foreach(FLUENTQT_PYSIDE6_GALLERY_ASSET
        IN LISTS FLUENTQT_PYSIDE6_GALLERY_ASSET_FILES)
    file(RELATIVE_PATH FLUENTQT_PYSIDE6_GALLERY_ASSET_RELATIVE
        "${FLUENTQT_PYSIDE6_GALLERY_ASSET_SOURCE_DIR}"
        "${FLUENTQT_PYSIDE6_GALLERY_ASSET}")
    get_filename_component(FLUENTQT_PYSIDE6_GALLERY_ASSET_RELATIVE_DIR
        "${FLUENTQT_PYSIDE6_GALLERY_ASSET_RELATIVE}" DIRECTORY)
    file(MAKE_DIRECTORY
        "${FLUENTQT_PYSIDE6_GALLERY_ASSET_PACKAGE_DIR}/${FLUENTQT_PYSIDE6_GALLERY_ASSET_RELATIVE_DIR}")
    configure_file(
        "${FLUENTQT_PYSIDE6_GALLERY_ASSET}"
        "${FLUENTQT_PYSIDE6_GALLERY_ASSET_PACKAGE_DIR}/${FLUENTQT_PYSIDE6_GALLERY_ASSET_RELATIVE}"
        COPYONLY)
endforeach()

set(FLUENTQT_PYSIDE6_GALLERY_ICON_CATALOG
    "${PROJECT_SOURCE_DIR}/res/icons/FluentQtIcons.json")
set(FLUENTQT_PYSIDE6_GALLERY_ICON_ALIASES
    "${PROJECT_SOURCE_DIR}/res/icons/FluentQtIconAliases.json")
configure_file(
    "${FLUENTQT_PYSIDE6_GALLERY_ICON_CATALOG}"
    "${FLUENTQT_PYSIDE6_GALLERY_ASSET_PACKAGE_DIR}/icon_catalog.json"
    COPYONLY)
configure_file(
    "${FLUENTQT_PYSIDE6_GALLERY_ICON_ALIASES}"
    "${FLUENTQT_PYSIDE6_GALLERY_ASSET_PACKAGE_DIR}/icon_aliases.json"
    COPYONLY)

file(GLOB FLUENTQT_PYSIDE6_NATIVE_GALLERY_SAMPLE_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/app/view/widgets/samples/*Samples.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/app/model/GalleryComponentCatalog.cpp"
    "${PROJECT_SOURCE_DIR}/app/model/GalleryContentCatalog.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/api-manifest.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/gallery/tools/generate_gallery_contract.py")
set(FLUENTQT_PYSIDE6_GALLERY_CONTRACT
    "${FLUENTQT_PYSIDE6_GALLERY_PACKAGE_DIR}/contract.json")
execute_process(
    COMMAND "${Python_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/gallery/tools/generate_gallery_contract.py"
        --project-root "${PROJECT_SOURCE_DIR}"
        --output "${FLUENTQT_PYSIDE6_GALLERY_CONTRACT}"
    RESULT_VARIABLE FLUENTQT_PYSIDE6_GALLERY_CONTRACT_RESULT
    ERROR_VARIABLE FLUENTQT_PYSIDE6_GALLERY_CONTRACT_ERROR)
if(NOT FLUENTQT_PYSIDE6_GALLERY_CONTRACT_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Unable to generate the standalone PySide6 Gallery contract:\n"
        "${FLUENTQT_PYSIDE6_GALLERY_CONTRACT_ERROR}")
endif()
list(APPEND FLUENTQT_PYSIDE6_GALLERY_SOURCE_PATHS
    ${FLUENTQT_PYSIDE6_GALLERY_ASSET_FILES}
    "${FLUENTQT_PYSIDE6_GALLERY_ICON_CATALOG}"
    "${FLUENTQT_PYSIDE6_GALLERY_ICON_ALIASES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/gallery/tools/generate_gallery_contract.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/api-manifest.json"
    "${PROJECT_SOURCE_DIR}/app/model/GalleryComponentCatalog.cpp"
    "${PROJECT_SOURCE_DIR}/app/model/GalleryContentCatalog.cpp"
    ${FLUENTQT_PYSIDE6_NATIVE_GALLERY_SAMPLE_SOURCES})

foreach(FLUENTQT_PYSIDE6_GALLERY_FILE
        IN LISTS FLUENTQT_PYSIDE6_GALLERY_FILES)
    install(FILES
        "${FLUENTQT_PYSIDE6_GALLERY_SOURCE_DIR}/${FLUENTQT_PYSIDE6_GALLERY_FILE}"
        DESTINATION fluentqt_gallery
        COMPONENT FluentQtPySide6Gallery
        EXCLUDE_FROM_ALL)
endforeach()
install(FILES
    "${FLUENTQT_PYSIDE6_GALLERY_ASSET_SOURCE_DIR}/app-icon.png"
    DESTINATION fluentqt_gallery/assets
    COMPONENT FluentQtPySide6Gallery
    EXCLUDE_FROM_ALL)
install(FILES "${FLUENTQT_PYSIDE6_GALLERY_ICON_CATALOG}"
    DESTINATION fluentqt_gallery/assets
    RENAME icon_catalog.json
    COMPONENT FluentQtPySide6Gallery
    EXCLUDE_FROM_ALL)
install(FILES "${FLUENTQT_PYSIDE6_GALLERY_ICON_ALIASES}"
    DESTINATION fluentqt_gallery/assets
    RENAME icon_aliases.json
    COMPONENT FluentQtPySide6Gallery
    EXCLUDE_FROM_ALL)
install(DIRECTORY
    "${FLUENTQT_PYSIDE6_GALLERY_ASSET_SOURCE_DIR}/control_images"
    "${FLUENTQT_PYSIDE6_GALLERY_ASSET_SOURCE_DIR}/home_header_tiles"
    DESTINATION fluentqt_gallery/assets
    COMPONENT FluentQtPySide6Gallery
    EXCLUDE_FROM_ALL
    FILES_MATCHING PATTERN "*.png")
install(FILES "${FLUENTQT_PYSIDE6_GALLERY_CONTRACT}"
    DESTINATION fluentqt_gallery
    COMPONENT FluentQtPySide6Gallery
    EXCLUDE_FROM_ALL)

set(FLUENTQT_PYSIDE6_GALLERY_WHEEL_ROOT
    "${PROJECT_BINARY_DIR}/pyside6-gallery-wheel")
set(FLUENTQT_PYSIDE6_GALLERY_WHEEL_STAGE
    "${FLUENTQT_PYSIDE6_GALLERY_WHEEL_ROOT}/staging")
set(FLUENTQT_PYSIDE6_GALLERY_WHEELHOUSE
    "${PROJECT_BINARY_DIR}/gallery-wheelhouse")
add_custom_target(fluentqt_pyside6_gallery_wheel
    COMMAND "${CMAKE_COMMAND}" -E remove_directory
        "${FLUENTQT_PYSIDE6_GALLERY_WHEEL_STAGE}"
    COMMAND "${CMAKE_COMMAND}" -E remove_directory
        "${FLUENTQT_PYSIDE6_GALLERY_WHEELHOUSE}"
    COMMAND "${CMAKE_COMMAND}" --install "${PROJECT_BINARY_DIR}"
        --prefix "${FLUENTQT_PYSIDE6_GALLERY_WHEEL_STAGE}"
        --component FluentQtPySide6Gallery
        --config "$<CONFIG>"
    COMMAND "${Python_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/gallery/tools/build_gallery_wheel.py"
        --package-dir
            "${FLUENTQT_PYSIDE6_GALLERY_WHEEL_STAGE}/fluentqt_gallery"
        --output-dir "${FLUENTQT_PYSIDE6_GALLERY_WHEELHOUSE}"
        --version "${PROJECT_VERSION}"
        --requires-python "${FLUENT_QT_PYSIDE6_REQUIRES_PYTHON}"
        --license-file "${PROJECT_SOURCE_DIR}/LICENSE"
        --license-file "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        --license-file "${PROJECT_SOURCE_DIR}/TRADEMARKS.md"
    DEPENDS
        ${FLUENTQT_PYSIDE6_GALLERY_SOURCE_PATHS}
        "${CMAKE_CURRENT_SOURCE_DIR}/gallery/tools/build_gallery_wheel.py"
        "${PROJECT_SOURCE_DIR}/LICENSE"
        "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "${PROJECT_SOURCE_DIR}/TRADEMARKS.md"
    COMMENT "Building standalone FluentQt PySide6 Gallery wheel"
    USES_TERMINAL
    VERBATIM)

add_custom_target(fluentqt_pyside6_wheels
    DEPENDS
        fluentqt_pyside6_wheel
        fluentqt_pyside6_gallery_wheel)
