#include "Breadcrumb.h"

#include <algorithm>

#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

#include "design/CornerRadius.h"
#include "design/Spacing.h"
#include "design/Typography.h"

namespace fluent::navigation {

namespace {
constexpr int kMinInteractiveWidth = 4;
constexpr int kStandardRowHeight = 20;
constexpr int kLargeRowHeight = 40;

} // namespace

BreadcrumbItem::BreadcrumbItem(const QString& itemText)
    : text(itemText)
{
}

BreadcrumbItem::BreadcrumbItem(const QString& itemText, const QVariant& itemData, bool itemEnabled, const QString& itemAccessibleName)
    : text(itemText)
    , data(itemData)
    , enabled(itemEnabled)
    , accessibleName(itemAccessibleName)
{
}

Breadcrumb::Breadcrumb(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setFont(contentFont());
    updateAccessibleText();
}

BreadcrumbItem Breadcrumb::itemAt(int index) const
{
    return isValidIndex(index) ? m_items.at(index) : BreadcrumbItem();
}

void Breadcrumb::setItems(const QStringList& items)
{
    QVector<BreadcrumbItem> converted;
    converted.reserve(items.size());
    for (const QString& text : items)
        converted.append(BreadcrumbItem(text));
    setItems(converted);
}

void Breadcrumb::setItems(const QVector<BreadcrumbItem>& items)
{
    m_items = items;
    m_hoveredRecord = -1;
    m_pressedRecord = -1;
    m_focusedRecord = -1;
    invalidateLayout();
    updateAccessibleText();
    emit itemsChanged();
}

void Breadcrumb::appendItem(const BreadcrumbItem& item)
{
    m_items.append(item);
    invalidateLayout();
    updateAccessibleText();
    emit itemsChanged();
}

void Breadcrumb::appendItem(const QString& text)
{
    appendItem(BreadcrumbItem(text));
}

void Breadcrumb::insertItem(int index, const BreadcrumbItem& item)
{
    if (index < 0 || index > m_items.size())
        return;
    m_items.insert(index, item);
    invalidateLayout();
    updateAccessibleText();
    emit itemsChanged();
}

bool Breadcrumb::removeItemAt(int index)
{
    if (!isValidIndex(index))
        return false;
    m_items.removeAt(index);
    invalidateLayout();
    updateAccessibleText();
    emit itemsChanged();
    return true;
}

void Breadcrumb::clearItems()
{
    if (m_items.isEmpty())
        return;
    m_items.clear();
    m_hoveredRecord = -1;
    m_pressedRecord = -1;
    m_focusedRecord = -1;
    invalidateLayout();
    updateAccessibleText();
    emit itemsChanged();
}

void Breadcrumb::setBreadcrumbSize(BreadcrumbSize size)
{
    if (m_breadcrumbSize == size)
        return;
    m_breadcrumbSize = size;
    setFont(contentFont());
    invalidateLayout();
    emit breadcrumbSizeChanged(m_breadcrumbSize);
}

void Breadcrumb::setOverflowMode(OverflowMode mode)
{
    if (m_overflowMode == mode)
        return;
    m_overflowMode = mode;
    invalidateLayout();
    emit overflowModeChanged(m_overflowMode);
}

void Breadcrumb::setAutoTruncateOnItemClick(bool enabled)
{
    if (m_autoTruncateOnItemClick == enabled)
        return;
    m_autoTruncateOnItemClick = enabled;
    emit autoTruncateOnItemClickChanged(m_autoTruncateOnItemClick);
}

void Breadcrumb::setStandardFontRole(const QString& role)
{
    const QString normalized = normalizedFontRole(role, QStringLiteral("Body"));
    if (m_standardFontRole == normalized)
        return;
    m_standardFontRole = normalized;
    if (m_breadcrumbSize == BreadcrumbSize::Standard)
        setFont(contentFont());
    invalidateLayout();
    emit standardFontRoleChanged(m_standardFontRole);
}

void Breadcrumb::setLargeFontRole(const QString& role)
{
    const QString normalized = normalizedFontRole(role, QStringLiteral("Title"));
    if (m_largeFontRole == normalized)
        return;
    m_largeFontRole = normalized;
    if (m_breadcrumbSize == BreadcrumbSize::Large)
        setFont(contentFont());
    invalidateLayout();
    emit largeFontRoleChanged(m_largeFontRole);
}

QRect Breadcrumb::itemGeometry(int itemIndex) const
{
    ensureLayout();
    for (const DisplayRecord& record : m_records) {
        if (record.type == RecordType::Item && record.itemIndex == itemIndex)
            return record.rect;
    }
    return QRect();
}

QRect Breadcrumb::separatorGeometry(int separatorIndex) const
{
    ensureLayout();
    int current = 0;
    for (const DisplayRecord& record : m_records) {
        if (record.type != RecordType::Separator)
            continue;
        if (current == separatorIndex)
            return record.rect;
        ++current;
    }
    return QRect();
}

QRect Breadcrumb::overflowGeometry() const
{
    ensureLayout();
    for (const DisplayRecord& record : m_records) {
        if (record.type == RecordType::Overflow)
            return record.rect;
    }
    return QRect();
}

QVector<int> Breadcrumb::hiddenItemIndexes() const
{
    ensureLayout();
    return m_hiddenItemIndexes;
}

int Breadcrumb::visibleItemCount() const
{
    ensureLayout();
    int count = 0;
    for (const DisplayRecord& record : m_records) {
        if (record.type == RecordType::Item)
            ++count;
    }
    return count;
}

QSize Breadcrumb::sizeHint() const
{
    const Metrics currentMetrics = metrics();
    if (m_items.isEmpty())
        return QSize(120, currentMetrics.rowHeight);

    QFontMetrics fontMetrics(contentFont());
    int width = 0;
    for (int index = 0; index < m_items.size(); ++index) {
        width += naturalItemWidth(index, fontMetrics);
        if (index > 0)
            width += currentMetrics.separatorWidth;
    }
    return QSize(qMax(width, currentMetrics.rowHeight), currentMetrics.rowHeight);
}

QSize Breadcrumb::minimumSizeHint() const
{
    const Metrics currentMetrics = metrics();
    return QSize(currentMetrics.rowHeight, currentMetrics.rowHeight);
}

void Breadcrumb::onThemeUpdated()
{
    setFont(contentFont());
    invalidateLayout(false);
}

void Breadcrumb::paintEvent(QPaintEvent*)
{
    ensureLayout();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    for (int index = 0; index < m_records.size(); ++index) {
        const DisplayRecord& record = m_records.at(index);
        if (record.type == RecordType::Item)
            paintItem(painter, record, index);
        else
            paintIconRecord(painter, record, index);
    }
}

void Breadcrumb::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    invalidateLayout(false);
}

