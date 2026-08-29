#include "DatePicker.h"

#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QtMath>

#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/date_time/private/PickerAccessibility_p.h"
#include "components/date_time/private/PickerFlyoutGeometry_p.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "design/Spacing.h"
#include "design/Typography.h"

namespace fluent::date_time {

namespace {
constexpr int kEntryHeight = 32;
constexpr int kDatePickerThemeMinWidth = 296;
constexpr int kSegmentHPadding = 12;
constexpr int kPopupShadowMargin = ::Spacing::Standard;
constexpr int kPopupTopInset = 8;
constexpr int kColumnNavHeight = 24;
constexpr int kColumnRowHeight = 40;
constexpr int kColumnVisibleRows = 7;
constexpr int kCommandBarHeight = 41;
constexpr int kDividerWidth = 1;
constexpr int kMonthColumnBaseWidth = 134;
constexpr int kDayColumnBaseWidth = 80;
constexpr int kYearColumnBaseWidth = 80;
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

int dateFieldBaseWidth(DatePicker::DateField field)
{
    switch (field) {
    case DatePicker::DateField::Month:
        return kMonthColumnBaseWidth;
    case DatePicker::DateField::Day:
        return kDayColumnBaseWidth;
    case DatePicker::DateField::Year:
        return kYearColumnBaseWidth;
    }
    return kDayColumnBaseWidth;
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

QDate dateWithClampedDay(int year, int month, int day)
{
    year = qBound(1, year, 9999);
    month = qBound(1, month, 12);

    const QDate first(year, month, 1);
    const int clampedDay = qBound(1, day, first.daysInMonth());
    return QDate(year, month, clampedDay);
}
} // namespace

class DatePickerFlyout;
class DatePickerFlyoutPanel;

class PickerColumn : public QWidget,
                     public FluentElement,
                     public detail::PickerColumnAccessibilityHost {
public:
    PickerColumn(DatePickerFlyout* flyout, DatePicker::DateField field, QWidget* parent = nullptr);

    DatePicker::DateField field() const { return m_field; }
    QSize sizeHint() const override { return QSize(m_widthHint, pickerColumnHeight(font())); }
    void setWidthHint(int width);

    QWidget* pickerColumnWidget() override { return this; }
    QString pickerColumnName() const override;
    QString pickerColumnValueText() const override;
    QVariant pickerColumnCurrentValue() const override;
    QVariant pickerColumnMinimumValue() const override;
    QVariant pickerColumnMaximumValue() const override;
    QVariant pickerColumnStepSize() const override { return 1; }
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

    DatePickerFlyout* m_flyout = nullptr;
    DatePicker::DateField m_field;
    int m_widthHint = 100;
    HitInfo m_hoverHit;
    bool m_columnHovered = false;
    qreal m_navButtonOpacity = 0.0;
    qreal m_navButtonTargetOpacity = 0.0;
    QVariantAnimation* m_navButtonAnimation = nullptr;
    qreal m_wheelAccum = 0.0;
    int m_wheelDir = 0;
    qint64 m_lastWheelTs = 0;
};

class DatePickerFlyout : public fluent::dialogs_flyouts::Flyout {
public:
    explicit DatePickerFlyout(DatePicker* owner);

    DatePicker* owner() const { return m_owner; }
    QDate pendingDate() const { return m_pendingDate; }

    void showForPicker();
    void refreshLayout();
    void setPendingDate(const QDate& date);
    QDate shifted(DatePicker::DateField field, int offset) const;
    bool canShift(DatePicker::DateField field, int offset) const;
    bool isDateSelectable(const QDate& date) const;
    Qt::Alignment textAlignment(DatePicker::DateField field) const;
    QString displayText(DatePicker::DateField field, const QDate& date) const;
    bool isFirstVisibleField(DatePicker::DateField field) const;
    bool isLastVisibleField(DatePicker::DateField field) const;
    void shiftField(DatePicker::DateField field, int offset);
    void commit();
    void cancel();
    void refreshActionAccessibility();

    void onThemeUpdated() override;

protected:
    QPoint computePosition() const override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    friend class DatePickerFlyoutPanel;

    QVector<DatePicker::DateField> visibleFields() const;
    int preferredColumnWidth(DatePicker::DateField field) const;
    void notifyColumnValueChanges(const QDate& before, const QDate& after);
    void updateColumns();

