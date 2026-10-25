#include "components/foundation/private/TextInputFrame_p.h"
#include "components/foundation/private/ControlMetrics_p.h"
#include "compatibility/InputEventCompat.h"
#include "NumberBox.h"

#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPointer>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QValidator>

#include <cmath>
#include <limits>

#include "components/basicinput/RepeatButton.h"
#include "components/foundation/private/ValueAccessibility_p.h"
#include "components/textfields/private/NumberBoxAccessibility_p.h"
#include "design/Spacing.h"
#include "design/Typography.h"

namespace fluent::textfields {

namespace {

constexpr double kNumberEpsilon = 1e-12;

bool isNan(double value)
{
    return std::isnan(value);
}

bool isFiniteNumber(double value)
{
    return std::isfinite(value);
}

bool numbersEqual(double left, double right)
{
    if (isNan(left) && isNan(right))
        return true;
    if (left == right)
        return true;
    if (!isFiniteNumber(left) || !isFiniteNumber(right))
        return false;
    // Keep roundoff tolerance small even when adjacent representable values are large.
    // zh_CN: 限制容差上限，避免大数值的相邻可表示变化漏发通知。
    const double scale = qMin(1.0, qMax(std::abs(left), std::abs(right)));
    return std::abs(left - right) <= 4 * std::numeric_limits<double>::epsilon() * scale;
}

class ExpressionParser {
public:
    explicit ExpressionParser(const QString& text) : m_text(text) {}

    bool parse(double* result)
    {
        if (!result)
            return false;
        m_pos = 0;
        double value = 0.0;
        if (!parseAddSub(value))
            return false;
        skipSpaces();
        if (m_pos != m_text.size() || !isFiniteNumber(value))
            return false;
        *result = value;
        return true;
    }

private:
    // Bound nested parentheses and right-associative powers independently of input length.
    // zh_CN: 限制括号和右结合幂运算的嵌套深度，避免长输入耗尽调用栈。
    static constexpr int kMaximumDepth = 128;

    bool parseAddSub(double& value, int depth = 0)
    {
        if (depth > kMaximumDepth || !parseMulDiv(value, depth))
            return false;
        while (true) {
            skipSpaces();
            if (match('+')) {
                double rhs = 0.0;
                if (!parseMulDiv(rhs, depth))
                    return false;
                value += rhs;
            } else if (match('-')) {
                double rhs = 0.0;
                if (!parseMulDiv(rhs, depth))
                    return false;
                value -= rhs;
            } else {
                return true;
            }
        }
    }

    bool parseMulDiv(double& value, int depth)
    {
        if (!parsePower(value, depth))
            return false;
        while (true) {
            skipSpaces();
            if (match('*')) {
                double rhs = 0.0;
                if (!parsePower(rhs, depth))
                    return false;
                value *= rhs;
            } else if (match('/')) {
                double rhs = 0.0;
                if (!parsePower(rhs, depth) || std::abs(rhs) <= kNumberEpsilon)
                    return false;
                value /= rhs;
            } else {
                return true;
            }
        }
    }

    bool parsePower(double& value, int depth)
    {
        if (depth > kMaximumDepth || !parseUnary(value, depth))
            return false;
        skipSpaces();
        if (!match('^'))
            return true;

        double exponent = 0.0;
        if (!parsePower(exponent, depth + 1))
            return false;
        value = std::pow(value, exponent);
        return isFiniteNumber(value);
    }

    bool parseUnary(double& value, int depth)
    {
        bool negative = false;
        while (true) {
            skipSpaces();
            if (match('+'))
                continue;
            if (match('-'))
                negative = !negative;
            else
                break;
        }
        if (!parsePrimary(value, depth))
            return false;
        if (negative)
            value = -value;
        return true;
    }

    bool parsePrimary(double& value, int depth)
    {
        skipSpaces();
        if (match('(')) {
            if (!parseAddSub(value, depth + 1))
                return false;
            skipSpaces();
            return match(')');
        }
        return parseNumber(value);
    }

