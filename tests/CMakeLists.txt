include(GoogleTest)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Network)
find_package(GTest CONFIG REQUIRED)
find_package(Python3 3.10 COMPONENTS Interpreter QUIET)

set(FLUENT_QT_CI_FAST_TARGETS
    test_qt_compat
    test_qt_test_environment
    test_visual_comparison
)
if(FLUENT_QT_BUILD_GALLERY)
    list(APPEND FLUENT_QT_CI_FAST_TARGETS test_gallery_single_instance)
endif()

set(FLUENT_QT_CI_WINDOWS_PLATFORM_TARGETS
    ${FLUENT_QT_CI_FAST_TARGETS}
    test_window
    test_list_view
    test_flip_view
)

set(FLUENT_QT_CI_FULL_CONTRACT_TARGETS
    test_foundation_contracts
    test_user_theme
    test_anchor_layout
    test_qml_plus
    test_resource_initialization
    test_font_icon
    test_compound_button
    test_split_button_accessibility
    test_transient_accessibility
    test_accordion
    test_card
    test_divider
    test_expander
    test_data_grid
    test_calendar_view
    test_navigation_accessibility
    test_value_accessibility
    test_complex_input_accessibility
    test_auxiliary_surface_accessibility
    test_collection_surface_accessibility
    test_semantic_presentation_accessibility
    test_stack_view
    test_avatar
    test_toast
    test_editing_command_router
    test_command_bar
    test_command_bar_flyout
    test_label
    test_line_edit
    test_password_box
    test_text_edit
    test_onboarding_workbench
)

set(FLUENT_QT_CI_FULL_TARGETS
    ${FLUENT_QT_CI_FAST_TARGETS}
    ${FLUENT_QT_CI_FULL_CONTRACT_TARGETS}
    test_high_dpi
    test_project_logging
    test_fluent_element
    test_button
    test_dropdown_button
    test_combo_box
    test_multi_select_combo_box
    test_list_view
    test_flip_view
    test_calendar_date_picker
    test_popup
    test_scroll_view
    test_navigation_view
    test_window
)
if(FLUENT_QT_BUILD_GALLERY)
    list(APPEND FLUENT_QT_CI_FULL_TARGETS
        test_gallery_content_pages
        test_gallery_preview_cli)
endif()

set(FLUENT_QT_MANUAL_VISUAL_TESTS
    ButtonTest.VisualPropertyVerification
    FluentElementTest.VisualExample
    ScrollBarTest.VisualPropertyVerification
    ToolTipTest.VisualGallery
    TypographyTest.VisualCheck
    SliderTest.VisualSliderGalleryLike
)

set(FLUENT_QT_LOCAL_DESKTOP_TESTS
    MenuBarTest.AccessKeysAndShortcutsRemainInvokable
    MenuBarTest.FluentMenuContentPatternsExposeDeterministicGeometry
    TabViewTest.PointerAddCloseSelectDisabledAndReorderBehavior
    DrawerViewTest.TopLevelResizeKeepsOverlayStackAboveSiblings
    FlowViewTest.DragReorderUsesVariableGeometryAndPreservesSelection
    FlowViewTest.DragReorderWithoutModifierSelectsOnlyDraggedItem
    FlowViewTest.MultiSelectRequiresControlClickAndDragDoesNotRubberBandSelect
    ListViewTest.SelectedIndicatorSettlesAfterDragReorder
    GridViewTest.DragReorderSingleMode
    GridViewTest.DragReorderBoundaryJitterKeepsStableTargetUntilRelease
    GridViewTest.DragReorderClearThresholdCrossingChangesTarget
    GridViewTest.DragDisplacementRepeatedStableMoveKeepsRunningAnimations
    GridViewTest.DragReorderMultipleMode
    GridViewTest.DragReorderSelectedItemsMoveAsGroupInMultipleMode
    GridViewTest.DragReorderExtendedMode
    GridViewTest.DragReorderNoneSelectionDisablesDrag
    GridViewTest.DragReorderDisabledWhenFlagOff
    GridViewTest.DragReorderPreservesSelectionInMultipleMode
    GridViewTest.DragReorderItemReorderedSignalArgs
    GridViewTest.ReorderStandardItemModelTakeRowFallback
    FlyoutTest.TopLevelResizeRepositionsAndRaisesAboveSiblings
    AutoSuggestBoxTest.ClearButtonReceivesMouseInputWhileSuggestionsOpen
    AutoSuggestBoxTest.OutsidePressLightDismissesSuggestions
    WindowTest.WindowsCustomTitleBarSharesNativeCaptionRow
    WindowTest.WindowsCustomChromeSuppressesNativeCaption
    WindowTest.WindowsTitleBarDoubleClickTogglesNativeMaximizeRestore
    WindowTest.WindowsMaximizedCustomChromeFillsAvailableGeometry
    WindowTest.TitleBarAutoSuggestClearButtonWorksWhilePopupOpen
)