    DatePicker* m_owner = nullptr;
    QDate m_pendingDate;
    DatePickerFlyoutPanel* m_panel = nullptr;
};

class DatePickerFlyoutPanel : public QWidget, public FluentElement {
public:
    explicit DatePickerFlyoutPanel(DatePickerFlyout* flyout, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    PickerColumn* firstVisibleColumn() const;
    int selectedRowCenterY() const;
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

    DatePickerFlyout* m_flyout = nullptr;
    PickerColumn* m_monthColumn = nullptr;
    PickerColumn* m_dayColumn = nullptr;
    PickerColumn* m_yearColumn = nullptr;
    fluent::basicinput::Button* m_confirmButton = nullptr;
    fluent::basicinput::Button* m_cancelButton = nullptr;
};

PickerColumn::PickerColumn(DatePickerFlyout* flyout, DatePicker::DateField field, QWidget* parent)
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

QString PickerColumn::pickerColumnName() const
{
    switch (m_field) {
    case DatePicker::DateField::Month:
        return QCoreApplication::translate("PickerAccessibility", "Month");
    case DatePicker::DateField::Day:
        return QCoreApplication::translate("PickerAccessibility", "Day");
    case DatePicker::DateField::Year:
        return QCoreApplication::translate("PickerAccessibility", "Year");
    }
    return {};
}

QString PickerColumn::pickerColumnValueText() const
{
    return m_flyout
        ? m_flyout->displayText(m_field, m_flyout->pendingDate())
        : QString();
}

QVariant PickerColumn::pickerColumnCurrentValue() const
{
    const QDate value = m_flyout ? m_flyout->pendingDate() : QDate();
    if (!value.isValid())
        return {};
    switch (m_field) {
    case DatePicker::DateField::Month:
        return value.month();
    case DatePicker::DateField::Day:
        return value.day();
    case DatePicker::DateField::Year:
        return value.year();
    }
    return {};
}

QVariant PickerColumn::pickerColumnMinimumValue() const
{
    if (!m_flyout || !m_flyout->owner())
        return {};
    const QDate pending = m_flyout->pendingDate();
    const QDate minimum = m_flyout->owner()->minimumDate();
    if (m_field == DatePicker::DateField::Year)
        return minimum.year();
    if (m_field == DatePicker::DateField::Month
        && pending.year() == minimum.year()) {
        return minimum.month();
    }
    if (m_field == DatePicker::DateField::Day
        && pending.year() == minimum.year()
        && pending.month() == minimum.month()) {
        return minimum.day();
    }
    return 1;
}

QVariant PickerColumn::pickerColumnMaximumValue() const
{
    if (!m_flyout || !m_flyout->owner())
        return {};
    const QDate pending = m_flyout->pendingDate();
    const QDate maximum = m_flyout->owner()->maximumDate();
    if (m_field == DatePicker::DateField::Year)
        return maximum.year();
    if (m_field == DatePicker::DateField::Month)
        return pending.year() == maximum.year() ? maximum.month() : 12;
    if (pending.year() == maximum.year()
        && pending.month() == maximum.month()) {
        return maximum.day();
    }
    return pending.isValid()
        ? QVariant(pending.daysInMonth()) : QVariant();
}

bool PickerColumn::pickerColumnCanShift(int direction) const
{
    return m_flyout && m_flyout->canShift(m_field, direction);
}

void PickerColumn::pickerColumnShift(int direction)
{
    if (m_flyout)
        m_flyout->shiftField(m_field, direction);
}

void PickerColumn::pickerColumnSetValue(const QVariant& value)
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
    if (ok && requested != current)
        m_flyout->shiftField(m_field, requested - current);
}

void PickerColumn::setWidthHint(int width)
{
    if (m_widthHint == width) {
        refreshProperties();
        return;
    }
    m_widthHint = qMax(48, width);
    refreshProperties();
    updateGeometry();
}

QRect PickerColumn::previousButtonRect() const
{
    return QRect(0, 0, width(), kColumnNavHeight);
}

QRect PickerColumn::nextButtonRect() const
{
    return QRect(0, height() - kColumnNavHeight, width(), kColumnNavHeight);
}

QRect PickerColumn::rowRect(int row) const
{
    const int rowHeight = pickerRowHeight(font());
    return QRect(0, kColumnNavHeight + row * rowHeight, width(), rowHeight);
}

PickerColumn::HitInfo PickerColumn::hitTest(const QPoint& pos) const
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

void PickerColumn::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto& colors = themeColorsRef();
    const auto radius = themeRadius();

    // macOS child widgets lack per-pixel alpha compositing; fill the background
    // explicitly so column content never stacks.
    // zh_CN: macOS 子控件不支持逐像素 alpha 合成，显式填充背景防止列内容叠加。
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
        const QDate valueDate = m_flyout ? m_flyout->shifted(m_field, offset) : QDate();
        const bool selectable = m_flyout && m_flyout->isDateSelectable(valueDate);
        const bool selected = offset == 0;
        const bool hovered = m_hoverHit.kind == HitKind::Row && m_hoverHit.offset == offset;
        const QRect rowBounds = selected
            ? rowRect(row).adjusted(firstVisible ? 4 : 0, 0, lastVisible ? -4 : 0, 0)
            : rowRect(row).adjusted(4, 2, -4, -2);

        // Per-language highlight + the text color that pairs with it. zh_CN: 各设计语言的高亮 + 与之搭配的文字色。
        QColor highlightFill = Qt::transparent; // guard against the invalid-QColor trap below.
        QColor selectedTextColor = colors.textOnAccent;
    // Fluent treatment. zh_CN: Fluent 样式。
            if (selected) {
                highlightFill = colors.accentDefault;
                selectedTextColor = colors.textOnAccent;
            } else if (hovered && selectable) {
                highlightFill = colors.subtleSecondary;
        }

        // Guard the optional fill: a default-constructed QColor is INVALID yet alpha()==255, so
        // setBrush(invalid) paints SOLID BLACK. zh_CN: 守卫可选填充:默认构造 QColor 无效却 alpha==255,
        // setBrush(无效色) 会涂成纯黑。
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

        QColor textColor = selectable ? colors.textPrimary : colors.textDisabled;
        if (selected)
            textColor = selectedTextColor;
        painter.setPen(textColor);

        const QString text = m_flyout ? m_flyout->displayText(m_field, valueDate) : QString();
        painter.drawText(rowBounds.adjusted(8, 0, -8, 0), Qt::AlignVCenter | textAlignment,
                         painter.fontMetrics().elidedText(
                             text, Qt::ElideRight, qMax(0, rowBounds.width() - 16)));
    }

}