    bool parseNumber(double& value)
    {
        skipSpaces();
        const int start = m_pos;
        bool sawDigit = false;

        while (m_pos < m_text.size() && m_text.at(m_pos).isDigit()) {
            sawDigit = true;
            ++m_pos;
        }
        if (m_pos < m_text.size() && m_text.at(m_pos) == '.') {
            ++m_pos;
            while (m_pos < m_text.size() && m_text.at(m_pos).isDigit()) {
                sawDigit = true;
                ++m_pos;
            }
        }
        if (!sawDigit)
            return false;

        if (m_pos < m_text.size() && (m_text.at(m_pos) == 'e' || m_text.at(m_pos) == 'E')) {
            const int exponentStart = m_pos;
            ++m_pos;
            if (m_pos < m_text.size() && (m_text.at(m_pos) == '+' || m_text.at(m_pos) == '-'))
                ++m_pos;
            bool sawExponentDigit = false;
            while (m_pos < m_text.size() && m_text.at(m_pos).isDigit()) {
                sawExponentDigit = true;
                ++m_pos;
            }
            if (!sawExponentDigit)
                m_pos = exponentStart;
        }

        bool ok = false;
        value = QLocale::c().toDouble(m_text.mid(start, m_pos - start), &ok);
        return ok && isFiniteNumber(value);
    }

    void skipSpaces()
    {
        while (m_pos < m_text.size() && m_text.at(m_pos).isSpace())
            ++m_pos;
    }

    bool match(QChar value)
    {
        if (m_pos >= m_text.size() || m_text.at(m_pos) != value)
            return false;
        ++m_pos;
        return true;
    }

