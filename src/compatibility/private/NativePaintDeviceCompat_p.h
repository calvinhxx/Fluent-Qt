#pragma once

#include <QPointer>
#include <QVector>
#include <functional>

class QPaintDevice;

namespace compatibility::detail {

// A bounded intermediate can still represent native backing-store damage.
// Keep captures distinct: only the native repaint adapter enters this scope.
// zh_CN: 有界中间画布仍可携带原生后备缓冲的脏区；仅原生刷新适配层进入此作用域，截图不进入。
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

    // Only a successful copy into the backing store publishes paint evidence.
    // Abandoned tiles and ordinary offscreen captures publish nothing.
    // zh_CN: 仅成功写回后备缓冲才公布绘制证据；放弃的图块及普通离屏抓图不公布。
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
