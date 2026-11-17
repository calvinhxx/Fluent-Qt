#include "compatibility/InputEventCompat.h"
#include "ToggleSwitch.h"
#include "components/basicinput/private/BasicValueAccessibility_p.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/foundation/private/ValueAccessibility_p.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScopedValueRollback>
#include <QStyle>
#include <QtMath>

namespace fluent::basicinput {

// WinUI 3 ToggleSwitch metrics (from ToggleSwitch_themeresources.xaml).
namespace {
constexpr int kTrackW = 40;
constexpr int kTrackH = 20;
constexpr int kKnobNormal = 12;
constexpr int kKnobHover = 14;
constexpr int kKnobPressedW = 17;
constexpr int kKnobPressedH = 14;
constexpr int kContentGap = 10;
constexpr qreal kTrackRadius = kTrackH / 2.0;
} // namespace

ToggleSwitch::ToggleSwitch(QWidget* parent) : QWidget(parent)
{
    detail::ensureBasicValueAccessibilityFactory();
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);

    applyFontRole();

    m_knobAnimation = new QPropertyAnimation(this, "knobPosition", this);
    m_knobAnimation->setDuration(themeAnimation().fast);
    m_knobAnimation->setEasingCurve(themeAnimation().decelerate);
    updateAccessibleText();
}

void ToggleSwitch::onThemeUpdated()
{
    if (!m_hasExplicitFont)
        applyFontRole();
    update();
}

void ToggleSwitch::setIsOn(bool on)
{
    if (m_isOn == on)
        return;
    m_isOn = on;
    animateKnob(on);
    updateAccessibleText();
    update();
    QAccessible::State changed;
    changed.checked = true;
    accessibility::detail::notifyValueAccessibilityState(this, changed);
    emit toggled(m_isOn);
}

void ToggleSwitch::setOnContent(const QString& content)
{
    if (m_onContent == content)
        return;
    m_onContent = content;
    updateAccessibleText();
    updateGeometry();
    update();
    emit onContentChanged(m_onContent);
}

void ToggleSwitch::setOffContent(const QString& content)
{
    if (m_offContent == content)
        return;
    m_offContent = content;
    updateAccessibleText();
    updateGeometry();
    update();
    emit offContentChanged(m_offContent);
}

void ToggleSwitch::setFontRole(Typography::FontRole role)
{
    const bool roleChanged = m_fontRole != role;
    if (!roleChanged && !m_hasExplicitFont)
        return;

    m_fontRole = role;
    m_hasExplicitFont = false;
    applyFontRole();
    updateGeometry();
    update();
    if (roleChanged)
        emit fontRoleChanged();
}

void ToggleSwitch::setFont(const QFont& font)
{
    m_hasExplicitFont = true;
    QWidget::setFont(font);
}

void ToggleSwitch::setVisualScale(qreal scale)
{
    if (!qIsFinite(scale))
        return;
    scale = qBound(qreal(0.5), scale, qreal(10.0));
    if (qFuzzyCompare(m_visualScale, scale))
        return;
    m_visualScale = scale;
    updateGeometry();
    update();
    emit visualScaleChanged(m_visualScale);
}

void ToggleSwitch::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() != QEvent::FontChange)
        return;

    if (!m_applyingFontRole)
        m_hasExplicitFont = true;
    updateGeometry();
    update();
}

void ToggleSwitch::applyFontRole()
{
    const QFont resolvedFont = themeFont(m_fontRole).toQFont();
    if (font() == resolvedFont)
        return;

    const QScopedValueRollback<bool> applyingFontRole(m_applyingFontRole, true);
    QWidget::setFont(resolvedFont);
}

void ToggleSwitch::setKnobPosition(qreal pos)
{
    pos = qBound(0.0, pos, 1.0);
    if (qFuzzyCompare(m_knobPosition, pos))
        return;
    m_knobPosition = pos;
    update();
}

