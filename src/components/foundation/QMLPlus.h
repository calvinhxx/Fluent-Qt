#ifndef QMLPLUS_H
#define QMLPLUS_H

#include <QWidget>
#include <QLayout>
#include <QString>
#include <QVariant>
#include <QMap>
#include <QVector>
#include <QPointer>
#include <QMargins>
#include <QMetaObject>
#include <QMetaProperty>
#include <QEasingCurve>
#include <functional>

class QVariantAnimation;

namespace fluent {

/**
 * @brief Lightweight anchor-based layout for QWidget controls.
 * zh_CN: 面向 QWidget 控件的轻量锚点布局。
 *
 * AnchorLayout stores per-child anchors and resolves them during setGeometry().
 * It is intentionally small and is used by QMLPlus as a QWidget-friendly
 * alternative to QML anchors.
 * zh_CN: AnchorLayout 保存每个子控件的锚点，并在 setGeometry() 中解析布局；
 * 它由 QMLPlus 使用，用作 QWidget 场景下的 QML anchors 替代能力。
 */
class AnchorLayout : public QLayout {
    Q_OBJECT
public:
    /**
     * @brief Anchorable edge or center line.
     * zh_CN: 可锚定的边缘或中心线。
     */
    enum class Edge { None, Left, Right, Top, Bottom, HCenter, VCenter };

    /**
     * @brief One anchor relation from an edge to a target widget edge.
     * zh_CN: 描述一个边缘到目标控件边缘的锚定关系。
     */
    struct Anchor {
        QPointer<QWidget> target;
        Edge edge = Edge::None;
        int offset = 0;
        Anchor() = default;
        Anchor(QWidget* t, Edge e, int o = 0) : target(t), edge(e), offset(o) {}
    };

    /**
     * @brief Complete anchor set for one child widget.
     * zh_CN: 单个子控件的完整锚点集合。
     */
    struct Anchors {
        Anchor left, right, top, bottom, horizontalCenter, verticalCenter;
        bool fill = false;
        QMargins fillMargins;
        Anchors() : fillMargins(0, 0, 0, 0) {}
    };

    explicit AnchorLayout(QWidget* parent = nullptr);
    ~AnchorLayout();

    void addItem(QLayoutItem* item) override;
    int count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect& rect) override;

    void addAnchoredWidget(QWidget* w, const Anchors& anchors);

    /**
     * @brief Returns the current anchors for a registered widget.
     * zh_CN: 返回已注册控件的当前锚点。
     */
    Anchors anchorsForWidget(QWidget* widget) const;
    /**
     * @brief Replaces a registered widget's anchors and invalidates the layout.
     * zh_CN: 替换已注册控件的锚点，并使布局失效。
     */
    bool setWidgetAnchors(QWidget* widget, const Anchors& anchors);

private:
    struct Item {
        QLayoutItem* item = nullptr;
        Anchors anchors;
        QRect geometry;
    };
    QVector<Item> m_items;
    bool m_firstLayout = true;
    mutable QString m_lastCycleDiagnostic;

    int getWidgetIndex(QWidget* widget) const;
    Anchors currentAnchors(const Item& item) const;
    QVector<QRect> resolveGeometries(const QRect& parentRect, bool minimum, bool reportCycles) const;
    QSize measuredSize(bool minimum) const;
};

/**
 * @brief Runtime link that synchronizes one QObject property into another.
 * zh_CN: 将一个 QObject 属性同步到另一个 QObject 属性的运行时绑定对象。
 */
class PropertyLink : public QObject {
    Q_OBJECT
public:
    PropertyLink(QObject* from,
        const QMetaProperty& fromProp,
        QObject* to,
        const QMetaProperty& toProp,
        QObject* parent);
public slots:
    void syncToTarget();

private:
    QPointer<QObject> m_from;
    QMetaProperty m_fromProp;
    QPointer<QObject> m_to;
    QMetaProperty m_toProp;
};

