include_guard(GLOBAL)

# Test-only host font configuration. No font binary enters a library or package.
function(fluentqt_test_font_environment output)
    set(_default_font "")
    if(CMAKE_HOST_WIN32)
        foreach(_font_name cour.ttf consola.ttf lucon.ttf)
            if(EXISTS "$ENV{WINDIR}/Fonts/${_font_name}")
                set(_default_font "$ENV{WINDIR}/Fonts/${_font_name}")
                break()
            endif()
        endforeach()
    elseif(CMAKE_HOST_APPLE)
        unset(_macos_font CACHE)
        find_file(_macos_font NAMES Menlo.ttc Monaco.ttf
            PATHS /System/Library/Fonts NO_DEFAULT_PATH)
        if(_macos_font)
            set(_default_font "${_macos_font}")
        endif()
        unset(_macos_font CACHE)
    endif()
    set(FLUENT_QT_TEST_MONOSPACE_FONT "${_default_font}" CACHE FILEPATH
        "Fixed-pitch font for headless test fixtures; auto-detected on Windows and macOS")
    if(CMAKE_HOST_WIN32 AND NOT FLUENT_QT_TEST_MONOSPACE_FONT)
        message(FATAL_ERROR
            "Windows test fixtures require a monospace font. Set FLUENT_QT_TEST_MONOSPACE_FONT "
            "to an existing fixed-pitch font file, or remove an empty cached override "
            "to discover cour.ttf, consola.ttf or lucon.ttf under WINDIR/Fonts.")
    elseif(CMAKE_HOST_APPLE AND NOT FLUENT_QT_TEST_MONOSPACE_FONT)
        message(FATAL_ERROR
            "macOS test fixtures require a monospace font. Set FLUENT_QT_TEST_MONOSPACE_FONT "
            "to an existing fixed-pitch font file, or remove an empty cached override "
            "to discover Menlo.ttc or Monaco.ttf under /System/Library/Fonts.")
    endif()
    set(_environment "")
    if(FLUENT_QT_TEST_MONOSPACE_FONT)
        get_filename_component(_font_path "${FLUENT_QT_TEST_MONOSPACE_FONT}"
            ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
        if(NOT EXISTS "${_font_path}" OR IS_DIRECTORY "${_font_path}")
            message(FATAL_ERROR "FLUENT_QT_TEST_MONOSPACE_FONT is not a font file: ${_font_path}")
        endif()
        file(TO_CMAKE_PATH "${_font_path}" _font_path)
        list(APPEND _environment "FLUENT_QT_TEST_MONOSPACE_FONT=${_font_path}")
    endif()
    set(${output} "${_environment}" PARENT_SCOPE)
endfunction()

# Derive recipients from their registered environment, preserving native tests.
function(fluentqt_apply_headless_test_font_environment environment test_name_pattern)
    if(NOT environment)
        return()
    endif()
    get_directory_property(_tests TESTS)
    foreach(_test IN LISTS _tests)
        if(_test MATCHES "${test_name_pattern}")
            get_property(_current_environment TEST "${_test}" PROPERTY ENVIRONMENT)
            if("QT_QPA_PLATFORM=offscreen" IN_LIST _current_environment
                    OR "QT_QPA_PLATFORM=minimal" IN_LIST _current_environment)
                set_property(TEST "${_test}" APPEND PROPERTY ENVIRONMENT "${environment}")
            endif()
        endif()
    endforeach()
endfunction()
