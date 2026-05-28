#include "NumberBox.h"

#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>

#include <cmath>
#include <limits>

#include "design/Spacing.h"
#include "design/Typography.h"
#include "view/basicinput/RepeatButton.h"

namespace view::textfields {

namespace {

constexpr double kNumberEpsilon = 1e-12;

bool isNan(double value) {
    return std::isnan(value);
}

bool isFiniteNumber(double value) {
    return std::isfinite(value);
}

bool numbersEqual(double left, double right) {
    if (isNan(left) && isNan(right)) return true;
    if (left == right) return true;
    if (!isFiniteNumber(left) || !isFiniteNumber(right)) return false;
    return std::abs(left - right) <= kNumberEpsilon;
}

class ExpressionParser {
public:
    explicit ExpressionParser(const QString& text)
        : m_text(text) {}

    bool parse(double* result) {
        if (!result) return false;
        m_pos = 0;
        double value = 0.0;
        if (!parseAddSub(value)) return false;
        skipSpaces();
        if (m_pos != m_text.size() || !isFiniteNumber(value)) return false;
        *result = value;
        return true;
    }

private:
    bool parseAddSub(double& value) {
        if (!parseMulDiv(value)) return false;
        while (true) {
            skipSpaces();
            if (match('+')) {
                double rhs = 0.0;
                if (!parseMulDiv(rhs)) return false;
                value += rhs;
            } else if (match('-')) {
                double rhs = 0.0;
                if (!parseMulDiv(rhs)) return false;
                value -= rhs;
            } else {
                return true;
            }
        }
    }

    bool parseMulDiv(double& value) {
        if (!parsePower(value)) return false;
        while (true) {
            skipSpaces();
            if (match('*')) {
                double rhs = 0.0;
                if (!parsePower(rhs)) return false;
                value *= rhs;
            } else if (match('/')) {
                double rhs = 0.0;
                if (!parsePower(rhs) || std::abs(rhs) <= kNumberEpsilon) return false;
                value /= rhs;
            } else {
                return true;
            }
        }
    }

    bool parsePower(double& value) {
        if (!parseUnary(value)) return false;
        skipSpaces();
        if (!match('^')) return true;

        double exponent = 0.0;
        if (!parsePower(exponent)) return false;
        value = std::pow(value, exponent);
        return isFiniteNumber(value);
    }

    bool parseUnary(double& value) {
        skipSpaces();
        if (match('+')) return parseUnary(value);
        if (match('-')) {
            if (!parseUnary(value)) return false;
            value = -value;
            return true;
        }
        return parsePrimary(value);
    }

    bool parsePrimary(double& value) {
        skipSpaces();
        if (match('(')) {
            if (!parseAddSub(value)) return false;
            skipSpaces();
            return match(')');
        }
        return parseNumber(value);
    }

    bool parseNumber(double& value) {
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
        if (!sawDigit) return false;

        if (m_pos < m_text.size() && (m_text.at(m_pos) == 'e' || m_text.at(m_pos) == 'E')) {
            const int exponentStart = m_pos;
            ++m_pos;
            if (m_pos < m_text.size() && (m_text.at(m_pos) == '+' || m_text.at(m_pos) == '-')) ++m_pos;
            bool sawExponentDigit = false;
            while (m_pos < m_text.size() && m_text.at(m_pos).isDigit()) {
                sawExponentDigit = true;
                ++m_pos;
            }
            if (!sawExponentDigit) m_pos = exponentStart;
        }

        bool ok = false;
        value = QLocale::c().toDouble(m_text.mid(start, m_pos - start), &ok);
        return ok && isFiniteNumber(value);
    }

    void skipSpaces() {
        while (m_pos < m_text.size() && m_text.at(m_pos).isSpace()) ++m_pos;
    }

    bool match(QChar value) {
        if (m_pos >= m_text.size() || m_text.at(m_pos) != value) return false;
        ++m_pos;
        return true;
    }

