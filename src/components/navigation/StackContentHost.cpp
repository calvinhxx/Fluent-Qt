#include "StackContentHost.h"

#include <QParallelAnimationGroup>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QPalette>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QStackedLayout>

#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/private/DpiPaintMetrics_p.h"
#include "components/windowing/WindowBackdrop.h"

namespace fluent::navigation {

namespace {

// Records the theme generation a page subtree was last refreshed for, so a page hidden during a theme
// change can be re-themed exactly once when it is next shown. zh_CN: 记录页子树上次刷新对应的主题代次，
// 使切换主题期间隐藏的页在下次显示时恰好重刷一次。
constexpr char kThemedGenerationProperty[] = "fluentThemedGeneration";

// Themes root and every FluentElement descendant. Used to bring a stale (off-screen during the switch)
// page subtree up to the current theme synchronously, before it is shown — so its update()s coalesce
// into the page's first paint. zh_CN: 刷新 root 及其所有 FluentElement 后代。用于在显示前同步把过期（切换时
// 在屏外）的页子树更新到当前主题，使其 update() 合并进该页首次绘制。
void refreshFluentTree(QWidget* root)
{
    if (!root)
        return;
    if (auto* element = dynamic_cast<FluentElement*>(root))
        element->onThemeUpdated();
    const auto children = root->findChildren<QWidget*>();
    for (QWidget* child : children) {
        if (auto* element = dynamic_cast<FluentElement*>(child))
            element->onThemeUpdated();
    }
}

} // namespace

StackContentHost::StackContentHost(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QStackedLayout())
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setStackingMode(QStackedLayout::StackOne);
    setLayout(m_layout);
    onThemeUpdated();
}

StackContentHost::~StackContentHost()
{
    discardTransitionGroup();
    for (PageRecord& page : m_pages) {
        QObject::disconnect(page.destroyedConnection);
        QWidget* content = page.content.data();
        if (!content)
            continue;
        if (page.ownership == WidgetOwnership::Borrowed) {
            content->setParent(nullptr);
        } else if (page.ownership == WidgetOwnership::Reparented) {
            content->setParent(page.originalParent.data());
        }
    }
    m_pages.clear();
}

void StackContentHost::setContentSurface(const QColor& fill, qreal topLeftRadius, const QColor& border)
{
    if (m_surfaceFill == fill && m_surfaceBorder == border
        && qFuzzyCompare(m_surfaceTopLeftRadius + 1.0, topLeftRadius + 1.0))
        return;
    m_surfaceFill = fill;
    m_surfaceBorder = border;
    m_surfaceTopLeftRadius = qMax(0.0, topLeftRadius);
    setAutoFillBackground(false);  // we paint the surface ourselves (or stay transparent)
    update();
}

void StackContentHost::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    // Transparent widgets share the top-level backing store. Replace this region on every
    // backdrop frame so pixels from an outgoing page cannot survive a stack switch.
    // zh_CN: 透明控件共享顶层后备缓冲；每个背景帧都替换此区域，避免切页后保留旧页面像素。
    if (window()
        && window()->testAttribute(Qt::WA_TranslucentBackground)
        && windowing::windowBackdropRequiresTransparentClear(window())) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(event->rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    // Paint only an explicitly configured surface; otherwise page gaps stay transparent.
    // zh_CN: 仅绘制显式配置的表面；否则页面间隙保持透明。
    if (!m_surfaceFill.isValid() || m_surfaceFill.alpha() == 0) {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing);

    const bool hasBorder = m_surfaceBorder.isValid() && m_surfaceBorder.alpha() > 0;
    const fluent::painting::DpiPaintMetrics metrics(painter);
    fluent::painting::DeviceAlignedStroke stroke;
    if (hasBorder) {
        stroke = metrics.alignedStroke(QRectF(rect()), 1.0);
    } else {
        stroke.rect = metrics.alignedOuterRect(QRectF(rect()));
    }
    const QRectF panelRect = stroke.rect;
    const bool rounded = m_surfaceTopLeftRadius > 0.0;
    const QPainterPath panel = fluent::overlay::roundedCornerRectPath(
        panelRect, m_surfaceTopLeftRadius, /*TL*/ rounded, /*TR*/ false, /*BR*/ false, /*BL*/ false);

    painter.fillPath(panel, m_surfaceFill);  // Explicit surface, including a translucent overlay.
    if (hasBorder) {
        painter.setPen(QPen(m_surfaceBorder, stroke.width));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(panel);
    }
}

