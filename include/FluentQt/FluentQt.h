#ifndef FLUENTQT_FLUENTQT_H
#define FLUENTQT_FLUENTQT_H

namespace fluent {

/**
 * @brief Configures process-wide High-DPI behavior before QApplication exists.
 * zh_CN: 在 QApplication 创建前配置进程级 High-DPI 行为。
 *
 * On Qt 5 this enables device-independent scaling and high-resolution pixmaps.
 * On every supported Qt version it selects pass-through fractional scaling so
 * 125%, 150%, 200%, and 300% keep the operating system's requested size.
 * Call this before constructing QApplication; repeated pre-application calls
 * are harmless. A late call emits a warning and leaves global state unchanged.
 * zh_CN: Qt 5 下会启用设备无关缩放与高分辨率位图；所有受支持 Qt 版本都会采用
 * 分数缩放直通策略，使 125%、150%、200% 与 300% 保持系统请求的实际比例。
 * 必须在构造 QApplication 前调用；应用创建前重复调用是安全的。调用过晚时会输出
 * 警告，且不再修改全局状态。
 */
void prepareHighDpiApplication();

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
