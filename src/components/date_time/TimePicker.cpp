#include "TimePicker.h"

#include <QDateTime>
#include <QFocusEvent>
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
#include "design/Spacing.h"
#include "design/Typography.h"
#include "components/basicinput/Button.h"
#include "components/dialogs_flyouts/Flyout.h"

namespace fluent::date_time {

namespace {
constexpr int kEntryHeight = 32;
constexpr int kMinEntryWidth = 128;
constexpr int kSegmentHPadding = 12;
constexpr int kPopupShadowMargin = ::Spacing::Standard;
constexpr int kPopupCardInset = 8;
constexpr int kColumnNavHeight = 24;
constexpr int kColumnRowHeight = 36;
constexpr int kColumnVisibleRows = 7;
constexpr int kColumnHeight = kColumnNavHeight * 2 + kColumnRowHeight * kColumnVisibleRows;
constexpr int kCommandBarHeight = 48;
constexpr int kDividerWidth = 1;
constexpr qreal kColumnWheelThreshold = 120.0;
constexpr int kColumnWheelClusterGapMs = 120;

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

int fieldColumnWidth(TimePicker::TimeField field, TimePicker::ClockIdentifier clockIdentifier)
{
    switch (field) {
    case TimePicker::TimeField::Hour:
        return clockIdentifier == TimePicker::ClockIdentifier::TwentyFourHourClock ? 82 : 72;
    case TimePicker::TimeField::Minute:
        return 82;
    case TimePicker::TimeField::Period:
        return 82;
    }
    return 82;
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

class TimePickerColumn : public QWidget, public FluentElement {
public:
    TimePickerColumn(TimePickerFlyout* flyout, TimePicker::TimeField field, QWidget* parent = nullptr);

    TimePicker::TimeField field() const { return m_field; }
    QSize sizeHint() const override { return QSize(m_widthHint, kColumnHeight); }
    void setWidthHint(int width);

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
    void shiftField(TimePicker::TimeField field, int offset);
    void commit();
    void cancel();

    void onThemeUpdated() override;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    friend class TimePickerFlyoutPanel;

    QVector<TimePicker::TimeField> visibleFields() const;
    int preferredColumnWidth(TimePicker::TimeField field) const;
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

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void onThemeUpdated() override;

private:
    void layoutContent();

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

void TimePickerColumn::setWidthHint(int width)
{
    if (m_widthHint == width)
        return;
    m_widthHint = qMax(48, width);
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
    return QRect(0, kColumnNavHeight + row * kColumnRowHeight, width(), kColumnRowHeight);
}

TimePickerColumn::HitInfo TimePickerColumn::hitTest(const QPoint& pos) const
{
    if (previousButtonRect().contains(pos))
        return {HitKind::Previous, -1};
    if (nextButtonRect().contains(pos))
        return {HitKind::Next, 1};

    const int rowAreaY = pos.y() - kColumnNavHeight;
    if (rowAreaY >= 0 && rowAreaY < kColumnRowHeight * kColumnVisibleRows) {
        const int row = rowAreaY / kColumnRowHeight;
        return {HitKind::Row, row - kColumnVisibleRows / 2};
    }
    return {};
}

void TimePickerColumn::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto colors = themeColors();
    const auto radius = themeRadius();

    painter.fillRect(rect(), colors.bgLayer);

    const bool canPrevious = m_flyout && m_flyout->canShift(m_field, -1);
    const bool canNext = m_flyout && m_flyout->canShift(m_field, 1);

    if (m_navButtonOpacity > 0.01) {
        QFont iconFont(Typography::FontFamily::FluentIcons);
        iconFont.setPixelSize(8);
        painter.setFont(iconFont);

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
            painter.drawText(rect, Qt::AlignCenter, glyph);
            painter.restore();
        };

        paintNavButton(previousButtonRect(), pickerChevronUpGlyph(), canPrevious,
                       m_hoverHit.kind == HitKind::Previous);
        paintNavButton(nextButtonRect(), pickerChevronDownGlyph(), canNext,
                       m_hoverHit.kind == HitKind::Next);
    }

    // The selected-row highlight + row hover branch on the active design language; the row loop,
    // geometry, alignment and text layout stay shared. DesignFluent is byte-for-byte unchanged.
    // zh_CN: 选中行高亮 + 行悬停按当前设计语言分支;行循环、几何、对齐与文字布局共用。Fluent 路径逐字节不变。
    const DesignLanguage lang = themeDesignLanguage();
    // Theme-aware veil (same idiom as Button/ComboBox): DARKEN light surfaces, LIGHTEN dark ones so
    // hover stays visible under both App themes. zh_CN: 主题感知薄层(与 Button/ComboBox 同):浅色面变暗、
    // 深色面变亮,使 hover 在明暗两主题下都可见。
    const bool darkTheme = effectiveTheme() == Dark;
    const auto veil = [darkTheme](int alpha) {
        return darkTheme ? QColor(255, 255, 255, alpha) : QColor(0, 0, 0, alpha);
    };
    const auto withAlpha = [](QColor c, int a) { c.setAlpha(a); return c; };

    painter.setFont(themeFont(Typography::FontRole::Body).toQFont());
    const int centerRow = kColumnVisibleRows / 2;
    const Qt::Alignment textAlignment = m_flyout ? m_flyout->textAlignment(m_field) : Qt::AlignLeft;
    for (int row = 0; row < kColumnVisibleRows; ++row) {
        const int offset = row - centerRow;
        const QRect rowBounds = rowRect(row).adjusted(4, 2, -4, -2);
        const QTime valueTime = m_flyout ? m_flyout->shifted(m_field, offset) : QTime();
        const bool selectable = m_flyout && m_flyout->canShift(m_field, offset);
        const bool selected = offset == 0;
        const bool hovered = m_hoverHit.kind == HitKind::Row && m_hoverHit.offset == offset;

        // Resolve the highlight fill + selected text color per language. Init highlightFill to a real
        // value (Qt::transparent) — a default-constructed QColor is INVALID yet alpha()==255, so a bare
        // alpha()>0 guard would fire and setBrush() would paint SOLID BLACK. zh_CN: 按语言解析高亮填充与选中
        // 文字色。highlightFill 必须初始化为真实值(Qt::transparent)——默认构造的 QColor 无效却 alpha()==255,
        // 裸 alpha()>0 会命中,setBrush() 会涂成纯黑。
        QColor highlightFill = Qt::transparent;
        QColor selectedText = colors.textOnAccent;
        if (lang == DesignMaterial) {
            // Material 3: the selected row is a low-alpha accent WASH pill (a "secondary-container"-style
            // tint, NOT a solid block) with accent-colored selected text; hover is the on-surface state
            // layer. zh_CN: Material 3:选中行为低 alpha 强调色 WASH 胶囊(类「secondary-container」着色,而非实心块),
            // 选中文字用强调色;悬停为 on-surface state layer。
            if (selected)
                highlightFill = withAlpha(colors.accentDefault, m_columnHovered ? 0x33 : 0x29); // ~16% / ~20%
            else if (hovered && selectable)
                highlightFill = veil(0x14); // 8% on-surface state layer
            selectedText = colors.accentDefault;
        } else if (lang == DesignCupertino) {
            // macOS: the selected row is a solid accent fill with white-on-accent text (the system list
            // selection look); hover uses the theme-aware veil so it reads in both Light and Dark.
            // zh_CN: macOS:选中行为实心 accent 填充 + on-accent 白字(系统列表选中外观);悬停用主题感知薄层,
            // 在明暗主题下都可读。
            if (selected)
                highlightFill = m_columnHovered ? colors.accentSecondary : colors.accentDefault;
            else if (hovered && selectable)
                highlightFill = veil(darkTheme ? 0x1C : 0x16);
            selectedText = colors.textOnAccent;
        } else {
            // DesignFluent (default): unchanged WinUI treatment. zh_CN: 默认 Fluent,WinUI 处理不变。
            if (selected)
                highlightFill = m_columnHovered ? colors.accentSecondary : colors.accentDefault;
            else if (hovered && selectable)
                highlightFill = colors.subtleSecondary;
            selectedText = colors.textOnAccent;
        }

        if (highlightFill.isValid() && highlightFill.alpha() > 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(highlightFill);
            painter.drawRoundedRect(rowBounds, radius.control, radius.control);
        }

        QColor textColor = selectable || selected ? colors.textPrimary : colors.textDisabled;
        if (selected)
            textColor = selectedText;
        painter.setPen(textColor);
        const QString text = m_flyout ? m_flyout->displayText(m_field, valueTime) : QString();
        painter.drawText(rowBounds.adjusted(8, 0, -8, 0), Qt::AlignVCenter | textAlignment,
                         painter.fontMetrics().elidedText(text, Qt::ElideRight, rowBounds.width() - 16));
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
    setProperty("previousButtonGlyph", pickerChevronUpGlyph());
    setProperty("nextButtonGlyph", pickerChevronDownGlyph());
    setProperty("textAlignment", static_cast<int>(m_flyout ? m_flyout->textAlignment(m_field) : Qt::AlignLeft));
    setProperty("visibleItemCount", m_field == TimePicker::TimeField::Period ? 2 : kColumnVisibleRows);
    setProperty("navButtonOpacity", m_navButtonOpacity);
    setProperty("navButtonTargetOpacity", m_navButtonTargetOpacity);
    setProperty("columnHovered", m_columnHovered);
    setProperty("focusFrameVisible", false);
    setProperty("selectedRowHasBackground", true);
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
    m_confirmButton->setFixedSize(48, 32);
    m_confirmButton->setAccessibleName(QStringLiteral("Accept time"));

    m_cancelButton = new fluent::basicinput::Button(this);
    m_cancelButton->setObjectName(QStringLiteral("TimePickerCancelButton"));
    m_cancelButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_cancelButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_cancelButton->setIconGlyph(Typography::Icons::Cancel, Typography::IconSize::Standard);
    m_cancelButton->setFixedSize(48, 32);
    m_cancelButton->setAccessibleName(QStringLiteral("Cancel time"));

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

    int width = kPopupCardInset * 2;
    const auto fields = m_flyout->visibleFields();
    for (TimePicker::TimeField field : fields)
        width += m_flyout->preferredColumnWidth(field);
    if (!fields.isEmpty())
        width += (fields.size() - 1) * kDividerWidth;

    const int height = kPopupCardInset + kColumnHeight + kCommandBarHeight;
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

    const auto fields = m_flyout->visibleFields();
    auto configure = [this, &fields](TimePickerColumn* column, TimePicker::TimeField field) {
        const bool visible = fields.contains(field);
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
    const auto colors = themeColors();

    painter.setPen(colors.strokeDivider);
    int x = kPopupCardInset;
    const auto fields = m_flyout->visibleFields();
    for (int i = 0; i < fields.size() - 1; ++i) {
        x += m_flyout->preferredColumnWidth(fields.at(i));
        painter.drawLine(x, kPopupCardInset, x, kPopupCardInset + kColumnHeight);
        x += kDividerWidth;
    }

    painter.drawLine(0, kPopupCardInset + kColumnHeight,
                     width(), kPopupCardInset + kColumnHeight);
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

void TimePickerFlyoutPanel::layoutContent()
{
    if (rect().isEmpty())
        return;

    int x = kPopupCardInset;
    const int y = kPopupCardInset;

    auto placeColumn = [this, &x, y](TimePickerColumn* column, TimePicker::TimeField field) {
        if (column->isHidden())
            return;
        const int w = m_flyout->preferredColumnWidth(field);
        column->setGeometry(x, y, w, kColumnHeight);
        x += w + kDividerWidth;
    };

    placeColumn(m_hourColumn, TimePicker::TimeField::Hour);
    placeColumn(m_minuteColumn, TimePicker::TimeField::Minute);
    placeColumn(m_periodColumn, TimePicker::TimeField::Period);

    const int buttonY = kPopupCardInset + kColumnHeight + (kCommandBarHeight - 32) / 2;
    const int totalButtonW = m_confirmButton->width() + m_cancelButton->width() + 16;
    const int startX = rect().center().x() - totalButtonW / 2;
    m_confirmButton->setGeometry(startX, buttonY, m_confirmButton->width(), m_confirmButton->height());
    m_cancelButton->setGeometry(startX + m_confirmButton->width() + 16, buttonY,
                                m_cancelButton->width(), m_cancelButton->height());
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
        return 82;
    return fieldColumnWidth(field, m_owner->clockIdentifier());
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
    m_pendingTime = normalized;
    updateColumns();
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
    m_pendingTime = next;
    updateColumns();
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

TimePicker::TimePicker(QWidget* parent)
    : fluent::basicinput::Button(parent)
{
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
        m_flyout->setAnimationEnabled(false);
        m_flyout->close();
        delete m_flyout;
        m_flyout = nullptr;
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
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    updateGeometry();
    update();
    emit clockIdentifierChanged(m_clockIdentifier);
}

void TimePicker::setLocale(const QLocale& locale)
{
    if (m_locale == locale)
        return;
    m_locale = locale;
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
    emit localeChanged(m_locale);
}

void TimePicker::openPicker()
{
    if (!isEnabled())
        return;

    if (!m_flyout)
        m_flyout = new TimePickerFlyout(this);

    m_flyout->showForPicker();
    setDropDownOpen(true);
    update();
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
        return QStringLiteral("hour");
    case TimeField::Minute:
        return QStringLiteral("minute");
    case TimeField::Period:
        return QStringLiteral("AM/PM");
    }
    return QString();
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
    int width = kMinEntryWidth;
    for (TimeField field : visibleFields())
        width += fieldColumnWidth(field, m_clockIdentifier);
    width = qMax(width, kMinEntryWidth);

    return QSize(width, kEntryHeight);
}

QSize TimePicker::minimumSizeHint() const
{
    return QSize(kMinEntryWidth, kEntryHeight);
}

void TimePicker::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto colors = themeColors();
    const auto radius = themeRadius();
    const QRect surface = fieldSurfaceRect();
    if (surface.isEmpty())
        return;

    const bool active = isEnabled() && (underMouse() || isDown());

    // Branch the CLOSED-FIELD surface on the active design language. The segment divider lines and
    // text (drawn below) are shared; only the field background/border differ per brand. "Open" reads
    // as focus for the closed field on every brand. DesignFluent is byte-for-byte unchanged.
    // zh_CN: 闭合字段表面按当前设计语言分支;下方分段分隔线与文字共用,仅字段背景/边框按品牌区分。
    // 对闭合字段而言「展开」等同聚焦。Fluent 路径逐字节不变。
    const DesignLanguage lang = themeDesignLanguage();
    const bool darkTheme = effectiveTheme() == Dark;
    const auto veil = [darkTheme](int alpha) {
        return darkTheme ? QColor(255, 255, 255, alpha) : QColor(0, 0, 0, alpha);
    };
    const bool fieldFocused = isEnabled() && (m_dropDownOpen || hasFocus());
    const QRectF r(surface);

    if (lang == DesignMaterial) {
        // Material 3: Outlined text-field look (matching ComboBox). Transparent/controlDefault fill, a
        // 1 dp `outline` (strokeStrong) at rest, a 2 dp `primary` (accentDefault) outline when focused/
        // open. Hover/press add a within-bounds state-layer veil. §4/§5. zh_CN: Material 3:描边文本框外观
        //(与 ComboBox 一致)。透明/controlDefault 填充,静息 1dp outline(strokeStrong),聚焦/展开 2dp
        // primary(accentDefault) 描边。Hover/press 叠加界内 state-layer 薄层。
        const qreal mR = qMax<qreal>(0.0, radius.control);
        QColor fill = !isEnabled() ? colors.controlDisabled
                    : ((isDown() || m_dropDownOpen) ? colors.controlDefault : QColor(Qt::transparent));

        QPainterPath bgPath;
        bgPath.addRoundedRect(r.adjusted(1.0, 1.0, -1.0, -1.0), mR, mR);
        painter.setPen(Qt::NoPen);
        if (fill.isValid() && fill != QColor(Qt::transparent)) {
            painter.setBrush(fill);
            painter.drawPath(bgPath);
        }

        if (isEnabled() && !fieldFocused) {
            QColor stateLayer = Qt::transparent;
            if (isDown()) stateLayer = veil(0x1A);        // 10%
            else if (underMouse()) stateLayer = veil(0x14); // 8%
            if (stateLayer.isValid() && stateLayer.alpha() > 0) {
                painter.save();
                painter.setClipPath(bgPath);
                painter.setBrush(stateLayer);
                painter.drawPath(bgPath);
                painter.restore();
            }
        }

        const qreal sw = fieldFocused ? 2.0 : 1.0;
        const QColor outline = !isEnabled() ? colors.strokeSecondary
                                            : (fieldFocused ? colors.accentDefault : colors.strokeStrong);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(outline, sw));
        const qreal inset = sw / 2.0;
        painter.drawRoundedRect(r.adjusted(inset, inset, -inset, -inset), mR, mR);
    } else if (lang == DesignCupertino) {
        // macOS: bezel push-button look (matching ComboBox/Button). Subtle fill + 1px strokeStrong
        // hairline + small radius (~6) + faint 1px lower-edge drop shadow. Pressed/open darken via the
        // theme-aware veil; focus/open gets an accent hairline. zh_CN: macOS:bezel 按钮外观(与 ComboBox/
        // Button 一致)。柔和填充 + 1px strokeStrong 发丝边框 + 小圆角(~6) + 底缘 1px 柔和投影。按下/展开用主题
        // 感知薄层压暗;聚焦/展开使用强调色发丝边。
        const qreal mR = qMax<qreal>(0.0, qMin<qreal>(radius.control, 6.0));
        QColor fill = isEnabled() ? colors.bgLayerAlt : colors.controlDisabled;

        QPainterPath bgPath;
        bgPath.addRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), mR, mR);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawPath(bgPath);

        if (isEnabled()) {
            QColor stateLayer = Qt::transparent;
            if (isDown() || m_dropDownOpen) stateLayer = veil(darkTheme ? 0x3A : 0x30);
            else if (underMouse()) stateLayer = veil(darkTheme ? 0x1C : 0x16);
            // Guard isValid() too — a default-constructed QColor is INVALID yet alpha()==255, so a bare
            // alpha()>0 check would fire at rest and paint SOLID BLACK over the bezel. zh_CN: 必须同时判
            // isValid()——默认构造的 QColor 无效却 alpha()==255,裸 alpha()>0 会在静息命中并把 bezel 涂成纯黑。
            if (stateLayer.isValid() && stateLayer.alpha() > 0) {
                painter.save();
                painter.setClipPath(bgPath);
                painter.setBrush(stateLayer);
                painter.drawPath(bgPath);
                painter.restore();
            }
        }

        if (isEnabled() && !isDown() && !m_dropDownOpen) {
            painter.save();
            painter.setClipPath(bgPath);
            painter.setBrush(Qt::NoBrush);
            QColor shadow(0, 0, 0, darkTheme ? 0x3A : 0x1E);
            painter.setPen(QPen(shadow, 1.0));
            painter.drawRoundedRect(r.adjusted(1.0, 2.0, -1.0, -1.0), mR, mR);
            painter.restore();
        }

        const QColor hairline = !isEnabled() ? colors.strokeSecondary
                                             : (fieldFocused ? colors.accentDefault : colors.strokeStrong);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(hairline, 1.0));
        painter.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), mR, mR);
    } else {
        // DesignFluent (default): unchanged WinUI treatment. zh_CN: 默认 Fluent,WinUI 处理不变。
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
    }

    const auto segments = fieldSegments();
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
        painter.setFont(themeFont(Typography::FontRole::Body).toQFont());
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
        (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        openPicker();
        event->accept();
        return;
    }
    fluent::basicinput::Button::keyPressEvent(event);
}

void TimePicker::changeEvent(QEvent* event)
{
    fluent::basicinput::Button::changeEvent(event);
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
        totalWeight += fieldColumnWidth(field, m_clockIdentifier);

    int x = surface.left();
    int remainingW = surface.width();
    for (int i = 0; i < fields.size(); ++i) {
        const int weight = fieldColumnWidth(fields.at(i), m_clockIdentifier);
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
    return QRect(0, 0, width(), kEntryHeight);
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
        return isPm(time) ? m_locale.pmText() : m_locale.amText();
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
    emit dropDownOpenChanged(m_dropDownOpen);
    update();
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