QWidget* StackContentHost::pageWidget(int index) const
{
    if (index < 0 || index >= m_pages.size())
        return nullptr;
    return m_pages.at(index).content.data();
}

bool StackContentHost::insertPage(int index, QWidget* widget)
{
    return insertPage(index, widget, WidgetOwnership::Owned);
}

bool StackContentHost::insertPage(int index,
                                  QWidget* widget,
                                  WidgetOwnership ownership)
{
    if (index < 0 || index > m_pages.size() || !canHostPage(widget))
        return false;

    PageRecord page = makePage(widget, ownership);
    m_pages.insert(index, page);
    m_layout->insertWidget(index, page.stackWidget);
    page.stackWidget->hide();
    if (m_currentIndex >= index)
        ++m_currentIndex;
    return true;
}

QWidget* StackContentHost::replacePage(int index, QWidget* widget)
{
    QWidget* transferredPage = nullptr;
    if (!replacePageImpl(index,
                         widget,
                         WidgetOwnership::Owned,
                         false,
                         &transferredPage)) {
        return nullptr;
    }
    return transferredPage;
}

bool StackContentHost::replacePage(int index,
                                   QWidget* widget,
                                   WidgetOwnership ownership)
{
    return replacePageImpl(index, widget, ownership, true, nullptr);
}

bool StackContentHost::replacePageImpl(int index,
                                       QWidget* widget,
                                       WidgetOwnership ownership,
                                       bool applyPreviousOwnership,
                                       QWidget** transferredPage)
{
    if (index < 0 || index >= m_pages.size() || !canHostPage(widget))
        return false;

    if (m_transitionGroup) {
        discardTransitionGroup();
        setBusy(false);
    }

    PageRecord oldPage = m_pages.at(index);
    QObject::disconnect(oldPage.destroyedConnection);
    removeStackWidget(oldPage.stackWidget);

    PageRecord newPage = makePage(widget, ownership);
    m_pages[index] = newPage;
    m_layout->insertWidget(index, newPage.stackWidget);
    if (m_currentIndex == index) {
        m_layout->setCurrentWidget(newPage.stackWidget);
        newPage.stackWidget->setGeometry(rect());
        newPage.stackWidget->show();
    } else {
        newPage.stackWidget->hide();
    }

    if (applyPreviousOwnership) {
        releasePageRecord(oldPage);
    } else if (transferredPage) {
        *transferredPage = transferPage(oldPage);
    }
    return true;
}

QWidget* StackContentHost::takePage(int index)
{
    if (index < 0 || index >= m_pages.size())
        return nullptr;

    const PageRecord page = extractPage(index);
    return transferPage(page);
}

bool StackContentHost::releasePage(int index)
{
    if (index < 0 || index >= m_pages.size())
        return false;

    const PageRecord page = extractPage(index);
    releasePageRecord(page);
    return true;
}

void StackContentHost::clearPages()
{
    clearPagesImpl(false);
}

void StackContentHost::releaseAllPages()
{
    clearPagesImpl(true);
}

bool StackContentHost::movePage(int from, int to)
{
    if (from < 0 || from >= m_pages.size() || to < 0 || to >= m_pages.size() || from == to)
        return false;

    PageRecord page = m_pages.takeAt(from);
    m_layout->removeWidget(page.stackWidget);
    m_pages.insert(to, page);
    m_layout->insertWidget(to, page.stackWidget);

    const int oldCurrent = m_currentIndex;
    if (oldCurrent == from) {
        m_currentIndex = to;
    } else if (from < oldCurrent && oldCurrent <= to) {
        m_currentIndex = oldCurrent - 1;
    } else if (to <= oldCurrent && oldCurrent < from) {
        m_currentIndex = oldCurrent + 1;
    }
    if (m_currentIndex >= 0) {
        if (QWidget* current = stackWidgetAt(m_currentIndex))
            m_layout->setCurrentWidget(current);
    }
    return true;
}

