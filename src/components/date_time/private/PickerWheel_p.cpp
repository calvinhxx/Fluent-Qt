#include "PickerWheel_p.h"

#include <QDateTime>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QtMath>

#include <utility>

#include "components/basicinput/Button.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "design/Typography.h"

namespace fluent::date_time::detail {

namespace {
constexpr int kEntryHeight = 32;
constexpr int kColumnNavHeight = 24;
constexpr int kColumnRowHeight = 40;
constexpr int kColumnVisibleRows = 7;
constexpr int kPopupTopInset = 8;
constexpr int kCommandBarHeight = 41;
constexpr int kDividerWidth = 1;
constexpr qreal kColumnWheelThreshold = 120.0;
constexpr int kColumnWheelClusterGapMs = 120;

void drawSelectionSegment(QPainter& painter, const QRect& rect, const QColor& fill, qreal radius,
                          bool roundLeft, bool roundRight)
{
    const QRectF bounds(rect);
    const qreal leftRadius = roundLeft ? radius : 0.0;
    const qreal rightRadius = roundRight ? radius : 0.0;
    QPainterPath path;
    path.moveTo(bounds.left() + leftRadius, bounds.top());
    path.lineTo(bounds.right() - rightRadius, bounds.top());
    if (roundRight) {
        path.quadTo(bounds.right(), bounds.top(), bounds.right(), bounds.top() + rightRadius);
    } else {
        path.lineTo(bounds.right(), bounds.top());
    }
    path.lineTo(bounds.right(), bounds.bottom() - rightRadius);
    if (roundRight) {
        path.quadTo(bounds.right(), bounds.bottom(), bounds.right() - rightRadius, bounds.bottom());
    } else {
        path.lineTo(bounds.right(), bounds.bottom());
    }
    path.lineTo(bounds.left() + leftRadius, bounds.bottom());
    if (roundLeft) {
        path.quadTo(bounds.left(), bounds.bottom(), bounds.left(), bounds.bottom() - leftRadius);
    } else {
        path.lineTo(bounds.left(), bounds.bottom());
    }
    path.lineTo(bounds.left(), bounds.top() + leftRadius);
    if (roundLeft) {
        path.quadTo(bounds.left(), bounds.top(), bounds.left() + leftRadius, bounds.top());
    } else {
        path.lineTo(bounds.left(), bounds.top());
    }
    path.closeSubpath();

    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawPath(path);
}

const QString& pickerChevronUpGlyph()
{
    return Typography::Icons::FlipViewPrevV;
}

const QString& pickerChevronDownGlyph()
{
    return Typography::Icons::FlipViewNextV;
}

qreal normalizedWheelDelta(const QWheelEvent* event)
{
    if (!event->pixelDelta().isNull())
        return static_cast<qreal>(event->pixelDelta().y());
    if (!event->angleDelta().isNull())
        return static_cast<qreal>(event->angleDelta().y());
    return 0.0;
}

int wheelStepForDelta(qreal delta)
{
    if (delta > 0.0)
        return -1;
    if (delta < 0.0)
        return 1;
    return 0;
}
} // namespace

int pickerEntryHeight(const QFont& font)
{
    return qMax(kEntryHeight, QFontMetrics(font).height() + 12);
}

int pickerRowHeight(const QFont& font)
{
    return qMax(kColumnRowHeight, QFontMetrics(font).height() + 12);
}

int pickerColumnHeight(const QFont& font)
{
    return kColumnNavHeight * 2 + pickerRowHeight(font) * kColumnVisibleRows;
}

QVector<int> distributedPickerWidths(const QVector<int>& preferredWidths, int availableWidth)
{
    QVector<int> result;
    if (preferredWidths.isEmpty() || availableWidth <= 0)
        return result;

    int totalWeight = 0;
    for (int width : preferredWidths)
        totalWeight += qMax(1, width);

    int remainingWidth = availableWidth;
    int remainingWeight = totalWeight;
    for (int i = 0; i < preferredWidths.size(); ++i) {
        const int weight = qMax(1, preferredWidths.at(i));
        const int width =
            i == preferredWidths.size() - 1
                ? remainingWidth
                : qRound(static_cast<qreal>(remainingWidth) * weight / remainingWeight);
        result.append(qMax(0, width));
        remainingWidth -= width;
        remainingWeight -= weight;
    }
    return result;
}

Qt::Alignment normalizedPickerHorizontalAlignment(Qt::Alignment alignment, Qt::Alignment fallback)
{
    const Qt::Alignment horizontal = alignment & Qt::AlignHorizontal_Mask;
    if (horizontal.testFlag(Qt::AlignHCenter))
        return Qt::AlignHCenter;
    if (horizontal.testFlag(Qt::AlignRight))
        return Qt::AlignRight;
    if (horizontal.testFlag(Qt::AlignLeft))
        return Qt::AlignLeft;
    return fallback;
}

int wrappedPickerValue(int value, int minimum, int maximum)
{
    const int span = maximum - minimum + 1;
    if (span <= 0)
        return minimum;
    int normalized = (value - minimum) % span;
    if (normalized < 0)
        normalized += span;
    return minimum + normalized;
}

PickerWheelColumn::PickerWheelColumn(int initialWidthHint, QWidget* parent)
    : QWidget(parent), m_widthHint(initialWidthHint)
{
    setAttribute(Qt::WA_Hover);
#ifdef Q_OS_MAC
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_navButtonAnimation = new QVariantAnimation(this);
    m_navButtonAnimation->setDuration(themeAnimation().fast);
    m_navButtonAnimation->setEasingCurve(themeAnimation().decelerate);
    connect(m_navButtonAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) {
                m_navButtonOpacity = value.toReal();
                refreshProperties();
                update();
            });
}

