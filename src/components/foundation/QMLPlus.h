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
        QWidget* target = nullptr;
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

private:
    struct Item {
        QLayoutItem* item = nullptr;
        Anchors anchors;
        QRect geometry;
    };
    QVector<Item> m_items;
    bool m_firstLayout = true; ///< 首次 setGeometry 标志，用于延迟 theme 初始化
    int getWidgetIndex(QWidget* w) const;
    int getEdgeValue(QWidget* target, Edge edge, const QRect& parentRect) const;
};

/**
 * @brief Runtime link that synchronizes one QObject property into another.
 * zh_CN: 将一个 QObject 属性同步到另一个 QObject 属性的运行时绑定对象。
 */
class PropertyLink : public QObject {
    Q_OBJECT
public:
    PropertyLink(QObject* from, const QMetaProperty& fromProp, QObject* to, const QMetaProperty& toProp, QObject* parent);
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
class PropertyBinder {
public:
    /**
     * @brief Binding direction.
     * zh_CN: 属性绑定方向。
     */
    enum Direction { OneWay, TwoWay };
    static void bind(QObject* source, const char* sPropName, QObject* target, const char* tPropName, Direction dir = OneWay);
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
 * @brief Named set of property changes.
 * zh_CN: 一组具名属性变更。
 */
struct QMLState {
    QString name;
    QVector<PropertyChange> changes;
};

/**
 * @brief Mixin that adds anchors, property binding, and simple states to QWidget-based components.
 * zh_CN: 为 QWidget 组件提供 anchors、属性绑定和简单 state 能力的 mixin。
 *
 * QMLPlus assumes the final component type also inherits QWidget. bind() uses
 * that QWidget host as the implicit target for targetProp.
 * zh_CN: QMLPlus 假设最终组件类型也继承 QWidget；bind() 会自动把该 QWidget
 * 宿主作为 targetProp 的隐式目标。
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
     * zh_CN: 未注册的非空名称会被忽略，并保持当前 state 不变。
     */
    void setState(const QString& name);
    QString state() const { return m_currentState; }

    /**
     * @brief Registers or replaces a named state definition.
     * zh_CN: 注册或替换一个具名 state 定义。
     */
    void addState(const QMLState& state);

    /**
     * @brief Binds a source property to a property on the QWidget host.
     * zh_CN: 将 source 属性绑定到 QWidget 宿主上的 target 属性。
     */
    void bind(const char* targetProp, QObject* source, const char* sourceProp, PropertyBinder::Direction dir = PropertyBinder::OneWay);

protected:
    void applyState(const QString& name);

private:
    void rememberDefaultValue(QObject* target, const QByteArray& propertyName);
    void restoreDefaultValues();

    AnchorLayout::Anchors* m_anchors = nullptr;
    QString m_currentState;
    QMap<QString, QMLState> m_states;
    QMap<QObject*, QMap<QByteArray, QVariant>> m_defaultValues;
    QMap<QObject*, QMetaObject::Connection> m_defaultValueCleanupConnections;
};

} // namespace fluent

#endif // QMLPLUS_H
