#include "components/foundation/QMLPlus.h"
#include "components/foundation/FluentElement.h"
#include "compatibility/QtCompat.h"
#include "utils/private/FluentQtLogging_p.h"

#include <QSet>
#include <QStringList>
#include <QWidgetItem>

#include <algorithm>
#include <functional>
#include <set>

namespace fluent {

// --- AnchorLayout implementation. zh_CN: AnchorLayout 实现。---

namespace {

QSize normalizedSize(QSize size)
{
    size.setWidth(qMax(0, size.width()));
    size.setHeight(qMax(0, size.height()));
    return size;
}

QSize effectiveItemSize(const QLayoutItem* item, bool minimum)
{
    if (!item)
        return QSize();

    QSize size = minimum ? item->minimumSize() : item->sizeHint();
    if (const QWidget* widget = fluentLayoutItemWidget(item)) {
        size = size.expandedTo(widget->minimumSize());
        size = size.boundedTo(widget->maximumSize());
    }
    return normalizedSize(size);
}

} // namespace

AnchorLayout::AnchorLayout(QWidget* parent) : QLayout(parent) {
    setContentsMargins(0, 0, 0, 0);
}

AnchorLayout::~AnchorLayout() {
    while (!m_items.isEmpty()) delete takeAt(0);
}

void AnchorLayout::addItem(QLayoutItem* item) {
    if (!item)
        return;
    Item it;
    it.item = item;
    m_items.append(it);
    invalidate();
}

int AnchorLayout::count() const { return m_items.size(); }

QLayoutItem* AnchorLayout::itemAt(int index) const {
    return (index >= 0 && index < m_items.size()) ? m_items.at(index).item : nullptr;
}

QLayoutItem* AnchorLayout::takeAt(int index) {
    if (index < 0 || index >= m_items.size()) return nullptr;
    QLayoutItem* item = m_items.takeAt(index).item;
    invalidate();
    return item;
}

QSize AnchorLayout::sizeHint() const
{
    return measuredSize(false).expandedTo(minimumSize());
}

QSize AnchorLayout::minimumSize() const
{
    return measuredSize(true);
}

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

int AnchorLayout::getWidgetIndex(QWidget* widget) const {
    for (int i = 0; i < m_items.size(); ++i) {
        if (fluentLayoutItemWidget(m_items[i].item) == widget)
            return i;
    }
    return -1;
}

AnchorLayout::Anchors AnchorLayout::currentAnchors(const Item& item) const
{
    if (QWidget* widget = fluentLayoutItemWidget(item.item)) {
        if (auto* qmlPlus = dynamic_cast<QMLPlus*>(widget))
            return *qmlPlus->anchors();
    }
    return item.anchors;
}

QVector<QRect> AnchorLayout::resolveGeometries(const QRect& parentRect,
                                               bool minimum,
                                               bool reportCycles) const
{
    const int itemCount = m_items.size();
    QVector<QRect> geometries(itemCount);
    QVector<QSize> naturalSizes(itemCount);
    QVector<Anchors> anchors(itemCount);
    QVector<QVector<int>> dependencies(itemCount);

    Anchor Anchors::* const anchorMembers[] = {
        &Anchors::left,
        &Anchors::right,
        &Anchors::top,
        &Anchors::bottom,
        &Anchors::horizontalCenter,
        &Anchors::verticalCenter,
    };

    for (int i = 0; i < itemCount; ++i) {
        naturalSizes[i] = effectiveItemSize(m_items[i].item, minimum);
        geometries[i] = QRect(parentRect.topLeft(), naturalSizes[i]);
        anchors[i] = currentAnchors(m_items[i]);
        if (anchors[i].fill)
            continue;

        for (Anchor Anchors::* member : anchorMembers) {
            const Anchor& anchor = anchors[i].*member;
            if (anchor.edge == Edge::None || anchor.target.isNull())
                continue;
            const int targetIndex = getWidgetIndex(anchor.target.data());
            if (targetIndex >= 0 && !dependencies[i].contains(targetIndex))
                dependencies[i].append(targetIndex);
        }
        std::sort(dependencies[i].begin(), dependencies[i].end());
    }

    // Tarjan's algorithm identifies only the actual strongly connected
    // components. Descendants of a cycle remain resolvable after the cyclic
    // edges are ignored.
    QVector<int> discoveryIndex(itemCount, -1);
    QVector<int> lowLink(itemCount, -1);
    QVector<int> stack;
    QVector<bool> onStack(itemCount, false);
    QVector<QVector<int>> components;
    int nextIndex = 0;
    std::function<void(int)> visit = [&](int itemIndex) {
        discoveryIndex[itemIndex] = nextIndex;
        lowLink[itemIndex] = nextIndex;
        ++nextIndex;
        stack.append(itemIndex);
        onStack[itemIndex] = true;

        for (int dependency : dependencies[itemIndex]) {
            if (discoveryIndex[dependency] < 0) {
                visit(dependency);
                lowLink[itemIndex] =
                    qMin(lowLink[itemIndex], lowLink[dependency]);
            } else if (onStack[dependency]) {
                lowLink[itemIndex] =
                    qMin(lowLink[itemIndex], discoveryIndex[dependency]);
            }
        }

        if (lowLink[itemIndex] != discoveryIndex[itemIndex])
            return;

        QVector<int> component;
        while (!stack.isEmpty()) {
            const int member = stack.takeLast();
            onStack[member] = false;
            component.append(member);
            if (member == itemIndex)
                break;
        }
        std::sort(component.begin(), component.end());
        components.append(component);
    };

    for (int i = 0; i < itemCount; ++i) {
        if (discoveryIndex[i] < 0)
            visit(i);
    }

    QVector<int> componentForItem(itemCount, -1);
    QVector<bool> cyclicComponent(components.size(), false);
    QVector<QVector<int>> cycleGroups;
    for (int componentIndex = 0;
         componentIndex < components.size();
         ++componentIndex) {
        const QVector<int>& component = components[componentIndex];
        for (int itemIndex : component)
            componentForItem[itemIndex] = componentIndex;

        const bool selfCycle =
            component.size() == 1
            && dependencies[component.constFirst()].contains(
                component.constFirst());
        cyclicComponent[componentIndex] =
            component.size() > 1 || selfCycle;
        if (cyclicComponent[componentIndex])
            cycleGroups.append(component);
    }
    std::sort(cycleGroups.begin(), cycleGroups.end(),
              [](const QVector<int>& lhs, const QVector<int>& rhs) {
                  return lhs.constFirst() < rhs.constFirst();
              });

    QStringList diagnosticGroups;
    for (const QVector<int>& group : cycleGroups) {
        QStringList members;
        for (int itemIndex : group) {
            const QWidget* widget =
                fluentLayoutItemWidget(m_items[itemIndex].item);
            QString name = widget ? widget->objectName() : QString();
            if (name.isEmpty() && widget)
                name = QString::fromLatin1(widget->metaObject()->className());
            if (name.isEmpty())
                name = QStringLiteral("layout-item");
            members.append(QStringLiteral("%1:%2").arg(itemIndex).arg(name));
        }
        diagnosticGroups.append(
            QStringLiteral("[%1]").arg(members.join(QStringLiteral(", "))));
    }
    const QString cycleDiagnostic =
        diagnosticGroups.join(QStringLiteral(" "));
    if (reportCycles) {
        if (cycleDiagnostic.isEmpty()) {
            m_lastCycleDiagnostic.clear();
        } else if (cycleDiagnostic != m_lastCycleDiagnostic) {
            m_lastCycleDiagnostic = cycleDiagnostic;
            const QWidget* layoutParent = parentWidget();
            QString parentName =
                layoutParent ? layoutParent->objectName() : QString();
            if (parentName.isEmpty() && layoutParent)
                parentName =
                    QString::fromLatin1(layoutParent->metaObject()->className());
            if (parentName.isEmpty())
                parentName = QStringLiteral("no-parent");
            qCWarning(logging::layoutCategory).noquote()
                << "AnchorLayout on" << parentName
                << "ignored cyclic sibling anchors:"
                << cycleDiagnostic;
        }
    }

    auto isIgnoredCycleEdge = [&](int itemIndex, int targetIndex) {
        if (targetIndex < 0)
            return false;
        const int componentIndex = componentForItem[itemIndex];
        return componentIndex >= 0
               && componentIndex == componentForItem[targetIndex]
               && cyclicComponent[componentIndex];
    };

    QVector<QVector<int>> dependents(itemCount);
    QVector<int> inDegree(itemCount, 0);
    for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
        for (int dependency : dependencies[itemIndex]) {
            if (isIgnoredCycleEdge(itemIndex, dependency))
                continue;
            dependents[dependency].append(itemIndex);
            ++inDegree[itemIndex];
        }
    }

