#ifndef GALLERYENTRYGRID_H
#define GALLERYENTRYGRID_H

#include <QPixmap>
#include <QString>
#include <QVector>
#include <QWidget>
#include <QVariantAnimation>
#include <QTransform>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

namespace fluent::gallery {

/**
 * @brief Virtualized grid of entry cards painted by a single widget.
 *
 * Stores entry metadata directly and paints only cards intersecting the exposed
 * paint rect, keeping construction O(1) regardless of entry count.
 * zh_CN: 由单个控件绘制的虚拟化卡片网格：仅持有条目数据并按曝光区域按需绘制，构造开销为 O(1)。
 */
class GalleryEntryGrid : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
    Q_OBJECT

public:
    struct Entry {
        QString routeId;
        QString title;
        QString description;
        QString imageResource; // SVG artwork; used when iconGlyph is empty
        QString iconGlyph;     // FluentQt Icons glyph drawn instead of artwork
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
    bool event(QEvent* event) override;
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
    bool recalculateRowLayout();
    void setHoveredIndex(int index);
    void animateTilt(const QPointF& tilt, qreal lift);
    QTransform cardTransform(int index) const;
    void paintEntry(QPainter& painter, int index, const QRect& rect, bool hovered) const;
    void resetDepthMotion();

    QVector<Entry> m_entries;
    QVector<int> m_rowHeights;
    QVector<int> m_rowTops;
    int m_hoveredIndex = -1;
    int m_lastColumns = 0;
    int m_lastColumnWidth = 0;
    QVariantAnimation m_depthMotion;
    QPointF m_tilt;
    qreal m_lift = 0;
    int m_animatedIndex = -1;
    int m_pressedIndex = -1;
    QPixmap m_hoverFrame;
    int m_frameTheme = -1;
};

} // namespace fluent::gallery

#endif // GALLERYENTRYGRID_H
