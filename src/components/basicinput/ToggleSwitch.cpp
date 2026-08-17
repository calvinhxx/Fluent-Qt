#include "ToggleSwitch.h"
#include "components/basicinput/private/BasicValueAccessibility_p.h"
#include "components/foundation/private/ValueAccessibility_p.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QStyle>

namespace fluent::basicinput {

// ── WinUI 3 ToggleSwitch metrics (from ToggleSwitch_themeresources.xaml). zh_CN: 尺寸常量 ──
namespace {
    constexpr int kTrackW = 40;
    constexpr int kTrackH = 20;
    constexpr int kKnobNormal = 12;
    constexpr int kKnobHover = 14;
    constexpr int kKnobPressedW = 17;
    constexpr int kKnobPressedH = 14;
    constexpr int kContentGap = 10;     // Gap between switch and content text (ToggleSwitchPreContentMargin). zh_CN: 开关与文字间距。
    constexpr qreal kTrackRadius = kTrackH / 2.0;
} // namespace

ToggleSwitch::ToggleSwitch(QWidget* parent)
    : QWidget(parent)
{
    detail::ensureBasicValueAccessibilityFactory();
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);

    auto fs = themeFont(m_fontRole);
    setFont(fs.toQFont());

    m_knobAnimation = new QPropertyAnimation(this, "knobPosition");
    m_knobAnimation->setDuration(themeAnimation().fast);
    m_knobAnimation->setEasingCurve(themeAnimation().decelerate);
    updateAccessibleText();
}

void ToggleSwitch::onThemeUpdated()
{
    auto fs = themeFont(m_fontRole);
    setFont(fs.toQFont());
    update();
}

// ── Property setters. zh_CN: 属性 setter ────────────────────────────────────

void ToggleSwitch::setIsOn(bool on)
{
    if (m_isOn == on) return;
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
    if (m_onContent == content) return;
    m_onContent = content;
    updateAccessibleText();
    updateGeometry();
    update();
    emit onContentChanged(m_onContent);
}

void ToggleSwitch::setOffContent(const QString& content)
{
    if (m_offContent == content) return;
    m_offContent = content;
    updateAccessibleText();
    updateGeometry();
    update();
    emit offContentChanged(m_offContent);
}

void ToggleSwitch::setFontRole(Typography::FontRole role)
{
    if (m_fontRole == role) return;
    m_fontRole = role;
    auto fs = themeFont(m_fontRole);
    setFont(fs.toQFont());
    updateGeometry();
    update();
    emit fontRoleChanged();
}

void ToggleSwitch::setKnobPosition(qreal pos)
{
    pos = qBound(0.0, pos, 1.0);
    if (qFuzzyCompare(m_knobPosition, pos)) return;
    m_knobPosition = pos;
    update();
}

// ── Geometry helpers. zh_CN: 几何辅助 ────────────────────────────────────────

int ToggleSwitch::contentAreaX() const
{
    return kTrackW + kContentGap;
}

QRectF ToggleSwitch::trackRect() const
{
    // Center vertically in the control row. zh_CN: 垂直居中到控件行。
    int rowH = qMax(kTrackH, QFontMetrics(font()).height());
    int trackY = (rowH - kTrackH) / 2;
    return QStyle::visualRect(layoutDirection(), rect(),
                              QRect(0, trackY, kTrackW, kTrackH));
}

QRectF ToggleSwitch::knobRect() const
{
    QRectF track = trackRect();
    int knobW, knobH;
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

    // Knob center Y equals the track center. zh_CN: knob 中心 Y = track 中心。
    qreal cy = track.center().y();
    // knob X travel: from left to right inside track
    qreal offX = track.left() + (kTrackH - knobW) / 2.0;
    qreal onX = track.right() - (kTrackH - knobW) / 2.0 - knobW;
    const qreal visualPosition = layoutDirection() == Qt::RightToLeft
        ? 1.0 - m_knobPosition
        : m_knobPosition;
    qreal x = offX + (onX - offX) * visualPosition;

    return QRectF(x, cy - knobH / 2.0, knobW, knobH);
}

