#ifndef TREEVIEW_H
#define TREEVIEW_H

#include <QTreeView>
#include <QPersistentModelIndex>
#include <QHash>
#include <QRectF>
#include <QStandardItemModel>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QLabel;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QVariantAnimation;
class QWheelEvent;
class QTimer;

namespace fluent::scrolling { class ScrollBar; class OverscrollController; }

namespace fluent::collections {

/**
 * @brief Fluent tree collection view with hierarchy-aware selection visuals.
 * zh_CN: 具备层级感知选中视觉的 Fluent 树形集合视图。
 *
 * TreeView builds on QTreeView while coordinating checkbox selection, parent/child
 * state propagation, indicator motion, header text, placeholder text, and reordering.
 * zh_CN: TreeView 基于 QTreeView，并统一处理 checkbox 选中、父子状态传播、
 * 指示器动效、标题、占位文本和拖拽重排。
 */
class TreeView : public QTreeView, public FluentElement, public QMLPlus {
    Q_OBJECT

public:
    enum class TreeSelectionMode {
        None,
        Single,
        Multiple,
        Extended
    };
    Q_ENUM(TreeSelectionMode)

    enum class IndicatorVerticalDirection {
        None,
        Up,
        Down
    };
    Q_ENUM(IndicatorVerticalDirection)

    enum class IndicatorHierarchyTransition {
        None,
        SameLevel,
        Inward,
        Outward
    };
    Q_ENUM(IndicatorHierarchyTransition)

    /**
     * @brief Style parameters used by the moving selection indicator overlay.
     * zh_CN: 移动选中指示器覆盖层使用的样式参数。
     */
    struct SelectionIndicatorStyle {
        qreal inset = 6.0;
        qreal width = 3.0;
        qreal height = 16.0;
        int insetRole = -1;
    };

    /**
     * @brief Selection mode used by the collection view.
     * zh_CN: 集合视图使用的选择模式。
     */
    Q_PROPERTY(TreeSelectionMode selectionMode READ selectionMode WRITE setSelectionMode NOTIFY selectionModeChanged)
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
     * @brief Animated progress of the tree selection indicator.
     * zh_CN: 树选中指示器的动画进度。
     */
    Q_PROPERTY(qreal indicatorMotionProgress READ indicatorMotionProgress NOTIFY indicatorMotionProgressChanged)
    /**
     * @brief Vertical direction used by tree indicator motion.
     * zh_CN: 树指示器动效使用的垂直方向。
     */
    Q_PROPERTY(IndicatorVerticalDirection indicatorMotionDirection READ indicatorMotionDirection NOTIFY indicatorMotionDirectionChanged)
    /**
     * @brief Hierarchy transition mode used by tree indicator motion.
     * zh_CN: 树指示器动效使用的层级转场模式。
     */
    Q_PROPERTY(IndicatorHierarchyTransition indicatorHierarchyTransition READ indicatorHierarchyTransition NOTIFY indicatorHierarchyTransitionChanged)
    /**
     * @brief Whether tree indicator motion animation is enabled.
     * zh_CN: 是否启用树指示器动效动画。
     */
    Q_PROPERTY(bool indicatorMotionAnimationEnabled READ isIndicatorMotionAnimationEnabled WRITE setIndicatorMotionAnimationEnabled NOTIFY indicatorMotionAnimationEnabledChanged)
    /**
     * @brief Whether the Fluent horizontal scroll overlay is allowed to appear.
     * zh_CN: 控制 Fluent 水平滚动条覆盖层是否允许显示。
     */
    Q_PROPERTY(bool horizontalFluentScrollBarEnabled READ isHorizontalFluentScrollBarEnabled WRITE setHorizontalFluentScrollBarEnabled)

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
     * @brief Text shown when the tree model has no rows to present.
     * zh_CN: 树形 model 没有可展示行时显示的占位文本。
     */
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText NOTIFY placeholderTextChanged)

