include(GoogleTest)

set(FLUENT_QT_CI_FAST_TARGETS
    test_qt_compat
    test_qt_test_environment
)

set(FLUENT_QT_CI_FULL_TARGETS
    ${FLUENT_QT_CI_FAST_TARGETS}
    test_project_logging
    test_fluent_element
    test_button
    test_dropdown_button
    test_combo_box
    test_list_view
    test_flip_view
    test_calendar_date_picker
    test_popup
    test_scroll_view
    test_navigation_view
    test_window
)

set(FLUENT_QT_MANUAL_VISUAL_TESTS
    SegoeTest.ComprehensiveCheck
    SliderTest.VisualSliderGalleryLike
)

set(FLUENT_QT_LOCAL_DESKTOP_TESTS
    MenuBarTest.AccessKeysAndShortcutsRemainInvokable
    MenuBarTest.FluentMenuContentPatternsExposeDeterministicGeometry
    TabViewTest.PointerAddCloseSelectDisabledAndReorderBehavior
    DrawerViewTest.TopLevelResizeKeepsOverlayStackAboveSiblings
    FlowViewTest.DragReorderUsesVariableGeometryAndPreservesSelection
    FlowViewTest.DragReorderWithoutModifierSelectsOnlyDraggedItem
    GridViewTest.DragReorderSingleMode
    GridViewTest.DragReorderBoundaryJitterKeepsStableTargetUntilRelease
    GridViewTest.DragReorderClearThresholdCrossingChangesTarget
    GridViewTest.DragDisplacementRepeatedStableMoveKeepsRunningAnimations
    GridViewTest.DragReorderMultipleMode
    GridViewTest.DragReorderSelectedItemsMoveAsGroupInMultipleMode
    GridViewTest.DragReorderExtendedMode
    GridViewTest.DragReorderNoneSelectionDisablesDrag
    GridViewTest.ReorderStandardItemModelTakeRowFallback
    WindowTest.WindowsCustomTitleBarSharesNativeCaptionRow
    WindowTest.WindowsTitleBarDoubleClickTogglesNativeMaximizeRestore
    WindowTest.WindowsMaximizedCustomChromeFillsAvailableGeometry
)

define_property(GLOBAL PROPERTY FLUENT_QT_TEST_TARGETS
    BRIEF_DOCS "Qt/GTest targets registered through add_qt_test_module"
    FULL_DOCS "Qt/GTest targets registered through add_qt_test_module"
)
define_property(GLOBAL PROPERTY FLUENT_QT_CI_FULL_TEST_TARGETS
    BRIEF_DOCS "Qt/GTest targets that contain at least one ci_full test"
    FULL_DOCS "Qt/GTest targets that contain at least one ci_full test"
)

set(FLUENT_QT_QT_GTEST_SUPPORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/support/QtGTestMain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/support/QtTestEnvironment.cpp
)