    QString m_text;
    int m_pos = 0;
};

} // namespace

NumberBox::NumberBox(QWidget* parent)
    : LineEdit(parent),
      m_value(std::numeric_limits<double>::quiet_NaN()),
      m_minimum(-std::numeric_limits<double>::infinity()),
      m_maximum(std::numeric_limits<double>::infinity()) {
    setAttribute(Qt::WA_Hover);
    setFrameVisible(false);
    setFixedHeight(totalPreferredHeight());

    initializeSpinnerButtons();
    updateHeaderTextMargins();
    updateTextMarginsForChrome();
    updateSpinnerState();

    connect(this, &QLineEdit::editingFinished, this, [this]() { commitInput(); });
}

void NumberBox::setValue(double value) {
    setValueInternal(value, true, false);
}

void NumberBox::setMinimum(double minimum) {
    if (isNan(minimum) || numbersEqual(m_minimum, minimum)) return;
    setRange(minimum, m_maximum);
}

void NumberBox::setMaximum(double maximum) {
    if (isNan(maximum) || numbersEqual(m_maximum, maximum)) return;
    setRange(m_minimum, maximum);
}

void NumberBox::setRange(double minimum, double maximum) {
    if (isNan(minimum) || isNan(maximum)) return;
    if (maximum < minimum) maximum = minimum;

    const double oldMinimum = m_minimum;
    const double oldMaximum = m_maximum;
    m_minimum = minimum;
    m_maximum = maximum;

    const bool valueChangedNow = !isNan(m_value) && setValueInternal(m_value, true, false);
    const bool minimumChangedNow = !numbersEqual(oldMinimum, m_minimum);
    const bool maximumChangedNow = !numbersEqual(oldMaximum, m_maximum);

    if (!minimumChangedNow && !maximumChangedNow && !valueChangedNow) return;
    updateSpinnerState();
    if (minimumChangedNow) emit minimumChanged(m_minimum);
    if (maximumChangedNow) emit maximumChanged(m_maximum);
}

void NumberBox::setSmallChange(double change) {
    if (!(change > 0.0) || !isFiniteNumber(change) || numbersEqual(m_smallChange, change)) return;
    m_smallChange = change;
    emit smallChangeChanged(m_smallChange);
}

void NumberBox::setLargeChange(double change) {
    if (!(change > 0.0) || !isFiniteNumber(change) || numbersEqual(m_largeChange, change)) return;
    m_largeChange = change;
    emit largeChangeChanged(m_largeChange);
}

void NumberBox::setHeader(const QString& header) {
    if (m_header == header) return;
    m_header = header;
    setFixedHeight(totalPreferredHeight());
    updateHeaderTextMargins();
    updateChildGeometry();
    updateGeometry();
    update();
    emit headerChanged();
}

void NumberBox::setAcceptsExpression(bool accepts) {
    if (m_acceptsExpression == accepts) return;
    m_acceptsExpression = accepts;
    emit acceptsExpressionChanged(m_acceptsExpression);
}

void NumberBox::setSpinButtonPlacementMode(SpinButtonPlacementMode mode) {
    if (m_spinButtonPlacementMode == mode) return;
    m_spinButtonPlacementMode = mode;
    updateSpinnerState();
    updateTextMarginsForChrome();
    updateChildGeometry();
    update();
    emit spinButtonPlacementModeChanged(m_spinButtonPlacementMode);
}

void NumberBox::setSpinButtonSize(const QSize& size) {
    if (size.width() <= 0 || size.height() <= 0 || m_spinButtonSize == size) return;
    m_spinButtonSize = size;
    updateTextMarginsForChrome();
    updateChildGeometry();
    update();
    emit spinButtonSizeChanged(m_spinButtonSize);
}

void NumberBox::setInlineSpinButtonSize(const QSize& size) {
    if (size.width() <= 0 || size.height() <= 0 || m_inlineSpinButtonSize == size) return;
    m_inlineSpinButtonSize = size;
    updateTextMarginsForChrome();
    updateChildGeometry();
    update();
    emit inlineSpinButtonSizeChanged(m_inlineSpinButtonSize);
}

void NumberBox::setSpinButtonRightMargin(int margin) {
    const int normalized = qMax(0, margin);
    if (m_spinButtonRightMargin == normalized) return;
    m_spinButtonRightMargin = normalized;
    updateTextMarginsForChrome();
    updateChildGeometry();
    update();
    emit spinButtonRightMarginChanged(m_spinButtonRightMargin);
}

void NumberBox::setCompactSpinButtonReservedWidth(int width) {
    const int normalized = qMax(0, width);
    if (m_compactSpinButtonReservedWidth == normalized) return;
    m_compactSpinButtonReservedWidth = normalized;
    updateTextMarginsForChrome();
    update();
    emit compactSpinButtonReservedWidthChanged(m_compactSpinButtonReservedWidth);
}

void NumberBox::setSpinButtonSpacing(int spacing) {
    const int normalized = qMax(0, spacing);
    if (m_spinButtonSpacing == normalized) return;
    m_spinButtonSpacing = normalized;
    updateTextMarginsForChrome();
    updateChildGeometry();
    update();
    emit spinButtonSpacingChanged(m_spinButtonSpacing);
}

void NumberBox::setSpinButtonTextGap(int gap) {
    const int normalized = qMax(0, gap);
    if (m_spinButtonTextGap == normalized) return;
    m_spinButtonTextGap = normalized;
    updateTextMarginsForChrome();
    update();
    emit spinButtonTextGapChanged(m_spinButtonTextGap);
}

void NumberBox::setSpinButtonIconSize(int size) {
    if (size <= 0 || m_spinButtonIconSize == size) return;
    m_spinButtonIconSize = size;
    updateSpinButtonIcons();
    update();
    emit spinButtonIconSizeChanged(m_spinButtonIconSize);
}

void NumberBox::setDisplayPrecision(int precision) {
    const int normalized = qMax(-1, precision);
    if (m_displayPrecision == normalized) return;
    m_displayPrecision = normalized;
    if (!isNan(m_value)) setText(formatValue(m_value));
    emit displayPrecisionChanged(m_displayPrecision);
}

void NumberBox::setFormatStep(double step) {
    const double normalized = (step > 0.0 && isFiniteNumber(step)) ? step : 0.0;
    if (numbersEqual(m_formatStep, normalized)) return;
    m_formatStep = normalized;
    if (!isNan(m_value)) setValueInternal(m_value, true, false);
    emit formatStepChanged(m_formatStep);
}

QSize NumberBox::sizeHint() const {
    QSize hint = LineEdit::sizeHint();
    hint.setHeight(totalPreferredHeight());
    hint.setWidth(qMax(hint.width(), kMinimumWidth));
    return hint;
}

QSize NumberBox::minimumSizeHint() const {
    QSize hint = sizeHint();
    hint.setWidth(qMax(hint.width(), kMinimumWidth));
    return hint;
}

void NumberBox::onThemeUpdated() {
    LineEdit::onThemeUpdated();
    if (m_spinUpButton) m_spinUpButton->onThemeUpdated();
    if (m_spinDownButton) m_spinDownButton->onThemeUpdated();
    update();
}

void NumberBox::paintEvent(QPaintEvent* event) {
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (!m_header.isEmpty()) paintHeader(painter);
        paintInputFrame(painter);
    }
    LineEdit::paintEvent(event);
}

