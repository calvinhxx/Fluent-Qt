#include "components/foundation/QMLPlus.h"
#include "components/foundation/FluentElement.h"
#include "compatibility/QtCompat.h"
#include <QWidgetItem>
#include <QDebug>

namespace fluent {

// --- AnchorLayout implementation. zh_CN: AnchorLayout 实现。---

namespace {

QSize effectiveItemSize(const QLayoutItem* item) {
    if (!item)
        return QSize();

    QSize size = item->sizeHint();
    if (const QWidget* widget = fluentLayoutItemWidget(item)) {
        size = size.expandedTo(widget->minimumSize());
        size = size.boundedTo(widget->maximumSize());
    }
    return size;
}

} // namespace

AnchorLayout::AnchorLayout(QWidget* parent) : QLayout(parent) {
    setContentsMargins(0, 0, 0, 0);
}

AnchorLayout::~AnchorLayout() {
    while (!m_items.isEmpty()) delete takeAt(0);
}

void AnchorLayout::addItem(QLayoutItem* item) {
    Item it; it.item = item; m_items.append(it); invalidate();
}

int AnchorLayout::count() const { return m_items.size(); }

QLayoutItem* AnchorLayout::itemAt(int index) const {
    return (index >= 0 && index < m_items.size()) ? m_items.at(index).item : nullptr;
}

QLayoutItem* AnchorLayout::takeAt(int index) {
    if (index < 0 || index >= m_items.size()) return nullptr;
    return m_items.takeAt(index).item;
}

QSize AnchorLayout::sizeHint() const { return QSize(400, 300); }
QSize AnchorLayout::minimumSize() const { return QSize(0, 0); }

void AnchorLayout::addAnchoredWidget(QWidget* w, const Anchors& anchors) {
    if (!w) return;
    if (parentWidget() && w->parent() != parentWidget()) w->setParent(parentWidget());
    if (auto* qp = dynamic_cast<QMLPlus*>(w)) *(qp->anchors()) = anchors;
    for (Item& it : m_items) {
        if (fluentLayoutItemWidget(it.item) == w) { it.anchors = anchors; invalidate(); return; }
    }
    addItem(new QWidgetItem(w));
    m_items.last().anchors = anchors;
    invalidate();
}

int AnchorLayout::getWidgetIndex(QWidget* w) const {
    for (int i = 0; i < m_items.size(); ++i) if (fluentLayoutItemWidget(m_items[i].item) == w) return i;
    return -1;
}

int AnchorLayout::getEdgeValue(QWidget* target, Edge edge, const QRect& parentRect) const {
    if (!target || target == parentWidget()) {
        switch (edge) {
            case Edge::Left: return parentRect.left(); case Edge::Right: return parentRect.right();
            case Edge::Top: return parentRect.top(); case Edge::Bottom: return parentRect.bottom();
            case Edge::HCenter: return parentRect.center().x(); case Edge::VCenter: return parentRect.center().y();
            default: return 0;
        }
    }
    int idx = getWidgetIndex(target);
    if (idx == -1) return 0;
    const QRect& r = m_items[idx].geometry;
    switch (edge) {
        case Edge::Left: return r.left(); case Edge::Right: return r.right();
        case Edge::Top: return r.top(); case Edge::Bottom: return r.bottom();
        case Edge::HCenter: return r.center().x(); case Edge::VCenter: return r.center().y();
        default: return 0;
    }
}

void AnchorLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    if (m_items.isEmpty()) return;

    // First layout pass: make sure each child's font metrics resolve before
    // sizeHint() is queried.
    //
    // FluentElement applies pixel-size fonts in onThemeUpdated() during
    // construction, but the widget is not yet in a window hierarchy and some
    // platform application-font backends may not have final metrics yet;
    // QStyle::polish() can also overwrite custom fonts. So on the first
    // setGeometry(): ensurePolished() first (style lands), then
    // onThemeUpdated() (Fluent fonts reapplied) so sizeHint() is correct.
    // zh_CN: 首次布局——在查询 sizeHint() 前确保子控件字体 metrics 已解析。
    // FluentElement 构造时即调用 onThemeUpdated() 设置 pixelSize 字体，但控件
    // 尚未入窗，macOS 上应用字体 metrics 可能尚未初始化，
    // QStyle::polish() 也可能覆盖自定义字体；因此首次 setGeometry() 先
    // ensurePolished() 再 onThemeUpdated()，保证 sizeHint() 正确。
    if (m_firstLayout) {
        m_firstLayout = false;
        for (const Item& it : m_items) {
            if (QWidget* w = fluentLayoutItemWidget(it.item)) {
                w->ensurePolished();
                if (auto* fe = dynamic_cast<FluentElement*>(w)) {
                    fe->onThemeUpdated();
                }
            }
        }
    }

