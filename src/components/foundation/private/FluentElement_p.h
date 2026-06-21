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
        QVector<FluentElement*> hiddenElements;
        deferredElements.reserve(copy.size());
        hiddenElements.reserve(copy.size());
        for (auto* element : copy) {
            if (!elements.contains(element))
                continue;
            auto* widget = dynamic_cast<QWidget*>(element);
            if (widget && !widget->isVisible())
                hiddenElements.append(element);
            else
                deferredElements.append(element);
        }
        deferredElements += hiddenElements;

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

        // A small batch keeps theme propagation below a frame budget even when the
        // gallery has prewarmed many component pages. Visible elements are queued first.
        // zh_CN: 小批次刷新避免预热页面较多时阻塞一整帧；可见元素优先入队。
        constexpr int batchSize = 24;
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