    QString m_text;
    int m_pos = 0;
};

} // namespace

NumberBox::NumberBox(QWidget* parent)
    : LineEdit(parent), m_value(std::numeric_limits<double>::quiet_NaN()),
      m_minimum(-std::numeric_limits<double>::infinity()), m_maximum(std::numeric_limits<double>::infinity())
{
    detail::ensureNumberBoxAccessibilityFactory();
    setAttribute(Qt::WA_Hover);
    setFixedHeight(totalPreferredHeight());

    initializeSpinnerButtons();
    updateHeaderTextMargins();
    updateTextMarginsForChrome();
    updateSpinnerState();

    connect(this, &QLineEdit::editingFinished, this, [this]() { commitInput(); });
}

void NumberBox::setValue(double value)
{
    const QPointer<NumberBox> guard(this);
    cancelScrub();
    if (!guard)
        return;
    setValueInternal(value, true, false);
}

void NumberBox::setMinimum(double minimum)
{
    if (isNan(minimum) || numbersEqual(m_minimum, minimum))
        return;
    setRange(minimum, m_maximum);
}

void NumberBox::setMaximum(double maximum)
{
    if (isNan(maximum) || numbersEqual(m_maximum, maximum))
        return;
    setRange(m_minimum, maximum);
}

void NumberBox::setRange(double minimum, double maximum)
{
    if (isNan(minimum) || isNan(maximum))
        return;
    if (maximum < minimum)
        maximum = minimum;

    const double oldMinimum = m_minimum;
    const double oldMaximum = m_maximum;
    m_minimum = minimum;
    m_maximum = maximum;

    const QPointer<NumberBox> guard(this);
    const bool valueChangedNow = !isNan(m_value) && setValueInternal(m_value, true, false);
    if (!guard || !numbersEqual(m_minimum, minimum) || !numbersEqual(m_maximum, maximum))
        return;
    const bool minimumChangedNow = !numbersEqual(oldMinimum, m_minimum);
    const bool maximumChangedNow = !numbersEqual(oldMaximum, m_maximum);

    if (!minimumChangedNow && !maximumChangedNow && !valueChangedNow)
        return;
    updateSpinnerState();
    if (!guard)
        return;
    if (!valueChangedNow) {
        accessibility::detail::notifyValueAccessibilityValue(this, isNan(m_value) ? QVariant() : QVariant(m_value));
        if (!guard)
            return;
    }
    if (minimumChangedNow)
        emit minimumChanged(m_minimum);
    if (!guard || !numbersEqual(m_minimum, minimum) || !numbersEqual(m_maximum, maximum))
        return;
    if (maximumChangedNow)
        emit maximumChanged(m_maximum);
}

void NumberBox::setSmallChange(double change)
{
    if (!(change > 0.0) || !isFiniteNumber(change) || numbersEqual(m_smallChange, change))
        return;
    m_smallChange = change;
    const QPointer<NumberBox> guard(this);
    accessibility::detail::notifyValueAccessibilityValue(this, isNan(m_value) ? QVariant() : QVariant(m_value));
    if (!guard || !numbersEqual(m_smallChange, change))
        return;
    emit smallChangeChanged(m_smallChange);
}

void NumberBox::setLargeChange(double change)
{
    if (!(change > 0.0) || !isFiniteNumber(change) || numbersEqual(m_largeChange, change))
        return;
    m_largeChange = change;
    emit largeChangeChanged(m_largeChange);
}

void NumberBox::setHeader(const QString& header)
{
    if (m_header == header)
        return;
    m_header = header;
    setFixedHeight(totalPreferredHeight());
    updateHeaderTextMargins();
    updateChildGeometry();
    updateGeometry();
    update();
    const QPointer<NumberBox> guard(this);
    accessibility::detail::notifyValueAccessibilityText(this, QAccessible::NameChanged);
    if (guard)
        emit headerChanged();
}

void NumberBox::setAcceptsExpression(bool accepts)
{
    if (m_acceptsExpression == accepts)
        return;
    m_acceptsExpression = accepts;
    emit acceptsExpressionChanged(m_acceptsExpression);
}

void NumberBox::setSpinButtonPlacementMode(SpinButtonPlacementMode mode)
{
    if (m_spinButtonPlacementMode == mode)
        return;
    m_spinButtonPlacementMode = mode;
    updateSpinnerState();
    updateTextMarginsForChrome();
    updateChildGeometry();
    update();
    emit spinButtonPlacementModeChanged(m_spinButtonPlacementMode);
}

void NumberBox::setSpinButtonSize(const QSize& size)
{
    if (size.width() <= 0 || size.height() <= 0 || m_spinButtonSize == size)
        return;
    m_spinButtonSize = size;
    updateTextMarginsForChrome();
    updateChildGeometry();
    update();
    emit spinButtonSizeChanged(m_spinButtonSize);
}

void NumberBox::setInlineSpinButtonSize(const QSize& size)
{
    if (size.width() <= 0 || size.height() <= 0 || m_inlineSpinButtonSize == size)
        return;
    m_inlineSpinButtonSize = size;
    updateTextMarginsForChrome();
    updateChildGeometry();
    update();
    emit inlineSpinButtonSizeChanged(m_inlineSpinButtonSize);
}

void NumberBox::setSpinButtonRightMargin(int margin)
{
    const int normalized = qMax(0, margin);
    if (m_spinButtonRightMargin == normalized)
        return;
    m_spinButtonRightMargin = normalized;
    updateTextMarginsForChrome();
    updateChildGeometry();
    update();
    emit spinButtonRightMarginChanged(m_spinButtonRightMargin);
}

void NumberBox::setCompactSpinButtonReservedWidth(int width)
{
    const int normalized = qMax(0, width);
    if (m_compactSpinButtonReservedWidth == normalized)
        return;
    m_compactSpinButtonReservedWidth = normalized;
    updateTextMarginsForChrome();
    update();
    emit compactSpinButtonReservedWidthChanged(m_compactSpinButtonReservedWidth);
}

void NumberBox::setSpinButtonSpacing(int spacing)
{
    const int normalized = qMax(0, spacing);
    if (m_spinButtonSpacing == normalized)
        return;
    m_spinButtonSpacing = normalized;
    updateTextMarginsForChrome();
    updateChildGeometry();
    update();
    emit spinButtonSpacingChanged(m_spinButtonSpacing);
}

void NumberBox::setSpinButtonTextGap(int gap)
{
    const int normalized = qMax(0, gap);
    if (m_spinButtonTextGap == normalized)
        return;
    m_spinButtonTextGap = normalized;
    updateTextMarginsForChrome();
    update();
    emit spinButtonTextGapChanged(m_spinButtonTextGap);
}

void NumberBox::setSpinButtonIconSize(int size)
{
    if (size <= 0 || m_spinButtonIconSize == size)
        return;
    m_spinButtonIconSize = size;
    updateSpinButtonIcons();
    update();
    emit spinButtonIconSizeChanged(m_spinButtonIconSize);
}

void NumberBox::setDisplayPrecision(int precision)
{
    const int normalized = qMax(-1, precision);
    if (m_displayPrecision == normalized)
        return;
    m_displayPrecision = normalized;
    const QPointer<NumberBox> guard(this);
    if (!isNan(m_value))
        setFormattedText(formatValue(m_value));
    if (guard && m_displayPrecision == normalized)
        emit displayPrecisionChanged(m_displayPrecision);
}

void NumberBox::setFormatStep(double step)
{
    const double normalized = (step > 0.0 && isFiniteNumber(step)) ? step : 0.0;
    if (numbersEqual(m_formatStep, normalized))
        return;
    m_formatStep = normalized;
    const QPointer<NumberBox> guard(this);
    if (!isNan(m_value))
        setValueInternal(m_value, true, false);
    if (guard && numbersEqual(m_formatStep, normalized))
        emit formatStepChanged(m_formatStep);
}

void NumberBox::setScrubbingEnabled(bool enabled)
{
    if (m_scrubbingEnabled == enabled)
        return;
    m_scrubbingEnabled = enabled;
    const QPointer<NumberBox> guard(this);
    if (!enabled)
        cancelScrub();
    if (!guard)
        return;
    setMouseTracking(enabled);
    if (!enabled)
        unsetCursor();
    updateTextMarginsForChrome();
    update();
    emit scrubbingEnabledChanged(enabled);
}

void NumberBox::setScrubSensitivity(double sensitivity)
{
    if (!std::isfinite(sensitivity) || sensitivity <= 0 || numbersEqual(m_scrubSensitivity, sensitivity))
        return;
    m_scrubSensitivity = sensitivity;
    emit scrubSensitivityChanged(sensitivity);
}

QRect NumberBox::scrubHandleRect() const
{
    return m_scrubbingEnabled ? QRect(4, inputTop() + 4, 24, inputRect().height() - 8) : QRect();
}

void NumberBox::cancelScrub()
{
    finishScrub(true);
}

void NumberBox::finishScrub(bool canceled)
{
    if (!m_scrubbing)
        return;
    m_scrubbing = false;
    const double initial = m_scrubInitialValue;
    const QString initialText = m_scrubInitialText;
    const QPointer<NumberBox> guard(this);
    if (canceled) {
        setValueInternal(initial, false, true);
        if (!guard)
            return;
        if (numbersEqual(m_value, normalizeValue(initial)))
            setFormattedText(initialText);
        if (!guard)
            return;
    }
    unsetCursor();
    update();
    const double finalValue = m_value;
    emit scrubbingChanged(false);
    if (guard)
        emit scrubFinished(initial, finalValue, canceled);
}

bool NumberBox::event(QEvent* event)
{
    if (event && m_scrubbing
        && (event->type() == QEvent::Hide || event->type() == QEvent::WindowDeactivate
            || event->type() == QEvent::UngrabMouse)) {
        const QPointer<NumberBox> guard(this);
        cancelScrub();
        if (!guard)
            return true;
    }
    return LineEdit::event(event);
}

QSize NumberBox::sizeHint() const
{
    QSize hint = LineEdit::sizeHint();
    hint.setHeight(totalPreferredHeight());
    hint.setWidth(qMax(hint.width(), kMinimumWidth));
    return hint;
}

QSize NumberBox::minimumSizeHint() const
{
    QSize hint = sizeHint();
    hint.setWidth(qMax(hint.width(), kMinimumWidth));
    return hint;
}

void NumberBox::onThemeUpdated()
{
    LineEdit::onThemeUpdated();
    if (m_spinUpButton)
        m_spinUpButton->onThemeUpdated();
    if (m_spinDownButton)
        m_spinDownButton->onThemeUpdated();
    update();
}

void NumberBox::paintEvent(QPaintEvent* event)
{
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (!m_header.isEmpty())
            paintHeader(painter);
    }
    LineEdit::paintEvent(event);
    if (m_scrubbingEnabled) {
        QPainter painter(this);
        const auto& colors = themeColorsRef();
        painter.setPen(QPen(isEnabled() && !isReadOnly() ? colors.textSecondary : colors.textDisabled, 1.5));
        const QPoint center = scrubHandleRect().center();
        painter.drawLine(center + QPoint(-6, 0), center + QPoint(6, 0));
        painter.drawLine(center + QPoint(-6, 0), center + QPoint(-3, -3));
        painter.drawLine(center + QPoint(-6, 0), center + QPoint(-3, 3));
        painter.drawLine(center + QPoint(6, 0), center + QPoint(3, -3));
        painter.drawLine(center + QPoint(6, 0), center + QPoint(3, 3));
    }
}

