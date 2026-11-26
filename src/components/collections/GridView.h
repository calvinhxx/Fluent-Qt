#ifndef GRIDVIEW_H
#define GRIDVIEW_H

#include "../../compatibility/InputEventCompat.h"
#include <QListView>

#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"
#include "SelectionMode.h"

class QLabel;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QTimer;
class QVariantAnimation;
class QWheelEvent;

namespace fluent::scrolling {
class ScrollBar;
class OverscrollController;
} // namespace fluent::scrolling

namespace fluent::collections {

/**
 * @brief Grid collection view with configurable cell size, spacing, and max columns.
 * Supports Fluent selection visuals, optional header/placeholder text, overscroll, and drag reordering.
 * zh_CN: 支持自定义单元格尺寸、间距和最大列数的网格视图，内置 Fluent 选中样式、标题/占位文本、边界回弹及拖拽重排。
 */
class GridView : public QListView, public FluentElement, public QMLPlus {
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

    /**
     * @brief Whether the control frame border is painted.
     * zh_CN: 是否绘制控件外框边线。
     */
    Q_PROPERTY(bool borderVisible READ borderVisible WRITE setBorderVisible NOTIFY borderVisibleChanged)
    /**
     * @brief Whether the control background is painted.
     * zh_CN: 是否绘制控件背景。
     */
    Q_PROPERTY(bool backgroundVisible READ backgroundVisible WRITE setBackgroundVisible NOTIFY backgroundVisibleChanged)
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
     * @brief Grid cell size used for item layout.
     * zh_CN: 网格条目布局使用的单元格尺寸。
     */
    Q_PROPERTY(QSize cellSize READ cellSize WRITE setCellSize NOTIFY cellSizeChanged)
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
     * @brief Maximum number of columns; zero means unconstrained.
     * zh_CN: 最大列数，0 表示不限制。
     */
    Q_PROPERTY(int maxColumns READ maxColumns WRITE setMaxColumns NOTIFY maxColumnsChanged)

    explicit GridView(QWidget* parent = nullptr);
    ~GridView() override;

    SelectionMode selectionMode() const { return m_selectionMode; }
    void setSelectionMode(SelectionMode mode);

    Typography::FontRole fontRole() const { return m_fontRole; }
    void setFontRole(Typography::FontRole role);

    bool borderVisible() const { return m_borderVisible; }
    bool isBorderVisible() const { return borderVisible(); }
    void setBorderVisible(bool visible);

    bool backgroundVisible() const { return m_backgroundVisible; }
    bool isBackgroundVisible() const { return backgroundVisible(); }
    void setBackgroundVisible(bool visible);

    QString headerText() const { return m_headerText; }
    void setHeaderText(const QString& text);

    QString placeholderText() const { return m_placeholderText; }
    void setPlaceholderText(const QString& text);

    bool viewportHovered() const { return m_viewportHovered; }
    bool isViewportHovered() const { return viewportHovered(); }

    bool canReorderItems() const { return m_canReorderItems; }
    void setCanReorderItems(bool enabled);

    bool isScrollChainingEnabled() const;
    void setScrollChainingEnabled(bool enabled);

    bool isOverscrollEnabled() const;
    void setOverscrollEnabled(bool enabled);

    QSize cellSize() const { return m_cellSize; }
    void setCellSize(const QSize& size);

    int horizontalSpacing() const { return m_hSpacing; }
    void setHorizontalSpacing(int spacing);

    int verticalSpacing() const { return m_vSpacing; }
    void setVerticalSpacing(int spacing);

    int maxColumns() const { return m_maxColumns; }
    void setMaxColumns(int maxCols);

    int selectedIndex() const;
    QList<int> selectedRows() const;
    void setSelectedIndex(int index);

    ::fluent::scrolling::ScrollBar* verticalFluentScrollBar() const;
    void refreshFluentScrollChrome();
    QRect visualRect(const QModelIndex& index) const override;

signals:
    void selectionModeChanged();
    void fontRoleChanged();
    void viewportHoveredChanged();
    void borderVisibleChanged();
    void backgroundVisibleChanged();
    void headerTextChanged();
    void placeholderTextChanged();
    void cellSizeChanged();
    void horizontalSpacingChanged();
    void verticalSpacingChanged();
    void maxColumnsChanged();
    void canReorderItemsChanged();
    void scrollChainingEnabledChanged();
    void overscrollEnabledChanged();
    void itemReordered(int fromIndex, int toIndex);
    /**
     * @brief Convenience signal emitting the clicked row index alongside QAbstractItemView::clicked().
     * zh_CN: 与 QAbstractItemView::clicked() 对应的便捷信号，直接传递被点击的行号。
     */
    void itemClicked(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    int verticalOffset() const override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    void onThemeUpdated() override;

private:
    void applyThemeStyle(bool refreshFonts = true);
    void syncFluentScrollBar();
    void layoutHeader();
    void setViewportHovered(bool hovered);
    void updateViewportMargins();
    void updateGridSize();
    int dropIndicatorIndex(const QPoint& pos) const;
    int stabilizedDropIndicatorIndex(const QPoint& pos) const;
    qreal dropIndicatorDistance(const QPoint& pos, int slot) const;
    qreal dropTargetHysteresis() const;
    void resetDragReorderFeedback();
    void updateDragDisplacement();
    void clearDragAnimations();
    QPixmap renderItemPixmap(int row) const;
    QPixmap renderDragPixmap() const;

    SelectionMode m_selectionMode = SelectionMode::Single;
    Typography::FontRole m_fontRole = Typography::FontRole::Body;

    // --- Container visuals ---
    bool m_borderVisible = true;
    bool m_backgroundVisible = true;
    QString m_headerText;
    QString m_placeholderText;
    QLabel* m_headerLabel = nullptr;

    // --- Grid layout ---
    QSize m_cellSize{112, 112};
    int m_hSpacing = 4;
    int m_vSpacing = 4;
    int m_maxColumns = 0;

    ::fluent::scrolling::ScrollBar* m_vScrollBar = nullptr;
    bool m_viewportHovered = false;

    // --- Drag reorder ---
    bool m_canReorderItems = false;
    bool m_isDragging = false;
    bool m_dragPressIntercepted = false;
    bool m_pressedOnBlank = false;
    QPoint m_dragStartPos;
    QPoint m_dragCurrentPos;
    QPixmap m_dragPixmap;
    int m_dragSourceIndex = -1;
    QList<int> m_dragSourceIndices;
    int m_dropTargetIndex = -1;
    QHash<int, QPointF> m_dragOffsets;
    QHash<int, QPointF> m_dragTargetOffsets;
    QHash<int, QVariantAnimation*> m_dragAnims;
    mutable bool m_paintingWithOffsets = false;

    // --- Overscroll bounce (shared state machine) ---
    ::fluent::scrolling::OverscrollController* m_overscroll = nullptr;
};

} // namespace fluent::collections

#endif // GRIDVIEW_H
