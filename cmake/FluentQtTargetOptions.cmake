function(fluent_qt_configure_cpp_target target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Unknown FluentQt target: ${target}")
    endif()

    set_target_properties("${target}" PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

    # These generators need explicit target folders. Other IDEs read the real
    # CMake directory tree; virtual folders would create a second hierarchy.
    if(CMAKE_GENERATOR MATCHES "^(Visual Studio|Xcode)")
        get_target_property(_fluent_qt_target_source_dir "${target}" SOURCE_DIR)
        file(RELATIVE_PATH _fluent_qt_module_dir "${PROJECT_SOURCE_DIR}"
            "${_fluent_qt_target_source_dir}")
        if(_fluent_qt_module_dir AND NOT _fluent_qt_module_dir STREQUAL "."
           AND NOT _fluent_qt_module_dir MATCHES "^\\.\\.")
            set_target_properties("${target}" PROPERTIES FOLDER "${_fluent_qt_module_dir}")
        endif()
    endif()

    if(MSVC)
        target_compile_definitions("${target}" PRIVATE
            UNICODE
            _UNICODE
            # Qt 5.15 still instantiates MSVC checked-array iterators internally.
            _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
        # Parallel MSBuild invocations can make multiple cl.exe processes write
        # the same target PDB. Serialize those writes to avoid C1041 failures.
        # Qt 5 moc can also emit unused internal notify-signal probes.
        target_compile_options("${target}" PRIVATE /utf-8 /FS /MP /wd4505)
        # Do not override MSVC_RUNTIME_LIBRARY here. With CMP0091 enabled, each
        # target inherits CMAKE_MSVC_RUNTIME_LIBRARY from the caller/toolchain,
        # so vcpkg's VCPKG_CRT_LINKAGE choice remains effective.
    endif()
endfunction()

function(fluent_qt_enable_project_warnings target)
    if(NOT FLUENT_QT_ENABLE_WARNINGS)
        return()
    endif()

    if(MSVC)
        target_compile_options("${target}" PRIVATE /W4)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options("${target}" PRIVATE -Wall -Wextra -Wpedantic)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            include(CheckCXXCompilerFlag)
            check_cxx_compiler_flag(
                "-Wno-variadic-macro-arguments-omitted"
                FLUENT_QT_HAS_WNO_VARIADIC_MACRO_ARGUMENTS_OMITTED)
            if(FLUENT_QT_HAS_WNO_VARIADIC_MACRO_ARGUMENTS_OMITTED)
                # Qt's stream-style qCWarning(category) API intentionally
                # leaves the variadic macro tail empty in C++17.
                target_compile_options("${target}" PRIVATE
                    -Wno-variadic-macro-arguments-omitted)
            endif()
        endif()
    endif()
endfunction()
