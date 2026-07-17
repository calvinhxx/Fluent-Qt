#include "TabView.h"

#include <algorithm>

#include <QApplication>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QPointer>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScrollBar>
#include <QVariantAnimation>
#include <QWheelEvent>

#include "compatibility/QtCompat.h"
#include "design/CornerRadius.h"
#include "design/Typography.h"
#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"

namespace fluent::navigation {

namespace {
constexpr int kDefaultWidth = 640;
QPointer<TabView> s_activeShortcutOwner;

TabStrip::WidthMode toStripWidthMode(TabView::TabWidthMode mode)
{
    switch (mode) {
    case TabView::TabWidthMode::Equal:
        return TabStrip::WidthMode::Equal;
    case TabView::TabWidthMode::SizeToContent:
        return TabStrip::WidthMode::SizeToContent;
    case TabView::TabWidthMode::Compact:
        return TabStrip::WidthMode::Compact;
    }
    return TabStrip::WidthMode::Equal;
}

TabStrip::CloseButtonOverlayMode toStripCloseMode(TabView::CloseButtonOverlayMode mode)
{
    switch (mode) {
    case TabView::CloseButtonOverlayMode::Auto:
        return TabStrip::CloseButtonOverlayMode::Auto;
    case TabView::CloseButtonOverlayMode::OnHover:
        return TabStrip::CloseButtonOverlayMode::OnHover;
    case TabView::CloseButtonOverlayMode::Always:
        return TabStrip::CloseButtonOverlayMode::Always;
    }
    return TabStrip::CloseButtonOverlayMode::Auto;
}

bool forwardMouseEvent(TabStrip* target, QMouseEvent* event)
{
    if (!target || !target->geometry().contains(fluentMousePos(event)))
        return false;

    const QPoint localPoint = fluentMousePos(event) - target->pos();
    QMouseEvent forwarded(event->type(),
                          QPointF(localPoint),
                          QPointF(target->mapToGlobal(localPoint)),
                          event->button(),
                          event->buttons(),
                          event->modifiers());
    if (!target->handleForwardedMouseEvent(&forwarded))
        return false;
    event->accept();
    return true;
}

bool usesTabShortcutModifier(QKeyEvent* event)
{
    return event && (event->modifiers().testFlag(Qt::ControlModifier) || event->modifiers().testFlag(Qt::MetaModifier));
}

bool objectBelongsToWidget(QObject* object, QWidget* widget)
{
    auto* candidate = qobject_cast<QWidget*>(object);
    while (candidate) {
        if (candidate == widget)
            return true;
        candidate = candidate->parentWidget();
    }
    return false;
}
} // namespace

namespace {
constexpr int kDragIndicatorWidth = 2;

QPainterPath topRoundedTabPath(const QRect& rect, int radius)
{
    QPainterPath path;
    if (rect.isEmpty())
        return path;

    path.moveTo(rect.left(), rect.bottom() + 1);
    path.lineTo(rect.left(), rect.top() + radius);
    path.quadTo(rect.left(), rect.top(), rect.left() + radius, rect.top());
    path.lineTo(rect.right() - radius, rect.top());
    path.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + radius);
    path.lineTo(rect.right(), rect.bottom() + 1);
    path.closeSubpath();
    return path;
}
}

TabStrip::TabStrip(QWidget* parent)
    : QWidget(parent)
    , m_overflowScrollBar(new QScrollBar(Qt::Horizontal, this))
    , m_indicatorAnimation(new QPropertyAnimation(this, "animatedIndicatorRect", this))
    , m_addButton(createIconButton(Typography::Icons::Add, metrics().iconPixelSize))
    , m_overflowBackButton(createIconButton(Typography::Icons::ChevronLeftMed, metrics().iconPixelSize))
    , m_overflowForwardButton(createIconButton(Typography::Icons::ChevronRightMed, metrics().iconPixelSize))
{
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setFont(tabFont());
    m_indicatorAnimation->setDuration(themeAnimation().fast);
    m_indicatorAnimation->setEasingCurve(themeAnimation().decelerate);
    m_overflowScrollBar->hide();
    m_overflowScrollBar->setSingleStep(1);
    m_overflowScrollBar->setPageStep(1);

    connect(m_overflowScrollBar, &QScrollBar::valueChanged, this, [this](int value) {
        m_firstVisibleTab = value;
        if (!m_updatingOverflowScrollBar)
            invalidateLayout();
    });

    connect(m_addButton, &basicinput::Button::clicked, this, [this]() {
        activateHit(HitRecord{HitKind::Add, -1});
    });
    connect(m_overflowBackButton, &basicinput::Button::clicked, this, [this]() {
        activateHit(HitRecord{HitKind::OverflowBack, -1});
    });
    connect(m_overflowForwardButton, &basicinput::Button::clicked, this, [this]() {
        activateHit(HitRecord{HitKind::OverflowForward, -1});
    });
}

void TabStrip::setItems(const QVector<TabViewItem>& items)
{
    m_items = items;
    clearDragState();
    stopDragOffsetAnimation();
    m_dragAnimatedOffsets.clear();
    m_dragStartIndex = -1;
    for (auto it = m_tabRevealAnimations.begin(); it != m_tabRevealAnimations.end();) {
        if (isValidIndex(it.key())) {
            ++it;
            continue;
        }
        if (it.value()) {
            it.value()->stop();
            it.value()->deleteLater();
        }
        m_tabRevealProgress.remove(it.key());
        it = m_tabRevealAnimations.erase(it);
    }
    for (auto it = m_compactExpansionAnimations.begin(); it != m_compactExpansionAnimations.end();) {
        if (isValidIndex(it.key())) {
            ++it;
            continue;
        }
        if (it.value()) {
            it.value()->stop();
            it.value()->deleteLater();
        }
        m_compactExpansionProgress.remove(it.key());
        it = m_compactExpansionAnimations.erase(it);
    }
    if (!isSelectableIndex(m_selectedIndex))
        m_selectedIndex = firstEnabledIndex();
    if (m_firstVisibleTab >= m_items.size())
        m_firstVisibleTab = qMax(0, m_items.size() - 1);
    m_hoveredHit = HitRecord();
    m_pressedHit = HitRecord();
    if (!isInteractiveHit(m_focusedHit))
        m_focusedHit = HitRecord{m_selectedIndex >= 0 ? HitKind::Tab : HitKind::None, m_selectedIndex};
    invalidateLayout();
}

void TabStrip::revealTab(int index)
{
    if (!isValidIndex(index))
        return;
    stopRevealAnimation(index);
    m_tabRevealProgress[index] = 0.0;

    auto* animation = new QVariantAnimation(this);
    animation->setDuration(themeAnimation().fast);
    animation->setEasingCurve(themeAnimation().decelerate);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    connect(animation, &QVariantAnimation::valueChanged, this, [this, index](const QVariant& value) {
        setTabRevealOpacity(index, value.toReal());
    });
    connect(animation, &QVariantAnimation::finished, this, [this, index, animation]() {
        if (m_tabRevealAnimations.value(index) == animation)
            m_tabRevealAnimations.remove(index);
        m_tabRevealProgress.remove(index);
        animation->deleteLater();
        update();
    });
    m_tabRevealAnimations[index] = animation;
    animation->start();
    invalidateLayout();
}

void TabStrip::setSelectedIndex(int index)
{
    const int normalized = isSelectableIndex(index) ? index : -1;
    if (m_selectedIndex == normalized)
        return;

    ensureLayout();
    const QRect oldIndicator = indicatorRectForTab(m_selectedIndex);
    const int previous = m_selectedIndex;
    m_selectedIndex = normalized;
    if (m_focusedHit.kind == HitKind::None || m_focusedHit.tabIndex == previous)
        m_focusedHit = HitRecord{normalized >= 0 ? HitKind::Tab : HitKind::None, normalized};
    if (isValidIndex(normalized))
        ensureSelectedTabVisible();

    invalidateLayout();
    ensureLayout();
    const QRect newIndicator = indicatorRectForTab(m_selectedIndex);
    animateIndicator(oldIndicator, newIndicator);
}

void TabStrip::setWidthMode(WidthMode mode)
{
    if (m_widthMode == mode)
        return;
    m_widthMode = mode;
    invalidateLayout();
}

void TabStrip::setCloseButtonOverlayMode(CloseButtonOverlayMode mode)
{
    if (m_closeButtonOverlayMode == mode)
        return;
    m_closeButtonOverlayMode = mode;
    invalidateLayout();
}

void TabStrip::setTabsClosable(bool closable)
{
    if (m_tabsClosable == closable)
        return;
    m_tabsClosable = closable;
    invalidateLayout();
}

void TabStrip::setAddButtonVisible(bool visible)
{
    if (m_addButtonVisible == visible)
        return;
    m_addButtonVisible = visible;
    invalidateLayout();
}

void TabStrip::setTabReorderEnabled(bool enabled)
{
    if (m_tabReorderEnabled == enabled)
        return;
    m_tabReorderEnabled = enabled;
    update();
}

void TabStrip::setKeyboardAcceleratorsEnabled(bool enabled)
{
    m_keyboardAcceleratorsEnabled = enabled;
}

void TabStrip::setTabFontRole(const QString& role)
{
    const QString normalized = normalizedString(role, QStringLiteral("Body"));
    if (m_tabFontRole == normalized)
        return;
    m_tabFontRole = normalized;
    setFont(tabFont());
    invalidateLayout();
}

void TabStrip::setIconFontFamily(const QString& family)
{
    const QString normalized = normalizedString(family, Typography::FontFamily::FluentIcons);
    if (m_iconFontFamily == normalized)
        return;
    m_iconFontFamily = normalized;
    invalidateLayout();
}

QRect TabStrip::tabGeometry(int index) const
{
    ensureLayout();
    if (const TabRecord* record = recordForTab(index))
        return record->tabRect;
    return QRect();
}

QRect TabStrip::closeButtonGeometry(int index) const
{
    ensureLayout();
    if (const TabRecord* record = recordForTab(index))
        return record->closeVisible ? record->closeRect : QRect();
    return QRect();
}

QRect TabStrip::addButtonGeometry() const
{
    ensureLayout();
    return m_addButtonRect;
}

QRect TabStrip::overflowBackGeometry() const
{
    ensureLayout();
    return m_overflowBackRect;
}

QRect TabStrip::overflowForwardGeometry() const
{
    ensureLayout();
    return m_overflowForwardRect;
}

QVector<int> TabStrip::visibleTabIndexes() const
{
    ensureLayout();
    return m_visibleTabIndexes;
}

int TabStrip::rowHeight() const
{
    return metrics().rowHeight;
}

bool TabStrip::handleForwardedMouseEvent(QMouseEvent* event)
{
    if (!event)
        return false;
    event->setAccepted(false);
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        mousePressEvent(event);
        break;
    case QEvent::MouseMove:
        mouseMoveEvent(event);
        break;
    case QEvent::MouseButtonRelease:
        mouseReleaseEvent(event);
        break;
    default:
        return false;
    }
    return event->isAccepted();
}

