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
} // namespace fluent::collections
class QPainter;
class QFont;
class QModelIndex;
class QRectF;
class QString;
class QStyleOptionViewItem;

namespace fluent::gallery {

/**
 * @brief Custom item roles shared by the Collections sample models / delegates.
 * zh_CN: Collections 示例模型 / 代理共用的自定义条目角色。
 */
enum GalleryItemRole {
    PhotoImageRole = Qt::UserRole + 701,    ///< QPixmap cover for photo cards.
    PhotoSubtitleRole = Qt::UserRole + 702, ///< Secondary caption line.
    TreeIconGlyphRole = Qt::UserRole + 720, ///< FluentQt Icons glyph string.
    TreeIconColorRole = Qt::UserRole + 721  ///< Optional QColor for the glyph.
};

void drawPhotoCaption(QPainter* painter,
    const QRectF& card,
    const QString& title,
    const QString& subtitle,
    const QFont& baseFont);

/**
 * @brief GridView photo-card delegate painting cover art, caption scrim, accent border, and multi-select check overlay.
 * zh_CN: GridView 图片卡片代理：绘制封面图、标题遮罩、强调色选中边框及多选模式勾选浮层。
 */
class GridPhotoDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    GridPhotoDelegate(fluent::FluentElement* themeHost, fluent::collections::GridView* view, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    void drawCheckOverlay(QPainter* painter, const QRectF& card, bool selected, bool enabled) const;

    fluent::FluentElement* m_themeHost = nullptr;
    fluent::collections::GridView* m_view = nullptr;
};

/**
 * @brief ListView row delegate drawing the rounded row fill, icon, and text while reserving space for the indicator pill.
 * zh_CN: ListView 行代理：绘制圆角行背景、图标与文字，并为容器左侧强调指示条预留间距。
 */
class ListRowDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    ListRowDelegate(fluent::FluentElement* themeHost, fluent::collections::ListView* view, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    fluent::FluentElement* m_themeHost = nullptr;
    fluent::collections::ListView* m_view = nullptr;
};

/**
 * @brief TreeView row delegate with rounded selection fill, rotating chevron, glyph icon, and optional tri-state checkbox.
 * zh_CN: TreeView 行代理：支持圆角选中背景、旋转展开箭头、图标字形及可选三态复选框（含父子级联勾选）。
 */
class TreeRowDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    TreeRowDelegate(fluent::FluentElement* themeHost,
        int rowHeight,
        fluent::collections::TreeView* view,
        QObject* parent = nullptr);

    void setCheckBoxVisible(bool visible) { m_checkBoxVisible = visible; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event,
        QAbstractItemModel* model,
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
