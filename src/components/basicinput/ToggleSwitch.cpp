#include "ToggleSwitch.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPropertyAnimation>
#include "design/Typography.h"
#include "design/Spacing.h"

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

// ── Property setters. zh_CN: 属性 setter ────────────────────────────────────

void ToggleSwitch::setIsOn(bool on)
{
    if (m_isOn == on) return;
    m_isOn = on;
    animateKnob(on);
    update();
    emit toggled(m_isOn);
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

    // Knob center Y equals the track center. zh_CN: knob 中心 Y = track 中心。
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

// ── Painting. zh_CN: 绘制 ────────────────────────────────────────────────────

void ToggleSwitch::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const auto& c = themeColors();
    bool enabled = isEnabled();

    // Resolve the active design language and theme polarity, mirroring Button::paintEvent.
    // zh_CN: 解析当前设计语言与主题明暗,处理方式与 Button::paintEvent 一致。
    const auto lang = themeDesignLanguage();   // inherited from FluentElement. zh_CN: 来自 FluentElement。
    const bool darkTheme = effectiveTheme() == fluent::FluentElement::Dark;
    // Theme-aware veil: darkens light surfaces, lightens dark ones — keeps hover/press visible in BOTH App
    // themes. zh_CN: 主题感知薄层:浅色面变暗、深色面变亮,使 hover/press 在明暗两主题下都可见。
    const auto veil = [darkTheme](int a) { return darkTheme ? QColor(255, 255, 255, a) : QColor(0, 0, 0, a); };
    // Track interaction overlay for the Cupertino branch (M3 uses a circular thumb halo instead, per its spec).
    // zh_CN: Cupertino 分支的轨道交互薄层(M3 改用滑块圆形光晕,遵循其规范)。
    const QColor trackVeil = m_isPressed ? veil(0x1E) : (m_isHovered ? veil(0x12) : QColor());

    // ── Track ──
    QRectF track = trackRect();

    if (lang == DesignMaterial) {
        // Material 3 switch (material-3.md §5 Switch): OFF = surface-container-highest-like fill + a ~2px
        // `outline` (strokeStrong) border with a SMALL outline-gray thumb; ON = `primary` (accentDefault)
        // filled pill, no border, with a LARGER `on-primary` (textOnAccent / white) thumb. The thumb visibly
        // GROWS 16→24dp as it travels. zh_CN: Material 3 开关:关闭=类 surface-container-highest 填充 + ~2px
        // outline(strokeStrong)描边,配小号 outline 灰滑块;开启=primary(accentDefault)填充胶囊、无描边,配更大的
        // on-primary(textOnAccent/白)滑块。滑块行进时明显变大(16→24dp)。
        QColor trackFill, trackStroke, knobFill;
        if (!enabled) {
            trackFill = m_isOn ? c.accentDisabled : Qt::transparent;
            trackStroke = m_isOn ? Qt::transparent : c.strokeDivider;
            knobFill = m_isOn ? c.textOnAccent : c.textDisabled;
        } else if (m_isOn) {
            trackFill = c.accentDefault;
            trackStroke = Qt::transparent;
            knobFill = c.textOnAccent;
        } else {
            // surface-container-highest-like neutral fill so the OFF track reads filled under the outline.
            // zh_CN: 类 surface-container-highest 中性填充,使描边内的关闭轨道呈现填充感。
            trackFill = c.controlSecondary.isValid() ? c.controlSecondary : c.controlTertiary;
            trackStroke = c.strokeStrong;
            knobFill = c.strokeStrong;
        }

        QPainterPath trackPath;
        trackPath.addRoundedRect(track.adjusted(1.0, 1.0, -1.0, -1.0), kTrackRadius, kTrackRadius);
        p.setPen(Qt::NoPen);
        p.setBrush(trackFill);
        p.drawPath(trackPath);
        // ~2px outline for the off (outlined) state — drawn before the thumb so the thumb sits on top.
        // zh_CN: 关闭(描边)态的 ~2px 描边——在滑块之前绘制,使滑块叠在其上。
        if (trackStroke != QColor(Qt::transparent)) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(trackStroke, 2.0));
            p.drawPath(trackPath);
        }

        // Thumb GROWS off→on, mirroring M3's 16→24dp thumb (material-3.md §5). Scaled to our 20px track: a small
        // OFF thumb (radius ~7px) that eases up to a larger ON thumb (radius ~10px = 20px dia, nearly filling the
        // track). The radius interpolates along the same knobPosition as the travel, so it visibly grows. Bounded
        // to the track height so it never overflows the paint-only geometry. zh_CN: 滑块在关→开时变大,对应 M3 的
        // 16→24dp(material-3.md §5)。按 20px 轨道缩放:关闭小滑块(半径 ~7px)渐增到开启大滑块(半径 ~10px=20px 直径,
        // 几乎填满轨道)。半径沿与位移相同的 knobPosition 插值,明显变大;受限于轨道高度,绝不溢出(仅绘制、不改几何)。
        constexpr qreal kM3KnobOffR = 7.0;
        const qreal kM3KnobOnR = kTrackH / 2.0; // 10px: fills the 20px track height. zh_CN: 10px,填满 20px 轨道高度。
        qreal knobR = kM3KnobOffR + (kM3KnobOnR - kM3KnobOffR) * m_knobPosition;
        qreal cy = track.center().y();
        // Travel so the larger on-thumb still clears the track edge. zh_CN: 滑动行程,使更大的开启滑块仍贴边不溢出。
        qreal cxOff = track.left() + kTrackRadius;
        qreal cxOn = track.right() - kTrackRadius;
        qreal cx = cxOff + (cxOn - cxOff) * m_knobPosition;

        // M3 selection-control state layer: a circular halo behind the thumb on hover/press — NOT a fill swap.
        // Colored primary (accentDefault) when on, on-surface veil when off (material-3.md §4). 8% hover / 10%
        // press, scaled toward the 40dp spec halo. zh_CN: M3 选择控件 state layer:hover/press 时滑块后方的圆形光晕
        // (非换填充)。开启用 primary(accentDefault),关闭用 on-surface 薄层;8% hover / 10% press,按 40dp 规格缩放。
        if (enabled && (m_isHovered || m_isPressed)) {
            const int haloA = m_isPressed ? 0x1A /*~10%*/ : 0x14 /*~8%*/;
            QColor halo = m_isOn
                ? QColor(c.accentDefault.red(), c.accentDefault.green(), c.accentDefault.blue(), haloA)
                : veil(haloA);
            // Clamp the halo so the WHOLE ellipse stays inside the widget's rect() — never larger than the
            // distance from the thumb center (cx,cy) to the nearest widget edge. Previously haloR = kTrackH*0.8
            // (16px) overflowed the ~20px tall control, and the backing-store clip cut the halo into a
            // hard-edged band that read as an ugly "shadow". Now it is the largest soft circle that fits.
            // zh_CN: 钳制光晕,使整个椭圆都落在控件 rect() 内——绝不超过滑块中心(cx,cy)到最近控件边缘的距离。
            // 此前 haloR = kTrackH*0.8(16px)溢出约 20px 高的控件,backing-store 裁剪把光晕切成硬边色带,
            // 看起来像一块难看的「阴影」。现改为能放下的最大柔和圆。
            const qreal maxR = qMin(qMin(cx, width() - cx), qMin(cy, height() - cy));
            const qreal haloR = qMax(0.0, qMin(kTrackH * 0.8, maxR));
            p.setPen(Qt::NoPen);
            p.setBrush(halo);
            p.drawEllipse(QPointF(cx, cy), haloR, haloR);
        }

        p.setPen(Qt::NoPen);
        p.setBrush(knobFill);
        p.drawEllipse(QPointF(cx, cy), knobR, knobR);
    } else if (lang == DesignCupertino) {
        // macOS NSSwitch: ON = accent BLUE FILLED pill (controlAccentColor — NOT green; iOS UISwitch is green,
        // macOS uses the accent) with a solid white knob; OFF = neutral/tertiary gray pill with a white knob.
        // The knob keeps a white fill + hairline ring + soft shadow in BOTH states (macos.md §4 Switch).
        // zh_CN: macOS NSSwitch:开启=强调色蓝色填充胶囊(controlAccentColor——非绿色;iOS 才是绿,macOS 用强调色)
        // 配纯白滑块;关闭=中性/三级灰色胶囊配白色滑块。滑块在开关两态都保持白色填充 + 发丝环 + 柔和投影。
        QColor trackFill, knobFill, knobRing, knobShadow;
        if (!enabled) {
            trackFill = m_isOn ? c.accentDisabled : c.controlDisabled;
            knobFill = QColor(255, 255, 255, 200);
            knobRing = Qt::transparent;
            knobShadow = Qt::transparent;
        } else if (m_isOn) {
            trackFill = c.accentDefault;   // accent blue, not green. zh_CN: 强调色蓝,非绿。
            knobFill = Qt::white;
            knobRing = QColor(0, 0, 0, 40);
            knobShadow = QColor(0, 0, 0, 45);
        } else {
            // Neutral gray off track (a tertiary/control fill). zh_CN: 关闭态中性灰轨道(三级/控件填充)。
            trackFill = c.controlTertiary.isValid() ? c.controlTertiary : c.bgLayerAlt;
            knobFill = Qt::white;
            knobRing = QColor(0, 0, 0, 40);
            knobShadow = QColor(0, 0, 0, 45);
        }

        QPainterPath trackPath;
        trackPath.addRoundedRect(track.adjusted(0.5, 0.5, -0.5, -0.5), kTrackRadius, kTrackRadius);
        p.setPen(Qt::NoPen);
        p.setBrush(trackFill);
        p.drawPath(trackPath);
        // Theme-aware interaction veil over the resting track — visible in BOTH light and dark. zh_CN:
        // 静息轨道上的主题感知交互薄层——明暗两主题下都可见。
        if (enabled && trackVeil.isValid()) {
            p.setBrush(trackVeil);
            p.drawPath(trackPath);
        }

        // White knob: macOS keeps a constant thumb size, riding the existing knob position. zh_CN: 白色滑块:
        // macOS 滑块尺寸恒定,沿用现有滑块位置。
        qreal knobR = (kTrackH - 4) / 2.0;
        qreal cy = track.center().y();
        qreal cxOff = track.left() + 2.0 + knobR;
        qreal cxOn = track.right() - 2.0 - knobR;
        qreal cx = cxOff + (cxOn - cxOff) * m_knobPosition;
        // Soft 1px drop shadow under the knob (offset down) — the macOS "bezel" cue. zh_CN: 滑块下方柔和 1px 投影
        // (向下偏移)——macOS「斜面」线索。
        if (knobShadow != QColor(Qt::transparent)) {
            p.setPen(Qt::NoPen);
            p.setBrush(knobShadow);
            p.drawEllipse(QPointF(cx, cy + 0.75), knobR, knobR);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(knobFill);
        p.drawEllipse(QPointF(cx, cy), knobR, knobR);
        // Subtle 1px hairline ring around the knob (both states). zh_CN: 滑块周围 1px 发丝环(两态皆有)。
        if (knobRing != QColor(Qt::transparent)) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(knobRing, 1.0));
            p.drawEllipse(QPointF(cx, cy), knobR - 0.5, knobR - 0.5);
        }
    } else {
        // DesignFluent (default): unchanged WinUI treatment. zh_CN: 默认 Fluent,WinUI 处理不变。
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
    }

    // ── Content text (On/Off). zh_CN: Content 文字 ──
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

// ── Mouse interaction. zh_CN: 鼠标交互 ───────────────────────────────────────

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

} // namespace fluent::basicinput