bool TabStrip::handleForwardedKeyEvent(QKeyEvent* event)
{
    if (!event)
        return false;
    event->setAccepted(false);
    keyPressEvent(event);
    return event->isAccepted();
}

bool TabStrip::handleForwardedWheelEvent(QWheelEvent* event)
{
    if (!event)
        return false;
    event->setAccepted(false);
    wheelEvent(event);
    return event->isAccepted();
}

QSize TabStrip::sizeHint() const
{
    return QSize(640, metrics().rowHeight);
}

QSize TabStrip::minimumSizeHint() const
{
    const Metrics currentMetrics = metrics();
    return QSize(currentMetrics.buttonWidth * 3, currentMetrics.rowHeight);
}

void TabStrip::onThemeUpdated()
{
    setFont(tabFont());
    m_indicatorAnimation->setDuration(themeAnimation().fast);
    m_indicatorAnimation->setEasingCurve(themeAnimation().decelerate);
    for (TabHeaderWidgets& widgets : m_headerWidgets) {
        if (widgets.label)
            widgets.label->onThemeUpdated();
        if (widgets.closeButton)
            widgets.closeButton->onThemeUpdated();
    }
    if (m_addButton)
        m_addButton->onThemeUpdated();
    if (m_overflowBackButton)
        m_overflowBackButton->onThemeUpdated();
    if (m_overflowForwardButton)
        m_overflowForwardButton->onThemeUpdated();
    invalidateLayout();
}

void TabStrip::paintEvent(QPaintEvent*)
{
    ensureLayout();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    paintRow(painter);
    for (const TabRecord& record : m_tabRecords) {
        if (record.tabIndex != m_selectedIndex && !(m_dragActive && record.tabIndex == m_dragStartIndex))
            paintTab(painter, record);
    }
    if (!(m_dragActive && m_selectedIndex == m_dragStartIndex)) {
        if (const TabRecord* selectedRecord = recordForTab(m_selectedIndex))
            paintTab(painter, *selectedRecord);
    }
    if (m_dragActive) {
        if (const TabRecord* draggedRecord = recordForTab(m_dragStartIndex))
            paintTab(painter, *draggedRecord);
        paintDragInsertionIndicator(painter);
    }
}

void TabStrip::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    invalidateLayout();
}

void TabStrip::enterEvent(FluentEnterEvent* event)
{
    QWidget::enterEvent(event);
}

void TabStrip::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    setHoveredHit(HitRecord());
    if (!m_dragActive)
        clearPressedHit();
}

