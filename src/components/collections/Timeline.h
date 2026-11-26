#ifndef FLUENTQT_COMPONENTS_COLLECTIONS_TIMELINE_H
#define FLUENTQT_COMPONENTS_COLLECTIONS_TIMELINE_H

#include <memory>
#include "ListView.h"

namespace fluent::collections {
class TimelineDelegate;
class TimelineAccessible;
/**
 * @brief Model-backed timeline view displaying chronological event nodes and status.
 * Reads DisplayRole for titles, DecorationRole for optional icons, and DataRole for details.
 * zh_CN: 按时间顺序展示事件节点与状态的时间线视图，数据通过模型提供。
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
     * @brief Optional model roles for description text, timestamp label, and node status (defaults to Neutral).
     * zh_CN: 可选的模型角色：描述文本、时间戳和节点状态（StatusRole 默认为 Neutral）。
     */
    enum DataRole { DescriptionRole = Qt::UserRole + 1, TimestampRole, StatusRole };
    Q_ENUM(DataRole)
    explicit Timeline(QWidget* parent = nullptr);
    ~Timeline() override;
    /**
     * @brief Sets the timeline model (still managed by the caller).
     * zh_CN: 设置时间线数据模型（仍由应用管理，不会随视图销毁）。
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
