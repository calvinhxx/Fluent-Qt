#pragma once

#include <QFontMetrics>
#include <QLineEdit>
#include <QMargins>
#include <QStyle>
#include <QStyleOptionFrame>

namespace compatibility::detail {

// QLineEdit adds this inset after its style contents rectangle on Qt 5 and 6.
// zh_CN: Qt 5/6 的 QLineEdit 在样式内容矩形内额外保留此水平边距。
inline constexpr int lineEditHorizontalTextInset()
{
    return 2;
}

inline QMargins lineEditStylePadding(const QMargins& contentMargins)
{
    // QSS accepts negative padding: a zero content inset must also cancel the
    // native margin, without changing the caller's separate textMargins().
    // zh_CN: QSS 支持负 padding；内容边距为零时也抵消原生留白，不改调用方的 textMargins()。
    return QMargins(contentMargins.left() - lineEditHorizontalTextInset(), contentMargins.top(),
        contentMargins.right() - lineEditHorizontalTextInset(), contentMargins.bottom());
}

inline QRect lineEditGeometryForTextRect(const QLineEdit& editor, const QRect& textRect)
{
    QStyleOptionFrame option;
    option.initFrom(&editor);
    option.rect = editor.rect();
    option.lineWidth =
        editor.hasFrame() ? editor.style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &option, &editor) : 0;
    const QRect contents = editor.style()->subElementRect(QStyle::SE_LineEditContents, &option, &editor);
    // Explicit textMargins remain an additional caller-owned inset.
    // zh_CN: 显式 textMargins 仍作为调用方设置的额外留白，不被宿主几何抵消。
    const int left = contents.left() + lineEditHorizontalTextInset();
    const int right = editor.width() - contents.right() - 1 + lineEditHorizontalTextInset();
    return textRect.adjusted(-left, 0, right, 0);
}

// QLineEdit keeps one native vertical pixel on each side even without a frame,
// and its minimum hint also reserves positive leading. Leave styling padding
// and layout-grid policy to the caller.
// zh_CN: 无边框 QLineEdit 仍保留上下各一像素，并考虑正行距；样式留白与网格策略由调用方决定。
inline int singleLineEditMinimumTextHeight(const QFontMetrics& metrics)
{
    return metrics.height() + qMax(2, metrics.leading());
}

} // namespace compatibility::detail