    std::set<int> ready;
    for (int i = 0; i < itemCount; ++i) {
        if (inDegree[i] == 0)
            ready.insert(i);
    }
    QVector<int> resolutionOrder;
    resolutionOrder.reserve(itemCount);
    while (!ready.empty()) {
        const int itemIndex = *ready.begin();
        ready.erase(ready.begin());
        resolutionOrder.append(itemIndex);
        for (int dependent : dependents[itemIndex]) {
            --inDegree[dependent];
            if (inDegree[dependent] == 0)
                ready.insert(dependent);
        }
    }
    // The SCC reduction above should always produce a DAG. Keep a stable
    // fallback so malformed future constraint types cannot reintroduce drift.
    for (int i = 0; i < itemCount; ++i) {
        if (!resolutionOrder.contains(i))
            resolutionOrder.append(i);
    }

    auto targetRect = [&](int itemIndex,
                          const Anchor& anchor,
                          QRect* result) -> bool {
        if (!result || anchor.edge == Edge::None || anchor.target.isNull())
            return false;

        QWidget* target = anchor.target.data();
        if (target == parentWidget()) {
            *result = parentRect;
            return true;
        }

        const int targetIndex = getWidgetIndex(target);
        if (targetIndex >= 0) {
            if (isIgnoredCycleEdge(itemIndex, targetIndex))
                return false;
            *result = geometries[targetIndex];
            return true;
        }

        QWidget* layoutParent = parentWidget();
        if (!layoutParent)
            return false;
        if (target->parentWidget() == layoutParent) {
            *result = target->geometry();
            return true;
        }
        if (target->window() != layoutParent->window())
            return false;
        *result = QRect(target->mapTo(layoutParent, QPoint(0, 0)),
                        target->size());
        return true;
    };

