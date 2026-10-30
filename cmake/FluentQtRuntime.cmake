include_guard(GLOBAL)

# Keep source builds and explicit SDK adoption on the same dependency policy.
# The trailing empty entry permits a release-only package with a generic location.
function(fluent_qt_release_dependency_configurations out_variable)
    set(${out_variable} "Release;RelWithDebInfo;MinSizeRel;None;" PARENT_SCOPE)
endfunction()

function(_fluent_qt_msvc_runtime_for_target target out_variable)
    set(_fluent_qt_runtime "")
    if(MSVC)
        get_property(_fluent_qt_runtime_set TARGET "${target}" PROPERTY MSVC_RUNTIME_LIBRARY SET)
        if(_fluent_qt_runtime_set)
            get_target_property(_fluent_qt_runtime "${target}" MSVC_RUNTIME_LIBRARY)
        else()
            # CMP0091 NEW uses this default when the target has no explicit value.
            set(_fluent_qt_runtime "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        endif()
    endif()
    set(${out_variable} "${_fluent_qt_runtime}" PARENT_SCOPE)
endfunction()

function(_fluent_qt_check_sdk_runtime_scope)
    if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        message(FATAL_ERROR
            "fluent_qt_use_sdk_runtime() must be called by the top-level application; "
            "an embedded project must leave runtime selection to its host")
    endif()
    if(CMAKE_VERSION VERSION_LESS 3.21)
        message(FATAL_ERROR "fluent_qt_use_sdk_runtime() requires CMake 3.21 or newer")
    endif()
    if(NOT DEFINED FluentQt_DEBUG_RELEASE_RUNTIME OR NOT DEFINED FluentQt_MSVC_RUNTIME_LIBRARY)
        message(FATAL_ERROR
            "fluent_qt_use_sdk_runtime() requires runtime metadata from an installed FluentQt SDK; "
            "call find_package(FluentQt CONFIG REQUIRED) first")
    endif()
    get_directory_property(_fluent_qt_existing_directories SUBDIRECTORIES)
    if(_fluent_qt_existing_directories)
        message(FATAL_ERROR
            "Call fluent_qt_use_sdk_runtime() after find_package() and before add_subdirectory() "
            "or creating application targets; existing targets cannot be adapted safely")
    endif()
    get_directory_property(_fluent_qt_existing_targets BUILDSYSTEM_TARGETS)
    foreach(_fluent_qt_target IN LISTS _fluent_qt_existing_targets)
        get_target_property(_fluent_qt_type "${_fluent_qt_target}" TYPE)
        if(_fluent_qt_type MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
            message(FATAL_ERROR
                "Call fluent_qt_use_sdk_runtime() before creating application targets "
                "(already created: ${_fluent_qt_target}); existing targets are not modified")
        endif()
    endforeach()
endfunction()

function(_fluent_qt_map_sdk_dependency_runtime)
    fluent_qt_release_dependency_configurations(_fluent_qt_release_configs)
    get_directory_property(_fluent_qt_imported_targets IMPORTED_TARGETS)
    foreach(_fluent_qt_target IN LISTS _fluent_qt_imported_targets)
        if(_fluent_qt_target MATCHES "^FluentQt::")
            continue()
        endif()
        get_target_property(_fluent_qt_alias "${_fluent_qt_target}" ALIASED_TARGET)
        if(_fluent_qt_alias)
            set(_fluent_qt_target "${_fluent_qt_alias}")
        endif()
        if(_fluent_qt_target MATCHES "^FluentQt::")
            continue()
        endif()
        get_target_property(_fluent_qt_type "${_fluent_qt_target}" TYPE)
        if(NOT _fluent_qt_type STREQUAL "EXECUTABLE")
            set_property(TARGET "${_fluent_qt_target}" PROPERTY
                MAP_IMPORTED_CONFIG_DEBUG "${_fluent_qt_release_configs}")
        endif()
    endforeach()
endfunction()

# Explicitly adopt the installed SDK ABI for one complete, standalone application.
# Call after all binary find_package() calls and before creating compiled targets.
# zh_CN: 由独立应用显式选择 SDK 的 ABI；所有二进制依赖导入后、编译目标创建前调用。
macro(fluent_qt_use_sdk_runtime)
    if(CMAKE_CURRENT_FUNCTION)
        message(FATAL_ERROR
            "Call fluent_qt_use_sdk_runtime() directly in the top-level application directory, "
            "not inside a function; otherwise its runtime selection would not reach later targets")
    endif()
    _fluent_qt_check_sdk_runtime_scope()
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "${FluentQt_MSVC_RUNTIME_LIBRARY}")
    endif()
    if(FluentQt_DEBUG_RELEASE_RUNTIME)
        _fluent_qt_map_sdk_dependency_runtime()
    endif()
endmacro()
