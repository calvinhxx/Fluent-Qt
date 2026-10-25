#ifndef FLUENTQT_COMPONENTS_COLLECTIONS_DATAGRID_H
#define FLUENTQT_COMPONENTS_COLLECTIONS_DATAGRID_H

#include <QMetaObject>
#include <QByteArray>
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
 * @brief Fluent two-dimensional item view for large model-backed data sets.
 * zh_CN: 面向大型模型数据集的 Fluent 二维条目视图。
 *
 * DataGrid keeps models, selection models, delegates, sorting, validation,
 * and persistence caller-owned. Cells are delegate-painted and editors are
 * created only for the active index, so retained widgets do not scale with the
 * total number of rows or columns.
 * zh_CN: DataGrid 不接管模型、选择模型、代理、排序、校验或持久化。单元格由代理
 * 绘制，仅为活动索引创建编辑器，因此常驻控件数量不会随总行列数增长。
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
     * @brief Installs a caller-owned model and refreshes table semantics.
     * zh_CN: 安装调用方拥有的模型并刷新表格语义。
     */
    void setModel(QAbstractItemModel* model) override;

    /**
     * @brief Installs a caller-owned selection model.
     * zh_CN: 安装调用方拥有的选择模型。
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

    bool isScrollChainingEnabled() const { return m_scrollChainingEnabled; }
    void setScrollChainingEnabled(bool enabled);

    bool isAnimationEnabled() const { return m_animationEnabled; }
    void setAnimationEnabled(bool enabled);

    /**
     * @brief Pins a logical column to the leading edge without copying its model.
     * zh_CN: 将逻辑列固定到起始边缘，不复制模型数据。
     */
    void setColumnPinned(int column, bool pinned);
    bool isColumnPinned(int column) const;
    QList<int> pinnedColumns() const;

    /**
     * @brief Changes column visibility and emits columnLayoutChanged on a real change.
     * zh_CN: 修改列可见性，仅在实际变化时发送 columnLayoutChanged。
     */
    void setColumnVisible(int column, bool visible);
    bool isColumnVisible(int column) const;

    /**
     * @brief Serializes column order, widths, visibility, pinning and sort indicator.
     * zh_CN: 序列化列顺序、宽度、可见性、固定状态和排序指示；存储由应用负责。
     */
    QByteArray saveColumnState() const;
    /**
     * @brief Restores a validated layout for the same model schema; invalid data is a no-op.
     * zh_CN: 为相同模型结构恢复通过校验的布局；无效数据不修改状态。
     */
    bool restoreColumnState(const QByteArray& state);

    void scrollTo(const QModelIndex& index, ScrollHint hint = EnsureVisible) override;
    QRect visualRect(const QModelIndex& index) const override;
    QModelIndex indexAt(const QPoint& point) const override;
    void setRootIndex(const QModelIndex& index) override;
    void setItemDelegate(QAbstractItemDelegate* delegate);

    /**
     * @brief Returns the Fluent overlay scrollbar mirroring the native range.
     * zh_CN: 返回镜像原生范围的 Fluent 覆盖式滚动条。
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
    void scrollChainingEnabledChanged();
    void animationEnabledChanged();
    void columnLayoutChanged();
    void modelChanged(QAbstractItemModel* model);

protected:
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