int StackContentHost::indexOf(QWidget* widget) const
{
    if (!widget)
        return -1;
    for (int index = 0; index < m_pages.size(); ++index) {
        if (m_pages.at(index).identity == widget)
            return index;
    }
    return -1;
}

WidgetOwnership StackContentHost::pageOwnershipAt(int index) const
{
    return index >= 0 && index < m_pages.size()
        ? m_pages.at(index).ownership
        : WidgetOwnership::Borrowed;
}

void StackContentHost::setCurrentIndex(int index, int direction, bool animated)
{
    const int normalized = index >= 0 && index < m_pages.size() ? index : -1;
    if (m_currentIndex == normalized)
        return;

    if (m_transitionGroup) {
        discardTransitionGroup();
        setBusy(false);
        showOnlyStackWidget(stackWidgetAt(m_currentIndex));
    }

    QWidget* fromWidget = stackWidgetAt(m_currentIndex);
    QWidget* toWidget = stackWidgetAt(normalized);
    m_currentIndex = normalized;
    emit currentIndexChanged(m_currentIndex);

    if (!toWidget) {
        showOnlyStackWidget(nullptr);
        return;
    }

    if (!canAnimate(fromWidget, toWidget, animated)) {
        showOnlyStackWidget(toWidget);
        return;
    }

    const QRect endRect = rect();
    const QPoint startOffset = transitionStartOffset(endRect, direction);

    showOnlyStackWidget(toWidget);
    toWidget->setGeometry(QRect(endRect.topLeft() + startOffset, endRect.size()));
    toWidget->show();
    toWidget->raise();

    m_transitionGroup = new QParallelAnimationGroup(this);
    auto addPosAnimation = [this](QWidget* widget, const QPoint& start, const QPoint& end) {
        auto* animation = new QPropertyAnimation(widget, "pos", m_transitionGroup);
        animation->setStartValue(start);
        animation->setEndValue(end);
        animation->setDuration(themeAnimation().normal);
        animation->setEasingCurve(themeAnimation().decelerate);
        m_transitionGroup->addAnimation(animation);
    };

    addPosAnimation(toWidget, toWidget->pos(), endRect.topLeft());

    setBusy(true);
    QPointer<QWidget> toPointer = toWidget;
    connect(m_transitionGroup, &QParallelAnimationGroup::finished, this, [this, normalized, toPointer]() {
        finishTransition(normalized, toPointer.data());
    });
    m_transitionGroup->start();
}

void StackContentHost::setTransitionAnimationEnabled(bool enabled)
{
    m_transitionAnimationEnabled = enabled;
}

void StackContentHost::setTransitionEffect(TransitionEffect effect)
{
    if (m_transitionEffect == effect)
        return;
    m_transitionEffect = effect;
    emit transitionEffectChanged(m_transitionEffect);
}

void StackContentHost::onThemeUpdated()
{
    // The background is painted in paintEvent (QSS-proof). A QPalette::Window +
    // autoFillBackground fill is dropped under an ancestor style sheet, so don't rely on it.
    // zh_CN: 背景在 paintEvent 中绘制（不受样式表影响）。祖先样式表下 QPalette::Window + autoFill
    // 会被丢弃，故不依赖它。
    setAutoFillBackground(false);
    update();
}

void StackContentHost::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_busy)
        return;
    if (QWidget* current = m_layout->currentWidget())
        current->setGeometry(rect());
}

bool StackContentHost::canHostPage(QWidget* widget) const
{
    return !widget
        || (widget != this && !widget->isAncestorOf(this) && indexOf(widget) < 0);
}

