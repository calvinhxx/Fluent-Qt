#include "TimePicker.h"

#include <QDateTime>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QTime>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QtMath>

#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/date_time/private/PickerAccessibility_p.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "design/Spacing.h"
#include "design/Typography.h"

namespace fluent::date_time {

namespace {
constexpr int kEntryHeight = 32;
constexpr int kTimePickerThemeMinWidth = 242;
constexpr int kSegmentHPadding = 12;
constexpr int kPopupShadowMargin = ::Spacing::Standard;
constexpr int kPopupTopInset = 8;
constexpr int kColumnNavHeight = 24;
constexpr int kColumnRowHeight = 40;
constexpr int kColumnVisibleRows = 7;
constexpr int kCommandBarHeight = 41;
constexpr int kDividerWidth = 1;
constexpr int kColumnBaseWidth = 80;
constexpr qreal kColumnWheelThreshold = 120.0;
constexpr int kColumnWheelClusterGapMs = 120;

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

QVector<int> distributedWidths(const QVector<int>& preferredWidths, int availableWidth)
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
        const int width = i == preferredWidths.size() - 1
            ? remainingWidth
            : qRound(static_cast<qreal>(remainingWidth) * weight / remainingWeight);
        result.append(qMax(0, width));
        remainingWidth -= width;
        remainingWeight -= weight;
    }
    return result;
}

void drawSelectionSegment(QPainter& painter, const QRect& rect, const QColor& fill,
                          qreal radius, bool roundLeft, bool roundRight)
{
    const QRectF bounds(rect);
    const qreal leftRadius = roundLeft ? radius : 0.0;
    const qreal rightRadius = roundRight ? radius : 0.0;
    QPainterPath path;
    path.moveTo(bounds.left() + leftRadius, bounds.top());
    path.lineTo(bounds.right() - rightRadius, bounds.top());
    if (roundRight)
        path.quadTo(bounds.right(), bounds.top(), bounds.right(), bounds.top() + rightRadius);
    else
        path.lineTo(bounds.right(), bounds.top());
    path.lineTo(bounds.right(), bounds.bottom() - rightRadius);
    if (roundRight)
        path.quadTo(bounds.right(), bounds.bottom(), bounds.right() - rightRadius, bounds.bottom());
    else
        path.lineTo(bounds.right(), bounds.bottom());
    path.lineTo(bounds.left() + leftRadius, bounds.bottom());
    if (roundLeft)
        path.quadTo(bounds.left(), bounds.bottom(), bounds.left(), bounds.bottom() - leftRadius);
    else
        path.lineTo(bounds.left(), bounds.bottom());
    path.lineTo(bounds.left(), bounds.top() + leftRadius);
    if (roundLeft)
        path.quadTo(bounds.left(), bounds.top(), bounds.left() + leftRadius, bounds.top());
    else
        path.lineTo(bounds.left(), bounds.top());
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

Qt::Alignment normalizedHorizontalAlignment(Qt::Alignment alignment, Qt::Alignment fallback)
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

int timeFieldBaseWidth(TimePicker::TimeField field)
{
    switch (field) {
    case TimePicker::TimeField::Hour:
    case TimePicker::TimeField::Minute:
    case TimePicker::TimeField::Period:
        return kColumnBaseWidth;
    }
    return kColumnBaseWidth;
}

int clampMinuteIncrement(int increment)
{
    return qBound(1, increment, 59);
}

int wrappedValue(int value, int minimum, int maximum)
{
    const int span = maximum - minimum + 1;
    if (span <= 0)
        return minimum;
    int normalized = (value - minimum) % span;
    if (normalized < 0)
        normalized += span;
    return minimum + normalized;
}

bool isPm(const QTime& time)
{
    return time.hour() >= 12;
}

int displayHour12(int hour)
{
    const int value = hour % 12;
    return value == 0 ? 12 : value;
}

int hourFromDisplay12(int displayHour, bool pm)
{
    displayHour = qBound(1, displayHour, 12);
    if (displayHour == 12)
        return pm ? 12 : 0;
    return pm ? displayHour + 12 : displayHour;
}
} // namespace

class TimePickerFlyout;
class TimePickerFlyoutPanel;

class TimePickerColumn : public QWidget,
                         public FluentElement,
                         public detail::PickerColumnAccessibilityHost {
public:
    TimePickerColumn(TimePickerFlyout* flyout, TimePicker::TimeField field, QWidget* parent = nullptr);

    TimePicker::TimeField field() const { return m_field; }
    QSize sizeHint() const override { return QSize(m_widthHint, pickerColumnHeight(font())); }
    void setWidthHint(int width);

    QWidget* pickerColumnWidget() override { return this; }
    QString pickerColumnName() const override;
    QString pickerColumnValueText() const override;
    QVariant pickerColumnCurrentValue() const override;
    QVariant pickerColumnMinimumValue() const override;
    QVariant pickerColumnMaximumValue() const override;
    QVariant pickerColumnStepSize() const override;
    bool pickerColumnCanShift(int direction) const override;
    void pickerColumnShift(int direction) override;
    void pickerColumnSetValue(const QVariant& value) override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

    void onThemeUpdated() override { update(); }

private:
    enum class HitKind {
        None,
        Previous,
        Next,
        Row
    };

    struct HitInfo {
        HitKind kind = HitKind::None;
        int offset = 0;
    };

    HitInfo hitTest(const QPoint& pos) const;
    QRect rowRect(int row) const;
    QRect previousButtonRect() const;
    QRect nextButtonRect() const;
    void setColumnHovered(bool hovered);
    void resetWheelState();
    void refreshProperties();

    TimePickerFlyout* m_flyout = nullptr;
    TimePicker::TimeField m_field;
    int m_widthHint = 82;
    HitInfo m_hoverHit;
    bool m_columnHovered = false;
    qreal m_navButtonOpacity = 0.0;
    qreal m_navButtonTargetOpacity = 0.0;
    QVariantAnimation* m_navButtonAnimation = nullptr;
    qreal m_wheelAccum = 0.0;
    int m_wheelDir = 0;
    qint64 m_lastWheelTs = 0;
};

