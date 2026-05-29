#include "Pivot.h"

#include <algorithm>
#include <utility>

#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

#include "compatibility/QtCompat.h"
#include "design/Typography.h"

namespace fluent::navigation {

namespace {
constexpr int kDefaultWidth = 640;
} // namespace

PivotItem::PivotItem(const QString& itemHeader)
    : header(itemHeader)
{
}

PivotItem::PivotItem(const QString& itemHeader,
                     const QString& itemIconGlyph,
                     bool itemEnabled,
                     const QVariant& itemData,
                     const QString& itemAccessibleName)
    : header(itemHeader)
    , iconGlyph(itemIconGlyph)
    , enabled(itemEnabled)
    , data(itemData)
    , accessibleName(itemAccessibleName)
{
}

Pivot::Pivot(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    updateAccessibleText();
}

Pivot::~Pivot() = default;

PivotItem Pivot::itemAt(int index) const
{
    return isValidIndex(index) ? m_items.at(index) : PivotItem();
}

int Pivot::addItem(const QString& header)
{
    return addItem(PivotItem(header));
}

int Pivot::addItem(const PivotItem& item)
{
    const int index = m_items.size();
    insertItem(index, item);
    return index;
}

bool Pivot::insertItem(int index, const QString& header)
{
    return insertItem(index, PivotItem(header));
}

bool Pivot::insertItem(int index, const PivotItem& item)
{
    if (index < 0 || index > m_items.size())
        return false;

    const int previousIndex = m_selectedIndex;
    m_items.insert(index, item);

    if (m_selectedIndex >= index)
        ++m_selectedIndex;
    if (m_selectedIndex < 0 && item.enabled)
        m_selectedIndex = index;
    if (!isSelectableIndex(m_selectedIndex))
        m_selectedIndex = firstEnabledIndex();

    if (m_focusedIndex >= index)
        ++m_focusedIndex;
    if (!isSelectableIndex(m_focusedIndex))
        m_focusedIndex = m_selectedIndex >= 0 ? m_selectedIndex : firstEnabledIndex();

    invalidateLayout();
    updateAccessibleText();
    if (m_selectedIndex != previousIndex) {
        emit this->selectedIndexChanged(m_selectedIndex);
        emit this->currentChanged(m_selectedIndex);
    }
    emit this->itemCountChanged(m_items.size());
    emit this->itemsChanged();
    return true;
}

bool Pivot::removeItem(int index)
{
    if (!isValidIndex(index))
        return false;

    const int previousIndex = m_selectedIndex;
    m_items.removeAt(index);
    clampSelectedIndexAfterMutation(qMin(index, m_items.size() - 1), previousIndex, false);
    if (m_focusedIndex == index)
        m_focusedIndex = m_selectedIndex >= 0 ? m_selectedIndex : firstEnabledIndex();
    else if (m_focusedIndex > index)
        --m_focusedIndex;

    invalidateLayout();
    updateAccessibleText();
    if (m_selectedIndex != previousIndex) {
        emit this->selectedIndexChanged(m_selectedIndex);
        emit this->currentChanged(m_selectedIndex);
    }
    emit this->itemCountChanged(m_items.size());
    emit this->itemsChanged();
    return true;
}

void Pivot::clearItems()
{
    if (m_items.isEmpty())
        return;

    const bool selectionChanged = m_selectedIndex != -1;
    m_items.clear();
    m_selectedIndex = -1;
    m_focusedIndex = -1;
    m_firstVisibleIndex = 0;
    m_hoveredHit = HitRecord();
    m_pressedHit = HitRecord();
    invalidateLayout();
    updateAccessibleText();
    if (selectionChanged) {
        emit this->selectedIndexChanged(-1);
        emit this->currentChanged(-1);
    }
    emit this->itemCountChanged(m_items.size());
    emit this->itemsChanged();
}

bool Pivot::setItemHeader(int index, const QString& header)
{
    if (!isValidIndex(index) || m_items.at(index).header == header)
        return false;
    m_items[index].header = header;
    invalidateLayout();
    updateAccessibleText();
    emit this->itemsChanged();
    return true;
}

bool Pivot::setItemIconGlyph(int index, const QString& glyph)
{
    if (!isValidIndex(index) || m_items.at(index).iconGlyph == glyph)
        return false;
    m_items[index].iconGlyph = glyph;
    invalidateLayout();
    emit this->itemsChanged();
    return true;
}

bool Pivot::setItemEnabled(int index, bool enabled)
{
    if (!isValidIndex(index) || m_items.at(index).enabled == enabled)
        return false;

    const int previousIndex = m_selectedIndex;
    m_items[index].enabled = enabled;
    if (!enabled && m_selectedIndex == index)
        clampSelectedIndexAfterMutation(index + 1 < m_items.size() ? index + 1 : index - 1, previousIndex, false);
    else if (m_selectedIndex < 0 && enabled)
        m_selectedIndex = index;
    if (!isSelectableIndex(m_focusedIndex))
        m_focusedIndex = m_selectedIndex >= 0 ? m_selectedIndex : firstEnabledIndex();

    invalidateLayout();
    updateAccessibleText();
    if (m_selectedIndex != previousIndex) {
        emit this->selectedIndexChanged(m_selectedIndex);
        emit this->currentChanged(m_selectedIndex);
    }
    emit this->itemsChanged();
    return true;
}

bool Pivot::setItemData(int index, const QVariant& data)
{
    if (!isValidIndex(index) || m_items.at(index).data == data)
        return false;
    m_items[index].data = data;
    emit this->itemsChanged();
    return true;
}

bool Pivot::setItemAccessibleName(int index, const QString& accessibleName)
{
    if (!isValidIndex(index) || m_items.at(index).accessibleName == accessibleName)
        return false;
    m_items[index].accessibleName = accessibleName;
    updateAccessibleText();
    emit this->itemsChanged();
    return true;
}

void Pivot::setSelectedIndex(int index)
{
    if (!isSelectableIndex(index))
        return;
    setSelectedIndexInternal(index, true, true);
}

void Pivot::clearSelection()
{
    setSelectedIndexInternal(-1, true, false);
}

void Pivot::setOverflowBehavior(OverflowBehavior behavior)
{
    if (m_overflowBehavior == behavior)
        return;
    m_overflowBehavior = behavior;
    invalidateLayout(false);
    emit this->overflowBehaviorChanged(m_overflowBehavior);
}

void Pivot::setItemFontRole(const QString& role)
{
    const QString normalized = normalizedString(role, QStringLiteral("Body"));
    if (m_itemFontRole == normalized)
        return;
    m_itemFontRole = normalized;
    invalidateLayout();
    emit this->itemFontRoleChanged(m_itemFontRole);
}

void Pivot::setIconFontFamily(const QString& family)
{
    const QString normalized = normalizedString(family, Typography::FontFamily::SegoeFluentIcons);
    if (m_iconFontFamily == normalized)
        return;
    m_iconFontFamily = normalized;
    invalidateLayout(false);
    emit this->iconFontFamilyChanged(m_iconFontFamily);
}

QRect Pivot::headerRowGeometry() const
{
    ensureLayout();
    return m_headerRowRect;
}

QRect Pivot::itemHeaderGeometry(int index) const
{
    ensureLayout();
    if (const HeaderRecord* record = recordForItem(index))
        return record->rect;
    return QRect();
}

QRect Pivot::selectedIndicatorGeometry(int index) const
{
    ensureLayout();
    if (const HeaderRecord* record = recordForItem(index))
        return record->indicatorRect;
    return QRect();
}

QRect Pivot::overflowBackGeometry() const
{
    ensureLayout();
    return m_overflowBackRect;
}

QRect Pivot::overflowForwardGeometry() const
{
    ensureLayout();
    return m_overflowForwardRect;
}

QRect Pivot::overflowGeometry() const
{
    ensureLayout();
    return m_overflowMoreRect;
}

QVector<int> Pivot::visibleItemIndexes() const
{
    ensureLayout();
    return m_visibleIndexes;
}

QVector<int> Pivot::hiddenItemIndexes() const
{
    ensureLayout();
    return m_hiddenIndexes;
}

QSize Pivot::sizeHint() const
{
    return QSize(kDefaultWidth, metrics().headerRowHeight);
}

QSize Pivot::minimumSizeHint() const
{
    const Metrics currentMetrics = metrics();
    return QSize(currentMetrics.overflowButtonWidth * 3, currentMetrics.headerRowHeight);
}

void Pivot::onThemeUpdated()
{
    invalidateLayout(false);
}

void Pivot::paintEvent(QPaintEvent*)
{
    ensureLayout();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    paintBackground(painter);
    for (const HeaderRecord& record : std::as_const(m_headerRecords))
        paintHeader(painter, record);

    const bool canBack = m_firstVisibleIndex > 0;
    const bool canForward = !m_visibleIndexes.isEmpty() && m_visibleIndexes.last() < m_items.size() - 1;
    paintOverflowButton(painter, m_overflowBackRect, Typography::Icons::ChevronLeftMed, HitRecord{HitKind::OverflowBack, -1}, canBack);
    paintOverflowButton(painter, m_overflowForwardRect, Typography::Icons::ChevronRightMed, HitRecord{HitKind::OverflowForward, -1}, canForward);
    paintOverflowButton(painter, m_overflowMoreRect, Typography::Icons::More, HitRecord{HitKind::OverflowMore, -1}, !m_hiddenIndexes.isEmpty());
}

void Pivot::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    invalidateLayout(false);
}

