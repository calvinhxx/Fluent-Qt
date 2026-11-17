#ifndef LISTVIEW_H
#define LISTVIEW_H

#include "compatibility/InputEventCompat.h"
#include <QListView>
#include <QHash>
#include <QMetaObject>
#include <QPersistentModelIndex>
#include <QRectF>
#include <functional>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/collections/SelectionMode.h"

class QLabel;
class QItemSelection;
class QItemSelectionModel;
class QPaintEvent;
class QPainter;
class QResizeEvent;
class QShowEvent;
class QTimer;
class QVariantAnimation;
class QWheelEvent;
class QPropertyAnimation;

namespace fluent::scrolling {
class ScrollBar;
class OverscrollController;
} // namespace fluent::scrolling

namespace fluent::collections {

/**
 * @brief Fluent list view with header/footer slots, section grouping, and animated selection indicator.
 * Uses Qt item models and delegates, providing Fluent selection styling, placeholder text, and drag reordering.
 * zh_CN: 支持头尾部区域、分组标题和选中指示器动效的 Fluent 列表视图，提供占位提示、拖拽重排与覆盖式滚动条。
 */
class ListView : public QListView, public FluentElement, public QMLPlus {
    Q_OBJECT

public:
    using SelectionMode = ::fluent::collections::SelectionMode;

    enum class IndicatorMotionDirection { None, Up, Down, Left, Right };
    Q_ENUM(IndicatorMotionDirection)

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
     * @brief Optional model role for trailing secondary text; -1 hides it.
     * zh_CN: 可选的行尾辅助文本角色；-1 表示不显示。
     */
    Q_PROPERTY(int trailingTextRole READ trailingTextRole WRITE setTrailingTextRole NOTIFY trailingTextRoleChanged)
    /**
     * @brief Whether the viewport is currently hovered.
     * zh_CN: 视口当前是否处于悬停状态。
     */
    Q_PROPERTY(bool viewportHovered READ viewportHovered NOTIFY viewportHoveredChanged)
    /**
     * @brief Item layout direction (TopToBottom or LeftToRight).
     * zh_CN: 列表排布方向（纵向 TopToBottom 或横向 LeftToRight）。
     */
    Q_PROPERTY(Flow flow READ flow WRITE setFlow NOTIFY flowChanged)
    /**
     * @brief Animated progress for the selected-item indicator.
     * zh_CN: 选中项指示器的动画进度。
     */
    Q_PROPERTY(qreal selectedIndicatorProgress READ selectedIndicatorProgress NOTIFY selectedIndicatorProgressChanged)
    /**
     * @brief Direction used by selected-indicator motion.
     * zh_CN: 选中指示器动效使用的方向。
     */
    Q_PROPERTY(IndicatorMotionDirection selectedIndicatorMotionDirection
        READ selectedIndicatorMotionDirection
        NOTIFY selectedIndicatorMotionDirectionChanged)
    /**
     * @brief Whether selected-indicator animation is enabled.
     * zh_CN: 是否启用选中指示器动画。
     */
    Q_PROPERTY(bool selectedIndicatorAnimationEnabled
        READ selectedIndicatorAnimationEnabled
        WRITE setSelectedIndicatorAnimationEnabled
        NOTIFY selectedIndicatorAnimationEnabledChanged)
    /**
     * @brief Whether the selected-item indicator is painted.
     * zh_CN: 是否绘制选中项指示器。
     */
    Q_PROPERTY(bool selectionIndicatorVisible
        READ selectionIndicatorVisible
        WRITE setSelectionIndicatorVisible
        NOTIFY selectionIndicatorVisibleChanged)

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
     * @brief Custom widget hosted in the ListView header area.
     * zh_CN: 承载在 ListView 头部区域的自定义控件。
     */
    Q_PROPERTY(QWidget* header READ header WRITE setHeader NOTIFY headerChanged)
    /**
     * @brief Custom widget hosted in the ListView footer area.
     * zh_CN: 承载在 ListView 尾部区域的自定义控件。
     */
    Q_PROPERTY(QWidget* footer READ footer WRITE setFooter NOTIFY footerChanged)
    /**
     * @brief Convenience text displayed in the header area.
     * zh_CN: 显示在头部区域的便捷标题文本。
     */
    Q_PROPERTY(QString headerText READ headerText WRITE setHeaderText NOTIFY headerTextChanged)
    /**
     * @brief Convenience text displayed in the footer area.
     * zh_CN: 显示在尾部区域的便捷文本。
     */
    Q_PROPERTY(QString footerText READ footerText WRITE setFooterText NOTIFY footerTextChanged)
    /**
     * @brief Text shown when the model has no items.
     * zh_CN: 模型为空时显示的占位文本。
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
    Q_PROPERTY(bool scrollChainingEnabled
        READ isScrollChainingEnabled
        WRITE setScrollChainingEnabled
        NOTIFY scrollChainingEnabledChanged)
    /**
     * @brief Whether adjacent items are grouped under section headers via the section key callback.
     * zh_CN: 是否按分组键回调将相邻条目归组并绘制分组标题。
     */
    Q_PROPERTY(bool sectionEnabled READ sectionEnabled WRITE setSectionEnabled NOTIFY sectionEnabledChanged)