class TimePickerFlyout : public fluent::dialogs_flyouts::Flyout {
public:
    explicit TimePickerFlyout(TimePicker* owner);

    TimePicker* owner() const { return m_owner; }
    QTime pendingTime() const { return m_pendingTime; }

    void showForPicker();
    void setPendingTime(const QTime& time);
    QTime shifted(TimePicker::TimeField field, int offset) const;
    bool canShift(TimePicker::TimeField field, int offset) const;
    Qt::Alignment textAlignment(TimePicker::TimeField field) const;
    QString displayText(TimePicker::TimeField field, const QTime& time) const;
    bool isFirstVisibleField(TimePicker::TimeField field) const;
    bool isLastVisibleField(TimePicker::TimeField field) const;
    void shiftField(TimePicker::TimeField field, int offset);
    void commit();
    void cancel();
    void refreshActionAccessibility();

    void onThemeUpdated() override;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    friend class TimePickerFlyoutPanel;

    QVector<TimePicker::TimeField> visibleFields() const;
    int preferredColumnWidth(TimePicker::TimeField field) const;
    void notifyColumnValueChanges(const QTime& before, const QTime& after);
    void updateColumns();

    TimePicker* m_owner = nullptr;
    QTime m_pendingTime;
    TimePickerFlyoutPanel* m_panel = nullptr;
};

class TimePickerFlyoutPanel : public QWidget, public FluentElement {
public:
    explicit TimePickerFlyoutPanel(TimePickerFlyout* flyout, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    TimePickerColumn* firstVisibleColumn() const;
    void refreshFromFlyout();
    void refreshTheme();
    void updateColumns();
    void refreshActionAccessibility();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void onThemeUpdated() override;

private:
    void layoutContent();
    QVector<int> columnWidths() const;

    TimePickerFlyout* m_flyout = nullptr;
    TimePickerColumn* m_hourColumn = nullptr;
    TimePickerColumn* m_minuteColumn = nullptr;
    TimePickerColumn* m_periodColumn = nullptr;
    fluent::basicinput::Button* m_confirmButton = nullptr;
    fluent::basicinput::Button* m_cancelButton = nullptr;
};

TimePickerColumn::TimePickerColumn(TimePickerFlyout* flyout, TimePicker::TimeField field, QWidget* parent)
    : QWidget(parent)
    , m_flyout(flyout)
    , m_field(field)
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
    connect(m_navButtonAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_navButtonOpacity = value.toReal();
        refreshProperties();
        update();
    });
    refreshProperties();
}

QString TimePickerColumn::pickerColumnName() const
{
    switch (m_field) {
    case TimePicker::TimeField::Hour:
        return QCoreApplication::translate("PickerAccessibility", "Hour");
    case TimePicker::TimeField::Minute:
        return QCoreApplication::translate("PickerAccessibility", "Minute");
    case TimePicker::TimeField::Period:
        return QCoreApplication::translate("PickerAccessibility", "Period");
    }
    return {};
}

QString TimePickerColumn::pickerColumnValueText() const
{
    return m_flyout
        ? m_flyout->displayText(m_field, m_flyout->pendingTime())
        : QString();
}

QVariant TimePickerColumn::pickerColumnCurrentValue() const
{
    const QTime value = m_flyout ? m_flyout->pendingTime() : QTime();
    if (!value.isValid())
        return {};
    switch (m_field) {
    case TimePicker::TimeField::Hour:
        if (m_flyout->owner()->clockIdentifier()
            == TimePicker::ClockIdentifier::TwentyFourHourClock) {
            return value.hour();
        }
        return displayHour12(value.hour());
    case TimePicker::TimeField::Minute:
        return value.minute();
    case TimePicker::TimeField::Period:
        return isPm(value) ? 1 : 0;
    }
    return {};
}

QVariant TimePickerColumn::pickerColumnMinimumValue() const
{
    if (m_field == TimePicker::TimeField::Hour && m_flyout
        && m_flyout->owner()->clockIdentifier()
            == TimePicker::ClockIdentifier::TwentyFourHourClock) {
        return 0;
    }
    if (m_field == TimePicker::TimeField::Period)
        return 0;
    return 1;
}

QVariant TimePickerColumn::pickerColumnMaximumValue() const
{
    if (m_field == TimePicker::TimeField::Period)
        return 1;
    if (m_field == TimePicker::TimeField::Hour && m_flyout
        && m_flyout->owner()->clockIdentifier()
            == TimePicker::ClockIdentifier::TwentyFourHourClock) {
        return 23;
    }
    if (m_field == TimePicker::TimeField::Minute && m_flyout) {
        const int increment = m_flyout->owner()->minuteIncrement();
        return (59 / increment) * increment;
    }
    return 12;
}

QVariant TimePickerColumn::pickerColumnStepSize() const
{
    return m_field == TimePicker::TimeField::Minute && m_flyout
        ? QVariant(m_flyout->owner()->minuteIncrement()) : QVariant(1);
}

bool TimePickerColumn::pickerColumnCanShift(int direction) const
{
    return m_flyout && m_flyout->canShift(m_field, direction);
}

void TimePickerColumn::pickerColumnShift(int direction)
{
    if (m_flyout)
        m_flyout->shiftField(m_field, direction);
}

void TimePickerColumn::pickerColumnSetValue(const QVariant& value)
{
    if (!m_flyout)
        return;
    bool ok = false;
    int requested = value.toInt(&ok);
    const int current = pickerColumnCurrentValue().toInt();
    if (ok) {
        requested = qBound(pickerColumnMinimumValue().toInt(), requested,
                           pickerColumnMaximumValue().toInt());
    }
    if (!ok || requested == current)
        return;

    int offset = requested - current;
    if (m_field == TimePicker::TimeField::Minute) {
        const int increment = m_flyout->owner()->minuteIncrement();
        requested = qBound(0,
                           qRound(static_cast<qreal>(requested) / increment)
                               * increment,
                           pickerColumnMaximumValue().toInt());
        offset = requested / increment - current / increment;
    }
    m_flyout->shiftField(m_field, offset);
}

void TimePickerColumn::setWidthHint(int width)
{
    if (m_widthHint == width) {
        refreshProperties();
        return;
    }
    m_widthHint = qMax(48, width);
    refreshProperties();
    updateGeometry();
}

