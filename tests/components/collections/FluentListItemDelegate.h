#ifndef TESTS_FLUENTLISTITEMDELEGATE_H
#define TESTS_FLUENTLISTITEMDELEGATE_H

#include <QStyledItemDelegate>

namespace fluent {
class FluentElement;
}
class QAbstractItemView;
class QModelIndex;
class QPainter;
class QStyleOptionViewItem;

/**
 * 测试 / 示例用：Fluent 风格列表行代理（业务层组装，不放入 FluentQt core library）。
 */
namespace listview_test {

class FluentListItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit FluentListItemDelegate(fluent::FluentElement* themeHost, int rowHeight,
                                  QAbstractItemView* view,
                                  QObject* parent = nullptr);

    void setThemeHost(fluent::FluentElement* host);
    void setRowHeight(int height);
    int rowHeight() const { return m_rowHeight; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    fluent::FluentElement* m_themeHost = nullptr;
    int m_rowHeight = 0;
};

} // namespace listview_test

#endif
