#ifndef FLUENTQT_COMPONENTS_LAYOUT_LISTDETAILVIEW_H
#define FLUENTQT_COMPONENTS_LAYOUT_LISTDETAILVIEW_H

#include <QPointer>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::layout {

class Divider;

/**
 * @brief Two persistent content panes that collapse to one at a width threshold.
 * zh_CN: 两个持久内容区，在宽度不足时只显示当前一栏。
 *
 * Populate listPane() and detailPane() with ordinary child widgets/layouts.
 * The view owns these containers; do not delete or reparent them. Selection,
 * models, documents, and back-button presentation remain application-owned.
 * Switching panes never reparents, rebuilds, or serializes their contents.
 * zh_CN: 在 listPane() 和 detailPane() 内添加子控件与布局。容器由本组件持有，
 * 不应删除或更改父对象；选中项、模型、文档与返回按钮由应用负责。切换时不重建、
 * 不移动内容的父对象，也不序列化内容状态。
 */
class ListDetailView : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(bool compact READ isCompact NOTIFY compactChanged)
    Q_PROPERTY(int compactThreshold READ compactThreshold WRITE setCompactThreshold NOTIFY compactThresholdChanged)
    Q_PROPERTY(int listPaneWidth READ listPaneWidth WRITE setListPaneWidth NOTIFY listPaneWidthChanged)
    Q_PROPERTY(Pane currentPane READ currentPane WRITE setCurrentPane NOTIFY currentPaneChanged)

public:
    /** @brief Active destination in compact mode; wide mode shows both panes.
     * zh_CN: 窄屏当前栏；宽屏始终显示两栏。 */
    enum Pane { List, Detail };
    Q_ENUM(Pane)

    explicit ListDetailView(QWidget* parent = nullptr);

    /** @brief Returns the permanent, view-owned list content container.
     * zh_CN: 返回本组件持有的持久列表内容容器。 */
    QWidget* listPane() const { return m_listPane; }
    /** @brief Returns the permanent, view-owned detail content container.
     * zh_CN: 返回本组件持有的持久详情内容容器。 */
    QWidget* detailPane() const { return m_detailPane; }

    bool isCompact() const { return m_compact; }
    int compactThreshold() const { return m_compactThreshold; }
    /** @brief Sets the width below which only currentPane is shown; minimum 1.
     * zh_CN: 设置仅显示当前栏的宽度阈值，最小为 1。 */
    void setCompactThreshold(int width);
    int listPaneWidth() const { return m_listPaneWidth; }
    /** @brief Sets the wide list width, bounded to half the available width.
     * zh_CN: 设置宽屏列表宽度，实际不超过可用宽度的一半。 */
    void setListPaneWidth(int width);
    Pane currentPane() const { return m_currentPane; }
    void setCurrentPane(Pane pane);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void showList() { setCurrentPane(List); }
    void showDetail() { setCurrentPane(Detail); }

signals:
    void compactChanged(bool compact);
    void compactThresholdChanged(int width);
    void listPaneWidthChanged(int width);
    void currentPaneChanged(Pane pane);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void updatePanes();
    void focusPane(QWidget* pane, const QPointer<QWidget>& previous);

    QWidget* m_listPane;
    QWidget* m_detailPane;
    Divider* m_divider;
    QPointer<QWidget> m_listFocus;
    QPointer<QWidget> m_detailFocus;
    int m_compactThreshold = 640;
    int m_listPaneWidth = 240;
    Pane m_currentPane = List;
    bool m_compact = false;
    bool m_reportedCompact = false;
    quint64 m_layoutRevision = 0;
    quint64 m_thresholdRevision = 0;
    quint64 m_widthRevision = 0;
    quint64 m_paneRevision = 0;
};

} // namespace fluent::layout

#endif // FLUENTQT_COMPONENTS_LAYOUT_LISTDETAILVIEW_H
