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
 * Stores per-child edge anchors and resolves widget geometries during layout passes.
 * zh_CN: 面向 QWidget 控件的轻量锚点布局，按子控件边缘锚定关系自动计算几何位置。
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
 * @brief Lightweight property binder built on Qt meta-properties and NOTIFY signals.
 * zh_CN: 基于 Qt 元属性和 NOTIFY 信号的轻量属性绑定工具。
 */
namespace detail {
class QMLBinding;
}

/**
 * @brief Observable property dependency for a computed binding or state condition.
 * The property must be readable with a NOTIFY signal on the calling thread.
 * zh_CN: 计算绑定或状态条件所监听的依赖属性；该属性须可读、带 NOTIFY 信号且位于调用线程。
 */
struct BindingDependency {
    QPointer<QObject> object;
    QByteArray propertyName;
};

/**
 * @brief Initial enablement and optional event-loop coalescing for a binding.
 * zh_CN: 绑定的初始启用状态以及是否合并同轮事件循环内的重复更新。
 */
struct BindingOptions {
    bool enabled = true;
    bool deferred = false;
};

/**
 * @brief Copyable handle to a property binding managed by the target object.
 * Copying or destroying the handle does not unbind; call unbind() to remove the binding permanently.
 * zh_CN: 用于操作属性绑定的可复制控制对象（绑定由目标对象管理）。复制或销毁该控制对象不会解除绑定，需显式调用 unbind() 才会移除。
 */
class BindingHandle {
public:
    BindingHandle() = default;
    /**
     * @brief Reports whether the binding and its endpoints remain valid.
     * zh_CN: 返回绑定及其两端对象是否仍然有效。
     */
    bool isValid() const;
    /**
     * @brief Returns enablement independently of pause or state suspension.
     * zh_CN: 返回启用状态，不受暂停或状态覆盖影响。
     */
    bool isEnabled() const;
    /**
     * @brief Reports manual pause or an active state override.
     * zh_CN: 返回绑定是否因手动暂停或状态覆盖而处于挂起状态。
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
     * zh_CN: 立即求值并同步一次活动绑定，跳过延迟调度。
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
 * A valid replacement invalidates the old binding handle; an invalid request leaves the existing binding intact.
 * zh_CN: 通过 Qt 元属性与 NOTIFY 信号同步属性值。新绑定通过校验后会替换原绑定，旧句柄随之失效；校验失败则保留原绑定。
 */
class PropertyBinder {
public:
    /**
     * @brief Binding direction.
     * zh_CN: 属性绑定方向。
     */
    enum Direction { OneWay, TwoWay };

    /**
     * @brief Binds @p sPropName on @p source to @p tPropName on @p target using available NOTIFY signals.
     * Both objects must live on the calling thread; a valid call replaces any existing binding on the target property.
     * zh_CN: 将 @p source 的属性绑定到 @p target 的属性并监听可用 NOTIFY 信号。两端对象须位于调用线程；注册成功时会替换目标属性上的已有绑定。
     */
    static void
    bind(QObject* source, const char* sPropName, QObject* target, const char* tPropName, Direction dir = OneWay);

    /**
     * @brief Installs a managed direct binding and synchronizes the initial value immediately.
     * Both objects must live on the calling thread; @p sourceProp requires NOTIFY and @p targetProp must be writable (TwoWay requires both directions to be readable, writable, and notifying).
     * zh_CN: 安装可管理的直接绑定并立即同步初值。两端对象须在调用线程，源属性须带 NOTIFY 且目标属性可写；双向绑定还要求源可写且目标带 NOTIFY。
     */
    static BindingHandle bindManaged(QObject* source,
        const char* sourceProp,
        QObject* target,
        const char* targetProp,
        Direction direction = OneWay,
        BindingOptions options = {});
    /**
     * @brief Installs a C++ expression binding driven by explicit @p dependencies.
     * The target property must be readable and writable; destroying any dependency permanently invalidates the binding.
     * zh_CN: 安装由显式 @p dependencies 驱动的 C++ 表达式绑定。目标属性须可读可写；任意依赖销毁后绑定即永久失效。
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
 * @brief Animation settings for managed property writes, respecting MotionPolicy.
 * zh_CN: 受管理属性写入的动画配置，自动遵循 MotionPolicy 动效策略。
 */
struct PropertyAnimation {
    int duration = 180;
    QEasingCurve easing = QEasingCurve::OutCubic;
    bool enabled = true;
};

/**
 * @brief Animation selected for matching state changes; an empty properties list matches all properties.
 * Empty from/to matches the default state, and "*" matches any state; anchor changes are not animated.
 * zh_CN: 为匹配的状态切换指定过渡动画（properties 为空时匹配全部属性）。空的 from/to 表示默认状态，"*" 匹配任意状态；锚点变更不播放动画。
 */
struct StateTransition {
    QString from = QStringLiteral("*");
    QString to = QStringLiteral("*");
    QVector<QByteArray> properties;
    PropertyAnimation animation;
};

/**
 * @brief Lazily created observer for QMLPlus state transitions, owned by the QObject host.
 * Emits transitionFinished() once when a transition settles, including immediate switches, unless interrupted.
 * zh_CN: 由 QObject 宿主按需创建并管理的状态过渡观察对象。过渡稳定后（含立即完成的情况）发送一次 transitionFinished()，被打断时不发送。
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
 * @brief Mixin adding anchors, property bindings, and simple states to QWidget-based controls.
 * States and runtime() require a QObject host; anchor and host-targeted binding helpers require QWidget.
 * zh_CN: 为 QWidget 控件提供锚点布局、属性绑定与简单状态切换能力的 mixin。状态与 runtime() 需要 QObject 宿主，锚点及面向宿主的绑定接口需要 QWidget。
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
     * @brief Applies a registered state by name, or restores the default state when @p name is empty.
     * Unknown names are ignored, and existing bindings resume when leaving the state.
     * zh_CN: 按名称切换状态；@p name 为空时恢复默认状态。未知名称会被忽略，退出状态后原属性绑定自动恢复同步。
     */
    void setState(const QString& name);
    QString state() const;

    /**
     * @brief Registers or replaces a named state definition; replacing the active state applies it immediately.
     * zh_CN: 注册或替换具名状态定义；若替换的正是当前状态则立即重新应用。
     */
    void addState(const QMLState& state);

    /**
     * @brief Reevaluates state conditions in registration order and activates the first match, or the default state if none match.
     * Also runs automatically when dependency properties notify; derived states using extend inherit ancestor changes unless cyclic or missing.
     * zh_CN: 按注册顺序重新计算各状态的 when 条件并应用首个成立的状态，无匹配时恢复默认状态（依赖属性变化时也会自动触发）。使用 extend 继承的状态会先应用祖先定义，循环或缺失祖先会被忽略。
     */
    void evaluateStates();
    /**
     * @brief Returns the lazily created state observer owned by this host.
     * zh_CN: 返回由当前宿主按需创建并管理的状态观察对象。
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
     * @brief Binds @p sourceProp on @p source to @p targetProp on this QWidget host via PropertyBinder::bind().
     * zh_CN: 通过 PropertyBinder::bind() 将 @p source 的属性绑定到当前 QWidget 宿主的指定属性。
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
     * @brief Sets the default animation used when @p propertyName is written via writeProperty() or a binding.
     * Explicit StateTransition rules take precedence; direct C++ setter calls are not animated.
     * zh_CN: 设置通过 writeProperty() 或绑定更新 @p propertyName 时使用的默认动画。显式 StateTransition 优先级更高，直接调用 C++ setter 不会触发动画。
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