    /**
     * @brief Whether drag reordering is enabled.
     * zh_CN: 是否启用拖拽重排。
     */
    Q_PROPERTY(bool canReorderItems READ canReorderItems WRITE setCanReorderItems NOTIFY canReorderItemsChanged)
    /**
     * @brief Whether boundary wheel input may continue to an enclosing scroller.
     * zh_CN: 边界滚轮输入是否允许继续传递给外层滚动容器。
     */
    Q_PROPERTY(bool scrollChainingEnabled READ isScrollChainingEnabled WRITE setScrollChainingEnabled NOTIFY scrollChainingEnabledChanged)
    /**
     * @brief Whether the view shows an elastic overscroll/bounce at the scroll boundary.
     * Enabled by default; disable for chrome (e.g. a navigation pane) that should stop
     * cleanly at the edge so reversing the wheel scrolls again immediately.
     * zh_CN: 滚动到边界时是否显示弹性回弹。默认开启；用于不希望回弹的 chrome（如导航窗格）时关闭，
     * 使其在边界干脆停住、反向滚动立即响应。
     */
    Q_PROPERTY(bool overscrollEnabled READ isOverscrollEnabled WRITE setOverscrollEnabled NOTIFY overscrollEnabledChanged)

    explicit TreeView(QWidget* parent = nullptr);
    ~TreeView() override = default;

    QModelIndex indexAt(const QPoint& point) const override;
    void setModel(QAbstractItemModel* model) override;
    void setSelectionModel(QItemSelectionModel* selectionModel) override;

    // --- Selection ---
    TreeSelectionMode selectionMode() const { return m_selectionMode; }
    void setSelectionMode(TreeSelectionMode mode);

    // --- Appearance ---
    QString fontRole() const { return m_fontRole; }
    void setFontRole(const QString& role);

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

    // --- Drag reorder ---
    bool canReorderItems() const { return m_canReorderItems; }
    void setCanReorderItems(bool enabled);

    bool isScrollChainingEnabled() const;
    void setScrollChainingEnabled(bool enabled);

    bool isOverscrollEnabled() const;
    void setOverscrollEnabled(bool enabled);

    // --- Tree API ---
    void expandAll();
    void collapseAll();
    void toggleExpanded(const QModelIndex& index);

    // --- Selection API ---
    QModelIndex selectedItem() const;
    QModelIndexList selectedItems() const;
    void setSelectedItem(const QModelIndex& index);

    // --- Selected indicator motion API ---
    qreal indicatorMotionProgress() const { return m_indicatorMotionProgress; }
    IndicatorVerticalDirection indicatorMotionDirection() const { return m_indicatorMotionDirection; }
    IndicatorHierarchyTransition indicatorHierarchyTransition() const { return m_indicatorHierarchyTransition; }
    bool isIndicatorMotionAnimationEnabled() const { return m_indicatorMotionAnimationEnabled; }
    void setIndicatorMotionAnimationEnabled(bool enabled);
    qreal selectedIndicatorProgress(const QModelIndex& index) const;
    bool isIndicatorMotionActiveForIndex(const QModelIndex& index) const;
    QModelIndex indicatorMotionPreviousIndex() const;
    QModelIndex indicatorMotionCurrentIndex() const;

    // --- Selected indicator pill rendering ---
    // zh_CN: 选中指示器药丸的绘制开关与样式参数。
    /**
     * @brief Returns the current selected indicator geometry in viewport coordinates.
     * zh_CN: 返回当前选中指示器在 viewport 坐标系中的几何区域。
     */
    QRectF selectedIndicatorRect() const;
    /**
     * @brief Returns selected indicator geometry for a normalized transition progress.
     * zh_CN: 返回指定归一化过渡进度下的选中指示器几何区域。
     */
    QRectF selectedIndicatorRect(qreal progress) const;
    /**
     * @brief Returns the style used by the selected indicator overlay.
     * zh_CN: 返回选中指示器覆盖层使用的样式。
     */
    SelectionIndicatorStyle selectionIndicatorStyle() const { return m_selectionIndicatorStyle; }
    /**
     * @brief Sets the style used by the selected indicator overlay.
     * zh_CN: 设置选中指示器覆盖层使用的样式。
     */
    void setSelectionIndicatorStyle(const SelectionIndicatorStyle& style);
    bool selectionIndicatorVisible() const { return m_selectionIndicatorVisible; }
    void setSelectionIndicatorVisible(bool visible);
    qreal selectionIndicatorInset() const { return m_selectionIndicatorStyle.inset; }
    void setSelectionIndicatorInset(qreal inset);

