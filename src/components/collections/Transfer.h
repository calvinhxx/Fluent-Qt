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
 * @brief Searchable dual-list transfer widget backed by an external flat model.
 * Reads DisplayRole and unique non-empty string keys from keyRole in column 0 (ignoring
 * invalid or duplicate keys). Tracks only selected key membership without copying, moving,
 * or editing source rows; keys still present survive model resets, and user transfers respect
 * Qt::ItemIsEnabled and Qt::ItemIsSelectable.
 * zh_CN: 基于外部扁平模型、支持双向搜索和移动的双栏穿梭框。
 * 读取第 0 列的 DisplayRole 与 keyRole（须为非空唯一字符串，无效或重复键会被忽略）。
 * 组件仅记录已选键集合，不复制、移动或修改源模型行；模型重置后仍存在的已选键会保留，用户操作遵守模型的启用和可选标记。
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
    /**
     * @brief Associates an external model without taking ownership; clears lists if the model is destroyed.
     * zh_CN: 关联外部数据模型（不接管所有权）；模型销毁时清空列表。
     */
    void setModel(QAbstractItemModel* model);
    int keyRole() const;
    void setKeyRole(int role);
    int descriptionRole() const;
    void setDescriptionRole(int role);
    QStringList selectedKeys() const;
    /**
     * @brief Sets selected keys using unique valid keys in the given order; disabled items can be selected via code.
     * zh_CN: 按传入顺序设置已选键（自动去重并忽略无效键）；程序调用可选中禁用项。
     */
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

    /**
     * @brief Returns the internal source ListView for delegate or inspection setup; keep its internal proxy model.
     * zh_CN: 返回内部源列表 ListView 以配置代理或检查状态，请勿替换其内部代理模型。
     */
    ListView* sourceView() const;
    /**
     * @brief Returns the internal target ListView for delegate or inspection setup; keep its internal proxy model.
     * zh_CN: 返回内部目标列表 ListView 以配置代理或检查状态，请勿替换其内部代理模型。
     */
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