void NumberBox::paintFrame(QPainter& painter)
{
    paintInputFrame(painter);
}

void NumberBox::resizeEvent(QResizeEvent* event)
{
    LineEdit::resizeEvent(event);
    updateChildGeometry();
}

void NumberBox::focusInEvent(QFocusEvent* event)
{
    m_focused = true;
    LineEdit::focusInEvent(event);
    update();
}

void NumberBox::focusOutEvent(QFocusEvent* event)
{
    const QPointer<NumberBox> guard(this);
    const bool canceled = m_scrubbing;
    cancelScrub();
    if (!guard)
        return;
    if (!canceled)
        commitInput();
    if (!guard)
        return;
    m_focused = false;
    LineEdit::focusOutEvent(event);
    if (guard)
        update();
}

void NumberBox::enterEvent(FluentEnterEvent* event)
{
    m_hovered = true;
    LineEdit::enterEvent(event);
    update();
}

void NumberBox::leaveEvent(QEvent* event)
{
    m_hovered = false;
    LineEdit::leaveEvent(event);
    update();
}

void NumberBox::mousePressEvent(QMouseEvent* event)
{
    if (event && event->button() == Qt::LeftButton && isEnabled() && !isReadOnly()
        && scrubHandleRect().contains(event->pos())) {
        const QPointer<NumberBox> guard(this);
        setFocus(Qt::MouseFocusReason);
        if (!guard)
            return;
        m_scrubInitialValue = m_value;
        m_scrubInitialText = text();
        m_scrubBase = std::isfinite(m_value) ? m_value : normalizedStepStart();
        m_scrubStart = fluentMouseGlobalPos(event);
        m_scrubbing = true;
        setCursor(Qt::SizeHorCursor);
        event->accept();
        emit scrubbingChanged(true);
        if (guard && m_scrubbing)
            emit scrubStarted(m_scrubInitialValue);
        return;
    }
    if (event && event->button() == Qt::LeftButton && isEnabled() && !isReadOnly()) {
        m_pressed = inputRect().contains(event->pos());
        update();
    }
    LineEdit::mousePressEvent(event);
}

