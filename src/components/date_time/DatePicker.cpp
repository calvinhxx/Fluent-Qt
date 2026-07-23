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

int monthColumnWidth(DatePicker::MonthFormat format)
{
    return format == DatePicker::MonthFormat::FullMonthName ? 142 : 96;
}

int dayColumnWidth(DatePicker::DayFormat format)
{
    return format == DatePicker::DayFormat::DayIntegerWithAbbreviatedWeekday ? 104 : 82;
}

int yearColumnWidth(DatePicker::YearFormat)
{
    return 96;
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

class PickerColumn : public QWidget, public FluentElement {
public:
    PickerColumn(DatePickerFlyout* flyout, DatePicker::DateField field, QWidget* parent = nullptr);

    DatePicker::DateField field() const { return m_field; }
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
    void setPendingDate(const QDate& date);
    QDate shifted(DatePicker::DateField field, int offset) const;
    bool canShift(DatePicker::DateField field, int offset) const;
    bool isDateSelectable(const QDate& date) const;
    Qt::Alignment textAlignment(DatePicker::DateField field) const;
    QString displayText(DatePicker::DateField field, const QDate& date) const;
    void shiftField(DatePicker::DateField field, int offset);
    void commit();
    void cancel();

    void onThemeUpdated() override;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    friend class DatePickerFlyoutPanel;

    QVector<DatePicker::DateField> visibleFields() const;
    int preferredColumnWidth(DatePicker::DateField field) const;
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
    void refreshFromFlyout();
    void refreshTheme();
    void updateColumns();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void onThemeUpdated() override;

private:
    void layoutContent();

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

void PickerColumn::setWidthHint(int width)
{
    if (m_widthHint == width)
        return;
    m_widthHint = qMax(48, width);
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
    return QRect(0, kColumnNavHeight + row * kColumnRowHeight, width(), kColumnRowHeight);
}

PickerColumn::HitInfo PickerColumn::hitTest(const QPoint& pos) const
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

void PickerColumn::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto colors = themeColors();
    const auto radius = themeRadius();

    // macOS child widgets lack per-pixel alpha compositing; fill the background
    // explicitly so column content never stacks.
    // zh_CN: macOS 子控件不支持逐像素 alpha 合成，显式填充背景防止列内容叠加。
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

    // Branch the selected-row highlight (and row hover) on the active design language. The Fluent path
    // is byte-for-byte unchanged; Material 3 uses a low-alpha accent "state-layer" pill with accent
    // text, and macOS (Cupertino) uses a solid accent fill with on-accent text. zh_CN: 选中行高亮(及
    // 行悬停)按当前设计语言分支:Fluent 路径逐字节不变;Material 3 用低透明度 accent「state-layer」胶囊
    // + accent 文字;macOS(Cupertino) 用实心 accent 填充 + on-accent 文字。
    const DesignLanguage lang = themeDesignLanguage();
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
        const QRect r = rowRect(row).adjusted(4, 2, -4, -2);
        const QDate valueDate = m_flyout ? m_flyout->shifted(m_field, offset) : QDate();
        const bool selectable = m_flyout && m_flyout->isDateSelectable(valueDate);
        const bool selected = offset == 0;
        const bool hovered = m_hoverHit.kind == HitKind::Row && m_hoverHit.offset == offset;

        // Per-language highlight + the text color that pairs with it. zh_CN: 各设计语言的高亮 + 与之搭配的文字色。
        QColor highlightFill = Qt::transparent; // guard against the invalid-QColor trap below.
        QColor selectedTextColor = colors.textOnAccent;
        if (lang == DesignMaterial) {
            // Material 3: the selected row is a tonal "state layer" pill — a low-alpha accent wash, not
            // a solid block — with accent-colored text. Row hover is the on-surface state layer veil
            // (8%), clipped to the rounded pill. zh_CN: Material 3:选中行为色调「state layer」胶囊——低透
            // 明度 accent 淡彩而非实心块——配 accent 文字。行悬停为 on-surface state-layer 薄层(8%)。
            if (selected) {
                highlightFill = withAlpha(colors.accentDefault, m_columnHovered ? 0x29 : 0x1F); // ~16%/12%
                selectedTextColor = colors.accentDefault;
            } else if (hovered && selectable) {
                highlightFill = veil(0x14); // 8% on-surface state layer
            }
        } else if (lang == DesignCupertino) {
            // macOS: the selected row is a solid accent fill with on-accent (white) text — the quiet,
            // saturated highlight of the native wheel picker. Row hover uses the theme-aware veil so it
            // stays visible in both light and dark. zh_CN: macOS:选中行为实心 accent 填充 + on-accent(白)
            // 文字——原生滚轮选择器的安静饱和高亮。行悬停用主题感知薄层,明暗两主题下都可见。
            if (selected) {
                highlightFill = m_columnHovered ? colors.accentSecondary : colors.accentDefault;
                selectedTextColor = colors.textOnAccent;
            } else if (hovered && selectable) {
                highlightFill = veil(darkTheme ? 0x1C : 0x16);
            }
        } else {
            // DesignFluent (default): unchanged WinUI treatment. zh_CN: 默认 Fluent,WinUI 处理不变。
            if (selected) {
                highlightFill = m_columnHovered ? colors.accentSecondary : colors.accentDefault;
                selectedTextColor = colors.textOnAccent;
            } else if (hovered && selectable) {
                highlightFill = colors.subtleSecondary;
            }
        }

        // Guard the optional fill: a default-constructed QColor is INVALID yet alpha()==255, so
        // setBrush(invalid) paints SOLID BLACK. zh_CN: 守卫可选填充:默认构造 QColor 无效却 alpha==255,
        // setBrush(无效色) 会涂成纯黑。
        if (highlightFill.isValid() && highlightFill.alpha() > 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(highlightFill);
            painter.drawRoundedRect(r, radius.control, radius.control);
        }

        QColor textColor = selectable ? colors.textPrimary : colors.textDisabled;
        if (selected)
            textColor = selectedTextColor;
        painter.setPen(textColor);

        const QString text = m_flyout ? m_flyout->displayText(m_field, valueDate) : QString();
        painter.drawText(r.adjusted(8, 0, -8, 0), Qt::AlignVCenter | textAlignment,
                         painter.fontMetrics().elidedText(text, Qt::ElideRight, r.width() - 16));
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
    setProperty("previousButtonGlyph", pickerChevronUpGlyph());
    setProperty("nextButtonGlyph", pickerChevronDownGlyph());
    setProperty("textAlignment", static_cast<int>(m_flyout ? m_flyout->textAlignment(m_field) : Qt::AlignLeft));
    setProperty("navButtonOpacity", m_navButtonOpacity);
    setProperty("navButtonTargetOpacity", m_navButtonTargetOpacity);
    setProperty("columnHovered", m_columnHovered);
    setProperty("focusFrameVisible", false);
    setProperty("selectedRowHasBackground", true);
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
    m_confirmButton->setFixedSize(48, 32);
    m_confirmButton->setAccessibleName(QStringLiteral("Accept date"));

    m_cancelButton = new fluent::basicinput::Button(this);
    m_cancelButton->setObjectName(QStringLiteral("DatePickerCancelButton"));
    m_cancelButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_cancelButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_cancelButton->setIconGlyph(Typography::Icons::Cancel, Typography::IconSize::Standard);
    m_cancelButton->setFixedSize(48, 32);
    m_cancelButton->setAccessibleName(QStringLiteral("Cancel date"));

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

    int width = kPopupCardInset * 2;
    const auto fields = m_flyout->visibleFields();
    for (DatePicker::DateField field : fields)
        width += m_flyout->preferredColumnWidth(field);
    if (!fields.isEmpty())
        width += (fields.size() - 1) * kDividerWidth;

    const int height = kPopupCardInset + kColumnHeight + kCommandBarHeight;
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

void DatePickerFlyoutPanel::refreshFromFlyout()
{
    if (!m_flyout)
        return;

    const auto fields = m_flyout->visibleFields();
    auto configure = [this, &fields](PickerColumn* column, DatePicker::DateField field) {
        const bool visible = fields.contains(field);
        column->setVisible(visible);
        column->setEnabled(visible);
        column->setWidthHint(m_flyout->preferredColumnWidth(field));
    };
    configure(m_monthColumn, DatePicker::DateField::Month);
    configure(m_dayColumn, DatePicker::DateField::Day);
    configure(m_yearColumn, DatePicker::DateField::Year);

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

void DatePickerFlyoutPanel::layoutContent()
{
    if (rect().isEmpty())
        return;

    int x = kPopupCardInset;
    const int y = kPopupCardInset;

    auto placeColumn = [this, &x, y](PickerColumn* column, DatePicker::DateField field) {
        if (column->isHidden())
            return;
        const int w = m_flyout->preferredColumnWidth(field);
        column->setGeometry(x, y, w, kColumnHeight);
        x += w + kDividerWidth;
    };

    placeColumn(m_monthColumn, DatePicker::DateField::Month);
    placeColumn(m_dayColumn, DatePicker::DateField::Day);
    placeColumn(m_yearColumn, DatePicker::DateField::Year);

    const int buttonY = kPopupCardInset + kColumnHeight + (kCommandBarHeight - 32) / 2;
    const int totalButtonW = m_confirmButton->width() + m_cancelButton->width() + 16;
    const int startX = rect().center().x() - totalButtonW / 2;
    m_confirmButton->setGeometry(startX, buttonY, m_confirmButton->width(), m_confirmButton->height());
    m_cancelButton->setGeometry(startX + m_confirmButton->width() + 16, buttonY,
                                m_cancelButton->width(), m_cancelButton->height());
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
        return 96;
    switch (field) {
    case DatePicker::DateField::Month:
        return monthColumnWidth(m_owner->monthFormat());
    case DatePicker::DateField::Day:
        return dayColumnWidth(m_owner->dayFormat());
    case DatePicker::DateField::Year:
        return yearColumnWidth(m_owner->yearFormat());
    }
    return 96;
}

void DatePickerFlyout::showForPicker()
{
    if (!m_owner)
        return;

    const QDate selected = m_owner->selectedDate();
    setPendingDate(selected.isValid() ? selected : m_owner->date());
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

void DatePickerFlyout::setPendingDate(const QDate& date)
{
    if (!m_owner)
        return;
    const QDate normalized = m_owner->clampDate(date.isValid() ? date : m_owner->date());
    if (m_pendingDate == normalized)
        return;
    m_pendingDate = normalized;
    updateColumns();
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
    m_pendingDate = next;
    updateColumns();
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

DatePicker::DatePicker(QWidget* parent)
    : fluent::basicinput::Button(parent)
{
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
        m_flyout->setAnimationEnabled(false);
        m_flyout->close();
        delete m_flyout;
        m_flyout = nullptr;
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
    if (setFieldVisible(DateField::Month, visible))
        emit monthVisibleChanged(m_monthVisible);
}

void DatePicker::setDayVisible(bool visible)
{
    if (setFieldVisible(DateField::Day, visible))
        emit dayVisibleChanged(m_dayVisible);
}

void DatePicker::setYearVisible(bool visible)
{
    if (setFieldVisible(DateField::Year, visible))
        emit yearVisibleChanged(m_yearVisible);
}

void DatePicker::setMonthFormat(MonthFormat format)
{
    if (m_monthFormat == format)
        return;
    m_monthFormat = format;
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
    emit monthFormatChanged(m_monthFormat);
}

void DatePicker::setDayFormat(DayFormat format)
{
    if (m_dayFormat == format)
        return;
    m_dayFormat = format;
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
    emit dayFormatChanged(m_dayFormat);
}

void DatePicker::setYearFormat(YearFormat format)
{
    if (m_yearFormat == format)
        return;
    m_yearFormat = format;
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
    emit yearFormatChanged(m_yearFormat);
}

void DatePicker::setLocale(const QLocale& locale)
{
    if (m_locale == locale)
        return;
    m_locale = locale;
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
    emit localeChanged(m_locale);
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

    if (!m_flyout)
        m_flyout = new DatePickerFlyout(this);

    m_flyout->showForPicker();
    setDropDownOpen(true);
    update();
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
        return QStringLiteral("month");
    case DateField::Day:
        return QStringLiteral("day");
    case DateField::Year:
        return QStringLiteral("year");
    }
    return QString();
}

QSize DatePicker::sizeHint() const
{
    int width = kMinEntryWidth;
    if (m_monthVisible)
        width += monthColumnWidth(m_monthFormat);
    if (m_dayVisible)
        width += dayColumnWidth(m_dayFormat);
    if (m_yearVisible)
        width += yearColumnWidth(m_yearFormat);
    width = qMax(width, kMinEntryWidth);

    return QSize(width, kEntryHeight);
}

QSize DatePicker::minimumSizeHint() const
{
    return QSize(kMinEntryWidth, kEntryHeight);
}

void DatePicker::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto colors = themeColors();
    const auto radius = themeRadius();
    const QRect surface = fieldSurfaceRect();
    if (surface.isEmpty())
        return;

    const bool active = isEnabled() && (underMouse() || isDown());

    // Branch the CLOSED-FIELD surface on the active design language (same idiom as ComboBox). The
    // segment text + dividers below are shared; only the field background/border differ per brand.
    // "Open" (m_dropDownOpen) reads as focus for the field on every brand. Fluent stays byte-for-byte
    // unchanged. zh_CN: 闭合字段表面按当前设计语言分支(与 ComboBox 同一套路)。下方分段文字与分隔线为共用,
    // 仅字段背景/边框按品牌区分。"展开"(m_dropDownOpen) 对每个品牌都等同于聚焦。Fluent 路径逐字节不变。
    const DesignLanguage lang = themeDesignLanguage();
    const bool darkTheme = effectiveTheme() == Dark;
    const auto veil = [darkTheme](int alpha) {
        return darkTheme ? QColor(255, 255, 255, alpha) : QColor(0, 0, 0, alpha);
    };
    const bool fieldFocused = isEnabled() && m_dropDownOpen;
    const QRectF sr(surface);

    if (lang == DesignMaterial) {
        // Material 3: outlined field. Transparent/controlDefault fill, a 1 dp `outline` (strokeStrong)
        // at rest → 2 dp `primary` (accentDefault) when open/focused. Hover/press add a within-bounds
        // state-layer veil. §4/§5. zh_CN: Material 3:描边字段。透明/controlDefault 填充,静息 1dp
        // outline(strokeStrong)→ 展开/聚焦 2dp primary(accentDefault)。Hover/press 叠加界内 state-layer 薄层。
        const qreal mR = qMax<qreal>(0.0, radius.control);
        QColor fill = !isEnabled() ? colors.controlDisabled
                                   : (isDown() || m_dropDownOpen ? colors.controlDefault
                                                                 : QColor(Qt::transparent));

        QPainterPath bgPath;
        bgPath.addRoundedRect(sr.adjusted(1.0, 1.0, -1.0, -1.0), mR, mR);
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
        painter.drawRoundedRect(sr.adjusted(inset, inset, -inset, -inset), mR, mR);
    } else if (lang == DesignCupertino) {
        // macOS: bezel push-button field. Subtle fill (bgLayerAlt) + 1px strokeStrong hairline + small
        // radius (~6) + a faint 1px lower-edge drop shadow. Pressed/open darken via the theme-aware
        // veil; open/focus gets an accent hairline. zh_CN: macOS:bezel 按钮字段。柔和填充(bgLayerAlt)+
        // 1px strokeStrong 发丝边 + 小圆角(~6) + 底缘 1px 柔和投影。按下/展开用主题感知薄层压暗;展开/聚焦
        // 使用强调色发丝边。
        const qreal mR = qMax<qreal>(0.0, qMin<qreal>(radius.control, 6.0));
        QColor fill = isEnabled() ? colors.bgLayerAlt : colors.controlDisabled;

        QPainterPath bgPath;
        bgPath.addRoundedRect(sr.adjusted(0.5, 0.5, -0.5, -0.5), mR, mR);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawPath(bgPath);

        if (isEnabled()) {
            QColor stateLayer = Qt::transparent;
            if (isDown() || m_dropDownOpen) stateLayer = veil(darkTheme ? 0x3A : 0x30);
            else if (underMouse()) stateLayer = veil(darkTheme ? 0x1C : 0x16);
            // Guard isValid() too: a default-constructed QColor reports alpha()==255, so a bare
            // alpha()>0 check + setBrush(invalid) would paint SOLID BLACK over the bezel at rest.
            // zh_CN: 必须同时判 isValid():默认构造 QColor 的 alpha() 返回 255,裸 alpha()>0 + setBrush(无效色)
            // 会在静息把 bezel 涂成纯黑。
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
            painter.drawRoundedRect(sr.adjusted(1.0, 2.0, -1.0, -1.0), mR, mR);
            painter.restore();
        }

        const QColor hairline = !isEnabled() ? colors.strokeSecondary
                                             : (fieldFocused ? colors.accentDefault : colors.strokeStrong);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(hairline, 1.0));
        painter.drawRoundedRect(sr.adjusted(0.5, 0.5, -0.5, -0.5), mR, mR);
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

        QColor segmentTextColor = isEnabled()
            ? (active ? colors.textPrimary : colors.textSecondary)
            : colors.textDisabled;
        painter.setPen(segmentTextColor);
        painter.setFont(themeFont(Typography::FontRole::Body).toQFont());
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
        (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        openPicker();
        event->accept();
        return;
    }
    fluent::basicinput::Button::keyPressEvent(event);
}

void DatePicker::changeEvent(QEvent* event)
{
    fluent::basicinput::Button::changeEvent(event);
    if (event->type() == QEvent::EnabledChange) {
        if (!isEnabled())
            closePicker();
        update();
    }
}

void DatePicker::onThemeUpdated()
{
    if (m_flyout)
        m_flyout->onThemeUpdated();
    update();
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
        switch (field) {
        case DateField::Month:
            return monthColumnWidth(m_monthFormat);
        case DateField::Day:
            return dayColumnWidth(m_dayFormat);
        case DateField::Year:
            return yearColumnWidth(m_yearFormat);
        }
        return 96;
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
    return QRect(0, 0, width(), kEntryHeight);
}

QString DatePicker::formatField(DateField field, const QDate& date) const
{
    if (!date.isValid())
        return QString();

    switch (field) {
    case DateField::Month:
        switch (m_monthFormat) {
        case MonthFormat::FullMonthName:
            return m_locale.monthName(date.month(), QLocale::LongFormat);
        case MonthFormat::AbbreviatedMonthName:
            return m_locale.monthName(date.month(), QLocale::ShortFormat);
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
                .arg(m_locale.dayName(date.dayOfWeek(), QLocale::ShortFormat));
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
    emit dropDownOpenChanged(m_dropDownOpen);
    update();
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
