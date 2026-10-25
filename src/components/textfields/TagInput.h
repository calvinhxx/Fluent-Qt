#ifndef FLUENTQT_TAGINPUT_H
#define FLUENTQT_TAGINPUT_H

#include "AutoSuggestBox.h"
#include <QVariantList>
#include <QVector>

namespace fluent::textfields {

/**
 * @brief Compact selected tags around AutoSuggestBox's native text editor.
 * zh_CN: 在 AutoSuggestBox 原生文字编辑区周围排列紧凑的已选标签。
 *
 * Each selected item is a QVariantMap with non-empty "key" and "text" strings.
 * Stable keys identify values; equal labels are allowed. Suggestions may carry
 * the same map in Qt::UserRole, or a string key with the candidate text as label.
 * Models and transport remain application-owned. Selection stores value
 * snapshots and survives suggestion model replacement and filtering.
 * zh_CN: 每项是含非空 key、text 字符串的 QVariantMap；key 标识身份，文字可重名。
 * 建议项 UserRole 可提供同形 map，或字符串身份并使用候选文字作为标签。
 * 模型与请求仍由应用拥有，选择保存值快照，不随候选模型替换或筛选丢失。
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

    /** @brief Restores value snapshots; ignores malformed items and duplicate keys, preserving first occurrence.
     * zh_CN: 恢复值快照；忽略无效项和重复身份，保留首个出现项及顺序。
     */
    void setSelectedItems(const QVariantList& items);
    /** @brief Adds a value snapshot; returns false for empty or already-selected keys.
     * zh_CN: 添加值快照；身份为空或已选中时返回 false。
     */
    bool addItem(const QString& key, const QString& text);
    bool removeItem(const QString& key);
    void clearSelection();
    void onThemeUpdated() override;

signals:
    void selectionChanged();
    /** @brief Asks the application to resolve submitted text or delimited pasted values.
     * zh_CN: 请应用解析提交文字或分隔粘贴的值；应用添加有效项并保留待修正文字。
     * Unknown text never silently becomes a directory member. The application
     * can call addItem and clear/setText after resolving these values.
     * zh_CN: 未知文字不会自动成为目录成员；应用解析后可调用 addItem 与 clear/setText。
     */
    void tokensSubmitted(const QStringList& tokens);

protected:
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
