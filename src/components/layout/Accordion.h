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
 * @brief Vertical group of Expander items with coordinated expansion and keyboard focus.
 * zh_CN: 纵向组织多个 Expander，并统一管理展开状态与键盘焦点的容器。
 *
 * Accordion composes existing Expander instances instead of duplicating their
 * header, content, animation, or ownership contracts. Items are borrowed by
 * default; callers opt into Reparented or Owned lifetime behavior explicitly.
 * zh_CN: Accordion 直接组合现有 Expander，不重复实现其标题、内容、动画或所有权契约。
 * 默认仅借用条目；调用方可显式选择 Reparented 或 Owned 生命周期语义。
 */
class Accordion : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(ExpansionMode expansionMode READ expansionMode
                   WRITE setExpansionMode NOTIFY expansionModeChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    /**
     * @brief Controls whether one or several items may be expanded.
     * zh_CN: 控制同一时间允许展开一个还是多个条目。
     */
    enum class ExpansionMode {
        Single,
        Multiple
    };
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
     * @brief Appends a borrowed Expander item.
     * zh_CN: 追加一个仅借用的 Expander 条目。
     */
    bool addItem(Expander* item);

    /**
     * @brief Appends an Expander item with explicit lifetime ownership.
     * zh_CN: 使用显式生命周期所有权追加 Expander 条目。
     */
    bool addItem(Expander* item, WidgetOwnership ownership);

    /**
     * @brief Inserts a borrowed Expander at the requested index.
     * zh_CN: 在指定位置插入一个仅借用的 Expander。
     */
    bool insertItem(int index, Expander* item);

    /**
     * @brief Inserts an Expander with explicit lifetime ownership.
     * zh_CN: 使用显式生命周期所有权在指定位置插入 Expander。
     */
    bool insertItem(int index,
                    Expander* item,
                    WidgetOwnership ownership);

    /**
     * @brief Removes an item and applies its configured ownership policy.
     * zh_CN: 移除条目并执行其已配置的所有权策略。
     */
    bool removeItem(int index);

    /**
     * @brief Removes an item without deleting it and transfers it to the caller.
     * zh_CN: 移除条目但不删除，并将其转交给调用方。
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
    Expander* releaseItem(int index,
                          bool deleteOwned,
                          bool restoreParent,
                          bool emitSignals);

    QVBoxLayout* m_layout = nullptr;
    QVector<ItemRecord> m_items;
    ExpansionMode m_expansionMode = ExpansionMode::Single;
    bool m_enforcingExpansion = false;
};

} // namespace fluent::layout

Q_DECLARE_METATYPE(fluent::layout::Accordion::ExpansionMode)

#endif // FLUENTQT_COMPONENTS_LAYOUT_ACCORDION_H