QSize ToggleSwitch::sizeHint() const
{
    QFontMetrics fm(font());
    int contentTextW = qMax(fm.horizontalAdvance(m_onContent),
                            fm.horizontalAdvance(m_offContent));
    int w = kTrackW + kContentGap + contentTextW;
    int h = qMax(kTrackH, fm.height());

    return QSize(w, h);
}

QSize ToggleSwitch::minimumSizeHint() const
{
    return QSize(kTrackW, kTrackH);
}

// ── Animation. zh_CN: 动画 ───────────────────────────────────────────────────

void ToggleSwitch::animateKnob(bool toOn)
{
    m_knobAnimation->stop();
    m_knobAnimation->setStartValue(m_knobPosition);
    m_knobAnimation->setEndValue(toOn ? 1.0 : 0.0);
    m_knobAnimation->start();
}

void ToggleSwitch::toggle()
{
    if (!isEnabled()) return;
    setIsOn(!m_isOn);
}

void ToggleSwitch::updateAccessibleText()
{
    const QString description = m_isOn ? m_onContent : m_offContent;
    if (accessibleDescription().isEmpty()
        || accessibleDescription() == m_autoAccessibleDescription) {
        setAccessibleDescription(description);
    }
    m_autoAccessibleDescription = description;
}

// ── Painting. zh_CN: 绘制 ────────────────────────────────────────────────────

void ToggleSwitch::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const auto& c = themeColorsRef();
    bool enabled = isEnabled();

    // ── Track ──
    QRectF track = trackRect();

    // Fluent treatment. zh_CN: Fluent 样式。
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

        // Paint the track fill. zh_CN: 绘制 track 背景。
        QPainterPath trackPath;
        trackPath.addRoundedRect(track.adjusted(0.5, 0.5, -0.5, -0.5), kTrackRadius, kTrackRadius);
        p.setPen(Qt::NoPen);
        p.setBrush(trackFill);
        p.drawPath(trackPath);
        // Paint the track outline. zh_CN: 绘制 track 描边。
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(trackStroke, 1.0));
        p.drawPath(trackPath);

        // ── Knob ──
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

    // ── Content text (On/Off). zh_CN: Content 文字 ──
    QString contentText = m_isOn ? m_onContent : m_offContent;
    if (!contentText.isEmpty()) {
        p.setFont(font());
        p.setPen(enabled ? c.textPrimary : c.textDisabled);
        int textX = contentAreaX();
        int textY = static_cast<int>(track.top());
        int textH = static_cast<int>(track.height());
        const QRect logicalTextRect(textX, textY, width() - textX, textH);
        const QRect textRect =
            QStyle::visualRect(layoutDirection(), rect(), logicalTextRect);
        p.drawText(textRect,
                   QStyle::visualAlignment(layoutDirection(),
                                           Qt::AlignVCenter | Qt::AlignLeft),
                   contentText);
    }

    if (enabled && hasFocus() && m_keyboardFocusVisible) {
        QColor focusColor = c.textSecondary;
        focusColor.setAlpha(120);
        p.setPen(QPen(focusColor, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(track.adjusted(1.5, 1.5, -1.5, -1.5),
                          kTrackRadius - 1.0, kTrackRadius - 1.0);
    }
}

// ── Mouse interaction. zh_CN: 鼠标交互 ───────────────────────────────────────

void ToggleSwitch::mousePressEvent(QMouseEvent* event)
{
    if (!isEnabled()) { QWidget::mousePressEvent(event); return; }
    if (event->button() == Qt::LeftButton) {
        if (!hasFocus())
            setFocus(Qt::MouseFocusReason);
        m_keyboardFocusVisible = false;
        m_isPressed = true;
        update();
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
    else if (event->reason() == Qt::TabFocusReason
             || event->reason() == Qt::BacktabFocusReason
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
