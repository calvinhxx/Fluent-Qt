function(fluent_qt_enable_sanitizers target)
    if(NOT FLUENT_QT_ENABLE_SANITIZERS)
        return()
    endif()

    if(NOT TARGET "${target}")
        message(FATAL_ERROR
            "fluent_qt_enable_sanitizers called for unknown target '${target}'")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options("${target}" PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer)
        target_link_options("${target}" PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer)
    else()
        message(FATAL_ERROR
            "FLUENT_QT_ENABLE_SANITIZERS currently supports GCC and Clang only "
            "(compiler: ${CMAKE_CXX_COMPILER_ID})")
    endif()
endfunction()
