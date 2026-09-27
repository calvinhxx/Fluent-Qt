#ifndef FLUENT_QT_QT_FONT_COMPARISON_H
#define FLUENT_QT_QT_FONT_COMPARISON_H

#include <QFont>
#include <QStringList>

namespace tests::support {

// Qt 5 may materialize the legacy family() as a singleton families() list when
// resolving a widget font, or repeat that family while resolving a child font.
// Canonicalize only those representations; callers still compare complete QFont
// values, preserving distinct fallback names/order and rendering options.
inline QFont normalizedFontFamilies(QFont font)
{
    QStringList families = font.families();
    if (families.isEmpty() && !font.family().isEmpty())
        families.append(font.family());
    families.removeDuplicates(); // Exact, case-sensitive duplicates only.
    if (families != font.families())
        font.setFamilies(families);
    return font;
}

} // namespace tests::support

#endif // FLUENT_QT_QT_FONT_COMPARISON_H