void NumberBox::mouseMoveEvent(QMouseEvent* event)
{
    if (m_scrubbing) {
        const QPoint position = fluentMouseGlobalPos(event);
        double factor = event->modifiers().testFlag(Qt::ShiftModifier) ? 0.1 : 1.0;
        if (event->modifiers().testFlag(Qt::ControlModifier))
            factor *= 10.0;
        const double limit = std::numeric_limits<double>::max();
        const double lower = qMax(-limit, m_minimum), upper = qMin(limit, m_maximum);
        const double distance = double(position.x()) - double(m_scrubStart.x());
        m_scrubBase = qBound(lower, m_scrubBase + distance * m_scrubSensitivity * factor, upper);
        m_scrubStart = position;
        event->accept();
        setValueInternal(m_scrubBase, true, false);
        return;
    }
    if (m_scrubbingEnabled && isEnabled() && !isReadOnly() && scrubHandleRect().contains(event->pos()))
        setCursor(Qt::SizeHorCursor);
    else if (m_scrubbingEnabled)
        unsetCursor();
    LineEdit::mouseMoveEvent(event);
}

void NumberBox::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_scrubbing && event->button() == Qt::LeftButton) {
        event->accept();
        finishScrub(false);
        return;
    }
    if (m_pressed) {
        m_pressed = false;
        update();
    }
    LineEdit::mouseReleaseEvent(event);
}

void NumberBox::keyPressEvent(QKeyEvent* event)
{
    if (!event) {
        LineEdit::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Escape:
        if (m_scrubbing) {
            event->accept();
            cancelScrub();
            return;
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        commitInput();
        event->accept();
        return;
    case Qt::Key_Up:
        stepBy(m_smallChange);
        event->accept();
        return;
    case Qt::Key_Down:
        stepBy(-m_smallChange);
        event->accept();
        return;
    case Qt::Key_PageUp:
        stepBy(m_largeChange);
        event->accept();
        return;
    case Qt::Key_PageDown:
        stepBy(-m_largeChange);
        event->accept();
        return;
    default:
        break;
    }
    LineEdit::keyPressEvent(event);
}

void NumberBox::changeEvent(QEvent* event)
{
    LineEdit::changeEvent(event);
    if (!event)
        return;
    if (event->type() == QEvent::FontChange) {
        setFixedHeight(totalPreferredHeight());
        updateChildGeometry();
        updateGeometry();
    }
    if (event->type() == QEvent::EnabledChange || event->type() == QEvent::ReadOnlyChange) {
        const QPointer<NumberBox> guard(this);
        if (!isEnabled() || isReadOnly())
            cancelScrub();
        if (!guard)
            return;
        updateSpinnerState();
        updateTextMarginsForChrome();
        update();
        QAccessible::State changed;
        changed.disabled = true;
        changed.readOnly = true;
        changed.editable = true;
        accessibility::detail::notifyValueAccessibilityState(this, changed);
    }
}

bool NumberBox::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_spinUpButton || watched == m_spinDownButton) && event) {
        switch (event->type()) {
        case QEvent::Enter:
            m_spinnerHovered = true;
            break;
        case QEvent::Leave:
            m_spinnerHovered = (m_spinUpButton && m_spinUpButton->underMouse())
                || (m_spinDownButton && m_spinDownButton->underMouse());
            break;
        case QEvent::MouseButtonPress:
            m_spinnerPressed = true;
            break;
        case QEvent::MouseButtonRelease:
            m_spinnerPressed = false;
            setFocus(Qt::MouseFocusReason);
            break;
        default:
            return LineEdit::eventFilter(watched, event);
        }
        update();
    }
    return LineEdit::eventFilter(watched, event);
}

