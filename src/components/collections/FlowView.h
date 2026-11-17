#ifndef FLOWVIEW_H
#define FLOWVIEW_H

#include "compatibility/InputEventCompat.h"
#include "compatibility/WidgetCompat.h"
#include <QAbstractItemView>
#include <QHash>
#include <QMargins>
#include <QPixmap>
#include <QVector>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/collections/SelectionMode.h"

class QLabel;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QTimer;
class QWheelEvent;
class QVariantAnimation;

namespace fluent::scrolling {
class ScrollBar;
class OverscrollController;
} // namespace fluent::scrolling

namespace fluent::collections {

/**
 * @brief Collection view that wraps items automatically across rows.
 * Supports configurable item size bounds, spacing, margins, selection, and drag reordering.
 * zh_CN: 自动换行排列条目的流式集合视图，支持自定义条目尺寸上下限、间距、选择和拖拽重排。
 */
class FlowView : public QAbstractItemView, public FluentElement, public QMLPlus {
    Q_OBJECT

public:
    using SelectionMode = ::fluent::collections::SelectionMode;

    /**
     * @brief Selection mode used by the collection view.
     * zh_CN: 集合视图使用的选择模式。
     */
    Q_PROPERTY(SelectionMode selectionMode READ selectionMode WRITE setSelectionMode NOTIFY selectionModeChanged)
    /**
     * @brief Fluent typography role used for text rendering.
     * zh_CN: 文本绘制使用的 Fluent 字号样式。
     */
    Q_PROPERTY(Typography::FontRole fontRole READ fontRole WRITE setFontRole NOTIFY fontRoleChanged)
    /**
     * @brief Whether the control frame border is painted.
     * zh_CN: 是否绘制控件外框边线。
     */
    Q_PROPERTY(bool borderVisible READ borderVisible WRITE setBorderVisible NOTIFY borderVisibleChanged)
    /**
     * @brief Convenience text displayed in the header area.
     * zh_CN: 显示在头部区域的便捷标题文本。
     */
    Q_PROPERTY(QString headerText READ headerText WRITE setHeaderText NOTIFY headerTextChanged)
    /**
     * @brief Text shown when the model has no items.
     * zh_CN: 模型为空时显示的占位文本。
     */
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText NOTIFY placeholderTextChanged)
    /**
     * @brief Default item size used when neither the model nor delegate provides a valid sizeHint.
     * zh_CN: 模型和代理均未提供有效尺寸时使用的默认条目尺寸。
     */
    Q_PROPERTY(QSize defaultItemSize READ defaultItemSize WRITE setDefaultItemSize NOTIFY defaultItemSizeChanged)
    /**
     * @brief Minimum item size allowed by the flow layout.
     * zh_CN: 流式布局允许的最小条目尺寸。
     */
    Q_PROPERTY(QSize minimumItemSize READ minimumItemSize WRITE setMinimumItemSize NOTIFY minimumItemSizeChanged)
    /**
     * @brief Maximum item size allowed by the flow layout.
     * zh_CN: 流式布局允许的最大条目尺寸。
     */
    Q_PROPERTY(QSize maximumItemSize READ maximumItemSize WRITE setMaximumItemSize NOTIFY maximumItemSizeChanged)
    /**
     * @brief Model role queried for per-item size hints.
     * zh_CN: 用于读取单个条目尺寸的模型角色。
     */
    Q_PROPERTY(int itemSizeRole READ itemSizeRole WRITE setItemSizeRole NOTIFY itemSizeRoleChanged)
    /**
     * @brief Horizontal spacing between items.
     * zh_CN: 条目之间的水平间距。
     */
    Q_PROPERTY(int horizontalSpacing READ horizontalSpacing WRITE setHorizontalSpacing NOTIFY horizontalSpacingChanged)
    /**
     * @brief Vertical spacing between items.
     * zh_CN: 条目之间的垂直间距。
     */
    Q_PROPERTY(int verticalSpacing READ verticalSpacing WRITE setVerticalSpacing NOTIFY verticalSpacingChanged)
    /**
     * @brief Margins applied around the control content area.
     * zh_CN: 控件内容区域周围的边距。
     */
    Q_PROPERTY(QMargins contentMargins READ contentMargins WRITE setContentMargins NOTIFY contentMarginsChanged)
    /**
     * @brief Whether the viewport is currently hovered.
     * zh_CN: 视口当前是否处于悬停状态。
     */
    Q_PROPERTY(bool viewportHovered READ viewportHovered NOTIFY viewportHoveredChanged)
    /**
     * @brief Whether drag reordering is enabled.
     * zh_CN: 是否启用拖拽重排。
     */
    Q_PROPERTY(bool canReorderItems READ canReorderItems WRITE setCanReorderItems NOTIFY canReorderItemsChanged)
    /**
     * @brief Whether boundary wheel input may continue to an enclosing scroller.
     * zh_CN: 边界滚轮输入是否允许继续传递给外层滚动容器。
     */
    Q_PROPERTY(bool scrollChainingEnabled
        READ isScrollChainingEnabled
        WRITE setScrollChainingEnabled
        NOTIFY scrollChainingEnabledChanged)
    /**
     * @brief Whether elastic overscroll bounce is enabled at scroll boundaries.
     * Enabled by default; disable when scrolling should stop cleanly at the edge.
     * zh_CN: 滚动到达边界时是否启用弹性回弹（默认开启，希望在边界直接停住时可关闭）。
     */
    Q_PROPERTY(bool overscrollEnabled
        READ isOverscrollEnabled
        WRITE setOverscrollEnabled
        NOTIFY overscrollEnabledChanged)