void TabStrip::mousePressEvent(QMouseEvent* event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    setFocus(Qt::MouseFocusReason);
    m_focusVisualVisible = false;
    clearDragState();
    const HitRecord hit = hitTest(event->pos());
    if (isInteractiveHit(hit)) {
        m_pressedHit = hit;
        m_focusedHit = hit;
        m_pressPosition = event->pos();
        m_dragStartIndex = hit.kind == HitKind::Tab ? hit.tabIndex : -1;
        m_dragTargetIndex = m_dragStartIndex;
        m_dragPosition = event->pos();
        if (const TabRecord* record = recordForTab(m_dragStartIndex))
            m_dragMouseOffset = event->pos().x() - record->tabRect.left();
        update();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TabStrip::mouseMoveEvent(QMouseEvent* event)
{
    if (!isEnabled()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    setHoveredHit(hitTest(event->pos()));
    if (m_tabReorderEnabled && m_dragStartIndex >= 0 && event->buttons().testFlag(Qt::LeftButton)) {
        if (!m_dragActive && (event->pos() - m_pressPosition).manhattanLength() >= QApplication::startDragDistance()) {
            m_dragActive = true;
            m_dragAnimatedOffsets = dragOffsetMapForTarget(m_dragTargetIndex);
        }
        if (m_dragActive) {
            m_dragPosition = event->pos();
            setDragTargetIndex(dropTargetIndexForPosition(event->pos()));
            updateHeaderWidgets();
            update();
            event->accept();
            return;
        }
    }

    event->accept();
}

void TabStrip::mouseReleaseEvent(QMouseEvent* event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const HitRecord hit = hitTest(event->pos());
    const HitRecord pressed = m_pressedHit;
    const int dragStart = m_dragStartIndex;
    const int dragTarget = isValidIndex(m_dragTargetIndex) ? m_dragTargetIndex : dropTargetIndexForPosition(event->pos());
    const bool wasDragging = m_dragActive;
    clearPressedHit();

    if (wasDragging) {
        if (m_tabReorderEnabled && isValidIndex(dragStart) && isValidIndex(dragTarget) && dragStart != dragTarget)
            emit tabMoveRequested(dragStart, dragTarget);
        clearDragState();
        m_dragStartIndex = -1;
        event->accept();
        return;
    }

    clearDragState();
    m_dragStartIndex = -1;

    if (sameHit(pressed, hit) && isInteractiveHit(hit)) {
        activateHit(hit);
        event->accept();
        return;
    }

    update();
    QWidget::mouseReleaseEvent(event);
}

void TabStrip::wheelEvent(QWheelEvent* event)
{
    if (!isEnabled() || !m_overflowScrollBar || m_overflowScrollBar->minimum() == m_overflowScrollBar->maximum()) {
        m_wheelScrollRemainder = 0;
        QWidget::wheelEvent(event);
        return;
    }

    QPoint delta = event->pixelDelta();
    int threshold = 80;
    if (delta.isNull()) {
        delta = event->angleDelta();
        threshold = 120;
    }
    const int dominantDelta = qAbs(delta.x()) > qAbs(delta.y()) ? delta.x() : delta.y();
    if (dominantDelta == 0) {
        QWidget::wheelEvent(event);
        return;
    }

    m_wheelScrollRemainder += dominantDelta;
    const int steps = m_wheelScrollRemainder / threshold;
    if (steps == 0) {
        event->accept();
        return;
    }

    m_wheelScrollRemainder -= steps * threshold;
    m_overflowScrollBar->setValue(m_overflowScrollBar->value() - steps * m_overflowScrollBar->singleStep());
    event->accept();
}

void TabStrip::keyPressEvent(QKeyEvent* event)
{
    if (!isEnabled()) {
        QWidget::keyPressEvent(event);
        return;
    }

    const bool control = event->modifiers().testFlag(Qt::ControlModifier);
    if (m_keyboardAcceleratorsEnabled && control) {
        if (event->key() == Qt::Key_T) {
            emit addTabRequested();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_W) {
            emitCloseRequested(m_selectedIndex);
            event->accept();
            return;
        }
        if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_8) {
            const int shortcutIndex = event->key() - Qt::Key_1;
            if (isSelectableIndex(shortcutIndex))
                emit selectedIndexRequested(shortcutIndex);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_9) {
            emit selectedIndexRequested(lastEnabledIndex());
            event->accept();
            return;
        }
    }

    if (m_keyboardAcceleratorsEnabled && event->key() == Qt::Key_Delete) {
        emitCloseRequested(m_selectedIndex);
        event->accept();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Left:
        m_focusVisualVisible = true;
        m_focusedHit = HitRecord{HitKind::Tab, nextEnabledIndex(m_focusedHit.tabIndex >= 0 ? m_focusedHit.tabIndex : m_selectedIndex, -1)};
        ensureSelectedTabVisible();
        invalidateLayout();
        event->accept();
        return;
    case Qt::Key_Right:
        m_focusVisualVisible = true;
        m_focusedHit = HitRecord{HitKind::Tab, nextEnabledIndex(m_focusedHit.tabIndex >= 0 ? m_focusedHit.tabIndex : m_selectedIndex, 1)};
        ensureSelectedTabVisible();
        invalidateLayout();
        event->accept();
        return;
    case Qt::Key_Home:
        m_focusVisualVisible = true;
        m_focusedHit = HitRecord{HitKind::Tab, firstEnabledIndex()};
        invalidateLayout();
        event->accept();
        return;
    case Qt::Key_End:
        m_focusVisualVisible = true;
        m_focusedHit = HitRecord{HitKind::Tab, lastEnabledIndex()};
        invalidateLayout();
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (isInteractiveHit(m_focusedHit)) {
            activateHit(m_focusedHit);
            event->accept();
            return;
        }
        break;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void TabStrip::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    m_focusVisualVisible = event->reason() != Qt::MouseFocusReason;
    if (!isInteractiveHit(m_focusedHit))
        m_focusedHit = HitRecord{HitKind::Tab, m_selectedIndex >= 0 ? m_selectedIndex : firstEnabledIndex()};
    update();
}

void TabStrip::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    m_focusVisualVisible = false;
    clearPressedHit();
    update();
}

bool TabStrip::eventFilter(QObject* watched, QEvent* event)
{
    for (const TabHeaderWidgets& widgets : m_headerWidgets) {
        if (widgets.closeButton != watched)
            continue;
        if (event->type() == QEvent::Enter || event->type() == QEvent::MouseMove) {
            setHoveredHit(HitRecord{HitKind::Close, widgets.tabIndex});
            break;
        }
        if (event->type() == QEvent::Leave) {
            if (sameHit(m_hoveredHit, HitRecord{HitKind::Close, widgets.tabIndex}))
                setHoveredHit(HitRecord());
            break;
        }
        if (event->type() == QEvent::MouseButtonPress) {
            m_pressedHit = HitRecord{HitKind::Close, widgets.tabIndex};
            update();
            break;
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            if (sameHit(m_pressedHit, HitRecord{HitKind::Close, widgets.tabIndex}))
                clearPressedHit();
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool TabStrip::isValidIndex(int index) const
{
    return index >= 0 && index < m_items.size();
}

bool TabStrip::isSelectableIndex(int index) const
{
    return isValidIndex(index) && m_items.at(index).enabled;
}

bool TabStrip::isCloseableIndex(int index) const
{
    return isValidIndex(index) && m_items.at(index).enabled && m_items.at(index).closable && m_tabsClosable;
}

TabStrip::Metrics TabStrip::metrics() const
{
    Metrics current;
    current.cornerRadius = CornerRadius::Control;
    return current;
}

QFont TabStrip::tabFont() const
{
    return themeFont(m_tabFontRole).toQFont();
}

QFont TabStrip::iconFont(int pixelSize) const
{
    QFont font(m_iconFontFamily);
    font.setPixelSize(pixelSize);
    return font;
}

QString TabStrip::normalizedString(const QString& value, const QString& fallback) const
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? fallback : trimmed;
}

void TabStrip::invalidateLayout()
{
    m_layoutDirty = true;
    updateGeometry();
    update();
}

void TabStrip::ensureLayout() const
{
    if (!m_layoutDirty)
        return;
    const_cast<TabStrip*>(this)->updateLayout();
}

void TabStrip::updateLayout()
{
    const Metrics currentMetrics = metrics();
    const QRect rowRect(contentsRect().left(), contentsRect().top(), contentsRect().width(), currentMetrics.rowHeight);
    m_tabRecords.clear();
    m_visibleTabIndexes.clear();
    m_addButtonRect = QRect();
    m_overflowBackRect = QRect();
    m_overflowForwardRect = QRect();

    int rightReserve = 0;
    if (m_addButtonVisible)
        rightReserve += currentMetrics.buttonWidth;

    QFontMetrics fontMetrics(tabFont());
    QVector<int> widths;
    widths.reserve(m_items.size());
    for (int index = 0; index < m_items.size(); ++index)
        widths.append(naturalTabWidth(index, fontMetrics, shouldReserveCloseSpace(index)));

    int stripWidth = qMax(0, rowRect.width() - rightReserve);
    int totalNaturalWidth = 0;
    for (int widthValue : widths)
        totalNaturalWidth += widthValue;
    const bool overflow = totalNaturalWidth > stripWidth && !m_items.isEmpty();
    if (overflow)
        stripWidth = qMax(0, stripWidth - currentMetrics.buttonWidth * 2);

    syncOverflowScrollBar(overflow);
    m_visibleTabIndexes = computeVisibleIndexes(stripWidth, widths, overflow);
    if (!overflow)
        setOverflowScrollValue(0);

    int x = rowRect.left();
    if (overflow) {
        m_overflowBackRect = QRect(x, rowRect.top() + (rowRect.height() - currentMetrics.tabHeight) / 2, currentMetrics.buttonWidth, currentMetrics.tabHeight);
        x += currentMetrics.buttonWidth;
    }

    int availableForTabs = overflow ? stripWidth : qMax(0, rowRect.width() - rightReserve);
    QVector<int> assignedWidths;
    assignedWidths.reserve(m_visibleTabIndexes.size());
    if (m_widthMode == WidthMode::Equal && !m_visibleTabIndexes.isEmpty()) {
        const int equalWidth = qBound(currentMetrics.minTabWidth, availableForTabs / m_visibleTabIndexes.size(), currentMetrics.maxTabWidth);
        for (int index : m_visibleTabIndexes) {
            Q_UNUSED(index)
            assignedWidths.append(equalWidth);
        }
    } else {
        for (int index : m_visibleTabIndexes)
            assignedWidths.append(widths.value(index, currentMetrics.preferredTabWidth));
    }

    int assignedTotal = 0;
    for (int value : assignedWidths)
        assignedTotal += value;
    while (assignedTotal > availableForTabs && !assignedWidths.isEmpty()) {
        bool changed = false;
        for (int i = assignedWidths.size() - 1; i >= 0 && assignedTotal > availableForTabs; --i) {
            const int tabIndex = m_visibleTabIndexes.at(i);
            int minWidth = m_widthMode == WidthMode::Compact && tabIndex != m_selectedIndex ? currentMetrics.compactInactiveWidth : currentMetrics.minTabWidth;
            if (m_widthMode == WidthMode::Compact && tabIndex != m_selectedIndex && compactExpansionProgress(tabIndex) > 0.0) {
                minWidth = qMax(minWidth,
                                currentMetrics.horizontalPadding * 2 + currentMetrics.iconSlot + currentMetrics.textGap + currentMetrics.closeButtonSize);
            }
            if (assignedWidths[i] > minWidth) {
                --assignedWidths[i];
                --assignedTotal;
                changed = true;
            }
        }
        if (!changed)
            break;
    }

    for (int i = 0; i < m_visibleTabIndexes.size(); ++i) {
        const int tabIndex = m_visibleTabIndexes.at(i);
        const int tabWidth = qMax(currentMetrics.minTabWidth, assignedWidths.value(i, currentMetrics.minTabWidth));
        TabRecord record;
        record.tabIndex = tabIndex;
        record.tabRect = QRect(x,
                               rowRect.top() + (rowRect.height() - currentMetrics.tabHeight) / 2,
                               qMin(tabWidth, rowRect.right() + 1 - x - rightReserve - (overflow ? currentMetrics.buttonWidth : 0)),
                               currentMetrics.tabHeight);
        const bool hasIcon = !m_items.at(tabIndex).iconGlyph.isEmpty();
        int contentX = record.tabRect.left() + currentMetrics.horizontalPadding;
        if (hasIcon || m_widthMode == WidthMode::Compact) {
            record.iconRect = QRect(contentX, record.tabRect.top() + (record.tabRect.height() - currentMetrics.iconSlot) / 2, currentMetrics.iconSlot, currentMetrics.iconSlot);
            contentX = record.iconRect.right() + 1 + currentMetrics.textGap;
        }
        const bool reserveClose = shouldReserveCloseSpace(tabIndex);
        if (reserveClose) {
            record.closeRect = QRect(record.tabRect.right() - currentMetrics.horizontalPadding - currentMetrics.closeButtonSize + 1,
                                     record.tabRect.top() + (record.tabRect.height() - currentMetrics.closeButtonSize) / 2,
                                     currentMetrics.closeButtonSize,
                                     currentMetrics.closeButtonSize);
            const bool animatingCompactClose = m_widthMode == WidthMode::Compact && tabIndex != m_selectedIndex;
            record.closeVisible = shouldShowCloseButton(tabIndex) && (!animatingCompactClose || compactExpansionProgress(tabIndex) >= 0.95);
        }
        const int textRight = reserveClose ? record.closeRect.left() - currentMetrics.textGap : record.tabRect.right() - currentMetrics.horizontalPadding;
        record.textRect = QRect(contentX, record.tabRect.top(), qMax(0, textRight - contentX + 1), record.tabRect.height());
        if (m_widthMode == WidthMode::Compact && tabIndex != m_selectedIndex)
            record.textRect = QRect();
        m_tabRecords.append(record);
        x += tabWidth;
    }

    if (overflow)
        m_overflowForwardRect = QRect(rowRect.right() - rightReserve - currentMetrics.buttonWidth + 1, rowRect.top() + (rowRect.height() - currentMetrics.tabHeight) / 2, currentMetrics.buttonWidth, currentMetrics.tabHeight);
    if (m_addButtonVisible)
        m_addButtonRect = QRect(rowRect.right() - currentMetrics.buttonWidth + 1, rowRect.top() + (rowRect.height() - currentMetrics.tabHeight) / 2, currentMetrics.buttonWidth, currentMetrics.tabHeight);

    if (m_indicatorAnimation->state() != QAbstractAnimation::Running)
        m_animatedIndicatorRect = indicatorRectForTab(m_selectedIndex);
    m_layoutDirty = false;
    updateHeaderWidgets();
}

int TabStrip::dropTargetIndexForPosition(const QPoint& position) const
{
    ensureLayout();
    if (m_tabRecords.isEmpty())
        return -1;

    const int dragStart = isValidIndex(m_dragStartIndex) ? m_dragStartIndex : -1;
    int lastCandidate = -1;
    for (const TabRecord& record : m_tabRecords) {
        if (record.tabIndex == dragStart)
            continue;
        lastCandidate = record.tabIndex;
        if (position.x() < record.tabRect.center().x()) {
            const int target = dragStart >= 0 && dragStart < record.tabIndex ? record.tabIndex - 1 : record.tabIndex;
            return qBound(0, target, qMax(0, m_items.size() - 1));
        }
    }
    if (lastCandidate < 0)
        return dragStart;
    const int target = dragStart > lastCandidate ? lastCandidate + 1 : lastCandidate;
    return qBound(0, target, qMax(0, m_items.size() - 1));
}

int TabStrip::dragOffsetForRecord(const TabRecord& record) const
{
    if (!m_dragActive || !isValidIndex(m_dragStartIndex) || !isValidIndex(m_dragTargetIndex))
        return 0;

    if (record.tabIndex == m_dragStartIndex) {
        const TabRecord* dragged = recordForTab(m_dragStartIndex);
        if (!dragged)
            return 0;
        const QRect bounds = contentsRect();
        const int maximumLeft = qMax(bounds.left(), bounds.right() - dragged->tabRect.width() + 1);
        const int desiredLeft = qBound(bounds.left(), m_dragPosition.x() - m_dragMouseOffset, maximumLeft);
        return desiredLeft - dragged->tabRect.left();
    }

    if (m_dragAnimatedOffsets.contains(record.tabIndex))
        return qRound(m_dragAnimatedOffsets.value(record.tabIndex));

    return targetDragOffsetForRecord(record, m_dragTargetIndex);
}

int TabStrip::targetDragOffsetForRecord(const TabRecord& record, int targetIndex) const
{
    if (!m_dragActive || record.tabIndex == m_dragStartIndex || !isValidIndex(m_dragStartIndex)
        || !isValidIndex(targetIndex)) {
        return 0;
    }

    const TabRecord* dragged = recordForTab(m_dragStartIndex);
    if (!dragged)
        return 0;

    const int startPosition = m_visibleTabIndexes.indexOf(m_dragStartIndex);
    const int targetPosition = m_visibleTabIndexes.indexOf(targetIndex);
    const int recordPosition = m_visibleTabIndexes.indexOf(record.tabIndex);
    if (startPosition < 0 || targetPosition < 0 || recordPosition < 0 || startPosition == targetPosition)
        return 0;

    const int draggedWidth = dragged->tabRect.width();
    if (startPosition < targetPosition && recordPosition > startPosition && recordPosition <= targetPosition)
        return -draggedWidth;
    if (targetPosition < startPosition && recordPosition >= targetPosition && recordPosition < startPosition)
        return draggedWidth;
    return 0;
}

QHash<int, qreal> TabStrip::dragOffsetMapForTarget(int targetIndex) const
{
    QHash<int, qreal> offsets;
    if (!m_dragActive)
        return offsets;

    for (const TabRecord& record : m_tabRecords) {
        if (record.tabIndex == m_dragStartIndex)
            continue;
        offsets.insert(record.tabIndex, targetDragOffsetForRecord(record, targetIndex));
    }
    return offsets;
}

void TabStrip::setDragTargetIndex(int index)
{
    const int normalized = isValidIndex(index) ? index : m_dragStartIndex;
    if (m_dragTargetIndex == normalized)
        return;

    QHash<int, qreal> startOffsets = m_dragAnimatedOffsets;
    for (const TabRecord& record : m_tabRecords) {
        if (record.tabIndex == m_dragStartIndex)
            continue;
        if (!startOffsets.contains(record.tabIndex))
            startOffsets.insert(record.tabIndex, targetDragOffsetForRecord(record, m_dragTargetIndex));
    }

    m_dragTargetIndex = normalized;
    if (!m_dragActive) {
        m_dragAnimatedOffsets.clear();
        return;
    }

    animateDragOffsets(startOffsets, dragOffsetMapForTarget(m_dragTargetIndex));
}

void TabStrip::animateDragOffsets(const QHash<int, qreal>& startOffsets, const QHash<int, qreal>& endOffsets)
{
    stopDragOffsetAnimation();

    bool changed = false;
    for (auto it = endOffsets.constBegin(); it != endOffsets.constEnd(); ++it) {
        if (!qFuzzyCompare(startOffsets.value(it.key()) + 1.0, it.value() + 1.0)) {
            changed = true;
            break;
        }
    }

    if (!changed) {
        m_dragAnimatedOffsets = endOffsets;
        updateHeaderWidgets();
        update();
        return;
    }

    auto* animation = new QVariantAnimation(this);
    animation->setDuration(qMax(90, themeAnimation().fast));
    animation->setEasingCurve(themeAnimation().decelerate);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    connect(animation, &QVariantAnimation::valueChanged, this, [this, startOffsets, endOffsets](const QVariant& value) {
        const qreal progress = qBound<qreal>(0.0, value.toReal(), 1.0);
        QHash<int, qreal> offsets;
        for (auto it = endOffsets.constBegin(); it != endOffsets.constEnd(); ++it) {
            const qreal start = startOffsets.value(it.key());
            offsets.insert(it.key(), start + (it.value() - start) * progress);
        }
        for (auto it = startOffsets.constBegin(); it != startOffsets.constEnd(); ++it) {
            if (offsets.contains(it.key()))
                continue;
            offsets.insert(it.key(), it.value() * (1.0 - progress));
        }
        m_dragAnimatedOffsets = offsets;
        updateHeaderWidgets();
        update();
    });
    connect(animation, &QVariantAnimation::finished, this, [this, animation, endOffsets]() {
        if (m_dragOffsetAnimation == animation)
            m_dragOffsetAnimation = nullptr;
        m_dragAnimatedOffsets = endOffsets;
        animation->deleteLater();
        updateHeaderWidgets();
        update();
    });
    m_dragOffsetAnimation = animation;
    animation->start();
}

void TabStrip::stopDragOffsetAnimation()
{
    if (!m_dragOffsetAnimation)
        return;
    m_dragOffsetAnimation->stop();
    m_dragOffsetAnimation->deleteLater();
    m_dragOffsetAnimation = nullptr;
}

QRect TabStrip::dragInsertionIndicatorRect() const
{
    if (!m_dragActive || !isValidIndex(m_dragStartIndex) || !isValidIndex(m_dragTargetIndex)
        || m_dragStartIndex == m_dragTargetIndex) {
        return QRect();
    }

    const TabRecord* targetRecord = recordForTab(m_dragTargetIndex);
    if (!targetRecord)
        return QRect();

    const int startPosition = m_visibleTabIndexes.indexOf(m_dragStartIndex);
    const int targetPosition = m_visibleTabIndexes.indexOf(m_dragTargetIndex);
    if (startPosition < 0 || targetPosition < 0 || startPosition == targetPosition)
        return QRect();

    const Metrics currentMetrics = metrics();
    const TabRecord visualTarget = visualRecordForRecord(*targetRecord);
    const int x = startPosition < targetPosition ? visualTarget.tabRect.right() + 1 : targetRecord->tabRect.left();
    const int height = qMax(12, currentMetrics.tabHeight - 10);
    const int y = targetRecord->tabRect.top() + (targetRecord->tabRect.height() - height) / 2;
    return QRect(x - kDragIndicatorWidth / 2, y, kDragIndicatorWidth, height);
}

TabStrip::TabRecord TabStrip::visualRecordForRecord(const TabRecord& record) const
{
    TabRecord visual = record;
    const int offset = dragOffsetForRecord(record);
    if (offset == 0)
        return visual;

    visual.tabRect.translate(offset, 0);
    visual.iconRect.translate(offset, 0);
    visual.textRect.translate(offset, 0);
    visual.closeRect.translate(offset, 0);
    return visual;
}

void TabStrip::clearDragState()
{
    const bool needsUpdate = m_dragActive || m_dragTargetIndex >= 0 || m_dragMouseOffset != 0 || !m_dragPosition.isNull();
    stopDragOffsetAnimation();
    m_dragActive = false;
    m_dragTargetIndex = -1;
    m_dragMouseOffset = 0;
    m_dragPosition = QPoint();
    m_dragAnimatedOffsets.clear();
    if (needsUpdate) {
        updateHeaderWidgets();
        update();
    }
}

bool TabStrip::compactExpansionTarget(int index) const
{
    return m_widthMode == WidthMode::Compact && isValidIndex(index) && index != m_selectedIndex && shouldShowCloseButton(index);
}

qreal TabStrip::compactExpansionProgress(int index) const
{
    if (m_compactExpansionProgress.contains(index))
        return qBound<qreal>(0.0, m_compactExpansionProgress.value(index), 1.0);
    return compactExpansionTarget(index) ? 1.0 : 0.0;
}

void TabStrip::animateCompactExpansion(int index, qreal start, qreal end)
{
    if (!isValidIndex(index))
        return;

    stopCompactExpansionAnimation(index);
    const qreal from = qBound<qreal>(0.0, start, 1.0);
    const qreal to = qBound<qreal>(0.0, end, 1.0);
    if (qFuzzyCompare(from, to)) {
        if (to <= 0.0)
            m_compactExpansionProgress.remove(index);
        else
            m_compactExpansionProgress[index] = to;
        invalidateLayout();
        return;
    }

    m_compactExpansionProgress[index] = from;
    auto* animation = new QVariantAnimation(this);
    animation->setDuration(themeAnimation().fast);
    animation->setEasingCurve(themeAnimation().decelerate);
    animation->setStartValue(from);
    animation->setEndValue(to);
    connect(animation, &QVariantAnimation::valueChanged, this, [this, index](const QVariant& value) {
        m_compactExpansionProgress[index] = qBound<qreal>(0.0, value.toReal(), 1.0);
        invalidateLayout();
    });
    connect(animation, &QVariantAnimation::finished, this, [this, index, animation, to]() {
        if (m_compactExpansionAnimations.value(index) == animation)
            m_compactExpansionAnimations.remove(index);
        if (to <= 0.0)
            m_compactExpansionProgress.remove(index);
        else
            m_compactExpansionProgress[index] = 1.0;
        animation->deleteLater();
        invalidateLayout();
    });
    m_compactExpansionAnimations[index] = animation;
    animation->start();
}

void TabStrip::stopCompactExpansionAnimation(int index)
{
    if (QVariantAnimation* animation = m_compactExpansionAnimations.take(index)) {
        animation->stop();
        animation->deleteLater();
    }
}

int TabStrip::naturalTabWidth(int index, const QFontMetrics& fontMetrics, bool reserveClose) const
{
    if (!isValidIndex(index))
        return 0;
    const Metrics currentMetrics = metrics();
    if (m_widthMode == WidthMode::Compact && index != m_selectedIndex) {
        const int expandedWidth = qMax(currentMetrics.compactInactiveWidth,
                                       currentMetrics.horizontalPadding * 2 + currentMetrics.iconSlot + currentMetrics.textGap + currentMetrics.closeButtonSize);
        const qreal progress = reserveClose ? compactExpansionProgress(index) : 0.0;
        const int widthValue = currentMetrics.compactInactiveWidth + qRound((expandedWidth - currentMetrics.compactInactiveWidth) * progress);
        return widthValue;
    }
    int widthValue = currentMetrics.horizontalPadding * 2;
    if (!m_items.at(index).iconGlyph.isEmpty() || m_widthMode == WidthMode::Compact)
        widthValue += currentMetrics.iconSlot + currentMetrics.textGap;
    widthValue += fontMetrics.horizontalAdvance(m_items.at(index).text);
    if (reserveClose)
        widthValue += currentMetrics.textGap + currentMetrics.closeButtonSize;
    return qBound(currentMetrics.minTabWidth, widthValue, currentMetrics.maxTabWidth);
}

bool TabStrip::shouldReserveCloseSpace(int index) const
{
    return isCloseableIndex(index);
}

bool TabStrip::shouldShowCloseButton(int index) const
{
    if (!isCloseableIndex(index))
        return false;
    const bool hovered = m_hoveredHit.tabIndex == index && (m_hoveredHit.kind == HitKind::Tab || m_hoveredHit.kind == HitKind::Close);
    const bool focused = hasFocus() && m_focusedHit.tabIndex == index && m_focusedHit.kind == HitKind::Tab;
    if (m_closeButtonOverlayMode == CloseButtonOverlayMode::Always)
        return true;
    if (m_closeButtonOverlayMode == CloseButtonOverlayMode::Auto)
        return index == m_selectedIndex || hovered || focused;
    return hovered || focused;
}

QVector<int> TabStrip::computeVisibleIndexes(int stripWidth, const QVector<int>& widths, bool overflow) const
{
    QVector<int> result;
    if (m_items.isEmpty())
        return result;
    if (!overflow) {
        for (int index = 0; index < m_items.size(); ++index)
            result.append(index);
        return result;
    }

    int used = 0;
    const int start = qBound(0, m_firstVisibleTab, qMax(0, m_items.size() - 1));
    for (int index = start; index < m_items.size(); ++index) {
        const int widthValue = widths.value(index, metrics().preferredTabWidth);
        if (!result.isEmpty() && used + widthValue > stripWidth)
            break;
        if (result.isEmpty() && widthValue > stripWidth && stripWidth >= metrics().minTabWidth) {
            result.append(index);
            break;
        }
        if (used + widthValue <= stripWidth || result.isEmpty()) {
            result.append(index);
            used += widthValue;
        }
    }
    if (result.isEmpty())
        result.append(start);
    return result;
}

void TabStrip::ensureSelectedTabVisible()
{
    const int target = m_focusedHit.kind == HitKind::Tab && isValidIndex(m_focusedHit.tabIndex) ? m_focusedHit.tabIndex : m_selectedIndex;
    if (!isValidIndex(target))
        return;
    if (target < m_firstVisibleTab)
        setOverflowScrollValue(target);
    if (!m_visibleTabIndexes.isEmpty() && target > m_visibleTabIndexes.last())
        setOverflowScrollValue(qMax(0, target - m_visibleTabIndexes.size() + 1));
    m_firstVisibleTab = qBound(0, m_firstVisibleTab, qMax(0, m_items.size() - 1));
}

int TabStrip::nextEnabledIndex(int from, int direction) const
{
    if (direction == 0 || m_items.isEmpty())
        return -1;
    int index = from;
    if (!isValidIndex(index))
        index = direction > 0 ? -1 : m_items.size();
    while (true) {
        index += direction > 0 ? 1 : -1;
        if (!isValidIndex(index))
            return -1;
        if (isSelectableIndex(index))
            return index;
    }
}

int TabStrip::firstEnabledIndex() const
{
    for (int index = 0; index < m_items.size(); ++index) {
        if (isSelectableIndex(index))
            return index;
    }
    return -1;
}

int TabStrip::lastEnabledIndex() const
{
    for (int index = m_items.size() - 1; index >= 0; --index) {
        if (isSelectableIndex(index))
            return index;
    }
    return -1;
}

TabStrip::TabRecord* TabStrip::recordForTab(int index)
{
    ensureLayout();
    for (TabRecord& record : m_tabRecords) {
        if (record.tabIndex == index)
            return &record;
    }
    return nullptr;
}

const TabStrip::TabRecord* TabStrip::recordForTab(int index) const
{
    ensureLayout();
    for (const TabRecord& record : m_tabRecords) {
        if (record.tabIndex == index)
            return &record;
    }
    return nullptr;
}

QRect TabStrip::indicatorRectForTab(int index) const
{
    for (const TabRecord& record : m_tabRecords) {
        if (record.tabIndex != index)
            continue;
        const Metrics currentMetrics = metrics();
        return QRect(record.tabRect.left() + currentMetrics.cornerRadius,
                     record.tabRect.bottom() - currentMetrics.selectionIndicatorHeight + 1,
                     qMax(0, record.tabRect.width() - currentMetrics.cornerRadius * 2),
                     currentMetrics.selectionIndicatorHeight);
    }
    return QRect();
}

void TabStrip::setAnimatedIndicatorRect(const QRect& rect)
{
    if (m_animatedIndicatorRect == rect)
        return;
    m_animatedIndicatorRect = rect;
    update();
}

void TabStrip::animateIndicator(const QRect& from, const QRect& to)
{
    m_indicatorAnimation->stop();
    if (!isVisible() || from.isEmpty() || to.isEmpty()) {
        setAnimatedIndicatorRect(to);
        return;
    }
    m_indicatorAnimation->setStartValue(from);
    m_indicatorAnimation->setEndValue(to);
    m_indicatorAnimation->start();
}

void TabStrip::setOverflowScrollValue(int value)
{
    const int maximum = qMax(0, m_items.size() - 1);
    const int clamped = qBound(0, value, maximum);
    m_firstVisibleTab = clamped;
    if (!m_overflowScrollBar || m_overflowScrollBar->value() == clamped)
        return;
    m_overflowScrollBar->setValue(clamped);
}

void TabStrip::syncOverflowScrollBar(bool overflow)
{
    if (!m_overflowScrollBar)
        return;
    const int maximum = overflow ? qMax(0, m_items.size() - 1) : 0;
    m_updatingOverflowScrollBar = true;
    m_overflowScrollBar->setRange(0, maximum);
    m_overflowScrollBar->setSingleStep(1);
    m_overflowScrollBar->setPageStep(qMax(1, m_visibleTabIndexes.size()));
    m_overflowScrollBar->setVisible(false);
    m_overflowScrollBar->setValue(qBound(0, m_firstVisibleTab, maximum));
    m_firstVisibleTab = m_overflowScrollBar->value();
    m_updatingOverflowScrollBar = false;
}

void TabStrip::updateHeaderWidgets()
{
    for (int i = 0; i < m_headerWidgets.size();) {
        if (m_visibleTabIndexes.contains(m_headerWidgets.at(i).tabIndex)) {
            ++i;
            continue;
        }
        if (m_headerWidgets.at(i).label) {
            m_headerWidgets.at(i).label->hide();
            m_headerWidgets.at(i).label->deleteLater();
        }
        if (m_headerWidgets.at(i).closeButton) {
            m_headerWidgets.at(i).closeButton->hide();
            m_headerWidgets.at(i).closeButton->deleteLater();
        }
        m_headerWidgets.removeAt(i);
    }

    const Metrics currentMetrics = metrics();
    for (const TabRecord& record : m_tabRecords) {
        const TabRecord visualRecord = visualRecordForRecord(record);
        TabHeaderWidgets* widgets = widgetsForTabHeader(record.tabIndex);
        if (!widgets) {
            TabHeaderWidgets created;
            created.tabIndex = record.tabIndex;
            created.label = new textfields::Label(this);
            created.label->setAttribute(Qt::WA_TransparentForMouseEvents);
            created.label->setFocusPolicy(Qt::NoFocus);
            created.label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            created.label->setContentsMargins(0, 0, 0, 0);
            created.label->setTextElideMode(Qt::ElideRight);
            created.labelOpacity = new QGraphicsOpacityEffect(created.label);
            created.label->setGraphicsEffect(created.labelOpacity);

            created.closeButton = createIconButton(Typography::Icons::Cancel, currentMetrics.closeIconPixelSize);
            created.closeButton->installEventFilter(this);
            created.closeOpacity = new QGraphicsOpacityEffect(created.closeButton);
            created.closeButton->setGraphicsEffect(created.closeOpacity);
            connect(created.closeButton, &basicinput::Button::clicked, this, [this, button = created.closeButton]() {
                emitCloseRequested(button->property("tabIndex").toInt());
            });

            m_headerWidgets.append(created);
            widgets = &m_headerWidgets.last();
        }

        widgets->label->setText(m_items.value(record.tabIndex).text);
        widgets->label->setFluentTypography(m_tabFontRole);
        widgets->label->setGeometry(visualRecord.textRect);
        widgets->label->setEnabled(isSelectableIndex(record.tabIndex));
        widgets->label->setVisible(!visualRecord.textRect.isEmpty());
        // Color the tab label through the Label's own style sheet (via a text-color role) instead of its
        // palette: a palette WindowText color is dropped under any ancestor style sheet — the gallery
        // sample card installs QStyleSheetStyle over its subtree and ignores child palettes, which made
        // the tab labels render near-black in dark theme. setTextColorRole no-ops when unchanged, so this
        // stays cheap on relayout; the role mirrors textColorForTab. zh_CN: 用 Label 自身样式表（经文本颜色
        // 角色）上色而非 palette：祖先样式表会安装 QStyleSheetStyle 并忽略子 palette，导致标签在深色主题里变近黑；
        // setTextColorRole 在角色未变时直接返回，故重排时开销很小；角色与 textColorForTab 一致。
        const bool tabUsable = isEnabled() && isValidIndex(record.tabIndex)
                               && m_items.at(record.tabIndex).enabled;
        // Brand-specific label color role, DesignFluent unchanged (Primary selected / Secondary rest).
        // M3 selected label is Accent text; macOS selected label is OnAccent (white) over the accent
        // segment. Kept in lockstep with textColorForTab so the painted icon matches the label.
        // zh_CN: 品牌专属标签颜色角色，DesignFluent 不变（选中 Primary / 其余 Secondary）。M3 选中标签为
        // Accent 文字；macOS 选中标签为强调段上的 OnAccent（白）。与 textColorForTab 同步，使图标与标签一致。
        const bool tabSelected = record.tabIndex == m_selectedIndex;
        const DesignLanguage labelLang = themeDesignLanguage();
        textfields::Label::TextColorRole role;
        if (!tabUsable) {
            role = textfields::Label::TextColorRole::Disabled;
        } else if (labelLang == DesignMaterial) {
            role = tabSelected ? textfields::Label::TextColorRole::Accent
                               : textfields::Label::TextColorRole::Secondary;
        } else if (labelLang == DesignCupertino) {
            role = tabSelected ? textfields::Label::TextColorRole::OnAccent
                               : textfields::Label::TextColorRole::Secondary;
        } else {
            role = tabSelected ? textfields::Label::TextColorRole::Primary
                               : textfields::Label::TextColorRole::Secondary;
        }
        widgets->label->setTextColorRole(role);
        widgets->label->raise();
        setHeaderWidgetOpacity(*widgets, tabRevealOpacity(record.tabIndex));

        widgets->closeButton->setProperty("tabIndex", record.tabIndex);
        updateIconButton(widgets->closeButton,
                         record.closeVisible ? visualRecord.closeRect : QRect(),
                         Typography::Icons::Cancel,
                         currentMetrics.closeIconPixelSize,
                         isCloseableIndex(record.tabIndex));
    }

    const bool hasBack = !m_overflowBackRect.isEmpty() && m_overflowScrollBar && m_overflowScrollBar->value() > m_overflowScrollBar->minimum();
    const bool hasForward = !m_overflowForwardRect.isEmpty() && m_overflowScrollBar && m_overflowScrollBar->value() < m_overflowScrollBar->maximum();
    updateIconButton(m_overflowBackButton, m_overflowBackRect, Typography::Icons::ChevronLeftMed, currentMetrics.iconPixelSize, isEnabled() && hasBack);
    updateIconButton(m_overflowForwardButton, m_overflowForwardRect, Typography::Icons::ChevronRightMed, currentMetrics.iconPixelSize, isEnabled() && hasForward);
    updateIconButton(m_addButton, m_addButtonRect, Typography::Icons::Add, currentMetrics.iconPixelSize, isEnabled() && m_addButtonVisible);
}

TabStrip::TabHeaderWidgets* TabStrip::widgetsForTabHeader(int index)
{
    for (TabHeaderWidgets& widgets : m_headerWidgets) {
        if (widgets.tabIndex == index)
            return &widgets;
    }
    return nullptr;
}

qreal TabStrip::tabRevealOpacity(int index) const
{
    return qBound<qreal>(0.0, m_tabRevealProgress.value(index, 1.0), 1.0);
}

void TabStrip::setTabRevealOpacity(int index, qreal opacity)
{
    if (!isValidIndex(index))
        return;
    m_tabRevealProgress[index] = qBound<qreal>(0.0, opacity, 1.0);
    if (TabHeaderWidgets* widgets = widgetsForTabHeader(index))
        setHeaderWidgetOpacity(*widgets, m_tabRevealProgress.value(index));
    update();
}

void TabStrip::stopRevealAnimation(int index)
{
    if (QVariantAnimation* animation = m_tabRevealAnimations.take(index)) {
        animation->stop();
        animation->deleteLater();
    }
}

void TabStrip::setHeaderWidgetOpacity(TabHeaderWidgets& widgets, qreal opacity)
{
    const qreal bounded = qBound<qreal>(0.0, opacity, 1.0);
    if (widgets.labelOpacity)
        widgets.labelOpacity->setOpacity(bounded);
    if (widgets.closeOpacity)
        widgets.closeOpacity->setOpacity(bounded);
}

basicinput::Button* TabStrip::createIconButton(const QString& glyph, int pixelSize)
{
    auto* button = new basicinput::Button(this);
    button->setFluentStyle(basicinput::Button::Subtle);
    button->setFluentSize(basicinput::Button::Small);
    button->setFluentLayout(basicinput::Button::IconOnly);
    button->setFocusPolicy(Qt::NoFocus);
    button->setIconGlyph(glyph, pixelSize, m_iconFontFamily);
    button->setVisible(false);
    return button;
}

void TabStrip::updateIconButton(basicinput::Button* button, const QRect& rect, const QString& glyph, int pixelSize, bool enabled)
{
    if (!button)
        return;
    button->setIconGlyph(glyph, pixelSize, m_iconFontFamily);
    button->setGeometry(rect);
    button->setEnabled(enabled);
    button->setVisible(!rect.isEmpty());
    button->raise();
}

TabStrip::HitRecord TabStrip::hitTest(const QPoint& position) const
{
    ensureLayout();
    for (const TabRecord& record : m_tabRecords) {
        if (record.closeVisible && record.closeRect.contains(position))
            return HitRecord{HitKind::Close, record.tabIndex};
    }
    if (m_addButtonRect.contains(position))
        return HitRecord{HitKind::Add, -1};
    if (m_overflowBackRect.contains(position))
        return HitRecord{HitKind::OverflowBack, -1};
    if (m_overflowForwardRect.contains(position))
        return HitRecord{HitKind::OverflowForward, -1};
    for (const TabRecord& record : m_tabRecords) {
        if (record.tabRect.contains(position))
            return HitRecord{HitKind::Tab, record.tabIndex};
    }
    return HitRecord();
}

void TabStrip::setHoveredHit(const HitRecord& hit)
{
    const HitRecord normalized = isInteractiveHit(hit) ? hit : HitRecord();
    if (sameHit(m_hoveredHit, normalized))
        return;
    const int previousIndex = m_hoveredHit.tabIndex;
    const qreal previousProgress = compactExpansionProgress(previousIndex);
    const qreal nextProgress = compactExpansionProgress(normalized.tabIndex);
    m_hoveredHit = normalized;
    if (m_widthMode == WidthMode::Compact) {
        animateCompactExpansion(previousIndex, previousProgress, compactExpansionTarget(previousIndex) ? 1.0 : 0.0);
        animateCompactExpansion(m_hoveredHit.tabIndex, nextProgress, compactExpansionTarget(m_hoveredHit.tabIndex) ? 1.0 : 0.0);
    }
    invalidateLayout();
}

void TabStrip::clearPressedHit()
{
    if (m_pressedHit.kind == HitKind::None)
        return;
    m_pressedHit = HitRecord();
    update();
}

bool TabStrip::sameHit(const HitRecord& lhs, const HitRecord& rhs) const
{
    return lhs.kind == rhs.kind && lhs.tabIndex == rhs.tabIndex;
}

bool TabStrip::isInteractiveHit(const HitRecord& hit) const
{
    if (!isEnabled())
        return false;
    switch (hit.kind) {
    case HitKind::Tab:
        return isSelectableIndex(hit.tabIndex);
    case HitKind::Close:
        return isCloseableIndex(hit.tabIndex) && closeButtonGeometry(hit.tabIndex).isValid();
    case HitKind::Add:
        return m_addButtonVisible && !m_addButtonRect.isEmpty();
    case HitKind::OverflowBack:
        return !m_overflowBackRect.isEmpty() && m_overflowScrollBar && m_overflowScrollBar->value() > m_overflowScrollBar->minimum();
    case HitKind::OverflowForward:
        return !m_overflowForwardRect.isEmpty() && m_overflowScrollBar && m_overflowScrollBar->value() < m_overflowScrollBar->maximum();
    case HitKind::None:
        break;
    }
    return false;
}

void TabStrip::activateHit(const HitRecord& hit)
{
    if (!isInteractiveHit(hit))
        return;
    switch (hit.kind) {
    case HitKind::Tab:
        selectFromUser(hit.tabIndex);
        break;
    case HitKind::Close:
        emitCloseRequested(hit.tabIndex);
        break;
    case HitKind::Add:
        emit addTabRequested();
        break;
    case HitKind::OverflowBack:
        scrollOverflow(-1);
        break;
    case HitKind::OverflowForward:
        scrollOverflow(1);
        break;
    case HitKind::None:
        break;
    }
}

void TabStrip::selectFromUser(int index)
{
    m_focusedHit = HitRecord{HitKind::Tab, index};
    emit selectedIndexRequested(index);
}

void TabStrip::emitCloseRequested(int index)
{
    if (!isCloseableIndex(index))
        return;
    emit tabCloseRequested(index);
}

void TabStrip::scrollOverflow(int direction)
{
    if (!m_overflowScrollBar || direction == 0)
        return;
    m_overflowScrollBar->setValue(m_overflowScrollBar->value() + (direction < 0 ? -m_overflowScrollBar->singleStep() : m_overflowScrollBar->singleStep()));
}

void TabStrip::paintRow(QPainter& painter)
{
    const Metrics currentMetrics = metrics();
    const QRect row(contentsRect().left(), contentsRect().top(), contentsRect().width(), currentMetrics.rowHeight);
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(themeColorsRef().bgCanvas);
    painter.drawRect(row);
    painter.restore();
}

void TabStrip::paintTab(QPainter& painter, const TabRecord& record)
{
    if (record.tabRect.isEmpty() || !isValidIndex(record.tabIndex))
        return;

    const TabRecord visualRecord = visualRecordForRecord(record);
    const Metrics currentMetrics = metrics();
    const TabViewItem& item = m_items.at(record.tabIndex);
    const bool selected = record.tabIndex == m_selectedIndex;
    const qreal revealOpacity = tabRevealOpacity(record.tabIndex);
    const QColor fill = tabFillColor(record);

    // Branch the tab chrome per brand, preserving DesignFluent EXACTLY (the document-style raised
    // tab with curved join feet). Document tabs aren't a native M3/macOS pattern, so M3 and macOS get
    // a tasteful, conservative treatment consistent with our Pivot tabs rather than a redesign:
    // M3 keeps the surface neutral and adds an accent label + rounded accent underbar; macOS retints
    // the selected tab toward the accent like a segmented control. Geometry is untouched — we only
    // reuse the existing tab/indicator rects. zh_CN: 按品牌分支 tab 外观，DesignFluent 完全保持原样
    //（带弧形接脚的文档式凸起 tab）。文档式 tab 非 M3/macOS 原生范式，故 M3 与 macOS 采用与 Pivot 一致的
    // 克制处理而非重设计：M3 保持面中性并加强调标签 + 圆角强调下划条；macOS 把选中 tab 朝强调色微调，如分段
    // 控件。几何不变——仅复用既有 tab/指示条矩形。
    const DesignLanguage lang = themeDesignLanguage();
    painter.save();
    painter.setOpacity(revealOpacity);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    if (lang == DesignFluent) {
        if (selected) {
            const QRect row(contentsRect().left(), contentsRect().top(), contentsRect().width(), currentMetrics.rowHeight);
            const int radius = qMax(2, currentMetrics.cornerRadius + 2);
            QRect tabVisualRect = visualRecord.tabRect;
            tabVisualRect.setBottom(row.bottom());

            painter.drawPath(topRoundedTabPath(tabVisualRect, radius));

            QPainterPath leftJoin;
            leftJoin.moveTo(tabVisualRect.left() - radius, tabVisualRect.bottom() + 1);
            leftJoin.lineTo(tabVisualRect.left(), tabVisualRect.bottom() + 1);
            leftJoin.lineTo(tabVisualRect.left(), tabVisualRect.bottom() - radius + 1);
            leftJoin.quadTo(tabVisualRect.left(), tabVisualRect.bottom() + 1, tabVisualRect.left() - radius, tabVisualRect.bottom() + 1);
            leftJoin.closeSubpath();
            painter.drawPath(leftJoin);

            QPainterPath rightJoin;
            rightJoin.moveTo(tabVisualRect.right(), tabVisualRect.bottom() - radius + 1);
            rightJoin.quadTo(tabVisualRect.right(), tabVisualRect.bottom() + 1, tabVisualRect.right() + radius, tabVisualRect.bottom() + 1);
            rightJoin.lineTo(tabVisualRect.right(), tabVisualRect.bottom() + 1);
            rightJoin.closeSubpath();
            painter.drawPath(rightJoin);
        } else if (fill.alpha() > 0) {
            const QRect row(contentsRect().left(), contentsRect().top(), contentsRect().width(), currentMetrics.rowHeight);
            QRect tabVisualRect = visualRecord.tabRect;
            tabVisualRect.setBottom(row.bottom());
            painter.drawPath(topRoundedTabPath(tabVisualRect, currentMetrics.cornerRadius));
        }
    } else {
        // M3 + macOS share a within-bounds rounded-rect fill (no curved join feet, no clipped halos)
        // so selected/hover affordances stay tidy inside each tab; tabFillColor() already supplies the
        // brand-correct color. zh_CN: M3 与 macOS 共用限制在范围内的圆角填充（无接脚、无溢出光晕），选中/
        // 悬停效果整洁地留在各 tab 内；颜色由 tabFillColor() 提供。
        if (fill.alpha() > 0) {
            const int radius = qMax(2, currentMetrics.cornerRadius + (selected ? 2 : 0));
            painter.drawRoundedRect(visualRecord.tabRect, radius, radius);
        }

        // M3 §5 tabs: a rounded primary indicator (~3 dp) along the selected tab's lower edge, hugging
        // the label width with fully rounded ends — mirrors Pivot's M3 indicator. macOS uses the
        // accent segment fill above instead of an underbar. zh_CN: M3 §5 tabs：选中 tab 下缘的圆角 primary
        // 指示条（约 3dp），贴合标签宽度且两端全圆角——与 Pivot 的 M3 指示条一致。macOS 用上面的强调段填充代替下划条。
        if (lang == DesignMaterial && selected) {
            const QRect base = indicatorRectForTab(record.tabIndex);
            if (base.isValid() && !base.isEmpty()) {
                const int textWidth = QFontMetrics(tabFont()).horizontalAdvance(item.text);
                const int labelWidth = qMin(base.width(), qMax(currentMetrics.minTabWidth / 2, textWidth));
                const int cx = visualRecord.tabRect.center().x();
                const QRect indicator(cx - labelWidth / 2, base.top(), labelWidth, base.height());
                painter.setBrush(themeColorsRef().accentDefault);
                const qreal r = indicator.height() / 2.0;
                painter.drawRoundedRect(indicator, r, r);
            }
        }
    }

    const QColor textColor = textColorForTab(record.tabIndex);
    if (!visualRecord.iconRect.isEmpty()) {
        painter.setFont(iconFont(currentMetrics.iconPixelSize));
        painter.setPen(textColor);
        const QString glyph = item.iconGlyph.isEmpty() ? Typography::Icons::Document : item.iconGlyph;
        painter.drawText(visualRecord.iconRect, Qt::AlignCenter, glyph);
    }

    if (!selected && !m_dragActive && m_focusVisualVisible && !sameHit(m_pressedHit, HitRecord{HitKind::Tab, record.tabIndex}) && hasFocus() && sameHit(m_focusedHit, HitRecord{HitKind::Tab, record.tabIndex}))
        paintFocus(painter, visualRecord.tabRect.adjusted(1, 1, -1, -1));
    painter.restore();
}

void TabStrip::paintDragInsertionIndicator(QPainter& painter)
{
    const QRect indicator = dragInsertionIndicatorRect();
    if (indicator.isEmpty())
        return;

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(themeColorsRef().accentDefault);
    painter.drawRoundedRect(indicator, kDragIndicatorWidth / 2.0, kDragIndicatorWidth / 2.0);
    painter.restore();
}

void TabStrip::paintSelectedIndicator(QPainter& painter)
{
    if (m_animatedIndicatorRect.isEmpty())
        return;
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(themeColorsRef().accentDefault);
    painter.drawRoundedRect(m_animatedIndicatorRect,
                            ::CornerRadius::Indicator, ::CornerRadius::Indicator);
    painter.restore();
}

void TabStrip::paintButton(QPainter& painter, const QRect& rect, const QString& glyph, const HitRecord& hit, bool enabled)
{
    if (rect.isEmpty())
        return;
    const Metrics currentMetrics = metrics();
    painter.save();
    painter.setPen(Qt::NoPen);
    const QColor fill = enabled ? fillForHit(hit) : Qt::transparent;
    if (fill.alpha() > 0) {
        painter.setBrush(fill);
        painter.drawRoundedRect(rect, currentMetrics.cornerRadius, currentMetrics.cornerRadius);
    }
    painter.setFont(iconFont(hit.kind == HitKind::Close ? currentMetrics.closeIconPixelSize : currentMetrics.iconPixelSize));
    painter.setPen(enabled ? themeColorsRef().textSecondary : themeColorsRef().textDisabled);
    painter.drawText(rect, Qt::AlignCenter, glyph);
    if (!m_dragActive && m_focusVisualVisible && hasFocus() && sameHit(m_focusedHit, hit))
        paintFocus(painter, rect.adjusted(1, 1, -1, -1));
    painter.restore();
}

void TabStrip::paintFocus(QPainter& painter, const QRect& rect)
{
    if (rect.isEmpty())
        return;
    const Metrics currentMetrics = metrics();
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(themeColorsRef().strokeFocusOuter, 1));
    painter.drawRoundedRect(rect, currentMetrics.cornerRadius, currentMetrics.cornerRadius);
    painter.setPen(QPen(themeColorsRef().strokeFocusInner, 1));
    painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), qMax(0, currentMetrics.cornerRadius - 1), qMax(0, currentMetrics.cornerRadius - 1));
    painter.restore();
}

QColor TabStrip::tabFillColor(const TabRecord& record) const
{
    const bool selected = record.tabIndex == m_selectedIndex;
    const HitRecord tabHit{HitKind::Tab, record.tabIndex};
    const HitRecord closeHit{HitKind::Close, record.tabIndex};
    const bool pressed = sameHit(m_pressedHit, tabHit) || sameHit(m_pressedHit, closeHit);
    const bool hovered = sameHit(m_hoveredHit, tabHit) || sameHit(m_hoveredHit, closeHit);

    // Brand-specific tab fill, DesignFluent unchanged. zh_CN: 品牌专属 tab 填充，DesignFluent 保持不变。
    const DesignLanguage lang = themeDesignLanguage();
    if (lang == DesignMaterial) {
        // M3 keeps document tabs on the neutral surface (no elevated selected fill); selection is
        // carried by the accent label + underbar. Hover/press get a subtle within-bounds primary
        // state-layer veil. zh_CN: M3 文档 tab 保持中性面（选中不抬升填充），选中由强调标签 + 下划条体现；
        // 悬停/按下用限制在范围内的轻量 primary state-layer 薄层。
        if (pressed || hovered) {
            QColor stateLayer = themeColorsRef().accentDefault;
            stateLayer.setAlpha(pressed ? 0x1A : 0x14);
            return stateLayer;
        }
        return Qt::transparent;
    }
    if (lang == DesignCupertino) {
        // macOS reads the selected tab as the active segment of a segmented control: an accent fill.
        // Inactive hover gets a faint theme-aware veil; otherwise neutral. zh_CN: macOS 把选中 tab 视为
        // 分段控件的激活段：强调色填充。未选中悬停为淡淡的主题感知薄层，其余中性。
        if (selected)
            return themeColorsRef().accentDefault;
        if (pressed || hovered) {
            const bool darkTheme = effectiveTheme() == Dark;
            return darkTheme ? QColor(255, 255, 255, pressed ? 0x2C : 0x1C)
                             : QColor(0, 0, 0, pressed ? 0x24 : 0x14);
        }
        return Qt::transparent;
    }

    // DesignFluent (default): unchanged. zh_CN: 默认 Fluent，保持不变。
    if (selected)
        return themeColorsRef().bgLayer;
    if (pressed)
        return themeColorsRef().subtleTertiary;
    if (hovered)
        return themeColorsRef().subtleSecondary;
    return Qt::transparent;
}

QColor TabStrip::textColorForTab(int index) const
{
    if (!isEnabled() || !isValidIndex(index) || !m_items.at(index).enabled)
        return themeColorsRef().textDisabled;

    const bool selected = index == m_selectedIndex;
    // Brand-specific tab text/icon color, DesignFluent unchanged. Mirrors the Label role chosen in
    // updateHeaderWidgets so the painted icon glyph matches the label. zh_CN: 品牌专属 tab 文字/图标色，
    // DesignFluent 不变。与 updateHeaderWidgets 选择的 Label 角色一致，使绘制的图标字符与标签匹配。
    const DesignLanguage lang = themeDesignLanguage();
    if (lang == DesignMaterial)
        return selected ? themeColorsRef().accentDefault : themeColorsRef().textSecondary;
    if (lang == DesignCupertino)
        return selected ? themeColorsRef().textOnAccent : themeColorsRef().textSecondary;

    // DesignFluent (default): unchanged. zh_CN: 默认 Fluent，保持不变。
    return selected ? themeColorsRef().textPrimary : themeColorsRef().textSecondary;
}

QColor TabStrip::fillForHit(const HitRecord& hit) const
{
    if (sameHit(m_pressedHit, hit))
        return themeColorsRef().subtleTertiary;
    if (sameHit(m_hoveredHit, hit))
        return themeColorsRef().subtleSecondary;
    return Qt::transparent;
}


TabView::TabView(QWidget* parent)
    : QWidget(parent)
    , m_tabStrip(new TabStrip(this))
{
    setFocusPolicy(Qt::StrongFocus);
    if (qApp)
        qApp->installEventFilter(this);
    if (!s_activeShortcutOwner)
        s_activeShortcutOwner = this;

    connect(m_tabStrip, &TabStrip::selectedIndexRequested, this, &TabView::setSelectedIndex);
    connect(m_tabStrip, &TabStrip::tabCloseRequested, this, &TabView::tabCloseRequested);
    connect(m_tabStrip, &TabStrip::addTabRequested, this, &TabView::addTabRequested);
    connect(m_tabStrip, &TabStrip::tabMoveRequested, this, &TabView::moveTab);

    syncTabStrip();
    updateChildGeometry();
    updateAccessibleText();
}

TabView::~TabView()
{
    if (qApp)
        qApp->removeEventFilter(this);
    if (s_activeShortcutOwner == this)
        s_activeShortcutOwner = nullptr;
}

TabViewItem TabView::tabAt(int index) const
{
    return isValidIndex(index) ? m_items.at(index) : TabViewItem();
}

int TabView::addTab(const QString& text)
{
    return addTab(TabViewItem(text));
}

int TabView::addTab(const TabViewItem& item)
{
    const int index = m_items.size();
    insertTab(index, item);
    return index;
}

bool TabView::insertTab(int index, const QString& text)
{
    return insertTab(index, TabViewItem(text));
}

bool TabView::insertTab(int index, const TabViewItem& item)
{
    if (index < 0 || index > m_items.size())
        return false;

    const int previousIndex = m_selectedIndex;
    TabViewItem inserted = item;
    m_items.insert(index, inserted);

    if (m_selectedIndex >= index)
        ++m_selectedIndex;
    if (m_selectedIndex < 0 && inserted.enabled)
        m_selectedIndex = index;
    clampSelectedIndex();

    syncTabStrip();
    m_tabStrip->revealTab(index);
    updateAccessibleText();
    if (m_selectedIndex != previousIndex) {
        emit selectedIndexChanged(m_selectedIndex);
        emit currentChanged(m_selectedIndex);
    }
    emit tabsChanged();
    return true;
}

bool TabView::removeTab(int index)
{
    if (!isValidIndex(index))
        return false;

    const int previousIndex = m_selectedIndex;
    m_items.removeAt(index);

    bool selectionChanged = false;
    if (m_selectedIndex == index) {
        m_selectedIndex = -1;
        if (!m_items.isEmpty()) {
            const int candidate = qMin(index, m_items.size() - 1);
            m_selectedIndex = isSelectableIndex(candidate) ? candidate : firstEnabledIndex();
        }
        selectionChanged = true;
    } else if (m_selectedIndex > index) {
        --m_selectedIndex;
    }

    syncTabStrip();
    updateAccessibleText();
    if (selectionChanged) {
        emit selectedIndexChanged(m_selectedIndex);
        emit currentChanged(m_selectedIndex);
    }
    emit tabsChanged();
    return true;
}

bool TabView::closeTab(int index)
{
    if (!isCloseableIndex(index))
        return false;
    return removeTab(index);
}

void TabView::clearTabs()
{
    if (m_items.isEmpty())
        return;

    const bool selectionChanged = m_selectedIndex != -1;
    m_items.clear();
    m_selectedIndex = -1;

    syncTabStrip();
    updateAccessibleText();
    if (selectionChanged) {
        emit selectedIndexChanged(-1);
        emit currentChanged(-1);
    }
    emit tabsChanged();
}

bool TabView::moveTab(int from, int to)
{
    if (!isValidIndex(from) || !isValidIndex(to) || from == to)
        return false;

    const int oldSelected = m_selectedIndex;
    TabViewItem item = m_items.takeAt(from);
    m_items.insert(to, item);

    if (oldSelected == from) {
        m_selectedIndex = to;
    } else if (from < oldSelected && oldSelected <= to) {
        m_selectedIndex = oldSelected - 1;
    } else if (to <= oldSelected && oldSelected < from) {
        m_selectedIndex = oldSelected + 1;
    }

    syncTabStrip();
    updateAccessibleText();
    emit tabMoved(from, to);
    emit tabsChanged();
    if (m_selectedIndex != oldSelected) {
        emit selectedIndexChanged(m_selectedIndex);
        emit currentChanged(m_selectedIndex);
    }
    return true;
}

bool TabView::setTabText(int index, const QString& text)
{
    if (!isValidIndex(index) || m_items.at(index).text == text)
        return false;
    m_items[index].text = text;
    syncTabStrip();
    updateAccessibleText();
    emit tabsChanged();
    return true;
}

bool TabView::setTabIconGlyph(int index, const QString& glyph)
{
    if (!isValidIndex(index) || m_items.at(index).iconGlyph == glyph)
        return false;
    m_items[index].iconGlyph = glyph;
    syncTabStrip();
    emit tabsChanged();
    return true;
}

bool TabView::setTabClosable(int index, bool closable)
{
    if (!isValidIndex(index) || m_items.at(index).closable == closable)
        return false;
    m_items[index].closable = closable;
    syncTabStrip();
    updateAccessibleText();
    emit tabsChanged();
    return true;
}

bool TabView::setTabEnabled(int index, bool enabled)
{
    if (!isValidIndex(index) || m_items.at(index).enabled == enabled)
        return false;

    const int previousIndex = m_selectedIndex;
    m_items[index].enabled = enabled;
    if (!enabled && m_selectedIndex == index) {
        m_selectedIndex = -1;
        const int replacement = index + 1 < m_items.size() ? index + 1 : index - 1;
        m_selectedIndex = isSelectableIndex(replacement) ? replacement : firstEnabledIndex();
    } else if (m_selectedIndex < 0 && enabled) {
        m_selectedIndex = index;
    }

    syncTabStrip();
    updateAccessibleText();
    if (m_selectedIndex != previousIndex) {
        emit selectedIndexChanged(m_selectedIndex);
        emit currentChanged(m_selectedIndex);
    }
    emit tabsChanged();
    return true;
}

bool TabView::setTabData(int index, const QVariant& data)
{
    if (!isValidIndex(index) || m_items.at(index).data == data)
        return false;
    m_items[index].data = data;
    emit tabsChanged();
    return true;
}

bool TabView::setTabAccessibleName(int index, const QString& accessibleName)
{
    if (!isValidIndex(index) || m_items.at(index).accessibleName == accessibleName)
        return false;
    m_items[index].accessibleName = accessibleName;
    syncTabStrip();
    updateAccessibleText();
    emit tabsChanged();
    return true;
}

void TabView::setSelectedIndex(int index)
{
    const int normalized = isSelectableIndex(index) ? index : -1;
    if (m_selectedIndex == normalized)
        return;

    const int previousIndex = m_selectedIndex;
    m_selectedIndex = normalized;
    syncTabStrip();
    Q_UNUSED(previousIndex)
    updateAccessibleText();
    emit selectedIndexChanged(m_selectedIndex);
    emit currentChanged(m_selectedIndex);
}

void TabView::setTabWidthMode(TabWidthMode mode)
{
    if (m_tabWidthMode == mode)
        return;
    m_tabWidthMode = mode;
    m_tabStrip->setWidthMode(toStripWidthMode(m_tabWidthMode));
    emit tabWidthModeChanged(m_tabWidthMode);
}

void TabView::setCloseButtonOverlayMode(CloseButtonOverlayMode mode)
{
    if (m_closeButtonOverlayMode == mode)
        return;
    m_closeButtonOverlayMode = mode;
    m_tabStrip->setCloseButtonOverlayMode(toStripCloseMode(m_closeButtonOverlayMode));
    emit closeButtonOverlayModeChanged(m_closeButtonOverlayMode);
}

void TabView::setTabsClosable(bool closable)
{
    if (m_tabsClosable == closable)
        return;
    m_tabsClosable = closable;
    m_tabStrip->setTabsClosable(m_tabsClosable);
    updateAccessibleText();
    emit tabsClosableChanged(m_tabsClosable);
}

void TabView::setAddTabButtonVisible(bool visible)
{
    if (m_addTabButtonVisible == visible)
        return;
    m_addTabButtonVisible = visible;
    m_tabStrip->setAddButtonVisible(m_addTabButtonVisible);
    updateAccessibleText();
    emit addTabButtonVisibleChanged(m_addTabButtonVisible);
}

void TabView::setTabReorderEnabled(bool enabled)
{
    if (m_tabReorderEnabled == enabled)
        return;
    m_tabReorderEnabled = enabled;
    m_tabStrip->setTabReorderEnabled(m_tabReorderEnabled);
    emit tabReorderEnabledChanged(m_tabReorderEnabled);
}

void TabView::setKeyboardAcceleratorsEnabled(bool enabled)
{
    if (m_keyboardAcceleratorsEnabled == enabled)
        return;
    m_keyboardAcceleratorsEnabled = enabled;
    m_tabStrip->setKeyboardAcceleratorsEnabled(m_keyboardAcceleratorsEnabled);
    emit keyboardAcceleratorsEnabledChanged(m_keyboardAcceleratorsEnabled);
}

void TabView::setTabFontRole(const QString& role)
{
    const QString normalized = normalizedString(role, QStringLiteral("Body"));
    if (m_tabFontRole == normalized)
        return;
    m_tabFontRole = normalized;
    m_tabStrip->setTabFontRole(m_tabFontRole);
    emit tabFontRoleChanged(m_tabFontRole);
}

void TabView::setIconFontFamily(const QString& family)
{
    const QString normalized = normalizedString(family, Typography::FontFamily::FluentIcons);
    if (m_iconFontFamily == normalized)
        return;
    m_iconFontFamily = normalized;
    m_tabStrip->setIconFontFamily(m_iconFontFamily);
    emit iconFontFamilyChanged(m_iconFontFamily);
}

QRect TabView::tabGeometry(int index) const
{
    return translateFromStrip(m_tabStrip->tabGeometry(index));
}

QRect TabView::closeButtonGeometry(int index) const
{
    return translateFromStrip(m_tabStrip->closeButtonGeometry(index));
}

QRect TabView::addButtonGeometry() const
{
    return translateFromStrip(m_tabStrip->addButtonGeometry());
}

QRect TabView::overflowBackGeometry() const
{
    return translateFromStrip(m_tabStrip->overflowBackGeometry());
}

QRect TabView::overflowForwardGeometry() const
{
    return translateFromStrip(m_tabStrip->overflowForwardGeometry());
}

QVector<int> TabView::visibleTabIndexes() const
{
    return m_tabStrip->visibleTabIndexes();
}

QSize TabView::sizeHint() const
{
    return QSize(kDefaultWidth, m_tabStrip->rowHeight());
}

QSize TabView::minimumSizeHint() const
{
    const QSize stripMinimum = m_tabStrip->minimumSizeHint();
    return stripMinimum;
}

void TabView::onThemeUpdated()
{
    m_tabStrip->onThemeUpdated();
    update();
}

void TabView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateChildGeometry();
}