QRect NumberBox::inputRect() const
{
    return QRect(0, inputTop(), width(), ::fluent::detail::standardControlHeight(fontMetrics()));
}

int NumberBox::inputTop() const
{
    return m_header.isEmpty() ? 0 : kHeaderHeight + kHeaderGap;
}

int NumberBox::totalPreferredHeight() const
{
    return inputTop() + ::fluent::detail::standardControlHeight(fontMetrics());
}

void NumberBox::initializeSpinnerButtons()
{
    m_spinUpButton = new ::fluent::basicinput::RepeatButton(this);
    m_spinDownButton = new ::fluent::basicinput::RepeatButton(this);

    auto configureButton =
        [this](::fluent::basicinput::RepeatButton* button, const QString& glyph, const QString& objectName) {
            button->setObjectName(objectName);
            button->setFluentStyle(::fluent::basicinput::Button::Standard);
            button->setFluentLayout(::fluent::basicinput::Button::IconOnly);
            button->setFluentSize(::fluent::basicinput::Button::Small);
            button->setFocusPolicy(Qt::NoFocus);
            button->setFixedSize(m_spinButtonSize);
            button->setIconGlyph(glyph, m_spinButtonIconSize, Typography::FontFamily::FluentIcons);
            button->installEventFilter(this);
            button->hide();
        };

    configureButton(m_spinUpButton, Typography::Icons::ChevronUp, QStringLiteral("NumberBoxSpinUpButton"));
    configureButton(m_spinDownButton, Typography::Icons::ChevronDown, QStringLiteral("NumberBoxSpinDownButton"));
    m_spinUpButton->setAccessibleName(tr("Increase value"));
    m_spinDownButton->setAccessibleName(tr("Decrease value"));

    connect(m_spinUpButton, &::fluent::basicinput::RepeatButton::clicked, this, [this]() {
        setFocus(Qt::MouseFocusReason);
        stepBy(m_smallChange);
    });
    connect(m_spinDownButton, &::fluent::basicinput::RepeatButton::clicked, this, [this]() {
        setFocus(Qt::MouseFocusReason);
        stepBy(-m_smallChange);
    });

    updateChildGeometry();
}

void NumberBox::updateSpinButtonIcons()
{
    if (m_spinUpButton) {
        m_spinUpButton->setIconGlyph(Typography::Icons::ChevronUp, m_spinButtonIconSize,
            Typography::FontFamily::FluentIcons);
    }
    if (m_spinDownButton) {
        m_spinDownButton->setIconGlyph(Typography::Icons::ChevronDown, m_spinButtonIconSize,
            Typography::FontFamily::FluentIcons);
    }
}

void NumberBox::updateChildGeometry()
{
    if (!m_spinUpButton || !m_spinDownButton)
        return;
    const QRect input = inputRect();

    if (m_spinButtonPlacementMode == SpinButtonPlacementMode::Inline) {
        m_spinUpButton->setFixedSize(m_inlineSpinButtonSize);
        m_spinDownButton->setFixedSize(m_inlineSpinButtonSize);
        const int y = input.top() + qMax(0, (input.height() - m_inlineSpinButtonSize.height()) / 2);
        const int x = input.right() - m_spinButtonRightMargin - inlineSpinnerWidth() + 1;
        m_spinDownButton->move(x, y);
        m_spinUpButton->move(x + m_inlineSpinButtonSize.width() + m_spinButtonSpacing, y);
    } else {
        m_spinUpButton->setFixedSize(m_spinButtonSize);
        m_spinDownButton->setFixedSize(m_spinButtonSize);
        const int stackHeight = m_spinButtonSize.height() * 2;
        const int y = input.top() + qMax(0, (input.height() - stackHeight) / 2);
        const int x = input.right() - m_spinButtonRightMargin - compactExpandedSpinnerWidth() + 1;
        m_spinUpButton->move(x, y);
        m_spinDownButton->move(x, y + m_spinButtonSize.height());
    }

    setClearButtonOffset(QPoint(::Spacing::XSmall, inputTop() / 2));
}

