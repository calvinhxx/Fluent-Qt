#ifndef GALLERYSTYLESUPPORT_H
#define GALLERYSTYLESUPPORT_H

#include <QColor>
#include <QString>

namespace fluent::gallery {

/**
 * @brief Formats a QColor as a Qt style-sheet `rgba(...)` literal including alpha.
 * zh_CN: 将 QColor 格式化为带 alpha 的 Qt 样式表 `rgba(...)` 字面量。
 */
inline QString cssColor(const QColor& color)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

} // namespace fluent::gallery

#endif // GALLERYSTYLESUPPORT_H
