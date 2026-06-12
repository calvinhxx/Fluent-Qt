#include "SelectorBar.h"

#include <algorithm>

#include <QAbstractAnimation>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QVariantAnimation>

#include "compatibility/QtCompat.h"
#include "design/Typography.h"
#include "design/CornerRadius.h"

namespace fluent::navigation {

namespace {
constexpr int kDefaultWidth = 520;

QRect collapsedIndicatorRect(const QRect& rect)
{
    if (rect.isEmpty())
        return QRect();
    constexpr int kCollapsedIndicatorWidth = 16;
    const int width = qMin(kCollapsedIndicatorWidth, rect.width());
    return QRect(rect.center().x() - width / 2, rect.top(), width, rect.height());
}
}

SelectorBarItem::SelectorBarItem(const QString& itemText)
    : text(itemText)
{
}

SelectorBarItem::SelectorBarItem(const QString& itemText,
                                 const QString& itemIconGlyph,
                                 bool itemEnabled,
                                 bool itemVisible,
                                 const QVariant& itemData,
                                 const QString& itemAccessibleName)
    : text(itemText)
    , iconGlyph(itemIconGlyph)
    , enabled(itemEnabled)
    , visible(itemVisible)
    , data(itemData)
    , accessibleName(itemAccessibleName)
{
}

SelectorBar::SelectorBar(QWidget* parent)
    : QWidget(parent)
    , m_iconFontFamily(Typography::FontFamily::SegoeFluentIcons)
    , m_indicatorAnimation(new QVariantAnimation(this))
{
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    m_indicatorAnimation->setDuration(themeAnimation().fast);
    m_indicatorAnimation->setEasingCurve(themeAnimation().decelerate);
    connect(m_indicatorAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        setAnimatedIndicatorRect(value.toRect());
    });
    updateAccessibleText();
}

SelectorBar::~SelectorBar() = default;

SelectorBarItem SelectorBar::itemAt(int index) const
{
    return isValidIndex(index) ? m_items.at(index) : SelectorBarItem();
}

SelectorBarItem SelectorBar::selectedItem() const
{
    return isValidIndex(m_selectedIndex) ? m_items.at(m_selectedIndex) : SelectorBarItem();
}

int SelectorBar::addItem(const QString& text)
{
    return addItem(SelectorBarItem(text));
}

int SelectorBar::addItem(const SelectorBarItem& item)
{
    const int index = m_items.size();
    insertItem(index, item);
    return index;
}

bool SelectorBar::insertItem(int index, const QString& text)
{
    return insertItem(index, SelectorBarItem(text));
}

bool SelectorBar::insertItem(int index, const SelectorBarItem& item)
{
    if (index < 0 || index > m_items.size())
        return false;

    const int previousIndex = m_selectedIndex;
    SelectorBarItem inserted = item;
    inserted.selected = false;
    m_items.insert(index, inserted);

    if (m_selectedIndex >= index)
        ++m_selectedIndex;
    if (m_focusedIndex >= index)
        ++m_focusedIndex;

    if (m_selectedIndex < 0 && isSelectableIndex(index))
        m_selectedIndex = index;
    if (!isSelectableIndex(m_selectedIndex))
        m_selectedIndex = nearestSelectableIndex(index);
    if (!isSelectableIndex(m_focusedIndex))
        m_focusedIndex = m_selectedIndex >= 0 ? m_selectedIndex : firstSelectableIndex();
    syncSelectedFlags();

    invalidateLayout();
    updateAccessibleText();
    if (m_selectedIndex != previousIndex) {
        emit selectedIndexChanged(m_selectedIndex);
        emit currentChanged(m_selectedIndex);
        emit selectionChanged(m_selectedIndex, selectedItem());
    }
    emit itemCountChanged(m_items.size());
    emit itemsChanged();
    return true;
}

