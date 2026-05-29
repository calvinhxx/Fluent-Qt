#ifndef GRIDVIEW_H
#define GRIDVIEW_H

#include <QListView>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QLabel;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QTimer;
class QVariantAnimation;
class QWheelEvent;

namespace fluent::scrolling { class ScrollBar; }

namespace fluent::collections {

/**
 * @brief Fluent grid collection view with configurable cell metrics.
 * zh_CN: 支持配置单元格尺寸的 Fluent 网格集合视图。
 *
 * GridView uses QListView grid behavior while adding Fluent selection chrome,
 * header and placeholder text, drag reordering, and column/spacing constraints.
 * zh_CN: GridView 使用 QListView 的网格能力，并增加 Fluent 选中外观、标题与占位文本、
 * 拖拽重排以及列数/间距约束。
 */
class GridView : public QListView, public FluentElement, public QMLPlus {
    Q_OBJECT

public:
    enum class GridSelectionMode {
        None,
        Single,
        Multiple,
        Extended
    };
    Q_ENUM(GridSelectionMode)

    /**
     * @brief Selection mode used by the collection view.
     * zh_CN: 集合视图使用的选择模式。
     */
    Q_PROPERTY(GridSelectionMode selectionMode READ selectionMode WRITE setSelectionMode NOTIFY selectionModeChanged)
    /**
     * @brief Fluent typography role used for text rendering.
     * zh_CN: 文本绘制使用的 Fluent 排版角色。
     */
    Q_PROPERTY(QString fontRole READ fontRole WRITE setFontRole NOTIFY fontRoleChanged)
    /**
     * @brief Whether the item-view viewport is currently hovered.
     * zh_CN: item-view viewport 当前是否处于悬停状态。
     */
    Q_PROPERTY(bool viewportHovered READ viewportHovered NOTIFY viewportHoveredChanged)

    /**
     * @brief Whether drag reordering is enabled.
     * zh_CN: 是否启用拖拽重排。
     */
    Q_PROPERTY(bool canReorderItems READ canReorderItems WRITE setCanReorderItems NOTIFY canReorderItemsChanged)

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
     * @brief Text shown when the grid model has no rows to present.
     * zh_CN: 网格 model 没有可展示行时显示的占位文本。
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
    ~GridView() override = default;

    // --- Selection ---
    GridSelectionMode selectionMode() const { return m_selectionMode; }
    void setSelectionMode(GridSelectionMode mode);

    // --- Appearance ---
    QString fontRole() const { return m_fontRole; }
    void setFontRole(const QString& role);

    bool borderVisible() const { return m_borderVisible; }
    bool isBorderVisible() const { return borderVisible(); }
    void setBorderVisible(bool visible);

    QString headerText() const { return m_headerText; }
    void setHeaderText(const QString& text);

    QString placeholderText() const { return m_placeholderText; }
    void setPlaceholderText(const QString& text);

    bool viewportHovered() const { return m_viewportHovered; }
    bool isViewportHovered() const { return viewportHovered(); }

    // --- Drag reorder ---
    bool canReorderItems() const { return m_canReorderItems; }
    void setCanReorderItems(bool enabled);

    // --- Grid layout ---
    QSize cellSize() const { return m_cellSize; }
    void setCellSize(const QSize& size);

    int horizontalSpacing() const { return m_hSpacing; }
    void setHorizontalSpacing(int spacing);

    int verticalSpacing() const { return m_vSpacing; }
    void setVerticalSpacing(int spacing);

    int maxColumns() const { return m_maxColumns; }
    void setMaxColumns(int maxCols);

    // --- Selection API ---
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
    void headerTextChanged();
    void placeholderTextChanged();
    void cellSizeChanged();
    void horizontalSpacingChanged();
    void verticalSpacingChanged();
    void maxColumnsChanged();
    void canReorderItemsChanged();
    void itemReordered(int fromIndex, int toIndex);
    void itemClicked(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    int verticalOffset() const override;

    // Drag-reorder (custom mouse handling)
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    void onThemeUpdated() override;

private:
    void applyThemeStyle();
    void syncFluentScrollBar();
    void layoutHeader();
    void setViewportHovered(bool hovered);
    void updateViewportMargins();
    void updateGridSize();
    void startBounceBack();
    int dropIndicatorIndex(const QPoint& pos) const;
    int stabilizedDropIndicatorIndex(const QPoint& pos) const;
    qreal dropIndicatorDistance(const QPoint& pos, int slot) const;
    qreal dropTargetHysteresis() const;
    void resetDragReorderFeedback();
    void updateDragDisplacement();
    void clearDragAnimations();
    QPixmap renderItemPixmap(int row) const;
    QPixmap renderDragPixmap() const;

    GridSelectionMode m_selectionMode = GridSelectionMode::Single;
    QString m_fontRole;

    // --- Container visuals ---
    bool m_borderVisible = true;
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
    int  m_dragSourceIndex = -1;
    QList<int> m_dragSourceIndices;
    int  m_dropTargetIndex = -1;
    QHash<int, QPointF>            m_dragOffsets;
    QHash<int, QPointF>            m_dragTargetOffsets;
    QHash<int, QVariantAnimation*> m_dragAnims;
    mutable bool m_paintingWithOffsets = false;

    // --- Overscroll bounce ---
    qreal m_overscrollY = 0.0;
    QVariantAnimation* m_bounceAnim = nullptr;
    QTimer* m_bounceTimer = nullptr;
};

using GridSelectionMode = GridView::GridSelectionMode;

} // namespace fluent::collections

#endif // GRIDVIEW_H