void Pivot::enterEvent(FluentEnterEvent* event)
{
    QWidget::enterEvent(event);
}

void Pivot::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    setHoveredHit(HitRecord());
    clearPressedHit();
}

void Pivot::mousePressEvent(QMouseEvent* event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    setFocus(Qt::MouseFocusReason);
    const HitRecord hit = hitTest(fluentMousePos(event));
    if (isInteractiveHit(hit)) {
        m_pressedHit = hit;
        if (hit.kind == HitKind::Header)
            focusItem(hit.itemIndex);
        update();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void Pivot::mouseMoveEvent(QMouseEvent* event)
{
    if (!isEnabled()) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    setHoveredHit(hitTest(fluentMousePos(event)));
    event->accept();
}

void Pivot::mouseReleaseEvent(QMouseEvent* event)
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

void Pivot::keyPressEvent(QKeyEvent* event)
{
    if (!isEnabled()) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Tab && event->modifiers().testFlag(Qt::ControlModifier)) {
        const int direction = event->modifiers().testFlag(Qt::ShiftModifier) ? -1 : 1;
        const int next = nextEnabledIndex(m_selectedIndex >= 0 ? m_selectedIndex : m_focusedIndex, direction);
        if (next >= 0) {
            focusItem(next);
            activateHeader(next);
            event->accept();
            return;
        }
    }

    switch (event->key()) {
    case Qt::Key_Left:
        focusItem(nextEnabledIndex(m_focusedIndex >= 0 ? m_focusedIndex : m_selectedIndex, -1));
        event->accept();
        return;
    case Qt::Key_Right:
        focusItem(nextEnabledIndex(m_focusedIndex >= 0 ? m_focusedIndex : m_selectedIndex, 1));
        event->accept();
        return;
    case Qt::Key_Home:
        focusItem(firstEnabledIndex());
        event->accept();
        return;
    case Qt::Key_End:
        focusItem(lastEnabledIndex());
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (isSelectableIndex(m_focusedIndex)) {
            activateHeader(m_focusedIndex);
            event->accept();
            return;
        }
        break;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void Pivot::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    if (!isSelectableIndex(m_focusedIndex))
        focusItem(m_selectedIndex >= 0 ? m_selectedIndex : firstEnabledIndex());
    update();
}

void Pivot::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    clearPressedHit();
    update();
}

bool Pivot::isValidIndex(int index) const
{
    return index >= 0 && index < m_items.size();
}

bool Pivot::isSelectableIndex(int index) const
{
    return isValidIndex(index) && m_items.at(index).enabled;
}

QString Pivot::normalizedString(const QString& value, const QString& fallback) const
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? fallback : trimmed;
}

