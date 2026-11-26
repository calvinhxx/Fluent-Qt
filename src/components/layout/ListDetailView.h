#ifndef FLUENTQT_COMPONENTS_LAYOUT_LISTDETAILVIEW_H
#define FLUENTQT_COMPONENTS_LAYOUT_LISTDETAILVIEW_H

#include <QPointer>
#include <QWidget>

#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

namespace fluent::layout {

class Divider;

/**
 * @brief Two persistent panes that switch to single-pane mode below a width threshold.
 * zh_CN: 双栏布局容器，窗口宽度低于阈值时切换为单栏显示。
 *
 * Add child widgets directly to listPane() and detailPane() (owned by this view; do not delete or reparent).
 * Switching panes keeps both widgets intact without reparenting or rebuilding contents.
 * zh_CN: 请直接在 listPane() 和 detailPane() 内添加子控件与布局（两个容器由本视图管理生命周期，请勿删除或重设父对象）；
 * 模式切换仅控制显隐，不会重建或重设子控件父对象。
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

    /** @brief Returns the view-owned list pane container.
     * zh_CN: 返回列表栏容器（由本控件管理生命周期）。 */
    QWidget* listPane() const { return m_listPane; }
    /** @brief Returns the view-owned detail pane container.
     * zh_CN: 返回详情栏容器（由本控件管理生命周期）。 */
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
