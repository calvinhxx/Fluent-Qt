#ifndef FLUENTQT_COMPONENTS_COLLECTIONS_TIMELINE_H
#define FLUENTQT_COMPONENTS_COLLECTIONS_TIMELINE_H

#include <memory>
#include "components/collections/ListView.h"

namespace fluent::collections {
class TimelineDelegate;
class TimelineAccessible;
/**
 * @brief Model-backed timeline with reusable Fluent scrolling, input and theme behavior.
 * zh_CN: 基于模型的时间线，复用 Fluent 滚动、输入和主题行为。
 *
 * The caller owns the model and controls ordering, timestamps and status.
 * DisplayRole is the title; DecorationRole is an optional QIcon. No I/O,
 * sorting, workflow advancement or persistent widget per row is performed.
 * Replace the inherited item delegate for application-defined row content.
 * zh_CN: 模型及顺序、时间和状态由调用方控制。DisplayRole 为标题，DecorationRole
 * 为可选 QIcon；不执行 I/O、排序或流程推进，也不为每行创建常驻控件。可通过
 * 继承的 delegate 接口提供应用自定义的行内容。
 */
class Timeline : public ListView {
    Q_OBJECT
    Q_PROPERTY(NodeAlignment nodeAlignment READ nodeAlignment WRITE setNodeAlignment NOTIFY
                   nodeAlignmentChanged)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY
                   animationEnabledChanged)
public:
    /**
     * @brief Logical placement of the timeline rail.
     * zh_CN: 时间线轨道的逻辑位置。
     */
    enum NodeAlignment { Leading, Trailing, Alternate };
    Q_ENUM(NodeAlignment)
    /**
     * @brief Node presentation independent of selection.
     * zh_CN: 独立于选中状态的节点外观。
     */
    enum Status { Neutral, Active, Success, Warning, Error };
    Q_ENUM(Status)
    /**
     * @brief Optional description, timestamp label and status roles.
     * zh_CN: 可选说明、时间标签和状态角色。
     * Strings are caller-formatted; StatusRole defaults to Neutral. Standard
     * AccessibleTextRole and AccessibleDescriptionRole override semantic text.
     * zh_CN: 字符串由调用方格式化，StatusRole 默认 Neutral；标准无障碍文字角色
     * 可覆盖语义文字。
     */
    enum DataRole { DescriptionRole = Qt::UserRole + 1, TimestampRole, StatusRole };
    Q_ENUM(DataRole)
    explicit Timeline(QWidget* parent = nullptr);
    ~Timeline() override;
    /**
     * @brief Borrows a model without changing its parent or data.
     * zh_CN: 借用模型，不改变其父对象或数据。
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
    void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight,
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
