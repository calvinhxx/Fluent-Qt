#include "design/Typography.h"
#include "design/ThemeColors.h"

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