int ToggleSwitch::contentAreaX() const
{
    return qCeil((kTrackW + kContentGap) * m_visualScale);
}

QRectF ToggleSwitch::trackRect() const
{
    // Widget size owns the hit area; only visualScale scales the track. Keep integer Y alignment on odd row heights.
    const qreal trackW = kTrackW * m_visualScale;
    const qreal trackH = kTrackH * m_visualScale;
    const qreal trackX = layoutDirection() == Qt::RightToLeft ? width() - trackW : 0.0;
    const int trackY = qFloor((height() - trackH) / 2.0);
    return QRectF(trackX, trackY, trackW, trackH);
}

QRectF ToggleSwitch::knobRect() const
{
    QRectF track = trackRect();
    qreal knobW, knobH;
    if (m_isPressed) {
        knobW = kKnobPressedW;
        knobH = kKnobPressedH;
    } else if (m_isHovered) {
        knobW = kKnobHover;
        knobH = kKnobHover;
    } else {
        knobW = kKnobNormal;
        knobH = kKnobNormal;
    }
    knobW *= m_visualScale;
    knobH *= m_visualScale;

    qreal cy = track.center().y();
    qreal offX = track.left() + (track.height() - knobW) / 2.0;
    qreal onX = track.right() - (track.height() - knobW) / 2.0 - knobW;
    const qreal visualPosition = layoutDirection() == Qt::RightToLeft ? 1.0 - m_knobPosition : m_knobPosition;
    qreal x = offX + (onX - offX) * visualPosition;

    return QRectF(x, cy - knobH / 2.0, knobW, knobH);
}

QSize ToggleSwitch::sizeHint() const
{
    QFontMetrics fm(font());
    int contentTextW = qMax(fm.horizontalAdvance(m_onContent), fm.horizontalAdvance(m_offContent));
    int w = contentAreaX() + contentTextW;
    int h = qMax(minimumSizeHint().height(), fm.height());

    return QSize(w, h);
}

QSize ToggleSwitch::minimumSizeHint() const
{
    return QSize(qCeil(kTrackW * m_visualScale), qMax(::Spacing::ControlHeight::Small, qCeil(kTrackH * m_visualScale)));
}

void ToggleSwitch::animateKnob(bool toOn)
{
    m_knobAnimation->stop();
    m_knobAnimation->setStartValue(m_knobPosition);
    m_knobAnimation->setEndValue(toOn ? 1.0 : 0.0);
    ::fluent::detail::startMotionTransition(m_knobAnimation, themeAnimation().fast);
}

void ToggleSwitch::toggle()
{
    if (!isEnabled())
        return;
    setIsOn(!m_isOn);
}

void ToggleSwitch::updateAccessibleText()
{
    const QString description = m_isOn ? m_onContent : m_offContent;
    if (accessibleDescription().isEmpty() || accessibleDescription() == m_autoAccessibleDescription) {
        setAccessibleDescription(description);
    }
    m_autoAccessibleDescription = description;
}