QFont Pivot::itemFont() const
{
    return themeFont(m_itemFontRole).toQFont();
}

QFont Pivot::iconFont(int pixelSize) const
{
    QFont font(m_iconFontFamily);
    font.setPixelSize(pixelSize);
    return font;
}

Pivot::Metrics Pivot::metrics() const
{
    return Metrics();
}

void Pivot::invalidateLayout(bool updateGeometryHint)
{
    m_layoutDirty = true;
    if (updateGeometryHint)
        updateGeometry();
    update();
}

void Pivot::ensureLayout() const
{
    if (!m_layoutDirty)
        return;
    const_cast<Pivot*>(this)->updateLayout();
}

void Pivot::updateLayout()
{
    const Metrics currentMetrics = metrics();
    const QRect bounds = contentsRect();
    m_headerRecords.clear();
    m_visibleIndexes.clear();
    m_hiddenIndexes.clear();
    m_overflowBackRect = QRect();
    m_overflowForwardRect = QRect();
    m_overflowMoreRect = QRect();

    m_headerRowRect = QRect(bounds.left(), bounds.top(), bounds.width(), currentMetrics.headerRowHeight);

    QFontMetrics fontMetrics(itemFont());
    QVector<int> widths;
    widths.reserve(m_items.size());
    int totalWidth = 0;
    for (int index = 0; index < m_items.size(); ++index) {
        const int width = naturalHeaderWidth(index, fontMetrics);
        widths.append(width);
        totalWidth += width;
    }

    bool overflow = totalWidth > m_headerRowRect.width() && !m_items.isEmpty();
    int availableForHeaders = qMax(0, m_headerRowRect.width());
    if (overflow) {
        if (m_overflowBehavior == OverflowBehavior::ScrollButtons)
            availableForHeaders = qMax(0, availableForHeaders - currentMetrics.overflowButtonWidth * 2);
        else
            availableForHeaders = qMax(0, availableForHeaders - currentMetrics.overflowButtonWidth);
        m_firstVisibleIndex = qBound(0, m_firstVisibleIndex, qMax(0, m_items.size() - 1));
    } else {
        m_firstVisibleIndex = 0;
    }

    m_visibleIndexes = computeVisibleIndexes(availableForHeaders, widths, overflow);
    for (int index = 0; index < m_items.size(); ++index) {
        if (!m_visibleIndexes.contains(index))
            m_hiddenIndexes.append(index);
    }

    int x = m_headerRowRect.left();
    if (overflow && m_overflowBehavior == OverflowBehavior::ScrollButtons) {
        m_overflowBackRect = QRect(x, m_headerRowRect.top() + (m_headerRowRect.height() - currentMetrics.headerVisualHeight) / 2, currentMetrics.overflowButtonWidth, currentMetrics.headerVisualHeight);
        x += currentMetrics.overflowButtonWidth;
    }

    const int rightLimit = m_headerRowRect.right() + 1 - ((overflow && m_overflowBehavior != OverflowBehavior::ScrollButtons) ? currentMetrics.overflowButtonWidth : 0) - ((overflow && m_overflowBehavior == OverflowBehavior::ScrollButtons) ? currentMetrics.overflowButtonWidth : 0);
    for (int index : std::as_const(m_visibleIndexes)) {
        const int preferredWidth = widths.value(index, currentMetrics.minHeaderWidth);
        const int recordWidth = qMax(0, qMin(preferredWidth, rightLimit - x));
        HeaderRecord record;
        record.itemIndex = index;
        record.rect = QRect(x, m_headerRowRect.top() + (m_headerRowRect.height() - currentMetrics.headerVisualHeight) / 2, recordWidth, currentMetrics.headerVisualHeight);
        record.enabled = m_items.at(index).enabled;

        int contentX = record.rect.left() + currentMetrics.horizontalPadding;
        if (!m_items.at(index).iconGlyph.isEmpty()) {
            record.iconRect = QRect(contentX, record.rect.top() + (record.rect.height() - currentMetrics.iconSize) / 2, currentMetrics.iconSize, currentMetrics.iconSize);
            contentX = record.iconRect.right() + 1 + currentMetrics.iconGap;
        }
        record.textRect = QRect(contentX, record.rect.top(), qMax(0, record.rect.right() - currentMetrics.horizontalPadding - contentX + 1), record.rect.height());
        record.indicatorRect = QRect(record.rect.left() + currentMetrics.horizontalPadding,
                                     record.rect.bottom() - currentMetrics.selectedIndicatorHeight + 1,
                                     qMax(0, record.rect.width() - currentMetrics.horizontalPadding * 2),
                                     currentMetrics.selectedIndicatorHeight);
        m_headerRecords.append(record);
        x += recordWidth;
    }

    if (overflow && m_overflowBehavior == OverflowBehavior::ScrollButtons) {
        m_overflowForwardRect = QRect(m_headerRowRect.right() + 1 - currentMetrics.overflowButtonWidth,
                                      m_headerRowRect.top() + (m_headerRowRect.height() - currentMetrics.headerVisualHeight) / 2,
                                      currentMetrics.overflowButtonWidth,
                                      currentMetrics.headerVisualHeight);
    } else if (overflow) {
        m_overflowMoreRect = QRect(m_headerRowRect.right() + 1 - currentMetrics.overflowButtonWidth,
                                   m_headerRowRect.top() + (m_headerRowRect.height() - currentMetrics.headerVisualHeight) / 2,
                                   currentMetrics.overflowButtonWidth,
                                   currentMetrics.headerVisualHeight);
    }

    m_layoutDirty = false;
}

