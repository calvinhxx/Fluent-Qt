include(GoogleTest)
include("${PROJECT_SOURCE_DIR}/cmake/FluentQtTestFonts.cmake")
fluentqt_test_font_environment(_fluent_qt_test_font_environment)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Network)
find_package(GTest CONFIG REQUIRED)
find_package(Python3 3.10 COMPONENTS Interpreter QUIET)

set(FLUENT_QT_CI_FAST_TARGETS
    test_qt_compat
    test_qt_test_environment
    test_visual_comparison
)
if(TARGET FluentQtSpatial)
    list(APPEND FLUENT_QT_CI_FAST_TARGETS test_spatial_view test_spatial_texture_backend_fallback)
endif()
if(FLUENT_QT_BUILD_GALLERY)
    list(APPEND FLUENT_QT_CI_FAST_TARGETS test_gallery_single_instance test_interface_zoom)
endif()

set(FLUENT_QT_CI_FULL_CONTRACT_TARGETS
    test_typography
    test_chart_model
    test_chart_view
    test_field
    test_property_panel
    test_foundation_contracts
    test_motion_policy
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
    test_list_detail_view
    test_expander
    test_data_grid
    test_data_grid_tool_bar
    test_calendar_view
    test_date_time_range_picker
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
    test_command_submenus
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
    test_auto_suggest_box
    test_tag_input
    test_tree_select
    test_range_slider
    test_list_view
    test_flip_view
    test_calendar_date_picker
    test_popup
    test_scroll_view
    test_image_viewer
    test_navigation_view
    test_window
)
if(FLUENT_QT_BUILD_GALLERY)
    list(APPEND FLUENT_QT_CI_FULL_TARGETS test_gallery_shell_framework test_gallery_zoom_restart)
endif()
if(TARGET FluentQtSpatial)
    list(APPEND FLUENT_QT_CI_FULL_TARGETS test_spatial_runtime test_spatial_plugin test_particle_layer)
endif()
if(TARGET FluentQtParticles)
    list(APPEND FLUENT_QT_CI_FULL_TARGETS test_particle_backdrop test_particles_plugin)
endif()
if(TARGET FluentQtPlugins)
    list(APPEND FLUENT_QT_CI_FULL_TARGETS test_plugin_manager)
endif()
if(TARGET FluentQtQmlBridge)
    list(APPEND FLUENT_QT_CI_FULL_TARGETS test_qml_bridge)
endif()
if(FLUENT_QT_BUILD_GALLERY)
    list(APPEND FLUENT_QT_CI_FULL_TARGETS
        test_gallery_content_pages
        test_gallery_shell_motion_policy
        test_gallery_preview_cli)
    if(TARGET FluentQtSpatial)
        list(APPEND FLUENT_QT_CI_FULL_TARGETS test_gallery_spatial)
    endif()
endif()

set(FLUENT_QT_MANUAL_VISUAL_TESTS
    FluentElementTest.VisualExample
    ScrollBarTest.VisualPropertyVerification
    ToolTipTest.VisualGallery
    TypographyTest.VisualCheck
)

set(FLUENT_QT_LOCAL_DESKTOP_TESTS
    InterfaceZoomRepaintTest.NativePartialRepaintsMatchACompleteFrame
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
    CodeBlockTest.Contract_WindowsClipboardLockReportsCopyFailure
)

set(FLUENT_QT_PERFORMANCE_TESTS
    GallerySpatialTest.NativeColdActivationProbe
    GallerySpatialTest.AssemblyFramePacingProbe
    GallerySpatialTest.PointerFramePacingProbe
    GallerySpatialTest.StationaryParticleFramePacingProbe
    GalleryThemeAndCodeTest.NativeGalleryInteractionProbe
    GalleryThemeAndCodeTest.NativeGlobalThemeAndSourceExpansionProbe
)

