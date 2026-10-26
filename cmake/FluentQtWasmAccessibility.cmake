include_guard(GLOBAL)

function(fluent_qt_configure_wasm_accessibility_overlay overlay_tool overlay_data overlay_project)
    if(NOT EMSCRIPTEN)
        return()
    endif()
    if(NOT Qt6_VERSION VERSION_EQUAL "6.9.3")
        message(FATAL_ERROR "The reviewed Wasm accessibility overlay requires Qt 6.9.3, found ${Qt6_VERSION}")
    endif()
    if(NOT TARGET Qt6::Gui OR NOT TARGET Qt6::QWasmIntegrationPlugin)
        message(FATAL_ERROR "The Qt 6.9.3 Wasm Gui and platform plugin targets are required")
    endif()

    set(_manifest "${overlay_data}/manifest.json")
    file(SHA256 "${_manifest}" _manifest_hash)
    get_target_property(_already_applied Qt6::Gui _fluent_qt_wasm_overlay_manifest)
    if(_already_applied)
        if(NOT _already_applied STREQUAL _manifest_hash)
            message(FATAL_ERROR "Conflicting Qt Wasm accessibility overlays in one build")
        endif()
        return()
    endif()
    get_target_property(_qt_source_id Qt6::Gui _qt_sbom_spdx_repo_document_namespace)
    if(NOT _qt_source_id MATCHES "qtbase-be09b211")
        message(FATAL_ERROR "Qt Gui metadata does not identify the reviewed QtBase revision be09b211")
    endif()
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" --version
        RESULT_VARIABLE _compiler_result OUTPUT_VARIABLE _compiler_version ERROR_VARIABLE _compiler_error)
    if(NOT _compiler_result EQUAL 0 OR NOT _compiler_version MATCHES "3\\.1\\.70([^0-9]|$)")
        message(FATAL_ERROR "The Qt Wasm accessibility overlay requires Emscripten 3.1.70: ${_compiler_version}${_compiler_error}")
    endif()

    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    if(NOT DEFINED FLUENT_QT_WASM_SOURCE_CACHE)
        if(DEFINED ENV{FLUENT_QT_WASM_SOURCE_CACHE} AND NOT "$ENV{FLUENT_QT_WASM_SOURCE_CACHE}" STREQUAL "")
            set(_default_cache "$ENV{FLUENT_QT_WASM_SOURCE_CACHE}")
        else()
            set(_default_cache "${CMAKE_BINARY_DIR}/qt-source-cache")
        endif()
        set(FLUENT_QT_WASM_SOURCE_CACHE "${_default_cache}" CACHE PATH "Verified offline Qt 6.9.3 overlay sources")
    endif()
    get_filename_component(FLUENT_QT_WASM_SOURCE_CACHE "${FLUENT_QT_WASM_SOURCE_CACHE}" ABSOLUTE)
    get_target_property(_original_gui Qt6::Gui IMPORTED_LOCATION_RELEASE)
    get_filename_component(_sdk_lib "${_original_gui}" DIRECTORY)
    get_filename_component(_sdk_root "${_sdk_lib}" DIRECTORY)
    get_filename_component(_sdk "${_sdk_root}" REALPATH)
    # Qt imported targets can be local to sibling directories. The ordinary
    # ExternalProject target is global, so it owns the shared overlay identity.
    if(TARGET fluent_qt_wasm_accessibility_overlay)
        get_target_property(_shared_manifest fluent_qt_wasm_accessibility_overlay _fluent_qt_overlay_manifest)
        get_target_property(_shared_sdk fluent_qt_wasm_accessibility_overlay _fluent_qt_overlay_sdk)
        get_target_property(_archives fluent_qt_wasm_accessibility_overlay _fluent_qt_overlay_archives)
        if(NOT _shared_manifest STREQUAL _manifest_hash OR NOT _shared_sdk STREQUAL _sdk)
            message(FATAL_ERROR "Conflicting Qt Wasm accessibility overlay manifest or SDK in one build")
        endif()
    else()
        file(READ "${_manifest}" _manifest_json)
        set(_verified_inputs "${_manifest}" "${overlay_tool}" "${overlay_project}/CMakeLists.txt")
        foreach(_group files sdk_archives sdk_headers patches)
            string(JSON _input_count LENGTH "${_manifest_json}" "${_group}")
            math(EXPR _last_input "${_input_count} - 1")
            foreach(_index RANGE 0 ${_last_input})
                string(JSON _relative MEMBER "${_manifest_json}" "${_group}" ${_index})
                if(_group STREQUAL "files")
                    list(APPEND _verified_inputs "${FLUENT_QT_WASM_SOURCE_CACHE}/${_relative}")
                elseif(_group STREQUAL "patches")
                    string(JSON _patch_file GET "${_manifest_json}" patches "${_relative}" file)
                    list(APPEND _verified_inputs "${overlay_data}/${_patch_file}")
                else()
                    list(APPEND _verified_inputs "${_sdk}/${_relative}")
                endif()
            endforeach()
        endforeach()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_verified_inputs})
        get_filename_component(_emscripten "${CMAKE_CXX_COMPILER}" DIRECTORY)
        find_program(_fluent_qt_overlay_ar NAMES llvm-ar
            HINTS "${_emscripten}/../bin" NO_DEFAULT_PATH)
        if(NOT _fluent_qt_overlay_ar)
            message(FATAL_ERROR "The active Emscripten SDK must provide llvm-ar beside its compiler")
        endif()
        set(_work "${CMAKE_BINARY_DIR}/fluentqt-wasm-accessibility")
        set(_prepared "${_work}/prepared")
        set(_archives "${_work}/archives")
        execute_process(COMMAND "${Python3_EXECUTABLE}" "${overlay_tool}" --manifest "${_manifest}"
            prepare --cache "${FLUENT_QT_WASM_SOURCE_CACHE}" --sdk "${_sdk}" --output "${_prepared}"
            RESULT_VARIABLE _prepare_result OUTPUT_VARIABLE _prepare_output ERROR_VARIABLE _prepare_error)
        if(NOT _prepare_result EQUAL 0)
            message(FATAL_ERROR
                "Qt Wasm source verification failed: ${_prepare_output}${_prepare_error}\n"
                "Fetch explicitly before configuring: ${Python3_EXECUTABLE} ${overlay_tool} --manifest ${_manifest} fetch --cache ${FLUENT_QT_WASM_SOURCE_CACHE}")
        endif()

        # The sub-build imports the original SDK. Keeping it separate prevents a
        # GuiPrivate -> patched Gui -> overlay object dependency cycle.
        include(ExternalProject)
        ExternalProject_Add(fluent_qt_wasm_accessibility_overlay
            SOURCE_DIR "${overlay_project}"
            BINARY_DIR "${_work}/objects"
            PREFIX "${_work}/steps"
            DOWNLOAD_COMMAND ""
            UPDATE_COMMAND ""
            CMAKE_ARGS
                "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
                "-DQT_HOST_PATH=${QT_HOST_PATH}"
                "-DQT_USE_VCPKG=OFF"
                "-DCMAKE_BUILD_TYPE=Release"
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
                "-DFLUENT_QT_OVERLAY_PREPARED=${_prepared}"
                "-DFLUENT_QT_OVERLAY_SOURCE_CACHE=${FLUENT_QT_WASM_SOURCE_CACHE}"
                "-DFLUENT_QT_OVERLAY_ARCHIVES=${_archives}"
                "-DFLUENT_QT_OVERLAY_TOOL=${overlay_tool}"
                "-DFLUENT_QT_OVERLAY_MANIFEST=${_manifest}"
                "-DFLUENT_QT_OVERLAY_SDK=${_sdk}"
                "-DFLUENT_QT_OVERLAY_PYTHON=${Python3_EXECUTABLE}"
                "-DFLUENT_QT_OVERLAY_AR=${_fluent_qt_overlay_ar}"
            BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --target fluent_qt_overlay_archives --parallel 1
            BUILD_ALWAYS TRUE
            BUILD_BYPRODUCTS "${_archives}/libQt6Gui.a" "${_archives}/libqwasm.a" "${_archives}/overlay.json"
            INSTALL_COMMAND "")
        ExternalProject_Add_StepDependencies(fluent_qt_wasm_accessibility_overlay configure
            "${overlay_project}/CMakeLists.txt" "${overlay_tool}" "${_manifest}"
            "${_prepared}/qaccessiblecache.cpp" "${_prepared}/qaccessiblecache_p.h" "${_prepared}/qwasmaccessibility.cpp")
        set_target_properties(fluent_qt_wasm_accessibility_overlay PROPERTIES
            _fluent_qt_overlay_manifest "${_manifest_hash}"
            _fluent_qt_overlay_sdk "${_sdk}"
            _fluent_qt_overlay_archives "${_archives}")
    endif()
    # Qt copies its Release archive into additional configurations and the
    # configuration-less location. Keep every imported alias on the overlay.
    foreach(_target Qt6::Gui Qt6::QWasmIntegrationPlugin)
        if(_target STREQUAL "Qt6::Gui")
            set(_archive "${_archives}/libQt6Gui.a")
        else()
            set(_archive "${_archives}/libqwasm.a")
        endif()
        set_property(TARGET ${_target} PROPERTY IMPORTED_LOCATION "${_archive}")
        get_property(_configurations TARGET ${_target} PROPERTY IMPORTED_CONFIGURATIONS)
        list(APPEND _configurations RELEASE)
        list(REMOVE_DUPLICATES _configurations)
        foreach(_configuration IN LISTS _configurations)
            string(TOUPPER "${_configuration}" _configuration)
            set_property(TARGET ${_target} PROPERTY IMPORTED_LOCATION_${_configuration} "${_archive}")
        endforeach()
    endforeach()
    add_dependencies(Qt6::Gui fluent_qt_wasm_accessibility_overlay)
    add_dependencies(Qt6::QWasmIntegrationPlugin fluent_qt_wasm_accessibility_overlay)
    set_property(TARGET Qt6::Gui PROPERTY _fluent_qt_wasm_overlay_manifest "${_manifest_hash}")
endfunction()