QRect TimePickerColumn::previousButtonRect() const
{
    return QRect(0, 0, width(), kColumnNavHeight);
}

QRect TimePickerColumn::nextButtonRect() const
{
    return QRect(0, height() - kColumnNavHeight, width(), kColumnNavHeight);
}

QRect TimePickerColumn::rowRect(int row) const
{
    const int rowHeight = pickerRowHeight(font());
    return QRect(0, kColumnNavHeight + row * rowHeight, width(), rowHeight);
}

TimePickerColumn::HitInfo TimePickerColumn::hitTest(const QPoint& pos) const
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

void TimePickerColumn::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto& colors = themeColorsRef();
    const auto radius = themeRadius();

    painter.fillRect(rect(), colors.bgLayer);

    const bool canPrevious = m_flyout && m_flyout->canShift(m_field, -1);
    const bool canNext = m_flyout && m_flyout->canShift(m_field, 1);

    if (m_navButtonOpacity > 0.01) {
        auto paintNavButton = [&](const QRect& rect, const QString& glyph, bool enabled, bool hovered) {
            painter.save();
            painter.setOpacity(m_navButtonOpacity);
            if (hovered && enabled) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(colors.subtleSecondary);
                painter.drawRoundedRect(rect.adjusted(5, 2, -5, -2), radius.control, radius.control);
            }
            const QColor iconColor = enabled
                ? (hovered ? colors.textPrimary : colors.textSecondary)
                : colors.textDisabled;
            painter.setPen(iconColor);
            Typography::Icons::paintGlyph(
                painter, QRectF(rect), glyph, Typography::IconSize::Compact, Qt::AlignCenter);
            painter.restore();
        };

        paintNavButton(previousButtonRect(), pickerChevronUpGlyph(), canPrevious,
                       m_hoverHit.kind == HitKind::Previous);
        paintNavButton(nextButtonRect(), pickerChevronDownGlyph(), canNext,
                       m_hoverHit.kind == HitKind::Next);
    }

    painter.setFont(font());
    const int centerRow = kColumnVisibleRows / 2;
    const Qt::Alignment textAlignment = m_flyout ? m_flyout->textAlignment(m_field) : Qt::AlignLeft;
    const bool firstVisible = m_flyout && m_flyout->isFirstVisibleField(m_field);
    const bool lastVisible = m_flyout && m_flyout->isLastVisibleField(m_field);
    for (int row = 0; row < kColumnVisibleRows; ++row) {
        const int offset = row - centerRow;
        const QTime valueTime = m_flyout ? m_flyout->shifted(m_field, offset) : QTime();
        const bool selectable = m_flyout && m_flyout->canShift(m_field, offset);
        const bool selected = offset == 0;
        const bool hovered = m_hoverHit.kind == HitKind::Row && m_hoverHit.offset == offset;
        const QRect rowBounds = selected
            ? rowRect(row).adjusted(firstVisible ? 4 : 0, 0, lastVisible ? -4 : 0, 0)
            : rowRect(row).adjusted(4, 2, -4, -2);

        // Resolve the highlight fill + selected text color per language. Init highlightFill to a real
        // value (Qt::transparent) — a default-constructed QColor is INVALID yet alpha()==255, so a bare
        // alpha()>0 guard would fire and setBrush() would paint SOLID BLACK. zh_CN: 按语言解析高亮填充与选中
        // 文字色。highlightFill 必须初始化为真实值(Qt::transparent)——默认构造的 QColor 无效却 alpha()==255,
        // 裸 alpha()>0 会命中,setBrush() 会涂成纯黑。
        QColor highlightFill = Qt::transparent;
        QColor selectedText = colors.textOnAccent;
    // Fluent treatment. zh_CN: Fluent 样式。
            if (selected)
                highlightFill = colors.accentDefault;
            else if (hovered && selectable)
                highlightFill = colors.subtleSecondary;
            selectedText = colors.textOnAccent;

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

        QColor textColor = selectable || selected ? colors.textPrimary : colors.textDisabled;
        if (selected)
            textColor = selectedText;
        painter.setPen(textColor);
        const QString text = m_flyout ? m_flyout->displayText(m_field, valueTime) : QString();
        painter.drawText(rowBounds.adjusted(8, 0, -8, 0), Qt::AlignVCenter | textAlignment,
                         painter.fontMetrics().elidedText(
                             text, Qt::ElideRight, qMax(0, rowBounds.width() - 16)));
    }
}

void TimePickerColumn::enterEvent(FluentEnterEvent* event)
{
    setColumnHovered(true);
    QWidget::enterEvent(event);
}

void TimePickerColumn::mouseMoveEvent(QMouseEvent* event)
{
    setColumnHovered(true);
    m_hoverHit = hitTest(fluentMousePos(event));
    refreshProperties();
    update();
    QWidget::mouseMoveEvent(event);
}

void TimePickerColumn::leaveEvent(QEvent* event)
{
    setColumnHovered(false);
    m_hoverHit = {};
    refreshProperties();
    update();
    QWidget::leaveEvent(event);
}

