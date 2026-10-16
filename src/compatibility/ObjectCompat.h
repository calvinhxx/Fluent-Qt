#pragma once

// Signal connections and metatype registration.

#include <QMetaType>
#include <QObject>
#include <memory>
#include <functional>
#include <mutex>
#include <type_traits>
#include <utility>

namespace compatibility::detail {

struct SingleShotState {
    std::mutex mutex;
    QMetaObject::Connection connection;
    bool invoked = false;
};

template <typename Context, typename Functor> struct SingleShotSlot {
    std::shared_ptr<SingleShotState> state;
    Context* context;
    Functor slot;

    // SFINAE lets QObject keep its usual argument-prefix matching, including
    // callbacks that ignore a signal's parameters. Delivery happens on context's thread.
    // zh_CN: 保留 QObject 的参数前缀匹配，允许回调忽略信号参数；在 context 线程中处理投递。
    template <typename... Args,
        std::enable_if_t<std::is_member_function_pointer_v<Functor>
                ? std::is_invocable_v<Functor&, Context*, Args...>
                : std::is_invocable_v<Functor&, Args...>,
            int> = 0>
    void operator()(Args&&... args)
    {
        QMetaObject::Connection connection;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            // Disconnecting does not remove already queued deliveries on Qt 5.
            if (std::exchange(state->invoked, true))
                return;
            connection = state->connection;
        }
        QObject::disconnect(connection);
        if constexpr (std::is_member_function_pointer_v<Functor>)
            std::invoke(slot, context, std::forward<Args>(args)...);
        else
            std::invoke(slot, std::forward<Args>(args)...);
    }
};

template <typename Sender, typename Signal, typename Context, typename Functor>
QMetaObject::Connection connectSingleShotFallback(Sender* sender, Signal signal, Context* context, Functor&& functor)
{
    auto state = std::make_shared<SingleShotState>();
    const auto connection = QObject::connect(sender, signal, context,
        SingleShotSlot<Context, std::decay_t<Functor>>{state, context, std::forward<Functor>(functor)});
    bool invoked;
    {
        // The context thread may deliver a signal before connect() returns here.
        std::lock_guard<std::mutex> lock(state->mutex);
        state->connection = connection;
        invoked = state->invoked;
    }
    if (invoked)
        QObject::disconnect(connection);
    return connection;
}

} // namespace compatibility::detail

template <typename Sender, typename Signal, typename Context, typename Functor>
QMetaObject::Connection fluentConnectSingleShot(Sender* sender, Signal signal, Context* context, Functor&& functor)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QObject::connect(sender, signal, context, std::forward<Functor>(functor), Qt::SingleShotConnection);
#else
    return compatibility::detail::connectSingleShotFallback(sender, signal, context, std::forward<Functor>(functor));
#endif
}

template <typename T> void fluentRegisterMetaTypeNames(const char* name)
{
    qRegisterMetaType<T>(name);
}

template <typename T, typename... Names>
void fluentRegisterMetaTypeNames(const char* firstName, const char* secondName, Names... remainingNames)
{
    qRegisterMetaType<T>(firstName);
    fluentRegisterMetaTypeNames<T>(secondName, remainingNames...);
}

/**
 * @brief Returns whether a metatype name is registered on the active Qt version.
 * zh_CN: 返回指定元类型名称是否已在当前 Qt 版本中注册。
 */
inline bool fluentMetaTypeNameIsRegistered(const char* name)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QMetaType::fromName(name).isValid();
#else
    return QMetaType::type(name) != QMetaType::UnknownType;
#endif
}