    explicit FlowView(QWidget* parent = nullptr);
    ~FlowView() override;

    SelectionMode selectionMode() const { return m_selectionMode; }
    void setSelectionMode(SelectionMode mode);

    Typography::FontRole fontRole() const { return m_fontRole; }
    void setFontRole(Typography::FontRole role);

    bool borderVisible() const { return m_borderVisible; }
    bool isBorderVisible() const { return borderVisible(); }
    void setBorderVisible(bool visible);

    QString headerText() const { return m_headerText; }
    void setHeaderText(const QString& text);

    QString placeholderText() const { return m_placeholderText; }
    void setPlaceholderText(const QString& text);

    QSize defaultItemSize() const { return m_defaultItemSize; }
    void setDefaultItemSize(const QSize& size);

    QSize minimumItemSize() const { return m_minimumItemSize; }
    void setMinimumItemSize(const QSize& size);

    QSize maximumItemSize() const { return m_maximumItemSize; }
    void setMaximumItemSize(const QSize& size);

    int itemSizeRole() const { return m_itemSizeRole; }
    void setItemSizeRole(int role);

    int horizontalSpacing() const { return m_hSpacing; }
    void setHorizontalSpacing(int spacing);

    int verticalSpacing() const { return m_vSpacing; }
    void setVerticalSpacing(int spacing);

    QMargins contentMargins() const { return m_contentMargins; }
    void setContentMargins(const QMargins& margins);

    bool viewportHovered() const { return m_viewportHovered; }
    bool isViewportHovered() const { return viewportHovered(); }

    bool canReorderItems() const { return m_canReorderItems; }
    void setCanReorderItems(bool enabled);

    bool isScrollChainingEnabled() const;
    void setScrollChainingEnabled(bool enabled);

    bool isOverscrollEnabled() const;
    void setOverscrollEnabled(bool enabled);

    int selectedIndex() const;
    QList<int> selectedRows() const;
    void setSelectedIndex(int index);

    ::fluent::scrolling::ScrollBar* verticalFluentScrollBar() const;
    void refreshFluentScrollChrome();

    void setModel(QAbstractItemModel* model) override;
    void setItemDelegate(QAbstractItemDelegate* delegate);

    QRect visualRect(const QModelIndex& index) const override;
    void scrollTo(const QModelIndex& index, ScrollHint hint = EnsureVisible) override;
    QModelIndex indexAt(const QPoint& point) const override;

signals:
    void selectionModeChanged();
    void fontRoleChanged();
    void borderVisibleChanged();
    void headerTextChanged();
    void placeholderTextChanged();
    void defaultItemSizeChanged();
    void minimumItemSizeChanged();
    void maximumItemSizeChanged();
    void itemSizeRoleChanged();
    void horizontalSpacingChanged();
    void verticalSpacingChanged();
    void contentMarginsChanged();
    void viewportHoveredChanged();
    void canReorderItemsChanged();
    void scrollChainingEnabledChanged();
    void overscrollEnabledChanged();
    /**
     * @brief Convenience signal emitting the clicked row index alongside QAbstractItemView::clicked().
     * zh_CN: 与 QAbstractItemView::clicked() 对应的便捷信号，直接传递被点击的行号。
     */
    void itemClicked(int row);
    void itemReordered(int fromIndex, int toIndex);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;
    int horizontalOffset() const override;
    int verticalOffset() const override;
    bool isIndexHidden(const QModelIndex& index) const override;
    void setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags flags) override;
    QRegion visualRegionForSelection(const QItemSelection& selection) const override;

    void rowsInserted(const QModelIndex& parent, int start, int end) override;
    void dataChanged(const QModelIndex& topLeft,
        const QModelIndex& bottomRight,
        const FluentItemDataRoles& roles = FluentItemDataRoles()) override;
    void reset() override;

    void onThemeUpdated() override;