void NumberBox::updateSpinnerState()
{
    const bool spinnerMode = m_spinButtonPlacementMode != SpinButtonPlacementMode::Hidden;
    setClearButtonEnabled(!spinnerMode);

    const bool visible = hasSpinnerButtonsVisible();
    const bool enabled = visible && isEnabled() && !isReadOnly();
    const bool atMinimum = !isNan(m_value) && (m_value <= m_minimum || numbersEqual(m_value, m_minimum));
    const bool atMaximum = !isNan(m_value) && (m_value >= m_maximum || numbersEqual(m_value, m_maximum));
    if (m_spinUpButton) {
        m_spinUpButton->setVisible(visible);
        m_spinUpButton->setEnabled(enabled && !atMaximum);
    }
    if (m_spinDownButton) {
        m_spinDownButton->setVisible(visible);
        m_spinDownButton->setEnabled(enabled && !atMinimum);
    }
    updateChildGeometry();
}

void NumberBox::updateTextMarginsForChrome()
{
    int rightMargin = ::Spacing::Padding::TextFieldHorizontal;
    if (m_spinButtonPlacementMode == SpinButtonPlacementMode::Inline) {
        rightMargin += m_spinButtonRightMargin + inlineSpinnerWidth() + m_spinButtonTextGap;
    } else if (m_spinButtonPlacementMode == SpinButtonPlacementMode::Compact) {
        rightMargin += m_spinButtonRightMargin + compactExpandedSpinnerWidth() + m_spinButtonTextGap;
    }

    QMargins margins = contentMargins();
    margins.setLeft(::Spacing::Padding::TextFieldHorizontal + (m_scrubbingEnabled ? 24 : 0));
    margins.setRight(rightMargin);
    margins.setTop(::Spacing::Padding::TextFieldVertical);
    margins.setBottom(::Spacing::Padding::TextFieldVertical);
    setContentMargins(margins);
}

void NumberBox::updateHeaderTextMargins()
{
    setTextMargins(0, inputTop(), 0, 0);
}

void NumberBox::commitInput()
{
    if (!isModified() && text() == formatValue(m_value))
        return;
    const QString trimmed = text().trimmed();
    if (trimmed.isEmpty()) {
        setInvalidValueFromText();
        return;
    }

    double parsed = 0.0;
    if (!parseInputText(trimmed, &parsed)) {
        setInvalidValueFromText();
        return;
    }

    setValueInternal(parsed, true, false);
}

void NumberBox::setInvalidValueFromText()
{
    setValueInternal(std::numeric_limits<double>::quiet_NaN(), false, true);
}

void NumberBox::stepBy(double delta)
{
    if (!isEnabled() || isReadOnly())
        return;
    const double base = isNan(m_value) ? normalizedStepStart() : m_value;
    setValueInternal(base + delta, true, false);
}

double NumberBox::normalizedStepStart() const
{
    if (m_minimum <= 0.0 && m_maximum >= 0.0)
        return 0.0;
    if (m_minimum > 0.0)
        return m_minimum;
    if (m_maximum < 0.0)
        return m_maximum;
    return 0.0;
}

double NumberBox::normalizeValue(double value) const
{
    if (isNan(value) || !isFiniteNumber(value))
        return std::numeric_limits<double>::quiet_NaN();
    value = applyFormatStep(value);
    if (value < m_minimum)
        value = m_minimum;
    if (value > m_maximum)
        value = m_maximum;
    return value;
}

double NumberBox::applyFormatStep(double value) const
{
    if (!(m_formatStep > 0.0) || !isFiniteNumber(m_formatStep))
        return value;
    return std::floor(value / m_formatStep + 0.5 + kNumberEpsilon) * m_formatStep;
}

QString NumberBox::formatValue(double value) const
{
    if (isNan(value))
        return QString();
    if (m_displayPrecision >= 0)
        return QLocale::c().toString(value, 'f', m_displayPrecision);
    return QString::number(value, 'g', 15);
}