int Pivot::naturalHeaderWidth(int index, const QFontMetrics& fontMetrics) const
{
    if (!isValidIndex(index))
        return 0;
    const Metrics currentMetrics = metrics();
    int width = currentMetrics.horizontalPadding * 2 + fontMetrics.horizontalAdvance(m_items.at(index).header);
    if (!m_items.at(index).iconGlyph.isEmpty())
        width += currentMetrics.iconSize + currentMetrics.iconGap;
    return qBound(currentMetrics.minHeaderWidth, width, currentMetrics.maxHeaderWidth);
}

QVector<int> Pivot::computeVisibleIndexes(int availableWidth, const QVector<int>& widths, bool overflow) const
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
    const int start = qBound(0, m_firstVisibleIndex, qMax(0, m_items.size() - 1));
    for (int index = start; index < m_items.size(); ++index) {
        const int width = widths.value(index, metrics().minHeaderWidth);
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
        result.append(start);
    return result;
}

void Pivot::ensureSelectedHeaderVisible()
{
    if (!isValidIndex(m_selectedIndex))
        return;
    if (m_selectedIndex < m_firstVisibleIndex)
        m_firstVisibleIndex = m_selectedIndex;
    if (!m_visibleIndexes.isEmpty() && m_selectedIndex > m_visibleIndexes.last())
        m_firstVisibleIndex = m_selectedIndex;
    m_firstVisibleIndex = qBound(0, m_firstVisibleIndex, qMax(0, m_items.size() - 1));
}

