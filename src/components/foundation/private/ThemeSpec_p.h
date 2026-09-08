#ifndef FLUENT_THEME_SPEC_P_H
#define FLUENT_THEME_SPEC_P_H

#include <QJsonObject>
#include "components/foundation/ThemeRegistry.h"

namespace fluent::detail {
bool validateThemeSpec(const QJsonObject& spec);
void applyThemeSpec(ThemeRegistry::ExtendedSnapshot& snapshot, const QJsonObject& spec);
} // namespace fluent::detail

#endif
