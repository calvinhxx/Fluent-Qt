#ifndef LISTVIEW_H
#define LISTVIEW_H

#include <QListView>
#include <QHash>
#include <QMetaObject>
#include <QPersistentModelIndex>
#include <QRectF>
#include <functional>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

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

namespace fluent::scrolling { class ScrollBar; }

namespace fluent::collections {

/**
 * @brief Fluent list collection view with header, footer, sections, and indicator motion.
 * zh_CN: 支持头尾部、分组和指示器动效的 Fluent 列表集合视图。
 *
 * ListView keeps model and delegate ownership with callers while adding reusable
 * Fluent chrome for selection, placeholder content, drag reordering, and scroll bars.
 * zh_CN: ListView 将 model/delegate 所有权保留给调用方，同时补充选中态、占位内容、
 * 拖拽重排和滚动条等可复用 Fluent 外观。
 */
class ListView : public QListView, public FluentElement, public QMLPlus {
    Q_OBJECT

public:
    enum class ListSelectionMode {
        None,
        Single,
        Multiple,
        Extended
    };
    Q_ENUM(ListSelectionMode)

    enum class IndicatorMotionDirection {
        None,
        Up,
        Down,
        Left,
        Right
    };
    Q_ENUM(IndicatorMotionDirection)

    /**
     * @brief Selection mode used by the collection view.
     * zh_CN: 集合视图使用的选择模式。
     */
    Q_PROPERTY(ListSelectionMode selectionMode READ selectionMode WRITE setSelectionMode NOTIFY selectionModeChanged)
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
     * @brief List flow direction used by the underlying item view.
     * zh_CN: 底层 item view 使用的列表流向。
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
    Q_PROPERTY(IndicatorMotionDirection selectedIndicatorMotionDirection READ selectedIndicatorMotionDirection NOTIFY selectedIndicatorMotionDirectionChanged)
    /**
     * @brief Whether selected-indicator animation is enabled.
     * zh_CN: 是否启用选中指示器动画。
     */
    Q_PROPERTY(bool selectedIndicatorAnimationEnabled READ selectedIndicatorAnimationEnabled WRITE setSelectedIndicatorAnimationEnabled NOTIFY selectedIndicatorAnimationEnabledChanged)

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
     * @brief Text shown when the model has no rows to present.
     * zh_CN: model 没有可展示行时显示的占位文本。
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
     * @brief Whether section headers are enabled for grouped rows.
     * zh_CN: 是否为分组行启用 section header。
     */
    Q_PROPERTY(bool sectionEnabled READ sectionEnabled WRITE setSectionEnabled NOTIFY sectionEnabledChanged)

    explicit ListView(QWidget* parent = nullptr);
    ~ListView() override;

    void setModel(QAbstractItemModel* model) override;
    void setSelectionModel(QItemSelectionModel* selectionModel) override;

    // --- Flow ---
    using Flow = QListView::Flow;  // TopToBottom or LeftToRight
    Flow flow() const;
    void setFlow(Flow flow);

    // --- Selection ---
    ListSelectionMode selectionMode() const { return m_selectionMode; }
    void setSelectionMode(ListSelectionMode mode);

    // --- Appearance ---
    QString fontRole() const { return m_fontRole; }
    void setFontRole(const QString& role);

    bool borderVisible() const { return m_borderVisible; }
    bool isBorderVisible() const { return borderVisible(); }
    void setBorderVisible(bool visible);

    bool backgroundVisible() const { return m_backgroundVisible; }
    bool isBackgroundVisible() const { return backgroundVisible(); }
    void setBackgroundVisible(bool visible);

    // --- Header / Footer (custom widget API) ---
    QWidget* header() const { return m_header; }
    void setHeader(QWidget* widget);

    QWidget* footer() const { return m_footer; }
    void setFooter(QWidget* widget);

    // --- Header / Footer (convenience text API) ---
    QString headerText() const { return m_headerText; }
    void setHeaderText(const QString& text);

    QString footerText() const { return m_footerText; }
    void setFooterText(const QString& text);

    QString placeholderText() const { return m_placeholderText; }
    void setPlaceholderText(const QString& text);

    bool viewportHovered() const { return m_viewportHovered; }
    bool isViewportHovered() const { return viewportHovered(); }

    // --- Drag reorder ---
    bool canReorderItems() const { return m_canReorderItems; }
    void setCanReorderItems(bool enabled);

    bool isScrollChainingEnabled() const { return m_scrollChainingEnabled; }
    void setScrollChainingEnabled(bool enabled);

    // --- Section ---
    using SectionKeyFunc = std::function<QString(int row)>;
    bool sectionEnabled() const { return m_sectionEnabled; }
    bool isSectionEnabled() const { return sectionEnabled(); }
    void setSectionEnabled(bool enabled);
    /** 设置分组 key 回调。相邻行返回相同 key 的归为一组，key 作为 section header 绘制。 */
    void setSectionKeyFunction(SectionKeyFunc func);

    // --- Selection API ---
    int selectedIndex() const;
    QList<int> selectedRows() const;
    void setSelectedIndex(int index);

