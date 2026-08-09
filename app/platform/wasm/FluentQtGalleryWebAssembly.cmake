include_guard(GLOBAL)

function(fluent_qt_configure_gallery_webassembly target source_dir)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR
            "fluent_qt_configure_gallery_webassembly requires Emscripten")
    endif()
    set(_adapter_dir "${source_dir}/platform/wasm")
    target_sources(${target} PRIVATE
        "${_adapter_dir}/GalleryApplication.cpp"
        "${_adapter_dir}/GalleryPlatform.cpp"
        "${_adapter_dir}/WasmSmokeRunner.cpp"
        "${_adapter_dir}/WasmSmokeRunner.h")
    target_link_libraries(${target} PRIVATE FluentQt::WebAssembly)
    set_target_properties(${target} PROPERTIES
        QT_WASM_INITIAL_MEMORY "128MB"
        QT_WASM_MAXIMUM_MEMORY "512MB")

    configure_file("${_adapter_dir}/index.html.in"
        "${CMAKE_CURRENT_BINARY_DIR}/index.html" @ONLY)
    configure_file("${_adapter_dir}/licenses.html.in"
        "${CMAKE_CURRENT_BINARY_DIR}/licenses.html" @ONLY)
    configure_file("${PROJECT_SOURCE_DIR}/LICENSE"
        "${CMAKE_CURRENT_BINARY_DIR}/FluentQt-LICENSE.txt" COPYONLY)
    configure_file("${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "${CMAKE_CURRENT_BINARY_DIR}/THIRD_PARTY_NOTICES.md" COPYONLY)
    configure_file("${PROJECT_SOURCE_DIR}/third_party/fonts/noto-sans-sc/LICENSE.txt"
        "${CMAKE_CURRENT_BINARY_DIR}/NotoSansSC-LICENSE.txt" COPYONLY)
    configure_file("${PROJECT_SOURCE_DIR}/third_party/runtime/qt/LICENSE.txt"
        "${CMAKE_CURRENT_BINARY_DIR}/Qt-LICENSE.txt" COPYONLY)

    get_filename_component(_emscripten_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(_emscripten_license "${_emscripten_dir}/LICENSE")
    if(NOT EXISTS "${_emscripten_license}")
        message(FATAL_ERROR
            "Could not find the Emscripten license beside ${CMAKE_CXX_COMPILER}")
    endif()
    configure_file("${_emscripten_license}"
        "${CMAKE_CURRENT_BINARY_DIR}/Emscripten-LICENSE.txt" COPYONLY)
endfunction()