/**
 * @brief Minimal property binder built on Qt meta-properties and notify signals.
 * zh_CN: 基于 Qt meta-property 和 notify signal 的轻量属性绑定工具。
 */
namespace detail {
class QMLBinding;
}

/**
 * @brief Explicit observable input to a computed binding or state condition.
 * zh_CN: 计算绑定或状态条件的显式可观察输入。
 *
 * The property must be readable and have a NOTIFY signal. All inputs and the
 * target must live on the calling thread. Callbacks must guard other captures.
 * zh_CN: 属性必须可读且具有 NOTIFY 信号。输入与目标须位于调用线程；
 * 回调需要自行保护未列入依赖的其他捕获对象。
 */
struct BindingDependency {
    QPointer<QObject> object;
    QByteArray propertyName;
};

/**
 * @brief Initial enablement and optional event-loop coalescing for a binding.
 * zh_CN: 绑定的初始启用状态和可选的事件循环合并更新策略。
 */
struct BindingOptions {
    bool enabled = true;
    bool deferred = false;
};

/**
 * @brief Copyable, non-owning control for a target-owned property binding.
 * zh_CN: 可复制且不持有所有权的属性绑定控制句柄；绑定由目标对象持有。
 *
 * Destruction of a handle does not unbind. A successful replacement invalidates
 * all old handles; an invalid replacement preserves the previous binding.
 * pause()/resume() retain enabled state; state overrides also suspend both
 * binding directions. refresh() evaluates immediately when active.
 * zh_CN: 销毁句柄不会解绑。成功替换会使旧句柄失效；无效替换保留原绑定。
 * pause()/resume() 保留 enabled 状态；状态覆盖也会暂停双向同步。
 * refresh() 在绑定活动时立即计算。
 */
class BindingHandle {
public:
    BindingHandle() = default;
    /**
     * @brief Reports whether the binding and its endpoints remain available.
     * zh_CN: 返回绑定及其端点是否仍然有效。
     */
    bool isValid() const;
    /**
     * @brief Returns enablement independently of manual or state suspension.
     * zh_CN: 返回启用状态，不受手动暂停或状态覆盖影响。
     */
    bool isEnabled() const;
    /**
     * @brief Reports manual pause or an active state override.
     * zh_CN: 返回绑定是否因手动暂停或活动状态覆盖而挂起。
     */
    bool isSuspended() const;
    /**
     * @brief Enables or disables synchronization; enabling refreshes immediately.
     * zh_CN: 启用或禁用同步；启用时立即刷新。
     */
    void setEnabled(bool enabled) const;
    /**
     * @brief Holds the current displayed value while preserving enablement.
     * zh_CN: 保持当前显示值，并保留启用状态。
     */
    void pause() const;
    /**
     * @brief Removes manual pause and refreshes an enabled, uncovered binding.
     * zh_CN: 解除手动暂停，并刷新已启用且未被状态覆盖的绑定。
     */
    void resume() const;
    /**
     * @brief Evaluates an active binding immediately, bypassing deferred scheduling.
     * zh_CN: 立即计算活动绑定，跳过延迟调度。
     */
    void refresh() const;
    /**
     * @brief Permanently removes the binding and holds its current displayed value.
     * zh_CN: 永久移除绑定，并保持当前显示值。
     */
    void unbind() const;

private:
    explicit BindingHandle(detail::QMLBinding* binding);
    QPointer<detail::QMLBinding> m_binding;
    friend class PropertyBinder;
};

/**
 * @brief Managed property synchronization through Qt meta-properties and NOTIFY.
 * zh_CN: 通过 Qt 元属性与 NOTIFY 信号提供可管理的属性同步。
 */
class PropertyBinder {
public:
    /**
     * @brief Binding direction.
     * zh_CN: 属性绑定方向。
     */
    enum Direction { OneWay, TwoWay };

