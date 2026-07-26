#include <FluentQt/BasicInput.h>
#include <FluentQt/Collections.h>
#include <FluentQt/DateTime.h>
#include <FluentQt/Design.h>
#include <FluentQt/DialogsFlyouts.h>
#include <FluentQt/Foundation.h>
#include <FluentQt/MenusToolbars.h>
#include <FluentQt/Navigation.h>
#include <FluentQt/Scrolling.h>
#include <FluentQt/StatusInfo.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/Windowing.h>

#include <type_traits>

static_assert(std::is_base_of<QWidget, fluent::basicinput::Button>::value,
              "BasicInput.h must expose the basic-input widgets");
static_assert(std::is_base_of<QWidget, fluent::collections::ListView>::value,
              "Collections.h must expose the collection widgets");
static_assert(std::is_base_of<QWidget, fluent::date_time::CalendarView>::value,
              "DateTime.h must expose the date/time widgets");
static_assert(std::is_base_of<QWidget, fluent::dialogs_flyouts::Popup>::value,
              "DialogsFlyouts.h must expose the overlay widgets");
static_assert(std::is_base_of<QWidget, fluent::navigation::NavigationView>::value,
              "Navigation.h must expose the navigation widgets");
static_assert(std::is_base_of<QWidget, fluent::scrolling::ScrollView>::value,
              "Scrolling.h must expose the scrolling widgets");
static_assert(std::is_base_of<QWidget, fluent::status_info::InfoBar>::value,
              "StatusInfo.h must expose the status widgets");
static_assert(std::is_base_of<QWidget, fluent::textfields::TextEdit>::value,
              "TextFields.h must expose the text-field widgets");
static_assert(std::is_base_of<QWidget, fluent::windowing::Window>::value,
              "Windowing.h must expose the window widgets");
