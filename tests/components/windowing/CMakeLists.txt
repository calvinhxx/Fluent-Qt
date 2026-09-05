add_qt_test_module(test_window TestWindow.cpp)
target_compile_definitions(test_window PRIVATE PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# Native window transitions can change Spaces or focus. Keep these out of local_full,
# while preserving the standard target/category labels for focused selection.
function(configure_native_window_desktop_test test_name platform)
    get_test_property(${test_name} LABELS _native_window_labels)
    list(REMOVE_ITEM _native_window_labels local_full)
    list(APPEND _native_window_labels local_desktop native_window)
    list(REMOVE_DUPLICATES _native_window_labels)
    set_tests_properties(${test_name} PROPERTIES
        LABELS "${_native_window_labels}"
        ENVIRONMENT "SKIP_VISUAL_TEST=1;QT_QPA_PLATFORM=${platform}"
        RUN_SERIAL TRUE
        TIMEOUT 30)
endfunction()

if(APPLE)
    add_qt_test_module(test_window_mac TestWindowMac.cpp)
    configure_native_window_desktop_test(
        CocoaWindowBackdropTest.PreservesQtContentAndReusesInWindowMaterial cocoa)
endif()
if(WIN32)
    add_qt_test_module(test_window_win TestWindowWin.cpp)
    configure_native_window_desktop_test(
        WindowsWindowBackdropTest.StateChangesDoNotRepeatActivationCompensation windows)
endif()
