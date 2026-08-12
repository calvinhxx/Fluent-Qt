if(NOT QT_VERSION_MAJOR EQUAL 6)
    message(FATAL_ERROR
        "FLUENT_QT_BUILD_PYSIDE6_BINDINGS requires Qt 6.2 or newer")
endif()

if(WIN32)
    # Python::Python is required by the Windows extension link step.
    find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development)
else()
    # Unix extension modules do not embed Python. Requiring Development.Embed
    # rejects the intentionally minimal CPython installations in manylinux.
    find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development.Module)
endif()

if(Python_VERSION VERSION_LESS 3.11)
    set(FLUENTQT_PYSIDE6_DEFAULT_REQUIRES_PYTHON ">=3.10,<3.11")
else()
    set(FLUENTQT_PYSIDE6_DEFAULT_REQUIRES_PYTHON ">=3.11,<3.14")
endif()
set(FLUENT_QT_PYSIDE6_REQUIRES_PYTHON
    "${FLUENTQT_PYSIDE6_DEFAULT_REQUIRES_PYTHON}"
    CACHE STRING
    "Requires-Python metadata for generated FluentQt wheels")
if(NOT FLUENT_QT_PYSIDE6_REQUIRES_PYTHON STREQUAL ">=3.10,<3.11"
   AND NOT FLUENT_QT_PYSIDE6_REQUIRES_PYTHON STREQUAL ">=3.11,<3.14")
    message(FATAL_ERROR
        "FLUENT_QT_PYSIDE6_REQUIRES_PYTHON must be either the non-published "
        "Qt 6.2 compatibility policy '>=3.10,<3.11' or the official release "
        "policy '>=3.11,<3.14'")
endif()
if(FLUENT_QT_PYSIDE6_REQUIRES_PYTHON STREQUAL ">=3.10,<3.11"
   AND (Python_VERSION VERSION_LESS 3.10
        OR NOT Python_VERSION VERSION_LESS 3.11))
    message(FATAL_ERROR
        "The non-published Qt 6.2 compatibility artifact requires CPython "
        "3.10, found ${Python_VERSION}")
endif()
if(FLUENT_QT_PYSIDE6_REQUIRES_PYTHON STREQUAL ">=3.11,<3.14"
   AND (Python_VERSION VERSION_LESS 3.11
        OR NOT Python_VERSION VERSION_LESS 3.14))
    message(FATAL_ERROR
        "Official FluentQt wheels currently support CPython 3.11 through "
        "3.13, found ${Python_VERSION}")
endif()

execute_process(
    COMMAND "${Python_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/QueryPySide6.py"
    RESULT_VARIABLE FLUENTQT_PYSIDE6_QUERY_RESULT
    OUTPUT_VARIABLE FLUENTQT_PYSIDE6_QUERY_OUTPUT
    ERROR_VARIABLE FLUENTQT_PYSIDE6_QUERY_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT FLUENTQT_PYSIDE6_QUERY_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Unable to discover PySide6/Shiboken6 for ${Python_EXECUTABLE}:\n"
        "${FLUENTQT_PYSIDE6_QUERY_ERROR}")
endif()

