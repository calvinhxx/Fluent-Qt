#ifndef FLUENTQT_COMPONENTS_COLLECTIONS_DATAGRID_H
#define FLUENTQT_COMPONENTS_COLLECTIONS_DATAGRID_H

#include <QMetaObject>
#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QPersistentModelIndex>
#include <QSet>
#include <QString>
#include <QTableView>
#include <QVector>

#include "components/collections/SelectionMode.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QAbstractItemModel;
class QEvent;
class QFocusEvent;
class QItemSelectionModel;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QWheelEvent;
class QVariantAnimation;

namespace fluent::scrolling {
class ScrollBar;
}

namespace fluent::collections {

namespace detail {
class DataGridCellDelegate;
class DataGridPinnedView;
} // namespace detail

/**
 * @brief Table view backed by a Qt item model with selection, sorting, and on-demand editing.
 * zh_CN: 基于 Qt 数据模型的表格视图，支持行/单元格选择、排序与按需编辑。
 */
class DataGrid : public QTableView, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(SelectionMode selectionMode READ selectionMode WRITE setSelectionMode NOTIFY selectionModeChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText NOTIFY placeholderTextChanged)
    Q_PROPERTY(bool borderVisible READ isBorderVisible WRITE setBorderVisible NOTIFY borderVisibleChanged)
    Q_PROPERTY(bool backgroundVisible
        READ isBackgroundVisible
        WRITE setBackgroundVisible
        NOTIFY backgroundVisibleChanged)
    Q_PROPERTY(QColor headerBackground
        READ headerBackground
        WRITE setHeaderBackground
        RESET resetHeaderBackground
        NOTIFY headerBackgroundChanged)
    Q_PROPERTY(bool scrollChainingEnabled
        READ isScrollChainingEnabled
        WRITE setScrollChainingEnabled
        NOTIFY scrollChainingEnabledChanged)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)

public:
    using SelectionMode = ::fluent::collections::SelectionMode;

    using QTableView::edit;
    explicit DataGrid(QWidget* parent = nullptr);
    ~DataGrid() override;

    /**
     * @brief Sets the table item model (still managed by the caller) and refreshes table state.
     * zh_CN: 设置数据模型（仍由应用管理，不会随视图销毁）并刷新表格状态。
     */
    void setModel(QAbstractItemModel* model) override;

    /**
     * @brief Sets the selection model (still managed by the caller).
     * zh_CN: 设置选择模型（仍由应用管理，不会随视图销毁）。
     */
    void setSelectionModel(QItemSelectionModel* selectionModel) override;

    SelectionMode selectionMode() const { return m_selectionMode; }
    void setSelectionMode(SelectionMode mode);

    QString placeholderText() const { return m_placeholderText; }
    void setPlaceholderText(const QString& text);
    bool isShowingPlaceholder() const;

    bool isBorderVisible() const { return m_borderVisible; }
    void setBorderVisible(bool visible);

    bool isBackgroundVisible() const { return m_backgroundVisible; }
    void setBackgroundVisible(bool visible);

    /** @brief Header color override. An invalid color follows the theme's layer surface.
     * zh_CN: 表头底色覆盖值。无效颜色表示跟随主题的层背景色。
     */
    QColor headerBackground() const { return m_headerBackground; }
    /** @brief Sets the solid background of row, column and pinned headers. Alpha is ignored.
     * zh_CN: 设置行、列和固定列表头的实色背景；忽略透明度。
     */
    void setHeaderBackground(const QColor& color);
    /** @brief Restores the theme header background. zh_CN: 恢复跟随主题的表头底色。 */
    void resetHeaderBackground() { setHeaderBackground(QColor()); }

    bool isScrollChainingEnabled() const { return m_scrollChainingEnabled; }
    void setScrollChainingEnabled(bool enabled);

    bool isAnimationEnabled() const { return m_animationEnabled; }
    void setAnimationEnabled(bool enabled);

    /**
     * @brief Pins or unpins a logical column at the leading edge of the table.
     * zh_CN: 将指定逻辑列固定到表格起始侧或取消固定。
     */
    void setColumnPinned(int column, bool pinned);
    bool isColumnPinned(int column) const;
    QList<int> pinnedColumns() const;

    /**
     * @brief Shows or hides a logical column and emits columnLayoutChanged() when visibility changes.
     * zh_CN: 设置逻辑列是否可见，仅在可见性实际变化时发出 columnLayoutChanged()。
     */
    void setColumnVisible(int column, bool visible);
    bool isColumnVisible(int column) const;

    /**
     * @brief Serializes column order, widths, visibility, pinned flags, and sort indicator.
     * zh_CN: 序列化列顺序、列宽、可见性、固定状态及排序指示器。
     */
    QByteArray saveColumnState() const;
    /**
     * @brief Restores a previously saved column layout for the same model schema; ignores invalid data.
     * zh_CN: 为相同表结构的模型恢复列布局；无效数据会被忽略并返回 false。
     */
    bool restoreColumnState(const QByteArray& state);

    void scrollTo(const QModelIndex& index, ScrollHint hint = EnsureVisible) override;
    QRect visualRect(const QModelIndex& index) const override;
    QModelIndex indexAt(const QPoint& point) const override;
    void setRootIndex(const QModelIndex& index) override;
    void setItemDelegate(QAbstractItemDelegate* delegate);

    /**
     * @brief Returns the Fluent vertical overlay scrollbar.
     * zh_CN: 返回 Fluent 垂直覆盖式滚动条。
     */
    fluent::scrolling::ScrollBar* verticalFluentScrollBar() const { return m_verticalFluentScrollBar; }

    /**
     * @brief Returns the Fluent horizontal overlay scrollbar.
     * zh_CN: 返回 Fluent 水平覆盖式滚动条。
     */
    fluent::scrolling::ScrollBar* horizontalFluentScrollBar() const { return m_horizontalFluentScrollBar; }

    void onThemeUpdated() override;

