include(GoogleTest)

set(FLUENT_QT_QT_GTEST_SUPPORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/support/QtGTestMain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/support/QtTestEnvironment.cpp
)

# 定义一个便捷的宏：添加一个新的 Qt GTest 模块（可在子目录中调用）
# 可执行文件输出到当前 CMake 子目录，便于按目录结构归类。
# 发现到的测试会带有 qt/unit/<目录>/<组件> 标签，便于用 ctest -L 分层筛选。
macro(add_qt_test_module module_name source_file)
    add_executable(${module_name} ${source_file} ${ARGN} ${FLUENT_QT_QT_GTEST_SUPPORT_SOURCES})
    get_filename_component(_test_subdir ${source_file} DIRECTORY)
    if(_test_subdir)
        set_target_properties(${module_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${_test_subdir}"
        )
    endif()

    file(RELATIVE_PATH _test_label_path "${CMAKE_SOURCE_DIR}/tests" "${CMAKE_CURRENT_SOURCE_DIR}")
    set(_test_labels qt unit ${module_name})
    string(REGEX REPLACE "^test_" "" _component_label "${module_name}")
    list(APPEND _test_labels "${_component_label}")
    if(_test_label_path AND NOT _test_label_path STREQUAL ".")
        string(REPLACE "/" ";" _test_path_labels "${_test_label_path}")
        list(APPEND _test_labels ${_test_path_labels})
    else()
        list(APPEND _test_labels root)
    endif()
    list(REMOVE_DUPLICATES _test_labels)
    list(JOIN _test_labels ";" _test_labels_joined)
    set(_test_label_file "${CMAKE_CURRENT_BINARY_DIR}/${module_name}_labels.cmake")
    file(WRITE "${_test_label_file}"
        "if(DEFINED ${module_name}_TESTS)\n"
        "    set(_fluent_qt_base_labels \"${_test_labels_joined}\")\n"
        "    foreach(_fluent_qt_test_name IN LISTS ${module_name}_TESTS)\n"
        "        set(_fluent_qt_labels \${_fluent_qt_base_labels})\n"
        "        if(_fluent_qt_test_name MATCHES \"(^|[._/:-])[^._/:-]*VisualCheck[^._/:-]*($|[._/:-])\")\n"
        "            list(APPEND _fluent_qt_labels visual interactive)\n"
        "        endif()\n"
        "        if(_fluent_qt_test_name MATCHES \"(^|[._/:-])[^._/:-]*Interactive[^._/:-]*($|[._/:-])\")\n"
        "            list(APPEND _fluent_qt_labels interactive)\n"
        "        endif()\n"
        "        if(_fluent_qt_test_name MATCHES \"(^|[._/:-])[^._/:-]*(Animation|Animated|Animations)[^._/:-]*($|[._/:-])\")\n"
        "            list(APPEND _fluent_qt_labels animation)\n"
        "        endif()\n"
        "        if(_fluent_qt_test_name MATCHES \"(^|[._/:-])[^._/:-]*Slow[^._/:-]*($|[._/:-])\")\n"
        "            list(APPEND _fluent_qt_labels slow)\n"
        "        endif()\n"
        "        if(_fluent_qt_test_name MATCHES \"(^|[._/:-])[^._/:-]*(Windows|Win32|WinUIWindows)[^._/:-]*($|[._/:-])\")\n"
        "            list(APPEND _fluent_qt_labels platform_windows)\n"
        "        endif()\n"
        "        if(_fluent_qt_test_name MATCHES \"(^|[._/:-])[^._/:-]*(MacOS|MacOs|macOS|Darwin|Cocoa)[^._/:-]*($|[._/:-])\")\n"
        "            list(APPEND _fluent_qt_labels platform_macos)\n"
        "        endif()\n"
        "        list(REMOVE_DUPLICATES _fluent_qt_labels)\n"
        "        set_tests_properties(\"\${_fluent_qt_test_name}\" PROPERTIES LABELS \"\${_fluent_qt_labels}\")\n"
        "    endforeach()\n"
        "endif()\n"
    )

    target_link_libraries(${module_name}
        PRIVATE
        fluent_qt_lib
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Network
        GTest::gtest
    )
    target_include_directories(${module_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests/support
    )
    target_compile_definitions(${module_name} PRIVATE
        FLUENT_QT_TEST_BINARY_DIR="${CMAKE_BINARY_DIR}"
        FLUENT_QT_TEST_TARGET="${module_name}"
    )
    set_target_properties(${module_name} PROPERTIES LABELS "${_test_labels}")
    gtest_discover_tests(${module_name}
        PROPERTIES
            ENVIRONMENT "SKIP_VISUAL_TEST=1"
    )
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${_test_label_file}")
endmacro()

# VisualCheck 测试使用 qApp->exec() 阻塞，需手动关闭窗口。
# 直接运行二进制（Qt Creator 中运行）时，可视化测试默认执行。
# ctest 自动化运行时，通过 ENVIRONMENT 属性注入 SKIP_VISUAL_TEST=1，自动跳过。
# 直接运行二进制并设置 VISUAL_SNAPSHOT=1 时，已迁移的 VisualCheck 生成 PNG 后退出。
# 运行方式：
#   ctest --verbose                                              ← 跑全部单元测试，VisualCheck 自动 SKIP
#   ./build/bin/test_xxx                                         ← 启动某个组件的全部测试（含可视化）
#   ./build/bin/test_xxx --gtest_filter="*VisualCheck*"         ← 只跑可视化测试
#   VISUAL_SNAPSHOT=1 ./build/bin/test_xxx --gtest_filter="*VisualCheck*" ← 生成 build/visual/*.png

# 按源码目录结构添加子目录，Qt Creator 工程树会呈现层级
add_qt_test_module(test_project_logging TestProjectLogging.cpp)
add_qt_test_module(test_qt_test_environment TestQtTestEnvironment.cpp)
add_subdirectory(components)
if(FLUENT_QT_BUILD_GALLERY)
    add_subdirectory(gallery)
endif()