    /**
     * @brief Synchronizes properties, observing the available NOTIFY signals.
     * zh_CN: 同步属性，并连接可用的 NOTIFY 信号。
     *
     * Both endpoints must belong to the calling Qt thread. A source without
     * NOTIFY still supplies the initial value; later changes in that direction
     * cannot be observed. TwoWay observes each direction only when it notifies.
     * A valid registration replaces the binding on the same target property;
     * an invalid registration leaves it intact. State overrides suspend this
     * binding and read the latest source value when they end.
     * zh_CN: 两端对象必须属于调用线程。源无 NOTIFY 时仍同步初值，但无法跟踪
     * 该方向的后续变化；TwoWay 仅监听带通知的方向。有效注册替换同一目标属性
     * 的已有绑定；无效注册保留原绑定。状态覆盖期间暂停绑定，退出时读取最新源值。
     */
    static void
    bind(QObject* source, const char* sPropName, QObject* target, const char* tPropName, Direction dir = OneWay);

    /**
     * @brief Installs a managed direct binding, synchronizing immediately.
     * zh_CN: 安装可管理的直接绑定，并立即同步初值。
     *
     * An enabled initial synchronization bypasses Behavior. TwoWay additionally
     * requires writable source and a target NOTIFY signal. State and animation
     * writes never propagate back into the source.
     * Both endpoints must be readable and belong to the calling Qt thread;
     * the target must also be writable and the source must expose NOTIFY.
     * zh_CN: 启用绑定时的初次同步跳过 Behavior。TwoWay 还要求源可写且目标具有
     * NOTIFY 信号。状态赋值和动画帧不会反向写入源。
     * 两端属性须可读且属于调用线程；目标还须可写，源须具有 NOTIFY 信号。
     */
    static BindingHandle bindManaged(QObject* source,
        const char* sourceProp,
        QObject* target,
        const char* targetProp,
        Direction direction = OneWay,
        BindingOptions options = {});
    /**
     * @brief Installs a C++ expression binding with explicitly observed inputs.
     * zh_CN: 安装使用显式可观察输入的 C++ 表达式绑定。
     *
     * Empty dependencies permit a constant expression. A destroyed dependency
     * permanently invalidates the binding before its callback can run again.
     * The target property must be readable and writable.
     * zh_CN: 空依赖允许常量表达式。依赖销毁会使绑定永久失效，且不再调用回调。
     * 目标属性须可读且可写。
     */
    static BindingHandle bindComputed(QObject* target,
        const char* targetProp,
        const QVector<BindingDependency>& dependencies,
        std::function<QVariant()> evaluate,
        BindingOptions options = {});
};

/**
 * @brief Property assignment applied by a QMLPlus state.
 * zh_CN: QMLPlus state 应用的一条属性赋值。
 */
struct PropertyChange {
    QPointer<QObject> target;
    QByteArray propertyName;
    QVariant value;
};

/**
 * @brief Complete replacement anchor set applied and restored by a state.
 * zh_CN: 由状态应用并恢复的完整锚点替换配置。
 */
struct AnchorChange {
    QPointer<QWidget> target;
    AnchorLayout::Anchors anchors;
};

/**
 * @brief Named set of property changes.
 * zh_CN: 一组具名属性变更。
 */
struct QMLState {
    QString name;
    QVector<PropertyChange> changes;
    std::function<bool()> when;
    QVector<BindingDependency> dependencies;
    QString extend;
    QVector<AnchorChange> anchorChanges;
};

/**
 * @brief MotionPolicy-aware animation for managed property writes.
 * zh_CN: 遵循 MotionPolicy 的受管理属性写入动画。
 */
struct PropertyAnimation {
    int duration = 180;
    QEasingCurve easing = QEasingCurve::OutCubic;
    bool enabled = true;
};

/**
 * @brief Animation selected for matching state changes; empty properties match all.
 * zh_CN: 匹配状态变化时使用的动画；空属性列表表示匹配全部属性。
 *
 * An empty from/to matches the default state; "*" matches any state. Exact
 * endpoint matches outrank wildcards; registration order breaks equal scores.
 * Anchors are resolved immediately by their owning layout and are not animated.
 * zh_CN: 空 from/to 匹配默认状态；"*" 匹配任意状态。精确端点优先于通配符；
 * 相同优先级按注册顺序选择。锚点由所属布局立即求解，不应用动画。
 */