define_property(GLOBAL PROPERTY FLUENT_QT_TEST_TARGETS
    BRIEF_DOCS "Qt/GTest targets registered through add_qt_test_module"
    FULL_DOCS "Qt/GTest targets registered through add_qt_test_module"
)
define_property(GLOBAL PROPERTY FLUENT_QT_TEST_EXECUTABLES
    BRIEF_DOCS "Executables that link one or more add_qt_test_module modules"
    FULL_DOCS "Executables that link one or more add_qt_test_module modules"
)
define_property(GLOBAL PROPERTY FLUENT_QT_CI_FULL_TEST_TARGETS
    BRIEF_DOCS "Qt/GTest targets that contain at least one ci_full test"
    FULL_DOCS "Qt/GTest targets that contain at least one ci_full test"
)
define_property(GLOBAL PROPERTY FLUENT_QT_CONTRACT_TEST_TARGETS
    BRIEF_DOCS "Qt/GTest targets that contain at least one contract test"
    FULL_DOCS "Qt/GTest targets that contain at least one contract test"
)
define_property(GLOBAL PROPERTY FLUENT_QT_CI_WINDOWS_PLATFORM_TARGETS
    BRIEF_DOCS "Qt/GTest targets that contain a platform_windows test"
    FULL_DOCS "Qt/GTest targets selected by the Windows platform test label"
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

add_library(FluentQtTestSupport STATIC
    support/QtGTestMain.cpp
    support/QtTestEnvironment.cpp
    support/VisualGeometryTestUtils.cpp
)
fluent_qt_configure_cpp_target(FluentQtTestSupport)
fluent_qt_enable_sanitizers(FluentQtTestSupport)
target_include_directories(FluentQtTestSupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(FluentQtTestSupport PUBLIC
    FluentQt::FluentQt FluentQtLoggingSupport FluentQtVisualComparison
    Qt${QT_VERSION_MAJOR}::Test GTest::gtest)
target_compile_definitions(FluentQtTestSupport PRIVATE
    FLUENT_QT_TEST_BINARY_DIR="${CMAKE_BINARY_DIR}"
    FLUENT_QT_VISUAL_BASELINE_DIR="${CMAKE_SOURCE_DIR}/tests/visual-baselines")

# add_qt_test_module(<module> <source>... [STANDALONE])
#
# A module is the logical test target. Its name stays a buildable target, its
# discovered tests carry qt/unit/<directory>/<module>/<component> labels, and the
# CI tier lists above select it by name. Its sources compile into the OBJECT
# library <module>_objects with the module's own compile settings; customize
# compilation there. Modules registered in one CMake directory and CI tier
# (ci_fast, ci_full or local) link into one shared executable,
# test_group_<group>_<tier>, so FluentQt is linked once per group instead of
# once per module. CTest still runs each discovered TEST in a fresh process with
# an exact --gtest_filter.
#
# STANDALONE keeps an executable named <module> for sources whose objects or
# definitions conflict with another module of the same group, or whose binary is
# itself a documented direct-run contract. The function sets <module>_TESTS and
# <module>_EXECUTABLE in the caller's scope; use the latter in $<TARGET_FILE:...>.
# Link OBJECT libraries whose objects must reach the executable with
# fluent_qt_link_test_objects().
function(add_qt_test_module module_name source_file)
    cmake_parse_arguments(_fluent_qt_module "STANDALONE" "" "" ${source_file} ${ARGN})
    set(_fluent_qt_module_sources ${_fluent_qt_module_UNPARSED_ARGUMENTS})
    if(NOT _fluent_qt_module_sources)
        message(FATAL_ERROR "add_qt_test_module(${module_name}) needs at least one source")
    endif()

    file(RELATIVE_PATH _test_label_path "${CMAKE_SOURCE_DIR}/tests" "${CMAKE_CURRENT_SOURCE_DIR}")
    if(_test_label_path AND NOT _test_label_path STREQUAL ".")
        get_filename_component(_fluent_qt_test_group "${_test_label_path}" NAME)
    else()
        set(_fluent_qt_test_group root)
    endif()
    if(module_name IN_LIST FLUENT_QT_CI_FAST_TARGETS)
        set(_fluent_qt_test_tier ci_fast)
    elseif(module_name IN_LIST FLUENT_QT_CI_FULL_TARGETS)
        set(_fluent_qt_test_tier ci_full)
    else()
        set(_fluent_qt_test_tier local)
    endif()

    if(_fluent_qt_module_STANDALONE)
        set(_fluent_qt_test_executable ${module_name})
    else()
        set(_fluent_qt_test_executable
            test_group_${_fluent_qt_test_group}_${_fluent_qt_test_tier})
    endif()
    if(NOT TARGET ${_fluent_qt_test_executable})
        # One generated source keeps every generator's target model valid; the
        # tests, their Qt sources and FluentQtTestSupport (with main) arrive
        # through the linked module object libraries. Linking the support
        # library here as well would list it twice on the link line.
        set(_fluent_qt_executable_source
            "${CMAKE_CURRENT_BINARY_DIR}/${_fluent_qt_test_executable}_executable.cpp")
        file(GENERATE OUTPUT "${_fluent_qt_executable_source}" CONTENT
            "// Generated test executable. Module objects are linked by add_qt_test_module.\n")
        add_executable(${_fluent_qt_test_executable} "${_fluent_qt_executable_source}")
        fluent_qt_configure_cpp_target(${_fluent_qt_test_executable})
        fluent_qt_enable_sanitizers(${_fluent_qt_test_executable})
        set_target_properties(${_fluent_qt_test_executable} PROPERTIES
            AUTOMOC OFF
            AUTORCC OFF
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        set_property(GLOBAL APPEND PROPERTY FLUENT_QT_TEST_EXECUTABLES ${_fluent_qt_test_executable})
    else()
        get_target_property(_fluent_qt_executable_dir ${_fluent_qt_test_executable} SOURCE_DIR)
        if(_fluent_qt_module_STANDALONE
           OR NOT _fluent_qt_executable_dir STREQUAL CMAKE_CURRENT_SOURCE_DIR)
            message(FATAL_ERROR
                "add_qt_test_module(${module_name}): target ${_fluent_qt_test_executable} "
                "already exists in ${_fluent_qt_executable_dir}. Test directories need "
                "distinct names, and a STANDALONE module needs a unique target name.")
        endif()
    endif()

    set(_fluent_qt_module_objects ${module_name}_objects)
    add_library(${_fluent_qt_module_objects} OBJECT ${_fluent_qt_module_sources})
    fluent_qt_configure_cpp_target(${_fluent_qt_module_objects})
    fluent_qt_enable_sanitizers(${_fluent_qt_module_objects})
    target_link_libraries(${_fluent_qt_module_objects} PRIVATE FluentQtTestSupport)
    target_include_directories(${_fluent_qt_module_objects} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests/support
    )
    # Snapshot identity follows the module rather than the shared executable.
    target_compile_definitions(${_fluent_qt_module_objects} PRIVATE
        FLUENT_QT_TEST_MODULE="${module_name}")
    target_link_libraries(${_fluent_qt_test_executable} PRIVATE ${_fluent_qt_module_objects})

    if(NOT _fluent_qt_module_STANDALONE)
        add_custom_target(${module_name})
        add_dependencies(${module_name} ${_fluent_qt_test_executable})
        get_target_property(_fluent_qt_module_folder ${_fluent_qt_module_objects} FOLDER)
        if(_fluent_qt_module_folder)
            set_target_properties(${module_name} PROPERTIES FOLDER "${_fluent_qt_module_folder}")
        endif()
    endif()
    set(_fluent_qt_module_source_paths)
    foreach(_fluent_qt_module_source IN LISTS _fluent_qt_module_sources)
        get_filename_component(_fluent_qt_module_source_path "${_fluent_qt_module_source}"
            ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND _fluent_qt_module_source_paths "${_fluent_qt_module_source_path}")
    endforeach()
    set_property(GLOBAL APPEND PROPERTY FLUENT_QT_TEST_TARGETS ${module_name})

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
    set_target_properties(${module_name} PROPERTIES
        LABELS "${_test_labels}"
        FLUENT_QT_TEST_EXECUTABLE ${_fluent_qt_test_executable}
        FLUENT_QT_TEST_SOURCES "${_fluent_qt_module_source_paths}")

    gtest_add_tests(TARGET ${_fluent_qt_test_executable}
        SOURCES ${_fluent_qt_module_sources}
        TEST_LIST _fluent_qt_registered_tests
    )
    set(${module_name}_TESTS ${_fluent_qt_registered_tests} PARENT_SCOPE)
    set(${module_name}_EXECUTABLE ${_fluent_qt_test_executable} PARENT_SCOPE)

    if(_fluent_qt_registered_tests)
        set(_fluent_qt_is_ci_fast_target FALSE)
        if(_fluent_qt_test_tier STREQUAL "ci_fast")
            set(_fluent_qt_is_ci_fast_target TRUE)
        endif()
        set(_fluent_qt_is_ci_full_target FALSE)
        if(module_name IN_LIST FLUENT_QT_CI_FULL_TARGETS)
            set(_fluent_qt_is_ci_full_target TRUE)
        endif()
        set(_fluent_qt_target_has_ci_full FALSE)
        set(_fluent_qt_target_has_contract FALSE)
        set(_fluent_qt_target_has_windows FALSE)

        set_tests_properties(${_fluent_qt_registered_tests} PROPERTIES
            ENVIRONMENT "SKIP_VISUAL_TEST=1"
        )
        if(_fluent_qt_test_font_environment)
            set_property(TEST ${_fluent_qt_registered_tests} APPEND PROPERTY
                ENVIRONMENT "${_fluent_qt_test_font_environment}")
        endif()
        foreach(_fluent_qt_test_name IN LISTS _fluent_qt_registered_tests)
            set(_fluent_qt_labels ${_test_labels})
            set(_fluent_qt_is_manual_visual FALSE)
            set(_fluent_qt_is_local_desktop FALSE)
            set(_fluent_qt_is_known_contract_gap FALSE)
            set(_fluent_qt_is_visual_gate FALSE)
            set(_fluent_qt_is_performance FALSE)
            if(_fluent_qt_test_name IN_LIST FLUENT_QT_PERFORMANCE_TESTS)
                set(_fluent_qt_is_performance TRUE)
                set(_fluent_qt_is_local_desktop TRUE)
                list(APPEND _fluent_qt_labels performance local_desktop)
                set_tests_properties("${_fluent_qt_test_name}" PROPERTIES
                    RUN_SERIAL TRUE
                    TIMEOUT 120)
            endif()
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
                set(_fluent_qt_target_has_windows TRUE)
            endif()
            if(_fluent_qt_test_name MATCHES "(^|[._/:-])[^._/:-]*(MacOS|MacOs|macOS|Darwin|Cocoa)[^._/:-]*($|[._/:-])")
                list(APPEND _fluent_qt_labels platform_macos)
            endif()
            if(NOT _fluent_qt_is_manual_visual
               AND NOT _fluent_qt_is_known_contract_gap
               AND NOT _fluent_qt_is_performance
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
        if(_fluent_qt_target_has_windows)
            set_property(GLOBAL APPEND PROPERTY FLUENT_QT_CI_WINDOWS_PLATFORM_TARGETS ${module_name})
        endif()
    endif()
endfunction()

# CMake links an OBJECT library's object files only into targets that name it
# directly. Use this for objects every consumer must retain, such as static Qt
# resources, so a shared executable receives them exactly once.
function(fluent_qt_link_test_objects module_name)
    get_target_property(_fluent_qt_test_executable ${module_name} FLUENT_QT_TEST_EXECUTABLE)
    if(NOT _fluent_qt_test_executable)
        message(FATAL_ERROR "${module_name} is not registered with add_qt_test_module")
    endif()
    get_property(_fluent_qt_linked_objects TARGET ${_fluent_qt_test_executable}
        PROPERTY FLUENT_QT_LINKED_TEST_OBJECTS)
    foreach(_fluent_qt_objects IN LISTS ARGN)
        if(NOT _fluent_qt_objects IN_LIST _fluent_qt_linked_objects)
            target_link_libraries(${_fluent_qt_test_executable} PRIVATE ${_fluent_qt_objects})
            list(APPEND _fluent_qt_linked_objects ${_fluent_qt_objects})
        endif()
    endforeach()
    set_property(TARGET ${_fluent_qt_test_executable}
        PROPERTY FLUENT_QT_LINKED_TEST_OBJECTS "${_fluent_qt_linked_objects}")
endfunction()

# VisualCheck 测试使用 qApp->exec() 阻塞，需手动关闭窗口。
# 直接运行二进制（Qt Creator 中运行）时，可视化测试默认执行。
# ctest 自动化运行时，通过 ENVIRONMENT 属性注入 SKIP_VISUAL_TEST=1，自动跳过。
# 直接运行二进制并设置 VISUAL_SNAPSHOT=1 时，已迁移的 VisualCheck 生成 PNG 后退出。
# 代表性 visual_gate 用 VISUAL_COMPARE=1 对比 tests/visual-baselines/；默认 CTest 仍注入 SKIP_VISUAL_TEST=1。
# 同目录、同 CI 层级的模块共享一个可执行文件；直接运行时用 --gtest_filter 选择模块的套件。
# `ctest -N -V -R '<Suite>\.<Test>'` 会打印某个用例对应的可执行文件与命令行。
# 运行方式：
#   ctest --verbose                                              ← 跑全部单元测试，VisualCheck 自动 SKIP
#   <group-executable> --gtest_filter="LabelTest.*"             ← 启动某个组件的全部测试（含可视化）
#   <group-executable> --gtest_filter="*VisualCheck*"           ← 只跑可视化测试
#   VISUAL_SNAPSHOT=1 <group-executable> --gtest_filter="*VisualCheck*" ← 生成 build/<preset>/visual/*.png
#   ctest --preset vcpkg-osx -L '^visual_gate$' --output-on-failure ← 3 张基线的 opt-in 像素门禁

# 按源码目录结构添加子目录，Qt Creator 工程树会呈现层级
add_qt_test_module(test_project_logging TestProjectLogging.cpp)
add_qt_test_module(test_qt_test_environment TestQtTestEnvironment.cpp)
target_compile_definitions(test_qt_test_environment_objects PRIVATE
    FLUENT_QT_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}/tests")
add_qt_test_module(test_visual_comparison TestVisualComparison.cpp)
target_compile_definitions(test_visual_comparison_objects PRIVATE
    FLUENT_QT_VISUAL_COMPARE_PATH="$<TARGET_FILE:fluent_qt_visual_compare>")
add_dependencies(test_visual_comparison fluent_qt_visual_compare)
add_qt_test_module(test_high_dpi TestHighDpi.cpp)

if(Python3_Interpreter_FOUND)
    add_test(NAME WslWorkspace.Contract_IncrementalSync
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/dev/test_fluent_qt_wsl.py)
    set_tests_properties(WslWorkspace.Contract_IncrementalSync PROPERTIES
        LABELS "quality;contract;ci_fast;ci_full;local_full" TIMEOUT 60)

    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
        add_test(
            NAME SdkRuntime.Contract_Configuration
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tools/quality/test_sdk_runtime_configuration.py
                --cmake ${CMAKE_COMMAND}
        )
        set_tests_properties(SdkRuntime.Contract_Configuration PROPERTIES
            LABELS "quality;contract;ci_fast;ci_full;local_full"
            TIMEOUT 90
        )
    endif()

    add_test(
        NAME DebugRuntime.Contract_Configuration
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/quality/test_debug_runtime_configuration.py
            --cmake ${CMAKE_COMMAND}
    )
    set_tests_properties(DebugRuntime.Contract_Configuration PROPERTIES
        LABELS "quality;contract;ci_fast;ci_full;local_full"
        TIMEOUT 60
    )

    add_test(
        NAME SpatialPrivateHeaders.Contract_Configuration
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/quality/test_spatial_private_headers.py
            --cmake ${CMAKE_COMMAND}
    )
    set_tests_properties(SpatialPrivateHeaders.Contract_Configuration PROPERTIES
        LABELS "test_spatial_view;quality;contract;ci_fast;ci_full;local_full"
        TIMEOUT 120
    )

    add_test(
        NAME TestFonts.Contract_Configuration
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/quality/test_test_font_environment.py
            --cmake ${CMAKE_COMMAND}
    )
    set_tests_properties(TestFonts.Contract_Configuration PROPERTIES
        LABELS "quality;contract;ci_fast;ci_full;local_full"
        TIMEOUT 60
    )

    add_test(
        NAME CompatibilityBoundary.Contract_Stable
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/quality/check_compatibility_boundaries.py
            --project-root ${CMAKE_SOURCE_DIR} --self-test
    )
    set_tests_properties(CompatibilityBoundary.Contract_Stable PROPERTIES
        LABELS "qt;test_qt_compat;quality;contract;ci_fast;ci_full;local_full"
        TIMEOUT 60
    )

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
        COMMAND $<TARGET_FILE:${test_high_dpi_EXECUTABLE}>
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
get_property(FLUENT_QT_ALL_TEST_EXECUTABLES GLOBAL PROPERTY FLUENT_QT_TEST_EXECUTABLES)

# Shared executables link the test-local code of several modules. Record which
# sources each executable links so the contract below can reject duplicate
# test-local types or inline helpers that the linker would silently merge.
set(_fluent_qt_test_executable_manifest
    "${CMAKE_CURRENT_BINARY_DIR}/fluent_qt_test_executables.tsv")
set(_fluent_qt_test_executable_rows
    "# executable\tmodule\tsource\n")
foreach(_fluent_qt_test_target IN LISTS FLUENT_QT_ALL_TEST_TARGETS)
    get_target_property(_fluent_qt_test_executable ${_fluent_qt_test_target} FLUENT_QT_TEST_EXECUTABLE)
    get_target_property(_fluent_qt_test_sources ${_fluent_qt_test_target} FLUENT_QT_TEST_SOURCES)
    foreach(_fluent_qt_test_source IN LISTS _fluent_qt_test_sources)
        string(APPEND _fluent_qt_test_executable_rows
            "${_fluent_qt_test_executable}\t${_fluent_qt_test_target}\t${_fluent_qt_test_source}\n")
    endforeach()
endforeach()
file(GENERATE OUTPUT "${_fluent_qt_test_executable_manifest}"
    CONTENT "${_fluent_qt_test_executable_rows}")
if(Python3_Interpreter_FOUND)
    add_test(
        NAME TestExecutables.Contract_UniqueModuleSymbols
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/quality/check_test_executable_symbols.py
            --manifest "${_fluent_qt_test_executable_manifest}" --self-test
    )
    set_tests_properties(TestExecutables.Contract_UniqueModuleSymbols PROPERTIES
        LABELS "quality;contract;ci_fast;ci_full;local_full"
        TIMEOUT 60
    )
endif()
get_property(FLUENT_QT_CI_FULL_TEST_TARGETS GLOBAL PROPERTY FLUENT_QT_CI_FULL_TEST_TARGETS)
get_property(FLUENT_QT_CONTRACT_TEST_TARGETS GLOBAL PROPERTY FLUENT_QT_CONTRACT_TEST_TARGETS)
get_property(FLUENT_QT_CI_WINDOWS_PLATFORM_TARGETS GLOBAL PROPERTY FLUENT_QT_CI_WINDOWS_PLATFORM_TARGETS)
list(APPEND FLUENT_QT_CI_WINDOWS_PLATFORM_TARGETS ${FLUENT_QT_CI_FAST_TARGETS})
list(REMOVE_DUPLICATES FLUENT_QT_CI_WINDOWS_PLATFORM_TARGETS)
function(fluent_qt_add_test_aggregate aggregate_name)
    add_custom_target(${aggregate_name})
    if(ARGN)
        add_dependencies(${aggregate_name} ${ARGN})
    endif()
endfunction()

if(FLUENT_QT_ALL_TEST_TARGETS)
    # Category aggregates let PR CI build affected test binaries without a second target catalog.
    # Aggregates depend on module targets, which build their shared executable and
    # any runtime tools the module's tests launch.
    set(_fluent_qt_component_groups
            basicinput
            charts
            collections
            date_time
            dialogs_flyouts
            layout
            menus_toolbars
            navigation
            scrolling
            spatial
            status_info
            textfields
            windowing)
    if(NOT TARGET FluentQtSpatial)
        list(REMOVE_ITEM _fluent_qt_component_groups spatial)
    endif()
    if(TARGET FluentQtParticles)
        list(APPEND _fluent_qt_component_groups particles)
    endif()
    if(TARGET FluentQtQmlBridge)
        list(APPEND _fluent_qt_component_groups qmlbridge)
    endif()
    if(FLUENT_QT_BUILD_GALLERY)
        list(APPEND _fluent_qt_component_groups gallery)
    endif()
    foreach(_fluent_qt_component_group IN LISTS _fluent_qt_component_groups)
        set(_fluent_qt_component_targets)
        foreach(_fluent_qt_test_target IN LISTS FLUENT_QT_ALL_TEST_TARGETS)
            get_target_property(_fluent_qt_target_labels
                ${_fluent_qt_test_target} LABELS)
            if(_fluent_qt_component_group IN_LIST _fluent_qt_target_labels)
                list(APPEND _fluent_qt_component_targets ${_fluent_qt_test_target})
            endif()
        endforeach()
        fluent_qt_add_test_aggregate(fluent_qt_${_fluent_qt_component_group}_tests
            ${_fluent_qt_component_targets})
    endforeach()

    fluent_qt_add_test_aggregate(fluent_qt_ci_fast_tests ${FLUENT_QT_CI_FAST_TARGETS})
    fluent_qt_add_test_aggregate(fluent_qt_ci_full_tests ${FLUENT_QT_CI_FULL_TEST_TARGETS})
    fluent_qt_add_test_aggregate(fluent_qt_ci_windows_platform_tests
        ${FLUENT_QT_CI_WINDOWS_PLATFORM_TARGETS})
    fluent_qt_add_test_aggregate(fluent_qt_all_tests ${FLUENT_QT_ALL_TEST_TARGETS})
    fluent_qt_add_test_aggregate(fluent_qt_contract_tests ${FLUENT_QT_CONTRACT_TEST_TARGETS})
endif()
