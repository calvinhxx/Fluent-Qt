#ifndef FLUENTQT_CLIPBOARDCOMPAT_P_H
#define FLUENTQT_CLIPBOARDCOMPAT_P_H
#include <functional>
#include <QString>
class QObject;
class QImage;
namespace compatibility::detail {
// Query immediately after a native clipboard write; cached MIME data is not proof on Windows.
// zh_CN: 原生剪贴板写入后立即确认结果；Windows 上的 MIME 缓存不能证明写入成功。
bool clipboardWriteSucceeded();
// Completion is suppressed if the requesting context is destroyed.
// zh_CN: 发起请求的上下文销毁后不再回调。
void copyText(QObject* context, const QString& text, std::function<void(bool)> completed);
void copyImage(QObject* context, const QImage& image, std::function<void(bool)> completed);
} // namespace compatibility::detail
#endif
