#ifndef GALLERY_COLLECTION_SAMPLE_DELEGATES_H
#define GALLERY_COLLECTION_SAMPLE_DELEGATES_H

#include <QStyledItemDelegate>

namespace fluent {
class FluentElement;
}
namespace fluent::collections {
class GridView;
class ListView;
class TreeView;
}
class QPainter;
class QModelIndex;
class QStyleOptionViewItem;

namespace fluent::gallery {

/**
 * @brief Custom item roles shared by the Collections sample models / delegates.
 * zh_CN: Collections 示例模型 / 代理共用的自定义条目角色。
 */
enum GalleryItemRole {
    PhotoImageRole = Qt::UserRole + 701,   ///< QPixmap cover for photo cards.
    PhotoSubtitleRole = Qt::UserRole + 702, ///< Secondary caption line.
    TreeIconGlyphRole = Qt::UserRole + 720, ///< Segoe Fluent Icons glyph string.
    TreeIconColorRole = Qt::UserRole + 721  ///< Optional QColor for the glyph.
};

/**
 * @brief GridView photo-card delegate (mirrors gridview_test::FluentGridItemDelegate).
 * zh_CN: GridView 图片卡片代理（对齐 gridview_test::FluentGridItemDelegate）。
 *
 * Paints the cover image, caption scrim and an accent selection border, and — when the
 * grid is in a Multiple/Extended selection mode — a top-right check overlay, the WinUI 3
 * multi-select affordance the TestGridView UT demonstrates.
 * zh_CN: 绘制封面图、标题渐隐条与强调色选中边框；当网格处于多选/扩展选择模式时，于右上角绘制
 * 选中勾选浮层——即 TestGridView UT 演示的 WinUI 3 多选样式。
 */
class GridPhotoDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    GridPhotoDelegate(fluent::FluentElement* themeHost,
                      fluent::collections::GridView* view,
                      QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    void drawCheckOverlay(QPainter* painter, const QRectF& card,
                          bool selected, bool enabled) const;

    fluent::FluentElement* m_themeHost = nullptr;
    fluent::collections::GridView* m_view = nullptr;
};

/**
 * @brief ListView row delegate (mirrors listview_test::FluentListItemDelegate).
 * zh_CN: ListView 行代理（对齐 listview_test::FluentListItemDelegate）。
 *
 * Draws the rounded selection / hover background with enough left padding to clear the
 * container's accent indicator pill, then the row icon and text. The ListView container
 * stays a pure container — the row look lives here, in the delegate layer.
 * zh_CN: 绘制圆角选中 / 悬停背景，并在左侧留出容器强调指示条的空间，再绘制行图标与文字。
 * ListView 保持为纯容器——条目观感放在代理层。
 */
class ListRowDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    ListRowDelegate(fluent::FluentElement* themeHost,
                    fluent::collections::ListView* view,
                    QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    fluent::FluentElement* m_themeHost = nullptr;
    fluent::collections::ListView* m_view = nullptr;
};

/**
 * @brief TreeView row delegate (mirrors treeview_test::FluentTreeItemDelegate).
 * zh_CN: TreeView 行代理（对齐 treeview_test::FluentTreeItemDelegate）。
 *
 * Rounded row background, animated accent indicator (single-select), rotating chevron,
 * optional tri-state checkbox (multi-select) and a per-row icon glyph. Clicks on the
 * chevron toggle expansion; clicks on the checkbox cascade down and roll up the tri-state.
 * zh_CN: 圆角行背景、强调指示条动画（单选）、旋转 chevron、可选三态复选框（多选）以及行图标字形。
 * 点击 chevron 展开/折叠；点击复选框向下级联并向上汇总三态。
 */
class TreeRowDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    TreeRowDelegate(fluent::FluentElement* themeHost, int rowHeight,
                    fluent::collections::TreeView* view,
                    QObject* parent = nullptr);

    void setCheckBoxVisible(bool visible) { m_checkBoxVisible = visible; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;

private:
    QRectF bgRectForOption(const QStyleOptionViewItem& option) const;
    QRectF chevronRectForOption(const QStyleOptionViewItem& option) const;
    QRectF checkBoxRectForOption(const QStyleOptionViewItem& option) const;

    fluent::FluentElement* m_themeHost = nullptr;
    int m_rowHeight = 0;
    fluent::collections::TreeView* m_view = nullptr;
    bool m_checkBoxVisible = false;
};

} // namespace fluent::gallery

#endif // GALLERY_COLLECTION_SAMPLE_DELEGATES_H