    // --- Selected indicator motion API ---
    qreal selectedIndicatorProgress() const { return m_selectedIndicatorProgress; }
    IndicatorMotionDirection selectedIndicatorMotionDirection() const { return m_selectedIndicatorMotionDirection; }
    /** 控制 selected indicator 过渡动画；关闭后选择变更直接落到目标几何，便于确定性测试。 */
    bool selectedIndicatorAnimationEnabled() const { return m_selectedIndicatorAnimationEnabled; }
    bool isSelectedIndicatorAnimationEnabled() const { return selectedIndicatorAnimationEnabled(); }
    void setSelectedIndicatorAnimationEnabled(bool enabled);
    /** 返回当前绘制用的 selected indicator viewport 几何；无可见选中项时为空。 */
    QRectF selectedIndicatorRect() const;
    /** 返回指定归一化进度下的 selected indicator viewport 几何，供自动化测试验证过渡轨迹。 */
    QRectF selectedIndicatorRect(qreal progress) const;
    /** 返回指定行的 selected indicator viewport 几何；多选模式用于验证每个选中项自己的 reveal 指示器。 */
    QRectF selectedIndicatorRectForRow(int row) const;
    QRectF selectedIndicatorRectForRow(int row, qreal progress) const;

    ::fluent::scrolling::ScrollBar* verticalFluentScrollBar() const;
    ::fluent::scrolling::ScrollBar* horizontalFluentScrollBar() const;

    /**
     * @brief Suppresses the built-in scroll bars and refreshes the fluent ones.
     * zh_CN: 隐藏内置滚动条并刷新 Fluent 滚动条。
     *
     * Platform styles may re-show the native bars (e.g. inside QComboBox
     * popups), so this must run again after show.
     * zh_CN: 平台/样式可能重新显示系统滚动条（如 QComboBox 弹层场景），
     * 需在 show 后再次压制。
     */
    void refreshFluentScrollChrome();

signals:
    void selectionModeChanged();
    void fontRoleChanged();
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
    void itemClicked(int index);
    void itemReordered(int fromRow, int toRow);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
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
    void applyThemeStyle();
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
    void startBounceBack();
    void installSectionProxy();
    bool isPointInSectionHeader(const QPoint& viewportPos) const;
    int dropIndicatorRow(const QPoint& pos) const;
    void updateDragDisplacement();
    void clearDragAnimations();
    QPixmap renderItemPixmap(int row) const;
    void resetNoPhaseCluster();
    void resetNoPhaseBoundaryBounce();

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
    IndicatorMotionDirection classifySelectedIndicatorDirection(const QModelIndex& previous, const QModelIndex& current) const;
    void startSelectedIndicatorAnimation(bool animated);
    void syncMultiSelectedIndicators(const QItemSelection& selected, const QItemSelection& deselected);
    void startMultiSelectedIndicatorReveal(const QModelIndex& index);
    qreal multiSelectedIndicatorProgress(const QModelIndex& index) const;
    void setSelectedIndicatorProgress(qreal progress);
    void setSelectedIndicatorMotionDirection(IndicatorMotionDirection direction);
    void refreshSelectedIndicatorGeometry(bool snapToTarget);
    void paintSelectedIndicator(QPainter& painter) const;
    void paintIndicatorRect(QPainter& painter, const QRectF& indicatorRect, qreal opacity = 1.0) const;

    ListSelectionMode m_selectionMode = ListSelectionMode::Single;
    QString m_fontRole;

    // --- Container visuals ---
    bool m_borderVisible = true;
    bool m_backgroundVisible = true;
    QString m_headerText;
    QString m_footerText;
    QString m_placeholderText;
    QWidget* m_header = nullptr;
    QWidget* m_footer = nullptr;
    bool m_ownsHeader = false;   // QLabel created internally via setHeaderText. zh_CN: 内部通过 setHeaderText 创建的 QLabel。
    bool m_ownsFooter = false;   // QLabel created internally via setFooterText. zh_CN: 内部通过 setFooterText 创建的 QLabel。

    ::fluent::scrolling::ScrollBar* m_vScrollBar = nullptr;
    ::fluent::scrolling::ScrollBar* m_hScrollBar = nullptr;
    bool m_scrollChainingEnabled = false;
    bool m_viewportHovered = false;

    // --- Drag reorder ---
    bool m_canReorderItems = false;
    bool m_isDragging = false;
    int  m_pressedRow = -1;
    int  m_pressedHoverRow = -1;
    int  m_dragSourceRow = -1;
    int  m_dropTargetRow = -1;      // Drop indicator position. zh_CN: 拖拽指示线位置。
    QPoint m_dragStartPos;
    QPoint m_dragCurrentPos;
    QPixmap m_dragPixmap;
    QHash<int, qreal>              m_dragOffsets;  // row → current Y offset in px. zh_CN: row → 当前 Y 位移。
    QHash<int, QVariantAnimation*> m_dragAnims;    // row → displacement animation. zh_CN: row → 位移动画。
    mutable bool m_paintingWithOffsets = false;

    // --- Section ---
    bool m_sectionEnabled = false;
    SectionKeyFunc m_sectionKeyFunc;
    QAbstractItemDelegate* m_sectionProxy = nullptr;
    QAbstractItemDelegate* m_userDelegate = nullptr;

    // --- Overscroll bounce ---
    qreal m_overscrollY = 0.0;
    qreal m_overscrollX = 0.0;
    QVariantAnimation* m_bounceAnim = nullptr;
    QTimer* m_bounceTimer = nullptr;

    // --- Cross-platform wheel input (see openspec listview-cross-platform-input) ---
    qint64 m_lastNoPhaseTs = 0;   // timestamp of last NoPhaseDiscrete event (ms)
    qreal  m_clusterAccum  = 0.0; // accumulated scrollPx within current cluster
    int    m_clusterDir    = 0;   // dominant scrollPx direction for current cluster
    int    m_noPhaseBoundaryDir = 0; // active NoPhaseDiscrete boundary rebound direction
    bool   m_noPhaseBounceArmed = false;

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
};

using ListSelectionMode = ListView::ListSelectionMode;

} // namespace fluent::collections

#endif // LISTVIEW_H
