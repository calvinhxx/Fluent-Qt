# components 根目录下的测试
add_qt_test_module(test_anchor_layout TestAnchorLayout.cpp)
add_qt_test_module(test_foundation_contracts TestFoundationContracts.cpp)
add_qt_test_module(test_fluent_element TestFluentElement.cpp)
add_qt_test_module(test_user_theme TestUserTheme.cpp)
add_qt_test_module(test_font_icon TestFontIcon.cpp)
add_qt_test_module(test_qml_plus TestQMLPlus.cpp)
add_qt_test_module(test_resource_initialization TestResourceInitialization.cpp)
add_qt_test_module(test_typography TestTypography.cpp DesignTokenProbe.cpp)
add_qt_test_module(test_qt_compat TestQtCompat.cpp)
add_qt_test_module(test_value_accessibility TestValueAccessibility.cpp)
add_qt_test_module(test_complex_input_accessibility TestComplexInputAccessibility.cpp)
add_qt_test_module(test_auxiliary_surface_accessibility TestAuxiliarySurfaceAccessibility.cpp)
add_qt_test_module(test_collection_surface_accessibility TestCollectionSurfaceAccessibility.cpp)
add_qt_test_module(test_semantic_presentation_accessibility TestSemanticPresentationAccessibility.cpp)
add_qt_test_module(test_visual_gate TestVisualGate.cpp)

# Opt-in representative Light/Dark/RTL pixel gate. Not in ci_fast/ci_full/local_full.
# Headless/offscreen skips inside the binary; Linux presets also exclude local_desktop.
add_test(NAME VisualGate.CompareBaselines
    COMMAND $<TARGET_FILE:test_visual_gate>
        --gtest_filter=VisualGateTest.*)
set_tests_properties(VisualGate.CompareBaselines PROPERTIES
    ENVIRONMENT "VISUAL_SNAPSHOT=1;VISUAL_COMPARE=1;QT_SCALE_FACTOR=1;QT_FONT_DPI=96"
    ENVIRONMENT_MODIFICATION "SKIP_VISUAL_TEST=unset:"
    LABELS "qt;unit;test_visual_gate;visual_gate;local_desktop"
    TIMEOUT 90)

add_custom_target(fluent_qt_visual_gate
    COMMAND ${CMAKE_COMMAND} -E env
        --unset=SKIP_VISUAL_TEST
        VISUAL_SNAPSHOT=1
        VISUAL_COMPARE=1
        QT_SCALE_FACTOR=1
        QT_FONT_DPI=96
        $<TARGET_FILE:test_visual_gate>
        --gtest_filter=VisualGateTest.*
    DEPENDS test_visual_gate
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Run the representative Light/Dark/RTL visual gate"
    VERBATIM)

add_subdirectory(basicinput)
add_subdirectory(layout)
add_subdirectory(menus_toolbars)
add_subdirectory(textfields)
add_subdirectory(dialogs_flyouts)
add_subdirectory(status_info)
add_subdirectory(scrolling)
add_subdirectory(collections)
add_subdirectory(navigation)
add_subdirectory(date_time)
add_subdirectory(windowing)