    auto edgeValue = [&](int itemIndex,
                         const Anchor& anchor,
                         int* value) -> bool {
        QRect rectangle;
        if (!value || !targetRect(itemIndex, anchor, &rectangle))
            return false;
        switch (anchor.edge) {
        case Edge::Left:
            *value = rectangle.x();
            return true;
        case Edge::Right:
            *value = rectangle.x() + rectangle.width();
            return true;
        case Edge::Top:
            *value = rectangle.y();
            return true;
        case Edge::Bottom:
            *value = rectangle.y() + rectangle.height();
            return true;
        case Edge::HCenter:
            *value = rectangle.x() + rectangle.width() / 2;
            return true;
        case Edge::VCenter:
            *value = rectangle.y() + rectangle.height() / 2;
            return true;
        case Edge::None:
            break;
        }
        return false;
    };

    for (int itemIndex : resolutionOrder) {
        const QSize naturalSize = naturalSizes[itemIndex];
        const Anchors& itemAnchors = anchors[itemIndex];
        QRect geometry(parentRect.topLeft(), naturalSize);

        if (itemAnchors.fill) {
            const QMargins margins = itemAnchors.fillMargins;
            geometry = QRect(parentRect.x() + margins.left(),
                             parentRect.y() + margins.top(),
                             qMax(0, parentRect.width()
                                         - margins.left()
                                         - margins.right()),
                             qMax(0, parentRect.height()
                                         - margins.top()
                                         - margins.bottom()));
            geometries[itemIndex] = geometry;
            continue;
        }

        int center = 0;
        if (edgeValue(itemIndex, itemAnchors.horizontalCenter, &center)) {
            geometry.moveLeft(center
                              + itemAnchors.horizontalCenter.offset
                              - naturalSize.width() / 2);
        } else {
            int left = 0;
            int right = 0;
            const bool hasLeft =
                edgeValue(itemIndex, itemAnchors.left, &left);
            const bool hasRight =
                edgeValue(itemIndex, itemAnchors.right, &right);
            if (hasLeft)
                left += itemAnchors.left.offset;
            if (hasRight)
                right += itemAnchors.right.offset;

            if (hasLeft && hasRight) {
                geometry.setX(left);
                geometry.setWidth(qMax(0, right - left));
            } else if (hasLeft) {
                geometry.moveLeft(left);
            } else if (hasRight) {
                geometry.moveLeft(right - naturalSize.width());
            }
        }

        if (edgeValue(itemIndex, itemAnchors.verticalCenter, &center)) {
            geometry.moveTop(center
                             + itemAnchors.verticalCenter.offset
                             - naturalSize.height() / 2);
        } else {
            int top = 0;
            int bottom = 0;
            const bool hasTop =
                edgeValue(itemIndex, itemAnchors.top, &top);
            const bool hasBottom =
                edgeValue(itemIndex, itemAnchors.bottom, &bottom);
            if (hasTop)
                top += itemAnchors.top.offset;
            if (hasBottom)
                bottom += itemAnchors.bottom.offset;

            if (hasTop && hasBottom) {
                geometry.setY(top);
                geometry.setHeight(qMax(0, bottom - top));
            } else if (hasTop) {
                geometry.moveTop(top);
            } else if (hasBottom) {
                geometry.moveTop(bottom - naturalSize.height());
            }
        }

        geometries[itemIndex] = geometry;
    }
    return geometries;
}