void ToggleSwitch::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const auto& c = themeColorsRef();
    bool enabled = isEnabled();

    QRectF track = trackRect();
    const qreal strokeWidth = qMax(qreal(1.0), m_visualScale);
    const qreal trackRadius = kTrackRadius * m_visualScale;

    QColor trackFill, trackStroke;

    if (!enabled) {
        if (m_isOn) {
            trackFill = c.accentDisabled;
            trackStroke = c.accentDisabled;
        } else {
            trackFill = c.controlDisabled;
            trackStroke = c.textDisabled;
        }
    } else if (m_isPressed) {
        if (m_isOn) {
            trackFill = c.accentTertiary;
            trackStroke = c.accentTertiary;
        } else {
            trackFill = c.controlTertiary;
            trackStroke = c.strokeStrong;
        }
    } else if (m_isHovered) {
        if (m_isOn) {
            trackFill = c.accentSecondary;
            trackStroke = c.accentSecondary;
        } else {
            trackFill = c.controlAltTertiary;
            trackStroke = c.strokeStrong;
        }
    } else {
        if (m_isOn) {
            trackFill = c.accentDefault;
            trackStroke = c.accentDefault;
        } else {
            trackFill = c.controlAltSecondary;
            trackStroke = c.strokeStrong;
        }
    }

    QPainterPath trackPath;
    const qreal inset = strokeWidth / 2.0;
    trackPath.addRoundedRect(track.adjusted(inset, inset, -inset, -inset), trackRadius, trackRadius);
    p.setPen(Qt::NoPen);
    p.setBrush(trackFill);
    p.drawPath(trackPath);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(trackStroke, strokeWidth));
    p.drawPath(trackPath);

    QRectF knob = knobRect();
    QColor knobFill;
    if (!enabled) {
        knobFill = m_isOn ? c.textDisabled : c.textDisabled;
    } else {
        knobFill = m_isOn ? c.textOnAccent : c.textSecondary;
    }

    p.setPen(Qt::NoPen);
    p.setBrush(knobFill);
    qreal knobR = qMin(knob.width(), knob.height()) / 2.0;
    p.drawRoundedRect(knob, knobR, knobR);

    QString contentText = m_isOn ? m_onContent : m_offContent;
    if (!contentText.isEmpty()) {
        p.setFont(font());
        p.setPen(enabled ? c.textPrimary : c.textDisabled);
        int textX = contentAreaX();
        int textH = qMax(qCeil(track.height()), QFontMetrics(font()).height());
        int textY = (height() - textH) / 2;
        const QRect logicalTextRect(textX, textY, width() - textX, textH);
        const QRect textRect = QStyle::visualRect(layoutDirection(), rect(), logicalTextRect);
        p.drawText(textRect, QStyle::visualAlignment(layoutDirection(), Qt::AlignVCenter | Qt::AlignLeft), contentText);
    }

    if (enabled && hasFocus() && m_keyboardFocusVisible) {
        QColor focusColor = c.textSecondary;
        focusColor.setAlpha(120);
        p.setPen(QPen(focusColor, strokeWidth));
        p.setBrush(Qt::NoBrush);
        const qreal focusInset = 1.5 * m_visualScale;
        const qreal focusRadius = trackRadius - m_visualScale;
        p.drawRoundedRect(track.adjusted(focusInset, focusInset, -focusInset, -focusInset), focusRadius, focusRadius);
    }
}

void ToggleSwitch::mousePressEvent(QMouseEvent* event)
{
    if (!isEnabled()) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (!hasFocus())
            setFocus(Qt::MouseFocusReason);
        m_keyboardFocusVisible = false;
        m_isPressed = true;
        update();
        // Accept press so embedded graphics proxies still deliver the matching release event.
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ToggleSwitch::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_isPressed) {
        m_isPressed = false;
        if (rect().contains(event->pos())) {
            toggle();
        }
        update();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ToggleSwitch::enterEvent(FluentEnterEvent* event)
{
    if (isEnabled()) {
        m_isHovered = true;
        update();
    }
    QWidget::enterEvent(event);
}

void ToggleSwitch::leaveEvent(QEvent* event)
{
    if (isEnabled()) {
        m_isHovered = false;
        update();
    }
    QWidget::leaveEvent(event);
}

void ToggleSwitch::keyPressEvent(QKeyEvent* event)
{
    m_keyboardFocusVisible = true;
    update();
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return) {
        toggle();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ToggleSwitch::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    if (event->reason() == Qt::MouseFocusReason)
        m_keyboardFocusVisible = false;
    else if (event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason
        || event->reason() == Qt::ShortcutFocusReason)
        m_keyboardFocusVisible = true;
    update();
}

void ToggleSwitch::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    update();
}

} // namespace fluent::basicinput
