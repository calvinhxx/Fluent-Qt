# Explicit allowlist for headers shipped in the FluentQt development package.
# Keeping installation separate from the source-directory layout prevents a new
# implementation header from becoming public merely because it was added under
# src/. Paths are relative to the FluentQt source root.

set(FLUENT_QT_INSTALL_HEADERS
    include/FluentQt/FluentQt.h

    src/compatibility/FontCompat.h
    src/compatibility/QtCompat.h
    src/compatibility/WindowChromeCompat.h

    src/components/basicinput/Button.h
    src/components/basicinput/CheckBox.h
    src/components/basicinput/ColorPicker.h
    src/components/basicinput/ComboBox.h
    src/components/basicinput/DropDownButton.h
    src/components/basicinput/HyperlinkButton.h
    src/components/basicinput/RadioButton.h
    src/components/basicinput/RatingControl.h
    src/components/basicinput/RepeatButton.h
    src/components/basicinput/Slider.h
    src/components/basicinput/SplitButton.h
    src/components/basicinput/ToggleButton.h
    src/components/basicinput/ToggleSplitButton.h
    src/components/basicinput/ToggleSwitch.h

    src/components/collections/DrawerView.h
    src/components/collections/FlipView.h
    src/components/collections/FlowView.h
    src/components/collections/GridView.h
    src/components/collections/ListView.h
    src/components/collections/SplitView.h
    src/components/collections/StackView.h
    src/components/collections/TreeView.h

    src/components/date_time/CalendarDatePicker.h
    src/components/date_time/CalendarView.h
    src/components/date_time/DatePicker.h
    src/components/date_time/TimePicker.h

    src/components/dialogs_flyouts/CoachMark.h
    src/components/dialogs_flyouts/ContentDialog.h
    src/components/dialogs_flyouts/Dialog.h
    src/components/dialogs_flyouts/Flyout.h
    src/components/dialogs_flyouts/Popup.h
    src/components/dialogs_flyouts/TeachingTip.h

    src/components/foundation/FluentElement.h
    src/components/foundation/QMLPlus.h
    src/components/foundation/StyleThemeCatalog.h
    src/components/foundation/ThemeRegistry.h
    src/components/foundation/overlay/OverlayGeometry.h
    src/components/foundation/overlay/OverlayLightDismiss.h
    src/components/foundation/overlay/OverlayScrim.h
    src/components/foundation/overlay/OverlayShadow.h
    src/components/foundation/overlay/OverlayWindow.h

    src/components/menus_toolbars/Menu.h
    src/components/menus_toolbars/MenuBar.h

    src/components/navigation/Breadcrumb.h
    src/components/navigation/NavigationView.h
    src/components/navigation/Pivot.h
    src/components/navigation/SelectorBar.h
    src/components/navigation/StackContentHost.h
    src/components/navigation/TabView.h

    src/components/scrolling/AnnotatedScrollBar.h
    src/components/scrolling/PipsPager.h
    src/components/scrolling/ScrollBar.h
    src/components/scrolling/ScrollView.h

    src/components/status_info/InfoBadge.h
    src/components/status_info/InfoBar.h
    src/components/status_info/ProgressBar.h
    src/components/status_info/ProgressRing.h
    src/components/status_info/Shimmer.h
    src/components/status_info/ToolTip.h

    src/components/textfields/AutoSuggestBox.h
    src/components/textfields/Label.h
    src/components/textfields/LineEdit.h
    src/components/textfields/NumberBox.h
    src/components/textfields/PasswordBox.h
    src/components/textfields/TextEdit.h

    src/components/windowing/TitleBar.h
    src/components/windowing/Window.h
    src/components/windowing/WindowBackdrop.h
    src/components/windowing/WindowBackdropMaterial.h

    src/design/Animation.h
    src/design/Breakpoints.h
    src/design/CornerRadius.h
    src/design/Elevation.h
    src/design/IconCatalog.h
    src/design/Material.h
    src/design/Spacing.h
    src/design/ThemeColors.h
    src/design/Typography.h

    # These implementation-oriented headers were already installed by FluentQt
    # 1.x. Keep their paths available for 1.x source compatibility, but do not
    # add new application dependencies on them.
    src/components/scrolling/OverlayScrollChrome.h
    src/components/scrolling/OverscrollController.h
    src/components/windowing/WindowChromeFrame.h
    src/utils/DebugOverlay.h
)

function(fluent_qt_install_headers source_root)
    foreach(_header IN LISTS FLUENT_QT_INSTALL_HEADERS)
        if(_header MATCHES "^include/FluentQt/(.+)$")
            set(_relative_path "${CMAKE_MATCH_1}")
        elseif(_header MATCHES "^src/(.+)$")
            set(_relative_path "${CMAKE_MATCH_1}")
        else()
            message(FATAL_ERROR "Unsupported FluentQt install header path: ${_header}")
        endif()

        get_filename_component(_relative_directory "${_relative_path}" DIRECTORY)
        set(_destination "${CMAKE_INSTALL_INCLUDEDIR}/FluentQt")
        if(_relative_directory)
            string(APPEND _destination "/${_relative_directory}")
        endif()
        install(FILES "${source_root}/${_header}"
            DESTINATION "${_destination}"
            COMPONENT Development)
    endforeach()
endfunction()