void NumberBox::resizeEvent(QResizeEvent* event) {
    LineEdit::resizeEvent(event);
    updateChildGeometry();
}

void NumberBox::focusInEvent(QFocusEvent* event) {
    m_focused = true;
    LineEdit::focusInEvent(event);
    update();
}

void NumberBox::focusOutEvent(QFocusEvent* event) {
    commitInput();
    m_focused = false;
    LineEdit::focusOutEvent(event);
    update();
}

void NumberBox::enterEvent(FluentEnterEvent* event) {
    m_hovered = true;
    LineEdit::enterEvent(event);
    update();
}

void NumberBox::leaveEvent(QEvent* event) {
    m_hovered = false;
    LineEdit::leaveEvent(event);
    update();
}

void NumberBox::mousePressEvent(QMouseEvent* event) {
    if (event && event->button() == Qt::LeftButton && isEnabled() && !isReadOnly()) {
        m_pressed = inputRect().contains(event->pos());
        update();
    }
    LineEdit::mousePressEvent(event);
}

void NumberBox::mouseReleaseEvent(QMouseEvent* event) {
    if (m_pressed) {
        m_pressed = false;
        update();
    }
    LineEdit::mouseReleaseEvent(event);
}

void NumberBox::keyPressEvent(QKeyEvent* event) {
    if (!event) {
        LineEdit::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
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

void NumberBox::changeEvent(QEvent* event) {
    LineEdit::changeEvent(event);
    if (!event) return;
    if (event->type() == QEvent::EnabledChange || event->type() == QEvent::ReadOnlyChange) {
        updateSpinnerState();
        updateTextMarginsForChrome();
        update();
    }
}

bool NumberBox::eventFilter(QObject* watched, QEvent* event) {
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

QRect NumberBox::inputRect() const {
    return QRect(0, inputTop(), width(), kInputHeight);
}

int NumberBox::inputTop() const {
    return m_header.isEmpty() ? 0 : kHeaderHeight + kHeaderGap;
}

int NumberBox::totalPreferredHeight() const {
    return inputTop() + kInputHeight;
}

void NumberBox::initializeSpinnerButtons() {
    m_spinUpButton = new ::view::basicinput::RepeatButton(this);
    m_spinDownButton = new ::view::basicinput::RepeatButton(this);

    auto configureButton = [this](::view::basicinput::RepeatButton* button,
                                  const QString& glyph,
                                  const QString& objectName) {
        button->setObjectName(objectName);
        button->setFluentStyle(::view::basicinput::Button::Standard);
        button->setFluentLayout(::view::basicinput::Button::IconOnly);
        button->setFluentSize(::view::basicinput::Button::Small);
        button->setFocusPolicy(Qt::NoFocus);
        button->setFixedSize(m_spinButtonSize);
        button->setIconGlyph(glyph, m_spinButtonIconSize, Typography::FontFamily::SegoeFluentIcons);
        button->installEventFilter(this);
        button->hide();
    };

    configureButton(m_spinUpButton, Typography::Icons::ChevronUp, QStringLiteral("NumberBoxSpinUpButton"));
    configureButton(m_spinDownButton, Typography::Icons::ChevronDown, QStringLiteral("NumberBoxSpinDownButton"));

    connect(m_spinUpButton, &::view::basicinput::RepeatButton::clicked, this, [this]() {
        setFocus(Qt::MouseFocusReason);
        stepBy(m_smallChange);
    });
    connect(m_spinDownButton, &::view::basicinput::RepeatButton::clicked, this, [this]() {
        setFocus(Qt::MouseFocusReason);
        stepBy(-m_smallChange);
    });

    updateChildGeometry();
}

void NumberBox::updateSpinButtonIcons() {
    if (m_spinUpButton) {
        m_spinUpButton->setIconGlyph(Typography::Icons::ChevronUp,
                                     m_spinButtonIconSize,
                                     Typography::FontFamily::SegoeFluentIcons);
    }
    if (m_spinDownButton) {
        m_spinDownButton->setIconGlyph(Typography::Icons::ChevronDown,
                                       m_spinButtonIconSize,
                                       Typography::FontFamily::SegoeFluentIcons);
    }
}

void NumberBox::updateChildGeometry() {
    if (!m_spinUpButton || !m_spinDownButton) return;
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

void NumberBox::updateSpinnerState() {
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

void NumberBox::updateTextMarginsForChrome() {
    int rightMargin = ::Spacing::Padding::TextFieldHorizontal;
    if (m_spinButtonPlacementMode == SpinButtonPlacementMode::Inline) {
        rightMargin += m_spinButtonRightMargin + inlineSpinnerWidth() + m_spinButtonTextGap;
    } else if (m_spinButtonPlacementMode == SpinButtonPlacementMode::Compact) {
        rightMargin += m_spinButtonRightMargin + compactExpandedSpinnerWidth() + m_spinButtonTextGap;
    }

    QMargins margins = contentMargins();
    margins.setLeft(::Spacing::Padding::TextFieldHorizontal);
    margins.setRight(rightMargin);
    margins.setTop(::Spacing::Padding::TextFieldVertical);
    margins.setBottom(::Spacing::Padding::TextFieldVertical);
    setContentMargins(margins);
}

void NumberBox::updateHeaderTextMargins() {
    setTextMargins(0, inputTop(), 0, 0);
}

void NumberBox::commitInput() {
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

void NumberBox::setInvalidValueFromText() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (numbersEqual(m_value, nan)) return;
    m_value = nan;
    updateSpinnerState();
    emit valueChanged(m_value);
}

void NumberBox::stepBy(double delta) {
    if (!isEnabled() || isReadOnly()) return;
    const double base = isNan(m_value) ? normalizedStepStart() : m_value;
    setValueInternal(base + delta, true, false);
}

double NumberBox::normalizedStepStart() const {
    if (m_minimum <= 0.0 && m_maximum >= 0.0) return 0.0;
    if (m_minimum > 0.0) return m_minimum;
    if (m_maximum < 0.0) return m_maximum;
    return 0.0;
}

double NumberBox::normalizeValue(double value) const {
    if (isNan(value) || !isFiniteNumber(value)) return std::numeric_limits<double>::quiet_NaN();
    value = applyFormatStep(value);
    if (value < m_minimum) value = m_minimum;
    if (value > m_maximum) value = m_maximum;
    return value;
}

double NumberBox::applyFormatStep(double value) const {
    if (!(m_formatStep > 0.0) || !isFiniteNumber(m_formatStep)) return value;
    return std::floor(value / m_formatStep + 0.5 + kNumberEpsilon) * m_formatStep;
}

QString NumberBox::formatValue(double value) const {
    if (isNan(value)) return QString();
    if (m_displayPrecision >= 0) return QLocale::c().toString(value, 'f', m_displayPrecision);
    return QString::number(value, 'g', 15);
}

bool NumberBox::parseInputText(const QString& input, double* result) const {
    if (!result) return false;
    if (m_acceptsExpression) {
        ExpressionParser parser(input);
        return parser.parse(result);
    }

    bool ok = false;
    const double parsed = QLocale::c().toDouble(input, &ok);
    if (!ok || !isFiniteNumber(parsed)) return false;
    *result = parsed;
    return true;
}

bool NumberBox::setValueInternal(double value, bool updateText, bool keepUserTextWhenNaN) {
    const double normalized = normalizeValue(value);
    const bool changed = !numbersEqual(m_value, normalized);
    m_value = normalized;

    if (updateText && !(keepUserTextWhenNaN && isNan(m_value))) setText(formatValue(m_value));
    if (changed) updateSpinnerState();
    if (changed) emit valueChanged(m_value);
    return changed;
}

void NumberBox::paintInputFrame(QPainter& painter) {
    const auto colors = themeColors();
    const QRectF frameRect = QRectF(inputRect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QColor bgColor;
    QColor borderColor;
    QColor bottomColor;
    int bottomWidth = ::Spacing::Border::Normal;

    if (!isEnabled()) {
        bgColor = colors.controlDisabled;
        borderColor = colors.strokeDivider;
        bottomColor = borderColor;
    } else if (isReadOnly()) {
        bgColor = colors.controlAltSecondary;
        borderColor = colors.strokeDefault;
        bottomColor = colors.strokeDivider;
    } else if (m_focused) {
        bgColor = (currentTheme() == Dark) ? colors.bgSolid : colors.controlDefault;
        borderColor = colors.strokeSecondary;
        bottomColor = colors.accentDefault;
        bottomWidth = ::Spacing::Border::Focused;
    } else if (m_pressed || m_spinnerPressed) {
        bgColor = colors.controlTertiary;
        borderColor = colors.strokeSecondary;
        bottomColor = colors.strokeSecondary;
    } else if (m_hovered || m_spinnerHovered) {
        bgColor = colors.controlSecondary;
        borderColor = colors.strokeSecondary;
        bottomColor = colors.strokeSecondary;
    } else {
        bgColor = colors.controlDefault;
        borderColor = colors.strokeDefault;
        bottomColor = colors.strokeDivider;
    }

    const qreal radius = themeRadius().control;
    QPainterPath framePath;
    framePath.addRoundedRect(frameRect, radius, radius);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawPath(framePath);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(borderColor, 1));
    painter.drawPath(framePath);

    if (isEnabled() && !isReadOnly()) {
        QPen bottomPen(bottomColor, bottomWidth);
        bottomPen.setCapStyle(Qt::RoundCap);
        painter.setPen(bottomPen);
        const qreal bottomY = frameRect.bottom() - (bottomWidth > 1 ? (bottomWidth - 1) / 2.0 : 0);
        QPainterPath bottomPath;
        bottomPath.moveTo(frameRect.left() + radius, bottomY);
        bottomPath.lineTo(frameRect.right() - radius, bottomY);
        painter.drawPath(bottomPath);
    }
}

void NumberBox::paintHeader(QPainter& painter) {
    if (m_header.isEmpty()) return;
    painter.setFont(themeFont(Typography::FontRole::Body).toQFont());
    painter.setPen(isEnabled() ? themeColors().textPrimary : themeColors().textDisabled);
    const QRect headerRect(0, 0, width(), kHeaderHeight);
    painter.drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(m_header, Qt::ElideRight, headerRect.width()));
}

bool NumberBox::hasSpinnerButtonsVisible() const {
    return m_spinButtonPlacementMode != SpinButtonPlacementMode::Hidden;
}

int NumberBox::inlineSpinnerWidth() const {
    return m_inlineSpinButtonSize.width() * 2 + m_spinButtonSpacing;
}

int NumberBox::compactExpandedSpinnerWidth() const {
    return m_spinButtonSize.width();
}

} // namespace view::textfields
