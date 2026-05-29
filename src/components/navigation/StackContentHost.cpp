#include "StackContentHost.h"

#include <utility>

#include <QParallelAnimationGroup>
#include <QPalette>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QStackedLayout>

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
    if (m_transitionGroup) {
        m_transitionGroup->stop();
        m_transitionGroup->deleteLater();
        m_transitionGroup = nullptr;
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

    PageRecord oldPage = m_pages.at(index);
    QWidget* oldContent = oldPage.content.data();
    removeStackWidget(oldPage.stackWidget);
    if (oldPage.placeholder) {
        deletePlaceholder(oldPage);
        oldContent = nullptr;
    } else if (oldContent) {
        oldContent->setParent(nullptr);
    }

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

    PageRecord page = m_pages.takeAt(index);
    QWidget* content = page.content.data();
    removeStackWidget(page.stackWidget);
    if (page.placeholder) {
        deletePlaceholder(page);
        content = nullptr;
    } else if (content) {
        content->setParent(nullptr);
    }

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
    for (const PageRecord& page : std::as_const(m_pages)) {
        removeStackWidget(page.stackWidget);
        if (page.placeholder) {
            deletePlaceholder(page);
        } else if (QWidget* content = page.content.data()) {
            content->setParent(nullptr);
        }
    }
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
    Q_UNUSED(direction)
    const int normalized = index >= 0 && index < m_pages.size() ? index : -1;
    if (m_currentIndex == normalized)
        return;

    if (m_transitionGroup) {
        m_transitionGroup->stop();
        m_transitionGroup->deleteLater();
        m_transitionGroup = nullptr;
        setBusy(false);
        showOnlyStackWidget(stackWidgetAt(m_currentIndex));
    }

    QWidget* fromWidget = stackWidgetAt(m_currentIndex);
    QWidget* toWidget = stackWidgetAt(normalized);
    m_currentIndex = normalized;
    emit currentIndexChanged(m_currentIndex);

    if (!toWidget) {
        m_layout->setStackingMode(QStackedLayout::StackOne);
        showOnlyStackWidget(nullptr);
        return;
    }

    if (!canAnimate(fromWidget, toWidget, animated)) {
        m_layout->setStackingMode(QStackedLayout::StackOne);
        m_layout->setCurrentWidget(toWidget);
        showOnlyStackWidget(toWidget);
        return;
    }

    const QRect endRect = rect();
    const QPoint startOffset = transitionStartOffset(endRect);

    Q_UNUSED(fromWidget)
    m_layout->setStackingMode(QStackedLayout::StackOne);
    m_layout->setCurrentWidget(toWidget);
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

void StackContentHost::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged(m_busy);
}

void StackContentHost::finishTransition(int targetIndex, QWidget* toWidget)
{
    if (m_transitionGroup) {
        m_transitionGroup->deleteLater();
        m_transitionGroup = nullptr;
    }

    m_layout->setStackingMode(QStackedLayout::StackOne);
    if (targetIndex >= 0 && targetIndex < m_pages.size() && toWidget) {
        m_layout->setCurrentWidget(toWidget);
        showOnlyStackWidget(toWidget);
    } else {
        showOnlyStackWidget(nullptr);
    }
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

QPoint StackContentHost::transitionStartOffset(const QRect& rect) const
{
    switch (m_transitionEffect) {
    case TransitionEffect::SlideFromLeft: {
        const int travel = qMax(1, qRound(rect.width() * 0.28));
        return QPoint(-travel, 0);
    }
    case TransitionEffect::SlideFromBottom: {
        const int travel = qMax(1, qRound(rect.height() * 0.28));
        return QPoint(0, travel);
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
        if (stackWidget == currentWidget) {
            stackWidget->setGeometry(rect());
            stackWidget->show();
            stackWidget->raise();
        } else {
            stackWidget->hide();
        }
    }
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