bool NumberBox::parseInputText(const QString& input, double* result) const
{
    if (!result)
        return false;
    if (m_acceptsExpression) {
        ExpressionParser parser(input);
        return parser.parse(result);
    }

    bool ok = false;
    const double parsed = QLocale::c().toDouble(input, &ok);
    if (!ok || !isFiniteNumber(parsed))
        return false;
    *result = parsed;
    return true;
}

void NumberBox::setFormattedText(const QString& value)
{
    const QString oldText = text();
    const int oldCursor = cursorPosition();
    const bool hadSelection = hasSelectedText();
    const bool blocked = signalsBlocked();
    // Finish Qt's text mutation before user callbacks can destroy its line control.
    // zh_CN: 先完成 Qt 文本赋值，再通知外部回调，避免回调销毁仍在执行的 line control。
    {
        const QSignalBlocker blocker(this);
        QLineEdit::setText(value);
    }
    if (blocked)
        return;

    const QPointer<NumberBox> guard(this);
    const QString updatedText = text();
    const int updatedCursor = cursorPosition();
    if (const QValidator* inputValidator = oldText != updatedText ? validator() : nullptr) {
        QString validatedText = updatedText;
        int validatedCursor = updatedCursor;
        if (inputValidator->validate(validatedText, validatedCursor) == QValidator::Invalid)
            emit inputRejected();
        if (!guard || text() != updatedText)
            return;
    }
    if (oldText != updatedText)
        emit textChanged(updatedText);
    if (!guard || text() != updatedText)
        return;
    if (hadSelection && !hasSelectedText())
        emit selectionChanged();
    if (!guard || text() != updatedText || cursorPosition() != updatedCursor)
        return;
    if (oldCursor != updatedCursor)
        emit cursorPositionChanged(oldCursor, updatedCursor);
}

bool NumberBox::setValueInternal(double value, bool updateText, bool keepUserTextWhenNaN)
{
    const double normalized = normalizeValue(value);
    const bool wasInvalid = isNan(m_value);
    const bool changed = !numbersEqual(m_value, normalized);
    m_value = normalized;

    const QPointer<NumberBox> guard(this);
    const auto isCurrent = [&]() { return guard && numbersEqual(m_value, normalized); };
    if (updateText && !(keepUserTextWhenNaN && isNan(m_value)))
        setFormattedText(formatValue(m_value));
    // textChanged can destroy this control or commit another value synchronously.
    // zh_CN: textChanged 回调可能同步销毁控件或提交新值。
    if (!isCurrent())
        return changed;
    if (changed) {
        updateSpinnerState();
        if (!isCurrent())
            return changed;
        accessibility::detail::notifyValueAccessibilityValue(this, isNan(m_value) ? QVariant() : QVariant(m_value));
        if (!isCurrent())
            return changed;
        if (wasInvalid != isNan(m_value)) {
            QAccessible::State changedState;
            changedState.invalid = true;
            accessibility::detail::notifyValueAccessibilityState(this, changedState);
            if (!isCurrent())
                return changed;
        }
        emit valueChanged(m_value);
    }
    return changed;
}

void NumberBox::paintInputFrame(QPainter& painter)
{
    ::fluent::detail::TextInputFrameState state;
    state.enabled = isEnabled();
    state.readOnly = isReadOnly();
    state.focused = m_focused;
    state.hovered = m_hovered || m_spinnerHovered;
    state.pressed = m_pressed || m_spinnerPressed;
    state.dark = effectiveThemeUsesDarkAppearance();
    ::fluent::detail::paintTextInputFrame(painter, QRectF(inputRect()), themeColorsRef(), themeRadius().control, state);
}

void NumberBox::paintHeader(QPainter& painter)
{
    if (m_header.isEmpty())
        return;
    painter.setFont(themeFont(Typography::FontRole::Body).toQFont());
    const auto& colors = themeColorsRef();
    painter.setPen(isEnabled() ? colors.textPrimary : colors.textDisabled);
    const QRect headerRect(0, 0, width(), kHeaderHeight);
    painter.drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter,
        painter.fontMetrics().elidedText(m_header, Qt::ElideRight, headerRect.width()));
}

bool NumberBox::hasSpinnerButtonsVisible() const
{
    return m_spinButtonPlacementMode != SpinButtonPlacementMode::Hidden;
}

int NumberBox::inlineSpinnerWidth() const
{
    return m_inlineSpinButtonSize.width() * 2 + m_spinButtonSpacing;
}

int NumberBox::compactExpandedSpinnerWidth() const
{
    return m_spinButtonSize.width();
}

} // namespace fluent::textfields
