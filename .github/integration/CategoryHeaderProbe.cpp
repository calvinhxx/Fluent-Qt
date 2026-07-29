#include <FluentQt/BasicInput.h>
#include <FluentQt/Collections.h>
#include <FluentQt/DateTime.h>
#include <FluentQt/Design.h>
#include <FluentQt/DialogsFlyouts.h>
#include <FluentQt/Foundation.h>
#include <FluentQt/Layout.h>
#include <FluentQt/MenusToolbars.h>
#include <FluentQt/Navigation.h>
#include <FluentQt/Scrolling.h>
#include <FluentQt/StatusInfo.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/Windowing.h>

#include <type_traits>

static_assert(std::is_base_of<QWidget, fluent::basicinput::Button>::value,
              "BasicInput.h must expose the basic-input widgets");
static_assert(std::is_base_of<QWidget, fluent::basicinput::CompoundButton>::value,
              "BasicInput.h must expose CompoundButton");
static_assert(std::is_base_of<QWidget, fluent::collections::ListView>::value,
              "Collections.h must expose the collection widgets");
static_assert(std::is_base_of<QWidget, fluent::date_time::CalendarView>::value,
              "DateTime.h must expose the date/time widgets");
static_assert(std::is_base_of<QWidget, fluent::dialogs_flyouts::Popup>::value,
              "DialogsFlyouts.h must expose the overlay widgets");
static_assert(std::is_base_of<QWidget, fluent::FontIcon>::value,
              "Foundation.h must expose the foundation widgets");
static_assert(std::is_base_of<QWidget, fluent::layout::Accordion>::value,
              "Layout.h must expose Accordion");
static_assert(std::is_base_of<QWidget, fluent::layout::Card>::value,
              "Layout.h must expose the layout widgets");
static_assert(std::is_base_of<QWidget, fluent::layout::Divider>::value,
              "Layout.h must expose the layout widgets");
static_assert(std::is_base_of<QWidget, fluent::layout::Expander>::value,
              "Layout.h must expose the layout widgets");
static_assert(std::is_base_of<QWidget, fluent::menus_toolbars::CommandBar>::value,
              "MenusToolbars.h must expose CommandBar");
static_assert(
    std::is_base_of<
        fluent::dialogs_flyouts::Flyout,
        fluent::menus_toolbars::CommandBarFlyout>::value,
    "MenusToolbars.h must expose CommandBarFlyout");
static_assert(std::is_base_of<QWidget, fluent::navigation::NavigationView>::value,
              "Navigation.h must expose the navigation widgets");
static_assert(std::is_base_of<QWidget, fluent::scrolling::ScrollView>::value,
              "Scrolling.h must expose the scrolling widgets");
static_assert(std::is_base_of<QWidget, fluent::status_info::Avatar>::value,
              "StatusInfo.h must expose Avatar");
static_assert(std::is_base_of<QWidget, fluent::status_info::InfoBar>::value,
              "StatusInfo.h must expose the status widgets");
static_assert(std::is_base_of<QWidget, fluent::status_info::Toast>::value,
              "StatusInfo.h must expose the status widgets");
static_assert(std::is_base_of<QWidget, fluent::textfields::TextEdit>::value,
              "TextFields.h must expose the text-field widgets");
static_assert(
    std::is_base_of<
        QObject,
        fluent::textfields::EditingCommandRouter>::value,
    "TextFields.h must expose EditingCommandRouter");
static_assert(std::is_base_of<QWidget, fluent::windowing::Window>::value,
              "Windowing.h must expose the window widgets");