void PickerColumn::enterEvent(FluentEnterEvent* event)
{
    setColumnHovered(true);
    QWidget::enterEvent(event);
}

void PickerColumn::mouseMoveEvent(QMouseEvent* event)
{
    setColumnHovered(true);
    m_hoverHit = hitTest(fluentMousePos(event));
    refreshProperties();
    update();
    QWidget::mouseMoveEvent(event);
}

void PickerColumn::leaveEvent(QEvent* event)
{
    setColumnHovered(false);
    m_hoverHit = {};
    refreshProperties();
    update();
    QWidget::leaveEvent(event);
}

void PickerColumn::mouseReleaseEvent(QMouseEvent* event)
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

void PickerColumn::wheelEvent(QWheelEvent* event)
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

void PickerColumn::keyPressEvent(QKeyEvent* event)
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

void PickerColumn::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    update();
}

void PickerColumn::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    update();
}

void PickerColumn::setColumnHovered(bool hovered)
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

void PickerColumn::resetWheelState()
{
    m_wheelAccum = 0.0;
    m_wheelDir = 0;
    m_lastWheelTs = 0;
}

void PickerColumn::refreshProperties()
{
    const bool firstVisible = m_flyout && m_flyout->isFirstVisibleField(m_field);
    const bool lastVisible = m_flyout && m_flyout->isLastVisibleField(m_field);
    setProperty("previousButtonGlyph", pickerChevronUpGlyph());
    setProperty("nextButtonGlyph", pickerChevronDownGlyph());
    setProperty("textAlignment", static_cast<int>(m_flyout ? m_flyout->textAlignment(m_field) : Qt::AlignLeft));
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

DatePickerFlyoutPanel::DatePickerFlyoutPanel(DatePickerFlyout* flyout, QWidget* parent)
    : QWidget(parent)
    , m_flyout(flyout)
{
    setObjectName(QStringLiteral("DatePickerFlyoutPanel"));
    setAttribute(Qt::WA_NoSystemBackground);

    m_monthColumn = new PickerColumn(flyout, DatePicker::DateField::Month, this);
    m_monthColumn->setObjectName(QStringLiteral("DatePickerMonthColumn"));
    m_dayColumn = new PickerColumn(flyout, DatePicker::DateField::Day, this);
    m_dayColumn->setObjectName(QStringLiteral("DatePickerDayColumn"));
    m_yearColumn = new PickerColumn(flyout, DatePicker::DateField::Year, this);
    m_yearColumn->setObjectName(QStringLiteral("DatePickerYearColumn"));

    m_confirmButton = new fluent::basicinput::Button(this);
    m_confirmButton->setObjectName(QStringLiteral("DatePickerConfirmButton"));
    m_confirmButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_confirmButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_confirmButton->setIconGlyph(Typography::Icons::CheckMark, Typography::IconSize::Standard);
    m_confirmButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_cancelButton = new fluent::basicinput::Button(this);
    m_cancelButton->setObjectName(QStringLiteral("DatePickerCancelButton"));
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

QSize DatePickerFlyoutPanel::sizeHint() const
{
    if (!m_flyout)
        return QSize();

    int width = 0;
    const auto fields = m_flyout->visibleFields();
    for (DatePicker::DateField field : fields)
        width += m_flyout->preferredColumnWidth(field);
    if (!fields.isEmpty())
        width += (fields.size() - 1) * kDividerWidth;
    width = qMax(kDatePickerThemeMinWidth, width);

    const int height = kPopupTopInset + pickerColumnHeight(font()) + kCommandBarHeight;
    return QSize(width, height);
}

PickerColumn* DatePickerFlyoutPanel::firstVisibleColumn() const
{
    if (m_monthColumn && !m_monthColumn->isHidden())
        return m_monthColumn;
    if (m_dayColumn && !m_dayColumn->isHidden())
        return m_dayColumn;
    if (m_yearColumn && !m_yearColumn->isHidden())
        return m_yearColumn;
    return nullptr;
}

int DatePickerFlyoutPanel::selectedRowCenterY() const
{
    const int centerRow = kColumnVisibleRows / 2;
    const int rowHeight = pickerRowHeight(font());
    return kPopupTopInset + kColumnNavHeight + centerRow * rowHeight + rowHeight / 2;
}

void DatePickerFlyoutPanel::refreshFromFlyout()
{
    if (!m_flyout)
        return;

    const QFont pickerFont = m_flyout->owner() ? m_flyout->owner()->font() : font();
    setFont(pickerFont);
    const auto fields = m_flyout->visibleFields();
    auto configure = [this, &fields, &pickerFont](PickerColumn* column, DatePicker::DateField field) {
        const bool visible = fields.contains(field);
        column->setFont(pickerFont);
        column->setVisible(visible);
        column->setEnabled(visible);
        column->setWidthHint(m_flyout->preferredColumnWidth(field));
    };
    configure(m_monthColumn, DatePicker::DateField::Month);
    configure(m_dayColumn, DatePicker::DateField::Day);
    configure(m_yearColumn, DatePicker::DateField::Year);

    setProperty("selectedRowCenterY", selectedRowCenterY());
    updateGeometry();
    layoutContent();
    updateColumns();
}

void DatePickerFlyoutPanel::updateColumns()
{
    if (m_monthColumn)
        m_monthColumn->update();
    if (m_dayColumn)
        m_dayColumn->update();
    if (m_yearColumn)
        m_yearColumn->update();
    update();
}

void DatePickerFlyoutPanel::refreshActionAccessibility()
{
    DatePicker* owner = m_flyout ? m_flyout->owner() : nullptr;
    if (m_confirmButton) {
        const QString overrideName = owner
            ? owner->confirmButtonAccessibleName() : QString();
        m_confirmButton->setAccessibleName(overrideName.isEmpty()
            ? QCoreApplication::translate("PickerAccessibility", "Confirm date")
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

void DatePickerFlyoutPanel::refreshTheme()
{
    onThemeUpdated();
}

void DatePickerFlyoutPanel::paintEvent(QPaintEvent*)
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

void DatePickerFlyoutPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutContent();
}

void DatePickerFlyoutPanel::onThemeUpdated()
{
    updateColumns();
    if (m_confirmButton)
        m_confirmButton->onThemeUpdated();
    if (m_cancelButton)
        m_cancelButton->onThemeUpdated();
}

QVector<int> DatePickerFlyoutPanel::columnWidths() const
{
    QVector<int> preferredWidths;
    if (!m_flyout)
        return preferredWidths;
    const auto fields = m_flyout->visibleFields();
    for (DatePicker::DateField field : fields)
        preferredWidths.append(m_flyout->preferredColumnWidth(field));
    const int dividerWidth = qMax(0, fields.size() - 1) * kDividerWidth;
    return distributedWidths(preferredWidths, qMax(0, width() - dividerWidth));
}

void DatePickerFlyoutPanel::layoutContent()
{
    if (rect().isEmpty())
        return;

    int x = 0;
    int columnIndex = 0;
    const auto widths = columnWidths();

    auto placeColumn = [this, &x, &columnIndex, &widths](PickerColumn* column) {
        if (column->isHidden())
            return;
        const int w = widths.value(columnIndex++);
        column->setGeometry(x, kPopupTopInset, w, pickerColumnHeight(font()));
        x += w + kDividerWidth;
    };

    placeColumn(m_monthColumn);
    placeColumn(m_dayColumn);
    placeColumn(m_yearColumn);

    const int buttonY = kPopupTopInset + pickerColumnHeight(font()) + 4;
    const int buttonHeight = kCommandBarHeight - 8;
    const int halfWidth = width() / 2;
    m_confirmButton->setGeometry(4, buttonY, qMax(0, halfWidth - 6), buttonHeight);
    m_cancelButton->setGeometry(halfWidth + 2, buttonY,
                                qMax(0, width() - halfWidth - 6), buttonHeight);
}

DatePickerFlyout::DatePickerFlyout(DatePicker* owner)
    : fluent::dialogs_flyouts::Flyout(owner)
    , m_owner(owner)
{
    setObjectName(QStringLiteral("DatePickerFlyout"));
    setAnimationEnabled(false);
    setPlacement(fluent::dialogs_flyouts::Flyout::Auto);
    setAnchorOffset(::Spacing::XSmall);
    setModal(false);
    setDim(false);
    setClosePolicy(ClosePolicy(CloseOnPressOutside | CloseOnEscape));

    m_panel = new DatePickerFlyoutPanel(this, this);
    connect(this, &DatePickerFlyout::closed, this, [this] {
        if (m_owner)
            m_owner->handleFlyoutClosed();
    });
}

QPoint DatePickerFlyout::computePosition() const
{
    if (!m_owner || !m_panel || !m_owner->window())
        return fluent::dialogs_flyouts::Flyout::computePosition();

    return detail::alignedWheelFlyoutPosition(
        m_owner,
        size(),
        kPopupShadowMargin,
        m_panel->selectedRowCenterY(),
        clampToWindow());
}

QVector<DatePicker::DateField> DatePickerFlyout::visibleFields() const
{
    QVector<DatePicker::DateField> fields;
    if (!m_owner)
        return fields;
    if (m_owner->monthVisible())
        fields.append(DatePicker::DateField::Month);
    if (m_owner->dayVisible())
        fields.append(DatePicker::DateField::Day);
    if (m_owner->yearVisible())
        fields.append(DatePicker::DateField::Year);
    return fields;
}

int DatePickerFlyout::preferredColumnWidth(DatePicker::DateField field) const
{
    if (!m_owner)
        return dateFieldBaseWidth(field);
    return m_owner->preferredFieldWidth(field);
}

bool DatePickerFlyout::isFirstVisibleField(DatePicker::DateField field) const
{
    const auto fields = visibleFields();
    return !fields.isEmpty() && fields.first() == field;
}

bool DatePickerFlyout::isLastVisibleField(DatePicker::DateField field) const
{
    const auto fields = visibleFields();
    return !fields.isEmpty() && fields.last() == field;
}

void DatePickerFlyout::showForPicker()
{
    if (!m_owner)
        return;

    const QDate selected = m_owner->selectedDate();
    setPendingDate(selected.isValid() ? selected : m_owner->date());
    refreshLayout();

    if (isOpen() || isVisible()) {
        show();
        raise();
        setFocus(Qt::PopupFocusReason);
    } else {
        showAt(m_owner);
    }

    if (auto* column = m_panel->firstVisibleColumn())
        column->setFocus(Qt::PopupFocusReason);
}

void DatePickerFlyout::refreshLayout()
{
    if (!m_owner)
        return;

    m_panel->refreshFromFlyout();

    const QSize cardSize = m_panel->sizeHint();
    const int cardW = cardSize.width();
    const int cardH = cardSize.height();
    setFixedSize(cardW + kPopupShadowMargin * 2, cardH + kPopupShadowMargin * 2);
    m_panel->setGeometry(kPopupShadowMargin, kPopupShadowMargin, cardW, cardH);
    setAnchor(m_owner);

    if (isOpen() || isVisible())
        move(computePosition());
}

void DatePickerFlyout::setPendingDate(const QDate& date)
{
    if (!m_owner)
        return;
    const QDate normalized = m_owner->clampDate(date.isValid() ? date : m_owner->date());
    if (m_pendingDate == normalized)
        return;
    const QDate before = m_pendingDate;
    m_pendingDate = normalized;
    updateColumns();
    notifyColumnValueChanges(before, m_pendingDate);
}

QDate DatePickerFlyout::shifted(DatePicker::DateField field, int offset) const
{
    if (!m_owner)
        return QDate();
    return m_owner->shiftedDate(m_pendingDate, field, offset);
}

bool DatePickerFlyout::canShift(DatePicker::DateField field, int offset) const
{
    const QDate candidate = shifted(field, offset);
    return isDateSelectable(candidate) && candidate != m_pendingDate;
}

bool DatePickerFlyout::isDateSelectable(const QDate& date) const
{
    if (!m_owner || !date.isValid())
        return false;
    return date >= m_owner->minimumDate() && date <= m_owner->maximumDate();
}

Qt::Alignment DatePickerFlyout::textAlignment(DatePicker::DateField field) const
{
    return m_owner ? m_owner->fieldTextAlignment(field) : Qt::AlignLeft;
}

QString DatePickerFlyout::displayText(DatePicker::DateField field, const QDate& date) const
{
    return m_owner ? m_owner->formatField(field, date) : QString();
}

void DatePickerFlyout::shiftField(DatePicker::DateField field, int offset)
{
    if (!m_owner || offset == 0)
        return;
    const QDate next = shifted(field, offset);
    if (!isDateSelectable(next) || next == m_pendingDate)
        return;
    const QDate before = m_pendingDate;
    m_pendingDate = next;
    updateColumns();
    notifyColumnValueChanges(before, m_pendingDate);
}

void DatePickerFlyout::commit()
{
    if (m_owner && isDateSelectable(m_pendingDate))
        m_owner->applyPendingDate(m_pendingDate);
    close();
}

void DatePickerFlyout::cancel()
{
    close();
}

void DatePickerFlyout::refreshActionAccessibility()
{
    if (m_panel)
        m_panel->refreshActionAccessibility();
}

void DatePickerFlyout::onThemeUpdated()
{
    fluent::dialogs_flyouts::Flyout::onThemeUpdated();
    if (m_panel)
        m_panel->refreshTheme();
}

void DatePickerFlyout::keyPressEvent(QKeyEvent* event)
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

void DatePickerFlyout::updateColumns()
{
    if (m_panel)
        m_panel->updateColumns();
    update();
}

void DatePickerFlyout::notifyColumnValueChanges(
    const QDate& before, const QDate& after)
{
    if (!m_panel || before == after)
        return;
    auto notify = [this](const char* objectName) {
        if (QWidget* column = m_panel->findChild<QWidget*>(
                QString::fromLatin1(objectName))) {
            detail::notifyPickerColumnValueChanged(column);
        }
    };
    if (before.month() != after.month())
        notify("DatePickerMonthColumn");
    if (before.day() != after.day())
        notify("DatePickerDayColumn");
    if (before.year() != after.year())
        notify("DatePickerYearColumn");
}

DatePicker::DatePicker(QWidget* parent)
    : fluent::basicinput::Button(parent)
{
    detail::ensurePickerAccessibilityFactory();
    m_observedLocale = QWidget::locale();
    const QDate today = QDate::currentDate();
    m_minimumDate = today.addYears(-100);
    m_maximumDate = today.addYears(100);
    m_date = clampDate(today);

    setObjectName(QStringLiteral("DatePicker"));
    setFocusPolicy(Qt::StrongFocus);
    setFluentStyle(fluent::basicinput::Button::Standard);
    setFluentSize(fluent::basicinput::Button::StandardSize);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(this, &fluent::basicinput::Button::clicked, this, &DatePicker::openPicker);
    onThemeUpdated();
}

DatePicker::~DatePicker()
{
    if (m_flyout) {
        DatePickerFlyout* flyout = m_flyout.data();
        m_flyout = nullptr;
        flyout->setAnimationEnabled(false);
        flyout->close();
        delete flyout;
    }
}

void DatePicker::setDate(const QDate& date)
{
    if (!date.isValid())
        return;
    setSelectedDate(date);
}

void DatePicker::setSelectedDate(const QDate& date)
{
    if (!date.isValid()) {
        clearSelectedDate();
        return;
    }

    const QDate normalized = clampDate(date);
    const QDate oldDate = this->date();
    const QDate oldSelected = m_selectedDate;

    m_date = normalized;
    m_selectedDate = normalized;

    if (oldSelected != m_selectedDate)
        detail::notifyPickerRootValueChanged(this);

    if (oldSelected != m_selectedDate)
        emit selectedDateChanged(m_selectedDate);
    if (oldDate != this->date())
        emit dateChanged(this->date());
    update();
}

void DatePicker::clearSelectedDate()
{
    if (!m_selectedDate.isValid())
        return;
    m_selectedDate = QDate();
    detail::notifyPickerRootValueChanged(this);
    emit selectedDateChanged(m_selectedDate);
    update();
}

void DatePicker::setMinimumDate(const QDate& date)
{
    setDateRange(date, m_maximumDate);
}

void DatePicker::setMaximumDate(const QDate& date)
{
    setDateRange(m_minimumDate, date);
}

void DatePicker::setDateRange(const QDate& minimumDate, const QDate& maximumDate)
{
    QDate nextMin = minimumDate.isValid() ? minimumDate : m_minimumDate;
    QDate nextMax = maximumDate.isValid() ? maximumDate : m_maximumDate;
    if (nextMin > nextMax)
        nextMax = nextMin;

    const bool minChanged = m_minimumDate != nextMin;
    const bool maxChanged = m_maximumDate != nextMax;
    if (!minChanged && !maxChanged)
        return;

    m_minimumDate = nextMin;
    m_maximumDate = nextMax;

    if (minChanged)
        emit minimumDateChanged(m_minimumDate);
    if (maxChanged)
        emit maximumDateChanged(m_maximumDate);

    const QDate oldDate = date();
    if (m_selectedDate.isValid()) {
        const QDate clamped = clampDate(m_selectedDate);
        if (clamped != m_selectedDate) {
            m_selectedDate = clamped;
            m_date = clamped;
            detail::notifyPickerRootValueChanged(this);
            emit selectedDateChanged(m_selectedDate);
        }
    } else {
        m_date = clampDate(m_date);
    }
    if (oldDate != date())
        emit dateChanged(date());

    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
}

void DatePicker::setMonthVisible(bool visible)
{
    if (setFieldVisible(DateField::Month, visible)) {
        if (m_selectedDate.isValid())
            detail::notifyPickerRootValueChanged(this);
        emit monthVisibleChanged(m_monthVisible);
    }
}

void DatePicker::setDayVisible(bool visible)
{
    if (setFieldVisible(DateField::Day, visible)) {
        if (m_selectedDate.isValid())
            detail::notifyPickerRootValueChanged(this);
        emit dayVisibleChanged(m_dayVisible);
    }
}

void DatePicker::setYearVisible(bool visible)
{
    if (setFieldVisible(DateField::Year, visible)) {
        if (m_selectedDate.isValid())
            detail::notifyPickerRootValueChanged(this);
        emit yearVisibleChanged(m_yearVisible);
    }
}

void DatePicker::setMonthFormat(MonthFormat format)
{
    if (m_monthFormat == format)
        return;
    m_monthFormat = format;
    updateGeometry();
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
    if (m_selectedDate.isValid())
        detail::notifyPickerRootValueChanged(this);
    emit monthFormatChanged(m_monthFormat);
}

void DatePicker::setDayFormat(DayFormat format)
{
    if (m_dayFormat == format)
        return;
    m_dayFormat = format;
    updateGeometry();
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
    if (m_selectedDate.isValid())
        detail::notifyPickerRootValueChanged(this);
    emit dayFormatChanged(m_dayFormat);
}

void DatePicker::setYearFormat(YearFormat format)
{
    if (m_yearFormat == format)
        return;
    m_yearFormat = format;
    updateGeometry();
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
    if (m_selectedDate.isValid())
        detail::notifyPickerRootValueChanged(this);
    emit yearFormatChanged(m_yearFormat);
}

void DatePicker::setLocale(const QLocale& locale)
{
    if (QWidget::locale() == locale)
        return;
    QWidget::setLocale(locale);
}

void DatePicker::setPlaceholderText(DateField field, const QString& text)
{
    QString* target = nullptr;
    switch (field) {
    case DateField::Month:
        target = &m_monthPlaceholderText;
        break;
    case DateField::Day:
        target = &m_dayPlaceholderText;
        break;
    case DateField::Year:
        target = &m_yearPlaceholderText;
        break;
    }
    if (!target || *target == text)
        return;

    *target = text;
    updateGeometry();
    update();
    emit placeholderTextChanged(field, text);
}

void DatePicker::setConfirmButtonAccessibleName(const QString& name)
{
    if (m_confirmButtonAccessibleName == name)
        return;
    m_confirmButtonAccessibleName = name;
    if (m_flyout)
        m_flyout->refreshActionAccessibility();
    emit confirmButtonAccessibleNameChanged(m_confirmButtonAccessibleName);
}

void DatePicker::setCancelButtonAccessibleName(const QString& name)
{
    if (m_cancelButtonAccessibleName == name)
        return;
    m_cancelButtonAccessibleName = name;
    if (m_flyout)
        m_flyout->refreshActionAccessibility();
    emit cancelButtonAccessibleNameChanged(m_cancelButtonAccessibleName);
}

Qt::Alignment DatePicker::fieldTextAlignment(DateField field) const
{
    switch (field) {
    case DateField::Month:
        return m_monthTextAlignment;
    case DateField::Day:
        return m_dayTextAlignment;
    case DateField::Year:
        return m_yearTextAlignment;
    }
    return Qt::AlignLeft;
}

void DatePicker::setFieldTextAlignment(DateField field, Qt::Alignment alignment)
{
    Qt::Alignment* target = nullptr;
    switch (field) {
    case DateField::Month:
        target = &m_monthTextAlignment;
        break;
    case DateField::Day:
        target = &m_dayTextAlignment;
        break;
    case DateField::Year:
        target = &m_yearTextAlignment;
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

void DatePicker::openPicker()
{
    if (!isEnabled() || visibleFieldCount() <= 0)
        return;

    QPointer<DatePicker> guard(this);
    if (!m_flyout) {
        setDropDownOpen(false);
        if (!guard)
            return;
        m_flyout = new DatePickerFlyout(this);
    }

    m_flyout->showForPicker();
    if (!guard)
        return;
    guard->setDropDownOpen(true);
    if (guard)
        guard->update();
}

void DatePicker::closePicker()
{
    if (m_flyout)
        m_flyout->cancel();
}

QString DatePicker::fieldDisplayText(DateField field) const
{
    if (!isFieldVisible(field))
        return QString();
    if (!m_selectedDate.isValid())
        return placeholderText(field);
    return formatField(field, m_selectedDate);
}

QString DatePicker::placeholderText(DateField field) const
{
    switch (field) {
    case DateField::Month:
        return m_monthPlaceholderText;
    case DateField::Day:
        return m_dayPlaceholderText;
    case DateField::Year:
        return m_yearPlaceholderText;
    }
    return QString();
}

int DatePicker::preferredFieldWidth(DateField field) const
{
    const QFontMetrics metrics(font());
    int textWidth = metrics.horizontalAdvance(placeholderText(field));
    auto includeDate = [this, field, &metrics, &textWidth](const QDate& date) {
        textWidth = qMax(textWidth, metrics.horizontalAdvance(formatField(field, date)));
    };

    switch (field) {
    case DateField::Month:
        for (int month = 1; month <= 12; ++month)
            includeDate(QDate(2026, month, 1));
        break;
    case DateField::Day:
        if (m_dayFormat == DayFormat::DayIntegerWithAbbreviatedWeekday) {
            for (int dayOfWeek = 1; dayOfWeek <= 7; ++dayOfWeek) {
                const QString text = QStringLiteral("31 (%1)").arg(
                    locale().dayName(dayOfWeek, QLocale::ShortFormat));
                textWidth = qMax(textWidth, metrics.horizontalAdvance(text));
            }
        } else {
            includeDate(QDate(2026, 1, 31));
        }
        break;
    case DateField::Year:
        includeDate(QDate(1, 1, 1));
        includeDate(QDate(8888, 1, 1));
        includeDate(QDate(9999, 1, 1));
        break;
    }

    return qMax(dateFieldBaseWidth(field), textWidth + kSegmentHPadding * 2);
}

QSize DatePicker::sizeHint() const
{
    int width = 0;
    if (m_monthVisible)
        width += preferredFieldWidth(DateField::Month);
    if (m_dayVisible)
        width += preferredFieldWidth(DateField::Day);
    if (m_yearVisible)
        width += preferredFieldWidth(DateField::Year);
    width += qMax(0, visibleFieldCount() - 1) * kDividerWidth;
    width = qMax(width, kDatePickerThemeMinWidth);

    return QSize(width, pickerEntryHeight(font()));
}

QSize DatePicker::minimumSizeHint() const
{
    return QSize(kDatePickerThemeMinWidth, pickerEntryHeight(font()));
}

void DatePicker::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto& colors = themeColorsRef();
    const auto radius = themeRadius();
    const QRect surface = fieldSurfaceRect();
    if (surface.isEmpty())
        return;

    const bool active = isEnabled() && (underMouse() || isDown());
    const QRectF sr(surface);

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

        QColor segmentTextColor = isEnabled()
            ? (active ? colors.textPrimary : colors.textSecondary)
            : colors.textDisabled;
        painter.setPen(segmentTextColor);
        const QString text = fieldDisplayText(segment.field);
        QRect textRect = segment.rect.adjusted(kSegmentHPadding, 0, -kSegmentHPadding, 0);
        const Qt::Alignment alignment = Qt::AlignVCenter | fieldTextAlignment(segment.field);
        painter.drawText(textRect, alignment,
                         painter.fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));
    }

}

void DatePicker::keyPressEvent(QKeyEvent* event)
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

void DatePicker::changeEvent(QEvent* event)
{
    fluent::basicinput::Button::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        updateGeometry();
        if (m_flyout && m_flyout->isOpen())
            m_flyout->refreshLayout();
        update();
    }
    if (event->type() == QEvent::LocaleChange
        && m_observedLocale != QWidget::locale()) {
        m_observedLocale = QWidget::locale();
        if (m_flyout && m_flyout->isOpen())
            m_flyout->showForPicker();
        updateGeometry();
        if (m_selectedDate.isValid())
            detail::notifyPickerRootValueChanged(this);
        update();
        emit localeChanged(m_observedLocale);
    }
    if (event->type() == QEvent::EnabledChange) {
        if (!isEnabled())
            closePicker();
        update();
    }
}

void DatePicker::onThemeUpdated()
{
    fluent::basicinput::Button::onThemeUpdated();
    if (m_flyout)
        m_flyout->onThemeUpdated();
}

int DatePicker::visibleFieldCount() const
{
    return (m_monthVisible ? 1 : 0) + (m_dayVisible ? 1 : 0) + (m_yearVisible ? 1 : 0);
}

bool DatePicker::isFieldVisible(DateField field) const
{
    switch (field) {
    case DateField::Month:
        return m_monthVisible;
    case DateField::Day:
        return m_dayVisible;
    case DateField::Year:
        return m_yearVisible;
    }
    return false;
}

bool DatePicker::setFieldVisible(DateField field, bool visible)
{
    bool* target = nullptr;
    switch (field) {
    case DateField::Month:
        target = &m_monthVisible;
        break;
    case DateField::Day:
        target = &m_dayVisible;
        break;
    case DateField::Year:
        target = &m_yearVisible;
        break;
    }

    if (!target || *target == visible)
        return false;
    if (!visible && visibleFieldCount() <= 1)
        return false;

    *target = visible;
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    updateGeometry();
    update();
    return true;
}

QVector<DatePicker::FieldSegment> DatePicker::fieldSegments() const
{
    QVector<DateField> fields;
    if (m_monthVisible)
        fields.append(DateField::Month);
    if (m_dayVisible)
        fields.append(DateField::Day);
    if (m_yearVisible)
        fields.append(DateField::Year);

    QVector<FieldSegment> result;
    const QRect surface = fieldSurfaceRect();
    if (surface.isEmpty() || fields.isEmpty())
        return result;

    int totalWeight = 0;
    auto weightFor = [this](DateField field) {
        return preferredFieldWidth(field);
    };
    for (DateField field : fields)
        totalWeight += weightFor(field);

    int x = surface.left();
    int remainingW = surface.width();
    int remainingWeight = totalWeight;
    for (int i = 0; i < fields.size(); ++i) {
        const int weight = weightFor(fields.at(i));
        int w = i == fields.size() - 1 ? remainingW
            : qMax(32, qRound(double(surface.width()) * double(weight) / double(totalWeight)));
        w = qMin(w, remainingW);
        result.append({fields.at(i), QRect(x, surface.top(), w, surface.height())});
        x += w;
        remainingW -= w;
        remainingWeight -= weight;
        Q_UNUSED(remainingWeight);
    }
    return result;
}

QRect DatePicker::fieldSurfaceRect() const
{
    return QRect(0, 0, width(), qMin(height(), pickerEntryHeight(font())));
}

QString DatePicker::formatField(DateField field, const QDate& date) const
{
    if (!date.isValid())
        return QString();

    switch (field) {
    case DateField::Month:
        switch (m_monthFormat) {
        case MonthFormat::FullMonthName:
            return locale().monthName(date.month(), QLocale::LongFormat);
        case MonthFormat::AbbreviatedMonthName:
            return locale().monthName(date.month(), QLocale::ShortFormat);
        case MonthFormat::NumericMonth:
            return QString::number(date.month());
        case MonthFormat::TwoDigitMonth:
            return QStringLiteral("%1").arg(date.month(), 2, 10, QLatin1Char('0'));
        }
        break;
    case DateField::Day:
        switch (m_dayFormat) {
        case DayFormat::DayInteger:
            return QString::number(date.day());
        case DayFormat::TwoDigitDay:
            return QStringLiteral("%1").arg(date.day(), 2, 10, QLatin1Char('0'));
        case DayFormat::DayIntegerWithAbbreviatedWeekday:
            return QStringLiteral("%1 (%2)")
                .arg(date.day())
                .arg(locale().dayName(date.dayOfWeek(), QLocale::ShortFormat));
        }
        break;
    case DateField::Year:
        switch (m_yearFormat) {
        case YearFormat::FullYear:
            return QString::number(date.year());
        case YearFormat::TwoDigitYear:
            return QStringLiteral("%1").arg(date.year() % 100, 2, 10, QLatin1Char('0'));
        }
        break;
    }
    return QString();
}

QDate DatePicker::normalizeDate(int year, int month, int day) const
{
    return clampDate(dateWithClampedDay(year, month, day));
}

QDate DatePicker::clampDate(const QDate& date) const
{
    QDate result = date.isValid() ? date : QDate::currentDate();
    if (m_minimumDate.isValid() && result < m_minimumDate)
        result = m_minimumDate;
    if (m_maximumDate.isValid() && result > m_maximumDate)
        result = m_maximumDate;
    return result;
}

QDate DatePicker::shiftedDate(const QDate& date, DateField field, int offset) const
{
    const QDate base = date.isValid() ? date : clampDate(QDate::currentDate());
    if (offset == 0)
        return base;

    switch (field) {
    case DateField::Month: {
        int month = (base.month() - 1 + offset) % 12;
        if (month < 0)
            month += 12;
        return dateWithClampedDay(base.year(), month + 1, base.day());
    }
    case DateField::Day: {
        const int daysInMonth = QDate(base.year(), base.month(), 1).daysInMonth();
        const int day = wrappedValue(base.day() + offset, 1, daysInMonth);
        return dateWithClampedDay(base.year(), base.month(), day);
    }
    case DateField::Year:
        return dateWithClampedDay(base.year() + offset, base.month(), base.day());
    }
    return base;
}

void DatePicker::setDropDownOpen(bool open)
{
    if (m_dropDownOpen == open)
        return;
    m_dropDownOpen = open;
    detail::notifyPickerRootPopupChanged(this);
    QPointer<DatePicker> guard(this);
    emit dropDownOpenChanged(m_dropDownOpen);
    if (guard)
        guard->update();
}

void DatePicker::applyPendingDate(const QDate& date)
{
    setSelectedDate(date);
}

void DatePicker::handleFlyoutClosed()
{
    setDropDownOpen(false);
}

} // namespace fluent::date_time