struct StateTransition {
    QString from = QStringLiteral("*");
    QString to = QStringLiteral("*");
    QVector<QByteArray> properties;
    PropertyAnimation animation;
};

/**
 * @brief Lazy QObject-host-owned observation surface for QMLPlus state transitions.
 * zh_CN: 由 QObject 宿主持有、按需创建的 QMLPlus 状态过渡观察对象。
 *
 * Signals describe managed state changes. transitionFinished is emitted once
 * after the destination settles, including immediate transitions. Interrupted
 * transitions do not emit finished; Behavior alone is not a state transition.
 * zh_CN: 信号描述受管理状态变化。目标状态稳定后发送一次 transitionFinished，
 * 包括立即完成的过渡。被打断的过渡不发送完成信号；单独 Behavior 不属于状态过渡。
 */
class QMLPlusRuntime : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool transitionRunning READ isTransitionRunning NOTIFY transitionRunningChanged)
public:
    ~QMLPlusRuntime() override;
    /**
     * @brief Returns the logical destination state, before animation completion.
     * zh_CN: 返回逻辑目标状态，无须等待动画完成。
     */
    QString state() const;
    /**
     * @brief Reports whether managed state transition animations are running.
     * zh_CN: 返回受管理的状态过渡动画是否正在运行。
     */
    bool isTransitionRunning() const;

signals:
    void stateChanged(const QString& state);
    void transitionRunningChanged(bool running);
    void transitionFinished(const QString& state);

private:
    struct Data;
    explicit QMLPlusRuntime(QObject* host);
    Data* d;
    void requestState(const QString& name, bool force = false);
    void evaluateConditions();
    void watchAnimation(QVariantAnimation* animation);
    void animationEnded(QVariantAnimation* animation, bool completed);
    friend class QMLPlus;
    friend class detail::QMLBinding;
    friend class QMLPlusPropertyHub;
};

/**
 * @brief Mixin that adds anchors, property binding, and simple states to QWidget-based components.
 * zh_CN: 为 QWidget 组件提供 anchors、属性绑定和简单 state 能力的 mixin。
 *
 * States and runtime observation require a QObject host. Anchor and implicit
 * binding conveniences require QWidget; bind() targets that QWidget host.
 * zh_CN: 状态与运行时观察要求 QObject 宿主；锚点和隐式绑定便捷接口要求
 * QWidget；bind() 自动以该 QWidget 宿主作为目标。
 */
class QMLPlus {
public:
    QMLPlus();
    virtual ~QMLPlus();

    /**
     * @brief Returns mutable anchors for the QWidget host.
     * zh_CN: 返回 QWidget 宿主的可变锚点配置。
     */
    AnchorLayout::Anchors* anchors();

    /**
     * @brief Applies a named state, or clears state when name is empty.
     * zh_CN: 应用具名 state；名称为空时恢复默认状态。
     *
     * Unknown non-empty names are ignored and leave the current state intact.
     * Properties already overridden by another state controller reject the
     * entire transition. States preserve bindings and refresh them on exit.
     * zh_CN: 未注册的非空名称会被忽略，并保持当前 state 不变。
     * 若属性已被其他状态控制器覆盖，则拒绝整个过渡。状态保留原绑定，退出时刷新。
     */
    void setState(const QString& name);
    QString state() const;

    /**
     * @brief Registers or replaces a named state definition.
     * zh_CN: 注册或替换一个具名 state 定义。
     *
     * Replacing the current state applies its new definition immediately.
     * An unchanged state name does not emit stateChanged again.
     * zh_CN: 替换当前状态时立即应用新定义；状态名称未改变时不重复发送 stateChanged。
     */
    void addState(const QMLState& state);