bool SelectorBar::removeItem(int index)
{
    if (!isValidIndex(index))
        return false;

    const int previousIndex = m_selectedIndex;
    m_items.removeAt(index);

    if (m_selectedIndex == index) {
        m_selectedIndex = -1;
        m_selectedIndex = nearestSelectableIndex(qMin(index, m_items.size() - 1));
    } else if (m_selectedIndex > index) {
        --m_selectedIndex;
    }

    if (m_focusedIndex == index)
        m_focusedIndex = m_selectedIndex >= 0 ? m_selectedIndex : firstSelectableIndex();
    else if (m_focusedIndex > index)
        --m_focusedIndex;
    if (!isSelectableIndex(m_focusedIndex))
        m_focusedIndex = m_selectedIndex >= 0 ? m_selectedIndex : firstSelectableIndex();
    syncSelectedFlags();

    invalidateLayout();
    updateAccessibleText();
    if (m_selectedIndex != previousIndex) {
        emit selectedIndexChanged(m_selectedIndex);
        emit currentChanged(m_selectedIndex);
        emit selectionChanged(m_selectedIndex, selectedItem());
    }
    emit itemCountChanged(m_items.size());
    emit itemsChanged();
    return true;
}

void SelectorBar::clearItems()
{
    if (m_items.isEmpty())
        return;

    const bool hadSelection = m_selectedIndex != -1;
    m_items.clear();
    m_selectedIndex = -1;
    m_focusedIndex = -1;
    m_firstVisibleIndex = 0;
    m_hoveredHit = HitRecord();
    m_pressedHit = HitRecord();
    invalidateLayout();
    updateAccessibleText();
    if (hadSelection) {
        emit selectedIndexChanged(-1);
        emit currentChanged(-1);
        emit selectionChanged(-1, SelectorBarItem());
    }
    emit itemCountChanged(0);
    emit itemsChanged();
}

bool SelectorBar::setItemText(int index, const QString& text)
{
    if (!isValidIndex(index) || m_items.at(index).text == text)
        return false;
    m_items[index].text = text;
    invalidateLayout();
    updateAccessibleText();
    emit itemsChanged();
    return true;
}

bool SelectorBar::setItemIconGlyph(int index, const QString& glyph)
{
    if (!isValidIndex(index) || m_items.at(index).iconGlyph == glyph)
        return false;
    m_items[index].iconGlyph = glyph;
    invalidateLayout();
    emit itemsChanged();
    return true;
}

bool SelectorBar::setItemEnabled(int index, bool enabled)
{
    if (!isValidIndex(index) || m_items.at(index).enabled == enabled)
        return false;

    const int previousIndex = m_selectedIndex;
    m_items[index].enabled = enabled;
    if (!enabled && m_selectedIndex == index)
        clampSelectionAfterMutation(index + 1 < m_items.size() ? index + 1 : index - 1, false);
    else if (enabled && m_selectedIndex < 0 && m_items.at(index).visible)
        m_selectedIndex = index;

    if (!isSelectableIndex(m_focusedIndex))
        m_focusedIndex = m_selectedIndex >= 0 ? m_selectedIndex : firstSelectableIndex();
    syncSelectedFlags();

    invalidateLayout();
    updateAccessibleText();
    if (m_selectedIndex != previousIndex) {
        emit selectedIndexChanged(m_selectedIndex);
        emit currentChanged(m_selectedIndex);
        emit selectionChanged(m_selectedIndex, selectedItem());
    }
    emit itemsChanged();
    return true;
}

bool SelectorBar::setItemVisible(int index, bool visible)
{
    if (!isValidIndex(index) || m_items.at(index).visible == visible)
        return false;

    const int previousIndex = m_selectedIndex;
    m_items[index].visible = visible;
    if (!visible && m_selectedIndex == index)
        clampSelectionAfterMutation(index + 1 < m_items.size() ? index + 1 : index - 1, false);
    else if (visible && m_selectedIndex < 0 && m_items.at(index).enabled)
        m_selectedIndex = index;

    if (!isSelectableIndex(m_focusedIndex))
        m_focusedIndex = m_selectedIndex >= 0 ? m_selectedIndex : firstSelectableIndex();
    syncSelectedFlags();

    invalidateLayout();
    updateAccessibleText();
    if (m_selectedIndex != previousIndex) {
        emit selectedIndexChanged(m_selectedIndex);
        emit currentChanged(m_selectedIndex);
        emit selectionChanged(m_selectedIndex, selectedItem());
    }
    emit itemsChanged();
    return true;
}

bool SelectorBar::setItemData(int index, const QVariant& data)
{
    if (!isValidIndex(index) || m_items.at(index).data == data)
        return false;
    m_items[index].data = data;
    emit itemsChanged();
    return true;
}

bool SelectorBar::setItemAccessibleName(int index, const QString& accessibleName)
{
    if (!isValidIndex(index) || m_items.at(index).accessibleName == accessibleName)
        return false;
    m_items[index].accessibleName = accessibleName;
    updateAccessibleText();
    emit itemsChanged();
    return true;
}

