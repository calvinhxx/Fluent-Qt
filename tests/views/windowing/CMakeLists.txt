add_qt_test_module(test_window TestWindow.cpp)
target_compile_definitions(test_window PRIVATE PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