set(FLUENTQT_PYSIDE6_QUERY_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/FluentQtPySide6Query.cmake")
file(WRITE "${FLUENTQT_PYSIDE6_QUERY_FILE}"
    "${FLUENTQT_PYSIDE6_QUERY_OUTPUT}\n")
include("${FLUENTQT_PYSIDE6_QUERY_FILE}")

foreach(FLUENTQT_PYSIDE6_REQUIRED_VERSION
        FLUENTQT_PYSIDE6_VERSION
        FLUENTQT_PYSIDE6_QT_VERSION
        FLUENTQT_SHIBOKEN6_VERSION
        FLUENTQT_SHIBOKEN6_GENERATOR_VERSION)
    if(${FLUENTQT_PYSIDE6_REQUIRED_VERSION} VERSION_LESS 6.2)
        message(FATAL_ERROR
            "${FLUENTQT_PYSIDE6_REQUIRED_VERSION} must be 6.2 or newer "
            "(found ${${FLUENTQT_PYSIDE6_REQUIRED_VERSION}})")
    endif()
endforeach()

if(NOT FLUENTQT_PYSIDE6_VERSION VERSION_EQUAL
       FLUENTQT_SHIBOKEN6_VERSION
   OR NOT FLUENTQT_PYSIDE6_VERSION VERSION_EQUAL
       FLUENTQT_SHIBOKEN6_GENERATOR_VERSION)
    message(FATAL_ERROR
        "PySide6, shiboken6, and shiboken6_generator must have the same "
        "version (found ${FLUENTQT_PYSIDE6_VERSION}, "
        "${FLUENTQT_SHIBOKEN6_VERSION}, and "
        "${FLUENTQT_SHIBOKEN6_GENERATOR_VERSION})")
endif()

if(NOT FLUENTQT_PYSIDE6_SAFE_NONE_RETURNS)
    message(FATAL_ERROR
        "The active PySide6/Shiboken6 runtime returns borrowed Py_None from "
        "wrapped void functions. This ABI combination is unsafe. On Linux "
        "ARM64 with PySide6 6.9+, use CPython 3.12 or newer.")
endif()

if(NOT Qt6_VERSION VERSION_EQUAL FLUENTQT_PYSIDE6_QT_VERSION)
    message(FATAL_ERROR
        "The C++ Qt SDK and PySide6 Qt runtime must have the same version "
        "(found Qt ${Qt6_VERSION} and PySide6 Qt "
        "${FLUENTQT_PYSIDE6_QT_VERSION})")
endif()

message(STATUS
    "Building PySide6 bindings with Python ${Python_VERSION}, "
    "PySide6 ${FLUENTQT_PYSIDE6_VERSION}, Qt ${Qt6_VERSION}, and "
    "Requires-Python ${FLUENT_QT_PYSIDE6_REQUIRES_PYTHON}")

set(FLUENTQT_PYSIDE6_GENERATED_ROOT
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(FLUENTQT_PYSIDE6_GENERATED_DIR
    "${FLUENTQT_PYSIDE6_GENERATED_ROOT}/_fluentqt")
set(FLUENTQT_PYSIDE6_GENERATED_SOURCES
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/_fluentqt_module_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_binding_anchorlayout_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_binding_anchorspec_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_binding_fluentwidget_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_binding_stategroup_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_button_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_checkbox_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_colorpicker_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_combobox_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_compoundbutton_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_dropdownbutton_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_hyperlinkbutton_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_radiobutton_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_ratingcontrol_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_repeatbutton_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_slider_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_splitbutton_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_togglebutton_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_togglesplitbutton_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_basicinput_toggleswitch_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_collections_drawerview_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_collections_flipview_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_collections_flowview_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_collections_gridview_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_collections_listview_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_collections_splitview_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_collections_splitviewpaneoptions_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_collections_stackview_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_collections_treeview_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_date_time_calendardatepicker_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_date_time_calendarview_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_date_time_datepicker_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_date_time_timepicker_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_dialogs_flyouts_coachmark_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_dialogs_flyouts_contentdialog_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_dialogs_flyouts_dialog_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_dialogs_flyouts_flyout_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_dialogs_flyouts_popup_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_dialogs_flyouts_teachingtip_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_fonticon_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_layout_accordion_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_layout_card_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_layout_divider_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_layout_expander_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_menus_toolbars_commandbar_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_menus_toolbars_commandbarflyout_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_menus_toolbars_fluentmenu_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_menus_toolbars_fluentmenuitem_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_menus_toolbars_fluentmenubar_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_navigation_breadcrumb_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_navigation_breadcrumbitem_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_navigation_navigationview_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_navigation_pivot_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_navigation_pivotitem_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_navigation_selectorbar_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_navigation_selectorbaritem_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_navigation_stackcontenthost_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_navigation_tabview_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_navigation_tabviewitem_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_scrolling_annotatedscrollbar_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_scrolling_annotatedscrollbarlabel_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_scrolling_pipspager_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_scrolling_scrollbar_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_scrolling_scrollview_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_binding_scrollviewzoomawarewidget_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_textfields_autosuggestbox_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_textfields_editingcommandrouter_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_textfields_label_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_textfields_lineedit_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_textfields_numberbox_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_textfields_passwordbox_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_textfields_textedit_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_status_info_avatar_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_status_info_infobadge_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_status_info_infobar_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_status_info_progressbar_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_status_info_progressring_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_status_info_shimmer_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_status_info_toast_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_status_info_tooltip_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_windowing_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_windowing_backdropcapabilities_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_windowing_backdropstate_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_windowing_titlebar_wrapper.cpp"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}/fluent_windowing_window_wrapper.cpp")

set(FLUENTQT_PYSIDE6_GENERATOR_INCLUDE_ARGUMENTS
    "-I${CMAKE_CURRENT_SOURCE_DIR}/native"
    "-I${PROJECT_SOURCE_DIR}/src"
    "-I${PROJECT_SOURCE_DIR}/include")
foreach(FLUENTQT_PYSIDE6_QT_TARGET Qt6::Core Qt6::Gui Qt6::Widgets)
    get_target_property(FLUENTQT_PYSIDE6_QT_INCLUDE_DIRS
        ${FLUENTQT_PYSIDE6_QT_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    foreach(FLUENTQT_PYSIDE6_QT_INCLUDE_DIR
            IN LISTS FLUENTQT_PYSIDE6_QT_INCLUDE_DIRS)
        if(NOT FLUENTQT_PYSIDE6_QT_INCLUDE_DIR MATCHES "^\\$<")
            list(APPEND FLUENTQT_PYSIDE6_GENERATOR_INCLUDE_ARGUMENTS
                "-I${FLUENTQT_PYSIDE6_QT_INCLUDE_DIR}")
        endif()
    endforeach()
endforeach()
list(REMOVE_DUPLICATES FLUENTQT_PYSIDE6_GENERATOR_INCLUDE_ARGUMENTS)

set(FLUENTQT_PYSIDE6_GENERATOR_ARGUMENTS
    --generator-set=shiboken
    --avoid-protected-hack
    --enable-pyside-extensions
    --enable-parent-ctor-heuristic
    --enable-return-value-heuristic
    --use-global-header
    --language-level=c++17
    --clang-option=-DFLUENTQT_PYSIDE6_GENERATOR
    "${FLUENTQT_PYSIDE6_GENERATOR_INCLUDE_ARGUMENTS}"
    "-T${FLUENTQT_PYSIDE6_TYPESYSTEM_DIR}"
    "--output-directory=${FLUENTQT_PYSIDE6_GENERATED_ROOT}")
if(APPLE)
    get_filename_component(FLUENTQT_PYSIDE6_QT_FRAMEWORK_DIR
        "${Qt6_DIR}/../.." ABSOLUTE)
    list(APPEND FLUENTQT_PYSIDE6_GENERATOR_ARGUMENTS
        "-F${FLUENTQT_PYSIDE6_QT_FRAMEWORK_DIR}")
endif()

file(GLOB_RECURSE FLUENTQT_PYSIDE6_INPUT_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/FluentQt/*.h"
    "${PROJECT_SOURCE_DIR}/src/*.h")
set(FLUENTQT_PYSIDE6_GENERATION_STAMP
    "${FLUENTQT_PYSIDE6_GENERATED_ROOT}/generation.stamp")
add_custom_command(
    OUTPUT "${FLUENTQT_PYSIDE6_GENERATION_STAMP}"
    BYPRODUCTS ${FLUENTQT_PYSIDE6_GENERATED_SOURCES}
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${FLUENTQT_PYSIDE6_GENERATED_ROOT}"
    COMMAND "${FLUENTQT_SHIBOKEN6_GENERATOR}"
        ${FLUENTQT_PYSIDE6_GENERATOR_ARGUMENTS}
        "${CMAKE_CURRENT_SOURCE_DIR}/native/BindingApi.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/native/typesystem_fluentqt.xml"
    COMMAND "${CMAKE_COMMAND}" -E touch
        "${FLUENTQT_PYSIDE6_GENERATION_STAMP}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/native/BindingApi.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/native/typesystem_fluentqt.xml"
        ${FLUENTQT_PYSIDE6_INPUT_HEADERS}
    COMMENT "Generating FluentQt PySide6 bindings"
    VERBATIM)
add_custom_target(fluentqt_pyside6_generate
    DEPENDS "${FLUENTQT_PYSIDE6_GENERATION_STAMP}")
set_source_files_properties(
    ${FLUENTQT_PYSIDE6_GENERATED_SOURCES} PROPERTIES GENERATED TRUE)

add_library(_fluentqt MODULE
    ${FLUENTQT_PYSIDE6_GENERATED_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/native/BindingApi.cpp")
add_dependencies(_fluentqt fluentqt_pyside6_generate)
set_target_properties(_fluentqt PROPERTIES
    AUTOMOC OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    PREFIX ""
    POSITION_INDEPENDENT_CODE ON)
if(WIN32)
    set_target_properties(_fluentqt PROPERTIES SUFFIX ".pyd")
endif()

set(FLUENTQT_PYSIDE6_PACKAGE_DIR
    "${PROJECT_BINARY_DIR}/python/fluentqt")
set_target_properties(_fluentqt PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${FLUENTQT_PYSIDE6_PACKAGE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${FLUENTQT_PYSIDE6_PACKAGE_DIR}")
foreach(FLUENTQT_PYSIDE6_CONFIGURATION
        DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(_fluentqt PROPERTIES
        "LIBRARY_OUTPUT_DIRECTORY_${FLUENTQT_PYSIDE6_CONFIGURATION}"
            "${FLUENTQT_PYSIDE6_PACKAGE_DIR}"
        "RUNTIME_OUTPUT_DIRECTORY_${FLUENTQT_PYSIDE6_CONFIGURATION}"
            "${FLUENTQT_PYSIDE6_PACKAGE_DIR}")
endforeach()

target_compile_definitions(_fluentqt PRIVATE
    FLUENT_QT_PYSIDE6_PROJECT_VERSION="${PROJECT_VERSION}"
    FLUENT_QT_PYSIDE6_VERSION="${FLUENTQT_PYSIDE6_VERSION}"
    FLUENT_QT_SHIBOKEN6_VERSION="${FLUENTQT_SHIBOKEN6_VERSION}"
    FLUENT_QT_SHIBOKEN6_GENERATOR_VERSION="${FLUENTQT_SHIBOKEN6_GENERATOR_VERSION}")
if(MSVC)
    target_compile_options(_fluentqt PRIVATE
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/native/WindowsBindingPrelude.h")
endif()
target_include_directories(_fluentqt PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/native"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
    "${FLUENTQT_PYSIDE6_GENERATED_DIR}"
    "${FLUENTQT_PYSIDE6_INCLUDE_DIR}"
    "${FLUENTQT_PYSIDE6_INCLUDE_DIR}/QtCore"
    "${FLUENTQT_PYSIDE6_INCLUDE_DIR}/QtGui"
    "${FLUENTQT_PYSIDE6_INCLUDE_DIR}/QtWidgets"
    "${FLUENTQT_SHIBOKEN6_INCLUDE_DIR}"
    ${Python_INCLUDE_DIRS})
target_link_libraries(_fluentqt PRIVATE
    FluentQt
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    "${FLUENTQT_PYSIDE6_LIBRARY}"
    "${FLUENTQT_SHIBOKEN6_LIBRARY}")
if(WIN32)
    target_link_libraries(_fluentqt PRIVATE Python::Python)
elseif(APPLE)
    target_link_options(_fluentqt PRIVATE "-undefined" "dynamic_lookup")
endif()

set(FLUENTQT_PYSIDE6_RELOCATABLE_RPATH)
if(APPLE)
    set(FLUENTQT_PYSIDE6_RELOCATABLE_RPATH
        "@loader_path/../PySide6"
        "@loader_path/../shiboken6"
        "@loader_path/../PySide6/Qt/lib")
elseif(UNIX)
    set(FLUENTQT_PYSIDE6_RELOCATABLE_RPATH
        "$ORIGIN/../PySide6"
        "$ORIGIN/../shiboken6"
        "$ORIGIN/../PySide6/Qt/lib")
endif()
set_target_properties(_fluentqt PROPERTIES
    BUILD_RPATH
        "${FLUENTQT_PYSIDE6_RELOCATABLE_RPATH};${FLUENTQT_PYSIDE6_ROOT};${FLUENTQT_SHIBOKEN6_ROOT};${FLUENTQT_PYSIDE6_QT_RUNTIME_DIR}"
    INSTALL_RPATH "${FLUENTQT_PYSIDE6_RELOCATABLE_RPATH}")

file(MAKE_DIRECTORY "${FLUENTQT_PYSIDE6_PACKAGE_DIR}")
set(FLUENTQT_PYSIDE6_PYTHON_FILES
    __init__.py
    basicinput.py
    collections.py
    date_time.py
    design.py
    dialogs_flyouts.py
    foundation.py
    layout.py
    menus_toolbars.py
    navigation.py
    scrolling.py
    status_info.py
    textfields.py
    windowing.py
    py.typed)
set(FLUENTQT_PYSIDE6_STUB_FILES
    __init__.pyi
    _fluentqt.pyi
    basicinput.pyi
    collections.pyi
    date_time.pyi
    design.pyi
    dialogs_flyouts.pyi
    foundation.pyi
    layout.pyi
    menus_toolbars.pyi
    navigation.pyi
    scrolling.pyi
    status_info.pyi
    textfields.pyi
    windowing.pyi)
set(FLUENTQT_PYSIDE6_PYTHON_SOURCE_PATHS)
foreach(FLUENTQT_PYSIDE6_PYTHON_FILE
        IN LISTS FLUENTQT_PYSIDE6_PYTHON_FILES)
    list(APPEND FLUENTQT_PYSIDE6_PYTHON_SOURCE_PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/fluentqt/${FLUENTQT_PYSIDE6_PYTHON_FILE}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/fluentqt/${FLUENTQT_PYSIDE6_PYTHON_FILE}"
        "${FLUENTQT_PYSIDE6_PACKAGE_DIR}/${FLUENTQT_PYSIDE6_PYTHON_FILE}"
        COPYONLY)
endforeach()
set(FLUENTQT_PYSIDE6_DESIGN_ICON_ALIASES
    "${PROJECT_SOURCE_DIR}/tools/fonts/fluent_icon_aliases.json")
configure_file(
    "${FLUENTQT_PYSIDE6_DESIGN_ICON_ALIASES}"
    "${FLUENTQT_PYSIDE6_PACKAGE_DIR}/_icon_aliases.json"
    COPYONLY)
list(APPEND FLUENTQT_PYSIDE6_PYTHON_SOURCE_PATHS
    "${FLUENTQT_PYSIDE6_DESIGN_ICON_ALIASES}")

set(FLUENTQT_PYSIDE6_STUB_PATHS)
foreach(FLUENTQT_PYSIDE6_STUB_FILE IN LISTS FLUENTQT_PYSIDE6_STUB_FILES)
    list(APPEND FLUENTQT_PYSIDE6_STUB_PATHS
        "${FLUENTQT_PYSIDE6_PACKAGE_DIR}/${FLUENTQT_PYSIDE6_STUB_FILE}")
endforeach()
set(FLUENTQT_PYSIDE6_STUB_STAMP
    "${CMAKE_CURRENT_BINARY_DIR}/stubs.stamp")
add_custom_command(
    OUTPUT "${FLUENTQT_PYSIDE6_STUB_STAMP}"
    BYPRODUCTS ${FLUENTQT_PYSIDE6_STUB_PATHS}
    COMMAND "${CMAKE_COMMAND}" -E env
        "PYTHONDONTWRITEBYTECODE=1"
        "PYTHONPATH=${PROJECT_BINARY_DIR}/python"
        "QT_QPA_PLATFORM=offscreen"
        "${Python_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_stubs.py"
        --extension "$<TARGET_FILE:_fluentqt>"
        --package-dir "${FLUENTQT_PYSIDE6_PACKAGE_DIR}"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/api-manifest.json"
    COMMAND "${CMAKE_COMMAND}" -E env
        "PYTHONDONTWRITEBYTECODE=1"
        "QT_QPA_PLATFORM=offscreen"
        "${Python_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_api_policy.py"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/api-manifest.json"
        --project-file "${PROJECT_SOURCE_DIR}/CMakeLists.txt"
    COMMAND "${CMAKE_COMMAND}" -E env
        "PYTHONDONTWRITEBYTECODE=1"
        "QT_QPA_PLATFORM=offscreen"
        "${Python_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_stubs.py"
        --package-dir "${FLUENTQT_PYSIDE6_PACKAGE_DIR}"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/api-manifest.json"
    COMMAND "${CMAKE_COMMAND}" -E touch
        "${FLUENTQT_PYSIDE6_STUB_STAMP}"
    DEPENDS
        _fluentqt
        "${CMAKE_CURRENT_SOURCE_DIR}/api-manifest.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_stubs.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_api_policy.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_stubs.py"
        ${FLUENTQT_PYSIDE6_PYTHON_SOURCE_PATHS}
    COMMENT "Generating and verifying FluentQt PySide6 type stubs"
    VERBATIM)
add_custom_target(fluentqt_pyside6_stubs ALL
    DEPENDS "${FLUENTQT_PYSIDE6_STUB_STAMP}")

install(TARGETS _fluentqt
    LIBRARY DESTINATION fluentqt
        COMPONENT FluentQtPySide6
        EXCLUDE_FROM_ALL
    RUNTIME DESTINATION fluentqt COMPONENT FluentQtPySide6
        EXCLUDE_FROM_ALL)
foreach(FLUENTQT_PYSIDE6_PYTHON_FILE
        IN LISTS FLUENTQT_PYSIDE6_PYTHON_FILES)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/fluentqt/${FLUENTQT_PYSIDE6_PYTHON_FILE}"
        DESTINATION fluentqt
        COMPONENT FluentQtPySide6
        EXCLUDE_FROM_ALL)
endforeach()
install(FILES "${FLUENTQT_PYSIDE6_DESIGN_ICON_ALIASES}"
    DESTINATION fluentqt
    RENAME _icon_aliases.json
    COMPONENT FluentQtPySide6
    EXCLUDE_FROM_ALL)
foreach(FLUENTQT_PYSIDE6_STUB_PATH IN LISTS FLUENTQT_PYSIDE6_STUB_PATHS)
    install(FILES "${FLUENTQT_PYSIDE6_STUB_PATH}"
        DESTINATION fluentqt
        COMPONENT FluentQtPySide6
        EXCLUDE_FROM_ALL)
endforeach()

set(FLUENTQT_PYSIDE6_WHEEL_ROOT
    "${PROJECT_BINARY_DIR}/pyside6-wheel")
set(FLUENTQT_PYSIDE6_WHEEL_STAGE
    "${FLUENTQT_PYSIDE6_WHEEL_ROOT}/staging")
set(FLUENTQT_PYSIDE6_WHEELHOUSE
    "${PROJECT_BINARY_DIR}/wheelhouse")
add_custom_target(fluentqt_pyside6_wheel
    COMMAND "${CMAKE_COMMAND}" -E remove_directory
        "${FLUENTQT_PYSIDE6_WHEEL_STAGE}"
    COMMAND "${CMAKE_COMMAND}" -E remove_directory
        "${FLUENTQT_PYSIDE6_WHEELHOUSE}"
    COMMAND "${CMAKE_COMMAND}" --install "${PROJECT_BINARY_DIR}"
        --prefix "${FLUENTQT_PYSIDE6_WHEEL_STAGE}"
        --component FluentQtPySide6
        --config "$<CONFIG>"
    COMMAND "${Python_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_wheel.py"
        --package-dir "${FLUENTQT_PYSIDE6_WHEEL_STAGE}/fluentqt"
        --output-dir "${FLUENTQT_PYSIDE6_WHEELHOUSE}"
        --version "${PROJECT_VERSION}"
        --pyside-version "${FLUENTQT_PYSIDE6_VERSION}"
        --shiboken-version "${FLUENTQT_SHIBOKEN6_VERSION}"
        --requires-python "${FLUENT_QT_PYSIDE6_REQUIRES_PYTHON}"
        --description-file "${CMAKE_CURRENT_SOURCE_DIR}/PYPI.md"
        --license-file "${PROJECT_SOURCE_DIR}/LICENSE"
        --license-file "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        --license-file "${PROJECT_SOURCE_DIR}/TRADEMARKS.md"
    DEPENDS
        fluentqt_pyside6_stubs
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_wheel.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/PYPI.md"
        "${PROJECT_SOURCE_DIR}/LICENSE"
        "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "${PROJECT_SOURCE_DIR}/TRADEMARKS.md"
    COMMENT "Building relocatable FluentQt PySide6 wheel"
    USES_TERMINAL
    VERBATIM)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FluentQtPySide6Gallery.cmake")

set(FLUENT_QT_PYSIDE6_MANYLINUX_POLICY "" CACHE STRING
    "PEP 600 policy for the opt-in repaired Linux wheel")
set(FLUENT_QT_PYSIDE6_AUDITWHEEL_VERSION "6.7.0" CACHE STRING
    "Pinned auditwheel version for the manylinux release gate")
if(UNIX AND NOT APPLE AND FLUENT_QT_PYSIDE6_MANYLINUX_POLICY)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        set(FLUENTQT_PYSIDE6_MANYLINUX_ARCHITECTURE "x86_64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(FLUENTQT_PYSIDE6_MANYLINUX_ARCHITECTURE "aarch64")
    else()
        message(FATAL_ERROR
            "The PySide6 manylinux target does not support architecture "
            "${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    set(FLUENTQT_PYSIDE6_MANYLINUX_WHEELHOUSE
        "${PROJECT_BINARY_DIR}/manylinux-wheelhouse")
    set(FLUENTQT_PYSIDE6_MANYLINUX_REPORT
        "${PROJECT_BINARY_DIR}/manylinux-audit.json")
add_custom_target(fluentqt_pyside6_manylinux_wheel
        COMMAND "${CMAKE_COMMAND}" -E remove_directory
            "${FLUENTQT_PYSIDE6_MANYLINUX_WHEELHOUSE}"
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/repair_manylinux_wheel.py"
            --wheel-dir "${FLUENTQT_PYSIDE6_WHEELHOUSE}"
            --output-dir "${FLUENTQT_PYSIDE6_MANYLINUX_WHEELHOUSE}"
            --policy "${FLUENT_QT_PYSIDE6_MANYLINUX_POLICY}"
            --architecture "${FLUENTQT_PYSIDE6_MANYLINUX_ARCHITECTURE}"
            --auditwheel-version
                "${FLUENT_QT_PYSIDE6_AUDITWHEEL_VERSION}"
            --pyside-version "${FLUENTQT_PYSIDE6_VERSION}"
            --shiboken-version "${FLUENTQT_SHIBOKEN6_VERSION}"
            --exclude "libQt6*.so.6"
            --exclude "libpyside6*.so.*"
            --exclude "libshiboken6*.so.*"
            --required-rpath "$ORIGIN/../PySide6"
            --required-rpath "$ORIGIN/../shiboken6"
            --required-rpath "$ORIGIN/../PySide6/Qt/lib"
            --report "${FLUENTQT_PYSIDE6_MANYLINUX_REPORT}"
        DEPENDS
            fluentqt_pyside6_wheel
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/repair_manylinux_wheel.py"
        COMMENT "Repairing and auditing the FluentQt PySide6 manylinux wheel"
        USES_TERMINAL
        VERBATIM)
endif()

if(BUILD_TESTING)
    add_test(
        NAME test_pyside6_bindings
        COMMAND "${Python_EXECUTABLE}"
            -X faulthandler
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bindings.py"
            -v)
    set_tests_properties(test_pyside6_bindings PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen;FLUENTQT_EXPECTED_VERSION=${PROJECT_VERSION}"
        LABELS "pyside;unit;ci_fast"
        TIMEOUT 60)

    add_test(
        NAME test_pyside6_foundation_authoring
        COMMAND "${Python_EXECUTABLE}"
            -X faulthandler
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_foundation_authoring.py"
            -v)
    set_tests_properties(test_pyside6_foundation_authoring PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;unit;ci_fast"
        TIMEOUT 60)

    function(fluentqt_add_pyside6_isolated_lifetime_test TEST_NAME TEST_METHOD)
        add_test(
            NAME "${TEST_NAME}"
            COMMAND "${Python_EXECUTABLE}"
                -X faulthandler
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bindings.py"
                -v
                "FluentQtBindingTest.${TEST_METHOD}")
        set_tests_properties("${TEST_NAME}" PROPERTIES
            ENVIRONMENT
                "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen;FLUENTQT_EXPECTED_VERSION=${PROJECT_VERSION}"
            LABELS "pyside;unit;ci_fast;lifetime"
            TIMEOUT 60)
    endfunction()

    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_qscrollarea_owned_gc_stress
        test_qscrollarea_owned_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_expander_owned_gc_stress
        test_expander_owned_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_expander_borrowed_gc_stress
        test_expander_borrowed_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_expander_reparented_gc_stress
        test_expander_reparented_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_accordion_owned_gc_stress
        test_accordion_owned_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_accordion_borrowed_gc_stress
        test_accordion_borrowed_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_accordion_reparented_gc_stress
        test_accordion_reparented_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_flip_view_owned_gc_stress
        test_flip_view_owned_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_flip_view_borrowed_gc_stress
        test_flip_view_borrowed_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_flip_view_reparented_gc_stress
        test_flip_view_reparented_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_split_view_owned_gc_stress
        test_split_view_owned_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_split_view_borrowed_gc_stress
        test_split_view_borrowed_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_split_view_reparented_gc_stress
        test_split_view_reparented_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_drawer_view_owned_gc_stress
        test_drawer_view_owned_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_drawer_view_borrowed_gc_stress
        test_drawer_view_borrowed_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_drawer_view_reparented_gc_stress
        test_drawer_view_reparented_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_popup_dependency_gc_stress
        test_popup_dependency_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_flyout_dependency_gc_stress
        test_flyout_dependency_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_guidance_overlay_target_dependency_gc_stress
        test_guidance_overlay_target_dependency_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_status_overlay_lifetime_gc_stress
        test_status_overlay_lifetime_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_dialog_theme_source_gc_stress
        test_dialog_theme_source_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_content_dialog_content_gc_stress
        test_content_dialog_content_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_combo_box_dependency_gc_stress
        test_combo_box_dependency_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_auto_suggest_box_popup_lifecycle_stress
        test_auto_suggest_box_popup_lifecycle_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_menu_button_dependency_gc_stress
        test_menu_button_dependency_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_command_surface_dependency_gc_stress
        test_command_surface_dependency_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_title_bar_content_gc_stress
        test_title_bar_content_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_window_title_bar_gc_stress
        test_window_title_bar_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_date_time_picker_popup_lifecycle_stress
        test_date_time_picker_popup_lifecycle_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_stack_content_host_owned_gc_stress
        test_stack_content_host_owned_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_stack_content_host_borrowed_gc_stress
        test_stack_content_host_borrowed_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_stack_content_host_reparented_gc_stress
        test_stack_content_host_reparented_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_navigation_view_owned_chrome_gc_stress
        test_navigation_view_owned_chrome_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_navigation_view_borrowed_chrome_gc_stress
        test_navigation_view_borrowed_chrome_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_navigation_view_reparented_chrome_gc_stress
        test_navigation_view_reparented_chrome_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_flow_view_dependency_gc_stress
        test_flow_view_dependency_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_grid_view_dependency_gc_stress
        test_grid_view_dependency_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_tree_view_dependency_gc_stress
        test_tree_view_dependency_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_list_view_dependency_gc_stress
        test_list_view_dependency_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_stack_view_owned_gc_stress
        test_stack_view_owned_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_stack_view_borrowed_gc_stress
        test_stack_view_borrowed_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_stack_view_reparented_gc_stress
        test_stack_view_reparented_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_info_bar_hosted_action_gc_stress
        test_info_bar_hosted_action_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_scroll_view_facade_empty_gc_stress
        test_scroll_view_facade_gc_stress_without_content)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_scroll_view_native_owned_gc_stress
        test_scroll_view_native_owned_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_scroll_view_facade_owned_gc_stress
        test_scroll_view_owned_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_scroll_view_facade_borrowed_gc_stress
        test_scroll_view_borrowed_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_scroll_view_facade_reparented_gc_stress
        test_scroll_view_reparented_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_scroll_view_qscrollarea_base_owned_gc_stress
        test_scroll_view_qscrollarea_base_owned_gc_stress)
    fluentqt_add_pyside6_isolated_lifetime_test(
        test_pyside6_annotated_scroll_bar_borrowed_link_gc
        test_annotated_scroll_bar_borrowed_scroll_view_link)

    add_test(
        NAME test_pyside6_generated_contracts
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_generated_contract_verifier.py")
    set_tests_properties(test_pyside6_generated_contracts PROPERTIES
        LABELS "pyside;unit;ci_fast"
        TIMEOUT 90)

    add_test(
        NAME test_pyside6_api_policy
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_api_policy.py")
    set_tests_properties(test_pyside6_api_policy PROPERTIES
        LABELS "pyside;unit;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_manylinux_wheel
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_manylinux_wheel.py")
    set_tests_properties(test_pyside6_manylinux_wheel PROPERTIES
        LABELS "pyside;unit;ci_fast"
        TIMEOUT 30)

    if(FLUENT_QT_BUILD_PYSIDE6_GALLERY)
        add_test(
            NAME test_pyside6_gallery
            COMMAND "${Python_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/gallery/tests/test_gallery.py")
        set_tests_properties(test_pyside6_gallery PROPERTIES
            ENVIRONMENT
                "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
            LABELS "pyside;unit;ci_fast"
            TIMEOUT 240)

        # The checked-in WebAssembly teaching catalog is produced by the
        # official PySide6 release runtime. Older, non-published Qt 6.2
        # compatibility wheels can render equivalent enum expressions with
        # different source spelling, so keep their Gallery behavior tests but
        # do not use them as canonical catalog generators.
        if(FLUENT_QT_PYSIDE6_REQUIRES_PYTHON STREQUAL ">=3.11,<3.14")
            add_test(
                NAME test_pyside6_gallery_python_snippet_catalog
                COMMAND "${Python_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/gallery/tools/generate_native_python_snippet_catalog.py"
                    --output "${PROJECT_SOURCE_DIR}/app/data/gallery_python_snippets.json"
                    --check)
            set_tests_properties(test_pyside6_gallery_python_snippet_catalog PROPERTIES
                ENVIRONMENT
                    "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
                LABELS "pyside;unit;ci_fast"
                TIMEOUT 240)
        endif()

        add_test(
            NAME test_pyside6_gallery_contract_generator
            COMMAND "${Python_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/gallery/tests/test_gallery_contract_generator.py"
                --project-root "${PROJECT_SOURCE_DIR}"
                --contract "${FLUENTQT_PYSIDE6_GALLERY_CONTRACT}")
        set_tests_properties(test_pyside6_gallery_contract_generator PROPERTIES
            LABELS "pyside;unit;ci_fast"
            TIMEOUT 30)

        add_test(
            NAME test_pyside6_gallery_wheel_builder
            COMMAND "${Python_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/gallery/tests/test_wheel_builder.py")
        set_tests_properties(test_pyside6_gallery_wheel_builder PROPERTIES
            LABELS "pyside;unit;ci_fast"
            TIMEOUT 30)
    endif()

    add_test(
        NAME test_pyside6_wheel_builder
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_wheel_builder.py")
    set_tests_properties(test_pyside6_wheel_builder PROPERTIES
        LABELS "pyside;unit;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_stub_generator
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_stub_generator.py")
    set_tests_properties(test_pyside6_stub_generator PROPERTIES
        LABELS "pyside;unit;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_typing_stubs
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_stubs.py"
            --package-dir "${FLUENTQT_PYSIDE6_PACKAGE_DIR}"
            --manifest "${CMAKE_CURRENT_SOURCE_DIR}/api-manifest.json")
    set_tests_properties(test_pyside6_typing_stubs PROPERTIES
        LABELS "pyside;unit;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_color_picker_showcase
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/color_picker_showcase.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-color-picker-showcase.png")
    set_tests_properties(test_pyside6_color_picker_showcase PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_calendar_view_showcase
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/calendar_view_showcase.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-calendar-view-showcase.png")
    set_tests_properties(test_pyside6_calendar_view_showcase PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_date_time_pickers_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/date_time_pickers.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-date-time-pickers.png")
    set_tests_properties(test_pyside6_date_time_pickers_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_auto_suggest_box_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/auto_suggest_box.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-auto-suggest-box.png")
    set_tests_properties(test_pyside6_auto_suggest_box_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_annotated_scroll_bar_showcase
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/annotated_scroll_bar_showcase.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-annotated-scroll-bar-showcase.png")
    set_tests_properties(test_pyside6_annotated_scroll_bar_showcase PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_accordion_ownership_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/accordion_ownership.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-accordion-ownership.png")
    set_tests_properties(test_pyside6_accordion_ownership_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_list_view_model_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/list_view_model.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-list-view-model.png")
    set_tests_properties(test_pyside6_list_view_model_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_grid_view_model_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/grid_view_model.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-grid-view-model.png")
    set_tests_properties(test_pyside6_grid_view_model_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_flow_view_model_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/flow_view_model.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-flow-view-model.png")
    set_tests_properties(test_pyside6_flow_view_model_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_flip_view_ownership_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/flip_view_ownership.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-flip-view-ownership.png")
    set_tests_properties(test_pyside6_flip_view_ownership_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_split_view_ownership_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/split_view_ownership.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-split-view-ownership.png")
    set_tests_properties(test_pyside6_split_view_ownership_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_drawer_view_ownership_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/drawer_view_ownership.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-drawer-view-ownership.png")
    set_tests_properties(test_pyside6_drawer_view_ownership_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_popup_overlay_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/popup_overlay.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-popup-overlay.png")
    set_tests_properties(test_pyside6_popup_overlay_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_flyout_overlay_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/flyout_overlay.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-flyout-overlay.png")
    set_tests_properties(test_pyside6_flyout_overlay_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_guidance_overlays_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/guidance_overlays.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-guidance-overlays.png")
    set_tests_properties(test_pyside6_guidance_overlays_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_status_overlays_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/status_overlays.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-status-overlays.png")
    set_tests_properties(test_pyside6_status_overlays_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_content_dialog_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/content_dialog.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-content-dialog.png")
    set_tests_properties(test_pyside6_content_dialog_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_combo_box_dropdown_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/combo_box_dropdown.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-combo-box-dropdown.png")
    set_tests_properties(test_pyside6_combo_box_dropdown_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_menu_buttons_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/menu_buttons.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-menu-buttons.png")
    set_tests_properties(test_pyside6_menu_buttons_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_command_surfaces_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/command_surfaces.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-command-surfaces.png")
    set_tests_properties(test_pyside6_command_surfaces_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_navigation_view_ownership_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/navigation_view_ownership.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-navigation-view-ownership.png")
    set_tests_properties(test_pyside6_navigation_view_ownership_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_tree_view_model_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/tree_view_model.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-tree-view-model.png")
    set_tests_properties(test_pyside6_tree_view_model_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_stack_view_navigation_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/stack_view_navigation.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-stack-view-navigation.png")
    set_tests_properties(test_pyside6_stack_view_navigation_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_breadcrumb_navigation_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/breadcrumb_navigation.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-breadcrumb-navigation.png")
    set_tests_properties(test_pyside6_breadcrumb_navigation_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_selector_pivot_navigation_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/selector_pivot_navigation.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-selector-pivot-navigation.png")
    set_tests_properties(
        test_pyside6_selector_pivot_navigation_example
        PROPERTIES
            ENVIRONMENT
                "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
            LABELS "pyside;acceptance;ci_fast"
            TIMEOUT 30)

    add_test(
        NAME test_pyside6_tab_view_navigation_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/tab_view_navigation.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-tab-view-navigation.png")
    set_tests_properties(test_pyside6_tab_view_navigation_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_compatibility_showcase
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/compatibility_showcase.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-compatibility-showcase.png")
    set_tests_properties(test_pyside6_compatibility_showcase PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    if(FLUENT_QT_BUILD_PYSIDE6_GALLERY)
        add_test(
            NAME test_pyside6_gallery_acceptance
            COMMAND "${Python_EXECUTABLE}"
                -m fluentqt_gallery
                --walk-routes
                --route home
                --snapshot "${PROJECT_BINARY_DIR}/pyside6-gallery.png"
                --report "${PROJECT_BINARY_DIR}/pyside6-gallery.json")
        set_tests_properties(test_pyside6_gallery_acceptance PROPERTIES
            ENVIRONMENT
                "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
            LABELS "pyside;acceptance;ci_fast"
            TIMEOUT 120)
    endif()

    add_test(
        NAME test_pyside6_window_chrome_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/window_chrome.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-window-chrome.png"
            --report
            "${PROJECT_BINARY_DIR}/pyside6-window-chrome.json")
    set_tests_properties(test_pyside6_window_chrome_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)

    add_test(
        NAME test_pyside6_scroll_view_ownership_example
        COMMAND "${Python_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/scroll_view_ownership.py"
            --snapshot
            "${PROJECT_BINARY_DIR}/pyside6-scroll-view-ownership.png")
    set_tests_properties(test_pyside6_scroll_view_ownership_example PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_BINARY_DIR}/python;QT_QPA_PLATFORM=offscreen"
        LABELS "pyside;acceptance;ci_fast"
        TIMEOUT 30)
endif()