void Pivot::clampSelectedIndexAfterMutation(int preferredIndex, int previousIndex, bool emitSelection)
{
    Q_UNUSED(previousIndex)
    if (isSelectableIndex(m_selectedIndex))
        return;
    if (isSelectableIndex(preferredIndex)) {
        m_selectedIndex = preferredIndex;
        return;
    }
    m_selectedIndex = firstEnabledIndex();
    if (emitSelection) {
        emit this->selectedIndexChanged(m_selectedIndex);
        emit this->currentChanged(m_selectedIndex);
    }
}

int Pivot::firstEnabledIndex() const
{
    for (int index = 0; index < m_items.size(); ++index) {
        if (isSelectableIndex(index))
            return index;
    }
    return -1;
}

int Pivot::lastEnabledIndex() const
{
    for (int index = m_items.size() - 1; index >= 0; --index) {
        if (isSelectableIndex(index))
            return index;
    }
    return -1;
}

int Pivot::nextEnabledIndex(int from, int direction) const
{
    if (direction == 0 || m_items.isEmpty())
        return -1;
    int index = from;
    if (!isValidIndex(index))
        index = direction > 0 ? -1 : m_items.size();
    while (true) {
        index += direction > 0 ? 1 : -1;
        if (!isValidIndex(index))
            return from >= 0 && isSelectableIndex(from) ? from : firstEnabledIndex();
        if (isSelectableIndex(index))
            return index;
    }
}

