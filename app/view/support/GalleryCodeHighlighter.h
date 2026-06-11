#ifndef GALLERYCODEHIGHLIGHTER_H
#define GALLERYCODEHIGHLIGHTER_H

#include <QString>

namespace fluent::gallery {

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
QString highlightCppToHtml(const QString& code, bool darkTheme);

} // namespace fluent::gallery

#endif // GALLERYCODEHIGHLIGHTER_H
