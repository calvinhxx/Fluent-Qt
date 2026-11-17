#ifndef FLUENTQT_COMPONENTS_COLLECTIONS_TIMELINE_H
#define FLUENTQT_COMPONENTS_COLLECTIONS_TIMELINE_H

#include <memory>
#include "components/collections/ListView.h"

namespace fluent::collections {
class TimelineDelegate;
class TimelineAccessible;
/**
 * @brief Model-backed timeline view built on ListView with Fluent scrolling and theming.
 * Uses an external model without taking ownership; ordering, timestamps, and node status
 * come from the model (DisplayRole for title, DecorationRole for optional QIcon), with no I/O
 * or per-row widgets. Replace the inherited item delegate for custom row layouts.
 * zh_CN: 基于 ListView 的模型驱动时间线视图，复用 Fluent 滚动与主题。
 * 关联外部模型（不接管所有权），条目顺序、时间戳和状态均由模型提供（DisplayRole 为标题，
 * DecorationRole 为可选图标），不执行 I/O 也不逐行创建控件；如需自定义行排版可替换代理。
 */
class Timeline : public ListView {
    Q_OBJECT
    Q_PROPERTY(NodeAlignment nodeAlignment READ nodeAlignment WRITE setNodeAlignment NOTIFY nodeAlignmentChanged)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
public:
    /**
     * @brief Side on which the timeline rail and nodes are placed.
     * zh_CN: 时间线轨道与节点的对齐侧。
     */
    enum NodeAlignment { Leading, Trailing, Alternate };
    Q_ENUM(NodeAlignment)
    /**
     * @brief Visual status tone of a timeline node.
     * zh_CN: 时间线节点的状态配色类型。
     */
    enum Status { Neutral, Active, Success, Warning, Error };
    Q_ENUM(Status)
    /**
     * @brief Optional roles for description text, timestamp label, and node status.
     * Text roles are displayed as formatted by the model; StatusRole defaults to Neutral.
     * Qt::AccessibleTextRole and Qt::AccessibleDescriptionRole override generated accessibility text.
     * zh_CN: 可选的描述、时间戳和节点状态角色。
     * 文本由模型自行格式化，StatusRole 默认为 Neutral；可通过 Qt::AccessibleTextRole 和
     * AccessibleDescriptionRole 覆盖默认无障碍文本。
     */
    enum DataRole { DescriptionRole = Qt::UserRole + 1, TimestampRole, StatusRole };
    Q_ENUM(DataRole)
    explicit Timeline(QWidget* parent = nullptr);
    ~Timeline() override;
    /**
     * @brief Associates an external model without taking ownership or modifying its data.
     * zh_CN: 关联外部数据模型，不接管所有权也不修改其数据。
     */
    void setModel(QAbstractItemModel* model) override;
    NodeAlignment nodeAlignment() const;
    void setNodeAlignment(NodeAlignment alignment);
    bool isAnimationEnabled() const;
    void setAnimationEnabled(bool enabled);
signals:
    void nodeAlignmentChanged(NodeAlignment alignment);
    void animationEnabledChanged(bool enabled);

protected:
    void dataChanged(const QModelIndex& topLeft,
        const QModelIndex& bottomRight,
        const QVector<int>& roles = QVector<int>()) override;
    void resizeEvent(QResizeEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void changeEvent(QEvent* event) override;
    void onThemeUpdated() override;

private:
    friend class TimelineDelegate;
    friend class TimelineAccessible;
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace fluent::collections
#endif
