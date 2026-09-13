#pragma once

#include <QComboBox>
#include <QPointer>

namespace fluent::compatibility {

/**
 * @brief Emits current and legacy text activation signals while guarding sender lifetime.
 * zh_CN: 发出当前及旧版文本激活信号，并保护信号发送者的生命周期。
 */
inline void emitComboBoxTextActivated(QComboBox* comboBox, const QString& text)
{
    QPointer<QComboBox> guard(comboBox);
    if (!guard)
        return;
    emit guard->textActivated(text);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0) && QT_DEPRECATED_SINCE(5, 15)
    if (!guard)
        return;
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    emit guard->activated(text);
    QT_WARNING_POP
#endif
}

} // namespace fluent::compatibility