void Pivot::setSelectedIndexInternal(int index, bool emitSignals, bool animated)
{
    if (index != -1 && !isSelectableIndex(index))
        return;
    if (m_selectedIndex == index)
        return;

    const int previousIndex = m_selectedIndex;
    m_selectedIndex = index;
    if (isSelectableIndex(index))
        m_focusedIndex = index;
    ensureSelectedHeaderVisible();
    invalidateLayout(false);
    Q_UNUSED(previousIndex)
    Q_UNUSED(animated)
    updateAccessibleText();
    if (emitSignals) {
        emit this->selectedIndexChanged(m_selectedIndex);
        emit this->currentChanged(m_selectedIndex);
    }
}

void Pivot::updateAccessibleText()
{
    const QString selectedText = isValidIndex(m_selectedIndex)
        ? (m_items.at(m_selectedIndex).accessibleName.isEmpty() ? m_items.at(m_selectedIndex).header : m_items.at(m_selectedIndex).accessibleName)
        : QStringLiteral("None");
    setAccessibleName(QStringLiteral("Pivot"));
    setAccessibleDescription(QStringLiteral("Selected item: %1. Item count: %2").arg(selectedText).arg(m_items.size()));
}

const Pivot::HeaderRecord* Pivot::recordForItem(int index) const
{
    ensureLayout();
    for (const HeaderRecord& record : m_headerRecords) {
        if (record.itemIndex == index)
            return &record;
    }
    return nullptr;
}

Pivot::HitRecord Pivot::hitTest(const QPoint& position) const
{
    ensureLayout();
    if (m_overflowBackRect.contains(position))
        return HitRecord{HitKind::OverflowBack, -1};
    if (m_overflowForwardRect.contains(position))
        return HitRecord{HitKind::OverflowForward, -1};
    if (m_overflowMoreRect.contains(position))
        return HitRecord{HitKind::OverflowMore, -1};
    for (const HeaderRecord& record : m_headerRecords) {
        if (record.rect.contains(position))
            return HitRecord{HitKind::Header, record.itemIndex};
    }
    return HitRecord();
}

bool Pivot::sameHit(const HitRecord& lhs, const HitRecord& rhs) const
{
    return lhs.kind == rhs.kind && lhs.itemIndex == rhs.itemIndex;
}

bool Pivot::isInteractiveHit(const HitRecord& hit) const
{
    if (!isEnabled())
        return false;
    switch (hit.kind) {
    case HitKind::Header:
        return isSelectableIndex(hit.itemIndex);
    case HitKind::OverflowBack:
        return !m_overflowBackRect.isEmpty() && m_firstVisibleIndex > 0;
    case HitKind::OverflowForward:
        return !m_overflowForwardRect.isEmpty() && !m_visibleIndexes.isEmpty() && m_visibleIndexes.last() < m_items.size() - 1;
    case HitKind::OverflowMore:
        return !m_overflowMoreRect.isEmpty() && !m_hiddenIndexes.isEmpty();
    case HitKind::None:
        return false;
    }
    return false;
}

