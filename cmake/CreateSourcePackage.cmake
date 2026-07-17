if(NOT DEFINED FLUENT_QT_SOURCE_DIR)
    message(FATAL_ERROR "FLUENT_QT_SOURCE_DIR is required")
endif()
if(NOT DEFINED FLUENT_QT_SOURCE_PACKAGE_DIR)
    message(FATAL_ERROR "FLUENT_QT_SOURCE_PACKAGE_DIR is required")
endif()
if(NOT DEFINED FLUENT_QT_SOURCE_PACKAGE_VERSION)
    message(FATAL_ERROR "FLUENT_QT_SOURCE_PACKAGE_VERSION is required")
endif()

get_filename_component(FLUENT_QT_SOURCE_DIR "${FLUENT_QT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(FLUENT_QT_SOURCE_PACKAGE_DIR
    "${FLUENT_QT_SOURCE_PACKAGE_DIR}" ABSOLUTE)

set(_package_name "FluentQt-${FLUENT_QT_SOURCE_PACKAGE_VERSION}-source")
set(_staging_root "${FLUENT_QT_SOURCE_PACKAGE_DIR}/.source-package")
set(_package_root "${_staging_root}/${_package_name}")
set(_archive "${FLUENT_QT_SOURCE_PACKAGE_DIR}/${_package_name}.zip")

file(REMOVE_RECURSE "${_staging_root}")
file(MAKE_DIRECTORY "${_package_root}/cmake")
file(MAKE_DIRECTORY "${_package_root}/examples")
file(MAKE_DIRECTORY "${_package_root}/tools")

file(COPY
    "${FLUENT_QT_SOURCE_DIR}/include"
    "${FLUENT_QT_SOURCE_DIR}/src"
    "${FLUENT_QT_SOURCE_DIR}/res"
    "${FLUENT_QT_SOURCE_DIR}/third_party"
    DESTINATION "${_package_root}")
file(COPY
    "${FLUENT_QT_SOURCE_DIR}/tools/fonts"
    DESTINATION "${_package_root}/tools")
file(COPY
    "${FLUENT_QT_SOURCE_DIR}/CMakeLists.txt"
    "${FLUENT_QT_SOURCE_DIR}/resources.qrc"
    "${FLUENT_QT_SOURCE_DIR}/LICENSE"
    "${FLUENT_QT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    "${FLUENT_QT_SOURCE_DIR}/TRADEMARKS.md"
    DESTINATION "${_package_root}")
file(COPY
    "${FLUENT_QT_SOURCE_DIR}/cmake/FluentQtConfig.cmake.in"
    "${FLUENT_QT_SOURCE_DIR}/cmake/FluentQtInstallHeaders.cmake"
    "${FLUENT_QT_SOURCE_DIR}/cmake/CreateSourcePackage.cmake"
    "${FLUENT_QT_SOURCE_DIR}/cmake/SourcePackageREADME.md"
    DESTINATION "${_package_root}/cmake")
file(COPY
    "${FLUENT_QT_SOURCE_DIR}/examples/hello_world"
    DESTINATION "${_package_root}/examples")
configure_file(
    "${FLUENT_QT_SOURCE_DIR}/cmake/SourcePackageREADME.md"
    "${_package_root}/README.md"
    COPYONLY)

file(REMOVE "${_archive}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${_archive}" --format=zip "${_package_name}"
    WORKING_DIRECTORY "${_staging_root}"
    RESULT_VARIABLE _archive_result)
if(NOT _archive_result EQUAL 0)
    message(FATAL_ERROR "Could not create ${_archive}")
endif()

file(REMOVE_RECURSE "${_staging_root}")
message(STATUS "Created ${_archive}")
