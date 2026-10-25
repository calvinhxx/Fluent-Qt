#ifndef FLUENTQT_PROPERTYPANEL_H
#define FLUENTQT_PROPERTYPANEL_H

#include <QVariant>
#include <QVector>
#include <QWidget>
#include <memory>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent {
class FormController;
}
namespace fluent::textfields {
class LineEdit;
}

namespace fluent::layout {
class Field;
class PropertyPanelPrivate;

/**
 * @brief Explicit metadata for one generated editor; business values remain application-owned.
 * zh_CN: 一个自动生成编辑器的显式描述；业务值仍由应用持有。
 * Choice values are strings in choices. Numbers are finite and use minimum/maximum/step.
 * A valid defaultValue enables reset. An invalid QVariant means no default.
 * zh_CN: 枚举值使用 choices 中的字符串；数值有限，按范围和步长编辑。
 * 有效 defaultValue 启用重置；无效 QVariant 表示没有默认值。
 */
class PropertyDefinition {
    Q_GADGET
public:
    enum Type { Text, Number, Boolean, Choice, Color };
    Q_ENUM(Type)
    QString key;
    QString label;
    QString group;
    QString description;
    Type type = Text;
    double minimum = -1000000;
    double maximum = 1000000;
    double step = 1;
    QStringList choices;
    QVariant defaultValue;
    bool readOnly = false;
};

/**
 * @brief A bounded property form composed of Fluent editors, search, groups and reset actions.
 * zh_CN: 由 Fluent 编辑器组成的有限属性表单，支持搜索、分组和重置。
 * Owns generated widgets, not the application's object or persistence. At most 128
 * definitions are accepted. valueEdited requests application updates; setValue refreshes
 * an editor without emitting valueEdited. Replacing definitions resets form tracking.
 * zh_CN: 持有生成的控件，不持有业务对象或持久化逻辑，最多接受 128 个字段。
 * valueEdited 请求应用更新；setValue 更新编辑器而不发出 valueEdited，替换描述会清除表单跟踪状态。
 */
class PropertyPanel : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly NOTIFY readOnlyChanged)
    Q_PROPERTY(LayoutMode layoutMode READ layoutMode WRITE setLayoutMode NOTIFY layoutModeChanged)
    Q_PROPERTY(SearchVisibility searchVisibility
        READ searchVisibility
        WRITE setSearchVisibility
        NOTIFY searchVisibilityChanged)
    Q_PROPERTY(QStringList modifiedPropertyKeys READ modifiedPropertyKeys NOTIFY modifiedPropertiesChanged)
public:
    /** @brief Automatic layout fits labels beside editors when space permits.
     * zh_CN: 自动布局在空间足够时并排显示标签与编辑器，否则上下排列。 */
    enum class LayoutMode { Automatic, Stacked, Inline };
    Q_ENUM(LayoutMode)
    /** @brief Automatic search appears for eight or more fields or a nonempty query.
     * zh_CN: 自动搜索在字段不少于八个或存在搜索内容时显示。 */
    enum class SearchVisibility { Automatic, Visible, Hidden };
    Q_ENUM(SearchVisibility)
    explicit PropertyPanel(QWidget* parent = nullptr);
    ~PropertyPanel() override;
    /** @brief Atomically replaces definitions; invalid or duplicate keys leave the form unchanged.
     * zh_CN: 原子替换字段描述；无效描述或重复键不会改变原表单。 */
    bool setProperties(const QVector<PropertyDefinition>& properties);
    QVector<PropertyDefinition> properties() const;
    QStringList propertyKeys() const;
    QVariant value(const QString& key) const;
    /** @brief Updates a known editor using its declared type; rejects invalid values and tracks changes.
     * zh_CN: 按声明类型更新已有编辑器，拒绝无效值并跟踪变化。 */
    bool setValue(const QString& key, const QVariant& value);
    /** @brief Applies a declared default and emits valueEdited only for a real editable change.
     * zh_CN: 恢复声明的默认值，仅在可编辑值实际改变时发出 valueEdited。 */
    bool resetProperty(const QString& key);
    /** @brief Resets editable properties with defaults, including filtered fields.
     * zh_CN: 重置有默认值的可编辑字段，包括被搜索隐藏的字段。 */
    void resetAll();
    QStringList modifiedPropertyKeys() const;
    LayoutMode layoutMode() const;
    void setLayoutMode(LayoutMode mode);
    SearchVisibility searchVisibility() const;
    void setSearchVisibility(SearchVisibility visibility);
    QString searchText() const;
    void setSearchText(const QString& text);
    bool isReadOnly() const;
    void setReadOnly(bool readOnly);
    /** @brief Borrows the generated editor/Field; pointers expire when definitions are replaced.
     * zh_CN: 借用生成的编辑器或 Field；替换描述后指针失效。 */
    QWidget* editor(const QString& key) const;
    Field* field(const QString& key) const;
    fluent::FormController* formController() const;
    fluent::textfields::LineEdit* searchField() const;
    QSize sizeHint() const override;
    void onThemeUpdated() override;
signals:
    void propertiesChanged();
    void searchTextChanged(const QString& text);
    void readOnlyChanged(bool readOnly);
    void valueEdited(const QString& key, const QVariant& value);
    void layoutModeChanged(LayoutMode mode);
    void searchVisibilityChanged(SearchVisibility visibility);
    void modifiedPropertiesChanged();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    std::unique_ptr<PropertyPanelPrivate> d;
};
} // namespace fluent::layout

Q_DECLARE_METATYPE(fluent::layout::PropertyDefinition)
#endif