void Pivot::setHoveredHit(const HitRecord& hit)
{
    const HitRecord normalized = isInteractiveHit(hit) ? hit : HitRecord();
    if (sameHit(m_hoveredHit, normalized))
        return;
    m_hoveredHit = normalized;
    update();
}

void Pivot::clearPressedHit()
{
    if (m_pressedHit.kind == HitKind::None)
        return;
    m_pressedHit = HitRecord();
    update();
}

void Pivot::focusItem(int index)
{
    if (!isSelectableIndex(index))
        return;
    if (m_focusedIndex == index)
        return;
    m_focusedIndex = index;
    if (index < m_firstVisibleIndex || !m_visibleIndexes.contains(index)) {
        m_firstVisibleIndex = index;
        invalidateLayout(false);
    }
    update();
}

void Pivot::activateHit(const HitRecord& hit)
{
    if (!isInteractiveHit(hit))
        return;
    switch (hit.kind) {
    case HitKind::Header:
        activateHeader(hit.itemIndex);
        return;
    case HitKind::OverflowBack:
        scrollOverflow(-1);
        return;
    case HitKind::OverflowForward:
        scrollOverflow(1);
        return;
    case HitKind::OverflowMore:
        emit this->overflowActivated(m_hiddenIndexes);
        return;
    case HitKind::None:
        return;
    }
}

void Pivot::activateHeader(int index)
{
    if (!isSelectableIndex(index))
        return;
    emit this->itemActivated(index, m_items.at(index));
    setSelectedIndexInternal(index, true, true);
}

void Pivot::scrollOverflow(int direction)
{
    if (direction < 0)
        m_firstVisibleIndex = qMax(0, m_firstVisibleIndex - 1);
    else if (direction > 0)
        m_firstVisibleIndex = qMin(qMax(0, m_items.size() - 1), m_firstVisibleIndex + 1);
    invalidateLayout(false);
}

void Pivot::paintBackground(QPainter& painter) const
{
    const auto colors = themeColors();
    painter.fillRect(rect(), colors.bgLayer);
    painter.fillRect(QRect(0, 0, width(), m_headerRowRect.bottom() + 1), colors.bgLayer);
}

void Pivot::paintHeader(QPainter& painter, const HeaderRecord& record) const
{
    if (record.rect.isEmpty() || !isValidIndex(record.itemIndex))
        return;
    const Metrics currentMetrics = metrics();

    painter.save();
    const QColor textColorValue = headerTextColor(record.itemIndex);
    if (!record.iconRect.isEmpty()) {
        painter.setFont(iconFont(currentMetrics.iconSize));
        painter.setPen(textColorValue);
        painter.drawText(record.iconRect, Qt::AlignCenter, m_items.at(record.itemIndex).iconGlyph);
    }
    painter.setFont(itemFont());
    painter.setPen(textColorValue);
    painter.drawText(record.textRect, Qt::AlignVCenter | Qt::AlignLeft, m_items.at(record.itemIndex).header);

    if (record.itemIndex == m_selectedIndex && record.enabled && record.indicatorRect.isValid()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(themeColors().accentDefault);
        painter.drawRoundedRect(record.indicatorRect, 1.5, 1.5);
    }
    painter.restore();
}

void Pivot::paintOverflowButton(QPainter& painter, const QRect& rect, const QString& glyph, const HitRecord& hit, bool enabled) const
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

QColor Pivot::headerTextColor(int index) const
{
    if (!isEnabled() || !isValidIndex(index) || !m_items.at(index).enabled)
        return themeColors().textDisabled;
    const HitRecord hit{HitKind::Header, index};
    if (index == m_selectedIndex || sameHit(m_pressedHit, hit) || sameHit(m_hoveredHit, hit))
        return themeColors().textPrimary;
    return themeColors().textSecondary;
}

} // namespace fluent::navigation