void TimePickerColumn::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_flyout || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const HitInfo hit = hitTest(fluentMousePos(event));
    if (hit.kind == HitKind::Previous || hit.kind == HitKind::Next) {
        m_flyout->shiftField(m_field, hit.offset);
        event->accept();
        return;
    }
    if (hit.kind == HitKind::Row && hit.offset != 0) {
        m_flyout->shiftField(m_field, hit.offset);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void TimePickerColumn::wheelEvent(QWheelEvent* event)
{
    if (!m_flyout) {
        QWidget::wheelEvent(event);
        return;
    }

    const qreal delta = normalizedWheelDelta(event);
    const int step = wheelStepForDelta(delta);
    if (step == 0) {
        event->accept();
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool clusterExpired = m_lastWheelTs != 0 && now - m_lastWheelTs > kColumnWheelClusterGapMs;
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
    m_flyout->shiftField(m_field, step);
    event->accept();
}

void TimePickerColumn::keyPressEvent(QKeyEvent* event)
{
    if (!m_flyout) {
        QWidget::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Up:
        m_flyout->shiftField(m_field, -1);
        event->accept();
        return;
    case Qt::Key_Down:
        m_flyout->shiftField(m_field, 1);
        event->accept();
        return;
    case Qt::Key_PageUp:
        m_flyout->shiftField(m_field, -5);
        event->accept();
        return;
    case Qt::Key_PageDown:
        m_flyout->shiftField(m_field, 5);
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        m_flyout->commit();
        event->accept();
        return;
    case Qt::Key_Escape:
        m_flyout->cancel();
        event->accept();
        return;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void TimePickerColumn::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    refreshProperties();
    update();
}

void TimePickerColumn::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    refreshProperties();
    update();
}

void TimePickerColumn::setColumnHovered(bool hovered)
{
    const qreal target = hovered ? 1.0 : 0.0;
    if (m_columnHovered == hovered && qFuzzyCompare(m_navButtonTargetOpacity + 1.0, target + 1.0))
        return;

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
    m_navButtonAnimation->start();
    refreshProperties();
}

void TimePickerColumn::resetWheelState()
{
    m_wheelAccum = 0.0;
    m_wheelDir = 0;
    m_lastWheelTs = 0;
}

void TimePickerColumn::refreshProperties()
{
    const bool firstVisible = m_flyout && m_flyout->isFirstVisibleField(m_field);
    const bool lastVisible = m_flyout && m_flyout->isLastVisibleField(m_field);
    setProperty("previousButtonGlyph", pickerChevronUpGlyph());
    setProperty("nextButtonGlyph", pickerChevronDownGlyph());
    setProperty("textAlignment", static_cast<int>(m_flyout ? m_flyout->textAlignment(m_field) : Qt::AlignLeft));
    setProperty("visibleItemCount", m_field == TimePicker::TimeField::Period ? 2 : kColumnVisibleRows);
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

TimePickerFlyoutPanel::TimePickerFlyoutPanel(TimePickerFlyout* flyout, QWidget* parent)
    : QWidget(parent)
    , m_flyout(flyout)
{
    setObjectName(QStringLiteral("TimePickerFlyoutPanel"));
    setAttribute(Qt::WA_NoSystemBackground);

    m_hourColumn = new TimePickerColumn(flyout, TimePicker::TimeField::Hour, this);
    m_hourColumn->setObjectName(QStringLiteral("TimePickerHourColumn"));
    m_minuteColumn = new TimePickerColumn(flyout, TimePicker::TimeField::Minute, this);
    m_minuteColumn->setObjectName(QStringLiteral("TimePickerMinuteColumn"));
    m_periodColumn = new TimePickerColumn(flyout, TimePicker::TimeField::Period, this);
    m_periodColumn->setObjectName(QStringLiteral("TimePickerPeriodColumn"));

    m_confirmButton = new fluent::basicinput::Button(this);
    m_confirmButton->setObjectName(QStringLiteral("TimePickerConfirmButton"));
    m_confirmButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_confirmButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_confirmButton->setIconGlyph(Typography::Icons::CheckMark, Typography::IconSize::Standard);
    m_confirmButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_cancelButton = new fluent::basicinput::Button(this);
    m_cancelButton->setObjectName(QStringLiteral("TimePickerCancelButton"));
    m_cancelButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_cancelButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_cancelButton->setIconGlyph(Typography::Icons::Cancel, Typography::IconSize::Standard);
    m_cancelButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    refreshActionAccessibility();

    connect(m_confirmButton, &fluent::basicinput::Button::clicked, this, [this] {
        if (m_flyout)
            m_flyout->commit();
    });
    connect(m_cancelButton, &fluent::basicinput::Button::clicked, this, [this] {
        if (m_flyout)
            m_flyout->cancel();
    });
}

QSize TimePickerFlyoutPanel::sizeHint() const
{
    if (!m_flyout)
        return QSize();

    int width = 0;
    const auto fields = m_flyout->visibleFields();
    for (TimePicker::TimeField field : fields)
        width += m_flyout->preferredColumnWidth(field);
    if (!fields.isEmpty())
        width += (fields.size() - 1) * kDividerWidth;
    width = qMax(kTimePickerThemeMinWidth, width);

    const int height = kPopupTopInset + pickerColumnHeight(font()) + kCommandBarHeight;
    return QSize(width, height);
}

TimePickerColumn* TimePickerFlyoutPanel::firstVisibleColumn() const
{
    if (m_hourColumn && !m_hourColumn->isHidden())
        return m_hourColumn;
    if (m_minuteColumn && !m_minuteColumn->isHidden())
        return m_minuteColumn;
    if (m_periodColumn && !m_periodColumn->isHidden())
        return m_periodColumn;
    return nullptr;
}

void TimePickerFlyoutPanel::refreshFromFlyout()
{
    if (!m_flyout)
        return;

    const QFont pickerFont = m_flyout->owner() ? m_flyout->owner()->font() : font();
    setFont(pickerFont);
    const auto fields = m_flyout->visibleFields();
    auto configure = [this, &fields, &pickerFont](TimePickerColumn* column, TimePicker::TimeField field) {
        const bool visible = fields.contains(field);
        column->setFont(pickerFont);
        column->setVisible(visible);
        column->setEnabled(visible);
        column->setWidthHint(m_flyout->preferredColumnWidth(field));
    };
    configure(m_hourColumn, TimePicker::TimeField::Hour);
    configure(m_minuteColumn, TimePicker::TimeField::Minute);
    configure(m_periodColumn, TimePicker::TimeField::Period);

    updateGeometry();
    layoutContent();
    updateColumns();
}

void TimePickerFlyoutPanel::updateColumns()
{
    if (m_hourColumn)
        m_hourColumn->update();
    if (m_minuteColumn)
        m_minuteColumn->update();
    if (m_periodColumn)
        m_periodColumn->update();
    update();
}

void TimePickerFlyoutPanel::refreshActionAccessibility()
{
    TimePicker* owner = m_flyout ? m_flyout->owner() : nullptr;
    if (m_confirmButton) {
        const QString overrideName = owner
            ? owner->confirmButtonAccessibleName() : QString();
        m_confirmButton->setAccessibleName(overrideName.isEmpty()
            ? QCoreApplication::translate("PickerAccessibility", "Confirm time")
            : overrideName);
    }
    if (m_cancelButton) {
        const QString overrideName = owner
            ? owner->cancelButtonAccessibleName() : QString();
        m_cancelButton->setAccessibleName(overrideName.isEmpty()
            ? QCoreApplication::translate("PickerAccessibility", "Cancel")
            : overrideName);
    }
}

void TimePickerFlyoutPanel::refreshTheme()
{
    onThemeUpdated();
}

void TimePickerFlyoutPanel::paintEvent(QPaintEvent*)
{
    if (!m_flyout)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto& colors = themeColorsRef();

    painter.setPen(colors.strokeDivider);
    int x = 0;
    const auto fields = m_flyout->visibleFields();
    const auto widths = columnWidths();
    for (int i = 0; i < fields.size() - 1; ++i) {
        x += widths.value(i);
        painter.drawLine(x, kPopupTopInset, x,
                         kPopupTopInset + pickerColumnHeight(font()));
        x += kDividerWidth;
    }

    const int dividerY = kPopupTopInset + pickerColumnHeight(font());
    painter.drawLine(0, dividerY, width(), dividerY);
}

void TimePickerFlyoutPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutContent();
}

void TimePickerFlyoutPanel::onThemeUpdated()
{
    updateColumns();
    if (m_confirmButton)
        m_confirmButton->onThemeUpdated();
    if (m_cancelButton)
        m_cancelButton->onThemeUpdated();
}

QVector<int> TimePickerFlyoutPanel::columnWidths() const
{
    QVector<int> preferredWidths;
    if (!m_flyout)
        return preferredWidths;
    const auto fields = m_flyout->visibleFields();
    for (TimePicker::TimeField field : fields)
        preferredWidths.append(m_flyout->preferredColumnWidth(field));
    const int dividerWidth = qMax(0, fields.size() - 1) * kDividerWidth;
    return distributedWidths(preferredWidths, qMax(0, width() - dividerWidth));
}

void TimePickerFlyoutPanel::layoutContent()
{
    if (rect().isEmpty())
        return;

    int x = 0;
    int columnIndex = 0;
    const auto widths = columnWidths();

    auto placeColumn = [this, &x, &columnIndex, &widths](TimePickerColumn* column) {
        if (column->isHidden())
            return;
        const int w = widths.value(columnIndex++);
        column->setGeometry(x, kPopupTopInset, w, pickerColumnHeight(font()));
        x += w + kDividerWidth;
    };

    placeColumn(m_hourColumn);
    placeColumn(m_minuteColumn);
    placeColumn(m_periodColumn);

    const int buttonY = kPopupTopInset + pickerColumnHeight(font()) + 4;
    const int buttonHeight = kCommandBarHeight - 8;
    const int halfWidth = width() / 2;
    m_confirmButton->setGeometry(4, buttonY, qMax(0, halfWidth - 6), buttonHeight);
    m_cancelButton->setGeometry(halfWidth + 2, buttonY,
                                qMax(0, width() - halfWidth - 6), buttonHeight);
}

TimePickerFlyout::TimePickerFlyout(TimePicker* owner)
    : fluent::dialogs_flyouts::Flyout(owner)
    , m_owner(owner)
{
    setObjectName(QStringLiteral("TimePickerFlyout"));
    setAnimationEnabled(false);
    setPlacement(fluent::dialogs_flyouts::Flyout::Auto);
    setAnchorOffset(::Spacing::XSmall);
    setModal(false);
    setDim(false);
    setClosePolicy(ClosePolicy(CloseOnPressOutside | CloseOnEscape));

    m_panel = new TimePickerFlyoutPanel(this, this);
    connect(this, &TimePickerFlyout::closed, this, [this] {
        if (m_owner)
            m_owner->handleFlyoutClosed();
    });
}

QVector<TimePicker::TimeField> TimePickerFlyout::visibleFields() const
{
    return m_owner ? m_owner->visibleFields() : QVector<TimePicker::TimeField>();
}

int TimePickerFlyout::preferredColumnWidth(TimePicker::TimeField field) const
{
    if (!m_owner)
        return timeFieldBaseWidth(field);
    return m_owner->preferredFieldWidth(field);
}

bool TimePickerFlyout::isFirstVisibleField(TimePicker::TimeField field) const
{
    const auto fields = visibleFields();
    return !fields.isEmpty() && fields.first() == field;
}

bool TimePickerFlyout::isLastVisibleField(TimePicker::TimeField field) const
{
    const auto fields = visibleFields();
    return !fields.isEmpty() && fields.last() == field;
}

void TimePickerFlyout::showForPicker()
{
    if (!m_owner)
        return;

    setPendingTime(m_owner->time());
    m_panel->refreshFromFlyout();

    const QSize cardSize = m_panel->sizeHint();
    const int cardW = cardSize.width();
    const int cardH = cardSize.height();
    setFixedSize(cardW + kPopupShadowMargin * 2, cardH + kPopupShadowMargin * 2);
    m_panel->setGeometry(kPopupShadowMargin, kPopupShadowMargin, cardW, cardH);
    setAnchor(m_owner);

    if (isOpen() || isVisible()) {
        move(computePosition());
        show();
        raise();
        setFocus(Qt::PopupFocusReason);
    } else {
        showAt(m_owner);
    }

    if (auto* column = m_panel->firstVisibleColumn())
        column->setFocus(Qt::PopupFocusReason);
}

void TimePickerFlyout::setPendingTime(const QTime& time)
{
    if (!m_owner)
        return;
    const QTime normalized = m_owner->normalizeTime(time.isValid() ? time : m_owner->time());
    if (m_pendingTime == normalized)
        return;
    const QTime before = m_pendingTime;
    m_pendingTime = normalized;
    updateColumns();
    notifyColumnValueChanges(before, m_pendingTime);
}

QTime TimePickerFlyout::shifted(TimePicker::TimeField field, int offset) const
{
    if (!m_owner)
        return QTime();
    return m_owner->shiftedTime(m_pendingTime, field, offset);
}

bool TimePickerFlyout::canShift(TimePicker::TimeField field, int offset) const
{
    const QTime candidate = shifted(field, offset);
    return candidate.isValid() && candidate != m_pendingTime;
}

Qt::Alignment TimePickerFlyout::textAlignment(TimePicker::TimeField field) const
{
    return m_owner ? m_owner->fieldTextAlignment(field) : Qt::AlignLeft;
}

QString TimePickerFlyout::displayText(TimePicker::TimeField field, const QTime& time) const
{
    return m_owner ? m_owner->formatField(field, time) : QString();
}

void TimePickerFlyout::shiftField(TimePicker::TimeField field, int offset)
{
    if (!m_owner || offset == 0)
        return;
    const QTime next = shifted(field, offset);
    if (!next.isValid() || next == m_pendingTime)
        return;
    const QTime before = m_pendingTime;
    m_pendingTime = next;
    updateColumns();
    notifyColumnValueChanges(before, m_pendingTime);
}

void TimePickerFlyout::commit()
{
    if (m_owner && m_pendingTime.isValid())
        m_owner->applyPendingTime(m_pendingTime);
    close();
}

void TimePickerFlyout::cancel()
{
    close();
}

void TimePickerFlyout::refreshActionAccessibility()
{
    if (m_panel)
        m_panel->refreshActionAccessibility();
}

void TimePickerFlyout::onThemeUpdated()
{
    fluent::dialogs_flyouts::Flyout::onThemeUpdated();
    if (m_panel)
        m_panel->refreshTheme();
}

void TimePickerFlyout::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        commit();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        cancel();
        event->accept();
        return;
    }
    fluent::dialogs_flyouts::Flyout::keyPressEvent(event);
}