QSize PickerWheelColumn::sizeHint() const
{
    return QSize(m_widthHint, pickerColumnHeight(font()));
}

void PickerWheelColumn::setWidthHint(int width)
{
    if (m_widthHint == width) {
        refreshProperties();
        return;
    }
    m_widthHint = qMax(48, width);
    refreshProperties();
    updateGeometry();
}

bool PickerWheelColumn::pickerColumnCanShift(int direction) const
{
    return canShiftBy(direction);
}

void PickerWheelColumn::pickerColumnShift(int direction)
{
    shiftBy(direction);
}

QRect PickerWheelColumn::previousButtonRect() const
{
    return QRect(0, 0, width(), kColumnNavHeight);
}

QRect PickerWheelColumn::nextButtonRect() const
{
    return QRect(0, height() - kColumnNavHeight, width(), kColumnNavHeight);
}

QRect PickerWheelColumn::rowRect(int row) const
{
    const int rowHeight = pickerRowHeight(font());
    return QRect(0, kColumnNavHeight + row * rowHeight, width(), rowHeight);
}

PickerWheelColumn::HitInfo PickerWheelColumn::hitTest(const QPoint& pos) const
{
    if (previousButtonRect().contains(pos))
        return {HitKind::Previous, -1};
    if (nextButtonRect().contains(pos))
        return {HitKind::Next, 1};

    const int rowHeight = pickerRowHeight(font());
    const int rowAreaY = pos.y() - kColumnNavHeight;
    if (rowAreaY >= 0 && rowAreaY < rowHeight * kColumnVisibleRows) {
        const int row = rowAreaY / rowHeight;
        return {HitKind::Row, row - kColumnVisibleRows / 2};
    }
    return {};
}