    explicit ListView(QWidget* parent = nullptr);
    ~ListView() override;

    void setModel(QAbstractItemModel* model) override;
    void setSelectionModel(QItemSelectionModel* selectionModel) override;

    using Flow = QListView::Flow;
    Flow flow() const;
    void setFlow(Flow flow);

    SelectionMode selectionMode() const { return m_selectionMode; }
    void setSelectionMode(SelectionMode mode);

    Typography::FontRole fontRole() const { return m_fontRole; }
    void setFontRole(Typography::FontRole role);
    int trailingTextRole() const { return m_trailingTextRole; }
    /**
     * @brief Sets the role used by the default delegate for trailing text; custom delegates handle their own layout.
     * zh_CN: 设置默认代理使用的行尾文本角色；自定义代理需自行绘制。
     */
    void setTrailingTextRole(int role);

    bool borderVisible() const { return m_borderVisible; }
    bool isBorderVisible() const { return borderVisible(); }
    void setBorderVisible(bool visible);

    bool backgroundVisible() const { return m_backgroundVisible; }
    bool isBackgroundVisible() const { return backgroundVisible(); }
    void setBackgroundVisible(bool visible);

    QWidget* header() const { return m_header; }
    void setHeader(QWidget* widget);

    QWidget* footer() const { return m_footer; }
    void setFooter(QWidget* widget);

    QString headerText() const { return m_headerText; }
    void setHeaderText(const QString& text);

    QString footerText() const { return m_footerText; }
    void setFooterText(const QString& text);

    QString placeholderText() const { return m_placeholderText; }
    void setPlaceholderText(const QString& text);

    bool viewportHovered() const { return m_viewportHovered; }
    bool isViewportHovered() const { return viewportHovered(); }

    bool canReorderItems() const { return m_canReorderItems; }
    void setCanReorderItems(bool enabled);

    bool isScrollChainingEnabled() const;
    void setScrollChainingEnabled(bool enabled);

    using SectionKeyFunc = std::function<QString(int row)>;
    bool sectionEnabled() const { return m_sectionEnabled; }
    bool isSectionEnabled() const { return sectionEnabled(); }
    void setSectionEnabled(bool enabled);
    /**
     * @brief Sets the callback returning a section key per row; adjacent rows with the same key share a section header.
     * zh_CN: 设置分组键回调；相邻行返回相同键时归为同一组，并以该键作为分组标题绘制。
     */
    void setSectionKeyFunction(SectionKeyFunc func);

    int selectedIndex() const;
    QList<int> selectedRows() const;
    void setSelectedIndex(int index);

