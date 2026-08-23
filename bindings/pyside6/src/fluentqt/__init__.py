"""PySide6 bindings for FluentQt.

Importing this package does not create a QApplication or change global theme
state. Call ``prepare_high_dpi_application()`` before constructing the
application and ``initialize_resources()`` afterwards.
"""

from PySide6 import QtCore as _QtCore
from PySide6 import QtGui as _QtGui
from PySide6 import QtWidgets as _QtWidgets

from ._fluentqt import (
    bindingBuildInfo,
    initializeResources,
    inspectWidgetForBinding,
    prepareHighDpiApplication,
)
from .basicinput import (
    Button,
    CheckBox,
    ColorPicker,
    ComboBox,
    CompoundButton,
    DropDownButton,
    HyperlinkButton,
    RadioButton,
    RatingControl,
    RepeatButton,
    Slider,
    SplitButton,
    ToggleButton,
    ToggleSplitButton,
    ToggleSwitch,
)
from .collections import (
    DataGrid,
    DrawerView,
    FlipView,
    FlowView,
    GridView,
    ListView,
    SelectionMode,
    SplitView,
    SplitViewPaneOptions,
    StackView,
    TreeView,
)
from .date_time import CalendarDatePicker, CalendarView, DatePicker, TimePicker
from .design import (
    CornerRadius,
    Icons,
    IconSize,
    Spacing,
    ThemeTokens,
    Typography,
)
from .dialogs_flyouts import (
    CoachMark,
    ContentDialog,
    ContentDialogButton,
    Dialog,
    Flyout,
    Popup,
    TeachingTip,
)
from .foundation import (
    AnchorEdge,
    AnchorLayout,
    AnchorSpec,
    BindingMode,
    FluentWidget,
    FontIcon,
    FontRole,
    StateGroup,
    Theme,
    accent_color,
    accentColor,
    anchors,
    apply_user_theme,
    applyUserTheme,
    bind,
    current_theme,
    currentTheme,
    font_scale,
    font_for_role,
    fontScale,
    fontForRole,
    reset_theme_tokens,
    resetThemeTokens,
    set_accent_color,
    set_font_scale,
    set_theme,
    setAccentColor,
    setFontScale,
    setTheme,
    theme_revision,
    themeRevision,
)
from .layout import Accordion, Card, Divider, Expander, Field
from .menus_toolbars import (
    CommandBar,
    CommandBarFlyout,
    FluentMenu,
    FluentMenuBar,
    FluentMenuItem,
)
from .navigation import (
    Breadcrumb,
    BreadcrumbItem,
    NavigationView,
    Pivot,
    PivotItem,
    SelectorBar,
    SelectorBarItem,
    StackContentHost,
    TabView,
    TabViewItem,
)
from .scrolling import (
    AnnotatedScrollBar,
    AnnotatedScrollBarLabel,
    PipsPager,
    ScrollBar,
    ScrollView,
    ScrollViewZoomAwareWidget,
    WidgetOwnership,
)
from .status_info import (
    Avatar,
    InfoBadge,
    InfoBar,
    ProgressBar,
    ProgressRing,
    Shimmer,
    Toast,
    ToolTip,
)
from .textfields import (
    AutoSuggestBox,
    EditingCommandRouter,
    Label,
    LineEdit,
    NumberBox,
    PasswordBox,
    TextEdit,
)
from .windowing import (
    BackdropBackend,
    BackdropCapabilities,
    BackdropEffect,
    BackdropFidelity,
    BackdropState,
    BackdropSurfaceMode,
    TitleBar,
    Window,
)

__version__ = str(bindingBuildInfo()["fluentqt_version"])
__api_version__ = ".".join(__version__.split(".")[:2])


def prepare_high_dpi_application():
    """Apply FluentQt's pre-QApplication high-DPI settings."""
    prepareHighDpiApplication()


def initialize_resources():
    """Initialize the fonts, icons, and other compiled FluentQt resources."""
    return initializeResources()


def binding_build_info():
    """Return the FluentQt and Qt versions used to compile this extension."""
    return dict(bindingBuildInfo())