# 定义一个便捷的宏：添加一个新的 Qt GTest 模块（可在子目录中调用）
# 可执行文件输出到当前 CMake 子目录，便于按目录结构归类。
# 发现到的测试会带有 qt/unit/<目录>/<组件> 标签，便于用 ctest -L 分层筛选。
macro(add_qt_test_module module_name source_file)
    add_executable(${module_name} ${source_file} ${ARGN} ${FLUENT_QT_QT_GTEST_SUPPORT_SOURCES})
    set_property(GLOBAL APPEND PROPERTY FLUENT_QT_TEST_TARGETS ${module_name})
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

    gtest_add_tests(TARGET ${module_name}
        SOURCES ${source_file} ${ARGN}
        TEST_LIST ${module_name}_TESTS
    )

    set(_fluent_qt_registered_tests ${${module_name}_TESTS})
    if(_fluent_qt_registered_tests)
        set(_fluent_qt_is_ci_fast_target FALSE)
        list(FIND FLUENT_QT_CI_FAST_TARGETS "${module_name}" _fluent_qt_ci_fast_target_index)
        if(NOT _fluent_qt_ci_fast_target_index EQUAL -1)
            set(_fluent_qt_is_ci_fast_target TRUE)
        endif()
        set(_fluent_qt_is_ci_full_target FALSE)
        list(FIND FLUENT_QT_CI_FULL_TARGETS "${module_name}" _fluent_qt_ci_full_target_index)
        if(NOT _fluent_qt_ci_full_target_index EQUAL -1)
            set(_fluent_qt_is_ci_full_target TRUE)
        endif()
        set(_fluent_qt_target_has_ci_full FALSE)

        set_tests_properties(${_fluent_qt_registered_tests} PROPERTIES
            ENVIRONMENT "SKIP_VISUAL_TEST=1"
        )
        foreach(_fluent_qt_test_name IN LISTS _fluent_qt_registered_tests)
            set(_fluent_qt_labels ${_test_labels})
            set(_fluent_qt_is_manual_visual FALSE)
            set(_fluent_qt_is_local_desktop FALSE)
            if(_fluent_qt_test_name MATCHES "(^|[._/:-])[^._/:-]*VisualCheck[^._/:-]*($|[._/:-])")
                set(_fluent_qt_is_manual_visual TRUE)
                set(_fluent_qt_is_local_desktop TRUE)
                list(APPEND _fluent_qt_labels visual interactive manual_visual local_desktop)
            endif()
            list(FIND FLUENT_QT_MANUAL_VISUAL_TESTS "${_fluent_qt_test_name}" _fluent_qt_manual_visual_index)
            if(NOT _fluent_qt_manual_visual_index EQUAL -1)
                set(_fluent_qt_is_manual_visual TRUE)
                set(_fluent_qt_is_local_desktop TRUE)
                list(APPEND _fluent_qt_labels visual interactive manual_visual local_desktop)
            endif()
            list(FIND FLUENT_QT_LOCAL_DESKTOP_TESTS "${_fluent_qt_test_name}" _fluent_qt_local_desktop_index)
            if(NOT _fluent_qt_local_desktop_index EQUAL -1)
                set(_fluent_qt_is_local_desktop TRUE)
                list(APPEND _fluent_qt_labels local_desktop)
            endif()
            if(_fluent_qt_test_name MATCHES "(^|[._/:-])[^._/:-]*Interactive[^._/:-]*($|[._/:-])")
                list(APPEND _fluent_qt_labels interactive)
            endif()
            if(_fluent_qt_test_name MATCHES "(^|[._/:-])[^._/:-]*(Animation|Animated|Animations)[^._/:-]*($|[._/:-])")
                list(APPEND _fluent_qt_labels animation)
            endif()
            if(_fluent_qt_test_name MATCHES "(^|[._/:-])[^._/:-]*Slow[^._/:-]*($|[._/:-])")
                list(APPEND _fluent_qt_labels slow)
            endif()
            if(_fluent_qt_test_name MATCHES "(^|[._/:-])[^._/:-]*(Windows|Win32|WinUIWindows)[^._/:-]*($|[._/:-])")
                list(APPEND _fluent_qt_labels platform_windows)
            endif()
            if(_fluent_qt_test_name MATCHES "(^|[._/:-])[^._/:-]*(MacOsRestState|DesignLanguageTest[._/:-].*MacOs)[^._/:-]*($|[._/:-])")
                list(APPEND _fluent_qt_labels design_macos)
            elseif(_fluent_qt_test_name MATCHES "(^|[._/:-])[^._/:-]*(MacOS|MacOs|macOS|Darwin|Cocoa)[^._/:-]*($|[._/:-])")
                list(APPEND _fluent_qt_labels platform_macos)
            endif()
            if(NOT _fluent_qt_is_manual_visual)
                list(APPEND _fluent_qt_labels local_full)
            endif()
            if(_fluent_qt_is_ci_full_target AND NOT _fluent_qt_is_manual_visual AND NOT _fluent_qt_is_local_desktop)
                list(APPEND _fluent_qt_labels ci_full)
                set(_fluent_qt_target_has_ci_full TRUE)
            endif()
            if(_fluent_qt_is_ci_fast_target AND NOT _fluent_qt_is_manual_visual AND NOT _fluent_qt_is_local_desktop)
                list(APPEND _fluent_qt_labels ci_fast)
            endif()
            list(REMOVE_DUPLICATES _fluent_qt_labels)
            set_tests_properties("${_fluent_qt_test_name}" PROPERTIES LABELS "${_fluent_qt_labels}")
        endforeach()
        if(_fluent_qt_target_has_ci_full)
            set_property(GLOBAL APPEND PROPERTY FLUENT_QT_CI_FULL_TEST_TARGETS ${module_name})
        endif()
    endif()
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

get_property(FLUENT_QT_ALL_TEST_TARGETS GLOBAL PROPERTY FLUENT_QT_TEST_TARGETS)
get_property(FLUENT_QT_CI_FULL_TEST_TARGETS GLOBAL PROPERTY FLUENT_QT_CI_FULL_TEST_TARGETS)
if(FLUENT_QT_ALL_TEST_TARGETS)
    add_custom_target(fluent_qt_ci_fast_tests
        DEPENDS ${FLUENT_QT_CI_FAST_TARGETS}
    )
    add_custom_target(fluent_qt_ci_full_tests
        DEPENDS ${FLUENT_QT_CI_FULL_TEST_TARGETS}
    )
    add_custom_target(fluent_qt_all_tests
        DEPENDS ${FLUENT_QT_ALL_TEST_TARGETS}
    )
endif()