define_property(GLOBAL PROPERTY FLUENT_QT_TEST_TARGETS
    BRIEF_DOCS "Qt/GTest targets registered through add_qt_test_module"
    FULL_DOCS "Qt/GTest targets registered through add_qt_test_module"
)
define_property(GLOBAL PROPERTY FLUENT_QT_CI_FULL_TEST_TARGETS
    BRIEF_DOCS "Qt/GTest targets that contain at least one ci_full test"
    FULL_DOCS "Qt/GTest targets that contain at least one ci_full test"
)
define_property(GLOBAL PROPERTY FLUENT_QT_CONTRACT_TEST_TARGETS
    BRIEF_DOCS "Qt/GTest targets that contain at least one contract test"
    FULL_DOCS "Qt/GTest targets that contain at least one contract test"
)

set(FLUENT_QT_QT_GTEST_SUPPORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/support/QtGTestMain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/support/QtTestEnvironment.cpp
)

add_library(FluentQtVisualComparison STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/support/VisualComparison.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/support/VisualComparison.h
)
fluent_qt_configure_cpp_target(FluentQtVisualComparison)
fluent_qt_enable_sanitizers(FluentQtVisualComparison)
target_include_directories(FluentQtVisualComparison
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(FluentQtVisualComparison
    PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)

add_executable(fluent_qt_visual_compare
    ${CMAKE_SOURCE_DIR}/tools/dev/FluentQtVisualCompare.cpp
)
fluent_qt_configure_cpp_target(fluent_qt_visual_compare)
fluent_qt_enable_sanitizers(fluent_qt_visual_compare)
target_link_libraries(fluent_qt_visual_compare
    PRIVATE FluentQtVisualComparison)
set_target_properties(fluent_qt_visual_compare PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/dev"
)
foreach(_fluent_qt_visual_compare_config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${_fluent_qt_visual_compare_config}"
        _fluent_qt_visual_compare_config_upper)
    set_target_properties(fluent_qt_visual_compare PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${_fluent_qt_visual_compare_config_upper}"
        "${CMAKE_BINARY_DIR}/tools/dev")
endforeach()

# 定义一个便捷的宏：添加一个新的 Qt GTest 模块（可在子目录中调用）
# 可执行文件输出到当前 CMake 子目录，便于按目录结构归类。
# 发现到的测试会带有 qt/unit/<目录>/<组件> 标签，便于用 ctest -L 分层筛选。
macro(add_qt_test_module module_name source_file)
    add_executable(${module_name} ${source_file} ${ARGN} ${FLUENT_QT_QT_GTEST_SUPPORT_SOURCES})
    fluent_qt_configure_cpp_target(${module_name})
    fluent_qt_enable_sanitizers(${module_name})
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
        FluentQt::FluentQt
        FluentQtLoggingSupport
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
        FluentQtVisualComparison
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
        FLUENT_QT_VISUAL_BASELINE_DIR="${CMAKE_SOURCE_DIR}/tests/visual-baselines"
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
        set(_fluent_qt_target_has_contract FALSE)

        set_tests_properties(${_fluent_qt_registered_tests} PROPERTIES
            ENVIRONMENT "SKIP_VISUAL_TEST=1"
        )
        foreach(_fluent_qt_test_name IN LISTS _fluent_qt_registered_tests)
            set(_fluent_qt_labels ${_test_labels})
            set(_fluent_qt_is_manual_visual FALSE)
            set(_fluent_qt_is_local_desktop FALSE)
            set(_fluent_qt_is_known_contract_gap FALSE)
            set(_fluent_qt_is_visual_gate FALSE)
            if(_fluent_qt_test_name MATCHES "(^|[._/:-])Contract_")
                list(APPEND _fluent_qt_labels contract)
                set(_fluent_qt_target_has_contract TRUE)
                get_test_property(
                    "${_fluent_qt_test_name}" DISABLED
                    _fluent_qt_contract_test_disabled)
                if(_fluent_qt_contract_test_disabled)
                    set(_fluent_qt_is_known_contract_gap TRUE)
                    list(APPEND _fluent_qt_labels known_contract_gap)
                endif()
            endif()
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
            if(_fluent_qt_test_name MATCHES "(^|[._/:-])VisualGate")
                set(_fluent_qt_is_visual_gate TRUE)
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
            if(_fluent_qt_test_name MATCHES "(^|[._/:-])[^._/:-]*(MacOS|MacOs|macOS|Darwin|Cocoa)[^._/:-]*($|[._/:-])")
                list(APPEND _fluent_qt_labels platform_macos)
            endif()
            if(NOT _fluent_qt_is_manual_visual
               AND NOT _fluent_qt_is_known_contract_gap
               AND NOT _fluent_qt_is_visual_gate)
                list(APPEND _fluent_qt_labels local_full)
            endif()
            if(_fluent_qt_is_ci_full_target
               AND NOT _fluent_qt_is_manual_visual
               AND NOT _fluent_qt_is_local_desktop
               AND NOT _fluent_qt_is_known_contract_gap
               AND NOT _fluent_qt_is_visual_gate)
                list(APPEND _fluent_qt_labels ci_full)
                set(_fluent_qt_target_has_ci_full TRUE)
            endif()
            if(_fluent_qt_is_ci_fast_target
               AND NOT _fluent_qt_is_manual_visual
               AND NOT _fluent_qt_is_local_desktop
               AND NOT _fluent_qt_is_known_contract_gap
               AND NOT _fluent_qt_is_visual_gate)
                list(APPEND _fluent_qt_labels ci_fast)
            endif()
            list(REMOVE_DUPLICATES _fluent_qt_labels)
            set_tests_properties("${_fluent_qt_test_name}" PROPERTIES LABELS "${_fluent_qt_labels}")
        endforeach()
        if(_fluent_qt_target_has_ci_full)
            set_property(GLOBAL APPEND PROPERTY FLUENT_QT_CI_FULL_TEST_TARGETS ${module_name})
        endif()
        if(_fluent_qt_target_has_contract)
            set_property(GLOBAL APPEND PROPERTY FLUENT_QT_CONTRACT_TEST_TARGETS ${module_name})
        endif()
    endif()
endmacro()

# VisualCheck 测试使用 qApp->exec() 阻塞，需手动关闭窗口。
# 直接运行二进制（Qt Creator 中运行）时，可视化测试默认执行。
# ctest 自动化运行时，通过 ENVIRONMENT 属性注入 SKIP_VISUAL_TEST=1，自动跳过。
# 直接运行二进制并设置 VISUAL_SNAPSHOT=1 时，已迁移的 VisualCheck 生成 PNG 后退出。
# 代表性 visual_gate 用 VISUAL_COMPARE=1 对比 tests/visual-baselines/；默认 CTest 仍注入 SKIP_VISUAL_TEST=1。
# 运行方式：
#   ctest --verbose                                              ← 跑全部单元测试，VisualCheck 自动 SKIP
#   ./build/bin/test_xxx                                         ← 启动某个组件的全部测试（含可视化）
#   ./build/bin/test_xxx --gtest_filter="*VisualCheck*"         ← 只跑可视化测试
#   VISUAL_SNAPSHOT=1 ./build/bin/test_xxx --gtest_filter="*VisualCheck*" ← 生成 build/visual/*.png
#   ctest --preset vcpkg-osx -L '^visual_gate$' --output-on-failure ← 3 张基线的 opt-in 像素门禁

# 按源码目录结构添加子目录，Qt Creator 工程树会呈现层级
add_qt_test_module(test_project_logging TestProjectLogging.cpp)
add_qt_test_module(test_qt_test_environment TestQtTestEnvironment.cpp)
add_qt_test_module(test_visual_comparison TestVisualComparison.cpp)
target_compile_definitions(test_visual_comparison PRIVATE
    FLUENT_QT_VISUAL_COMPARE_PATH="$<TARGET_FILE:fluent_qt_visual_compare>")
add_dependencies(test_visual_comparison fluent_qt_visual_compare)
add_qt_test_module(test_high_dpi TestHighDpi.cpp)

if(Python3_Interpreter_FOUND)
    add_test(
        NAME ComponentApi.Contract_Stable
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/quality/validate_component_api.py
            --project-root ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(ComponentApi.Contract_Stable PROPERTIES
        LABELS "quality;contract;local_full"
        TIMEOUT 30
    )

    add_test(
        NAME VisualEvidenceInventory.Contract_Complete
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/quality/validate_visual_evidence_inventory.py
            --project-root ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(VisualEvidenceInventory.Contract_Complete PROPERTIES
        LABELS "quality;contract;local_full"
        TIMEOUT 30
    )

    add_test(
        NAME AccessibilityInventory.Contract_Complete
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/quality/validate_accessibility_inventory.py
            --project-root ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(AccessibilityInventory.Contract_Complete PROPERTIES
        LABELS "quality;contract;ci_fast;ci_full;local_full"
        TIMEOUT 30
    )
endif()

foreach(_fluent_qt_high_dpi_scale 1.1 1.25 1.5 1.75 2 3)
    string(REPLACE "." "" _fluent_qt_high_dpi_percent
        "${_fluent_qt_high_dpi_scale}")
    if(_fluent_qt_high_dpi_scale MATCHES "^[0-9]+$")
        string(APPEND _fluent_qt_high_dpi_percent "00")
    elseif(_fluent_qt_high_dpi_percent MATCHES "^[0-9][0-9]$")
        string(APPEND _fluent_qt_high_dpi_percent "0")
    endif()
    set(_fluent_qt_high_dpi_test
        "HighDpiScaleTest.Scale${_fluent_qt_high_dpi_percent}")
    add_test(NAME "${_fluent_qt_high_dpi_test}"
        COMMAND $<TARGET_FILE:test_high_dpi>
            --gtest_filter=HighDpiTest.*)
    set_tests_properties("${_fluent_qt_high_dpi_test}" PROPERTIES
        ENVIRONMENT
            "SKIP_VISUAL_TEST=1;QT_QPA_PLATFORM=offscreen;QT_FONT_DPI=96;QT_SCALE_FACTOR=${_fluent_qt_high_dpi_scale}"
        LABELS "qt;unit;test_high_dpi;high_dpi;ci_full;local_full"
        TIMEOUT 30)
endforeach()

add_subdirectory(components)
add_subdirectory(onboarding)
if(FLUENT_QT_BUILD_GALLERY)
    add_subdirectory(gallery)
endif()

get_property(FLUENT_QT_ALL_TEST_TARGETS GLOBAL PROPERTY FLUENT_QT_TEST_TARGETS)
get_property(FLUENT_QT_CI_FULL_TEST_TARGETS GLOBAL PROPERTY FLUENT_QT_CI_FULL_TEST_TARGETS)
get_property(FLUENT_QT_CONTRACT_TEST_TARGETS GLOBAL PROPERTY FLUENT_QT_CONTRACT_TEST_TARGETS)
if(FLUENT_QT_ALL_TEST_TARGETS)
    add_custom_target(fluent_qt_ci_fast_tests
        DEPENDS ${FLUENT_QT_CI_FAST_TARGETS}
    )
    add_custom_target(fluent_qt_ci_full_tests
        DEPENDS ${FLUENT_QT_CI_FULL_TEST_TARGETS}
    )
    add_custom_target(fluent_qt_ci_windows_platform_tests
        DEPENDS ${FLUENT_QT_CI_WINDOWS_PLATFORM_TARGETS}
    )
    add_custom_target(fluent_qt_all_tests
        DEPENDS ${FLUENT_QT_ALL_TEST_TARGETS}
    )
    add_custom_target(fluent_qt_contract_tests
        DEPENDS ${FLUENT_QT_CONTRACT_TEST_TARGETS}
    )
endif()