StackContentHost::PageRecord StackContentHost::makePage(
    QWidget* widget,
    WidgetOwnership ownership)
{
    PageRecord page;
    if (widget) {
        page.content = widget;
        page.stackWidget = widget;
        page.identity = widget;
        page.originalParent = widget->parentWidget();
        page.ownership = ownership;
        page.placeholder = false;
        page.destroyedConnection = connect(
            widget,
            &QObject::destroyed,
            this,
            [this, widget]() { handlePageDestroyed(widget); });
        widget->setParent(this);
    } else {
        auto* placeholder = new QWidget(this);
        placeholder->setObjectName(QStringLiteral("StackContentHostBlankPage"));
        page.stackWidget = placeholder;
        page.placeholder = true;
    }
    if (QWidget* stackWidget = page.stackWidget.data()) {
        stackWidget->setGeometry(rect());
        stackWidget->hide();
        // A freshly built page already reflects the current theme, so stamp it current — the on-show
        // staleness check then only refreshes it if the theme changes while it is hidden. zh_CN: 新建页
        // 已是当前主题,先标记为当前——显示时的过期检查便只在该页隐藏期间主题变化时才刷新它。
        stackWidget->setProperty(kThemedGenerationProperty, FluentElement::themeGeneration());
    }
    return page;
}

StackContentHost::PageRecord StackContentHost::extractPage(int index)
{
    if (m_transitionGroup) {
        discardTransitionGroup();
        setBusy(false);
    }

    PageRecord page = m_pages.takeAt(index);
    QObject::disconnect(page.destroyedConnection);
    removeStackWidget(page.stackWidget);
    finishPageRemoval(index);
    return page;
}

void StackContentHost::removeStackWidget(QWidget* widget)
{
    if (!widget)
        return;
    m_layout->removeWidget(widget);
    widget->hide();
}

void StackContentHost::deletePlaceholder(const PageRecord& page)
{
    if (page.placeholder && page.stackWidget)
        page.stackWidget->deleteLater();
}

QWidget* StackContentHost::transferPage(const PageRecord& page)
{
    removeStackWidget(page.stackWidget);
    if (page.placeholder) {
        deletePlaceholder(page);
        return nullptr;
    }
    QWidget* content = page.content.data();
    if (content)
        content->setParent(nullptr);
    return content;
}

void StackContentHost::releasePageRecord(const PageRecord& page)
{
    removeStackWidget(page.stackWidget);
    if (page.placeholder) {
        deletePlaceholder(page);
        return;
    }

    QWidget* content = page.content.data();
    if (!content)
        return;
    if (page.ownership == WidgetOwnership::Owned) {
        delete content;
    } else if (page.ownership == WidgetOwnership::Reparented) {
        content->setParent(page.originalParent.data());
    } else {
        content->setParent(nullptr);
    }
}

void StackContentHost::clearPagesImpl(bool applyOwnership)
{
    if (m_transitionGroup) {
        discardTransitionGroup();
        setBusy(false);
    }

    const QVector<PageRecord> pages = m_pages;
    m_pages.clear();
    for (const PageRecord& page : pages) {
        QObject::disconnect(page.destroyedConnection);
        if (applyOwnership)
            releasePageRecord(page);
        else
            transferPage(page);
    }

    const bool changed = m_currentIndex != -1;
    m_currentIndex = -1;
    if (changed)
        emit currentIndexChanged(-1);
}

void StackContentHost::handlePageDestroyed(QWidget* widget)
{
    const int index = indexOf(widget);
    if (index < 0)
        return;

    if (m_transitionGroup) {
        discardTransitionGroup();
        setBusy(false);
    }
    m_pages.removeAt(index);
    finishPageRemoval(index);
}

void StackContentHost::finishPageRemoval(int removedIndex)
{
    normalizeCurrentIndexAfterRemoval(removedIndex);
    if (m_currentIndex >= 0 && m_currentIndex < m_pages.size()) {
        if (QWidget* current = stackWidgetAt(m_currentIndex)) {
            m_layout->setCurrentWidget(current);
            current->show();
        }
    } else {
        showOnlyStackWidget(nullptr);
    }
}

