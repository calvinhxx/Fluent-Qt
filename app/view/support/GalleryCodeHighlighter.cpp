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
    static const Palette light{QStringLiteral("#1F1F1F"), QStringLiteral("#0000FF"),
                               QStringLiteral("#267F99"), QStringLiteral("#795E26"),
                               QStringLiteral("#A31515"), QStringLiteral("#008000"),
                               QStringLiteral("#098658"), QStringLiteral("#800000")};
    static const Palette darkPalette{QStringLiteral("#D4D4D4"), QStringLiteral("#569CD6"),
                                     QStringLiteral("#4EC9B0"), QStringLiteral("#DCDCAA"),
                                     QStringLiteral("#CE9178"), QStringLiteral("#6A9955"),
                                     QStringLiteral("#B5CEA8"), QStringLiteral("#C586C0")};
    return dark ? darkPalette : light;
}

const QSet<QString>& cppKeywords()
{
    static const QSet<QString> keywords{
        QStringLiteral("alignas"),      QStringLiteral("alignof"),
        QStringLiteral("auto"),         QStringLiteral("bool"),
        QStringLiteral("break"),        QStringLiteral("case"),
        QStringLiteral("catch"),        QStringLiteral("char"),
        QStringLiteral("class"),        QStringLiteral("const"),
        QStringLiteral("constexpr"),    QStringLiteral("const_cast"),
        QStringLiteral("continue"),     QStringLiteral("decltype"),
        QStringLiteral("default"),      QStringLiteral("delete"),
        QStringLiteral("do"),           QStringLiteral("double"),
        QStringLiteral("dynamic_cast"), QStringLiteral("else"),
        QStringLiteral("enum"),         QStringLiteral("explicit"),
        QStringLiteral("export"),       QStringLiteral("extern"),
        QStringLiteral("false"),        QStringLiteral("final"),
        QStringLiteral("float"),        QStringLiteral("for"),
        QStringLiteral("friend"),       QStringLiteral("goto"),
        QStringLiteral("if"),           QStringLiteral("inline"),
        QStringLiteral("int"),          QStringLiteral("long"),
        QStringLiteral("mutable"),      QStringLiteral("namespace"),
        QStringLiteral("new"),          QStringLiteral("noexcept"),
        QStringLiteral("nullptr"),      QStringLiteral("operator"),
        QStringLiteral("override"),     QStringLiteral("private"),
        QStringLiteral("protected"),    QStringLiteral("public"),
        QStringLiteral("register"),     QStringLiteral("reinterpret_cast"),
        QStringLiteral("return"),       QStringLiteral("short"),
        QStringLiteral("signed"),       QStringLiteral("sizeof"),
        QStringLiteral("static"),       QStringLiteral("static_assert"),
        QStringLiteral("static_cast"),  QStringLiteral("struct"),
        QStringLiteral("switch"),       QStringLiteral("template"),
        QStringLiteral("this"),         QStringLiteral("throw"),
        QStringLiteral("true"),         QStringLiteral("try"),
        QStringLiteral("typedef"),      QStringLiteral("typeid"),
        QStringLiteral("typename"),     QStringLiteral("union"),
        QStringLiteral("unsigned"),     QStringLiteral("using"),
        QStringLiteral("virtual"),      QStringLiteral("void"),
        QStringLiteral("volatile"),     QStringLiteral("wchar_t"),
        QStringLiteral("while")};
    return keywords;
}

const QSet<QString>& pythonKeywords()
{
    static const QSet<QString> keywords{
        QStringLiteral("and"),      QStringLiteral("as"),       QStringLiteral("assert"),
        QStringLiteral("async"),    QStringLiteral("await"),    QStringLiteral("break"),
        QStringLiteral("class"),    QStringLiteral("continue"), QStringLiteral("def"),
        QStringLiteral("del"),      QStringLiteral("elif"),     QStringLiteral("else"),
        QStringLiteral("except"),   QStringLiteral("False"),    QStringLiteral("finally"),
        QStringLiteral("for"),      QStringLiteral("from"),     QStringLiteral("global"),
        QStringLiteral("if"),       QStringLiteral("import"),   QStringLiteral("in"),
        QStringLiteral("is"),       QStringLiteral("lambda"),   QStringLiteral("None"),
        QStringLiteral("nonlocal"), QStringLiteral("not"),      QStringLiteral("or"),
        QStringLiteral("pass"),     QStringLiteral("raise"),    QStringLiteral("return"),
        QStringLiteral("True"),     QStringLiteral("try"),      QStringLiteral("while"),
        QStringLiteral("with"),     QStringLiteral("yield")};
    return keywords;
}

QString escapeChar(QChar c)
{
    switch (c.unicode()) {
    case '&':
        return QStringLiteral("&amp;");
    case '<':
        return QStringLiteral("&lt;");
    case '>':
        return QStringLiteral("&gt;");
    case '"':
        return QStringLiteral("&quot;");
    default:
        return QString(c);
    }
}