void TimePickerFlyout::updateColumns()
{
    if (m_panel)
        m_panel->updateColumns();
    update();
}

void TimePickerFlyout::notifyColumnValueChanges(
    const QTime& before, const QTime& after)
{
    if (!m_panel || before == after)
        return;
    auto notify = [this](const char* objectName) {
        if (QWidget* column = m_panel->findChild<QWidget*>(
                QString::fromLatin1(objectName))) {
            detail::notifyPickerColumnValueChanged(column);
        }
    };
    if (before.hour() != after.hour())
        notify("TimePickerHourColumn");
    if (before.minute() != after.minute())
        notify("TimePickerMinuteColumn");
    if (isPm(before) != isPm(after))
        notify("TimePickerPeriodColumn");
}

TimePicker::TimePicker(QWidget* parent)
    : fluent::basicinput::Button(parent)
{
    detail::ensurePickerAccessibilityFactory();
    m_observedLocale = QWidget::locale();
    m_time = normalizeTime(QTime::currentTime());

    setObjectName(QStringLiteral("TimePicker"));
    setFocusPolicy(Qt::StrongFocus);
    setFluentStyle(fluent::basicinput::Button::Standard);
    setFluentSize(fluent::basicinput::Button::StandardSize);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(this, &fluent::basicinput::Button::clicked, this, &TimePicker::openPicker);
    onThemeUpdated();
}

