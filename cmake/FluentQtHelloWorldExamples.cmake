include_guard(GLOBAL)

get_filename_component(_fluent_qt_hello_world_source_dir
    "${CMAKE_CURRENT_LIST_DIR}/../examples/hello_world" ABSOLUTE)
set_property(GLOBAL PROPERTY FLUENT_QT_HELLO_WORLD_SOURCE_DIR
    "${_fluent_qt_hello_world_source_dir}")

# Declare each example in its owning CMake directory while reusing the minimal
# consumer sources and keeping the documented executable paths unchanged.
function(fluent_qt_add_hello_world_example target library window_source runtime_output_dir)
    get_property(_source_dir GLOBAL PROPERTY FLUENT_QT_HELLO_WORLD_SOURCE_DIR)
    get_filename_component(_output_dir "${runtime_output_dir}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

    if(NOT TARGET "${library}")
        if(library STREQUAL "FluentQt::Spatial")
            find_package(FluentQt CONFIG REQUIRED COMPONENTS Spatial)
        else()
            find_package(FluentQt CONFIG REQUIRED)
        endif()
    endif()
    if(EMSCRIPTEN AND NOT TARGET FluentQt::WebAssembly)
        message(FATAL_ERROR
            "The installed FluentQt SDK must enable FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT for this browser example")
    endif()

    set(_sources
        "${_source_dir}/main.cpp"
        "${_source_dir}/HelloWorldApplication.h"
        "${_source_dir}/HelloWorldWindow.h"
        "${_source_dir}/${window_source}")
    if(EMSCRIPTEN)
        list(APPEND _sources "${_source_dir}/platform/wasm/HelloWorldApplication.cpp")
    else()
        list(APPEND _sources "${_source_dir}/platform/desktop/HelloWorldApplication.cpp")
    endif()

    if(COMMAND qt_add_executable)
        qt_add_executable(${target} ${_sources})
    else()
        add_executable(${target} ${_sources})
    endif()
    if(COMMAND fluent_qt_configure_cpp_target)
        fluent_qt_configure_cpp_target(${target})
    else()
        set_target_properties(${target} PROPERTIES
            AUTOMOC ON
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF)
    endif()
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_output_dir}")
    target_link_libraries(${target} PRIVATE ${library})
    target_include_directories(${target} PRIVATE "${_source_dir}")

    if(EMSCRIPTEN)
        target_link_libraries(${target} PRIVATE FluentQt::WebAssembly)
        set_target_properties(${target} PROPERTIES
            QT_WASM_INITIAL_MEMORY "64MB"
            QT_WASM_MAXIMUM_MEMORY "256MB")
        set(FLUENTQT_EXAMPLE_TARGET "${target}")
        set(FLUENTQT_EXAMPLE_LIBRARY "${library}")
        if(target STREQUAL "fluentqt_hello_world_spatial")
            set(FLUENTQT_EXAMPLE_TITLE "FluentQt Spatial Hello World")
            set(_slug "hello-world-spatial")
        else()
            set(FLUENTQT_EXAMPLE_TITLE "FluentQt UILib Hello World")
            set(_slug "hello-world")
        endif()
        set(_destinations "${_output_dir}/${_slug}")
        if(FLUENT_QT_BUILD_GALLERY)
            list(APPEND _destinations "${PROJECT_BINARY_DIR}/app/${_slug}")
            set_property(GLOBAL APPEND PROPERTY FLUENT_QT_GALLERY_EXAMPLE_TARGETS ${target})
            if(target STREQUAL "fluentqt_hello_world_spatial")
                target_compile_definitions(${target} PRIVATE
                    FLUENTQT_HELLO_WORLD_SPATIAL_OBSERVATIONS=1)
            endif()
        endif()
        foreach(_destination IN LISTS _destinations)
            file(MAKE_DIRECTORY "${_destination}")
            configure_file("${_source_dir}/platform/wasm/index.html.in"
                "${_destination}/index.html" @ONLY)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${target}>"
                    "$<TARGET_FILE_DIR:${target}>/${target}.wasm"
                    "$<TARGET_FILE_DIR:${target}>/qtloader.js"
                    "$<TARGET_FILE_DIR:${target}>/qtlogo.svg"
                    "${_destination}"
                COMMENT "Stage ${target} with the shared FluentQt browser shell")
        endforeach()
    endif()
endfunction()
