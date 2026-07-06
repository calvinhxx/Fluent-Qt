#ifndef TESTS_FLUENTGRIDITEMDELEGATE_H
#define TESTS_FLUENTGRIDITEMDELEGATE_H

#include <QStyledItemDelegate>

namespace fluent {
class FluentElement;
}
class QListView;
class QModelIndex;
class QPainter;
class QStyleOptionViewItem;

/**
 * 测试 / 示例用：Fluent 风格网格项代理（业务层组装，不放入 FluentQt core library）。
 *
 * 功能：
 *   - 支持图片网格卡片（通过 ImageRole 传入 QPixmap）
 *   - 无图片时回退为 icon glyph + text 卡片
 *   - 选中态 accent 边框 + 右上角 check 浮层（WinUI 3 多选样式）
 *   - 圆角 4px 对齐 Figma GridView Item 设计稿
 */
namespace gridview_test {

/** 自定义 role：传入 QPixmap 用于网格项图片 */
enum {
    ImageRole = Qt::UserRole + 100,
    ImageLoadingRole = Qt::UserRole + 101
};

class FluentGridItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit FluentGridItemDelegate(fluent::FluentElement* themeHost,
                                    QListView* view,
                                    QObject* parent = nullptr);

    void setThemeHost(fluent::FluentElement* host);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    void drawCheckOverlay(QPainter* painter, const QRectF& cellRect,
                          bool selected, bool enabled) const;
    bool hasLoadingRows() const;
    qreal shimmerProgress() const;

    fluent::FluentElement* m_themeHost = nullptr;
    QListView* m_view = nullptr;
};

} // namespace gridview_test

#endif
