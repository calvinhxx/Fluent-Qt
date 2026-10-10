#include "QmlTheme.h"

#include "QmlView.h"
#include "components/foundation/MotionPolicy.h"

namespace fluent::qmlbridge {
QmlTheme::QmlTheme(QmlView* view) : QObject(view), m_view(view)
{
    connect(&MotionPolicy::instance(), &MotionPolicy::modeChanged, this, &QmlTheme::changed);
}
QColor QmlTheme::background() const
{
    return m_view->themeColorsRef().bgCanvas;
}
QColor QmlTheme::surface() const
{
    return m_view->themeColorsRef().bgLayer;
}
QColor QmlTheme::surfaceAlt() const
{
    return m_view->themeColorsRef().bgLayerAlt;
}
QColor QmlTheme::text() const
{
    return m_view->themeColorsRef().textPrimary;
}
QColor QmlTheme::secondaryText() const
{
    return m_view->themeColorsRef().textSecondary;
}
QColor QmlTheme::accent() const
{
    return m_view->themeColorsRef().accentDefault;
}
QColor QmlTheme::onAccent() const
{
    return m_view->themeColorsRef().textOnAccent;
}
QColor QmlTheme::border() const
{
    return m_view->themeColorsRef().strokeCard;
}
bool QmlTheme::isDark() const
{
    return m_view->effectiveThemeUsesDarkAppearance();
}
bool QmlTheme::areAnimationsEnabled() const
{
    return MotionPolicy::instance().shouldAnimate();
}
QString QmlTheme::fontFamily() const
{
    return m_view->themeFont().family;
}
int QmlTheme::fontPixelSize() const
{
    return m_view->themeFont().size;
}
int QmlTheme::cornerRadius() const
{
    return m_view->themeRadius().control;
}
int QmlTheme::transitionDuration() const
{
    return MotionPolicy::instance().resolvedDuration(m_view->themeAnimation().normal);
}
} // namespace fluent::qmlbridge
