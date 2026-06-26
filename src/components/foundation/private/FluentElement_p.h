#ifndef FLUENTELEMENT_P_H
#define FLUENTELEMENT_P_H

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include "components/foundation/FluentElement.h"

namespace fluent {

/**
 * @brief Registry that broadcasts global Fluent theme changes to live elements.
 * zh_CN: 向存活 FluentElement 广播全局主题变化的注册表。
 *
 * FluentThemeManager is an internal singleton used by FluentElement so public
 * components can refresh token-derived caches without managing registration manually.
 * zh_CN: FluentThemeManager 是 FluentElement 使用的内部单例，让公共组件无需手动注册，
 * 也能刷新由 token 派生的缓存。
 */
class FluentThemeManager : public QObject {
    Q_OBJECT
public:
    static FluentThemeManager* instance() {
        static FluentThemeManager inst;
        return &inst;
    }
    
    FluentElement::Theme currentTheme = FluentElement::Light;
    QSet<FluentElement*> elements;

    // Bumped on every theme change. A widget subtree that was hidden (and thus deferred) during a change
    // can compare its last-themed generation against this to decide whether it must refresh on show.
    // zh_CN: 每次主题变化时自增。切换期间处于隐藏（因而被延后）的子树，可用自己上次刷新的代次与此比较，决定显示时是否需刷新。
    int generation() const { return notificationGeneration; }

    void notifyAll() {
        ++notificationGeneration;
        deferredElements.clear();
        deferredIndex = 0;
        // Iterate over a copy: callbacks may destroy elements and invalidate iterators.
        // zh_CN: 使用副本遍历，防止回调中对象销毁导致迭代器失效。
        auto copy = elements;
        for (auto* e : copy) {
            if (elements.contains(e)) {
                e->onThemeUpdated();
            }
        }
    }

    void notifyVisibleThenDeferred() {
        const int generation = ++notificationGeneration;
        deferredElements.clear();
        deferredIndex = 0;

        const auto copy = elements;
        QVector<FluentElement*> visibleElements;
        deferredElements.reserve(copy.size());
        visibleElements.reserve(copy.size());
        for (auto* element : copy) {
            if (!elements.contains(element))
                continue;
            auto* widget = dynamic_cast<QWidget*>(element);
            if (widget && !widget->isVisible())
                deferredElements.append(element);   // off-screen → themed lazily (backstop)
            else
                visibleElements.append(element);    // on-screen → themed now
        }

        // Theme the on-screen elements synchronously so the visible switch is ATOMIC: every update()
        // coalesces into a single repaint instead of staggering across timer ticks. The cost is bounded
        // by what's visible (tens of widgets), not the whole prewarmed tree (thousands). Off-screen
        // elements are deferred and also re-themed on show (see StackContentHost), so navigating to a
        // prewarmed page never reveals a stale theme. zh_CN: 同步刷新可见元素，使屏幕切换是原子的：所有 update()
        // 合并为一次重绘，而非分散到多个定时器 tick。开销受可见元素数（数十个）限制，而非整棵预热树（数千个）。
        // 隐藏元素延后刷新，并在显示时再刷新（见 StackContentHost），导航到预热页绝不会露出过期主题。
        for (auto* element : visibleElements) {
            if (elements.contains(element))
                element->onThemeUpdated();
        }

        if (!deferredElements.isEmpty()) {
            QTimer::singleShot(0, this, [this, generation]() {
                notifyDeferredBatch(generation);
            });
        }
    }

private:
    void notifyDeferredBatch(int generation) {
        if (generation != notificationGeneration)
            return;

        // Only off-screen elements reach here (visible ones were themed synchronously). This is an
        // eventual-consistency net for hidden widgets not covered by an on-show refresh (e.g. collapsed
        // nav rows); a stacked page that is navigated to is re-themed on show. Keep the batch small so a
        // single tick stays well under a frame even though each onThemeUpdated isn't free.
        // zh_CN: 只有屏外元素会走到这里(可见的已同步刷新)。这是对未被"显示时刷新"覆盖的隐藏控件(如折叠的导航行)
        // 的最终一致性兜底;被导航到的分页会在显示时重刷。批次保持小,使单个 tick 远低于一帧。
        constexpr int batchSize = 8;
        const int end = qMin(deferredIndex + batchSize, deferredElements.size());
        for (; deferredIndex < end; ++deferredIndex) {
            auto* element = deferredElements.at(deferredIndex);
            if (elements.contains(element))
                element->onThemeUpdated();
        }

        if (deferredIndex < deferredElements.size()) {
            QTimer::singleShot(1, this, [this, generation]() {
                notifyDeferredBatch(generation);
            });
        } else {
            deferredElements.clear();
            deferredIndex = 0;
        }
    }

    QVector<FluentElement*> deferredElements;
    int deferredIndex = 0;
    int notificationGeneration = 0;
};

/**
 * @brief Private storage for FluentElement theme registration state.
 * zh_CN: 保存 FluentElement 主题注册状态的私有数据。
 *
 * FluentElementPrivate keeps implementation details out of the public mixin
 * header while preserving the lightweight PImpl-style ownership contract.
 * zh_CN: FluentElementPrivate 将实现细节从公共 mixin 头文件中移出，同时保持轻量 PImpl 风格所有权契约。
 */
class FluentElementPrivate {
public:
    explicit FluentElementPrivate(FluentElement* q) : q_ptr(q) {}
    FluentElement* q_ptr;
};

} // namespace fluent

#endif // FLUENTELEMENT_P_H
