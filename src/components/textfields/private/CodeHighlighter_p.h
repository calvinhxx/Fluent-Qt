#ifndef FLUENTQT_CODEHIGHLIGHTER_P_H
#define FLUENTQT_CODEHIGHLIGHTER_P_H

#include <QString>
#include <QVector>

namespace fluent::textfields::detail {

// Half-open source range for one rendered UTF-16 character. Empty ranges mark
// synthetic wrapping characters; expanded tabs share their original range.
struct CodeSourceSpan {
    int start;
    int end;
};
using CodeSourceMap = QVector<CodeSourceSpan>;

/**
 * @brief Renders a C++ snippet as color-highlighted HTML (VSCode-like palette).
 * zh_CN: 将 C++ 片段渲染为带语法配色的 HTML（接近 VSCode 的配色）。
 *
 * Produces rich text suitable for a QLabel: keywords, types, functions, strings, comments,
 * numbers and preprocessor lines are colored; whitespace is preserved with &nbsp;/<br>. The
 * palette switches between the light and dark VSCode-style themes.
 * zh_CN: 输出适合 QLabel 的富文本：关键字、类型、函数、字符串、注释、数字、预处理行分别上色；
 * 空白用 &nbsp;/<br> 保留。配色在明/暗两套 VSCode 风格之间切换。
 */
QString highlightCppToHtml(const QString& code, bool darkTheme, CodeSourceMap* sourceMap = nullptr);

/** @brief Highlights a QML file without treating it as a host-language string.
 * zh_CN: 对独立 QML 文件进行高亮，不将其视为宿主语言字符串。 */
QString highlightQmlToHtml(const QString& code, bool darkTheme, CodeSourceMap* sourceMap = nullptr);

/**
 * @brief Renders a Python snippet with the same light/dark token palette.
 * zh_CN: 使用同一套明暗 token 配色渲染 Python 片段。
 */
QString highlightPythonToHtml(const QString& code, bool darkTheme, CodeSourceMap* sourceMap = nullptr);

QString highlightPlainToHtml(const QString& code, const QString& color, CodeSourceMap* sourceMap = nullptr);
QString
highlightCodeToHtml(const QString& code, const QString& language, bool dark, CodeSourceMap* sourceMap = nullptr);

} // namespace fluent::textfields::detail

#endif // FLUENTQT_CODEHIGHLIGHTER_P_H