void PickerWheelColumn::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto& colors = themeColorsRef();
    const auto radius = themeRadius();

    painter.fillRect(rect(), colors.bgLayer);

    const bool canPrevious = canShiftBy(-1);
    const bool canNext = canShiftBy(1);
    if (m_navButtonOpacity > 0.01) {
        auto paintNavButton = [&](const QRect& bounds, const QString& glyph, bool enabled,
                                  bool hovered) {
            painter.save();
            painter.setOpacity(m_navButtonOpacity);
            if (hovered && enabled) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(colors.subtleSecondary);
                painter.drawRoundedRect(bounds.adjusted(5, 2, -5, -2), radius.control,
                                        radius.control);
            }
            const QColor iconColor = enabled ? (hovered ? colors.textPrimary : colors.textSecondary)
                                             : colors.textDisabled;
            painter.setPen(iconColor);
            Typography::Icons::paintGlyph(painter, QRectF(bounds), glyph,
                                          Typography::IconSize::Compact, Qt::AlignCenter);
            painter.restore();
        };

        paintNavButton(previousButtonRect(), pickerChevronUpGlyph(), canPrevious,
                       m_hoverHit.kind == HitKind::Previous);
        paintNavButton(nextButtonRect(), pickerChevronDownGlyph(), canNext,
                       m_hoverHit.kind == HitKind::Next);
    }

    painter.setFont(font());
    const int centerRow = kColumnVisibleRows / 2;
    const Qt::Alignment textAlignment = columnTextAlignment();
    const bool firstVisible = isFirstVisibleColumn();
    const bool lastVisible = isLastVisibleColumn();
    for (int row = 0; row < kColumnVisibleRows; ++row) {
        const int offset = row - centerRow;
        const bool selectable = isRowSelectable(offset);
        const bool selected = offset == 0;
        const bool hovered = m_hoverHit.kind == HitKind::Row && m_hoverHit.offset == offset;
        const QRect rowBounds =
            selected ? rowRect(row).adjusted(firstVisible ? 4 : 0, 0, lastVisible ? -4 : 0, 0)
                     : rowRect(row).adjusted(4, 2, -4, -2);

        QColor highlightFill = Qt::transparent;
        if (selected)
            highlightFill = colors.accentDefault;
        else if (hovered && selectable)
            highlightFill = colors.subtleSecondary;

        if (highlightFill.isValid() && highlightFill.alpha() > 0) {
            if (selected) {
                drawSelectionSegment(painter, rowBounds, highlightFill, radius.control,
                                     firstVisible, lastVisible);
            } else {
                painter.setPen(Qt::NoPen);
                painter.setBrush(highlightFill);
                painter.drawRoundedRect(rowBounds, radius.control, radius.control);
            }
        }

        QColor textColor = isRowTextEnabled(offset) ? colors.textPrimary : colors.textDisabled;
        if (selected)
            textColor = colors.textOnAccent;
        painter.setPen(textColor);
        const QString text = displayTextForOffset(offset);
        painter.drawText(rowBounds.adjusted(8, 0, -8, 0), Qt::AlignVCenter | textAlignment,
                         painter.fontMetrics().elidedText(text, Qt::ElideRight,
                                                          qMax(0, rowBounds.width() - 16)));
    }
}

void PickerWheelColumn::enterEvent(FluentEnterEvent* event)
{
    setColumnHovered(true);
    QWidget::enterEvent(event);
}

void PickerWheelColumn::mouseMoveEvent(QMouseEvent* event)
{
    setColumnHovered(true);
    m_hoverHit = hitTest(fluentMousePos(event));
    refreshProperties();
    update();
    QWidget::mouseMoveEvent(event);
}

void PickerWheelColumn::leaveEvent(QEvent* event)
{
    setColumnHovered(false);
    m_hoverHit = {};
    refreshProperties();
    update();
    QWidget::leaveEvent(event);
}

void PickerWheelColumn::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const HitInfo hit = hitTest(fluentMousePos(event));
    if (hit.kind == HitKind::Previous || hit.kind == HitKind::Next) {
        shiftBy(hit.offset);
        event->accept();
        return;
    }
    if (hit.kind == HitKind::Row && hit.offset != 0) {
        shiftBy(hit.offset);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void PickerWheelColumn::wheelEvent(QWheelEvent* event)
{
    const qreal delta = normalizedWheelDelta(event);
    const int step = wheelStepForDelta(delta);
    if (step == 0) {
        event->accept();
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool clusterExpired =
        m_lastWheelTs != 0 && now - m_lastWheelTs > kColumnWheelClusterGapMs;
    const bool directionChanged = m_wheelDir != 0 && m_wheelDir != step;
    if (m_lastWheelTs == 0 || clusterExpired || directionChanged)
        resetWheelState();

    m_lastWheelTs = now;
    m_wheelDir = step;
    m_wheelAccum += qAbs(delta);
    if (m_wheelAccum < kColumnWheelThreshold) {
        event->accept();
        return;
    }

    m_wheelAccum = 0.0;
    shiftBy(step);
    event->accept();
}

void PickerWheelColumn::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Up:
        shiftBy(-1);
        event->accept();
        return;
    case Qt::Key_Down:
        shiftBy(1);
        event->accept();
        return;
    case Qt::Key_PageUp:
        shiftBy(-5);
        event->accept();
        return;
    case Qt::Key_PageDown:
        shiftBy(5);
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        commitPickerValue();
        event->accept();
        return;
    case Qt::Key_Escape:
        cancelPickerValue();
        event->accept();
        return;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void PickerWheelColumn::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    if (refreshPropertiesOnFocus())
        refreshProperties();
    update();
}

void PickerWheelColumn::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    if (refreshPropertiesOnFocus())
        refreshProperties();
    update();
}