void StackContentHost::discardTransitionGroup()
{
    if (!m_transitionGroup)
        return;
    m_transitionGroup->stop();
    m_transitionGroup->deleteLater();
    m_transitionGroup = nullptr;
}

void StackContentHost::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged(m_busy);
}

void StackContentHost::finishTransition(int targetIndex, QWidget* toWidget)
{
    discardTransitionGroup();

    if (targetIndex >= 0 && targetIndex < m_pages.size() && toWidget)
        showOnlyStackWidget(toWidget);
    else
        showOnlyStackWidget(nullptr);
    setBusy(false);
}

bool StackContentHost::canAnimate(QWidget* fromWidget, QWidget* toWidget, bool requested) const
{
    return requested
        && m_transitionAnimationEnabled
        && isVisible()
        && rect().isValid()
        && rect().width() > 0
        && rect().height() > 0
        && fromWidget
        && toWidget
        && fromWidget != toWidget;
}

QPoint StackContentHost::transitionStartOffset(const QRect& rect, int direction) const
{
    // direction < 0 (back navigation) mirrors the incoming slide to the opposite side.
    const int sign = direction < 0 ? -1 : 1;
    switch (m_transitionEffect) {
    case TransitionEffect::SlideFromLeft: {
        const int travel = qMax(1, qRound(rect.width() * 0.28));
        return QPoint(-travel * sign, 0);
    }
    case TransitionEffect::SlideFromBottom: {
        const int travel = qMax(1, qRound(rect.height() * 0.28));
        return QPoint(0, travel * sign);
    }
    }
    return QPoint();
}

void StackContentHost::showOnlyStackWidget(QWidget* currentWidget)
{
    QWidget* previousWidget = m_layout->currentWidget();
    if (!currentWidget && previousWidget)
        previousWidget->hide();

    if (currentWidget) {
        // If the theme changed while this page was hidden, the global manager only themed visible
        // elements — refresh this subtree now, before it paints, so a prewarmed page never flashes a
        // stale theme on navigation. The cost is one page's worth of widgets, only when stale.
        // zh_CN: 若该页隐藏期间主题变了,全局管理器只刷新了可见元素——在它绘制前刷新本子树,使预热页导航时
        // 绝不会闪过期主题。开销仅为一页的控件量,且仅在过期时发生。
        const int generation = FluentElement::themeGeneration();
        if (currentWidget->property(kThemedGenerationProperty).toInt() != generation) {
            refreshFluentTree(currentWidget);
            currentWidget->setProperty(kThemedGenerationProperty, generation);
        }
        if (previousWidget != currentWidget)
            m_layout->setCurrentWidget(currentWidget);
        if (currentWidget->geometry() != rect())
            currentWidget->setGeometry(rect());
        if (!currentWidget->isVisible())
            currentWidget->show();
        currentWidget->raise();
    }
    // QStackedLayout::StackOne hides the outgoing widget. Touch only the
    // outgoing/current pair and queue one coalesced backing-store refresh, so
    // switching remains O(1) as the resident page count grows.
    // zh_CN: StackOne 会自动隐藏旧页；这里只处理旧页/当前页并合并一次刷新，
    // 使常驻页面数量增加后切换仍保持 O(1)。
    update();
}

void StackContentHost::normalizeCurrentIndexAfterRemoval(int removedIndex)
{
    if (m_pages.isEmpty()) {
        const bool changed = m_currentIndex != -1;
        m_currentIndex = -1;
        if (changed)
            emit currentIndexChanged(-1);
        return;
    }

    int nextIndex = m_currentIndex;
    if (m_currentIndex == removedIndex)
        nextIndex = qMin(removedIndex, m_pages.size() - 1);
    else if (m_currentIndex > removedIndex)
        nextIndex = m_currentIndex - 1;

    if (nextIndex != m_currentIndex) {
        m_currentIndex = nextIndex;
        emit currentIndexChanged(m_currentIndex);
    }
}

QWidget* StackContentHost::stackWidgetAt(int index) const
{
    if (index < 0 || index >= m_pages.size())
        return nullptr;
    return m_pages.at(index).stackWidget.data();
}

} // namespace fluent::navigation