TimePicker::~TimePicker()
{
    if (m_flyout) {
        TimePickerFlyout* flyout = m_flyout.data();
        m_flyout = nullptr;
        flyout->setAnimationEnabled(false);
        flyout->close();
        delete flyout;
    }
}

void TimePicker::setTime(const QTime& time)
{
    if (!time.isValid())
        return;
    const QTime normalized = normalizeTime(time);
    if (m_time == normalized)
        return;
    m_time = normalized;
    if (!m_selectedTime.isValid())
        emit timeChanged(this->time());
    if (m_flyout && m_flyout->isOpen())
        m_flyout->setPendingTime(m_time);
    update();
}

void TimePicker::setSelectedTime(const QTime& time)
{
    if (!time.isValid()) {
        clearSelectedTime();
        return;
    }

    const QTime normalized = normalizeTime(time);
    const QTime oldTime = this->time();
    const QTime oldSelected = m_selectedTime;

    m_time = normalized;
    m_selectedTime = normalized;

    if (oldSelected != m_selectedTime)
        detail::notifyPickerRootValueChanged(this);

    if (oldSelected != m_selectedTime)
        emit selectedTimeChanged(m_selectedTime);
    if (oldTime != this->time())
        emit timeChanged(this->time());
    update();
}

void TimePicker::clearSelectedTime()
{
    if (!m_selectedTime.isValid())
        return;
    m_selectedTime = QTime();
    detail::notifyPickerRootValueChanged(this);
    emit selectedTimeChanged(m_selectedTime);
    update();
}

void TimePicker::setMinuteIncrement(int increment)
{
    const int normalizedIncrement = clampMinuteIncrement(increment);
    if (m_minuteIncrement == normalizedIncrement)
        return;

    const QTime oldTime = time();
    const QTime oldSelected = m_selectedTime;
    m_minuteIncrement = normalizedIncrement;
    m_time = normalizeTime(m_time);
    if (m_selectedTime.isValid())
        m_selectedTime = normalizeTime(m_selectedTime);

    if (oldSelected != m_selectedTime)
        detail::notifyPickerRootValueChanged(this);
    emit minuteIncrementChanged(m_minuteIncrement);
    if (oldSelected != m_selectedTime)
        emit selectedTimeChanged(m_selectedTime);
    if (oldTime != time())
        emit timeChanged(time());

    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
}

void TimePicker::setClockIdentifier(ClockIdentifier identifier)
{
    if (m_clockIdentifier == identifier)
        return;
    m_clockIdentifier = identifier;
    if (m_selectedTime.isValid())
        detail::notifyPickerRootValueChanged(this);
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    updateGeometry();
    update();
    emit clockIdentifierChanged(m_clockIdentifier);
}

void TimePicker::setLocale(const QLocale& locale)
{
    if (QWidget::locale() == locale)
        return;
    QWidget::setLocale(locale);
}