// Preserve indentation and repeated spaces without making entire source lines
// unbreakable. State spans token boundaries, including CRLF split by a comment.
class CodeEscaper {
public:
    explicit CodeEscaper(CodeSourceMap* sourceMap) : m_sourceMap(sourceMap)
    {
        if (m_sourceMap)
            m_sourceMap->clear();
    }

    QString escape(const QString& text)
    {
        QString out;
        out.reserve(text.size());
        for (int i = 0; i < text.size(); ++i) {
            const QChar c = text.at(i);
            if (c == QLatin1Char('\n') && m_previousWasCarriageReturn) {
                m_previousWasCarriageReturn = false;
                ++m_sourceOffset;
                if (m_sourceMap && !m_sourceMap->isEmpty())
                    m_sourceMap->last().end = m_sourceOffset;
                continue;
            }
            m_previousWasCarriageReturn = c == QLatin1Char('\r');
            if (c == QLatin1Char('\r') || c == QLatin1Char('\n')) {
                recordSourceCharacter();
                out += QStringLiteral("<br/>");
                m_lineStart = true;
            } else if (c == QLatin1Char(' ') || c == QLatin1Char('\t')) {
                int width = c == QLatin1Char('\t') ? 4 : 1;
                recordSourceCharacter(width);
                while (i + 1 < text.size() && (text.at(i + 1) == QLatin1Char(' ') ||
                                               text.at(i + 1) == QLatin1Char('\t'))) {
                    const int nextWidth = text.at(++i) == QLatin1Char('\t') ? 4 : 1;
                    recordSourceCharacter(nextWidth);
                    width += nextWidth;
                }
                out += QStringLiteral("&nbsp;").repeated(m_lineStart ? width : width - 1);
                if (!m_lineStart)
                    out += QLatin1Char(' ');
            } else {
                recordSourceCharacter();
                out += escapeChar(c);
                m_lineStart = false;
            }
        }
        return out;
    }

    QString span(const QString& color, const QString& text)
    {
        return QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(color, escape(text));
    }

    QString wrapOpportunity()
    {
        if (m_sourceMap)
            m_sourceMap->append({m_sourceOffset, m_sourceOffset});
        return QStringLiteral("&#8203;");
    }

private:
    void recordSourceCharacter(int renderedWidth = 1)
    {
        if (m_sourceMap) {
            for (int i = 0; i < renderedWidth; ++i)
                m_sourceMap->append({m_sourceOffset, m_sourceOffset + 1});
        }
        ++m_sourceOffset;
    }

    CodeSourceMap* m_sourceMap;
    int m_sourceOffset = 0;
    bool m_lineStart = true;
    bool m_previousWasCarriageReturn = false;
};

int appendSeparator(const QString& code, int offset, bool cpp, CodeEscaper& escaper, QString& html,
                    int& qualifiedDepth)
{
    const QChar c = code.at(offset);
    if (c.isSpace()) {
        int end = offset + 1;
        while (end < code.size() && code.at(end).isSpace())
            ++end;
        html += escaper.escape(code.mid(offset, end - offset));
        qualifiedDepth = 0;
        return end;
    }

    const bool qualifier = cpp ? c == QLatin1Char(':') && offset + 1 < code.size() &&
                                     code.at(offset + 1) == QLatin1Char(':')
                               : c == QLatin1Char('.');
    const int length = qualifier && cpp ? 2 : 1;
    html += escaper.escape(code.mid(offset, length));
    // Keep the root/type pair together, but allow nested names and argument
    // boundaries to wrap in compact windows. The copied source is unchanged.
    if (qualifier) {
        if (++qualifiedDepth >= 2)
            html += escaper.wrapOpportunity();
    } else {
        qualifiedDepth = 0;
        if (c == QLatin1Char(','))
            html += escaper.wrapOpportunity();
    }
    return offset + length;
}

bool isIdentStart(QChar c)
{
    return c.isLetter() || c == QLatin1Char('_');
}
bool isIdentPart(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

} // namespace

