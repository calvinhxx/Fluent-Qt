#include "GalleryCodeHighlighter.h"

#include <QChar>
#include <QSet>

namespace fluent::gallery {
namespace {

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

const Palette& paletteFor(bool dark)
{
    static const Palette light{
        QStringLiteral("#1F1F1F"), QStringLiteral("#0000FF"), QStringLiteral("#267F99"),
        QStringLiteral("#795E26"), QStringLiteral("#A31515"), QStringLiteral("#008000"),
        QStringLiteral("#098658"), QStringLiteral("#800000")};
    static const Palette darkPalette{
        QStringLiteral("#D4D4D4"), QStringLiteral("#569CD6"), QStringLiteral("#4EC9B0"),
        QStringLiteral("#DCDCAA"), QStringLiteral("#CE9178"), QStringLiteral("#6A9955"),
        QStringLiteral("#B5CEA8"), QStringLiteral("#C586C0")};
    return dark ? darkPalette : light;
}

const QSet<QString>& cppKeywords()
{
    static const QSet<QString> keywords{
        QStringLiteral("alignas"), QStringLiteral("alignof"), QStringLiteral("auto"),
        QStringLiteral("bool"), QStringLiteral("break"), QStringLiteral("case"),
        QStringLiteral("catch"), QStringLiteral("char"), QStringLiteral("class"),
        QStringLiteral("const"), QStringLiteral("constexpr"), QStringLiteral("const_cast"),
        QStringLiteral("continue"), QStringLiteral("decltype"), QStringLiteral("default"),
        QStringLiteral("delete"), QStringLiteral("do"), QStringLiteral("double"),
        QStringLiteral("dynamic_cast"), QStringLiteral("else"), QStringLiteral("enum"),
        QStringLiteral("explicit"), QStringLiteral("export"), QStringLiteral("extern"),
        QStringLiteral("false"), QStringLiteral("final"), QStringLiteral("float"),
        QStringLiteral("for"), QStringLiteral("friend"), QStringLiteral("goto"),
        QStringLiteral("if"), QStringLiteral("inline"), QStringLiteral("int"),
        QStringLiteral("long"), QStringLiteral("mutable"), QStringLiteral("namespace"),
        QStringLiteral("new"), QStringLiteral("noexcept"), QStringLiteral("nullptr"),
        QStringLiteral("operator"), QStringLiteral("override"), QStringLiteral("private"),
        QStringLiteral("protected"), QStringLiteral("public"), QStringLiteral("register"),
        QStringLiteral("reinterpret_cast"), QStringLiteral("return"), QStringLiteral("short"),
        QStringLiteral("signed"), QStringLiteral("sizeof"), QStringLiteral("static"),
        QStringLiteral("static_assert"), QStringLiteral("static_cast"), QStringLiteral("struct"),
        QStringLiteral("switch"), QStringLiteral("template"), QStringLiteral("this"),
        QStringLiteral("throw"), QStringLiteral("true"), QStringLiteral("try"),
        QStringLiteral("typedef"), QStringLiteral("typeid"), QStringLiteral("typename"),
        QStringLiteral("union"), QStringLiteral("unsigned"), QStringLiteral("using"),
        QStringLiteral("virtual"), QStringLiteral("void"), QStringLiteral("volatile"),
        QStringLiteral("wchar_t"), QStringLiteral("while")};
    return keywords;
}

QString escapeChar(QChar c)
{
    switch (c.unicode()) {
    case '&': return QStringLiteral("&amp;");
    case '<': return QStringLiteral("&lt;");
    case '>': return QStringLiteral("&gt;");
    case '"': return QStringLiteral("&quot;");
    case ' ': return QStringLiteral("&nbsp;");
    case '\t': return QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;");
    case '\n': return QStringLiteral("<br/>");
    default: return QString(c);
    }
}

QString escape(const QString& text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text)
        out += escapeChar(c);
    return out;
}

QString span(const QString& color, const QString& text)
{
    return QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(color, escape(text));
}

bool isIdentStart(QChar c) { return c.isLetter() || c == QLatin1Char('_'); }
bool isIdentPart(QChar c) { return c.isLetterOrNumber() || c == QLatin1Char('_'); }

} // namespace

QString highlightCppToHtml(const QString& code, bool darkTheme)
{
    const Palette& pal = paletteFor(darkTheme);
    const QSet<QString>& keywords = cppKeywords();

    QString html;
    html.reserve(code.size() * 2);

    const int n = code.size();
    int i = 0;
    while (i < n) {
        const QChar c = code.at(i);

        // Line comment.
        if (c == QLatin1Char('/') && i + 1 < n && code.at(i + 1) == QLatin1Char('/')) {
            int j = i;
            while (j < n && code.at(j) != QLatin1Char('\n'))
                ++j;
            html += span(pal.comment, code.mid(i, j - i));
            i = j;
            continue;
        }
        // Block comment.
        if (c == QLatin1Char('/') && i + 1 < n && code.at(i + 1) == QLatin1Char('*')) {
            int j = i + 2;
            while (j + 1 < n && !(code.at(j) == QLatin1Char('*') && code.at(j + 1) == QLatin1Char('/')))
                ++j;
            j = qMin(n, j + 2);
            html += span(pal.comment, code.mid(i, j - i));
            i = j;
            continue;
        }
        // Preprocessor directive (to end of line).
        if (c == QLatin1Char('#')) {
            int j = i;
            while (j < n && code.at(j) != QLatin1Char('\n'))
                ++j;
            html += span(pal.preprocessor, code.mid(i, j - i));
            i = j;
            continue;
        }
        // String / char literal (with escape handling).
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            const QChar quote = c;
            int j = i + 1;
            while (j < n) {
                if (code.at(j) == QLatin1Char('\\') && j + 1 < n) { j += 2; continue; }
                if (code.at(j) == quote) { ++j; break; }
                if (code.at(j) == QLatin1Char('\n')) break;
                ++j;
            }
            html += span(pal.string, code.mid(i, j - i));
            i = j;
            continue;
        }
        // Number.
        if (c.isDigit()) {
            int j = i;
            while (j < n && (isIdentPart(code.at(j)) || code.at(j) == QLatin1Char('.')))
                ++j;
            html += span(pal.number, code.mid(i, j - i));
            i = j;
            continue;
        }
        // Identifier / keyword / type / function.
        if (isIdentStart(c)) {
            int j = i;
            while (j < n && isIdentPart(code.at(j)))
                ++j;
            const QString word = code.mid(i, j - i);
            int k = j;
            while (k < n && code.at(k).isSpace())
                ++k;
            const bool isCall = k < n && code.at(k) == QLatin1Char('(');
            if (keywords.contains(word))
                html += span(pal.keyword, word);
            else if (isCall)
                html += span(pal.function, word);
            else if (word.at(0).isUpper())
                html += span(pal.type, word);
            else
                html += escape(word);  // plain identifier uses the base text color
            i = j;
            continue;
        }
        // Whitespace and operators/punctuation: base text color.
        html += escapeChar(c);
        ++i;
    }

    // Wrap in the default color so unspanned text (plain identifiers, operators) is themed too,
    // independent of the hosting label's own color.
    // zh_CN: 用默认色整体包一层，让未上 span 的文本（普通标识符、运算符）也随主题着色，
    // 与宿主 label 自身的颜色无关。
    return QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(pal.text, html);
}

} // namespace fluent::gallery