void TimePicker::setPlaceholderText(TimeField field, const QString& text)
{
    QString* target = nullptr;
    switch (field) {
    case TimeField::Hour:
        target = &m_hourPlaceholderText;
        break;
    case TimeField::Minute:
        target = &m_minutePlaceholderText;
        break;
    case TimeField::Period:
        target = &m_periodPlaceholderText;
        break;
    }
    if (!target || *target == text)
        return;

    *target = text;
    updateGeometry();
    update();
    emit placeholderTextChanged(field, text);
}

void TimePicker::setConfirmButtonAccessibleName(const QString& name)
{
    if (m_confirmButtonAccessibleName == name)
        return;
    m_confirmButtonAccessibleName = name;
    if (m_flyout)
        m_flyout->refreshActionAccessibility();
    emit confirmButtonAccessibleNameChanged(m_confirmButtonAccessibleName);
}

void TimePicker::setCancelButtonAccessibleName(const QString& name)
{
    if (m_cancelButtonAccessibleName == name)
        return;
    m_cancelButtonAccessibleName = name;
    if (m_flyout)
        m_flyout->refreshActionAccessibility();
    emit cancelButtonAccessibleNameChanged(m_cancelButtonAccessibleName);
}

void TimePicker::openPicker()
{
    if (!isEnabled())
        return;

    QPointer<TimePicker> guard(this);
    if (!m_flyout) {
        setDropDownOpen(false);
        if (!guard)
            return;
        m_flyout = new TimePickerFlyout(this);
    }

    m_flyout->showForPicker();
    if (!guard)
        return;
    guard->setDropDownOpen(true);
    if (guard)
        guard->update();
}

void TimePicker::closePicker()
{
    if (m_flyout)
        m_flyout->cancel();
}

QString TimePicker::fieldDisplayText(TimeField field) const
{
    if (!isFieldVisible(field))
        return QString();
    if (!m_selectedTime.isValid())
        return placeholderText(field);
    return formatField(field, m_selectedTime);
}

QString TimePicker::placeholderText(TimeField field) const
{
    switch (field) {
    case TimeField::Hour:
        return m_hourPlaceholderText;
    case TimeField::Minute:
        return m_minutePlaceholderText;
    case TimeField::Period:
        return m_periodPlaceholderText;
    }
    return QString();
}

int TimePicker::preferredFieldWidth(TimeField field) const
{
    const QFontMetrics metrics(font());
    int textWidth = metrics.horizontalAdvance(placeholderText(field));
    auto includeTime = [this, field, &metrics, &textWidth](const QTime& time) {
        textWidth = qMax(textWidth, metrics.horizontalAdvance(formatField(field, time)));
    };

    switch (field) {
    case TimeField::Hour:
        for (int hour = 0; hour < 24; ++hour)
            includeTime(QTime(hour, 0));
        break;
    case TimeField::Minute:
        for (int minute : minuteValues())
            includeTime(QTime(0, minute));
        break;
    case TimeField::Period:
        includeTime(QTime(0, 0));
        includeTime(QTime(13, 0));
        break;
    }

    return qMax(timeFieldBaseWidth(field), textWidth + kSegmentHPadding * 2);
}

Qt::Alignment TimePicker::fieldTextAlignment(TimeField field) const
{
    switch (field) {
    case TimeField::Hour:
        return m_hourTextAlignment;
    case TimeField::Minute:
        return m_minuteTextAlignment;
    case TimeField::Period:
        return m_periodTextAlignment;
    }
    return Qt::AlignLeft;
}

void TimePicker::setFieldTextAlignment(TimeField field, Qt::Alignment alignment)
{
    Qt::Alignment* target = nullptr;
    switch (field) {
    case TimeField::Hour:
        target = &m_hourTextAlignment;
        break;
    case TimeField::Minute:
        target = &m_minuteTextAlignment;
        break;
    case TimeField::Period:
        target = &m_periodTextAlignment;
        break;
    }
    if (!target)
        return;

    const Qt::Alignment normalized = normalizedHorizontalAlignment(alignment, *target);
    if (*target == normalized)
        return;

    *target = normalized;
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
}

QSize TimePicker::sizeHint() const
{
    int width = 0;
    for (TimeField field : visibleFields())
        width += preferredFieldWidth(field);
    width += qMax(0, visibleFields().size() - 1) * kDividerWidth;
    width = qMax(width, kTimePickerThemeMinWidth);

    return QSize(width, pickerEntryHeight(font()));
}

QSize TimePicker::minimumSizeHint() const
{
    return QSize(kTimePickerThemeMinWidth, pickerEntryHeight(font()));
}

void TimePicker::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto& colors = themeColorsRef();
    const auto radius = themeRadius();
    const QRect surface = fieldSurfaceRect();
    if (surface.isEmpty())
        return;

    const bool active = isEnabled() && (underMouse() || isDown());
    const QRectF r(surface);

    // Fluent treatment. zh_CN: Fluent 样式。
        QColor bg = colors.controlDefault;
        if (!isEnabled())
            bg = colors.controlDisabled;
        else if (isDown())
            bg = colors.subtleTertiary;
        else if (underMouse())
            bg = colors.subtleSecondary;

        painter.setPen(Qt::NoPen);
        painter.setBrush(bg);
        painter.drawRoundedRect(QRectF(surface), radius.control, radius.control);

    const auto segments = fieldSegments();
    painter.setFont(font());
    for (int i = 0; i < segments.size(); ++i) {
        const auto& segment = segments.at(i);

        if (i > 0) {
            painter.setPen(colors.strokeDivider);
            painter.drawLine(segment.rect.left(), surface.top() + 1,
                             segment.rect.left(), surface.bottom() - 1);
        }

        QColor textColor = isEnabled()
            ? (active ? colors.textPrimary : colors.textSecondary)
            : colors.textDisabled;
        painter.setPen(textColor);
        const QString text = fieldDisplayText(segment.field);
        QRect textRect = segment.rect.adjusted(kSegmentHPadding, 0, -kSegmentHPadding, 0);
        painter.drawText(textRect, Qt::AlignVCenter | fieldTextAlignment(segment.field),
                         painter.fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));
    }
}

void TimePicker::resizeEvent(QResizeEvent* event)
{
    QPushButton::resizeEvent(event);
}

