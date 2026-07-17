#ifndef FLUENTQT_FLUENTQT_H
#define FLUENTQT_FLUENTQT_H

namespace fluent {

/**
 * @brief Registers Fluent-Qt bundled resources and application fonts.
 * zh_CN: 注册 Fluent-Qt 内置资源与应用字体。
 *
 * Call this once after constructing QApplication in standalone applications so
 * the bundled open-source text and icon faces are available without depending
 * on platform font matching.
 * zh_CN: 独立应用应在构造 QApplication 后调用一次，使内置的开源文本与图标字体
 * 可用，且不依赖平台字体匹配。
 */
bool initializeResources();

} // namespace fluent

#include <components/basicinput/Button.h>
#include <components/basicinput/CheckBox.h>
#include <components/basicinput/ColorPicker.h>
#include <components/basicinput/ComboBox.h>
#include <components/basicinput/DropDownButton.h>
#include <components/basicinput/HyperlinkButton.h>
#include <components/basicinput/RadioButton.h>
#include <components/basicinput/RatingControl.h>
#include <components/basicinput/RepeatButton.h>
#include <components/basicinput/Slider.h>
#include <components/basicinput/SplitButton.h>
#include <components/basicinput/ToggleButton.h>
#include <components/basicinput/ToggleSplitButton.h>
#include <components/basicinput/ToggleSwitch.h>

#include <components/collections/DrawerView.h>
#include <components/collections/FlipView.h>
#include <components/collections/FlowView.h>
#include <components/collections/GridView.h>
#include <components/collections/ListView.h>
#include <components/collections/SplitView.h>
#include <components/collections/StackView.h>
#include <components/collections/TreeView.h>

#include <components/date_time/CalendarDatePicker.h>
#include <components/date_time/CalendarView.h>
#include <components/date_time/DatePicker.h>
#include <components/date_time/TimePicker.h>

#include <components/dialogs_flyouts/CoachMark.h>
#include <components/dialogs_flyouts/ContentDialog.h>
#include <components/dialogs_flyouts/Dialog.h>
#include <components/dialogs_flyouts/Flyout.h>
#include <components/dialogs_flyouts/Popup.h>
#include <components/dialogs_flyouts/TeachingTip.h>

#include <components/foundation/FluentElement.h>
#include <components/foundation/QMLPlus.h>
#include <components/foundation/StyleThemeCatalog.h>
#include <components/foundation/ThemeRegistry.h>

#include <components/menus_toolbars/Menu.h>
#include <components/menus_toolbars/MenuBar.h>

#include <components/navigation/Breadcrumb.h>
#include <components/navigation/NavigationView.h>
#include <components/navigation/Pivot.h>
#include <components/navigation/SelectorBar.h>
#include <components/navigation/StackContentHost.h>
#include <components/navigation/TabView.h>

#include <components/scrolling/AnnotatedScrollBar.h>
#include <components/scrolling/PipsPager.h>
#include <components/scrolling/ScrollBar.h>
#include <components/scrolling/ScrollView.h>

#include <components/status_info/InfoBadge.h>
#include <components/status_info/InfoBar.h>
#include <components/status_info/ProgressBar.h>
#include <components/status_info/ProgressRing.h>
#include <components/status_info/Shimmer.h>
#include <components/status_info/ToolTip.h>

#include <components/textfields/AutoSuggestBox.h>
#include <components/textfields/Label.h>
#include <components/textfields/LineEdit.h>
#include <components/textfields/NumberBox.h>
#include <components/textfields/PasswordBox.h>
#include <components/textfields/TextEdit.h>

#include <components/windowing/TitleBar.h>
#include <components/windowing/Window.h>
#include <components/windowing/WindowBackdrop.h>
#include <components/windowing/WindowBackdropMaterial.h>

#include <design/Animation.h>
#include <design/Breakpoints.h>
#include <design/CornerRadius.h>
#include <design/Elevation.h>
#include <design/IconCatalog.h>
#include <design/Material.h>
#include <design/Spacing.h>
#include <design/ThemeColors.h>
#include <design/Typography.h>

#endif // FLUENTQT_FLUENTQT_H