def inspect_widget(
    widget,
    *,
    minimum_hit_area=(24, 24),
    spacing_grid=4,
    check_clipped_text=True,
    check_accessibility_names=True,
    check_hit_areas=True,
    check_focus_order=True,
    check_duplicate_actions=True,
    check_nested_scrolling=True,
    check_layout_grid=False,
):
    """Return a read-only, versioned quality report for a visible widget tree."""

    if not isinstance(widget, _QtWidgets.QWidget):
        raise TypeError("widget must be a QWidget")
    if isinstance(minimum_hit_area, _QtCore.QSize):
        minimum_width = minimum_hit_area.width()
        minimum_height = minimum_hit_area.height()
    elif isinstance(minimum_hit_area, (tuple, list)) and len(minimum_hit_area) == 2:
        minimum_width, minimum_height = minimum_hit_area
    else:
        raise TypeError("minimum_hit_area must be QSize or (width, height)")
    if (
        not isinstance(minimum_width, int)
        or isinstance(minimum_width, bool)
        or not isinstance(minimum_height, int)
        or isinstance(minimum_height, bool)
    ):
        raise TypeError("minimum_hit_area values must be integers")
    if not isinstance(spacing_grid, int) or isinstance(spacing_grid, bool):
        raise TypeError("spacing_grid must be an integer")
    if minimum_width < 1 or minimum_height < 1:
        raise ValueError("minimum_hit_area values must be positive")
    if spacing_grid < 1:
        raise ValueError("spacing_grid must be positive")

    checks = {
        "check_clipped_text": check_clipped_text,
        "check_accessibility_names": check_accessibility_names,
        "check_hit_areas": check_hit_areas,
        "check_focus_order": check_focus_order,
        "check_duplicate_actions": check_duplicate_actions,
        "check_nested_scrolling": check_nested_scrolling,
        "check_layout_grid": check_layout_grid,
    }
    if not all(isinstance(value, bool) for value in checks.values()):
        raise TypeError("Inspector check options must be bool values")
    return dict(
        inspectWidgetForBinding(
            widget,
            minimum_width,
            minimum_height,
            spacing_grid,
            check_clipped_text,
            check_accessibility_names,
            check_hit_areas,
            check_focus_order,
            check_duplicate_actions,
            check_nested_scrolling,
            check_layout_grid,
        )
    )


__all__ = [
    "Accordion",
    "AnchorEdge",
    "AnchorLayout",
    "AnchorSpec",
    "BackdropBackend",
    "BackdropCapabilities",
    "BackdropEffect",
    "BackdropFidelity",
    "BackdropState",
    "BackdropSurfaceMode",
    "AnnotatedScrollBar",
    "AnnotatedScrollBarLabel",
    "AutoSuggestBox",
    "Avatar",
    "Breadcrumb",
    "BreadcrumbItem",
    "Button",
    "BindingMode",
    "CalendarDatePicker",
    "CalendarView",
    "Card",
    "CheckBox",
    "ColorPicker",
    "ComboBox",
    "CommandBar",
    "CommandBarFlyout",
    "CoachMark",
    "CompoundButton",
    "ContentDialog",
    "ContentDialogButton",
    "CornerRadius",
    "DatePicker",
    "DataGrid",
    "Dialog",
    "Divider",
    "DrawerView",
    "DropDownButton",
    "EditingCommandRouter",
    "Expander",
    "Field",
    "FontIcon",
    "FontRole",
    "FlipView",
    "FlowView",
    "Flyout",
    "FluentMenu",
    "FluentMenuBar",
    "FluentMenuItem",
    "FluentWidget",
    "GridView",
    "HyperlinkButton",
    "InfoBadge",
    "InfoBar",
    "Icons",
    "IconSize",
    "Label",
    "LineEdit",
    "ListView",
    "NumberBox",
    "NavigationView",
    "PasswordBox",
    "TextEdit",
    "Pivot",
    "PivotItem",
    "PipsPager",
    "Popup",
    "ProgressBar",
    "ProgressRing",
    "RadioButton",
    "RatingControl",
    "RepeatButton",
    "ScrollBar",
    "ScrollView",
    "ScrollViewZoomAwareWidget",
    "SelectorBar",
    "SelectorBarItem",
    "SelectionMode",
    "Slider",
    "SplitButton",
    "Shimmer",
    "SplitView",
    "SplitViewPaneOptions",
    "Spacing",
    "StackView",
    "StackContentHost",
    "StateGroup",
    "TabView",
    "TabViewItem",
    "TeachingTip",
    "Theme",
    "ThemeTokens",
    "TimePicker",
    "TitleBar",
    "ToggleButton",
    "ToggleSplitButton",
    "ToggleSwitch",
    "Toast",
    "ToolTip",
    "TreeView",
    "Typography",
    "Window",
    "WidgetOwnership",
    "__api_version__",
    "__version__",
    "accent_color",
    "accentColor",
    "anchors",
    "apply_user_theme",
    "applyUserTheme",
    "bind",
    "binding_build_info",
    "bindingBuildInfo",
    "current_theme",
    "currentTheme",
    "font_scale",
    "font_for_role",
    "fontScale",
    "fontForRole",
    "initialize_resources",
    "initializeResources",
    "inspect_widget",
    "prepare_high_dpi_application",
    "prepareHighDpiApplication",
    "reset_theme_tokens",
    "resetThemeTokens",
    "set_accent_color",
    "set_font_scale",
    "set_theme",
    "setAccentColor",
    "setFontScale",
    "setTheme",
    "theme_revision",
    "themeRevision",
]
