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

    if(MSVC)
        target_compile_definitions("${target}" PRIVATE UNICODE _UNICODE)
        # Parallel MSBuild invocations can make multiple cl.exe processes write
        # the same target PDB. Serialize those writes to avoid C1041 failures.
        target_compile_options("${target}" PRIVATE /utf-8 /FS)
        set_target_properties("${target}" PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
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
