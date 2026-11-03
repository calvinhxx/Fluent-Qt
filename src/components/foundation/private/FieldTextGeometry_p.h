#pragma once

#include <QMargins>
#include <QRect>
#include <QStyle>

namespace fluent::detail {

// Margins describe the logical leading/trailing text edges. Native editors
// adapt their internal padding separately; adornments reserve their own space.
// zh_CN: 边距描述逻辑首尾文字边缘；原生编辑器另行适配内部留白，附加控件自行预留空间。
inline QRect fieldTextRect(const QRect& bounds, Qt::LayoutDirection direction, const QMargins& margins)
{
    return QStyle::visualRect(direction, bounds, bounds.marginsRemoved(margins));
}

} // namespace fluent::detail
