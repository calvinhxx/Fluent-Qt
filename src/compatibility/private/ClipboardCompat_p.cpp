#include "ClipboardCompat_p.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QHash>
#include <QPointer>
#include <QObject>
#include <utility>
#if defined(Q_OS_WASM)
#include <emscripten.h>
#endif

namespace compatibility::detail {
#if defined(Q_OS_WASM)
namespace {
struct ClipboardRequest {
    QPointer<QObject> context;
    std::function<void(bool)> completed;
    QMetaObject::Connection destroyedConnection;
};

QHash<quint32, ClipboardRequest> clipboardRequests;
quint32 nextClipboardRequest = 0;

// clang-format off
EM_JS(void, fluentQtCopyTextToClipboard, (unsigned int request, const char* utf8), {
    const complete = success => Module['_fluentQtClipboardCopyCompleted'](request, success ? 1 : 0);
    try {
        const text = UTF8ToString(utf8);
        if (!navigator.clipboard || !navigator.clipboard.writeText) {
            complete(false);
            return;
        }
        // Invoke in the original input turn, before awaiting browser permission.
        navigator.clipboard.writeText(text).then(() => complete(true), () => complete(false));
    } catch (_) {
        complete(false);
    }
});

// clang-format on
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void fluentQtClipboardCopyCompleted(unsigned int request, int success)
{
    const auto found = clipboardRequests.find(request);
    if (found == clipboardRequests.end())
        return;
    const ClipboardRequest pending = found.value();
    clipboardRequests.erase(found);
    QObject::disconnect(pending.destroyedConnection);
    if (pending.context)
        pending.completed(success != 0);
}

void copyText(QObject* context, const QString& text, std::function<void(bool)> completed)
{
    if (!context || !completed)
        return;
    do {
        ++nextClipboardRequest;
    } while (nextClipboardRequest == 0 || clipboardRequests.contains(nextClipboardRequest));
    const quint32 request = nextClipboardRequest;
    ClipboardRequest pending{context, std::move(completed), {}};
    pending.destroyedConnection = QObject::connect(context, &QObject::destroyed, [request] {
        clipboardRequests.remove(request);
    });
    clipboardRequests.insert(request, std::move(pending));
    // Qt's WASM clipboard cache is not proof of a successful browser write.
    // zh_CN: Qt 的 WASM 剪贴板缓存不能证明浏览器写入已成功。
    fluentQtCopyTextToClipboard(request, text.toUtf8().constData());
}

#else
void copyText(QObject* context, const QString& text, std::function<void(bool)> completed)
{
    if (!context || !completed)
        return;
    const QPointer<QObject> guard(context);
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard)
        clipboard->setText(text);
    if (guard)
        completed(clipboard != nullptr);
}
#endif
} // namespace compatibility::detail