    qreal selectedIndicatorProgress() const { return m_selectedIndicatorProgress; }
    IndicatorMotionDirection selectedIndicatorMotionDirection() const { return m_selectedIndicatorMotionDirection; }
    /**
     * @brief Whether the selection indicator animates between rows; disabling snaps immediately to the target rect.
     * zh_CN: 是否启用选中指示器过渡动画；关闭后切换选中项会立即移至目标位置。
     */
    bool selectedIndicatorAnimationEnabled() const { return m_selectedIndicatorAnimationEnabled; }
    bool isSelectedIndicatorAnimationEnabled() const { return selectedIndicatorAnimationEnabled(); }
    void setSelectedIndicatorAnimationEnabled(bool enabled);
    /**
     * @brief Whether the selected-item indicator is painted.
     * zh_CN: 是否绘制选中项指示器。
     */
    bool selectionIndicatorVisible() const { return m_selectionIndicatorVisible; }
    /**
     * @brief Shows or hides the selection indicator without affecting selection behavior.
     * zh_CN: 控制是否绘制选中指示器，不影响实际选中逻辑。
     */
    void setSelectionIndicatorVisible(bool visible);
    /**
     * @brief Returns the current selection indicator rectangle in viewport coordinates, or empty if none is visible.
     * zh_CN: 返回当前选中指示器在视口坐标系中的矩形区域；无可见选中项时返回空矩形。
     */
    QRectF selectedIndicatorRect() const;
    /**
     * @brief Returns the selection indicator rectangle at a normalized animation progress in [0, 1].
     * zh_CN: 返回指定归一化动画进度 [0, 1] 下的选中指示器矩形区域。
     */
    QRectF selectedIndicatorRect(qreal progress) const;
    /**
     * @brief Returns the selection indicator rectangle for a specific row (used in multi-selection mode).
     * zh_CN: 返回指定行的选中指示器矩形区域（用于多选模式下的独立指示器）。
     */
    QRectF selectedIndicatorRectForRow(int row) const;
    QRectF selectedIndicatorRectForRow(int row, qreal progress) const;

    ::fluent::scrolling::ScrollBar* verticalFluentScrollBar() const;
    ::fluent::scrolling::ScrollBar* horizontalFluentScrollBar() const;

    /**
     * @brief Hides native scrollbars and refreshes Fluent overlay scrollbars.
     * Call again after show if a platform style (such as inside QComboBox popups) restores native bars.
     * zh_CN: 隐藏原生滚动条并刷新 Fluent 覆盖式滚动条（若在 QComboBox 弹层等场景中 show 后被系统样式重置，可再次调用）。
     */
    void refreshFluentScrollChrome();

signals:
    void selectionModeChanged();
    void fontRoleChanged();
    void trailingTextRoleChanged();
    void viewportHoveredChanged();
    void flowChanged();
    void borderVisibleChanged();
    void backgroundVisibleChanged();
    void headerChanged();
    void footerChanged();
    void headerTextChanged();
    void footerTextChanged();
    void placeholderTextChanged();
    void canReorderItemsChanged();
    void scrollChainingEnabledChanged();
    void sectionEnabledChanged();
    void selectedIndicatorProgressChanged();
    void selectedIndicatorMotionDirectionChanged();
    void selectedIndicatorAnimationEnabledChanged();
    void selectionIndicatorVisibleChanged();
    /**
     * @brief Convenience signal emitting the clicked row index alongside QAbstractItemView::clicked().
     * zh_CN: 与 QAbstractItemView::clicked() 对应的便捷信号，直接传递被点击的行号。
     */
    void itemClicked(int index);
    void itemReordered(int fromRow, int toRow);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;
    void scrollContentsBy(int dx, int dy) override;
    int verticalOffset() const override;
    int horizontalOffset() const override;
    QRect visualRect(const QModelIndex& index) const override;

    void onThemeUpdated() override;

    friend class SectionProxyDelegate;

private:
    void applyThemeStyle(bool refreshFonts = true);
    void applyPointerSelection(const QModelIndex& index, QMouseEvent* event);
    /** 压制原生条 + 同步 range/pageStep + 定位 + 显隐 Fluent 纵向滚动条 */
    void syncFluentScrollBar();
    void syncFluentHScrollBar();
    void layoutHeader();
    void layoutFooter();
    void setViewportHovered(bool hovered);
    void updatePressedHoverRow(int row);
    void paintPressedHoverFeedback(QPainter& painter) const;
    void updateViewportMargins();
    void installSectionProxy();
    bool isPointInSectionHeader(const QPoint& viewportPos) const;
    int dropIndicatorRow(const QPoint& pos) const;
    void updateDragDisplacement();
    void clearDragAnimations();
    QPixmap renderItemPixmap(int row) const;