    /**
     * @brief Reevaluates conditions; the first registered true condition wins.
     * zh_CN: 重新计算条件；按注册顺序选择首个成立的条件。
     *
     * Dependency notifications reevaluate automatically. No match selects the
     * default state. setState() remains manual until the next reevaluation.
     * Replacement preserves state order. extend applies ancestors first, with
     * derived changes winning; missing or cyclic ancestors leave state intact.
     * zh_CN: 依赖通知会自动重新计算；无匹配时进入默认状态。setState() 的手动
     * 选择保持到下次计算。替换保留注册顺序；extend 先应用祖先，再由派生变更
     * 覆盖；祖先缺失或循环时保持当前状态不变。
     */
    void evaluateStates();
    /**
     * @brief Returns the lazy host-owned state observation object.
     * zh_CN: 返回按需创建、由宿主持有的状态观察对象。
     */
    QMLPlusRuntime* runtime();

    /**
     * @brief Replaces all host anchors and invalidates the parent layout.
     * zh_CN: 替换宿主的全部锚点，并使父布局失效。
     */
    void setAnchors(const AnchorLayout::Anchors& anchors);
    /**
     * @brief Clears all host anchors and invalidates the parent layout.
     * zh_CN: 清除宿主全部锚点，并使父布局失效。
     */
    void resetAnchors();

    /**
     * @brief Binds a source property to a property on the QWidget host.
     * zh_CN: 将 source 属性绑定到 QWidget 宿主上的 target 属性。
     *
     * Uses PropertyBinder::bind(), including initial synchronization without
     * NOTIFY, same-thread validation, and replacement of an existing binding.
     * zh_CN: 使用 PropertyBinder::bind()，保留无 NOTIFY 的初值同步，要求同线程，
     * 并替换已有绑定。
     */
    void bind(const char* targetProp,
        QObject* source,
        const char* sourceProp,
        PropertyBinder::Direction dir = PropertyBinder::OneWay);

    /**
     * @brief Installs a managed direct binding on this QWidget host.
     * zh_CN: 在当前 QWidget 宿主上安装可管理的直接绑定。
     */
    BindingHandle bindManaged(const char* targetProp,
        QObject* source,
        const char* sourceProp,
        PropertyBinder::Direction direction = PropertyBinder::OneWay,
        BindingOptions options = {});
    /**
     * @brief Installs an explicit-dependency C++ expression on this QWidget host.
     * zh_CN: 在当前 QWidget 宿主上安装使用显式依赖的 C++ 表达式绑定。
     */
    BindingHandle bindComputed(const char* targetProp,
        const QVector<BindingDependency>& dependencies,
        std::function<QVariant()> evaluate,
        BindingOptions options = {});
    /**
     * @brief Registers a state transition in precedence order.
     * zh_CN: 按优先顺序注册状态过渡。
     */
    void addTransition(const StateTransition& transition);
    /**
     * @brief Sets the default animation for this host's managed property writes.
     * zh_CN: 设置宿主受管理属性写入时使用的默认动画。
     *
     * Explicit state transitions take precedence. Arbitrary QWidget setters
     * are not intercepted. Retargeting starts from the current visible value.
     * zh_CN: 显式状态过渡优先；不会拦截任意 QWidget setter；重新定向从当前
     * 可见值开始。
     */
    void setBehavior(const char* propertyName, const PropertyAnimation& animation);
    /**
     * @brief Removes the default animation for future managed writes.
     * zh_CN: 移除后续受管理写入使用的默认动画。
     */
    void clearBehavior(const char* propertyName);
    /**
     * @brief Writes a host property through Behavior without removing its binding.
     * zh_CN: 通过 Behavior 写入宿主属性，且不移除该属性的绑定。
     */
    bool writeProperty(const char* propertyName, const QVariant& value);

protected:
    void applyState(const QString& name);

private:
    AnchorLayout::Anchors* m_anchors = nullptr;
    QPointer<QMLPlusRuntime> m_runtime;
};

} // namespace fluent

#endif // QMLPLUS_H