    ::fluent::scrolling::ScrollBar* verticalFluentScrollBar() const;
    ::fluent::scrolling::ScrollBar* horizontalFluentScrollBar() const;
    /**
     * @brief Returns whether the Fluent horizontal scroll overlay may be shown.
     * zh_CN: 返回 Fluent 水平滚动条覆盖层是否允许显示。
     */
    bool isHorizontalFluentScrollBarEnabled() const { return m_horizontalFluentScrollBarEnabled; }
    /**
     * @brief Enables or disables the Fluent horizontal scroll overlay.
     * zh_CN: 启用或禁用 Fluent 水平滚动条覆盖层。
     */
    void setHorizontalFluentScrollBarEnabled(bool enabled);

    void refreshFluentScrollChrome();

signals:
    void selectionModeChanged();
    void fontRoleChanged();
    void viewportHoveredChanged();
    void borderVisibleChanged();
    void backgroundVisibleChanged();
    void headerTextChanged();
    void placeholderTextChanged();
    void itemPressed(const QModelIndex& index);
    void itemClicked(const QModelIndex& index);
    void canReorderItemsChanged();
    void scrollChainingEnabledChanged();
    void overscrollEnabledChanged();
    void itemReordered(const QModelIndex& srcParent, int srcRow,
                       const QModelIndex& dstParent, int dstRow);
    void indicatorMotionProgressChanged();
    void indicatorMotionDirectionChanged();
    void indicatorHierarchyTransitionChanged();
    void indicatorMotionAnimationEnabledChanged();
    void selectionIndicatorStyleChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;
    int verticalOffset() const override;
    void drawRow(QPainter* painter, const QStyleOptionViewItem& options, const QModelIndex& index) const override;
    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override;

    void onThemeUpdated() override;

private:
    void applyThemeStyle();
    void syncFluentScrollBar();
    void syncFluentHScrollBar();
    void layoutHeader();
    void setViewportHovered(bool hovered);
    void updateViewportMargins();

    void connectIndicatorMotionModel(QAbstractItemModel* model);
    void disconnectIndicatorMotionModel();
    void clearIndicatorMotionState();
    void updateIndicatorMotionForIndex(const QModelIndex& current, const QModelIndex& previous);
    void startIndicatorMotionAnimation(bool animated);
    bool isIndicatorEndpointUsable(const QModelIndex& index) const;
    int indicatorHierarchyDepth(const QModelIndex& index) const;
    IndicatorVerticalDirection classifyIndicatorVerticalDirection(const QModelIndex& previous, const QModelIndex& current) const;
    IndicatorHierarchyTransition classifyIndicatorHierarchyTransition(const QModelIndex& previous, const QModelIndex& current) const;
    void setIndicatorMotionProgress(qreal progress);
    void setIndicatorMotionDirection(IndicatorVerticalDirection direction);
    void setIndicatorHierarchyTransition(IndicatorHierarchyTransition transition);

    // --- Selected indicator pill geometry / painting ---
    QRectF selectedIndicatorBaseRect(const QModelIndex& index) const;
    QRectF currentSelectedIndicatorRect() const;
    void paintSelectedIndicator(QPainter& painter) const;
    // Minimal repaint regions for the per-frame animation ticks, so a click
    // doesn't re-rasterize the whole viewport every frame (matters on Windows'
    // software raster). zh_CN: 动画每帧的最小重绘区域，避免每帧整块 viewport 重绘
    // （对 Windows 软件光栅尤为关键）。
    QRect indicatorMotionDirtyRect() const;
    void syncCheckStatesWithSelection(const QItemSelection& selected, const QItemSelection& deselected);
    bool shouldSyncCheckStateWithSelection(const QModelIndex& index) const;
    void applyCheckStateToSubtree(const QModelIndex& index, Qt::CheckState state);
    void updateAncestorCheckStates(const QModelIndex& index);
    Qt::CheckState aggregateChildCheckState(const QModelIndex& parent) const;
    void setCheckStateAndSelection(const QModelIndex& index, Qt::CheckState state);
    void clearExpandRevealState();
    void completeActiveExpandReveal();
    qreal computeSubtreeHeight(const QModelIndex& parent) const;
    void startExpandReveal(const QModelIndex& parent);
    void startCollapseReveal(const QModelIndex& parent);
    void finalizeDeferredCollapse();