void TabView::mousePressEvent(QMouseEvent* event)
{
    setActiveShortcutOwner();
    if (forwardMouseEvent(m_tabStrip, event))
        return;
    QWidget::mousePressEvent(event);
}

void TabView::mouseMoveEvent(QMouseEvent* event)
{
    if (forwardMouseEvent(m_tabStrip, event))
        return;
    QWidget::mouseMoveEvent(event);
}

void TabView::mouseReleaseEvent(QMouseEvent* event)
{
    if (forwardMouseEvent(m_tabStrip, event))
        return;
    QWidget::mouseReleaseEvent(event);
}

void TabView::wheelEvent(QWheelEvent* event)
{
    if (!event) {
        QWidget::wheelEvent(event);
        return;
    }
    if (m_tabStrip && m_tabStrip->geometry().contains(fluentWheelPosition(event).toPoint())) {
        if (m_tabStrip->handleForwardedWheelEvent(event)) {
            event->accept();
            return;
        }
    }
    QWidget::wheelEvent(event);
}

void TabView::keyPressEvent(QKeyEvent* event)
{
    if (handleShortcutKey(event))
        return;
    if (m_tabStrip && m_tabStrip->handleForwardedKeyEvent(event))
        return;
    QWidget::keyPressEvent(event);
}