    void connectSelectedIndicatorModel(QAbstractItemModel* model);
    void disconnectSelectedIndicatorModel();
    void clearSelectedIndicatorState();
    void clearMultiSelectedIndicatorState();
    void updateSelectedIndicatorFromSelection(const QModelIndex& previous = QModelIndex());
    void updateSelectedIndicatorForIndex(const QModelIndex& current, const QModelIndex& previous);
    QModelIndex normalizedSelectedModelIndex() const;
    bool isSelectedIndicatorEndpointUsable(const QModelIndex& index) const;
    bool usesMovingSelectedIndicator() const;
    bool usesRevealSelectedIndicators() const;
    QRectF selectedIndicatorBaseRect(const QModelIndex& index) const;
    QRectF revealedSelectedIndicatorRect(const QRectF& baseRect, qreal progress) const;
    QRectF interpolatedSelectedIndicatorRect(const QRectF& previous, const QRectF& target, qreal progress) const;
    IndicatorMotionDirection classifySelectedIndicatorDirection(const QModelIndex& previous,
        const QModelIndex& current) const;
    void startSelectedIndicatorAnimation(bool animated);
    void syncMultiSelectedIndicators(const QItemSelection& selected, const QItemSelection& deselected);
    void startMultiSelectedIndicatorReveal(const QModelIndex& index);
    qreal multiSelectedIndicatorProgress(const QModelIndex& index) const;
    void setSelectedIndicatorProgress(qreal progress);
    void setSelectedIndicatorMotionDirection(IndicatorMotionDirection direction);
    void refreshSelectedIndicatorGeometry(bool snapToTarget);
    void paintSelectedIndicator(QPainter& painter) const;
    void paintIndicatorRect(QPainter& painter, const QRectF& indicatorRect, qreal opacity = 1.0) const;

    SelectionMode m_selectionMode = SelectionMode::Single;
    Typography::FontRole m_fontRole = Typography::FontRole::Body;
    int m_trailingTextRole = -1;

    // --- Container visuals ---
    bool m_borderVisible = true;
    bool m_backgroundVisible = true;
    QString m_headerText;
    QString m_footerText;
    QString m_placeholderText;
    QWidget* m_header = nullptr;
    QWidget* m_footer = nullptr;
    bool m_ownsHeader =
        false; // QLabel created internally via setHeaderText. zh_CN: 内部通过 setHeaderText 创建的 QLabel。
    bool m_ownsFooter =
        false; // QLabel created internally via setFooterText. zh_CN: 内部通过 setFooterText 创建的 QLabel。

    ::fluent::scrolling::ScrollBar* m_vScrollBar = nullptr;
    ::fluent::scrolling::ScrollBar* m_hScrollBar = nullptr;
    ::fluent::scrolling::OverscrollController* m_overscroll = nullptr;
    bool m_revealSelectionOnShow = true;
    bool m_viewportHovered = false;

    // --- Drag reorder ---
    bool m_canReorderItems = false;
    bool m_isDragging = false;
    int m_pressedRow = -1;
    int m_pressedHoverRow = -1;
    int m_dragSourceRow = -1;
    int m_dropTargetRow = -1; // Drop indicator position. zh_CN: 拖拽指示线位置。
    QPoint m_dragStartPos;
    QPoint m_dragCurrentPos;
    QPixmap m_dragPixmap;
    QHash<int, qreal> m_dragOffsets;            // row → current Y offset in px. zh_CN: row → 当前 Y 位移。
    QHash<int, QVariantAnimation*> m_dragAnims; // row → displacement animation. zh_CN: row → 位移动画。
    mutable bool m_paintingWithOffsets = false;

    // --- Section ---
    bool m_sectionEnabled = false;
    SectionKeyFunc m_sectionKeyFunc;
    QAbstractItemDelegate* m_sectionProxy = nullptr;
    QAbstractItemDelegate* m_userDelegate = nullptr;

    // --- Selected indicator motion ---
    QMetaObject::Connection m_indicatorModelAboutToResetConnection;
    QMetaObject::Connection m_indicatorModelResetConnection;
    QMetaObject::Connection m_indicatorRowsAboutToBeRemovedConnection;
    QMetaObject::Connection m_indicatorRowsMovedConnection;
    QMetaObject::Connection m_indicatorLayoutChangedConnection;
    QPersistentModelIndex m_previousIndicatorIndex;
    QPersistentModelIndex m_currentIndicatorIndex;
    QVariantAnimation* m_selectedIndicatorAnimation = nullptr;
    QHash<QPersistentModelIndex, QVariantAnimation*> m_multiIndicatorAnimations;
    qreal m_selectedIndicatorProgress = 1.0;
    IndicatorMotionDirection m_selectedIndicatorMotionDirection = IndicatorMotionDirection::None;
    bool m_selectedIndicatorAnimationEnabled = true;
    bool m_selectionIndicatorVisible = true;
};

} // namespace fluent::collections

#endif // LISTVIEW_H
