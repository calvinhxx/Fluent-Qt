#ifndef FLUENTQT_COLLECTIONS_TRANSFER_H
#define FLUENTQT_COLLECTIONS_TRANSFER_H

#include <QWidget>
#include <QStringList>
#include <memory>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QAbstractItemModel;

namespace fluent::collections {

class ListView;

/**
 * @brief Searchable dual-list selection backed by a caller-owned flat model.
 * zh_CN: 基于调用方持有的扁平模型，支持两侧搜索和批量移动的双栏选择器。
 *
 * Column zero supplies DisplayRole and unique, nonempty string keys at keyRole.
 * Invalid or duplicate keys are excluded. Only key membership is stored here;
 * source rows are never copied, moved, edited, or deleted. Selected keys survive
 * model resets when still present. Missing keys are removed after model changes.
 * Qt ItemIsEnabled and ItemIsSelectable govern user transfers in both directions.
 * zh_CN: 第零列通过 DisplayRole 提供文字，通过 keyRole 提供非空且唯一的字符串键。
 * 无效或重复键不显示；组件仅记录归属，不复制、移动、修改或删除源模型行。
 * 重置后仍存在的已选键会保留，不再存在的键会移除；两侧移动均遵守模型的启用及可选标记。
 */
class Transfer : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(int keyRole READ keyRole WRITE setKeyRole NOTIFY keyRoleChanged)
    Q_PROPERTY(int descriptionRole READ descriptionRole WRITE setDescriptionRole NOTIFY descriptionRoleChanged)
    Q_PROPERTY(QStringList selectedKeys READ selectedKeys WRITE setSelectedKeys NOTIFY selectedKeysChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY countsChanged)
    Q_PROPERTY(int availableCount READ availableCount NOTIFY countsChanged)
    Q_PROPERTY(QString sourceHeader READ sourceHeader WRITE setSourceHeader NOTIFY sourceHeaderChanged)
    Q_PROPERTY(QString targetHeader READ targetHeader WRITE setTargetHeader NOTIFY targetHeaderChanged)
    Q_PROPERTY(QString sourceFilter READ sourceFilter WRITE setSourceFilter NOTIFY sourceFilterChanged)
    Q_PROPERTY(QString targetFilter READ targetFilter WRITE setTargetFilter NOTIFY targetFilterChanged)
    Q_PROPERTY(QString searchPlaceholderText
        READ searchPlaceholderText
        WRITE setSearchPlaceholderText
        NOTIFY searchPlaceholderTextChanged)

public:
    explicit Transfer(QWidget* parent = nullptr);
    ~Transfer() override;

    QAbstractItemModel* model() const;
    /** @brief Borrows the model; destruction clears the surface without taking ownership.
     * zh_CN: 借用模型，模型销毁时清空表面，不接管其所有权。 */
    void setModel(QAbstractItemModel* model);
    int keyRole() const;
    void setKeyRole(int role);
    int descriptionRole() const;
    void setDescriptionRole(int role);
    QStringList selectedKeys() const;
    /** @brief Keeps unique known keys in the supplied order; disabled keys may be assigned programmatically.
     * zh_CN: 按传入顺序保留已知且唯一的键，程序可显式分配禁用项。 */
    void setSelectedKeys(const QStringList& keys);
    int selectedCount() const;
    int availableCount() const;
    QString sourceHeader() const;
    void setSourceHeader(const QString& text);
    QString targetHeader() const;
    void setTargetHeader(const QString& text);
    QString sourceFilter() const;
    void setSourceFilter(const QString& text);
    QString targetFilter() const;
    void setTargetFilter(const QString& text);
    QString searchPlaceholderText() const;
    void setSearchPlaceholderText(const QString& text);

    /** @brief Owned views for delegates, keyboard selection, and inspection; keep their internal models installed.
     * zh_CN: 组件持有的视图，供设置代理、键盘选择和检查使用；请保留其内部模型。 */
    ListView* sourceView() const;
    ListView* targetView() const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    int heightForWidth(int width) const override;
    void onThemeUpdated() override;

public slots:
    void addSelected();
    void removeSelected();

signals:
    void modelChanged();
    void keyRoleChanged();
    void descriptionRoleChanged();
    void selectedKeysChanged(const QStringList& keys);
    void countsChanged();
    void sourceHeaderChanged();
    void targetHeaderChanged();
    void sourceFilterChanged();
    void targetFilterChanged();
    void searchPlaceholderTextChanged();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace fluent::collections

#endif
