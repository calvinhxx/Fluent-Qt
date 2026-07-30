#ifndef FLUENTQT_PYSIDE6_BINDINGAPI_H
#define FLUENTQT_PYSIDE6_BINDINGAPI_H

#include <QFont>
#include <QVariantMap>

#include <components/basicinput/Button.h>
#include <components/basicinput/CheckBox.h>
#include <components/basicinput/HyperlinkButton.h>
#include <components/basicinput/RadioButton.h>
#include <components/basicinput/RepeatButton.h>
#include <components/basicinput/Slider.h>
#include <components/basicinput/ToggleButton.h>
#include <components/basicinput/ToggleSwitch.h>
#include <components/foundation/StyleThemeCatalog.h>
#include <components/layout/Divider.h>
#include <components/status_info/InfoBadge.h>
#include <components/status_info/ProgressBar.h>
#include <components/status_info/ProgressRing.h>
#include <components/status_info/Shimmer.h>
#include <components/textfields/Label.h>
#include <components/textfields/LineEdit.h>
#include <components/textfields/NumberBox.h>
#include <components/textfields/PasswordBox.h>
#include <components/windowing/Window.h>
#include <design/Typography.h>

namespace fluent::binding {

enum class Theme {
    Light,
    Dark
};

enum class DesignLanguage {
    DesignFluent,
    DesignMaterial,
    DesignCupertino
};

} // namespace fluent::binding

void prepareHighDpiApplication();
bool initializeResources();
QFont fontForRole(Typography::FontRole role);
void setTheme(fluent::binding::Theme theme);
fluent::binding::Theme currentTheme();
void applyStyleTheme(fluent::StyleTheme theme);
void setAccentColor(const QColor &color);
QColor accentColor();
void resetThemeTokens();
void setFontScale(qreal scale);
qreal fontScale();
fluent::binding::DesignLanguage currentDesignLanguage();
int themeRevision();
QVariantMap bindingBuildInfo();

#endif // FLUENTQT_PYSIDE6_BINDINGAPI_H
