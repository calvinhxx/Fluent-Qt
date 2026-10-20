#ifndef FLUENTQT_CLIPBOARDCOMPAT_P_H
#define FLUENTQT_CLIPBOARDCOMPAT_P_H
#include <functional>
#include <QString>
class QObject;
namespace compatibility::detail {
// Completion is suppressed if the requesting context is destroyed.
// zh_CN: 发起请求的上下文销毁后不再回调。
void copyText(QObject* context, const QString& text, std::function<void(bool)> completed);
} // namespace compatibility::detail
#endif
