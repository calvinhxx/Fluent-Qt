#include "BindingApi.h"

#include <FluentQt/FluentQt.h>

#include <components/foundation/ThemeRegistry.h>

#include <QSysInfo>
#include <QtGlobal>

static_assert(
    static_cast<int>(fluent::binding::Theme::Light) ==
    static_cast<int>(fluent::FluentElement::Light));
static_assert(
    static_cast<int>(fluent::binding::Theme::Dark) ==
    static_cast<int>(fluent::FluentElement::Dark));
static_assert(
    static_cast<int>(fluent::binding::DesignLanguage::DesignFluent) ==
    static_cast<int>(fluent::FluentElement::DesignFluent));
static_assert(
    static_cast<int>(fluent::binding::DesignLanguage::DesignMaterial) ==
    static_cast<int>(fluent::FluentElement::DesignMaterial));
static_assert(
    static_cast<int>(fluent::binding::DesignLanguage::DesignCupertino) ==
    static_cast<int>(fluent::FluentElement::DesignCupertino));

void prepareHighDpiApplication() { fluent::prepareHighDpiApplication(); }

bool initializeResources() { return fluent::initializeResources(); }

QFont fontForRole(Typography::FontRole role) {
  return fluent::ThemeRegistry::instance().resolvedFontStyle(role).toQFont();
}

void setTheme(fluent::binding::Theme theme) {
  fluent::FluentElement::setTheme(
      static_cast<fluent::FluentElement::Theme>(static_cast<int>(theme)));
}

fluent::binding::Theme currentTheme() {
  return static_cast<fluent::binding::Theme>(
      static_cast<int>(fluent::FluentElement::currentTheme()));
}

void applyStyleTheme(fluent::StyleTheme theme) {
  fluent::StyleThemeCatalog::apply(theme);
}

void setAccentColor(const QColor &color) {
  fluent::StyleThemeCatalog::applyAccentOverride(color);
}

QColor accentColor() {
  const bool dark =
      fluent::FluentElement::currentTheme() == fluent::FluentElement::Dark;
  return fluent::ThemeRegistry::instance().colors(dark).accentDefault;
}

void resetThemeTokens() { fluent::ThemeRegistry::instance().resetToDefaults(); }

void setFontScale(qreal scale) {
  fluent::ThemeRegistry::instance().setFontScale(scale);
}

qreal fontScale() { return fluent::ThemeRegistry::instance().fontScale(); }

fluent::binding::DesignLanguage currentDesignLanguage() {
  return static_cast<fluent::binding::DesignLanguage>(
      static_cast<int>(fluent::ThemeRegistry::instance().designLanguage()));
}

int themeRevision() { return fluent::ThemeRegistry::instance().revision(); }

QVariantMap bindingBuildInfo() {
  QVariantMap info;
  info.insert(QStringLiteral("fluentqt_version"),
              QStringLiteral(FLUENT_QT_PYSIDE6_PROJECT_VERSION));
  info.insert(QStringLiteral("pyside6_version"),
              QStringLiteral(FLUENT_QT_PYSIDE6_VERSION));
  info.insert(QStringLiteral("shiboken6_version"),
              QStringLiteral(FLUENT_QT_SHIBOKEN6_VERSION));
  info.insert(QStringLiteral("shiboken6_generator_version"),
              QStringLiteral(FLUENT_QT_SHIBOKEN6_GENERATOR_VERSION));
  info.insert(QStringLiteral("qt_compile_version"),
              QStringLiteral(QT_VERSION_STR));
  info.insert(QStringLiteral("qt_runtime_version"),
              QString::fromLatin1(qVersion()));
  info.insert(QStringLiteral("architecture"),
              QSysInfo::currentCpuArchitecture());
  return info;
}
