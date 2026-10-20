#ifndef FLUENTQT_DESIGN_CODETOKENS_P_H
#define FLUENTQT_DESIGN_CODETOKENS_P_H
#include <QString>
namespace fluent::code_tokens {
// VSCode-style token palettes (light = "Light+", dark = "Dark+").
// zh_CN: VSCode 风格的 token 配色（light = "Light+"，dark = "Dark+"）。
struct Palette {
    QString text;
    QString keyword;
    QString type;
    QString function;
    QString string;
    QString comment;
    QString number;
    QString preprocessor;
};

inline const Palette& paletteFor(bool dark)
{
    static const Palette light{QStringLiteral("#1F1F1F"), QStringLiteral("#0000FF"), QStringLiteral("#267F99"),
        QStringLiteral("#795E26"), QStringLiteral("#A31515"), QStringLiteral("#008000"), QStringLiteral("#098658"),
        QStringLiteral("#800000")};
    static const Palette darkPalette{QStringLiteral("#D4D4D4"), QStringLiteral("#569CD6"), QStringLiteral("#4EC9B0"),
        QStringLiteral("#DCDCAA"), QStringLiteral("#CE9178"), QStringLiteral("#6A9955"), QStringLiteral("#B5CEA8"),
        QStringLiteral("#C586C0")};
    return dark ? darkPalette : light;
}

} // namespace fluent::code_tokens
#endif
