#pragma once

#include <QFontMetrics>

namespace compatibility::detail {

// QLineEdit keeps one native vertical pixel on each side even without a frame,
// and its minimum hint also reserves positive leading. Leave styling padding
// and layout-grid policy to the caller.
// zh_CN: 无边框 QLineEdit 仍保留上下各一像素，并考虑正行距；样式留白与网格策略由调用方决定。
inline int singleLineEditMinimumTextHeight(const QFontMetrics& metrics)
{
    return metrics.height() + qMax(2, metrics.leading());
}

} // namespace compatibility::detail
