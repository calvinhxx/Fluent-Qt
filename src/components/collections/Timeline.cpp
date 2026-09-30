#include "Timeline.h"

#include <algorithm>
#include <QAbstractItemModel>
#include <QAccessible>
#include <QHideEvent>
#include <QIcon>
#include <QPainter>
#include <QPointer>
#include <QResizeEvent>
#include <QStyledItemDelegate>
#include <QVariantAnimation>
#include "components/collections/private/CollectionItemState_p.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/private/LogicalItemAccessibility_p.h"
#include "components/foundation/private/MotionPolicy_p.h"

namespace fluent::collections {
namespace {
Timeline::Status nodeStatus(const QModelIndex& index)
{
    const int state = index.data(Timeline::StatusRole).toInt();
    return state >= Timeline::Neutral && state <= Timeline::Error
               ? static_cast<Timeline::Status>(state)
               : Timeline::Neutral;
}
QColor targetColor(const Timeline* view, const QModelIndex& index)
{
    const auto& c = view->themeColorsRef();
    if (!view->isEnabled() || !(index.flags() & Qt::ItemIsEnabled))
        return c.controlDisabled;
    switch (nodeStatus(index)) {
    case Timeline::Active:
        return c.accentDefault;
    case Timeline::Success:
        return c.systemSuccess;
    case Timeline::Warning:
        return c.systemCaution;
    case Timeline::Error:
        return c.systemCritical;
    case Timeline::Neutral: {
        QColor color = c.strokeStrong;
        color.setAlpha(0);
        return color;
    }
    }
    return {};
}
struct RowGeometry {
    QRectF text;
    QPointF node;
    Qt::Alignment alignment = Qt::AlignLeft;
};
RowGeometry rowGeometry(const Timeline* view, const QRectF& rect, const QModelIndex& index)
{
    RowGeometry result;
    qreal rail = rect.left() + 20;
    bool trailing = view->nodeAlignment() == Timeline::Trailing;
    if (view->nodeAlignment() == Timeline::Alternate) {
        rail = rect.center().x();
        trailing = index.row() % 2 != 0;
        result.text = trailing ? QRectF(rect.left() + 12, rect.top() + 12,
                                        qMax(1.0, rail - rect.left() - 32), rect.height() - 24)
                               : QRectF(rail + 20, rect.top() + 12,
                                        qMax(1.0, rect.right() - rail - 32), rect.height() - 24);
    } else {
        if (trailing)
            rail = rect.right() - 20;
        result.text = QRectF(rect.left() + (trailing ? 12 : 40), rect.top() + 12,
                             qMax(1.0, rect.width() - 52), rect.height() - 24);
    }
    result.node = QPointF(
        rail, rect.top() + 12 +
                  qMax(20, QFontMetrics(view->themeFont(Typography::FontRole::BodyStrong).toQFont())
                               .height()) /
                      2.0);
    result.alignment = trailing ? Qt::AlignRight : Qt::AlignLeft;
    if (view->layoutDirection() == Qt::RightToLeft) {
        result.node.setX(rect.left() + rect.right() - result.node.x());
        result.text.moveLeft(rect.left() + rect.right() - result.text.right());
        result.alignment = trailing ? Qt::AlignLeft : Qt::AlignRight;
    }
    result.alignment |= Qt::AlignAbsolute;
    return result;
}
int wrappedTextFlags(const QString& text)
{
    return Qt::TextWordWrap |
           (text.isRightToLeft() ? Qt::TextForceRightToLeft : Qt::TextForceLeftToRight);
}
int textHeight(const QString& text, const QFont& font, int width)
{
    if (text.isEmpty())
        return 0;
    return QFontMetrics(font)
        .boundingRect(QRect(0, 0, qMax(1, width), 100000), wrappedTextFlags(text), text)
        .height();
}
QString rowDescription(const QModelIndex& index)
{
    if (index.data(Qt::AccessibleDescriptionRole).isValid())
        return index.data(Qt::AccessibleDescriptionRole).toString();
    QStringList parts;
    for (int role : {int(Timeline::TimestampRole), int(Timeline::DescriptionRole)}) {
        const auto value = index.data(role).toString();
        if (!value.isEmpty())
            parts.append(value);
    }
    switch (nodeStatus(index)) {
    case Timeline::Active:
        parts.append(Timeline::tr("Active"));
        break;
    case Timeline::Success:
        parts.append(Timeline::tr("Success"));
        break;
    case Timeline::Warning:
        parts.append(Timeline::tr("Warning"));
        break;
    case Timeline::Error:
        parts.append(Timeline::tr("Error"));
        break;
    case Timeline::Neutral:
        break;
    }
    return parts.join(QStringLiteral("; "));
}
} // namespace

class Timeline::Private {
public:
    explicit Private(Timeline* view) : q(view) {}
    Timeline* q;
    NodeAlignment alignment = Leading;
    bool animations = true;
    QHash<QPersistentModelIndex, QColor> colors;
    QHash<QPersistentModelIndex, QVariantAnimation*> motion;
    QVector<QMetaObject::Connection> modelConnections;
    void settle()
    {
        const auto active = motion.values();
        motion.clear();
        for (auto* animation : active) {
            animation->disconnect(q);
            animation->stop();
            delete animation;
        }
        colors.clear();
        if (q->viewport())
            q->viewport()->update();
    }
    QColor color(const QModelIndex& index)
    {
        const QPersistentModelIndex key(index);
        if (!colors.contains(key))
            colors.insert(key, targetColor(q, index));
        return colors.value(key);
    }
    void transition(const QPersistentModelIndex& key)
    {
        const QColor target = targetColor(q, key);
        if (!colors.contains(key) || colors.value(key) == target)
            return;
        if (auto* previous = motion.take(key)) {
            previous->disconnect(q);
            previous->stop();
            previous->deleteLater();
        }
        if (!animations || !q->isVisible() || !q->isEnabled()) {
            colors.insert(key, target);
            return;
        }
        auto* animation = new QVariantAnimation(q);
        animation->setObjectName(QStringLiteral("timelineMarkerTransition"));
        animation->setStartValue(colors.value(key));
        animation->setEndValue(target);
        animation->setEasingCurve(q->themeAnimation().decelerate);
        motion.insert(key, animation);
        QObject::connect(animation, &QVariantAnimation::valueChanged, q,
                         [this, key](const QVariant& value) {
                             if (!key.isValid())
                                 return;
                             colors.insert(key, value.value<QColor>());
                             q->viewport()->update(q->visualRect(key));
                         });
        QObject::connect(animation, &QVariantAnimation::finished, q, [this, key, animation] {
            motion.remove(key);
            animation->deleteLater();
        });
        fluent::detail::startMotionTransition(animation, q->themeAnimation().normal, animations);
    }
    void prune()
    {
        for (auto it = colors.begin(); it != colors.end();) {
            if (!it.key().isValid() || !q->visualRect(it.key()).intersects(q->viewport()->rect())) {
                if (auto* animation = motion.take(it.key())) {
                    animation->disconnect(q);
                    animation->stop();
                    animation->deleteLater();
                }
                it = colors.erase(it);
            } else
                ++it;
        }
    }
};

class TimelineDelegate final : public QStyledItemDelegate {
public:
    explicit TimelineDelegate(Timeline* view) : QStyledItemDelegate(view), q(view) {}
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        if (!q || !index.isValid())
            return;
        const auto& c = q->themeColorsRef();
        const auto geometry = rowGeometry(q, option.rect, index);
        const bool enabled = q->isEnabled() && (index.flags() & Qt::ItemIsEnabled);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const auto visual = detail::collectionItemVisualStyle(option.state, c);
        painter->setPen(Qt::NoPen);
        painter->setBrush(visual.background);
        painter->drawRoundedRect(QRectF(option.rect).adjusted(2, 1, -2, -1),
                                 q->themeRadius().control, q->themeRadius().control);
        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        const qreal radius = icon.isNull() ? 6.0 : 12.0;
        painter->setPen(QPen(c.strokeDivider, 2));
        if (index.row() > 0)
            painter->drawLine(QPointF(geometry.node.x(), option.rect.top()),
                              geometry.node - QPointF(0, radius + 4));
        if (index.row() + 1 < index.model()->rowCount(index.parent()))
            painter->drawLine(geometry.node + QPointF(0, radius + 4),
                              QPointF(geometry.node.x(), option.rect.bottom() + 1));
        const QColor fill = q->d->color(index);
        const QRectF marker(geometry.node - QPointF(radius, radius),
                            QSizeF(2 * radius, 2 * radius));
        // Fade the accent over the neutral stroke, retaining alpha throughout.
        // zh_CN: 将强调色叠加到中性描边上，全程保留 alpha，避免首帧闪黑。
        painter->setPen(QPen(c.strokeStrong, 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(marker);
        painter->setPen(QPen(fill, 2));
        painter->setBrush(fill);
        painter->drawEllipse(marker);
        if (!icon.isNull())
            icon.paint(painter, marker.adjusted(3, 3, -3, -3).toRect(), Qt::AlignCenter,
                       enabled ? QIcon::Normal : QIcon::Disabled);
        qreal y = geometry.text.top();
        const int width = qMax(1, int(geometry.text.width()));
        const auto titleFont = q->themeFont(Typography::FontRole::BodyStrong).toQFont();
        const auto captionFont = q->themeFont(Typography::FontRole::Caption).toQFont();
        auto draw = [&](const QString& text, const QFont& font, const QColor& color) {
            if (text.isEmpty())
                return;
            const int height = textHeight(text, font, width);
            painter->setFont(font);
            painter->setPen(enabled ? color : c.textDisabled);
            painter->drawText(QRectF(geometry.text.left(), y, width, height),
                              geometry.alignment | wrappedTextFlags(text), text);
            y += height + 4;
        };
        draw(index.data(Qt::DisplayRole).toString(), titleFont, c.textPrimary);
        draw(index.data(Timeline::TimestampRole).toString(), captionFont, c.textSecondary);
        draw(index.data(Timeline::DescriptionRole).toString(), captionFont, c.textSecondary);
        painter->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const override
    {
        const int width = qMax(96, q->viewport()->width());
        const int textWidth =
            qMax(1, int(rowGeometry(q, QRectF(0, 0, width, 64), index).text.width()));
        int height = 24;
        for (int role :
             {int(Qt::DisplayRole), int(Timeline::TimestampRole), int(Timeline::DescriptionRole)}) {
            const auto text = index.data(role).toString();
            if (!text.isEmpty())
                height += textHeight(text,
                                     q->themeFont(role == Qt::DisplayRole
                                                      ? Typography::FontRole::BodyStrong
                                                      : Typography::FontRole::Caption)
                                         .toQFont(),
                                     textWidth) +
                          4;
        }
        return QSize(width,
                     qMax(qMax(48, height - 4), index.data(Qt::SizeHintRole).toSize().height()));
    }

private:
    QPointer<Timeline> q;
};

#if QT_CONFIG(accessibility)
class TimelineAccessible final : public accessibility::detail::LogicalItemAccessibleAdapter {
public:
    explicit TimelineAccessible(Timeline* view)
        : LogicalItemAccessibleAdapter(view, QAccessible::List)
    {}
    Timeline* view() const { return qobject_cast<Timeline*>(widget()); }
    QModelIndex indexAt(int i) const
    {
        return view()->model()
                   ? view()->model()->index(i, view()->modelColumn(), view()->rootIndex())
                   : QModelIndex();
    }
    int logicalChildCount() const override
    {
        return view()->model() ? view()->model()->rowCount(view()->rootIndex()) : 0;
    }
    QAccessible::Role logicalChildRole(int) const override { return QAccessible::ListItem; }
    QString logicalChildText(int i, QAccessible::Text type) const override
    {
        const auto index = indexAt(i);
        if (type == QAccessible::Name)
            return index.data(Qt::AccessibleTextRole).isValid()
                       ? index.data(Qt::AccessibleTextRole).toString()
                       : index.data(Qt::DisplayRole).toString();
        return type == QAccessible::Description ? rowDescription(index) : QString();
    }
    QRect logicalChildRect(int i) const override
    {
        const auto rect = view()->viewport()->rect().intersected(view()->visualRect(indexAt(i)));
        return QRect(view()->viewport()->mapToGlobal(rect.topLeft()), rect.size());
    }
    accessibility::detail::LogicalItemAccessibleState logicalChildState(int i) const override
    {
        accessibility::detail::LogicalItemAccessibleState s;
        const auto index = indexAt(i);
        s.valid = index.isValid();
        s.enabled = view()->isEnabled() && (index.flags() & Qt::ItemIsEnabled);
        s.readOnly = true;
        s.selectable = false;
        s.focusable = s.enabled;
        s.focused = view()->hasFocus() && view()->currentIndex() == index;
        s.invisible = !view()->isVisible();
        s.offscreen = logicalChildRect(i).isEmpty();
        return s;
    }
    bool logicalSelectionSupported() const override { return false; }
    int logicalFocusChild() const override
    {
        return view()->hasFocus() && view()->currentIndex().isValid() ? view()->currentIndex().row()
                                                                      : -1;
    }
    QStringList logicalChildActions(int i) const override
    {
        return logicalChildState(i).enabled
                   ? QStringList{QAccessibleActionInterface::setFocusAction()}
                   : QStringList();
    }
    void performLogicalChildAction(int i, const QString& action) override
    {
        if (logicalChildState(i).enabled &&
            action == QAccessibleActionInterface::setFocusAction()) {
            view()->scrollTo(indexAt(i));
            view()->setCurrentIndex(indexAt(i));
            view()->setFocus();
        }
    }
};
#endif

Timeline::Timeline(QWidget* parent) : ListView(parent), d(new Private(this))
{
    setItemDelegate(new TimelineDelegate(this));
    setSelectionMode(SelectionMode::None);
    setSelectionIndicatorVisible(false);
    setBorderVisible(false);
    setBackgroundVisible(false);
    setUniformItemSizes(false);
    setLayoutMode(QListView::Batched);
    setBatchSize(64);
    setSpacing(0);
    setResizeMode(QListView::Adjust);
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory([](const QString&, QObject* object) -> QAccessibleInterface* {
            if (auto* view = qobject_cast<Timeline*>(object))
                return new TimelineAccessible(view);
            return nullptr;
        });
        return true;
    }();
    Q_UNUSED(installed)
#endif
    connect(&MotionPolicy::instance(), &MotionPolicy::modeChanged, this, [this] { d->settle(); });
}
Timeline::~Timeline()
{
    for (const auto& connection : d->modelConnections)
        disconnect(connection);
    d->settle();
}
void Timeline::setModel(QAbstractItemModel* model)
{
    for (const auto& connection : d->modelConnections)
        disconnect(connection);
    d->modelConnections.clear();
    d->settle();
    ListView::setModel(model);
    if (!model)
        return;
    auto structural = [this] {
        d->settle();
        accessibility::detail::notifyLogicalItemAccessibilityStructure(this);
    };
    d->modelConnections.append(connect(model, &QAbstractItemModel::rowsInserted, this, structural));
    d->modelConnections.append(connect(model, &QAbstractItemModel::rowsRemoved, this, structural));
    d->modelConnections.append(connect(model, &QAbstractItemModel::rowsMoved, this, structural));
    d->modelConnections.append(connect(model, &QAbstractItemModel::modelReset, this, structural));
    d->modelConnections.append(
        connect(model, &QAbstractItemModel::layoutChanged, this, structural));
    d->modelConnections.append(connect(model, &QObject::destroyed, this, structural));
    accessibility::detail::notifyLogicalItemAccessibilityStructure(this);
}
Timeline::NodeAlignment Timeline::nodeAlignment() const
{
    return d->alignment;
}
void Timeline::setNodeAlignment(NodeAlignment value)
{
    if (value < Leading || value > Alternate || d->alignment == value)
        return;
    d->alignment = value;
    d->settle();
    scheduleDelayedItemsLayout();
    updateGeometry();
    emit nodeAlignmentChanged(value);
}
bool Timeline::isAnimationEnabled() const
{
    return d->animations;
}
void Timeline::setAnimationEnabled(bool enabled)
{
    if (d->animations == enabled)
        return;
    d->animations = enabled;
    d->settle();
    emit animationEnabledChanged(enabled);
}
void Timeline::dataChanged(const QModelIndex& first, const QModelIndex& last,
                           const QVector<int>& roles)
{
    const bool presentationOnly =
        dynamic_cast<TimelineDelegate*>(itemDelegate()) && !roles.isEmpty() &&
        std::all_of(roles.cbegin(), roles.cend(), [](int role) {
            return role == StatusRole || role == Qt::DecorationRole ||
                   role == Qt::AccessibleTextRole || role == Qt::AccessibleDescriptionRole;
        });
    // The built-in marker and semantic roles do not change row geometry.
    // zh_CN: 内置节点和语义角色不改变行几何；自定义 delegate 保留常规重排路径。
    if (presentationOnly)
        QAbstractItemView::dataChanged(first, last, roles);
    else
        ListView::dataChanged(first, last, roles);
    if (!d)
        return;
    const auto keys = d->colors.keys();
    for (const auto& key : keys)
        if (key.isValid() && key.parent() == first.parent() && key.row() >= first.row() &&
            key.row() <= last.row())
            d->transition(key);
#if QT_CONFIG(accessibility)
    // One range refresh avoids materializing an accessible child for every row.
    // zh_CN: 一次范围刷新避免为每个变更行实例化无障碍子对象。
    QAccessibleEvent changed(this, QAccessible::VisibleDataChanged);
    QAccessible::updateAccessibility(&changed);
#endif
}
void Timeline::resizeEvent(QResizeEvent* e)
{
    ListView::resizeEvent(e);
    // QListView's Adjust mode already schedules width-dependent layout.
    // zh_CN: QListView 的 Adjust 模式已负责随宽度重排，仅回收不可见节点的状态。
    if (d)
        d->prune();
}
void Timeline::hideEvent(QHideEvent* e)
{
    d->settle();
    ListView::hideEvent(e);
}
void Timeline::scrollContentsBy(int dx, int dy)
{
    ListView::scrollContentsBy(dx, dy);
    if (d)
        d->prune();
}
void Timeline::changeEvent(QEvent* e)
{
    ListView::changeEvent(e);
    if (d && e->type() == QEvent::EnabledChange)
        d->settle();
}
void Timeline::onThemeUpdated()
{
    ListView::onThemeUpdated();
    if (d) {
        d->settle();
        scheduleDelayedItemsLayout();
    }
}
} // namespace fluent::collections
