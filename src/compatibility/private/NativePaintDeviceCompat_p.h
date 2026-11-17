#pragma once

#include <QPointer>
#include <QVector>
#include <functional>

class QPaintDevice;

namespace compatibility::detail {

// Only the native repaint adapter enters this scope so offscreen captures stay distinct from backing-store paints.
// zh_CN: 仅原生刷新适配层进入此作用域，以区分屏幕绘制缓冲的脏区和普通截图。
inline thread_local const QPaintDevice* nativeRasterDevice = nullptr;
class ScopedNativeRasterDevice;
inline thread_local ScopedNativeRasterDevice* nativeRasterScope = nullptr;

class ScopedNativeRasterDevice {
public:
    explicit ScopedNativeRasterDevice(const QPaintDevice* device)
        : previous(nativeRasterDevice), previousScope(nativeRasterScope)
    {
        nativeRasterDevice = device;
        nativeRasterScope = this;
    }
    ~ScopedNativeRasterDevice()
    {
        nativeRasterDevice = previous;
        nativeRasterScope = previousScope;
    }
    ScopedNativeRasterDevice(const ScopedNativeRasterDevice&) = delete;
    ScopedNativeRasterDevice& operator=(const ScopedNativeRasterDevice&) = delete;

    void setDevice(const QPaintDevice* device) { nativeRasterDevice = device; }

    void afterCommit(QObject* context, std::function<void()> callback)
    {
        callbacks.append({context, std::move(callback)});
    }

    // Only a successful write back to the backing store counts as a committed surface paint (skips abandoned tiles and grab()).
    // zh_CN: 仅成功写回绘制缓冲后才触发提交回调（放弃的图块与普通截图不计入）。
    void commit()
    {
        const auto pending = std::move(callbacks);
        callbacks.clear();
        for (const auto& item : pending)
            if (item.context)
                item.callback();
    }

private:
    struct Callback {
        QPointer<QObject> context;
        std::function<void()> callback;
    };
    const QPaintDevice* previous;
    ScopedNativeRasterDevice* previousScope;
    QVector<Callback> callbacks;
};

inline bool afterNativeRasterCommit(const QPaintDevice* device, QObject* context, std::function<void()> callback)
{
    if (!device || device != nativeRasterDevice || !nativeRasterScope)
        return false;
    nativeRasterScope->afterCommit(context, std::move(callback));
    return true;
}

} // namespace compatibility::detail
