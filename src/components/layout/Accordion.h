#ifndef FLUENTQT_COMPONENTS_LAYOUT_ACCORDION_H
#define FLUENTQT_COMPONENTS_LAYOUT_ACCORDION_H

#include <QMetaObject>
#include <QMetaType>
#include <QPointer>
#include <QVector>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/WidgetOwnership.h"

class QEvent;
class QVBoxLayout;

namespace fluent::layout {

class Expander;

/**
 * @brief Vertical container of Expander items with coordinated expansion and keyboard navigation.
 * zh_CN: 纵向排列多个 Expander 并协调展开状态与键盘焦点的容器。
 *
 * By default items use WidgetOwnership::Borrowed (non-owning); pass WidgetOwnership::Reparented
 * or WidgetOwnership::Owned to restore or transfer item lifetime.
 * zh_CN: 默认不接管条目所有权（WidgetOwnership::Borrowed）；可通过 WidgetOwnership 显式指定恢复原父对象或由容器托管销毁。
 */
class Accordion : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(ExpansionMode expansionMode READ expansionMode WRITE setExpansionMode NOTIFY expansionModeChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    /**
     * @brief Controls whether one or several items may be expanded.
     * zh_CN: 控制同一时间允许展开一个还是多个条目。
     */
    enum class ExpansionMode { Single, Multiple };
    Q_ENUM(ExpansionMode)

    explicit Accordion(QWidget* parent = nullptr);
    ~Accordion() override;

    ExpansionMode expansionMode() const { return m_expansionMode; }
    void setExpansionMode(ExpansionMode mode);

    int count() const { return m_items.size(); }
    Expander* itemAt(int index) const;
    int indexOf(const Expander* item) const;
    WidgetOwnership itemOwnershipAt(int index) const;

    /**
     * @brief Appends an Expander item without taking ownership (WidgetOwnership::Borrowed).
     * zh_CN: 追加 Expander 条目（不接管所有权）。
     */
    bool addItem(Expander* item);

    /**
     * @brief Appends an Expander item with the given lifetime policy.
     * zh_CN: 按指定的所有权策略追加 Expander 条目。
     */
    bool addItem(Expander* item, WidgetOwnership ownership);

    /**
     * @brief Inserts an Expander item at index without taking ownership (WidgetOwnership::Borrowed).
     * zh_CN: 在指定位置插入 Expander 条目（不接管所有权）。
     */
    bool insertItem(int index, Expander* item);

    /**
     * @brief Inserts an Expander item at index with the given lifetime policy.
     * zh_CN: 按指定的所有权策略在指定位置插入 Expander 条目。
     */
    bool insertItem(int index, Expander* item, WidgetOwnership ownership);

    /**
     * @brief Removes an item and detaches, restores, or deletes it per its ownership policy.
     * zh_CN: 移除条目，并按其所有权策略解除关联、恢复原父对象或销毁。
     */
    bool removeItem(int index);

    /**
     * @brief Removes and returns an item without deleting it, transferring ownership to the caller.
     * zh_CN: 移出并返回条目（不销毁，所有权交还调用方）。
     */
    Expander* takeItem(int index);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void expansionModeChanged(ExpansionMode mode);
    void countChanged(int count);
    void itemAdded(int index, Expander* item);
    void itemRemoved(int index, Expander* item);
    void itemExpansionChanged(int index, bool expanded);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct ItemRecord {
        Expander* identity = nullptr;
        QPointer<Expander> item;
        QPointer<QWidget> originalParent;
        WidgetOwnership ownership = WidgetOwnership::Borrowed;
        QMetaObject::Connection expandedConnection;
        QMetaObject::Connection destroyedConnection;
    };

    void handleExpandedChanged(Expander* item, bool expanded);
    void handleItemDestroyed(Expander* identity);
    void enforceSingleExpansion(Expander* preferredItem);
    bool focusHeaderAt(int index);
    bool focusRelativeHeader(int currentIndex, int direction);
    Expander* releaseItem(int index, bool deleteOwned, bool restoreParent, bool emitSignals);

    QVBoxLayout* m_layout = nullptr;
    QVector<ItemRecord> m_items;
    ExpansionMode m_expansionMode = ExpansionMode::Single;
    bool m_enforcingExpansion = false;
};

} // namespace fluent::layout

Q_DECLARE_METATYPE(fluent::layout::Accordion::ExpansionMode)

#endif // FLUENTQT_COMPONENTS_LAYOUT_ACCORDION_H