void TimePicker::keyPressEvent(QKeyEvent* event)
{
    if (!m_dropDownOpen && isEnabled() &&
        (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return
         || event->key() == Qt::Key_Enter || event->key() == Qt::Key_F4
         || (event->key() == Qt::Key_Down
             && event->modifiers().testFlag(Qt::AltModifier)))) {
        openPicker();
        event->accept();
        return;
    }
    fluent::basicinput::Button::keyPressEvent(event);
}

void TimePicker::changeEvent(QEvent* event)
{
    fluent::basicinput::Button::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        updateGeometry();
        if (m_flyout && m_flyout->isOpen())
            m_flyout->showForPicker();
        update();
    }
    if (event->type() == QEvent::LocaleChange
        && m_observedLocale != QWidget::locale()) {
        m_observedLocale = QWidget::locale();
        if (m_flyout && m_flyout->isOpen())
            m_flyout->showForPicker();
        updateGeometry();
        update();
        if (m_selectedTime.isValid())
            detail::notifyPickerRootValueChanged(this);
        emit localeChanged(m_observedLocale);
    }
    if (event->type() == QEvent::EnabledChange) {
        if (!isEnabled())
            closePicker();
        update();
    }
}

void TimePicker::onThemeUpdated()
{
    if (m_flyout)
        m_flyout->onThemeUpdated();
    update();
}

QVector<TimePicker::TimeField> TimePicker::visibleFields() const
{
    QVector<TimeField> fields;
    fields.append(TimeField::Hour);
    fields.append(TimeField::Minute);
    if (m_clockIdentifier == ClockIdentifier::TwelveHourClock)
        fields.append(TimeField::Period);
    return fields;
}

bool TimePicker::isFieldVisible(TimeField field) const
{
    return field != TimeField::Period || m_clockIdentifier == ClockIdentifier::TwelveHourClock;
}

QVector<TimePicker::FieldSegment> TimePicker::fieldSegments() const
{
    const QVector<TimeField> fields = visibleFields();
    QVector<FieldSegment> result;
    const QRect surface = fieldSurfaceRect();
    if (surface.isEmpty() || fields.isEmpty())
        return result;

    int totalWeight = 0;
    for (TimeField field : fields)
        totalWeight += preferredFieldWidth(field);

    int x = surface.left();
    int remainingW = surface.width();
    for (int i = 0; i < fields.size(); ++i) {
        const int weight = preferredFieldWidth(fields.at(i));
        int w = i == fields.size() - 1 ? remainingW
            : qMax(32, qRound(double(surface.width()) * double(weight) / double(totalWeight)));
        w = qMin(w, remainingW);
        result.append({fields.at(i), QRect(x, surface.top(), w, surface.height())});
        x += w;
        remainingW -= w;
    }
    return result;
}

QRect TimePicker::fieldSurfaceRect() const
{
    return QRect(0, 0, width(), qMin(height(), pickerEntryHeight(font())));
}

QString TimePicker::formatField(TimeField field, const QTime& time) const
{
    if (!time.isValid())
        return QString();

    switch (field) {
    case TimeField::Hour:
        if (m_clockIdentifier == ClockIdentifier::TwentyFourHourClock)
            return QStringLiteral("%1").arg(time.hour(), 2, 10, QLatin1Char('0'));
        return QString::number(displayHour12(time.hour()));
    case TimeField::Minute:
        return QStringLiteral("%1").arg(time.minute(), 2, 10, QLatin1Char('0'));
    case TimeField::Period:
        return isPm(time) ? locale().pmText() : locale().amText();
    }
    return QString();
}

QTime TimePicker::normalizeTime(const QTime& time) const
{
    const QTime base = time.isValid() ? time : QTime::currentTime();
    return QTime(base.hour(), snappedMinute(base.minute()));
}

QTime TimePicker::shiftedTime(const QTime& time, TimeField field, int offset) const
{
    const QTime base = normalizeTime(time.isValid() ? time : this->time());
    if (offset == 0)
        return base;

    switch (field) {
    case TimeField::Hour: {
        if (m_clockIdentifier == ClockIdentifier::TwentyFourHourClock)
            return QTime(wrappedValue(base.hour() + offset, 0, 23), base.minute());
        const int nextDisplayHour = wrappedValue(displayHour12(base.hour()) + offset, 1, 12);
        return QTime(hourFromDisplay12(nextDisplayHour, isPm(base)), base.minute());
    }
    case TimeField::Minute: {
        const QVector<int> values = minuteValues();
        const int current = snappedMinute(base.minute());
        int index = values.indexOf(current);
        if (index < 0)
            index = 0;
        const int nextIndex = wrappedValue(index + offset, 0, values.size() - 1);
        return QTime(base.hour(), values.at(nextIndex));
    }
    case TimeField::Period:
        if (!isPm(base) && offset == 1)
            return QTime(base.hour() + 12, base.minute());
        if (isPm(base) && offset == -1)
            return QTime(base.hour() - 12, base.minute());
        return QTime();
    }
    return base;
}

QVector<int> TimePicker::minuteValues() const
{
    QVector<int> values;
    const int increment = clampMinuteIncrement(m_minuteIncrement);
    for (int minute = 0; minute < 60; minute += increment)
        values.append(minute);
    if (values.isEmpty())
        values.append(0);
    return values;
}

int TimePicker::snappedMinute(int minute) const
{
    minute = qBound(0, minute, 59);
    const QVector<int> values = minuteValues();
    int best = values.first();
    int bestDistance = qAbs(minute - best);
    for (int value : values) {
        const int distance = qAbs(minute - value);
        if (distance < bestDistance || (distance == bestDistance && value > best)) {
            best = value;
            bestDistance = distance;
        }
    }
    return best;
}

void TimePicker::setDropDownOpen(bool open)
{
    if (m_dropDownOpen == open)
        return;
    m_dropDownOpen = open;
    detail::notifyPickerRootPopupChanged(this);
    QPointer<TimePicker> guard(this);
    emit dropDownOpenChanged(m_dropDownOpen);
    if (guard)
        guard->update();
}

void TimePicker::applyPendingTime(const QTime& time)
{
    setSelectedTime(time);
}

void TimePicker::handleFlyoutClosed()
{
    setDropDownOpen(false);
}

} // namespace fluent::date_time
