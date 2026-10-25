#ifndef FLUENTQT_TREESELECT_H
#define FLUENTQT_TREESELECT_H

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include <QAbstractItemModel>
#include <QHash>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QSet>
#include <QVariantList>
#include <QWidget>

class QStackedLayout;
class QTimer;
namespace fluent::collections {
class TreeView;
}
namespace fluent::textfields {
class AutoSuggestBox;
class TagInput;
} // namespace fluent::textfields

namespace fluent::basicinput {
class TreeSelectPopup;
class TreeSelectProxy;

/**
 * @brief Searchable single or multiple selection from a borrowed hierarchical model.
 * zh_CN: 从调用方持有的层级模型中搜索并进行单选或多选。
 *
 * Column zero provides DisplayRole labels and unique non-empty string keys in
 * keyRole (UserRole by default). Missing/duplicate keys cannot be selected.
 * ItemIsEnabled and ItemIsSelectable control selection; branches may be browse-only.
 * Each node is independent: selecting a parent does not select its descendants.
 * Filtering never edits the model. Removed keys are pruned after model changes;
 * filtering, sorting and row moves retain selection by key.
 * zh_CN: 第零列 DisplayRole 为标签，keyRole（默认 UserRole）为唯一非空字符串身份。
 * 缺失或重复身份不可选择；模型 flags 决定是否可选，分组可仅用于浏览。
 * 父子节点独立选择；搜索不修改模型。模型移除的身份会被清理，筛选、排序、移动保留选择。
 */
class TreeSelect : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(int keyRole READ keyRole WRITE setKeyRole NOTIFY keyRoleChanged)
    Q_PROPERTY(SelectionMode selectionMode READ selectionMode WRITE setSelectionMode NOTIFY selectionModeChanged)
    Q_PROPERTY(QStringList selectedKeys READ selectedKeys WRITE setSelectedKeys NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectedItems READ selectedItems NOTIFY selectionChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText NOTIFY placeholderTextChanged)
    Q_PROPERTY(QString emptyText READ emptyText WRITE setEmptyText NOTIFY emptyTextChanged)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly NOTIFY readOnlyChanged)
    Q_PROPERTY(bool isOpen READ isOpen WRITE setIsOpen NOTIFY isOpenChanged)
public:
    enum class SelectionMode { Single, Multiple };
    Q_ENUM(SelectionMode)
    explicit TreeSelect(QWidget* parent = nullptr);
    ~TreeSelect() override;

    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* model);
    int keyRole() const { return m_keyRole; }
    void setKeyRole(int role);
    SelectionMode selectionMode() const { return m_mode; }
    void setSelectionMode(SelectionMode mode);
    QStringList selectedKeys() const { return m_selectedKeys; }
    /** @brief Selected value snapshots with key, text and full path strings.
     * zh_CN: 已选项值快照，包含 key、text 和完整 path 字符串。
     */
    QVariantList selectedItems() const { return m_selectedItems; }
    int selectedCount() const { return m_selectedKeys.size(); }
    /** @brief Selects known selectable keys; single mode retains the first valid key.
     * zh_CN: 选择已有可选身份；单选模式仅保留首个有效身份。
     */
    void setSelectedKeys(const QStringList& keys);
    void clearSelection();
    QString searchText() const { return m_search; }
    /** @brief Applies a query immediately; rapid typing on large trees is coalesced before filtering.
     * zh_CN: 程序查询立即生效；大型树的连续键入合并后再筛选，确认选择前会刷新结果。 */
    void setSearchText(const QString& text);
    QString placeholderText() const { return m_placeholder; }
    void setPlaceholderText(const QString& text);
    /** @brief Message shown when the search has no matching rows; empty text hides the message.
     * zh_CN: 搜索无匹配项时显示的文案；空字符串隐藏提示。 */
    QString emptyText() const { return m_emptyText; }
    void setEmptyText(const QString& text);
    bool isReadOnly() const { return m_readOnly; }
    void setReadOnly(bool readOnly);
    bool isOpen() const;
    void setIsOpen(bool open);
    void open();
    void close();
    void onThemeUpdated() override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void modelChanged(QAbstractItemModel* model);
    void keyRoleChanged(int role);
    void selectionModeChanged(SelectionMode mode);
    void selectionChanged();
    void searchTextChanged(const QString& text);
    void placeholderTextChanged();
    void emptyTextChanged();
    void readOnlyChanged(bool readOnly);
    void isOpenChanged(bool open);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    friend class TreeSelectProxy;
    friend class TreeSelectPopup;
    textfields::AutoSuggestBox* editor() const;
    void ensureEditor();
    void updateInputMetrics();
    void ensurePopup();
    bool isComposing() const;
    void rebuildIndex();
    void indexBranch(const QModelIndex& index);
    void unindexBranch(const QModelIndex& index);
    void updateIndexKey(const QModelIndex& index);
    void refreshSelection();
    void schedulePopupRefresh();
    void observeModel();
    void syncInputs();
    void updateSearchText(const QString& text, bool defer);
    void applySelection(const QStringList& keys, bool allowDisabled);
    void filter(bool resetCurrent = true);
    void activateIndex(const QModelIndex& index);
    void navigate(int direction);
    void resizePopup();
    bool canSelect(const QModelIndex& source, bool allowDisabled = false) const;
    QString pathForIndex(QModelIndex source) const;
    QPointer<QAbstractItemModel> m_model;
    QList<QMetaObject::Connection> m_modelConnections;
    QMultiHash<QString, QPersistentModelIndex> m_indexes;
    QHash<QPersistentModelIndex, QString> m_indexKeys;
    QSet<QPersistentModelIndex> m_expandedIndexes;
    QStringList m_selectedKeys;
    QVariantList m_selectedItems;
    QString m_search;
    QStringList m_searchTokens;
    QTimer* m_searchTimer = nullptr;
    QString m_placeholder;
    QString m_emptyText;
    SelectionMode m_mode = SelectionMode::Single;
    int m_keyRole = Qt::UserRole;
    bool m_readOnly = false;
    bool m_composing = false;
    bool m_syncing = false;
    bool m_checkHandled = false;
    bool m_destroying = false;
    bool m_filtering = false;
    bool m_browseInitialized = false;
    bool m_popupRefreshPending = false;
    QStackedLayout* m_layout = nullptr;
    textfields::AutoSuggestBox* m_single = nullptr;
    textfields::TagInput* m_multiple = nullptr;
    TreeSelectProxy* m_proxy = nullptr;
    TreeSelectPopup* m_popup = nullptr;
    collections::TreeView* m_tree = nullptr;
};
} // namespace fluent::basicinput
#endif
