#ifndef GALLERYENTRYGRID_H
#define GALLERYENTRYGRID_H

#include <QPixmap>
#include <QString>
#include <QVector>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

namespace fluent::gallery {

/**
 * @brief A virtualized grid of entry cards drawn by a single widget.
 * zh_CN: 由单个 widget 绘制的虚拟化卡片网格。
 *
 * Where the old category page built one GalleryEntryCard widget per control (60+
 * widgets, each with its own labels and stylesheet — the bulk of a category page's
 * build cost), this holds the entries as plain data and paints only the cards that
 * intersect the exposed paint rect. The scroll area clips to the viewport, so a
 * scroll repaints just the visible band: construction is O(1) and painting is bound
 * to what's on screen, regardless of how many entries exist.
 * zh_CN: 旧分类页为每个控件建一个 GalleryEntryCard（60+ 个 widget，各自带标签和样式表——
 * 分类页建页开销的大头）；这里把条目当作纯数据持有，只绘制与曝光区相交的卡片。滚动区裁剪到
 * 视口，滚动时只重绘可见的一条带：构造是 O(1)、绘制量只跟屏幕上的内容相关，与条目总数无关。
 */
class GalleryEntryGrid : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
    Q_OBJECT

public:
    struct Entry {
        QString routeId;
        QString title;
        QString description;
        QPixmap icon;       // control image; used when iconGlyph is empty
        QString iconGlyph;  // Segoe Fluent Icons glyph drawn on a tile instead of `icon`
    };

    explicit GalleryEntryGrid(QWidget* parent = nullptr);

    void setEntries(const QVector<Entry>& entries);
    int entryCount() const { return m_entries.size(); }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

signals:
    /** @brief Emitted when the user clicks a card. */
    void activated(const QString& routeId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    int columns() const;
    int rowCount() const;
    int columnWidth() const;
    QRect cardRect(int index) const;
    int cardIndexAt(const QPoint& pos) const;
    int gridHeight() const;
    void setHoveredIndex(int index);

    QVector<Entry> m_entries;
    int m_hoveredIndex = -1;
    int m_lastColumns = 0;
};

} // namespace fluent::gallery

#endif // GALLERYENTRYGRID_H
