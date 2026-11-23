#include "design/Typography.h"
#include "design/ThemeColors.h"

#include <type_traits>

// Windows RPC headers define this macro before some Qt 5 Quick headers include FluentElement.
#pragma push_macro("small")
#undef small
#define small char
#include "components/foundation/FluentElement.h"
static_assert(std::is_same_v<small, char>);
#undef small
static_assert(std::is_same_v<decltype(fluent::FluentElement::Spacing::small), int>);
#pragma pop_macro("small")

const QString* typographyProbeUiFamilyAddress()
{
    return &Typography::FontFamily::UI;
}

const QString* typographyProbeBackIconAddress()
{
    return &Typography::Icons::Back;
}

const Typography::FontStyle* typographyProbeBodyStyleAddress()
{
    return &Typography::Styles::Body;
}

const QColor* themeColorProbeLightAccentAddress()
{
    return &ThemeColors::Light::Fill::AccentDefault;
}

const std::vector<QColor>* themeColorProbeDarkChartsAddress()
{
    return &ThemeColors::Dark::Charts;
}
