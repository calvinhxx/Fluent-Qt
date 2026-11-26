#include <FluentQt/FluentQt.h>
#include <FluentQt/Diagnostics.h>

#include <type_traits>

static_assert(std::is_base_of_v<QWidget, fluent::basicinput::Button>);
