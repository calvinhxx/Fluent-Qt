#ifndef FLUENTQT_TAGINPUT_H
#define FLUENTQT_TAGINPUT_H

#include "AutoSuggestBox.h"
#include <QVariantList>
#include <QVector>

namespace fluent::textfields {

/**
 * @brief Multi-value tag input arranged around AutoSuggestBox's text editor.
 * zh_CN: 基于 AutoSuggestBox 的多标签输入控件，在编辑区周围排列已选标签。
 *
 * Each selected tag is stored as a QVariantMap with non-empty "key" and "text"
 * strings (keys uniquely identify tags; duplicate display text is allowed).
 * Suggestions may pass that map or a string key via Qt::UserRole. Selected
 * items are stored as value snapshots and persist across model updates.
 * zh_CN: 每个已选标签以包含非空 "key" 和 "text" 的 QVariantMap 存储（key 唯一标识标签，
 * 允许显示文本重名）；候选项可通过 Qt::UserRole 传入同结构 map 或字符串 key。
 * 已选列表保存值快照，不会随候选模型替换或筛选丢失。
 */
class TagInput : public AutoSuggestBox {
    Q_OBJECT
    Q_PROPERTY(QVariantList selectedItems READ selectedItems WRITE setSelectedItems NOTIFY selectionChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)

public:
    explicit TagInput(QWidget* parent = nullptr);
    ~TagInput() override;

    QVariantList selectedItems() const { return m_items; }
    int selectedCount() const { return m_items.size(); }
    QStringList selectedKeys() const;
    QStringList selectedTexts() const;
    bool containsKey(const QString& key) const;

    /** @brief Replaces selected tags, ignoring malformed items and duplicate keys (keeping first order).
     * zh_CN: 批量设置已选标签，忽略格式无效项和重复 key（按首次出现顺序保留）。
     */
    void setSelectedItems(const QVariantList& items);
    /** @brief Adds a tag snapshot; returns false if key is empty or already selected.
     * zh_CN: 添加标签；key 为空或已存在时返回 false。
     */
    bool addItem(const QString& key, const QString& text);
    bool removeItem(const QString& key);
    void clearSelection();
    void onThemeUpdated() override;

signals:
    void selectionChanged();
    /** @brief Emitted when typed text or delimited pasted tokens need external resolution.
     *
     * Unresolved text is not added automatically; call addItem() and clear() or setText() after resolving tokens.
     * zh_CN: 用户提交输入文本或粘贴带分隔符的片段时发出，由外部校验后调用 addItem() 及 clear()/setText()（未识别文本不会自动成标签）。
     */
    void tokensSubmitted(const QStringList& tokens);

protected:
    void trailingContentGeometryChanged() override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private:
    QWidget* createTag(const QVariantMap& item);
    void updateTag(QWidget* tag, const QVariantMap& item);
    void retireTag(QWidget* tag);
    void rebuildTags();
    void layoutTags();
    void focusTag(int index);
    void submitText(const QString& text);
    void scheduleDelimitedInput();
    int indexForKey(const QString& key) const;

    QVariantList m_items;
    QVector<QWidget*> m_tags;
    bool m_layouting = false;
    bool m_delimitedPending = false;
};

} // namespace fluent::textfields
#endif
