#include "StackContentHost.h"

#include <utility>

#include <QParallelAnimationGroup>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QStackedLayout>

#include "components/foundation/overlay/OverlayGeometry.h"

namespace fluent::navigation {

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

void StackContentHost::paintEvent(QPaintEvent*)
{
    QPainter painter(this);

    // Transparent widgets share the top-level backing store. Replace this region on every
    // backdrop frame so pixels from an outgoing page cannot survive a stack switch.
    // zh_CN: 透明控件共享顶层后备缓冲；每个背景帧都替换此区域，避免切页后保留旧页面像素。
    if (window()
        && window()->testAttribute(Qt::WA_TranslucentBackground)
        && window()->property("fluentMicaBackdrop").toBool()) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        return;
    }

    if (!m_surfaceFill.isValid() || m_surfaceFill.alpha() == 0)
        return;  // transparent host: nothing to paint (pages/backdrop show through)

    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF panelRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const bool rounded = m_surfaceTopLeftRadius > 0.0;
    const QPainterPath panel = fluent::overlay::roundedCornerRectPath(
        panelRect, m_surfaceTopLeftRadius, /*TL*/ rounded, /*TR*/ false, /*BR*/ false, /*BL*/ false);

    painter.fillPath(panel, m_surfaceFill);  // opaque layer — survives a translucent top-level
    if (m_surfaceBorder.isValid() && m_surfaceBorder.alpha() > 0) {
        painter.setPen(QPen(m_surfaceBorder, 1.0));
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
    if (index < 0 || index > m_pages.size())
        return false;

    PageRecord page = makePage(widget);
    m_pages.insert(index, page);
    m_layout->insertWidget(index, page.stackWidget);
    page.stackWidget->hide();
    if (m_currentIndex >= index)
        ++m_currentIndex;
    return true;
}

QWidget* StackContentHost::replacePage(int index, QWidget* widget)
{
    if (index < 0 || index >= m_pages.size())
        return nullptr;

    QWidget* oldContent = detachPage(m_pages.at(index));

    PageRecord newPage = makePage(widget);
    m_pages[index] = newPage;
    m_layout->insertWidget(index, newPage.stackWidget);
    if (m_currentIndex == index) {
        m_layout->setCurrentWidget(newPage.stackWidget);
        newPage.stackWidget->setGeometry(rect());
        newPage.stackWidget->show();
    } else {
        newPage.stackWidget->hide();
    }
    return oldContent;
}

QWidget* StackContentHost::takePage(int index)
{
    if (index < 0 || index >= m_pages.size())
        return nullptr;

    QWidget* content = detachPage(m_pages.takeAt(index));

    normalizeCurrentIndexAfterRemoval(index);
    if (m_currentIndex >= 0 && m_currentIndex < m_pages.size()) {
        if (QWidget* current = stackWidgetAt(m_currentIndex)) {
            m_layout->setCurrentWidget(current);
            current->show();
        }
    }
    return content;
}

void StackContentHost::clearPages()
{
    for (const PageRecord& page : std::as_const(m_pages))
        detachPage(page);
    m_pages.clear();
    const bool changed = m_currentIndex != -1;
    m_currentIndex = -1;
    if (changed)
        emit currentIndexChanged(-1);
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
    QPalette pal = palette();
    pal.setColor(QPalette::Window, themeColors().bgLayer);
    setPalette(pal);
    setAutoFillBackground(true);
    update();
}

void StackContentHost::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_busy)
        return;
    for (const PageRecord& page : std::as_const(m_pages)) {
        if (QWidget* stackWidget = page.stackWidget.data())
            stackWidget->setGeometry(rect());
    }
}

StackContentHost::PageRecord StackContentHost::makePage(QWidget* widget)
{
    PageRecord page;
    if (widget) {
        page.content = widget;
        page.stackWidget = widget;
        page.placeholder = false;
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
    }
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

QWidget* StackContentHost::detachPage(const PageRecord& page)
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
    for (const PageRecord& page : std::as_const(m_pages)) {
        QWidget* stackWidget = page.stackWidget.data();
        if (!stackWidget)
            continue;
        stackWidget->hide();
    }

    const bool translucentBackdrop = window()
        && window()->testAttribute(Qt::WA_TranslucentBackground)
        && window()->property("fluentMicaBackdrop").toBool();
    if (translucentBackdrop && isVisible()) {
        // Clear synchronously while every page is hidden. An asynchronous update can run after
        // the incoming transparent page starts painting, leaving outgoing pixels underneath it.
        // zh_CN: 所有页面隐藏时同步清屏；异步 update 可能晚于新透明页面绘制，导致旧页面像素残留在下方。
        repaint();
    }

    if (currentWidget) {
        m_layout->setCurrentWidget(currentWidget);
        currentWidget->setGeometry(rect());
        currentWidget->show();
        currentWidget->raise();
    }
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