QSize AnchorLayout::measuredSize(bool minimum) const
{
    int leftMargin = 0;
    int topMargin = 0;
    int rightMargin = 0;
    int bottomMargin = 0;
    getContentsMargins(&leftMargin, &topMargin, &rightMargin, &bottomMargin);

    if (m_items.isEmpty()) {
        return QSize(leftMargin + rightMargin, topMargin + bottomMargin);
    }

    QVector<QSize> naturalSizes;
    naturalSizes.reserve(m_items.size());
    QSize candidate;
    for (const Item& item : m_items) {
        const QSize naturalSize = effectiveItemSize(item.item, minimum);
        naturalSizes.append(naturalSize);
        candidate = candidate.expandedTo(naturalSize);
    }

    // Probe a comfortably large parent once to distinguish constraints that
    // can be satisfied by growing the layout from constant outward offsets
    // that can never move inside the parent.
    // zh_CN: 先以足够大的父区域探测一次，用于区分“扩大布局即可满足”的约束
    // 与无论如何扩大都仍位于父区域之外的固定偏移。
    constexpr int kProbeExtent = 1 << 20;
    const QRect probeRect(0, 0, kProbeExtent, kProbeExtent);
    const QVector<QRect> probeGeometries =
        resolveGeometries(probeRect, minimum, false);

    struct MeasurableBounds {
        bool naturalWidth = false;
        bool naturalHeight = false;
        bool left = false;
        bool top = false;
        bool right = false;
        bool bottom = false;
    };
    QVector<MeasurableBounds> measurable(m_items.size());
    for (int i = 0; i < m_items.size(); ++i) {
        const QRect& geometry = probeGeometries[i];
        const QSize& naturalSize = naturalSizes[i];
        measurable[i] = {
            geometry.width() >= naturalSize.width(),
            geometry.height() >= naturalSize.height(),
            geometry.left() >= probeRect.left(),
            geometry.top() >= probeRect.top(),
            geometry.x() + geometry.width()
                <= probeRect.x() + probeRect.width(),
            geometry.y() + geometry.height()
                <= probeRect.y() + probeRect.height(),
        };
    }

    // Grow to the smallest discrete parent rectangle that preserves every
    // satisfiable natural size and boundary. Re-resolving the complete anchor
    // graph on each pass also handles chains that connect a leading item to a
    // trailing item, such as ContentDialog's title/content/action stack.
    // zh_CN: 逐轮扩展到能保留所有可满足自然尺寸与边界的最小整数父区域；每轮
    // 重新求解完整锚点图，也可覆盖从顶部元素连接到底部元素的约束链。
    constexpr int kMaximumPasses = 64;
    for (int pass = 0; pass < kMaximumPasses; ++pass) {
        const QRect candidateRect(QPoint(0, 0), candidate);
        const QVector<QRect> geometries =
            resolveGeometries(candidateRect, minimum, false);
        int growWidth = 0;
        int growHeight = 0;

        for (int i = 0; i < m_items.size(); ++i) {
            const QRect& geometry = geometries[i];
            const QSize& naturalSize = naturalSizes[i];
            const MeasurableBounds& bounds = measurable[i];
            if (bounds.naturalWidth)
                growWidth =
                    qMax(growWidth, naturalSize.width() - geometry.width());
            if (bounds.naturalHeight)
                growHeight =
                    qMax(growHeight, naturalSize.height() - geometry.height());
            if (bounds.left)
                growWidth = qMax(growWidth, -geometry.x());
            if (bounds.top)
                growHeight = qMax(growHeight, -geometry.y());
            if (bounds.right) {
                growWidth =
                    qMax(growWidth,
                         geometry.x() + geometry.width() - candidate.width());
            }
            if (bounds.bottom) {
                growHeight =
                    qMax(growHeight,
                         geometry.y() + geometry.height()
                             - candidate.height());
            }
        }

        if (growWidth <= 0 && growHeight <= 0)
            break;
        candidate.rwidth() += qMax(0, growWidth);
        candidate.rheight() += qMax(0, growHeight);
    }

    return QSize(candidate.width() + leftMargin + rightMargin,
                 candidate.height() + topMargin + bottomMargin);
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

    const QVector<QRect> geometries =
        resolveGeometries(contentsRect(), false, true);
    for (int i = 0; i < m_items.size(); ++i) {
        Item& it = m_items[i];
        it.anchors = currentAnchors(it);
        it.geometry = geometries[i];
        if (!it.item)
            continue;
        if (QWidget* widget = fluentLayoutItemWidget(it.item))
            widget->setGeometry(it.geometry);
        else
            it.item->setGeometry(it.geometry);
    }
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

QMLPlus::~QMLPlus() {
    for (auto it = m_defaultValueCleanupConnections.cbegin();
         it != m_defaultValueCleanupConnections.cend(); ++it) {
        QObject::disconnect(it.value());
    }
    delete m_anchors;
}

AnchorLayout::Anchors* QMLPlus::anchors() { 
    if (!m_anchors) m_anchors = new AnchorLayout::Anchors(); 
    return m_anchors; 
}

void QMLPlus::setState(const QString& name) {
    if (m_currentState == name)
        return;
    if (!name.isEmpty() && !m_states.contains(name))
        return;

    if (m_stateChangeInProgress) {
        m_pendingState = name;
        m_hasPendingState = true;
        return;
    }

    QObject* host = dynamic_cast<QObject*>(this);
    const QPointer<QObject> hostGuard(host);
    const int maximumTransitions = qMax(8, m_states.size() * 2 + 2);
    int transitionCount = 0;
    QString requestedState = name;

    m_stateChangeInProgress = true;
    m_hasPendingState = false;
    while (m_currentState != requestedState) {
        // Publish the destination before applying properties so same-state
        // callbacks cannot recursively enter the transition.
        m_currentState = requestedState;
        applyState(requestedState);
        if (host && !hostGuard)
            return;
        ++transitionCount;

        if (!m_hasPendingState)
            break;

        requestedState = m_pendingState;
        m_hasPendingState = false;
        if ((!requestedState.isEmpty() && !m_states.contains(requestedState))
            || requestedState == m_currentState) {
            break;
        }
        if (transitionCount >= maximumTransitions) {
            qCWarning(logging::layoutCategory)
                << "QMLPlus stopped a cyclic reentrant state transition at"
                << m_currentState;
            break;
        }
    }
    m_stateChangeInProgress = false;
    m_hasPendingState = false;
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

void QMLPlus::rememberDefaultValue(QObject* target, const QByteArray& propertyName) {
    if (!target || m_defaultValues[target].contains(propertyName))
        return;

    m_defaultValues[target].insert(
        propertyName, target->property(propertyName.constData()));
    if (m_defaultValueCleanupConnections.contains(target))
        return;

    const QMetaObject::Connection connection =
        QObject::connect(target, &QObject::destroyed,
                         [this, target](QObject*) {
                             m_defaultValues.remove(target);
                             m_defaultValueCleanupConnections.remove(target);
                         });
    m_defaultValueCleanupConnections.insert(target, connection);
}

bool QMLPlus::restoreDefaultValues() {
    struct RestoreEntry {
        QPointer<QObject> target;
        QMap<QByteArray, QVariant> values;
    };

    QVector<RestoreEntry> entries;
    entries.reserve(m_defaultValues.size());
    for (auto targetIt = m_defaultValues.cbegin();
         targetIt != m_defaultValues.cend(); ++targetIt) {
        entries.push_back({targetIt.key(), targetIt.value()});
    }

    QObject* host = dynamic_cast<QObject*>(this);
    const QPointer<QObject> hostGuard(host);
    for (const RestoreEntry& entry : std::as_const(entries)) {
        QObject* target = entry.target.data();
        if (!target)
            continue;
        for (auto propertyIt = entry.values.cbegin();
             propertyIt != entry.values.cend(); ++propertyIt) {
            target->setProperty(propertyIt.key().constData(), propertyIt.value());
            if (host && !hostGuard)
                return false;
            if (!entry.target)
                break;
        }
    }
    return true;
}

void QMLPlus::applyState(const QString& name) {
    QMLState state;
    if (!name.isEmpty()) {
        const auto stateIt = m_states.constFind(name);
        if (stateIt == m_states.constEnd())
            return;
        state = stateIt.value();
    }

    if (!restoreDefaultValues() || name.isEmpty())
        return;

    QObject* host = dynamic_cast<QObject*>(this);
    const QPointer<QObject> hostGuard(host);
    for (const PropertyChange& change : std::as_const(state.changes)) {
        QObject* target = change.target.data();
        if (!target)
            continue;
        rememberDefaultValue(target, change.propertyName);
        target->setProperty(change.propertyName.constData(), change.value);
        if (host && !hostGuard)
            return;
    }
}

} // namespace fluent