QString highlightCppToHtml(const QString& code, bool darkTheme, CodeSourceMap* sourceMap)
{
    const Palette& pal = paletteFor(darkTheme);
    const QSet<QString>& keywords = cppKeywords();

    QString html;
    html.reserve(code.size() * 2);
    CodeEscaper escaper(sourceMap);
    int qualifiedDepth = 0;

    const int n = code.size();
    int i = 0;
    while (i < n) {
        const QChar c = code.at(i);

        // Line comment.
        if (c == QLatin1Char('/') && i + 1 < n && code.at(i + 1) == QLatin1Char('/')) {
            int j = i;
            while (j < n && code.at(j) != QLatin1Char('\n'))
                ++j;
            html += escaper.span(pal.comment, code.mid(i, j - i));
            i = j;
            continue;
        }
        // Block comment.
        if (c == QLatin1Char('/') && i + 1 < n && code.at(i + 1) == QLatin1Char('*')) {
            int j = i + 2;
            while (j + 1 < n &&
                   !(code.at(j) == QLatin1Char('*') && code.at(j + 1) == QLatin1Char('/')))
                ++j;
            j = qMin(n, j + 2);
            html += escaper.span(pal.comment, code.mid(i, j - i));
            i = j;
            continue;
        }
        // Preprocessor directive (to end of line).
        if (c == QLatin1Char('#')) {
            int j = i;
            while (j < n && code.at(j) != QLatin1Char('\n'))
                ++j;
            html += escaper.span(pal.preprocessor, code.mid(i, j - i));
            i = j;
            continue;
        }
        // String / char literal (with escape handling).
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            const QChar quote = c;
            int j = i + 1;
            while (j < n) {
                if (code.at(j) == QLatin1Char('\\') && j + 1 < n) {
                    j += 2;
                    continue;
                }
                if (code.at(j) == quote) {
                    ++j;
                    break;
                }
                if (code.at(j) == QLatin1Char('\n'))
                    break;
                ++j;
            }
            html += escaper.span(pal.string, code.mid(i, j - i));
            i = j;
            continue;
        }
        // Number.
        if (c.isDigit()) {
            int j = i;
            while (j < n && (isIdentPart(code.at(j)) || code.at(j) == QLatin1Char('.')))
                ++j;
            html += escaper.span(pal.number, code.mid(i, j - i));
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
                html += escaper.span(pal.keyword, word);
            else if (isCall)
                html += escaper.span(pal.function, word);
            else if (word.at(0).isUpper())
                html += escaper.span(pal.type, word);
            else
                html += escaper.escape(word); // plain identifier uses the base text color
            i = j;
            continue;
        }
        // Whitespace and operators/punctuation: base text color.
        i = appendSeparator(code, i, true, escaper, html, qualifiedDepth);
    }

    // Wrap in the default color so unspanned text (plain identifiers, operators) is themed too,
    // independent of the hosting label's own color.
    // zh_CN: 用默认色整体包一层，让未上 span 的文本（普通标识符、运算符）也随主题着色，
    // 与宿主 label 自身的颜色无关。
    return QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(pal.text, html);
}

QString highlightPythonToHtml(const QString& code, bool darkTheme, CodeSourceMap* sourceMap)
{
    const Palette& pal = paletteFor(darkTheme);
    const QSet<QString>& keywords = pythonKeywords();

    QString html;
    html.reserve(code.size() * 2);
    CodeEscaper escaper(sourceMap);
    int qualifiedDepth = 0;

    const int n = code.size();
    int i = 0;
    while (i < n) {
        const QChar c = code.at(i);

        if (c == QLatin1Char('#')) {
            int j = i;
            while (j < n && code.at(j) != QLatin1Char('\n'))
                ++j;
            html += escaper.span(pal.comment, code.mid(i, j - i));
            i = j;
            continue;
        }

        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            const QChar quote = c;
            const bool triple = i + 2 < n && code.at(i + 1) == quote && code.at(i + 2) == quote;
            int j = i + (triple ? 3 : 1);
            while (j < n) {
                if (code.at(j) == QLatin1Char('\\') && j + 1 < n) {
                    j += 2;
                    continue;
                }
                if (triple) {
                    if (j + 2 < n && code.at(j) == quote && code.at(j + 1) == quote &&
                        code.at(j + 2) == quote) {
                        j += 3;
                        break;
                    }
                } else if (code.at(j) == quote) {
                    ++j;
                    break;
                } else if (code.at(j) == QLatin1Char('\n')) {
                    break;
                }
                ++j;
            }
            html += escaper.span(pal.string, code.mid(i, j - i));
            i = j;
            continue;
        }

        if (c.isDigit()) {
            int j = i;
            while (j < n && (isIdentPart(code.at(j)) || code.at(j) == QLatin1Char('.')))
                ++j;
            html += escaper.span(pal.number, code.mid(i, j - i));
            i = j;
            continue;
        }

        if (c == QLatin1Char('@')) {
            int j = i + 1;
            while (j < n && (isIdentPart(code.at(j)) || code.at(j) == QLatin1Char('.')))
                ++j;
            html += escaper.span(pal.preprocessor, code.mid(i, j - i));
            i = j;
            continue;
        }

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
                html += escaper.span(pal.keyword, word);
            else if (isCall)
                html += escaper.span(pal.function, word);
            else if (word.at(0).isUpper())
                html += escaper.span(pal.type, word);
            else
                html += escaper.escape(word);
            i = j;
            continue;
        }

        i = appendSeparator(code, i, false, escaper, html, qualifiedDepth);
    }

    return QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(pal.text, html);
}

} // namespace fluent::gallery
