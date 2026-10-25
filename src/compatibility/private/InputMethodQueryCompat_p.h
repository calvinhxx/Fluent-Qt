#pragma once

#include <QPointer>
#include <QRectF>
#include <QVariant>
#include <QWidget>

namespace compatibility::detail {

// Delegate a focused companion widget's IME queries to its native editor,
// translating only the rectangle-valued answers into the companion's space.
// zh_CN: 将焦点辅助控件的输入法查询交给原生编辑器，仅转换矩形答案的坐标。
inline QVariant forwardedInputMethodQuery(const QWidget* receiver, const QWidget* editor, Qt::InputMethodQuery query)
{
    if (!receiver || !editor)
        return {};
    QPointer<const QWidget> receiverGuard(receiver);
    QPointer<const QWidget> editorGuard(editor);
    QVariant value = editor->inputMethodQuery(query);
    if (!receiverGuard || !editorGuard)
        return {};
    if (value.isValid()
        && (query == Qt::ImCursorRectangle || query == Qt::ImAnchorRectangle
            || query == Qt::ImInputItemClipRectangle)) {
        const QPoint offset = editor->mapToGlobal(QPoint()) - receiver->mapToGlobal(QPoint());
        value = value.toRectF().translated(offset);
    }
    return value;
}

} // namespace compatibility::detail
