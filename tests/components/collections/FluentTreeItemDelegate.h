#ifndef TESTS_FLUENTTREEITEMDELEGATE_H
#define TESTS_FLUENTTREEITEMDELEGATE_H

#include <QStyledItemDelegate>

#include "components/foundation/FluentElement.h"

class QAbstractItemView;
class QModelIndex;
class QPainter;
class QStyleOptionViewItem;
class QMouseEvent;

/**
 * 测试 / 示例用：Fluent 风格树视图行代理（业务层组装，不放入 FluentQt core library）。
 *
 * WinUI 3 TreeView 视觉规范：
 *   - 32px 行高, 4px 圆角背景
 *   - 中号 Chevron 展开/折叠图标 (12px Segoe Fluent Icons)
 *   - 选中项左侧 3px×16px accent 指示条
 *   - 每级缩进 16px
 *   - 可选 CheckBox (Multi-selection 模式)
 *   - 可选图标 (通过 IconGlyphRole)
 */
namespace treeview_test {

/** 自定义 Role: 存放 Segoe Fluent Icons 字符串作为行前图标 */
static constexpr int IconGlyphRole = Qt::UserRole + 100;

class FluentTreeItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit FluentTreeItemDelegate(fluent::FluentElement* themeHost, int rowHeight,
                                    QAbstractItemView* view,
                                    QObject* parent = nullptr);

    void setThemeHost(fluent::FluentElement* host);
    void setRowHeight(int height);
    int rowHeight() const { return m_rowHeight; }

    /** 是否在每行显示 CheckBox（WinUI Multi-selection 模式） */
    void setCheckBoxVisible(bool visible);
    bool checkBoxVisible() const { return m_checkBoxVisible; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;

private:
    qreal accentProgress(const QModelIndex& index) const;

    void paintCheckBox(QPainter* painter, const QRectF& rect,
                       Qt::CheckState state, const fluent::FluentElement::Colors& colors) const;
    void paintIconGlyph(QPainter* painter, const QRectF& rect,
                        const QString& glyph, const QColor& color) const;

    /** Compute the full-width background rect for a row */
    QRectF bgRectForOption(const QStyleOptionViewItem& option) const;
    /** Compute the chevron hit-test rect within a row */
    QRectF chevronRectForOption(const QStyleOptionViewItem& option) const;
    /** Compute the checkbox hit-test rect within a row */
    QRectF checkBoxRectForOption(const QStyleOptionViewItem& option) const;

    fluent::FluentElement* m_themeHost = nullptr;
    int m_rowHeight = 0;
    QAbstractItemView* m_view = nullptr;
    bool m_checkBoxVisible = false;
};

} // namespace treeview_test

#endif
