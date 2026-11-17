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
 * Items default to WidgetOwnership::Borrowed; pass WidgetOwnership to restore or transfer item cleanup.
 * zh_CN: 纵向排列多个 Expander 并协调展开与键盘焦点的容器；条目默认按 WidgetOwnership::Borrowed 管理，也可传入其他销毁方式。
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
     * @brief Appends an Expander item (WidgetOwnership::Borrowed; not deleted by Accordion).
     * zh_CN: 追加 Expander 条目（默认不自动销毁，可用带 WidgetOwnership 的重载指定销毁方式）。
     */
    bool addItem(Expander* item);

    /**
     * @brief Appends an Expander item with the given WidgetOwnership cleanup mode.
     * zh_CN: 追加 Expander 条目，并传入 WidgetOwnership 指定销毁方式。
     */
    bool addItem(Expander* item, WidgetOwnership ownership);

    /**
     * @brief Inserts an Expander item at index (WidgetOwnership::Borrowed; not deleted by Accordion).
     * zh_CN: 在指定位置插入 Expander 条目（默认不自动销毁，可用带 WidgetOwnership 的重载指定销毁方式）。
     */
    bool insertItem(int index, Expander* item);

    /**
     * @brief Inserts an Expander item at index with the given WidgetOwnership cleanup mode.
     * zh_CN: 在指定位置插入 Expander 条目，并传入 WidgetOwnership 指定销毁方式。
     */
    bool insertItem(int index, Expander* item, WidgetOwnership ownership);

    /**
     * @brief Removes the item at index and handles cleanup according to its WidgetOwnership mode.
     * zh_CN: 移除指定位置的条目，并按其 WidgetOwnership 指定的销毁方式解除关联、恢复原父控件或销毁。
     */
    bool removeItem(int index);

    /**
     * @brief Removes and returns the item at index for the caller to manage or delete.
     * zh_CN: 移出并返回指定位置的条目控件，改由调用方自行销毁。
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
