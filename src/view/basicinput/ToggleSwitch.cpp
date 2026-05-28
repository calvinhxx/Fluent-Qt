#include "ToggleSwitch.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPropertyAnimation>
#include "design/Typography.h"
#include "design/Spacing.h"

namespace view::basicinput {

// ── WinUI 3 ToggleSwitch 尺寸常量（来自 ToggleSwitch_themeresources.xaml）───
namespace {
    constexpr int kTrackW = 40;
    constexpr int kTrackH = 20;
    constexpr int kKnobNormal = 12;
    constexpr int kKnobHover = 14;
    constexpr int kKnobPressedW = 17;
    constexpr int kKnobPressedH = 14;
    constexpr int kKnobTravel = 20;     // On 状态 knob 平移距离
    constexpr int kContentGap = 10;     // 开关与文字间距 (ToggleSwitchPreContentMargin)
    constexpr int kHeaderGap = 4;       // Header 与开关行间距
    constexpr qreal kTrackRadius = kTrackH / 2.0;
}

ToggleSwitch::ToggleSwitch(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);

    auto fs = themeFont(m_fontRole);
    setFont(fs.toQFont());

    m_knobAnimation = new QPropertyAnimation(this, "knobPosition");
    m_knobAnimation->setDuration(themeAnimation().fast);
    m_knobAnimation->setEasingCurve(themeAnimation().decelerate);
}

void ToggleSwitch::onThemeUpdated()
{
    auto fs = themeFont(m_fontRole);
    setFont(fs.toQFont());
    update();
}

// ── 属性 setter ─────────────────────────────────────────────────────────────

void ToggleSwitch::setIsOn(bool on)
{
    if (m_isOn == on) return;
    m_isOn = on;
    animateKnob(on);
    update();
    emit toggled(m_isOn);
}

void ToggleSwitch::setHeader(const QString& header)
{
    if (m_header == header) return;
    m_header = header;
    updateGeometry();
    update();
    emit headerChanged(m_header);
}

void ToggleSwitch::setOnContent(const QString& content)
{
    if (m_onContent == content) return;
    m_onContent = content;
    updateGeometry();
    update();
    emit onContentChanged(m_onContent);
}

void ToggleSwitch::setOffContent(const QString& content)
{
    if (m_offContent == content) return;
    m_offContent = content;
    updateGeometry();
    update();
    emit offContentChanged(m_offContent);
}

void ToggleSwitch::setFontRole(const QString& role)
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

// ── 几何辅助 ─────────────────────────────────────────────────────────────────

int ToggleSwitch::contentAreaX() const
{
    return kTrackW + kContentGap;
}

QRectF ToggleSwitch::trackRect() const
{
    int y = 0;
    if (!m_header.isEmpty()) {
        QFontMetrics fm(font());
        y = fm.height() + kHeaderGap;
    }
    // 垂直居中到控件行
    int rowH = qMax(kTrackH, QFontMetrics(font()).height());
    int trackY = y + (rowH - kTrackH) / 2;
    return QRectF(0, trackY, kTrackW, kTrackH);
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

    // knob 中心 Y = track 中心
    qreal cy = track.center().y();
    // knob X travel: from left to right inside track
    qreal offX = track.left() + (kTrackH - knobW) / 2.0;
    qreal onX = track.right() - (kTrackH - knobW) / 2.0 - knobW;
    qreal x = offX + (onX - offX) * m_knobPosition;

    return QRectF(x, cy - knobH / 2.0, knobW, knobH);
}

QSize ToggleSwitch::sizeHint() const
{
    QFontMetrics fm(font());
    int contentTextW = qMax(fm.horizontalAdvance(m_onContent),
                            fm.horizontalAdvance(m_offContent));
    int w = kTrackW + kContentGap + contentTextW;
    int h = qMax(kTrackH, fm.height());

    if (!m_header.isEmpty()) {
        int headerW = fm.horizontalAdvance(m_header);
        w = qMax(w, headerW);
        h += fm.height() + kHeaderGap;
    }

    return QSize(w, h);
}

QSize ToggleSwitch::minimumSizeHint() const
{
    return QSize(kTrackW, kTrackH);
}

// ── 动画 ─────────────────────────────────────────────────────────────────────

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

// ── 绘制 ─────────────────────────────────────────────────────────────────────

void ToggleSwitch::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const auto& c = themeColors();
    bool enabled = isEnabled();

    // ── Header ──
    if (!m_header.isEmpty()) {
        p.setFont(font());
        p.setPen(enabled ? c.textPrimary : c.textDisabled);
        QFontMetrics fm(font());
        QRect headerRect(0, 0, width(), fm.height());
        p.drawText(headerRect, Qt::AlignVCenter | Qt::AlignLeft, m_header);
    }

    // ── Track ──
    QRectF track = trackRect();
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

    // 绘制 track 背景
    QPainterPath trackPath;
    trackPath.addRoundedRect(track.adjusted(0.5, 0.5, -0.5, -0.5), kTrackRadius, kTrackRadius);
    p.setPen(Qt::NoPen);
    p.setBrush(trackFill);
    p.drawPath(trackPath);
    // 绘制 track 描边
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

    // ── Content 文字 (On/Off) ──
    QString contentText = m_isOn ? m_onContent : m_offContent;
    if (!contentText.isEmpty()) {
        p.setFont(font());
        p.setPen(enabled ? c.textPrimary : c.textDisabled);
        int textX = contentAreaX();
        int textY = static_cast<int>(track.top());
        int textH = static_cast<int>(track.height());
        QRect textRect(textX, textY, width() - textX, textH);
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, contentText);
    }
}

// ── 鼠标交互 ─────────────────────────────────────────────────────────────────

void ToggleSwitch::mousePressEvent(QMouseEvent* event)
{
    if (!isEnabled()) { QWidget::mousePressEvent(event); return; }
    if (event->button() == Qt::LeftButton) {
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
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return) {
        toggle();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace view::basicinput