    QRect parentRect = contentsRect();
    for (Item& it : m_items) {
        if (QWidget* w = fluentLayoutItemWidget(it.item)) {
            if (auto* qp = dynamic_cast<QMLPlus*>(w)) it.anchors = *(qp->anchors());
        }
        it.geometry = QRect(parentRect.topLeft(), effectiveItemSize(it.item));
    }
    for (int pass = 0; pass < 5; ++pass) {
        bool changed = false;
        for (Item& it : m_items) {
            QRect old = it.geometry; QSize s = effectiveItemSize(it.item);
            if (it.anchors.fill) { it.geometry = parentRect.marginsRemoved(it.anchors.fillMargins); }
            else {
                if (it.anchors.horizontalCenter.edge != Edge::None) {
                    it.geometry.moveCenter(QPoint(getEdgeValue(it.anchors.horizontalCenter.target, it.anchors.horizontalCenter.edge, parentRect) + it.anchors.horizontalCenter.offset, it.geometry.center().y()));
                } else {
                    int l = getEdgeValue(it.anchors.left.target, it.anchors.left.edge, parentRect) + it.anchors.left.offset;
                    int r = getEdgeValue(it.anchors.right.target, it.anchors.right.edge, parentRect) + it.anchors.right.offset;
                    if (it.anchors.left.edge != Edge::None && it.anchors.right.edge != Edge::None) { it.geometry.setLeft(l); it.geometry.setRight(r); }
                    else if (it.anchors.left.edge != Edge::None) { it.geometry.moveLeft(l); it.geometry.setWidth(s.width()); }
                    else if (it.anchors.right.edge != Edge::None) { it.geometry.moveRight(r); it.geometry.setWidth(s.width()); }
                }
                if (it.anchors.verticalCenter.edge != Edge::None) {
                    it.geometry.moveCenter(QPoint(it.geometry.center().x(), getEdgeValue(it.anchors.verticalCenter.target, it.anchors.verticalCenter.edge, parentRect) + it.anchors.verticalCenter.offset));
                } else {
                    int t = getEdgeValue(it.anchors.top.target, it.anchors.top.edge, parentRect) + it.anchors.top.offset;
                    int b = getEdgeValue(it.anchors.bottom.target, it.anchors.bottom.edge, parentRect) + it.anchors.bottom.offset;
                    if (it.anchors.top.edge != Edge::None && it.anchors.bottom.edge != Edge::None) { it.geometry.setTop(t); it.geometry.setBottom(b); }
                    else if (it.anchors.top.edge != Edge::None) { it.geometry.moveTop(t); it.geometry.setHeight(s.height()); }
                    else if (it.anchors.bottom.edge != Edge::None) { it.geometry.moveBottom(b); it.geometry.setHeight(s.height()); }
                }
            }
            if (it.geometry != old) changed = true;
        }
        if (!changed) break;
    }
    for (const Item& it : m_items) fluentLayoutItemWidget(it.item)->setGeometry(it.geometry);
}

// --- PropertyBinder implementation. zh_CN: PropertyBinder 实现。---

PropertyLink::PropertyLink(QObject* from, const QMetaProperty& fromProp, QObject* to, const QMetaProperty& toProp, QObject* parent)
    : QObject(parent), m_from(from), m_fromProp(fromProp), m_to(to), m_toProp(toProp) {}

void PropertyLink::syncToTarget() {
    if (!m_from || !m_to) return;
    QVariant val = m_fromProp.read(m_from);
    if (val.isValid() && m_toProp.read(m_to) != val) m_toProp.write(m_to, val);
}

void PropertyBinder::bind(QObject* s, const char* sp, QObject* t, const char* tp, Direction dir) {
    if (!s || !t) return;
    auto sProp = s->metaObject()->property(s->metaObject()->indexOfProperty(sp));
    auto tProp = t->metaObject()->property(t->metaObject()->indexOfProperty(tp));
    if (!sProp.isValid() || !tProp.isValid()) return;
    
    auto* link1 = new PropertyLink(s, sProp, t, tProp, t);
    
    // The pointer-based connect syntax is safer and version-portable.
    // zh_CN: 使用新的信号槽连接语法，更安全且跨版本兼容。
    QObject::connect(s, sProp.notifySignal(), link1, link1->metaObject()->method(link1->metaObject()->indexOfMethod("syncToTarget()")));
    
    link1->syncToTarget();
    
    if (dir == TwoWay) {
        auto* link2 = new PropertyLink(t, tProp, s, sProp, s);
        QObject::connect(t, tProp.notifySignal(), link2, link2->metaObject()->method(link2->metaObject()->indexOfMethod("syncToTarget()")));
    }
}

// --- QMLPlus implementation. zh_CN: QMLPlus 实现。---

QMLPlus::QMLPlus() : m_anchors(nullptr), m_currentState("") {}
QMLPlus::~QMLPlus() { delete m_anchors; }

AnchorLayout::Anchors* QMLPlus::anchors() { 
    if (!m_anchors) m_anchors = new AnchorLayout::Anchors(); 
    return m_anchors; 
}

void QMLPlus::setState(const QString& name) { 
    if (m_currentState != name) { applyState(name); m_currentState = name; } 
}

void QMLPlus::addState(const QMLState& state) { 
    m_states[state.name] = state; 
}

void QMLPlus::bind(const char* tp, QObject* s, const char* sp, PropertyBinder::Direction dir) {
    // Auto-discovers the QWidget host that mixes in QMLPlus. zh_CN: 自动发现混入 QMLPlus 的 QWidget 宿主。
    if (auto* host = dynamic_cast<QWidget*>(this)) {
        PropertyBinder::bind(s, sp, host, tp, dir);
    } else {
        qWarning() << "QMLPlus::bind failed: Host is not a QWidget!";
    }
}

void QMLPlus::applyState(const QString& name) {
    if (name.isEmpty()) {
        for (auto it = m_defaultValues.begin(); it != m_defaultValues.end(); ++it) {
            if (!it.key()) continue;
            for (auto pIt = it.value().begin(); pIt != it.value().end(); ++pIt) it.key()->setProperty(pIt.key().constData(), pIt.value());
        }
        return;
    }
    if (!m_states.contains(name)) return;
    for (const auto& c : m_states[name].changes) {
        if (!c.target) continue;
        if (!m_defaultValues[c.target.data()].contains(c.propertyName)) m_defaultValues[c.target.data()][c.propertyName] = c.target->property(c.propertyName.constData());
        c.target->setProperty(c.propertyName.constData(), c.value);
    }
}

} // namespace fluent