void TabView::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    setActiveShortcutOwner();
    if (m_tabStrip)
        m_tabStrip->setFocus(event->reason());
}

bool TabView::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn) {
        if (objectBelongsToWidget(watched, this))
            setActiveShortcutOwner();
    }

    if (event->type() == QEvent::KeyPress && isVisible()) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (objectBelongsToWidget(watched, this))
            setActiveShortcutOwner();
        if (s_activeShortcutOwner == this && window() && window()->isActiveWindow() && handleShortcutKey(keyEvent))
            return true;
    }

    return QWidget::eventFilter(watched, event);
}

bool TabView::isValidIndex(int index) const
{
    return index >= 0 && index < m_items.size();
}

bool TabView::isSelectableIndex(int index) const
{
    return isValidIndex(index) && m_items.at(index).enabled;
}

bool TabView::isCloseableIndex(int index) const
{
    return isValidIndex(index) && m_items.at(index).enabled && m_items.at(index).closable && m_tabsClosable;
}

QString TabView::normalizedString(const QString& value, const QString& fallback) const
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? fallback : trimmed;
}

void TabView::clampSelectedIndex()
{
    if (isSelectableIndex(m_selectedIndex))
        return;
    m_selectedIndex = firstEnabledIndex();
}

int TabView::firstEnabledIndex() const
{
    for (int index = 0; index < m_items.size(); ++index) {
        if (isSelectableIndex(index))
            return index;
    }
    return -1;
}

