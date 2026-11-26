#ifndef FLUENTQT_COLLECTIONS_TRANSFER_H
#define FLUENTQT_COLLECTIONS_TRANSFER_H

#include <QWidget>
#include <QStringList>
#include <memory>
#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

class QAbstractItemModel;

namespace fluent::collections {

class ListView;

/**
 * @brief Searchable dual-list transfer widget backed by a flat item model.
 * Column 0's keyRole must provide unique non-empty keys; only selected keys are tracked without editing the source model.
 * zh_CN: 双栏穿梭框，支持搜索并在两侧间移动条目。要求模型第 0 列的 keyRole 为非空唯一值，组件只记录已选键不改动源模型。
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
     * @brief Sets the source item model (still managed by the caller); clears lists if the model is destroyed.
     * zh_CN: 设置源数据模型（仍由应用管理，不会随视图销毁）；模型销毁时清空列表。
     */
    void setModel(QAbstractItemModel* model);
    int keyRole() const;
    void setKeyRole(int role);
    int descriptionRole() const;
    void setDescriptionRole(int role);
    QStringList selectedKeys() const;
    /**
     * @brief Sets selected keys in order, ignoring duplicates or invalid keys (code can select disabled items).
     * zh_CN: 按传入顺序设置已选键（自动去重并忽略无效键，代码调用可选中禁用项）。
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
     * @brief Returns the internal source ListView for styling or delegates; do not replace its proxy model.
     * zh_CN: 返回左侧源列表 ListView 以便定制代理，请勿替换其内部代理模型。
     */
    ListView* sourceView() const;
    /**
     * @brief Returns the internal target ListView for styling or delegates; do not replace its proxy model.
     * zh_CN: 返回右侧目标列表 ListView 以便定制代理，请勿替换其内部代理模型。
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
