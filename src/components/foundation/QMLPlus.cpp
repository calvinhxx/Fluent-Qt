#include "compatibility/WidgetCompat.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "compatibility/private/VariantCompat_p.h"
#include "utils/private/FluentQtLogging_p.h"

#include <QSet>
#include <QStringList>
#include <QWidgetItem>
#include <QThread>
#include <QTimer>
#include <QVariantAnimation>

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

AnchorLayout::AnchorLayout(QWidget* parent) : QLayout(parent)
{
    setContentsMargins(0, 0, 0, 0);
}

AnchorLayout::~AnchorLayout()
{
    while (!m_items.isEmpty())
        delete takeAt(0);
}

void AnchorLayout::addItem(QLayoutItem* item)
{
    if (!item)
        return;
    Item it;
    it.item = item;
    m_items.append(it);
    invalidate();
}

int AnchorLayout::count() const
{
    return m_items.size();
}

QLayoutItem* AnchorLayout::itemAt(int index) const
{
    return (index >= 0 && index < m_items.size()) ? m_items.at(index).item : nullptr;
}

QLayoutItem* AnchorLayout::takeAt(int index)
{
    if (index < 0 || index >= m_items.size())
        return nullptr;
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

void AnchorLayout::addAnchoredWidget(QWidget* w, const Anchors& anchors)
{
    if (!w)
        return;
    if (parentWidget() && w->parent() != parentWidget())
        w->setParent(parentWidget());
    if (auto* qp = dynamic_cast<QMLPlus*>(w))
        *(qp->anchors()) = anchors;
    for (Item& it : m_items) {
        if (fluentLayoutItemWidget(it.item) == w) {
            it.anchors = anchors;
            invalidate();
            return;
        }
    }
    addItem(new QWidgetItem(w));
    m_items.last().anchors = anchors;
    invalidate();
}

AnchorLayout::Anchors AnchorLayout::anchorsForWidget(QWidget* widget) const
{
    const int index = getWidgetIndex(widget);
    return index >= 0 ? currentAnchors(m_items[index]) : Anchors();
}

bool AnchorLayout::setWidgetAnchors(QWidget* widget, const Anchors& anchors)
{
    const int index = getWidgetIndex(widget);
    if (index < 0)
        return false;
    if (auto* plus = dynamic_cast<QMLPlus*>(widget))
        *plus->anchors() = anchors;
    m_items[index].anchors = anchors;
    invalidate();
    return true;
}

int AnchorLayout::getWidgetIndex(QWidget* widget) const
{
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

QVector<QRect> AnchorLayout::resolveGeometries(const QRect& parentRect, bool minimum, bool reportCycles) const
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
                lowLink[itemIndex] = qMin(lowLink[itemIndex], lowLink[dependency]);
            } else if (onStack[dependency]) {
                lowLink[itemIndex] = qMin(lowLink[itemIndex], discoveryIndex[dependency]);
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
    for (int componentIndex = 0; componentIndex < components.size(); ++componentIndex) {
        const QVector<int>& component = components[componentIndex];
        for (int itemIndex : component)
            componentForItem[itemIndex] = componentIndex;

        const bool selfCycle = component.size() == 1
            && dependencies[component.constFirst()].contains(component.constFirst());
        cyclicComponent[componentIndex] = component.size() > 1 || selfCycle;
        if (cyclicComponent[componentIndex])
            cycleGroups.append(component);
    }
    std::sort(cycleGroups.begin(), cycleGroups.end(), [](const QVector<int>& lhs, const QVector<int>& rhs) {
        return lhs.constFirst() < rhs.constFirst();
    });

    QStringList diagnosticGroups;
    for (const QVector<int>& group : cycleGroups) {
        QStringList members;
        for (int itemIndex : group) {
            const QWidget* widget = fluentLayoutItemWidget(m_items[itemIndex].item);
            QString name = widget ? widget->objectName() : QString();
            if (name.isEmpty() && widget)
                name = QString::fromLatin1(widget->metaObject()->className());
            if (name.isEmpty())
                name = QStringLiteral("layout-item");
            members.append(QStringLiteral("%1:%2").arg(itemIndex).arg(name));
        }
        diagnosticGroups.append(QStringLiteral("[%1]").arg(members.join(QStringLiteral(", "))));
    }
    const QString cycleDiagnostic = diagnosticGroups.join(QStringLiteral(" "));
    if (reportCycles) {
        if (cycleDiagnostic.isEmpty()) {
            m_lastCycleDiagnostic.clear();
        } else if (cycleDiagnostic != m_lastCycleDiagnostic) {
            m_lastCycleDiagnostic = cycleDiagnostic;
            const QWidget* layoutParent = parentWidget();
            QString parentName = layoutParent ? layoutParent->objectName() : QString();
            if (parentName.isEmpty() && layoutParent)
                parentName = QString::fromLatin1(layoutParent->metaObject()->className());
            if (parentName.isEmpty())
                parentName = QStringLiteral("no-parent");
            qCWarning(logging::layoutCategory).noquote()
                << "AnchorLayout on" << parentName << "ignored cyclic sibling anchors:" << cycleDiagnostic;
        }
    }

    auto isIgnoredCycleEdge = [&](int itemIndex, int targetIndex) {
        if (targetIndex < 0)
            return false;
        const int componentIndex = componentForItem[itemIndex];
        return componentIndex >= 0 && componentIndex == componentForItem[targetIndex]
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

    auto targetRect = [&](int itemIndex, const Anchor& anchor, QRect* result) -> bool {
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
        *result = QRect(target->mapTo(layoutParent, QPoint(0, 0)), target->size());
        return true;
    };

    auto edgeValue = [&](int itemIndex, const Anchor& anchor, int* value) -> bool {
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
            geometry = QRect(parentRect.x() + margins.left(), parentRect.y() + margins.top(),
                qMax(0, parentRect.width() - margins.left() - margins.right()),
                qMax(0, parentRect.height() - margins.top() - margins.bottom()));
            geometries[itemIndex] = geometry;
            continue;
        }

        int center = 0;
        if (edgeValue(itemIndex, itemAnchors.horizontalCenter, &center)) {
            geometry.moveLeft(center + itemAnchors.horizontalCenter.offset - naturalSize.width() / 2);
        } else {
            int left = 0;
            int right = 0;
            const bool hasLeft = edgeValue(itemIndex, itemAnchors.left, &left);
            const bool hasRight = edgeValue(itemIndex, itemAnchors.right, &right);
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
            geometry.moveTop(center + itemAnchors.verticalCenter.offset - naturalSize.height() / 2);
        } else {
            int top = 0;
            int bottom = 0;
            const bool hasTop = edgeValue(itemIndex, itemAnchors.top, &top);
            const bool hasBottom = edgeValue(itemIndex, itemAnchors.bottom, &bottom);
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
    const QVector<QRect> probeGeometries = resolveGeometries(probeRect, minimum, false);

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
            geometry.x() + geometry.width() <= probeRect.x() + probeRect.width(),
            geometry.y() + geometry.height() <= probeRect.y() + probeRect.height(),
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
        const QVector<QRect> geometries = resolveGeometries(candidateRect, minimum, false);
        int growWidth = 0;
        int growHeight = 0;

        for (int i = 0; i < m_items.size(); ++i) {
            const QRect& geometry = geometries[i];
            const QSize& naturalSize = naturalSizes[i];
            const MeasurableBounds& bounds = measurable[i];
            if (bounds.naturalWidth)
                growWidth = qMax(growWidth, naturalSize.width() - geometry.width());
            if (bounds.naturalHeight)
                growHeight = qMax(growHeight, naturalSize.height() - geometry.height());
            if (bounds.left)
                growWidth = qMax(growWidth, -geometry.x());
            if (bounds.top)
                growHeight = qMax(growHeight, -geometry.y());
            if (bounds.right) {
                growWidth = qMax(growWidth, geometry.x() + geometry.width() - candidate.width());
            }
            if (bounds.bottom) {
                growHeight = qMax(growHeight, geometry.y() + geometry.height() - candidate.height());
            }
        }

        if (growWidth <= 0 && growHeight <= 0)
            break;
        candidate.rwidth() += qMax(0, growWidth);
        candidate.rheight() += qMax(0, growHeight);
    }

    return QSize(candidate.width() + leftMargin + rightMargin, candidate.height() + topMargin + bottomMargin);
}

void AnchorLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    if (m_items.isEmpty())
        return;

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

    const QVector<QRect> geometries = resolveGeometries(contentsRect(), false, true);
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

// --- Managed binding and state runtime. zh_CN: 可管理绑定与状态运行时。---

namespace {

bool validThread(QObject* object)
{
    return object && object->thread() == QThread::currentThread();
}

QMetaProperty metaProperty(QObject* object, const QByteArray& name)
{
    if (!object || name.isEmpty())
        return {};
    const int index = object->metaObject()->indexOfProperty(name.constData());
    return index < 0 ? QMetaProperty() : object->metaObject()->property(index);
}

bool validDependencies(const QVector<BindingDependency>& dependencies, bool requireNotify = true)
{
    for (const BindingDependency& dependency : dependencies) {
        const QMetaProperty property = metaProperty(dependency.object, dependency.propertyName);
        if (!validThread(dependency.object) || !property.isReadable() || (requireNotify && !property.hasNotifySignal()))
            return false;
    }
    return true;
}

bool interpolates(int type)
{
    switch (type) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Double:
    case QMetaType::Float:
    case QMetaType::QColor:
    case QMetaType::QPoint:
    case QMetaType::QPointF:
    case QMetaType::QSize:
    case QMetaType::QSizeF:
    case QMetaType::QRect:
    case QMetaType::QRectF:
        return true;
    default:
        return false;
    }
}

void invalidateAnchors(QWidget* widget)
{
    if (widget && widget->parentWidget() && widget->parentWidget()->layout()) {
        QLayout* layout = widget->parentWidget()->layout();
        layout->invalidate();
        layout->activate();
    }
}

AnchorLayout::Anchors widgetAnchors(QWidget* widget)
{
    if (auto* plus = dynamic_cast<QMLPlus*>(widget))
        return *plus->anchors();
    if (widget && widget->parentWidget()) {
        if (auto* layout = qobject_cast<AnchorLayout*>(widget->parentWidget()->layout()))
            return layout->anchorsForWidget(widget);
    }
    return {};
}

void replaceWidgetAnchors(QWidget* widget, const AnchorLayout::Anchors& anchors)
{
    if (auto* plus = dynamic_cast<QMLPlus*>(widget)) {
        plus->setAnchors(anchors);
    } else if (widget && widget->parentWidget()) {
        if (auto* layout = qobject_cast<AnchorLayout*>(widget->parentWidget()->layout())) {
            layout->setWidgetAnchors(widget, anchors);
            layout->activate();
        }
    }
}

} // namespace

class QMLPlusPropertyHub : public QObject {
public:
    explicit QMLPlusPropertyHub(QObject* target) : QObject(target), m_target(target) {}

    struct Entry {
        QPointer<detail::QMLBinding> binding;
        QPointer<QVariantAnimation> animation;
        QPointer<QMLPlusRuntime> observer;
        QSet<QObject*> blockers;
        PropertyAnimation behavior;
        bool hasBehavior = false;
        int writing = 0;
    };

    static QMLPlusPropertyHub* get(QObject* target, bool create = true)
    {
        if (!target)
            return nullptr;
        for (QObject* child : target->children()) {
            if (auto* hub = dynamic_cast<QMLPlusPropertyHub*>(child))
                return hub;
        }
        return create ? new QMLPlusPropertyHub(target) : nullptr;
    }

    bool isBlocked(const QByteArray& property) const { return !m_entries.value(property).blockers.isEmpty(); }
    bool isWriting(const QByteArray& property) const { return m_entries.value(property).writing > 0; }
    bool canAcquire(const QByteArray& property, QObject* owner) const
    {
        const auto blockers = m_entries.value(property).blockers;
        return blockers.isEmpty() || (blockers.size() == 1 && blockers.contains(owner));
    }
    void acquire(QByteArray property, QObject* owner);
    void release(QByteArray property, QObject* owner);
    detail::QMLBinding* binding(const QByteArray& property) const;
    void install(QByteArray property, detail::QMLBinding* binding);
    void remove(QByteArray property, detail::QMLBinding* binding);
    void cancelAnimation(QByteArray property);
    bool write(QByteArray property,
        QVariant value,
        const PropertyAnimation* animation = nullptr,
        QMLPlusRuntime* observer = nullptr,
        bool useBehavior = true);
    bool writeRaw(QByteArray property, QVariant value);
    void setBehavior(const QByteArray& property, const PropertyAnimation* behavior)
    {
        Entry& entry = m_entries[property];
        entry.hasBehavior = behavior != nullptr;
        if (behavior)
            entry.behavior = *behavior;
    }

private:
    QPointer<QObject> m_target;
    QMap<QByteArray, Entry> m_entries;
    QMap<QObject*, QMetaObject::Connection> m_ownerConnections;
};

namespace detail {

class QMLDependencyWatcher : public QObject {
    Q_OBJECT
public:
    QMLDependencyWatcher(const QVector<BindingDependency>& dependencies, std::function<void()> notify, QObject* parent)
        : QObject(parent), m_notify(std::move(notify))
    {
        const QMetaMethod slot = metaObject()->method(metaObject()->indexOfSlot("changed()"));
        for (const BindingDependency& dependency : dependencies) {
            const QMetaProperty property = metaProperty(dependency.object, dependency.propertyName);
            QObject::connect(dependency.object, property.notifySignal(), this, slot);
            QObject::connect(dependency.object, &QObject::destroyed, this, [this] {
                // Sibling controls may already be gone while a parent deletes
                // children. Defer state signals until teardown has completed;
                // deleting this watcher cancels the queued reevaluation.
                // zh_CN: 父对象删除子对象时，兄弟控件可能已经销毁。延后状态通知，
                // 待析构完成后再求值；观察器销毁会取消排队任务。
                QTimer::singleShot(0, this, [this] { changed(); });
            });
        }
    }

public slots:
    void changed()
    {
        const auto notify = m_notify;
        if (notify)
            notify();
    }

private:
    std::function<void()> m_notify;
};

class QMLBinding : public QObject {
    Q_OBJECT
public:
    QMLBinding(QObject* target,
        QByteArray property,
        QVector<BindingDependency> dependencies,
        std::function<QVariant()> evaluate,
        BindingOptions options,
        bool requireNotify = true)
        : QObject(target), m_target(target), m_property(std::move(property)), m_dependencies(std::move(dependencies)),
          m_evaluate(std::move(evaluate)), m_options(options), m_hub(QMLPlusPropertyHub::get(target)),
          m_requireNotify(requireNotify)
    {}

    void connectInputs()
    {
        const QMetaMethod slot = metaObject()->method(metaObject()->indexOfSlot("dependencyChanged()"));
        for (const BindingDependency& dependency : std::as_const(m_dependencies)) {
            const QMetaProperty property = metaProperty(dependency.object, dependency.propertyName);
            if (property.hasNotifySignal())
                QObject::connect(dependency.object, property.notifySignal(), this, slot);
            QObject::connect(dependency.object, &QObject::destroyed, this, [this] { invalidate(); });
        }
    }

    void setReverse(QObject* source, const QByteArray& property)
    {
        m_reverseTarget = source;
        m_reverseProperty = property;
        const QMetaProperty targetProperty = metaProperty(m_target, m_property);
        const QMetaMethod slot = metaObject()->method(metaObject()->indexOfSlot("reverseChanged()"));
        if (targetProperty.hasNotifySignal())
            QObject::connect(m_target, targetProperty.notifySignal(), this, slot);
    }

    bool isValid() const
    {
        if (!m_valid || !validThread(m_target) || !m_hub || m_hub->binding(m_property) != this)
            return false;
        for (const auto& dependency : m_dependencies) {
            if (!validThread(dependency.object))
                return false;
        }
        return true;
    }
    bool isEnabled() const { return isValid() && m_options.enabled; }
    bool isSuspended() const { return isValid() && (m_paused || m_hub->isBlocked(m_property)); }
    bool isActive() const { return isEnabled() && !isSuspended(); }
    void setEnabled(bool enabled)
    {
        if (!isValid() || m_options.enabled == enabled)
            return;
        m_options.enabled = enabled;
        if (enabled)
            refresh();
        else
            m_hub->cancelAnimation(m_property);
    }
    void setPaused(bool paused)
    {
        if (!isValid() || m_paused == paused)
            return;
        m_paused = paused;
        if (!paused)
            refresh();
        else
            m_hub->cancelAnimation(m_property);
    }
    void invalidate()
    {
        if (!m_valid)
            return;
        QPointer<QMLBinding> guard(this);
        m_valid = false;
        if (m_hub)
            m_hub->remove(m_property, this);
        if (guard)
            deleteLater();
    }
    void
    refresh(const PropertyAnimation* animation = nullptr, QMLPlusRuntime* observer = nullptr, bool immediate = false)
    {
        if (!isActive())
            return;
        if (m_evaluating) {
            m_requested = true;
            return;
        }
        QPointer<QMLBinding> guard(this);
        m_evaluating = true;
        int iterations = 0;
        do {
            m_requested = false;
            if (!validDependencies(m_dependencies, m_requireNotify)) {
                invalidate();
                return;
            }
            QVariant value;
            try {
                const auto evaluate = m_evaluate;
                value = evaluate();
            } catch (...) {
                if (guard)
                    m_evaluating = false;
                qCWarning(logging::layoutCategory) << "QMLPlus binding evaluator threw an exception";
                return;
            }
            if (!guard)
                return;
            if (!isActive())
                break;
            const bool initial = !m_synchronized || immediate;
            m_synchronized = true;
            if (value.isValid())
                m_hub->write(m_property, value, initial ? nullptr : animation, initial ? nullptr : observer, !initial);
            if (!guard)
                return;
            if (++iterations >= 32) {
                qCWarning(logging::layoutCategory) << "QMLPlus stopped a cyclic computed binding";
                break;
            }
        } while (m_requested && isActive());
        m_evaluating = false;
    }

public slots:
    void dependencyChanged()
    {
        if (!isActive())
            return;
        if (m_evaluating) {
            m_requested = true;
        } else if (m_options.deferred) {
            if (m_queued)
                return;
            m_queued = true;
            QTimer::singleShot(0, this, [this] {
                m_queued = false;
                refresh();
            });
        } else {
            refresh();
        }
    }
    void reverseChanged()
    {
        if (!isActive() || !m_reverseTarget || m_evaluating || m_hub->isWriting(m_property))
            return;
        QPointer<QMLBinding> guard(this);
        m_hub->cancelAnimation(m_property);
        if (!guard || !isActive())
            return;
        const QVariant value = m_target->property(m_property.constData());
        if (!guard || !isActive() || !m_reverseTarget)
            return;
        m_evaluating = true;
        m_reverseTarget->setProperty(m_reverseProperty.constData(), value);
        if (!guard)
            return;
        m_evaluating = false;
        refresh();
    }

private:
    QPointer<QObject> m_target;
    QByteArray m_property;
    QVector<BindingDependency> m_dependencies;
    std::function<QVariant()> m_evaluate;
    BindingOptions m_options;
    QPointer<QMLPlusPropertyHub> m_hub;
    QPointer<QObject> m_reverseTarget;
    QByteArray m_reverseProperty;
    bool m_requireNotify = true;
    bool m_valid = true;
    bool m_paused = false;
    bool m_evaluating = false;
    bool m_requested = false;
    bool m_queued = false;
    bool m_synchronized = false;
};

} // namespace detail

void QMLPlusPropertyHub::acquire(QByteArray property, QObject* owner)
{
    m_entries[property].blockers.insert(owner);
    if (!m_ownerConnections.contains(owner)) {
        m_ownerConnections.insert(owner, QObject::connect(owner, &QObject::destroyed, this, [this, owner] {
            QPointer<QMLPlusPropertyHub> guard(this);
            const auto properties = m_entries.keys();
            m_ownerConnections.remove(owner);
            for (const QByteArray& property : properties) {
                const bool removed = m_entries[property].blockers.remove(owner);
                if (removed && !isBlocked(property)) {
                    if (auto* link = binding(property))
                        link->refresh();
                }
                if (!guard)
                    return;
            }
        }));
    }
    cancelAnimation(property);
}

void QMLPlusPropertyHub::release(QByteArray property, QObject* owner)
{
    m_entries[property].blockers.remove(owner);
}

detail::QMLBinding* QMLPlusPropertyHub::binding(const QByteArray& property) const
{
    return m_entries.value(property).binding;
}

void QMLPlusPropertyHub::install(QByteArray property, detail::QMLBinding* binding)
{
    const QPointer<QMLPlusPropertyHub> guard(this);
    const QPointer<detail::QMLBinding> bindingGuard(binding);
    const QPointer<detail::QMLBinding> previous(this->binding(property));
    // Publish before cancellation. A reentrant registration can then retire
    // this candidate, and the most recent registration remains authoritative.
    // zh_CN: 取消前先发布候选绑定；重入注册可使其失效，并保留最近一次注册。
    m_entries[property].binding = binding;
    if (previous)
        previous->invalidate();
    if (guard && bindingGuard && this->binding(property) == bindingGuard)
        cancelAnimation(property);
}

void QMLPlusPropertyHub::remove(QByteArray property, detail::QMLBinding* binding)
{
    if (m_entries[property].binding == binding) {
        m_entries[property].binding.clear();
        cancelAnimation(property);
    }
}

void QMLPlusPropertyHub::cancelAnimation(QByteArray property)
{
    const QPointer<QVariantAnimation> animation = m_entries[property].animation;
    const QPointer<QMLPlusRuntime> observer = m_entries[property].observer;
    m_entries[property].animation.clear();
    m_entries[property].observer.clear();
    if (animation) {
        QVariantAnimation* identity = animation.data();
        animation->stop();
        if (animation)
            animation->deleteLater();
        if (observer)
            observer->animationEnded(identity, false);
    }
}

bool QMLPlusPropertyHub::writeRaw(QByteArray property, QVariant value)
{
    if (!m_target)
        return false;
    QPointer<QMLPlusPropertyHub> guard(this);
    const bool dynamic = !metaProperty(m_target, property).isValid();
    ++m_entries[property].writing;
    const bool success = m_target->setProperty(property.constData(), value);
    if (guard)
        --m_entries[property].writing;
    return success || dynamic;
}

bool QMLPlusPropertyHub::write(QByteArray property,
    QVariant value,
    const PropertyAnimation* animation,
    QMLPlusRuntime* observer,
    bool useBehavior)
{
    if (!m_target)
        return false;
    QPointer<QMLPlusPropertyHub> guard(this);
    QPointer<QMLPlusRuntime> observerGuard(observer);
    PropertyAnimation selected;
    bool animate = animation != nullptr;
    if (animation)
        selected = *animation;
    else if (useBehavior && m_entries[property].hasBehavior) {
        selected = m_entries[property].behavior;
        animate = true;
    }
    cancelAnimation(property);
    if (!guard)
        return false;
    QVariant current = m_target->property(property.constData());
    if (!guard)
        return false;
    const QMetaProperty targetProperty = metaProperty(m_target, property);
    if (targetProperty.isValid() && value.userType() != targetProperty.userType()) {
        ::compatibility::detail::convertVariantToType(value, targetProperty.userType());
    }
    if (current == value)
        return true;
    if (!animate || !current.isValid() || current.userType() != value.userType() || !interpolates(current.userType()))
        return writeRaw(property, value);

    auto* movement = new QVariantAnimation(this);
    movement->setStartValue(current);
    movement->setEndValue(value);
    movement->setEasingCurve(selected.easing);
    m_entries[property].animation = movement;
    m_entries[property].observer = observerGuard;
    QObject::connect(movement, &QVariantAnimation::valueChanged, this,
        [this, property, movement](const QVariant& frame) {
            if (m_entries[property].animation == movement)
                writeRaw(property, frame);
        });
    QObject::connect(movement, &QVariantAnimation::finished, this, [this, property, movement] {
        if (m_entries[property].animation == movement) {
            m_entries[property].animation.clear();
            m_entries[property].observer.clear();
        }
        movement->deleteLater();
    });
    QPointer<QVariantAnimation> movementGuard(movement);
    if (observerGuard)
        observerGuard->watchAnimation(movement);
    if (!guard || !movementGuard || (observer && !observerGuard))
        return false;
    detail::startMotionTransition(movement, qMax(0, selected.duration), selected.enabled);
    return true;
}

BindingHandle::BindingHandle(detail::QMLBinding* binding) : m_binding(binding) {}

bool BindingHandle::isValid() const
{
    return m_binding && m_binding->isValid();
}
bool BindingHandle::isEnabled() const
{
    return m_binding && m_binding->isEnabled();
}
bool BindingHandle::isSuspended() const
{
    return m_binding && m_binding->isSuspended();
}
void BindingHandle::setEnabled(bool enabled) const
{
    if (m_binding)
        m_binding->setEnabled(enabled);
}
void BindingHandle::pause() const
{
    if (m_binding)
        m_binding->setPaused(true);
}
void BindingHandle::resume() const
{
    if (m_binding)
        m_binding->setPaused(false);
}
void BindingHandle::refresh() const
{
    if (m_binding)
        m_binding->refresh();
}
void BindingHandle::unbind() const
{
    if (m_binding)
        m_binding->invalidate();
}

// Kept as a standalone compatibility helper. Managed bind() uses QMLBinding.
// zh_CN: 保留为独立兼容工具；受管理的 bind() 使用 QMLBinding。
PropertyLink::PropertyLink(QObject* from,
    const QMetaProperty& fromProp,
    QObject* to,
    const QMetaProperty& toProp,
    QObject* parent)
    : QObject(parent), m_from(from), m_fromProp(fromProp), m_to(to), m_toProp(toProp)
{}

void PropertyLink::syncToTarget()
{
    if (!m_from || !m_to)
        return;
    QPointer<PropertyLink> guard(this);
    const QVariant value = m_fromProp.read(m_from);
    if (!guard || !m_to)
        return;
    if (value.isValid()) {
        const QVariant previous = m_toProp.read(m_to);
        if (guard && m_to && previous != value)
            m_toProp.write(m_to, value);
    }
}

namespace {

detail::QMLBinding* installDirectBinding(QObject* source,
    const char* sourceProp,
    QObject* target,
    const char* targetProp,
    PropertyBinder::Direction direction,
    BindingOptions options,
    bool requireNotify)
{
    if (!sourceProp || !targetProp)
        return {};
    const QMetaProperty input = metaProperty(source, sourceProp);
    const QMetaProperty output = metaProperty(target, targetProp);
    if (!validThread(source) || !validThread(target) || !input.isReadable() || !output.isReadable()
        || !output.isWritable() || (requireNotify && !input.hasNotifySignal())
        || (direction == PropertyBinder::TwoWay
            && (!input.isWritable() || (requireNotify && !output.hasNotifySignal())))) {
        qCWarning(logging::layoutCategory) << "QMLPlus rejected an invalid direct binding";
        return {};
    }
    const QPointer<QObject> sourceGuard(source);
    const QByteArray property(sourceProp);
    auto* binding = new detail::QMLBinding(
        target, targetProp, {{source, sourceProp}},
        [sourceGuard, property] { return sourceGuard ? sourceGuard->property(property.constData()) : QVariant(); },
        options, requireNotify);
    QPointer<detail::QMLBinding> guard(binding);
    QMLPlusPropertyHub::get(target)->install(targetProp, binding);
    if (!guard || !guard->isValid()) {
        if (guard)
            guard->invalidate();
        return {};
    }
    binding->connectInputs();
    if (direction == PropertyBinder::TwoWay)
        binding->setReverse(source, sourceProp);
    binding->refresh();
    return guard && guard->isValid() ? guard.data() : nullptr;
}

} // namespace

void PropertyBinder::
    bind(QObject* source, const char* sourceProp, QObject* target, const char* targetProp, Direction direction)
{
    installDirectBinding(source, sourceProp, target, targetProp, direction, {}, false);
}

BindingHandle PropertyBinder::bindManaged(QObject* source,
    const char* sourceProp,
    QObject* target,
    const char* targetProp,
    Direction direction,
    BindingOptions options)
{
    return BindingHandle(installDirectBinding(source, sourceProp, target, targetProp, direction, options, true));
}

BindingHandle PropertyBinder::bindComputed(QObject* target,
    const char* targetProp,
    const QVector<BindingDependency>& dependencies,
    std::function<QVariant()> evaluate,
    BindingOptions options)
{
    if (!targetProp)
        return {};
    const QMetaProperty output = metaProperty(target, targetProp);
    if (!validThread(target) || !output.isReadable() || !output.isWritable() || !evaluate
        || !validDependencies(dependencies)) {
        qCWarning(logging::layoutCategory) << "QMLPlus rejected an invalid computed binding";
        return {};
    }
    auto* binding = new detail::QMLBinding(target, targetProp, dependencies, std::move(evaluate), options);
    QPointer<detail::QMLBinding> guard(binding);
    QMLPlusPropertyHub::get(target)->install(targetProp, binding);
    if (!guard || !guard->isValid()) {
        if (guard)
            guard->invalidate();
        return {};
    }
    binding->connectInputs();
    binding->refresh();
    return guard && guard->isValid() ? BindingHandle(binding) : BindingHandle();
}

struct QMLPlusRuntime::Data {
    struct PropertySnapshot {
        QPointer<QObject> target;
        QByteArray property;
        QVariant value;
    };
    struct AnchorSnapshot {
        QPointer<QWidget> target;
        AnchorLayout::Anchors anchors;
    };
    QPointer<QObject> host;
    QString current;
    QString pending;
    bool changing = false;
    bool hasPending = false;
    bool pendingForce = false;
    bool reevaluating = false;
    bool reevaluatePending = false;
    bool applying = false;
    bool running = false;
    bool canceled = false;
    bool completionPending = false;
    bool tearingDown = false;
    quint64 epoch = 0;
    QMap<QString, QMLState> states;
    QVector<QString> order;
    QVector<QPointer<detail::QMLDependencyWatcher>> watchers;
    QVector<PropertySnapshot> properties;
    QVector<AnchorSnapshot> anchors;
    QVector<StateTransition> transitions;
    QSet<QVariantAnimation*> animations;

    bool flatten(const QString& name, QMLState& result) const
    {
        result.name = name;
        QVector<QMLState> chain;
        QSet<QString> visited;
        QString currentName = name;
        while (!currentName.isEmpty()) {
            if (visited.contains(currentName) || !states.contains(currentName))
                return false;
            visited.insert(currentName);
            const QMLState state = states.value(currentName);
            chain.prepend(state);
            currentName = state.extend;
        }
        for (const QMLState& state : chain) {
            for (const PropertyChange& change : state.changes) {
                for (int index = result.changes.size() - 1; index >= 0; --index) {
                    if (result.changes[index].target == change.target
                        && result.changes[index].propertyName == change.propertyName)
                        result.changes.removeAt(index);
                }
                result.changes.append(change);
            }
            for (const AnchorChange& change : state.anchorChanges) {
                for (int index = result.anchorChanges.size() - 1; index >= 0; --index) {
                    if (result.anchorChanges[index].target == change.target)
                        result.anchorChanges.removeAt(index);
                }
                result.anchorChanges.append(change);
            }
        }
        return true;
    }

    const PropertyAnimation* animation(const QString& from, const QString& to, const QByteArray& property) const
    {
        const PropertyAnimation* selected = nullptr;
        int best = -1;
        for (const StateTransition& transition : transitions) {
            if ((transition.from != QStringLiteral("*") && transition.from != from)
                || (transition.to != QStringLiteral("*") && transition.to != to)
                || (!transition.properties.isEmpty() && !transition.properties.contains(property)))
                continue;
            const int score = int(transition.from != QStringLiteral("*")) + int(transition.to != QStringLiteral("*"));
            if (score > best) {
                selected = &transition.animation;
                best = score;
            }
        }
        return selected;
    }
};

QMLPlusRuntime::QMLPlusRuntime(QObject* host) : QObject(host), d(new Data)
{
    d->host = host;
}

QMLPlusRuntime::~QMLPlusRuntime()
{
    d->tearingDown = true;
    const auto watchers = d->watchers;
    d->watchers.clear();
    for (const auto& watcher : watchers)
        delete watcher.data();
    const auto properties = d->properties;
    const auto anchors = d->anchors;
    QVector<QPointer<QVariantAnimation>> animations;
    for (QVariantAnimation* animation : std::as_const(d->animations))
        animations.append(animation);
    const QPointer<QObject> host = d->host;
    for (const auto& animation : animations) {
        if (!animation)
            continue;
        QObject::disconnect(animation, nullptr, this, nullptr);
        animation->stop();
        if (animation)
            animation->deleteLater();
    }
    d->animations.clear();
    d->properties.clear();
    d->anchors.clear();
    for (const auto& snapshot : properties) {
        if (!snapshot.target)
            continue;
        QPointer<QMLPlusPropertyHub> hub(QMLPlusPropertyHub::get(snapshot.target));
        hub->release(snapshot.property, this);
        if (snapshot.target == host || hub->isBlocked(snapshot.property))
            continue;
        if (auto* binding = hub->binding(snapshot.property); binding && binding->isActive())
            binding->refresh(nullptr, nullptr, true);
        else
            hub->write(snapshot.property, snapshot.value, nullptr, nullptr, false);
    }
    for (const auto& snapshot : anchors) {
        if (snapshot.target && snapshot.target != host)
            replaceWidgetAnchors(snapshot.target, snapshot.anchors);
    }
    delete d;
}

QString QMLPlusRuntime::state() const
{
    return d->current;
}
bool QMLPlusRuntime::isTransitionRunning() const
{
    return d->running;
}

void QMLPlusRuntime::watchAnimation(QVariantAnimation* animation)
{
    if (d->tearingDown)
        return;
    d->animations.insert(animation);
    QObject::connect(animation, &QVariantAnimation::finished, this, [this, animation] {
        animationEnded(animation, true);
    });
    QObject::connect(animation, &QObject::destroyed, this, [this, animation] { animationEnded(animation, false); });
    if (!d->running) {
        d->running = true;
        emit transitionRunningChanged(true);
    }
}

void QMLPlusRuntime::animationEnded(QVariantAnimation* animation, bool completed)
{
    if (!d->animations.remove(animation) || d->tearingDown)
        return;
    if (!completed)
        d->canceled = true;
    if (!d->animations.isEmpty() || d->applying)
        return;
    QPointer<QMLPlusRuntime> guard(this);
    const quint64 epoch = d->epoch;
    if (d->running) {
        d->running = false;
        emit transitionRunningChanged(false);
        if (!guard)
            return;
    }
    if (d->epoch != epoch || d->running || !d->animations.isEmpty() || d->applying)
        return;
    if (!d->canceled && d->completionPending) {
        d->completionPending = false;
        const QString settled = d->current;
        emit transitionFinished(settled);
    }
}

void QMLPlusRuntime::evaluateConditions()
{
    if (d->tearingDown)
        return;
    if (d->reevaluating) {
        d->reevaluatePending = true;
        return;
    }
    QPointer<QMLPlusRuntime> guard(this);
    d->reevaluating = true;
    int rounds = 0;
    do {
        d->reevaluatePending = false;
        QString selected;
        const auto order = d->order;
        for (const QString& name : order) {
            const QMLState definition = d->states.value(name);
            if (!definition.when || !validDependencies(definition.dependencies))
                continue;
            bool matches = false;
            try {
                matches = definition.when();
            } catch (...) {
                qCWarning(logging::layoutCategory) << "QMLPlus state condition threw an exception";
            }
            if (!guard)
                return;
            if (matches) {
                selected = name;
                break;
            }
        }
        requestState(selected);
        if (!guard)
            return;
    } while (d->reevaluatePending && ++rounds < 16);
    d->reevaluating = false;
}

void QMLPlusRuntime::requestState(const QString& name, bool force)
{
    if (d->tearingDown)
        return;
    if (!name.isEmpty() && !d->states.contains(name))
        return;
    if (d->changing) {
        d->pending = name;
        d->hasPending = true;
        d->pendingForce = force;
        return;
    }
    if (d->current == name && !force)
        return;
    QPointer<QMLPlusRuntime> guard(this);
    d->changing = true;
    QString requested = name;
    int rounds = 0;
    while (d->current != requested || force) {
        force = false;
        QMLState destination;
        if (!d->flatten(requested, destination)) {
            qCWarning(logging::layoutCategory) << "QMLPlus rejected missing or cyclic state inheritance";
            break;
        }
        bool available = true;
        for (const auto& change : std::as_const(destination.changes)) {
            if (change.target && !change.propertyName.isEmpty()
                && !QMLPlusPropertyHub::get(change.target)->canAcquire(change.propertyName, this)) {
                available = false;
                break;
            }
        }
        if (!available) {
            qCWarning(logging::layoutCategory) << "QMLPlus property is already owned by another state controller";
            break;
        }
        const QString previous = d->current;
        ++d->epoch;
        d->hasPending = false;
        QVector<QPointer<QVariantAnimation>> animations;
        for (QVariantAnimation* animation : std::as_const(d->animations))
            animations.append(animation);
        d->animations.clear();
        d->canceled = false;
        d->completionPending = true;
        for (const auto& animation : animations) {
            if (!animation)
                continue;
            animation->stop();
            if (!guard)
                return;
            if (animation)
                animation->deleteLater();
        }
        if (d->running) {
            d->running = false;
            emit transitionRunningChanged(false);
            if (!guard)
                return;
        }
        d->current = requested;
        d->applying = true;

        // Acquire the full destination before writing any value, so reentrant
        // notifications cannot leak a state assignment through a TwoWay link.
        // zh_CN: 写入前先取得全部目标属性，防止重入通知经双向绑定泄漏状态赋值。
        for (const PropertyChange& change : std::as_const(destination.changes)) {
            if (!change.target || change.propertyName.isEmpty())
                continue;
            bool remembered = false;
            for (const auto& snapshot : std::as_const(d->properties)) {
                if (snapshot.target == change.target && snapshot.property == change.propertyName) {
                    remembered = true;
                    break;
                }
            }
            QPointer<QObject> target = change.target;
            if (!remembered) {
                const QVariant baseline = target->property(change.propertyName.constData());
                if (!guard)
                    return;
                if (!target)
                    continue;
                d->properties.append({target, change.propertyName, baseline});
            }
            QMLPlusPropertyHub::get(target)->acquire(change.propertyName, this);
            if (!guard)
                return;
        }

        const auto snapshots = d->properties;
        for (const auto& snapshot : snapshots) {
            bool retained = false;
            for (const auto& change : std::as_const(destination.changes)) {
                if (change.target == snapshot.target && change.propertyName == snapshot.property) {
                    retained = true;
                    break;
                }
            }
            if (retained)
                continue;
            for (int index = d->properties.size() - 1; index >= 0; --index) {
                if (d->properties[index].target == snapshot.target
                    && d->properties[index].property == snapshot.property)
                    d->properties.removeAt(index);
            }
            if (!snapshot.target)
                continue;
            QPointer<QMLPlusPropertyHub> hub(QMLPlusPropertyHub::get(snapshot.target));
            hub->release(snapshot.property, this);
            const PropertyAnimation* selected = d->animation(previous, requested, snapshot.property);
            const PropertyAnimation animation = selected ? *selected : PropertyAnimation();
            if (auto* binding = hub->binding(snapshot.property); binding && binding->isActive())
                binding->refresh(selected ? &animation : nullptr, this);
            else
                hub->write(snapshot.property, snapshot.value, selected ? &animation : nullptr, this);
            if (!guard)
                return;
        }
        for (const auto& change : std::as_const(destination.changes)) {
            if (!change.target || change.propertyName.isEmpty())
                continue;
            const PropertyAnimation* selected = d->animation(previous, requested, change.propertyName);
            const PropertyAnimation animation = selected ? *selected : PropertyAnimation();
            QMLPlusPropertyHub::get(change.target)
                ->write(change.propertyName, change.value, selected ? &animation : nullptr, this);
            if (!guard)
                return;
        }

        const auto anchorSnapshots = d->anchors;
        for (const auto& snapshot : anchorSnapshots) {
            bool retained = false;
            for (const auto& change : std::as_const(destination.anchorChanges)) {
                if (change.target == snapshot.target) {
                    retained = true;
                    break;
                }
            }
            if (retained)
                continue;
            for (int index = d->anchors.size() - 1; index >= 0; --index) {
                if (d->anchors[index].target == snapshot.target)
                    d->anchors.removeAt(index);
            }
            if (snapshot.target)
                replaceWidgetAnchors(snapshot.target, snapshot.anchors);
            if (!guard)
                return;
        }
        for (const auto& change : std::as_const(destination.anchorChanges)) {
            if (!change.target)
                continue;
            bool remembered = false;
            for (const auto& snapshot : std::as_const(d->anchors)) {
                if (snapshot.target == change.target) {
                    remembered = true;
                    break;
                }
            }
            if (!remembered)
                d->anchors.append({change.target, widgetAnchors(change.target)});
            replaceWidgetAnchors(change.target, change.anchors);
            if (!guard)
                return;
        }
        d->applying = false;
        if (previous != d->current) {
            const QString settled = d->current;
            emit stateChanged(settled);
        }
        if (!guard)
            return;
        if (d->animations.isEmpty()) {
            if (d->running) {
                d->running = false;
                emit transitionRunningChanged(false);
                if (!guard)
                    return;
            }
            if (!d->canceled && d->completionPending && !d->hasPending) {
                d->completionPending = false;
                const QString finishedState = d->current;
                emit transitionFinished(finishedState);
                if (!guard)
                    return;
            }
        }
        if (!d->hasPending)
            break;
        requested = d->pending;
        force = d->pendingForce;
        d->hasPending = false;
        if (++rounds >= qMax(8, d->states.size() * 2 + 2)) {
            qCWarning(logging::layoutCategory) << "QMLPlus stopped cyclic reentrant state changes";
            break;
        }
    }
    d->changing = false;
    d->hasPending = false;
}

QMLPlus::QMLPlus() = default;

QMLPlus::~QMLPlus()
{
    delete m_runtime.data();
    delete m_anchors;
}

AnchorLayout::Anchors* QMLPlus::anchors()
{
    if (!m_anchors)
        m_anchors = new AnchorLayout::Anchors;
    return m_anchors;
}

void QMLPlus::setAnchors(const AnchorLayout::Anchors& value)
{
    *anchors() = value;
    invalidateAnchors(dynamic_cast<QWidget*>(this));
}

void QMLPlus::resetAnchors()
{
    setAnchors({});
}

QMLPlusRuntime* QMLPlus::runtime()
{
    if (!m_runtime) {
        if (auto* host = dynamic_cast<QObject*>(this))
            m_runtime = new QMLPlusRuntime(host);
    }
    return m_runtime;
}

QString QMLPlus::state() const
{
    return m_runtime ? m_runtime->state() : QString();
}

void QMLPlus::setState(const QString& name)
{
    if (auto* observer = runtime())
        observer->requestState(name);
}

void QMLPlus::applyState(const QString& name)
{
    setState(name);
}

void QMLPlus::addState(const QMLState& definition)
{
    auto* observer = runtime();
    if (!observer || definition.name.isEmpty()
        || (!definition.dependencies.isEmpty() && !validDependencies(definition.dependencies)))
        return;
    QPointer<QMLPlusRuntime> guard(observer);
    auto* data = observer->d;
    const bool activeReplacement = data->current == definition.name;
    const QMLState previousDefinition = data->states.value(definition.name);
    if (!data->states.contains(definition.name))
        data->order.append(definition.name);
    data->states[definition.name] = definition;
    if (activeReplacement) {
        QMLState flattened;
        if (!data->flatten(definition.name, flattened)) {
            data->states[definition.name] = previousDefinition;
            qCWarning(logging::layoutCategory) << "QMLPlus rejected invalid inheritance in an active state replacement";
            return;
        }
    }
    for (const auto& watcher : std::as_const(data->watchers)) {
        if (watcher)
            delete watcher.data();
    }
    data->watchers.clear();
    for (const QString& name : std::as_const(data->order)) {
        const auto dependencies = data->states.value(name).dependencies;
        if (dependencies.isEmpty() || !validDependencies(dependencies))
            continue;
        data->watchers.append(new detail::QMLDependencyWatcher(
            dependencies,
            [guard] {
                if (guard)
                    guard->evaluateConditions();
            },
            observer));
    }
    if (activeReplacement) {
        // A replacement must reapply even if its public name is unchanged.
        // zh_CN: 即使公开状态名称未改变，也需要重新应用替换后的定义。
        observer->requestState(data->current, true);
        if (!guard)
            return;
    }
    if (definition.when)
        observer->evaluateConditions();
}

void QMLPlus::evaluateStates()
{
    if (auto* observer = runtime())
        observer->evaluateConditions();
}

void QMLPlus::bind(const char* targetProp, QObject* source, const char* sourceProp, PropertyBinder::Direction direction)
{
    PropertyBinder::bind(source, sourceProp, dynamic_cast<QWidget*>(this), targetProp, direction);
}

BindingHandle QMLPlus::bindManaged(const char* targetProp,
    QObject* source,
    const char* sourceProp,
    PropertyBinder::Direction direction,
    BindingOptions options)
{
    return PropertyBinder::bindManaged(source, sourceProp, dynamic_cast<QWidget*>(this), targetProp, direction,
        options);
}

BindingHandle QMLPlus::bindComputed(const char* targetProp,
    const QVector<BindingDependency>& dependencies,
    std::function<QVariant()> evaluate,
    BindingOptions options)
{
    return PropertyBinder::bindComputed(dynamic_cast<QWidget*>(this), targetProp, dependencies, std::move(evaluate),
        options);
}

void QMLPlus::addTransition(const StateTransition& transition)
{
    if (auto* observer = runtime())
        observer->d->transitions.append(transition);
}

void QMLPlus::setBehavior(const char* propertyName, const PropertyAnimation& animation)
{
    auto* host = dynamic_cast<QWidget*>(this);
    const QMetaProperty property = propertyName ? metaProperty(host, propertyName) : QMetaProperty();
    if (host && property.isReadable() && property.isWritable())
        QMLPlusPropertyHub::get(host)->setBehavior(propertyName, &animation);
}

void QMLPlus::clearBehavior(const char* propertyName)
{
    if (propertyName) {
        if (auto* hub = QMLPlusPropertyHub::get(dynamic_cast<QWidget*>(this), false))
            hub->setBehavior(propertyName, nullptr);
    }
}

bool QMLPlus::writeProperty(const char* propertyName, const QVariant& value)
{
    auto* host = dynamic_cast<QWidget*>(this);
    const QMetaProperty property = propertyName ? metaProperty(host, propertyName) : QMetaProperty();
    if (!host || !property.isReadable() || !property.isWritable())
        return false;
    return QMLPlusPropertyHub::get(host)->write(propertyName, value);
}

} // namespace fluent

#include "QMLPlus.moc"
