set(_fluent_qt_workbench_template_root
    "${CMAKE_SOURCE_DIR}/tools/onboarding/starters/cpp-workbench")
set(_fluent_qt_workbench_generated_root
    "${CMAKE_CURRENT_BINARY_DIR}/generated-workbench")

set(APP_NAMESPACE fluentqt_inspector_workbench)
set(APP_NAME_CPP "Inspector Workbench")
set(ACCENT "#2457D6")

set(_fluent_qt_workbench_template_files
    src/application/WorkspaceController.cpp
    src/application/WorkspaceController.h
    src/application/WorkspaceRepository.h
    src/domain/Workspace.h
    src/ui/components/WorkbenchShell.cpp
    src/ui/components/WorkbenchShell.h
    src/ui/pages/WorkspacePage.cpp
    src/ui/pages/WorkspacePage.h
    src/ui/shell/MainWindow.cpp
    src/ui/shell/MainWindow.h
    src/ui/theme/BrandTheme.cpp
    src/ui/theme/BrandTheme.h
)

set(_fluent_qt_workbench_generated_sources)
foreach(_relative_path IN LISTS _fluent_qt_workbench_template_files)
    get_filename_component(_relative_directory "${_relative_path}" DIRECTORY)
    file(MAKE_DIRECTORY
        "${_fluent_qt_workbench_generated_root}/${_relative_directory}")
    configure_file(
        "${_fluent_qt_workbench_template_root}/${_relative_path}.in"
        "${_fluent_qt_workbench_generated_root}/${_relative_path}"
        @ONLY)
    list(APPEND _fluent_qt_workbench_generated_sources
        "${_fluent_qt_workbench_generated_root}/${_relative_path}")
endforeach()

add_qt_test_module(test_onboarding_workbench
    TestGeneratedWorkbenchInspector.cpp
    ${_fluent_qt_workbench_generated_sources})
target_include_directories(test_onboarding_workbench PRIVATE
    "${_fluent_qt_workbench_generated_root}/src")
target_compile_definitions(test_onboarding_workbench PRIVATE
    FLUENT_QT_APPLICATION_SCENES_PATH="${CMAKE_SOURCE_DIR}/docs/ai/evals/application-scenes.json")
