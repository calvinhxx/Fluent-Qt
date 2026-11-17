#pragma once

#include <QFontMetrics>
#include <QLineEdit>
#include <QMargins>
#include <QStyle>
#include <QStyleOptionFrame>
#include <QToolButton>
#include <QWidgetAction>

namespace compatibility::detail {

// QLineEdit adds this inset after its style contents rectangle on Qt 5 and 6.
// zh_CN: Qt 5/6 的 QLineEdit 在样式内容矩形内额外保留此水平边距。
inline constexpr int lineEditHorizontalTextInset()
{
    return 2;
}

// Native addAction() children live outside the QSS contents rectangle. Read
// their actual bounds instead of duplicating Qt's version/style-specific sizes.
// zh_CN: 原生 addAction() 子控件位于 QSS 内容矩形之外；读取实际边界，避免复制 Qt 版本/样式尺寸。
inline QMargins lineEditSideActionMargins(const QLineEdit& editor)
{
    QMargins margins;
    const auto actions = editor.actions();
    if (actions.isEmpty())
        return margins;
    const auto buttons = editor.findChildren<QToolButton*>(QString(), Qt::FindDirectChildrenOnly);
    auto reserve = [&](const QWidget* widget) {
        if (!widget || widget->parentWidget() != &editor || widget->isHidden())
            return;
        const QRect bounds = widget->geometry();
        if (bounds.center().x() < editor.width() / 2)
            margins.setLeft(qMax(margins.left(), bounds.right() + 1));
        else
            margins.setRight(qMax(margins.right(), editor.width() - bounds.left()));
    };
    for (QAction* action : actions) {
        if (!action->isVisible())
            continue;
        if (const auto* widgetAction = qobject_cast<QWidgetAction*>(action))
            reserve(widgetAction->defaultWidget());
        for (const QToolButton* button : buttons) {
            if (button->defaultAction() == action)
                reserve(button);
        }
    }
    return margins;
}

inline QMargins lineEditStylePadding(const QMargins& contentMargins)
{
    // QSS supports negative padding; subtract the native inset even at 0 margins without altering textMargins().
    // zh_CN: QSS 支持负 padding，内容边距为 0 时也可抵消原生留白，且不影响显式设置的 textMargins()。
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
    // Explicit textMargins remain an additional inner inset.
    // zh_CN: 显式 textMargins 作为额外的内侧留白保留。
    const int left = contents.left() + lineEditHorizontalTextInset();
    const int right = editor.width() - contents.right() - 1 + lineEditHorizontalTextInset();
    return textRect.adjusted(-left, 0, right, 0);
}

// Unframed QLineEdit still keeps 1px top/bottom inset and positive leading.
// zh_CN: 无边框 QLineEdit 仍保留上下各 1px 及正行距。
inline int singleLineEditMinimumTextHeight(const QFontMetrics& metrics)
{
    return metrics.height() + qMax(2, metrics.leading());
}

} // namespace compatibility::detail