private:
    struct LayoutBand {
        int firstRow = 0;
        int pastLastRow = 0;
        int top = 0;
        int bottom = 0;
    };

    void applyThemeStyle(bool refreshFonts = true);
    void layoutHeader();
    void updateViewportMargins();
    void refreshAccessibleName();
    void setViewportHovered(bool hovered);
    void invalidateFlowLayout(int firstChangedRow = 0);
    void scheduleLayoutUpdate();
    void syncFluentScrollBar();
    void ensureLayout() const;
    void computeLayoutForRows(const QList<int>& rows, QHash<int, QRect>* rects, QSize* contentSize) const;
    QSize itemSizeForIndex(const QModelIndex& index) const;
    QSize clampedItemSize(const QSize& size) const;
    QRect contentToViewport(const QRect& rect) const;
    QPoint viewportToContent(const QPoint& point) const;
    QStyleOptionViewItem optionForIndex(const QModelIndex& index, const QRect& rect) const;
    int modelRowCount() const;
    QModelIndex indexForRow(int row) const;
    QModelIndex nearestVerticalIndex(int currentRow, int direction) const;
    void applyPointerSelection(const QModelIndex& index, Qt::KeyboardModifiers modifiers);

    void clearModelConnections();
    void connectModelSignals(QAbstractItemModel* model);

    int firstLayoutBandIntersectingY(int contentY) const;
    int rowAt(const QPoint& point) const;
    void rebuildDropIndicatorRects() const;
    int dropIndicatorIndex(const QPoint& point) const;
    QRect dropIndicatorRectForSlot(int slot) const;
    qreal dropIndicatorDistance(const QPoint& point, int slot) const;
    qreal dropTargetHysteresis() const;
    int stabilizedDropIndicatorIndex(const QPoint& point) const;
    void updateDragDisplacement();
    void resetDragReorderFeedback();
    void clearDragAnimations();
    QPixmap renderItemPixmap(int row) const;
    QPixmap renderDragPixmap() const;

    SelectionMode m_selectionMode = SelectionMode::Single;
    Typography::FontRole m_fontRole = Typography::FontRole::Body;

    bool m_borderVisible = true;
    QString m_headerText;
    QString m_placeholderText;
    QString m_autoAccessibleName;
    QLabel* m_headerLabel = nullptr;

    QSize m_defaultItemSize{120, 64};
    QSize m_minimumItemSize{24, 24};
    QSize m_maximumItemSize;
    int m_itemSizeRole = Qt::SizeHintRole;
    int m_hSpacing = 8;
    int m_vSpacing = 8;
    QMargins m_contentMargins{8, 8, 8, 8};

    ::fluent::scrolling::ScrollBar* m_vScrollBar = nullptr;
    ::fluent::scrolling::OverscrollController* m_overscroll = nullptr;
    bool m_viewportHovered = false;
    int m_hoveredRow = -1;
    int m_pressedRow = -1;

    mutable bool m_layoutDirty = true;
    mutable bool m_layoutInProgress = false;
    quint64 m_layoutRevision = 0;
    mutable int m_layoutValidRowCount = 0;
    bool m_layoutUpdatePending = false;
    bool m_painting = false;
    bool m_syncingScrollBar = false;
    mutable QVector<QRect> m_itemRects;
    mutable QVector<LayoutBand> m_layoutBands;
    mutable QSize m_contentSize;

    QList<QMetaObject::Connection> m_modelConnections;

    bool m_canReorderItems = false;
    bool m_isDragging = false;
    bool m_dragPressIntercepted = false;
    QPoint m_dragStartPos;
    QPoint m_dragCurrentPos;
    QPixmap m_dragPixmap;
    int m_dragSourceIndex = -1;
    QList<int> m_dragSourceIndices;
    int m_dropTargetIndex = -1;
    mutable QVector<QRect> m_dropIndicatorRects;
    QHash<int, QPointF> m_dragOffsets;
    QHash<int, QPointF> m_dragStartOffsets;
    QHash<int, QPointF> m_dragTargetOffsets;
    QVariantAnimation* m_dragAnimation = nullptr;
    mutable bool m_paintingWithOffsets = false;
};

} // namespace fluent::collections

#endif // FLOWVIEW_H