bool SelectorBar::setItemSelected(int index, bool selected)
{
    if (!isValidIndex(index))
        return false;
    if (selected) {
        if (!isSelectableIndex(index))
            return false;
        const int before = m_selectedIndex;
        setSelectedIndexInternal(index, true);
        return before != m_selectedIndex;
    }

    if (m_selectedIndex != index)
        return false;
    setSelectedIndexInternal(-1, true);
    return true;
}

void SelectorBar::setSelectedIndex(int index)
{
    if (!isSelectableIndex(index))
        return;
    setSelectedIndexInternal(index, true);
}

void SelectorBar::clearSelection()
{
    setSelectedIndexInternal(-1, true);
}

void SelectorBar::setOverflowBehavior(OverflowBehavior behavior)
{
    if (m_overflowBehavior == behavior)
        return;
    m_overflowBehavior = behavior;
    invalidateLayout(false);
    emit overflowBehaviorChanged(m_overflowBehavior);
}

void SelectorBar::setItemFontRole(const QString& role)
{
    const QString normalized = normalizedString(role, QStringLiteral("Body"));
    if (m_itemFontRole == normalized)
        return;
    m_itemFontRole = normalized;
    invalidateLayout();
    emit itemFontRoleChanged(m_itemFontRole);
}

void SelectorBar::setIconFontFamily(const QString& family)
{
    const QString normalized = normalizedString(family, Typography::FontFamily::SegoeFluentIcons);
    if (m_iconFontFamily == normalized)
        return;
    m_iconFontFamily = normalized;
    invalidateLayout(false);
    emit iconFontFamilyChanged(m_iconFontFamily);
}

QRect SelectorBar::selectorRowGeometry() const
{
    ensureLayout();
    return m_rowRect;
}

QRect SelectorBar::itemGeometry(int index) const
{
    ensureLayout();
    if (const ItemRecord* record = recordForItem(index))
        return record->rect;
    return QRect();
}

QRect SelectorBar::selectedIndicatorGeometry(int index) const
{
    ensureLayout();
    return indicatorRectForItem(index);
}

QRect SelectorBar::overflowBackGeometry() const
{
    ensureLayout();
    return m_overflowBackRect;
}

QRect SelectorBar::overflowForwardGeometry() const
{
    ensureLayout();
    return m_overflowForwardRect;
}

QRect SelectorBar::overflowGeometry() const
{
    ensureLayout();
    return m_overflowMoreRect;
}

QVector<int> SelectorBar::visibleItemIndexes() const
{
    ensureLayout();
    return m_visibleIndexes;
}

QVector<int> SelectorBar::hiddenItemIndexes() const
{
    ensureLayout();
    return m_hiddenIndexes;
}

QSize SelectorBar::sizeHint() const
{
    return QSize(kDefaultWidth, metrics().rowHeight);
}

QSize SelectorBar::minimumSizeHint() const
{
    const Metrics currentMetrics = metrics();
    return QSize(currentMetrics.overflowButtonWidth * 3, currentMetrics.rowHeight);
}

void SelectorBar::onThemeUpdated()
{
    m_indicatorAnimation->setDuration(themeAnimation().fast);
    m_indicatorAnimation->setEasingCurve(themeAnimation().decelerate);
    invalidateLayout(false);
}

void SelectorBar::paintEvent(QPaintEvent*)
{
    ensureLayout();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    paintBackground(painter);
    for (const ItemRecord& record : std::as_const(m_itemRecords))
        paintItem(painter, record);
    paintSelectedIndicator(painter);

    const bool canBack = !m_overflowBackRect.isEmpty() && !m_visibleIndexes.isEmpty() && m_visibleIndexes.first() != allVisibleItemIndexes().value(0, -1);
    const QVector<int> candidates = allVisibleItemIndexes();
    const bool canForward = !m_overflowForwardRect.isEmpty() && !m_visibleIndexes.isEmpty() && !candidates.isEmpty() && m_visibleIndexes.last() != candidates.last();
    paintOverflowButton(painter, m_overflowBackRect, Typography::Icons::ChevronLeftMed, HitRecord{HitKind::OverflowBack, -1}, canBack);
    paintOverflowButton(painter, m_overflowForwardRect, Typography::Icons::ChevronRightMed, HitRecord{HitKind::OverflowForward, -1}, canForward);
    paintOverflowButton(painter, m_overflowMoreRect, Typography::Icons::More, HitRecord{HitKind::OverflowMore, -1}, !m_hiddenIndexes.isEmpty());
}

void SelectorBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    invalidateLayout(false);
}

void SelectorBar::enterEvent(FluentEnterEvent* event)
{
    QWidget::enterEvent(event);
}

void SelectorBar::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    setHoveredHit(HitRecord());
    clearPressedHit();
}

void SelectorBar::mousePressEvent(QMouseEvent* event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    setFocus(Qt::MouseFocusReason);
    const HitRecord hit = hitTest(fluentMousePos(event));
    if (isInteractiveHit(hit)) {
        m_pressedHit = hit;
        if (hit.kind == HitKind::Item)
            focusItem(hit.itemIndex);
        update();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void SelectorBar::mouseMoveEvent(QMouseEvent* event)
{
    if (!isEnabled()) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    setHoveredHit(hitTest(fluentMousePos(event)));
    event->accept();
}

void SelectorBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const HitRecord hit = hitTest(fluentMousePos(event));
    const HitRecord pressed = m_pressedHit;
    clearPressedHit();
    if (sameHit(pressed, hit) && isInteractiveHit(hit)) {
        activateHit(hit);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void SelectorBar::keyPressEvent(QKeyEvent* event)
{
    if (!isEnabled()) {
        QWidget::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Left:
        focusItem(nextSelectableIndex(m_focusedIndex >= 0 ? m_focusedIndex : m_selectedIndex, -1));
        event->accept();
        return;
    case Qt::Key_Right:
        focusItem(nextSelectableIndex(m_focusedIndex >= 0 ? m_focusedIndex : m_selectedIndex, 1));
        event->accept();
        return;
    case Qt::Key_Home:
        focusItem(firstSelectableIndex());
        event->accept();
        return;
    case Qt::Key_End:
        focusItem(lastSelectableIndex());
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (isSelectableIndex(m_focusedIndex)) {
            activateItem(m_focusedIndex);
            event->accept();
            return;
        }
        break;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void SelectorBar::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    if (!isSelectableIndex(m_focusedIndex))
        focusItem(m_selectedIndex >= 0 ? m_selectedIndex : firstSelectableIndex());
    update();
}

void SelectorBar::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    clearPressedHit();
    update();
}

bool SelectorBar::isValidIndex(int index) const
{
    return index >= 0 && index < m_items.size();
}

bool SelectorBar::isSelectableIndex(int index) const
{
    return isValidIndex(index) && m_items.at(index).enabled && m_items.at(index).visible;
}

QVector<int> SelectorBar::allVisibleItemIndexes() const
{
    QVector<int> indexes;
    indexes.reserve(m_items.size());
    for (int index = 0; index < m_items.size(); ++index) {
        if (m_items.at(index).visible)
            indexes.append(index);
    }
    return indexes;
}

QString SelectorBar::normalizedString(const QString& value, const QString& fallback) const
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? fallback : trimmed;
}

QFont SelectorBar::itemFont() const
{
    return themeFont(m_itemFontRole).toQFont();
}

QFont SelectorBar::iconFont(int pixelSize) const
{
    QFont font(m_iconFontFamily);
    font.setPixelSize(pixelSize);
    return font;
}

SelectorBar::Metrics SelectorBar::metrics() const
{
    return Metrics();
}

void SelectorBar::invalidateLayout(bool updateGeometryHint)
{
    m_layoutDirty = true;
    if (updateGeometryHint)
        updateGeometry();
    update();
}

void SelectorBar::ensureLayout() const
{
    if (!m_layoutDirty)
        return;
    const_cast<SelectorBar*>(this)->updateLayout();
}

void SelectorBar::updateLayout()
{
    const Metrics currentMetrics = metrics();
    const QRect bounds = contentsRect();
    m_itemRecords.clear();
    m_visibleIndexes.clear();
    m_hiddenIndexes.clear();
    m_overflowBackRect = QRect();
    m_overflowForwardRect = QRect();
    m_overflowMoreRect = QRect();
    m_rowRect = QRect(bounds.left(), bounds.top(), bounds.width(), currentMetrics.rowHeight);

    QFontMetrics fontMetrics(itemFont());
    QVector<int> widths(m_items.size(), 0);
    QVector<int> candidates = allVisibleItemIndexes();
    int totalWidth = 0;
    for (int index : std::as_const(candidates)) {
        const int width = naturalItemWidth(index, fontMetrics);
        widths[index] = width;
        totalWidth += width;
    }

    bool overflow = totalWidth > m_rowRect.width() && !candidates.isEmpty();
    int availableForItems = qMax(0, m_rowRect.width());
    if (overflow) {
        if (m_overflowBehavior == OverflowBehavior::ScrollButtons)
            availableForItems = qMax(0, availableForItems - currentMetrics.overflowButtonWidth * 2);
        else
            availableForItems = qMax(0, availableForItems - currentMetrics.overflowButtonWidth);
    } else {
        m_firstVisibleIndex = candidates.value(0, 0);
    }

    m_visibleIndexes = computeVisibleIndexes(candidates, availableForItems, widths, overflow);
    if (overflow && isValidIndex(m_selectedIndex) && m_items.at(m_selectedIndex).visible && !m_visibleIndexes.contains(m_selectedIndex)) {
        m_firstVisibleIndex = m_selectedIndex;
        m_visibleIndexes = computeVisibleIndexes(candidates, availableForItems, widths, overflow);
    }

    for (int index = 0; index < m_items.size(); ++index) {
        if (!m_visibleIndexes.contains(index))
            m_hiddenIndexes.append(index);
    }

    int x = m_rowRect.left();
    if (overflow && m_overflowBehavior == OverflowBehavior::ScrollButtons) {
        m_overflowBackRect = QRect(x,
                                   m_rowRect.top() + (m_rowRect.height() - currentMetrics.itemVisualHeight) / 2,
                                   currentMetrics.overflowButtonWidth,
                                   currentMetrics.itemVisualHeight);
        x += currentMetrics.overflowButtonWidth;
    }

    const int rightReserve = overflow
        ? (m_overflowBehavior == OverflowBehavior::ScrollButtons ? currentMetrics.overflowButtonWidth : currentMetrics.overflowButtonWidth)
        : 0;
    const int rightLimit = m_rowRect.right() + 1 - rightReserve;
    for (int index : std::as_const(m_visibleIndexes)) {
        const int preferredWidth = widths.value(index, currentMetrics.minItemWidth);
        const int recordWidth = qMax(0, qMin(preferredWidth, rightLimit - x));
        if (recordWidth <= 0)
            break;

        ItemRecord record;
        record.itemIndex = index;
        record.rect = QRect(x,
                            m_rowRect.top() + (m_rowRect.height() - currentMetrics.itemVisualHeight) / 2,
                            recordWidth,
                            currentMetrics.itemVisualHeight);
        record.enabled = m_items.at(index).enabled;

        int contentX = record.rect.left() + currentMetrics.horizontalPadding;
        if (!m_items.at(index).iconGlyph.isEmpty()) {
            record.iconRect = QRect(contentX,
                                    record.rect.top() + (record.rect.height() - currentMetrics.iconSize) / 2,
                                    currentMetrics.iconSize,
                                    currentMetrics.iconSize);
            contentX = record.iconRect.right() + 1 + currentMetrics.iconGap;
        }
        record.textRect = QRect(contentX,
                                record.rect.top(),
                                qMax(0, record.rect.right() - currentMetrics.horizontalPadding - contentX + 1),
                                record.rect.height());
        const int availableIndicatorWidth = qMax(0, record.rect.width() - currentMetrics.horizontalPadding * 2);
        const int indicatorWidth = qMin(currentMetrics.selectedIndicatorWidth, availableIndicatorWidth);
        record.indicatorRect = QRect(record.rect.center().x() - indicatorWidth / 2,
                                     record.rect.bottom() - currentMetrics.selectedIndicatorHeight + 1,
                                     indicatorWidth,
                                     currentMetrics.selectedIndicatorHeight);
        m_itemRecords.append(record);
        x += recordWidth;
    }

    if (overflow && m_overflowBehavior == OverflowBehavior::ScrollButtons) {
        m_overflowForwardRect = QRect(m_rowRect.right() + 1 - currentMetrics.overflowButtonWidth,
                                      m_rowRect.top() + (m_rowRect.height() - currentMetrics.itemVisualHeight) / 2,
                                      currentMetrics.overflowButtonWidth,
                                      currentMetrics.itemVisualHeight);
    } else if (overflow) {
        m_overflowMoreRect = QRect(m_rowRect.right() + 1 - currentMetrics.overflowButtonWidth,
                                   m_rowRect.top() + (m_rowRect.height() - currentMetrics.itemVisualHeight) / 2,
                                   currentMetrics.overflowButtonWidth,
                                   currentMetrics.itemVisualHeight);
    }

    m_layoutDirty = false;
    if (m_indicatorAnimation->state() != QAbstractAnimation::Running)
        setAnimatedIndicatorRect(indicatorRectForItem(m_selectedIndex));
}

int SelectorBar::naturalItemWidth(int index, const QFontMetrics& fontMetrics) const
{
    if (!isValidIndex(index) || !m_items.at(index).visible)
        return 0;
    const Metrics currentMetrics = metrics();
    int width = currentMetrics.horizontalPadding * 2 + fontMetrics.horizontalAdvance(m_items.at(index).text);
    if (!m_items.at(index).iconGlyph.isEmpty())
        width += currentMetrics.iconSize + currentMetrics.iconGap;
    return qBound(currentMetrics.minItemWidth, width, currentMetrics.maxItemWidth);
}

QVector<int> SelectorBar::computeVisibleIndexes(const QVector<int>& candidates, int availableWidth, const QVector<int>& widths, bool overflow) const
{
    QVector<int> result;
    if (candidates.isEmpty())
        return result;
    if (!overflow) {
        return candidates;
    }

    int startPosition = 0;
    for (int i = 0; i < candidates.size(); ++i) {
        if (candidates.at(i) >= m_firstVisibleIndex) {
            startPosition = i;
            break;
        }
    }

    int used = 0;
    for (int position = startPosition; position < candidates.size(); ++position) {
        const int index = candidates.at(position);
        const int width = widths.value(index, metrics().minItemWidth);
        if (!result.isEmpty() && used + width > availableWidth)
            break;
        if (result.isEmpty() && width > availableWidth) {
            result.append(index);
            break;
        }
        result.append(index);
        used += width;
    }
    if (result.isEmpty())
        result.append(candidates.at(startPosition));
    return result;
}

void SelectorBar::ensureIndexVisible(int index)
{
    if (!isValidIndex(index) || !m_items.at(index).visible)
        return;
    ensureLayout();
    if (!m_visibleIndexes.contains(index)) {
        m_firstVisibleIndex = index;
        invalidateLayout(false);
    }
}

int SelectorBar::nearestSelectableIndex(int preferredIndex) const
{
    if (isSelectableIndex(preferredIndex))
        return preferredIndex;
    if (m_items.isEmpty())
        return -1;

    const int clamped = qBound(0, preferredIndex, m_items.size() - 1);
    for (int offset = 1; offset < m_items.size(); ++offset) {
        const int forward = clamped + offset;
        if (isSelectableIndex(forward))
            return forward;
        const int backward = clamped - offset;
        if (isSelectableIndex(backward))
            return backward;
    }
    return -1;
}

int SelectorBar::firstSelectableIndex() const
{
    for (int index = 0; index < m_items.size(); ++index) {
        if (isSelectableIndex(index))
            return index;
    }
    return -1;
}

int SelectorBar::lastSelectableIndex() const
{
    for (int index = m_items.size() - 1; index >= 0; --index) {
        if (isSelectableIndex(index))
            return index;
    }
    return -1;
}

int SelectorBar::nextSelectableIndex(int from, int direction) const
{
    if (direction == 0 || m_items.isEmpty())
        return -1;
    int index = from;
    if (!isValidIndex(index))
        index = direction > 0 ? -1 : m_items.size();
    while (true) {
        index += direction > 0 ? 1 : -1;
        if (!isValidIndex(index))
            return from >= 0 && isSelectableIndex(from) ? from : firstSelectableIndex();
        if (isSelectableIndex(index))
            return index;
    }
}

void SelectorBar::clampSelectionAfterMutation(int preferredIndex, bool emitSignals)
{
    if (isSelectableIndex(m_selectedIndex))
        return;
    setSelectedIndexInternal(nearestSelectableIndex(preferredIndex), emitSignals);
}

void SelectorBar::setSelectedIndexInternal(int index, bool emitSignals)
{
    if (index != -1 && !isSelectableIndex(index))
        return;
    if (m_selectedIndex == index)
        return;

    ensureLayout();
    const QRect oldIndicator = indicatorRectForItem(m_selectedIndex);
    m_selectedIndex = index;
    if (isSelectableIndex(index))
        m_focusedIndex = index;
    syncSelectedFlags();
    ensureIndexVisible(index);
    invalidateLayout(false);
    ensureLayout();
    animateIndicator(oldIndicator, indicatorRectForItem(m_selectedIndex));
    updateAccessibleText();
    if (emitSignals) {
        emit selectedIndexChanged(m_selectedIndex);
        emit currentChanged(m_selectedIndex);
        emit selectionChanged(m_selectedIndex, selectedItem());
    }
}

void SelectorBar::syncSelectedFlags()
{
    for (int index = 0; index < m_items.size(); ++index)
        m_items[index].selected = index == m_selectedIndex;
}

void SelectorBar::updateAccessibleText()
{
    const QString selectedText = isValidIndex(m_selectedIndex)
        ? (m_items.at(m_selectedIndex).accessibleName.isEmpty() ? m_items.at(m_selectedIndex).text : m_items.at(m_selectedIndex).accessibleName)
        : QStringLiteral("None");
    int visibleCount = 0;
    for (const SelectorBarItem& item : std::as_const(m_items)) {
        if (item.visible)
            ++visibleCount;
    }
    setAccessibleName(QStringLiteral("SelectorBar"));
    setAccessibleDescription(QStringLiteral("Selected item: %1. Visible item count: %2").arg(selectedText).arg(visibleCount));
}

QRect SelectorBar::indicatorRectForItem(int index) const
{
    for (const ItemRecord& record : m_itemRecords) {
        if (record.itemIndex == index)
            return record.indicatorRect;
    }
    return QRect();
}

void SelectorBar::setAnimatedIndicatorRect(const QRect& rect)
{
    if (m_animatedIndicatorRect == rect)
        return;
    m_animatedIndicatorRect = rect;
    setProperty("animatedIndicatorRect", rect);
    update();
}

void SelectorBar::animateIndicator(const QRect& from, const QRect& to)
{
    m_indicatorAnimation->stop();
    if (!isVisible() || to.isEmpty()) {
        setAnimatedIndicatorRect(to);
        return;
    }

    const QRect collapsedFrom = collapsedIndicatorRect(from.isEmpty() ? to : from);
    const QRect collapsedTo = collapsedIndicatorRect(to);
    setAnimatedIndicatorRect(collapsedFrom);
    m_indicatorAnimation->setStartValue(collapsedFrom);
    m_indicatorAnimation->setKeyValueAt(0.55, collapsedTo);
    m_indicatorAnimation->setEndValue(to);
    m_indicatorAnimation->start();
}

const SelectorBar::ItemRecord* SelectorBar::recordForItem(int index) const
{
    ensureLayout();
    for (const ItemRecord& record : m_itemRecords) {
        if (record.itemIndex == index)
            return &record;
    }
    return nullptr;
}

SelectorBar::HitRecord SelectorBar::hitTest(const QPoint& position) const
{
    ensureLayout();
    if (m_overflowBackRect.contains(position))
        return HitRecord{HitKind::OverflowBack, -1};
    if (m_overflowForwardRect.contains(position))
        return HitRecord{HitKind::OverflowForward, -1};
    if (m_overflowMoreRect.contains(position))
        return HitRecord{HitKind::OverflowMore, -1};
    for (const ItemRecord& record : m_itemRecords) {
        if (record.rect.contains(position))
            return HitRecord{HitKind::Item, record.itemIndex};
    }
    return HitRecord();
}

bool SelectorBar::sameHit(const HitRecord& lhs, const HitRecord& rhs) const
{
    return lhs.kind == rhs.kind && lhs.itemIndex == rhs.itemIndex;
}

bool SelectorBar::isInteractiveHit(const HitRecord& hit) const
{
    if (!isEnabled())
        return false;
    const QVector<int> candidates = allVisibleItemIndexes();
    switch (hit.kind) {
    case HitKind::Item:
        return isSelectableIndex(hit.itemIndex);
    case HitKind::OverflowBack:
        return !m_overflowBackRect.isEmpty() && !m_visibleIndexes.isEmpty() && !candidates.isEmpty() && m_visibleIndexes.first() != candidates.first();
    case HitKind::OverflowForward:
        return !m_overflowForwardRect.isEmpty() && !m_visibleIndexes.isEmpty() && !candidates.isEmpty() && m_visibleIndexes.last() != candidates.last();
    case HitKind::OverflowMore:
        return !m_overflowMoreRect.isEmpty() && !m_hiddenIndexes.isEmpty();
    case HitKind::None:
        return false;
    }
    return false;
}

void SelectorBar::setHoveredHit(const HitRecord& hit)
{
    const HitRecord normalized = isInteractiveHit(hit) ? hit : HitRecord();
    if (sameHit(m_hoveredHit, normalized))
        return;
    m_hoveredHit = normalized;
    update();
}

void SelectorBar::clearPressedHit()
{
    if (m_pressedHit.kind == HitKind::None)
        return;
    m_pressedHit = HitRecord();
    update();
}

void SelectorBar::focusItem(int index)
{
    if (!isSelectableIndex(index))
        return;
    if (m_focusedIndex == index && m_visibleIndexes.contains(index))
        return;
    m_focusedIndex = index;
    ensureIndexVisible(index);
    update();
}

void SelectorBar::activateHit(const HitRecord& hit)
{
    if (!isInteractiveHit(hit))
        return;
    switch (hit.kind) {
    case HitKind::Item:
        activateItem(hit.itemIndex);
        return;
    case HitKind::OverflowBack:
        scrollOverflow(-1);
        return;
    case HitKind::OverflowForward:
        scrollOverflow(1);
        return;
    case HitKind::OverflowMore:
        emit overflowActivated(m_hiddenIndexes);
        return;
    case HitKind::None:
        return;
    }
}

void SelectorBar::activateItem(int index)
{
    if (!isSelectableIndex(index) || m_selectedIndex == index)
        return;
    emit itemActivated(index, m_items.at(index));
    setSelectedIndexInternal(index, true);
}

void SelectorBar::scrollOverflow(int direction)
{
    const QVector<int> candidates = allVisibleItemIndexes();
    if (candidates.isEmpty())
        return;
    int position = candidates.indexOf(m_firstVisibleIndex);
    if (position < 0)
        position = 0;
    position = qBound(0, position + (direction < 0 ? -1 : 1), candidates.size() - 1);
    m_firstVisibleIndex = candidates.at(position);
    invalidateLayout(false);
}

void SelectorBar::paintBackground(QPainter& painter) const
{
    painter.fillRect(rect(), themeColors().bgLayer);
}

void SelectorBar::paintItem(QPainter& painter, const ItemRecord& record) const
{
    if (record.rect.isEmpty() || !isValidIndex(record.itemIndex))
        return;

    const Metrics currentMetrics = metrics();
    const QColor textColorValue = itemTextColor(record.itemIndex);
    painter.save();
    if (!record.iconRect.isEmpty()) {
        painter.setFont(iconFont(currentMetrics.iconSize));
        painter.setPen(textColorValue);
        painter.drawText(record.iconRect, Qt::AlignCenter, m_items.at(record.itemIndex).iconGlyph);
    }

    painter.setFont(itemFont());
    painter.setPen(textColorValue);
    painter.drawText(record.textRect,
                     Qt::AlignVCenter | Qt::AlignLeft,
                     m_items.at(record.itemIndex).text);

    painter.restore();
}

void SelectorBar::paintSelectedIndicator(QPainter& painter) const
{
    if (m_animatedIndicatorRect.isEmpty())
        return;
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(themeColors().accentDefault);
    painter.drawRoundedRect(m_animatedIndicatorRect,
                            ::CornerRadius::Indicator, ::CornerRadius::Indicator);
    painter.restore();
}

void SelectorBar::paintOverflowButton(QPainter& painter, const QRect& rect, const QString& glyph, const HitRecord& hit, bool enabled) const
{
    if (rect.isEmpty())
        return;
    painter.save();
    painter.setFont(iconFont(metrics().iconSize));
    const bool highlighted = sameHit(m_pressedHit, hit) || sameHit(m_hoveredHit, hit);
    painter.setPen(enabled ? (highlighted ? themeColors().textPrimary : themeColors().textSecondary) : themeColors().textDisabled);
    painter.drawText(rect, Qt::AlignCenter, glyph);
    painter.restore();
}

QColor SelectorBar::itemTextColor(int index) const
{
    if (!isEnabled() || !isValidIndex(index) || !m_items.at(index).enabled)
        return themeColors().textDisabled;
    const HitRecord hit{HitKind::Item, index};
    if (index == m_selectedIndex || sameHit(m_pressedHit, hit) || sameHit(m_hoveredHit, hit))
        return themeColors().textPrimary;
    return themeColors().textSecondary;
}

} // namespace fluent::navigation