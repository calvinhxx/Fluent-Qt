#ifndef FLUENTQT_COMPONENTS_COLLECTIONS_DATAGRID_H
#define FLUENTQT_COMPONENTS_COLLECTIONS_DATAGRID_H

#include <QMetaObject>
#include <QString>
#include <QTableView>
#include <QVector>

#include "components/collections/SelectionMode.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QAbstractItemModel;
class QEvent;
class QItemSelectionModel;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QWheelEvent;

namespace fluent::scrolling {
class ScrollBar;
}

namespace fluent::collections {

namespace detail {
class DataGridCellDelegate;
}

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
    Q_PROPERTY(SelectionMode selectionMode READ selectionMode
                   WRITE setSelectionMode NOTIFY selectionModeChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText
                   WRITE setPlaceholderText NOTIFY placeholderTextChanged)
    Q_PROPERTY(bool borderVisible READ isBorderVisible
                   WRITE setBorderVisible NOTIFY borderVisibleChanged)
    Q_PROPERTY(bool backgroundVisible READ isBackgroundVisible
                   WRITE setBackgroundVisible NOTIFY backgroundVisibleChanged)
    Q_PROPERTY(bool scrollChainingEnabled READ isScrollChainingEnabled
                   WRITE setScrollChainingEnabled
                   NOTIFY scrollChainingEnabledChanged)

public:
    using SelectionMode = ::fluent::collections::SelectionMode;

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

    /**
     * @brief Returns the Fluent overlay scrollbar mirroring the native range.
     * zh_CN: 返回镜像原生范围的 Fluent 覆盖式滚动条。
     */
    fluent::scrolling::ScrollBar* verticalFluentScrollBar() const
    {
        return m_verticalFluentScrollBar;
    }

    /**
     * @brief Returns the Fluent horizontal overlay scrollbar.
     * zh_CN: 返回 Fluent 水平覆盖式滚动条。
     */
    fluent::scrolling::ScrollBar* horizontalFluentScrollBar() const
    {
        return m_horizontalFluentScrollBar;
    }

    void onThemeUpdated() override;

signals:
    void selectionModeChanged();
    void placeholderTextChanged();
    void borderVisibleChanged();
    void backgroundVisibleChanged();
    void scrollChainingEnabledChanged();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    friend class detail::DataGridCellDelegate;

    void applyThemePalette();
    void connectModelSignals(QAbstractItemModel* model);
    void refreshModelPresentation();
    void syncFluentScrollBars();
    void setHoveredRow(int row);
    void updateAutomaticAccessibleDescription();

    SelectionMode m_selectionMode = SelectionMode::Single;
    QString m_placeholderText;
    QString m_automaticAccessibleDescription;
    bool m_borderVisible = true;
    bool m_backgroundVisible = true;
    bool m_scrollChainingEnabled = false;
    int m_hoveredRow = -1;
    QVector<QMetaObject::Connection> m_modelConnections;
    QWidget* m_borderOverlay = nullptr;
    fluent::scrolling::ScrollBar* m_verticalFluentScrollBar = nullptr;
    fluent::scrolling::ScrollBar* m_horizontalFluentScrollBar = nullptr;
};

} // namespace fluent::collections

#endif // FLUENTQT_COMPONENTS_COLLECTIONS_DATAGRID_H