signals:
    void selectionModeChanged();
    void placeholderTextChanged();
    void borderVisibleChanged();
    void backgroundVisibleChanged();
    void headerBackgroundChanged();
    void scrollChainingEnabledChanged();
    void animationEnabledChanged();
    void columnLayoutChanged();
    void modelChanged(QAbstractItemModel* model);

protected:
    bool event(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool edit(const QModelIndex& index, EditTrigger trigger, QEvent* event) override;

private:
    friend class detail::DataGridCellDelegate;
    friend class detail::DataGridPinnedView;

    void applyThemePalette();
    void updateHeaderSurfaces();
    QColor effectiveHeaderBackground() const;
    void connectModelSignals(QAbstractItemModel* model);
    void refreshModelPresentation();
    void syncFluentScrollBars();
    void updateSurfaceMask();
    void setHoveredRow(int row);
    void updateFocusPresentation();
    void updateAutomaticAccessibleDescription();
    void syncPinnedColumns();
    void ensurePinnedView();
    qreal selectionOpacity(const QModelIndex& index, bool selected) const;

    SelectionMode m_selectionMode = SelectionMode::Single;
    QString m_placeholderText;
    QColor m_headerBackground;
    QString m_automaticAccessibleDescription;
    bool m_borderVisible = true;
    bool m_backgroundVisible = true;
    bool m_scrollChainingEnabled = false;
    int m_hoveredRow = -1;
    int m_previousHoveredRow = -1;
    qreal m_hoverProgress = 1.0;
    qreal m_previousHoverOpacity = 1.0;
    bool m_animationEnabled = true;
    bool m_keyboardFocusVisible = false;
    QVariantAnimation* m_hoverAnimation = nullptr;
    QVariantAnimation* m_selectionAnimation = nullptr;
    qreal m_selectionProgress = 1.0;
    QHash<QPersistentModelIndex, QPair<qreal, qreal>> m_selectionTransitions;
    QSet<int> m_pinnedColumns;
    QHash<int, int> m_columnWidths;
    QHash<int, int> m_columnWidthsBeforeChange;
    bool m_changingColumns = false;
    QTableView* m_pinnedView = nullptr;
    QAbstractItemDelegate* m_pinnedDefaultDelegate = nullptr;
    bool m_syncingColumns = false;
    bool m_pinnedStructureDirty = true;
    int m_pinnedWidth = 0;
    int m_pinnedHiddenCount = -1;
    QVector<QMetaObject::Connection> m_modelConnections;
    QWidget* m_borderOverlay = nullptr;
    fluent::scrolling::ScrollBar* m_verticalFluentScrollBar = nullptr;
    fluent::scrolling::ScrollBar* m_horizontalFluentScrollBar = nullptr;
};

} // namespace fluent::collections

#endif // FLUENTQT_COMPONENTS_COLLECTIONS_DATAGRID_H