void PickerWheelColumn::setColumnHovered(bool hovered)
{
    const qreal target = hovered ? 1.0 : 0.0;
    if (m_columnHovered == hovered && qFuzzyCompare(m_navButtonTargetOpacity + 1.0, target + 1.0)) {
        return;
    }

    m_columnHovered = hovered;
    m_navButtonTargetOpacity = target;
    if (!m_navButtonAnimation) {
        m_navButtonOpacity = target;
        refreshProperties();
        return;
    }

    m_navButtonAnimation->stop();
    m_navButtonAnimation->setStartValue(m_navButtonOpacity);
    m_navButtonAnimation->setEndValue(target);
    ::fluent::detail::startMotionTransition(m_navButtonAnimation, themeAnimation().fast);
    refreshProperties();
}

void PickerWheelColumn::resetWheelState()
{
    m_wheelAccum = 0.0;
    m_wheelDir = 0;
    m_lastWheelTs = 0;
}

void PickerWheelColumn::refreshProperties()
{
    const bool firstVisible = isFirstVisibleColumn();
    const bool lastVisible = isLastVisibleColumn();
    setProperty("previousButtonGlyph", pickerChevronUpGlyph());
    setProperty("nextButtonGlyph", pickerChevronDownGlyph());
    setProperty("textAlignment", static_cast<int>(columnTextAlignment()));
    const int visibleItemCount = visibleItemCountProperty();
    if (visibleItemCount > 0)
        setProperty("visibleItemCount", visibleItemCount);
    setProperty("navButtonOpacity", m_navButtonOpacity);
    setProperty("navButtonTargetOpacity", m_navButtonTargetOpacity);
    setProperty("columnHovered", m_columnHovered);
    setProperty("focusFrameVisible", false);
    setProperty("selectedRowHasBackground", true);
    setProperty("selectedRowContinuous", true);
    setProperty("selectedRowLeftInset", firstVisible ? 4 : 0);
    setProperty("selectedRowRightInset", lastVisible ? 4 : 0);
    setProperty("selectedRowHeight", pickerRowHeight(font()));
}

PickerWheelPanel::PickerWheelPanel(const QString& panelObjectName, int themeMinimumWidth,
                                   QWidget* parent)
    : QWidget(parent), m_themeMinimumWidth(themeMinimumWidth)
{
    setObjectName(panelObjectName);
    setAttribute(Qt::WA_NoSystemBackground);
}

void PickerWheelPanel::initializeActions(const QString& confirmButtonObjectName,
                                         const QString& cancelButtonObjectName,
                                         std::function<void()> commit, std::function<void()> cancel)
{
    Q_ASSERT(!m_confirmButton);
    Q_ASSERT(!m_cancelButton);

    m_confirmButton = new fluent::basicinput::Button(this);
    m_confirmButton->setObjectName(confirmButtonObjectName);
    m_confirmButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_confirmButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_confirmButton->setIconGlyph(Typography::Icons::CheckMark, Typography::IconSize::Standard);
    m_confirmButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_cancelButton = new fluent::basicinput::Button(this);
    m_cancelButton->setObjectName(cancelButtonObjectName);
    m_cancelButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_cancelButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_cancelButton->setIconGlyph(Typography::Icons::Cancel, Typography::IconSize::Standard);
    m_cancelButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    connect(m_confirmButton, &fluent::basicinput::Button::clicked, this,
            [commit = std::move(commit)] {
                if (commit)
                    commit();
            });
    connect(m_cancelButton, &fluent::basicinput::Button::clicked, this,
            [cancel = std::move(cancel)] {
                if (cancel)
                    cancel();
            });
}

QSize PickerWheelPanel::sizeHint() const
{
    int width = 0;
    const auto columns = visibleColumns();
    for (const PickerWheelColumn* column : columns)
        width += column->sizeHint().width();
    if (!columns.isEmpty())
        width += (columns.size() - 1) * kDividerWidth;
    width = qMax(m_themeMinimumWidth, width);

    const int height = kPopupTopInset + pickerColumnHeight(font()) + kCommandBarHeight;
    return QSize(width, height);
}

PickerWheelColumn* PickerWheelPanel::firstVisibleColumn() const
{
    for (PickerWheelColumn* column : m_columns) {
        if (column && !column->isHidden())
            return column;
    }
    return nullptr;
}

