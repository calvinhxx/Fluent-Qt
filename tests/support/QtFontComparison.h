#ifndef FLUENT_QT_QT_FONT_COMPARISON_H
#define FLUENT_QT_QT_FONT_COMPARISON_H

#include <QFont>
#include <QStringList>

namespace tests::support {

// Qt 5 may materialize the legacy family() as a singleton families() list when
// resolving a widget font. Canonicalize only that representation; callers still
// compare complete QFont values, including fallback lists and rendering options.
inline QFont normalizedFontFamilies(QFont font)
{
    if (font.families().isEmpty() && !font.family().isEmpty())
        font.setFamilies({font.family()});
    return font;
}

} // namespace tests::support

#endif // FLUENT_QT_QT_FONT_COMPARISON_H