void Breadcrumb::enterEvent(FluentEnterEvent* event)
{
    QWidget::enterEvent(event);
    setHoveredRecord(-1);
}

void Breadcrumb::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    setHoveredRecord(-1);
    resetPressedRecord();
}

void Breadcrumb::mousePressEvent(QMouseEvent* event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    setFocus(Qt::MouseFocusReason);
    const int hit = recordAt(event->pos());
    if (isRecordInteractive(hit)) {
        m_pressedRecord = hit;
        m_focusedRecord = hit;
        update();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void Breadcrumb::mouseMoveEvent(QMouseEvent* event)
{
    if (!isEnabled()) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    setHoveredRecord(recordAt(event->pos()));
    event->accept();
}

void Breadcrumb::mouseReleaseEvent(QMouseEvent* event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const int hit = recordAt(event->pos());
    const int pressed = m_pressedRecord;
    resetPressedRecord();
    if (pressed >= 0 && pressed == hit && isRecordInteractive(hit)) {
        activateRecord(hit);
        event->accept();
        return;
    }
    update();
    QWidget::mouseReleaseEvent(event);
}

void Breadcrumb::keyPressEvent(QKeyEvent* event)
{
    if (!isEnabled()) {
        QWidget::keyPressEvent(event);
        return;
    }

    ensureLayout();
    if (m_focusedRecord < 0)
        m_focusedRecord = firstInteractiveRecord();

    switch (event->key()) {
    case Qt::Key_Left:
        m_focusedRecord = nextInteractiveRecord(m_focusedRecord, -1);
        update();
        event->accept();
        return;
    case Qt::Key_Right:
        m_focusedRecord = nextInteractiveRecord(m_focusedRecord, 1);
        update();
        event->accept();
        return;
    case Qt::Key_Home:
        m_focusedRecord = firstInteractiveRecord();
        update();
        event->accept();
        return;
    case Qt::Key_End:
        m_focusedRecord = lastInteractiveRecord();
        update();
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (isRecordInteractive(m_focusedRecord)) {
            activateRecord(m_focusedRecord);
            event->accept();
            return;
        }
        break;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void Breadcrumb::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    ensureLayout();
    if (m_focusedRecord < 0)
        m_focusedRecord = firstInteractiveRecord();
    update();
}

void Breadcrumb::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    resetPressedRecord();
    update();
}

bool Breadcrumb::isValidIndex(int index) const
{
    return index >= 0 && index < m_items.size();
}

Breadcrumb::Metrics Breadcrumb::metrics() const
{
    Metrics current;
    if (m_breadcrumbSize == BreadcrumbSize::Large) {
        current.rowHeight = kLargeRowHeight;
        current.textHeight = Typography::LineHeight::Title;
        current.separatorWidth = 24;
        current.overflowWidth = 24;
        current.itemHorizontalPadding = ::Spacing::Small;
        current.chevronPixelSize = 16;
        current.overflowPixelSize = 24;
        current.cornerRadius = 5;
    } else {
        current.rowHeight = kStandardRowHeight;
        current.textHeight = Typography::LineHeight::Body;
        current.separatorWidth = 16;
        current.overflowWidth = 16;
        current.itemHorizontalPadding = ::Spacing::XSmall;
        current.chevronPixelSize = 12;
        current.overflowPixelSize = 16;
        current.cornerRadius = CornerRadius::Control;
    }
    return current;
}

QFont Breadcrumb::contentFont() const
{
    const QString role = m_breadcrumbSize == BreadcrumbSize::Large ? m_largeFontRole : m_standardFontRole;
    return themeFont(role).toQFont();
}

QString Breadcrumb::normalizedFontRole(const QString& role, const QString& fallback) const
{
    const QString trimmed = role.trimmed();
    return trimmed.isEmpty() ? fallback : trimmed;
}

void Breadcrumb::invalidateLayout(bool updateGeometryHint)
{
    m_layoutDirty = true;
    if (updateGeometryHint)
        updateGeometry();
    update();
}

void Breadcrumb::ensureLayout() const
{
    if (!m_layoutDirty)
        return;
    const_cast<Breadcrumb*>(this)->updateLayout();
}

void Breadcrumb::updateLayout()
{
    const QVector<int> visibleItems = visibleItemsForCurrentMode();
    const bool hasOverflow = !m_hiddenItemIndexes.isEmpty();
    buildRecords(visibleItems, hasOverflow, m_hiddenItemIndexes);
    m_layoutDirty = false;
    clampFocusedRecord();
}

int Breadcrumb::naturalItemWidth(int index, const QFontMetrics& fontMetrics) const
{
    if (!isValidIndex(index))
        return 0;
    const Metrics currentMetrics = metrics();
    const int textWidth = fontMetrics.horizontalAdvance(m_items.at(index).text);
    return qMax(kMinInteractiveWidth, textWidth + currentMetrics.itemHorizontalPadding * 2);
}

int Breadcrumb::requiredWidthForItems(const QVector<int>& visibleItems, bool hasOverflow) const
{
    if (visibleItems.isEmpty() && !hasOverflow)
        return 0;

    QFontMetrics fontMetrics(contentFont());
    const Metrics currentMetrics = metrics();
    int width = 0;
    int recordCount = 0;
    for (int index : visibleItems) {
        width += naturalItemWidth(index, fontMetrics);
        ++recordCount;
    }
    if (hasOverflow) {
        width += currentMetrics.overflowWidth;
        ++recordCount;
    }
    if (recordCount > 1)
        width += (recordCount - 1) * currentMetrics.separatorWidth;
    return width;
}

QVector<int> Breadcrumb::visibleItemsForCurrentMode() const
{
    QVector<int> allItems;
    allItems.reserve(m_items.size());
    for (int index = 0; index < m_items.size(); ++index)
        allItems.append(index);

    m_hiddenItemIndexes.clear();
    if (m_items.size() <= 1 || m_overflowMode == OverflowMode::None)
        return allItems;

    const int availableWidth = width() > 0 ? width() : sizeHint().width();
    if (requiredWidthForItems(allItems, false) <= availableWidth)
        return allItems;

    if (m_overflowMode == OverflowMode::Beginning) {
        QVector<int> visible;
        visible.prepend(m_items.size() - 1);
        for (int index = m_items.size() - 2; index >= 0; --index) {
            QVector<int> candidate = visible;
            candidate.prepend(index);
            if (requiredWidthForItems(candidate, true) > availableWidth)
                break;
            visible = candidate;
        }
        const int firstVisible = visible.isEmpty() ? m_items.size() : visible.first();
        for (int index = 0; index < firstVisible; ++index)
            m_hiddenItemIndexes.append(index);
        if (m_hiddenItemIndexes.isEmpty())
            return allItems;
        return visible;
    }

    QVector<int> visible;
    visible.append(0);
    if (m_items.size() > 1)
        visible.append(m_items.size() - 1);

    for (int index = m_items.size() - 2; index > 0; --index) {
        QVector<int> candidate = visible;
        candidate.insert(1, index);
        std::sort(candidate.begin(), candidate.end());
        if (requiredWidthForItems(candidate, true) > availableWidth)
            break;
        visible = candidate;
    }

    std::sort(visible.begin(), visible.end());
    for (int index = 0; index < m_items.size(); ++index) {
        if (!visible.contains(index))
            m_hiddenItemIndexes.append(index);
    }
    if (m_hiddenItemIndexes.isEmpty())
        return allItems;
    return visible;
}

void Breadcrumb::buildRecords(const QVector<int>& visibleItems, bool hasOverflow, const QVector<int>& hiddenItems)
{
    m_records.clear();
    if (m_items.isEmpty())
        return;

    QVector<int> sequence = visibleItems;
    std::sort(sequence.begin(), sequence.end());
    const Metrics currentMetrics = metrics();
    QFontMetrics fontMetrics(contentFont());

    const auto appendSeparator = [&]() {
        DisplayRecord separator;
        separator.type = RecordType::Separator;
        separator.naturalWidth = currentMetrics.separatorWidth;
        m_records.append(separator);
    };

    const auto appendItem = [&](int itemIndex) {
        DisplayRecord item;
        item.type = RecordType::Item;
        item.itemIndex = itemIndex;
        item.naturalWidth = naturalItemWidth(itemIndex, fontMetrics);
        m_records.append(item);
    };

    const auto appendOverflow = [&]() {
        DisplayRecord overflow;
        overflow.type = RecordType::Overflow;
        overflow.naturalWidth = currentMetrics.overflowWidth;
        m_records.append(overflow);
    };

    if (!hasOverflow) {
        for (int i = 0; i < sequence.size(); ++i) {
            if (i > 0)
                appendSeparator();
            appendItem(sequence.at(i));
        }
    } else if (m_overflowMode == OverflowMode::Beginning) {
        appendOverflow();
        for (int itemIndex : sequence) {
            appendSeparator();
            appendItem(itemIndex);
        }
    } else {
        const int firstTailIndex = hiddenItems.isEmpty() ? 1 : hiddenItems.last() + 1;
        appendItem(0);
        appendSeparator();
        appendOverflow();
        for (int itemIndex : sequence) {
            if (itemIndex == 0 || itemIndex < firstTailIndex)
                continue;
            appendSeparator();
            appendItem(itemIndex);
        }
    }

    int fixedWidth = 0;
    int naturalItemsWidth = 0;
    for (int index = 0; index < m_records.size(); ++index) {
        if (m_records.at(index).type == RecordType::Item) {
            naturalItemsWidth += m_records.at(index).naturalWidth;
        } else {
            fixedWidth += m_records.at(index).naturalWidth;
        }
    }

    const int availableWidth = qMax(0, contentsRect().width());
    const int remainingForItems = qMax(0, availableWidth - fixedWidth);
    QVector<int> assignedWidths(m_records.size(), 0);
    for (int index = 0; index < m_records.size(); ++index) {
        const DisplayRecord& record = m_records.at(index);
        if (record.type != RecordType::Item || naturalItemsWidth <= remainingForItems) {
            assignedWidths[index] = record.naturalWidth;
            continue;
        }
        assignedWidths[index] = qMax(0, (record.naturalWidth * remainingForItems) / qMax(1, naturalItemsWidth));
    }

    int assignedTotal = 0;
    for (int widthValue : assignedWidths)
        assignedTotal += widthValue;
    if (assignedTotal > availableWidth && !assignedWidths.isEmpty()) {
        int overflow = assignedTotal - availableWidth;
        for (int i = assignedWidths.size() - 1; i >= 0 && overflow > 0; --i) {
            const int delta = qMin(assignedWidths[i], overflow);
            assignedWidths[i] -= delta;
            overflow -= delta;
        }
    }

    const QRect content = contentsRect();
    int x = content.left();
    for (int index = 0; index < m_records.size(); ++index) {
        DisplayRecord& record = m_records[index];
        const int clampedWidth = qMax(0, qMin(assignedWidths.at(index), content.right() + 1 - x));
        record.rect = QRect(x, content.top() + (content.height() - currentMetrics.rowHeight) / 2, clampedWidth, currentMetrics.rowHeight);
        if (record.type == RecordType::Item) {
            const int textTop = record.rect.top() + (record.rect.height() - currentMetrics.textHeight) / 2;
            const int textLeft = record.rect.left() + currentMetrics.itemHorizontalPadding;
            const int textWidth = qMax(0, record.rect.width() - currentMetrics.itemHorizontalPadding * 2);
            record.contentRect = QRect(textLeft, textTop, textWidth, currentMetrics.textHeight);
        } else {
            record.contentRect = record.rect;
        }
        x += assignedWidths.at(index);
    }
}

void Breadcrumb::clampFocusedRecord()
{
    if (isRecordInteractive(m_focusedRecord))
        return;
    m_focusedRecord = firstInteractiveRecord();
}

int Breadcrumb::recordAt(const QPoint& position) const
{
    ensureLayout();
    for (int index = m_records.size() - 1; index >= 0; --index) {
        if (m_records.at(index).rect.contains(position))
            return index;
    }
    return -1;
}

int Breadcrumb::firstInteractiveRecord() const
{
    ensureLayout();
    for (int index = 0; index < m_records.size(); ++index) {
        if (isRecordInteractive(index))
            return index;
    }
    return -1;
}

int Breadcrumb::lastInteractiveRecord() const
{
    ensureLayout();
    for (int index = m_records.size() - 1; index >= 0; --index) {
        if (isRecordInteractive(index))
            return index;
    }
    return -1;
}

int Breadcrumb::nextInteractiveRecord(int from, int direction) const
{
    ensureLayout();
    if (direction == 0)
        return from;
    int index = from;
    if (index < 0)
        return direction > 0 ? firstInteractiveRecord() : lastInteractiveRecord();
    while (true) {
        index += direction > 0 ? 1 : -1;
        if (index < 0 || index >= m_records.size())
            return from;
        if (isRecordInteractive(index))
            return index;
    }
}

bool Breadcrumb::isRecordInteractive(int recordIndex) const
{
    if (recordIndex < 0 || recordIndex >= m_records.size() || !isEnabled())
        return false;
    const DisplayRecord& record = m_records.at(recordIndex);
    if (record.rect.isEmpty())
        return false;
    if (record.type == RecordType::Overflow)
        return !m_hiddenItemIndexes.isEmpty();
    if (record.type != RecordType::Item || !isValidIndex(record.itemIndex))
        return false;
    return record.itemIndex < m_items.size() - 1 && m_items.at(record.itemIndex).enabled;
}

void Breadcrumb::setHoveredRecord(int recordIndex)
{
    const int normalized = isRecordInteractive(recordIndex) ? recordIndex : -1;
    if (m_hoveredRecord == normalized)
        return;
    m_hoveredRecord = normalized;
    update();
}

void Breadcrumb::resetPressedRecord()
{
    if (m_pressedRecord < 0)
        return;
    m_pressedRecord = -1;
    update();
}

void Breadcrumb::activateRecord(int recordIndex)
{
    ensureLayout();
    if (!isRecordInteractive(recordIndex))
        return;

    const DisplayRecord record = m_records.at(recordIndex);
    if (record.type == RecordType::Overflow) {
        emit overflowActivated(m_hiddenItemIndexes);
        return;
    }
    if (record.type == RecordType::Item)
        activateItem(record.itemIndex);
}

void Breadcrumb::activateItem(int itemIndex)
{
    if (!isValidIndex(itemIndex) || itemIndex >= m_items.size() - 1
        || !m_items.at(itemIndex).enabled || !isEnabled())
        return;

    const BreadcrumbItem item = m_items.at(itemIndex);
    emit itemClicked(itemIndex);
    emit itemActivated(itemIndex, item);
    if (m_autoTruncateOnItemClick)
        truncateAfter(itemIndex);
}

void Breadcrumb::truncateAfter(int itemIndex)
{
    if (!isValidIndex(itemIndex) || itemIndex >= m_items.size() - 1)
        return;
    while (m_items.size() > itemIndex + 1)
        m_items.removeLast();
    m_hoveredRecord = -1;
    m_pressedRecord = -1;
    m_focusedRecord = -1;
    invalidateLayout();
    updateAccessibleText();
    emit itemsChanged();
}

void Breadcrumb::updateAccessibleText()
{
    QStringList names;
    names.reserve(m_items.size());
    for (const BreadcrumbItem& item : m_items)
        names.append(item.accessibleName.isEmpty() ? item.text : item.accessibleName);
    const QString path = names.join(QStringLiteral(" > "));
    setAccessibleName(path.isEmpty() ? QStringLiteral("Breadcrumb") : path);
    if (names.isEmpty()) {
        setAccessibleDescription(QStringLiteral("Breadcrumb path is empty"));
    } else {
        setAccessibleDescription(QStringLiteral("Current location: %1").arg(names.last()));
    }
}

void Breadcrumb::paintItem(QPainter& painter, const DisplayRecord& record, int recordIndex)
{
    if (record.rect.isEmpty() || !isValidIndex(record.itemIndex))
        return;

    painter.save();
    painter.setFont(contentFont());
    painter.setPen(textColorForRecord(record, recordIndex));
    const QString text = m_items.at(record.itemIndex).text;
    const QFontMetrics fontMetrics(contentFont());
    const int textWidth = fontMetrics.horizontalAdvance(text);
    const QString displayedText = record.contentRect.width() >= textWidth
        ? text
        : fontMetrics.elidedText(text, Qt::ElideRight, qMax(0, record.contentRect.width()));
    painter.drawText(record.contentRect, Qt::AlignVCenter | Qt::AlignLeft, displayedText);

    painter.restore();
}

void Breadcrumb::paintIconRecord(QPainter& painter, const DisplayRecord& record, int recordIndex)
{
    if (record.rect.isEmpty())
        return;

    const bool interactive = record.type == RecordType::Overflow;
    QColor iconColor = isEnabled() ? themeColors().textSecondary : themeColors().textDisabled;
    if (interactive && (m_hoveredRecord == recordIndex || m_pressedRecord == recordIndex))
        iconColor = themeColors().textPrimary;
    const QString glyph = record.type == RecordType::Overflow ? Typography::Icons::More : Typography::Icons::ChevronRightMed;
    const Metrics currentMetrics = metrics();
    const int pixelSize = record.type == RecordType::Overflow ? currentMetrics.overflowPixelSize : currentMetrics.chevronPixelSize;

    painter.save();
    paintIconGlyph(painter, record.contentRect, glyph, iconColor, pixelSize, currentMetrics.textHeight);
    painter.restore();
}

QColor Breadcrumb::textColorForRecord(const DisplayRecord& record, int recordIndex) const
{
    if (!isEnabled() || !isValidIndex(record.itemIndex) || !m_items.at(record.itemIndex).enabled)
        return themeColors().textDisabled;
    if (m_hoveredRecord == recordIndex || m_pressedRecord == recordIndex)
        return themeColors().textSecondary;
    return themeColors().textPrimary;
}

void Breadcrumb::paintIconGlyph(QPainter& painter, const QRect& rect, const QString& glyph, const QColor& color, int pixelSize, int lineHeight)
{
    if (rect.isEmpty() || glyph.isEmpty())
        return;
    QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
    iconFont.setPixelSize(pixelSize);
    painter.save();
    painter.setFont(iconFont);
    painter.setPen(color);
    painter.drawText(rect, Qt::AlignCenter, glyph);
    Q_UNUSED(lineHeight)
    painter.restore();
}

} // namespace fluent::navigation
