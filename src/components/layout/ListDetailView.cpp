#include "components/layout/ListDetailView.h"

#include <QApplication>
#include <QEvent>
#include <QStyle>

#include "components/layout/Divider.h"

namespace fluent::layout {

ListDetailView::ListDetailView(QWidget* parent)
    : QWidget(parent), m_listPane(new QWidget(this)), m_detailPane(new QWidget(this)),
      m_divider(new Divider(Qt::Vertical, this))
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_listPane->setObjectName(QStringLiteral("listDetailListPane"));
    m_detailPane->setObjectName(QStringLiteral("listDetailDetailPane"));
    updatePanes();
}

void ListDetailView::setCompactThreshold(int width)
{
    width = qMax(1, width);
    if (width == m_compactThreshold)
        return;
    m_compactThreshold = width;
    const auto revision = ++m_thresholdRevision;
    QPointer<ListDetailView> guard(this);
    updatePanes();
    if (guard && revision == m_thresholdRevision)
        emit compactThresholdChanged(width);
}

void ListDetailView::setListPaneWidth(int width)
{
    width = qMax(1, width);
    if (width == m_listPaneWidth)
        return;
    m_listPaneWidth = width;
    const auto revision = ++m_widthRevision;
    QPointer<ListDetailView> guard(this);
    updatePanes();
    if (guard && revision == m_widthRevision)
        emit listPaneWidthChanged(width);
}

void ListDetailView::setCurrentPane(Pane pane)
{
    if ((pane != List && pane != Detail) || pane == m_currentPane)
        return;
    m_currentPane = pane;
    const auto revision = ++m_paneRevision;
    QPointer<ListDetailView> guard(this);
    updatePanes();
    if (guard && revision == m_paneRevision)
        emit currentPaneChanged(pane);
}

QSize ListDetailView::sizeHint() const
{
    return QSize(720, 400);
}
QSize ListDetailView::minimumSizeHint() const
{
    return QSize(0, 0);
}

void ListDetailView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updatePanes();
}

void ListDetailView::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LayoutDirectionChange)
        updatePanes();
}

void ListDetailView::focusPane(QWidget* pane, const QPointer<QWidget>& previous)
{
    if (previous && pane->isAncestorOf(previous) && previous->isVisible() && previous->isEnabled()) {
        previous->setFocus(Qt::OtherFocusReason);
        return;
    }
    for (QWidget* next = pane->nextInFocusChain(); next != pane; next = next->nextInFocusChain()) {
        if (pane->isAncestorOf(next) && next->isVisible() && next->isEnabled()
            && (next->focusPolicy() & Qt::TabFocus)) {
            next->setFocus(Qt::OtherFocusReason);
            return;
        }
    }
}

void ListDetailView::updatePanes()
{
    QPointer<ListDetailView> guard(this);
    const auto revision = ++m_layoutRevision;
    const auto current = [&] { return guard && revision == guard->m_layoutRevision; };
    const bool compact = width() < m_compactThreshold;
    m_compact = compact;
    const bool showListPane = !compact || m_currentPane == List;
    const bool showDetailPane = !compact || m_currentPane == Detail;

    QWidget* focused = QApplication::focusWidget();
    const bool listHadFocus = focused && m_listPane->isAncestorOf(focused);
    const bool detailHadFocus = focused && m_detailPane->isAncestorOf(focused);
    if (listHadFocus)
        m_listFocus = focused;
    if (detailHadFocus)
        m_detailFocus = focused;

    // Leave hidden geometry intact: the child's scroll range must not collapse.
    // zh_CN: 保留隐藏栏的几何，避免子控件滚动范围收缩而丢失滚动位置。
    const int listWidth = qMin(m_listPaneWidth, qMax(0, (width() - 1) / 2));
    if (showListPane)
        m_listPane->setGeometry(
            compact ? rect() : QStyle::visualRect(layoutDirection(), rect(), QRect(0, 0, listWidth, height())));
    if (!current())
        return;
    if (showDetailPane)
        m_detailPane->setGeometry(compact
                ? rect()
                : QStyle::visualRect(layoutDirection(), rect(),
                      QRect(listWidth + 1, 0, qMax(0, width() - listWidth - 1), height())));
    if (!current())
        return;
    m_divider->setGeometry(QStyle::visualRect(layoutDirection(), rect(), QRect(listWidth, 0, 1, height())));
    if (!current())
        return;
    if (showListPane)
        m_listPane->show();
    if (!current())
        return;
    if (showDetailPane)
        m_detailPane->show();
    if (!current())
        return;
    // Transfer focus while both panes still exist in the visible focus chain.
    // zh_CN: 在原栏仍可见时把焦点交给目标栏，避免隐藏触发窗口外部控件的临时聚焦。
    if (listHadFocus && !showListPane)
        focusPane(m_detailPane, m_detailFocus);
    else if (detailHadFocus && !showDetailPane)
        focusPane(m_listPane, m_listFocus);
    if (!current())
        return;
    if (!showListPane)
        m_listPane->hide();
    if (!current())
        return;
    if (!showDetailPane)
        m_detailPane->hide();
    if (!current())
        return;
    m_divider->setVisible(!compact);
    if (!current())
        return;
    // A child geometry/focus callback may have replaced this layout pass.
    // Publish only the final state, including when a nested pass kept its mode.
    // zh_CN: 子控件几何或焦点回调可能替换布局，仅发布最终状态。
    if (m_reportedCompact != compact) {
        m_reportedCompact = compact;
        emit compactChanged(compact);
    }
}

} // namespace fluent::layout