int PickerWheelPanel::selectedRowCenterY() const
{
    const int centerRow = kColumnVisibleRows / 2;
    const int rowHeight = pickerRowHeight(font());
    return kPopupTopInset + kColumnNavHeight + centerRow * rowHeight + rowHeight / 2;
}

void PickerWheelPanel::setColumns(const QVector<PickerWheelColumn*>& columns)
{
    m_columns = columns;
    for (PickerWheelColumn* column : m_columns) {
        if (column && column->parentWidget() != this)
            column->setParent(this);
    }
}

void PickerWheelPanel::configureColumns(const QFont& pickerFont, const QVector<bool>& visible,
                                        const QVector<int>& preferredWidths)
{
    setFont(pickerFont);
    for (int i = 0; i < m_columns.size(); ++i) {
        PickerWheelColumn* column = m_columns.at(i);
        if (!column)
            continue;
        const bool isVisible = visible.value(i, true);
        column->setFont(pickerFont);
        column->setVisible(isVisible);
        column->setEnabled(isVisible);
        column->setWidthHint(preferredWidths.value(i, column->sizeHint().width()));
    }

    setProperty("selectedRowCenterY", selectedRowCenterY());
    updateGeometry();
    layoutContent();
    updateColumns();
}

void PickerWheelPanel::setActionAccessibleNames(const QString& confirmName,
                                                const QString& cancelName)
{
    if (m_confirmButton)
        m_confirmButton->setAccessibleName(confirmName);
    if (m_cancelButton)
        m_cancelButton->setAccessibleName(cancelName);
}

void PickerWheelPanel::updateColumns()
{
    for (PickerWheelColumn* column : m_columns) {
        if (column)
            column->update();
    }
    update();
}

void PickerWheelPanel::refreshTheme()
{
    onThemeUpdated();
}

void PickerWheelPanel::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(themeColorsRef().strokeDivider);

    int x = 0;
    const auto columns = visibleColumns();
    const auto widths = columnWidths();
    for (int i = 0; i < columns.size() - 1; ++i) {
        x += widths.value(i);
        painter.drawLine(x, kPopupTopInset, x, kPopupTopInset + pickerColumnHeight(font()));
        x += kDividerWidth;
    }

    const int dividerY = kPopupTopInset + pickerColumnHeight(font());
    painter.drawLine(0, dividerY, width(), dividerY);
}

void PickerWheelPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutContent();
}

void PickerWheelPanel::onThemeUpdated()
{
    updateColumns();
    if (m_confirmButton)
        m_confirmButton->onThemeUpdated();
    if (m_cancelButton)
        m_cancelButton->onThemeUpdated();
}

QVector<PickerWheelColumn*> PickerWheelPanel::visibleColumns() const
{
    QVector<PickerWheelColumn*> result;
    for (PickerWheelColumn* column : m_columns) {
        if (column && !column->isHidden())
            result.append(column);
    }
    return result;
}

QVector<int> PickerWheelPanel::columnWidths() const
{
    QVector<int> preferredWidths;
    const auto columns = visibleColumns();
    for (const PickerWheelColumn* column : columns)
        preferredWidths.append(column->sizeHint().width());
    const int dividerWidth = qMax(0, columns.size() - 1) * kDividerWidth;
    return distributedPickerWidths(preferredWidths, qMax(0, width() - dividerWidth));
}

void PickerWheelPanel::layoutContent()
{
    if (rect().isEmpty())
        return;

    int x = 0;
    int columnIndex = 0;
    const auto widths = columnWidths();
    for (PickerWheelColumn* column : m_columns) {
        if (!column || column->isHidden())
            continue;
        const int columnWidth = widths.value(columnIndex++);
        column->setGeometry(x, kPopupTopInset, columnWidth, pickerColumnHeight(font()));
        x += columnWidth + kDividerWidth;
    }

    const int buttonY = kPopupTopInset + pickerColumnHeight(font()) + 4;
    const int buttonHeight = kCommandBarHeight - 8;
    const int halfWidth = width() / 2;
    if (m_confirmButton)
        m_confirmButton->setGeometry(4, buttonY, qMax(0, halfWidth - 6), buttonHeight);
    if (m_cancelButton) {
        m_cancelButton->setGeometry(halfWidth + 2, buttonY, qMax(0, width() - halfWidth - 6),
                                    buttonHeight);
    }
}

} // namespace fluent::date_time::detail