    // Drag helpers — file-manager style
    void computeDropTarget(const QPoint& pos);
    bool isDescendantOf(const QModelIndex& candidate, const QModelIndex& ancestor) const;
    QPixmap renderRowPixmap(const QModelIndex& index) const;

    TreeSelectionMode m_selectionMode = TreeSelectionMode::Single;
    QString m_fontRole;

    // --- Container visuals ---
    bool m_borderVisible = true;
    bool m_backgroundVisible = true;
    QString m_headerText;
    QString m_placeholderText;
    QLabel* m_headerLabel = nullptr;

    ::fluent::scrolling::ScrollBar* m_vScrollBar = nullptr;
    ::fluent::scrolling::ScrollBar* m_hScrollBar = nullptr;
    ::fluent::scrolling::OverscrollController* m_overscroll = nullptr;
    bool m_horizontalFluentScrollBarEnabled = true;
    bool m_viewportHovered = false;

    // --- Selected indicator motion ---
    QMetaObject::Connection m_indicatorModelAboutToResetConnection;
    QMetaObject::Connection m_indicatorModelResetConnection;
    QMetaObject::Connection m_indicatorRowsAboutToBeRemovedConnection;
    QPersistentModelIndex m_previousIndicatorIndex;
    QPersistentModelIndex m_currentIndicatorIndex;
    QPersistentModelIndex m_activeIndicatorIndex;
    QVariantAnimation* m_indicatorMotionAnim = nullptr;
    qreal m_indicatorMotionProgress = 1.0;
    IndicatorVerticalDirection m_indicatorMotionDirection = IndicatorVerticalDirection::None;
    IndicatorHierarchyTransition m_indicatorHierarchyTransition = IndicatorHierarchyTransition::None;
    bool m_indicatorMotionAnimationEnabled = true;
    bool m_syncingCheckStateWithSelection = false;

    // --- Selected indicator pill rendering ---
    bool m_selectionIndicatorVisible = false;
    SelectionIndicatorStyle m_selectionIndicatorStyle;

    // --- Drag reorder (file-manager style) ---
    enum class DropMode { None, Between, OnItem };
    bool m_canReorderItems = false;
    bool m_isDragging = false;
    QPersistentModelIndex m_dragSourceIndex;
    QPoint m_dragStartPos;
    QPoint m_dragCurrentPos;
    QPixmap m_dragPixmap;
    DropMode m_dropMode = DropMode::None;
    QPersistentModelIndex m_dropTargetParent;  // parent for Between, unused for OnItem
    int m_dropTargetRow = -1;                  // row index for Between, -1 for OnItem
    QPersistentModelIndex m_dropOnIndex;       // target item for OnItem (re-parent)

    // --- Expand animation ---
    QVariantAnimation* m_expandRevealAnim = nullptr;
    QPersistentModelIndex m_animParent;
    bool m_animEnabled = true;
    bool m_animExpanding = true;   // true=expand, false=collapse
    qreal m_animSubtreeHeight = 0.0;       // pixel height of the animating subtree
    bool m_pendingCollapseFinalize = false; // deferred collapse: actually collapse on finish

public:
    /** Query chevron rotation progress for delegate painting. 0=right, 1=down. */
    qreal chevronRotation(const QModelIndex& index) const;
};

using TreeSelectionMode = TreeView::TreeSelectionMode;

} // namespace fluent::collections

#endif // TREEVIEW_H