int TabView::lastEnabledIndex() const
{
    for (int index = m_items.size() - 1; index >= 0; --index) {
        if (isSelectableIndex(index))
            return index;
    }
    return -1;
}

bool TabView::handleShortcutKey(QKeyEvent* event)
{
    if (!event || !isEnabled() || !m_keyboardAcceleratorsEnabled || !usesTabShortcutModifier(event))
        return false;

    if (event->key() == Qt::Key_T) {
        emit addTabRequested();
        event->accept();
        return true;
    }
    if (event->key() == Qt::Key_W) {
        if (isCloseableIndex(m_selectedIndex))
            emit tabCloseRequested(m_selectedIndex);
        event->accept();
        return true;
    }
    if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_8) {
        const int shortcutIndex = event->key() - Qt::Key_1;
        if (isSelectableIndex(shortcutIndex))
            setSelectedIndex(shortcutIndex);
        event->accept();
        return true;
    }
    if (event->key() == Qt::Key_9) {
        const int lastIndex = lastEnabledIndex();
        if (lastIndex >= 0)
            setSelectedIndex(lastIndex);
        event->accept();
        return true;
    }
    return false;
}

void TabView::setActiveShortcutOwner()
{
    s_activeShortcutOwner = this;
}

void TabView::syncTabStrip()
{
    m_tabStrip->setWidthMode(toStripWidthMode(m_tabWidthMode));
    m_tabStrip->setCloseButtonOverlayMode(toStripCloseMode(m_closeButtonOverlayMode));
    m_tabStrip->setTabsClosable(m_tabsClosable);
    m_tabStrip->setAddButtonVisible(m_addTabButtonVisible);
    m_tabStrip->setTabReorderEnabled(m_tabReorderEnabled);
    m_tabStrip->setKeyboardAcceleratorsEnabled(m_keyboardAcceleratorsEnabled);
    m_tabStrip->setTabFontRole(m_tabFontRole);
    m_tabStrip->setIconFontFamily(m_iconFontFamily);
    m_tabStrip->setItems(m_items);
    m_tabStrip->setSelectedIndex(m_selectedIndex);
}

void TabView::updateChildGeometry()
{
    if (!m_tabStrip)
        return;

    const QRect bounds = contentsRect();
    const int stripHeight = m_tabStrip->rowHeight();
    m_tabStrip->setGeometry(bounds.left(), bounds.top(), bounds.width(), stripHeight);
    m_tabStrip->raise();
}

QRect TabView::translateFromStrip(const QRect& rect) const
{
    if (rect.isEmpty() || !m_tabStrip)
        return QRect();
    return rect.translated(m_tabStrip->pos());
}

void TabView::updateAccessibleText()
{
    const QString selectedName = isValidIndex(m_selectedIndex)
        ? (m_items.at(m_selectedIndex).accessibleName.isEmpty() ? m_items.at(m_selectedIndex).text : m_items.at(m_selectedIndex).accessibleName)
        : QStringLiteral("None");
    setAccessibleName(QStringLiteral("TabView"));
    setAccessibleDescription(QStringLiteral("Selected tab: %1. Tab count: %2").arg(selectedName).arg(m_items.size()));
}

} // namespace fluent::navigation
