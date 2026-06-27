#include "Slider.h"
#include "components/textfields/Label.h"
#include "components/status_info/ToolTip.h"

#include <QPainter>
#include <QStyleOptionSlider>
#include <QStyle>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <algorithm>

#include "design/Typography.h"

namespace fluent::basicinput {

using namespace fluent::textfields;

Slider::Slider(Qt::Orientation orientation, QWidget* parent)
    : QSlider(orientation, parent) {
    setAttribute(Qt::WA_Hover);
    
    // m_handleSize is initialized to 20 in header

    const auto& anim = themeAnimation();

    m_hoverAnim = new QPropertyAnimation(this, "hoverRatio");
    m_hoverAnim->setDuration(anim.normal);
    m_hoverAnim->setEasingCurve(anim.decelerate);

    m_pressAnim = new QPropertyAnimation(this, "pressRatio");
    m_pressAnim->setDuration(anim.normal);
    m_pressAnim->setEasingCurve(anim.decelerate);
}

Slider::Slider(QWidget* parent)
    : Slider(Qt::Horizontal, parent) {
}

Slider::~Slider() {
    if (m_toolTip) {
        delete m_toolTip;
    }
}

QSize Slider::sizeHint() const {
    // Ensure enough space for the handle to avoid clipping
    // The thickness (height/width) should be at least m_handleSize + some margin
    int length = m_defaultLength;
    // Add small margin (e.g. 2 * m_visualMargin) to avoid anti-aliasing clipping
    int thickness = std::max(m_handleSize, m_trackHeight) + 2 * m_visualMargin;
    
    if (orientation() == Qt::Horizontal) {
        return QSize(length, thickness); 
    } else {
        return QSize(thickness, length);
    }
}

void Slider::paintEvent(QPaintEvent*) {
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    if (opt.orientation == Qt::Horizontal) {
        drawHorizontal(p, opt);
    } else {
        drawVertical(p, opt);
    }
}

void Slider::enterEvent(FluentEnterEvent* event) {
    m_hoverAnim->stop();
    m_hoverAnim->setEndValue(1.0);
    m_hoverAnim->start();
    QSlider::enterEvent(event);
}

void Slider::leaveEvent(QEvent* event) {
    if (!m_isPressed) { // Only fade out if not currently dragging
        m_hoverAnim->stop();
        m_hoverAnim->setEndValue(0.0);
        m_hoverAnim->start();
    }
    QSlider::leaveEvent(event);
}

void Slider::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    event->accept();
    m_isPressed = true;
    setSliderDown(true);
    
    // Animate Press
    m_pressAnim->stop();
    m_pressAnim->setEndValue(1.0);
    m_pressAnim->start();

    // Show ToolTip
    showToolTip();
    
    int val = pixelPosToRangeValue(orientation() == Qt::Horizontal ? fluentMousePos(event).x() : fluentMousePos(event).y());
    setSliderPosition(val);
    updateToolTipPos(); // update after value change
    
    triggerAction(SliderMove);
    update();
}

void Slider::mouseMoveEvent(QMouseEvent *event) {
    if (!m_isPressed) {
        event->ignore();
        return;
    }
    event->accept();
    int val = pixelPosToRangeValue(orientation() == Qt::Horizontal ? fluentMousePos(event).x() : fluentMousePos(event).y());
    setSliderPosition(val);
    updateToolTipPos();

    triggerAction(SliderMove);
    update();
}

void Slider::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    event->accept();
    m_isPressed = false;
    setSliderDown(false);
    
    // Animate Release
    m_pressAnim->stop();
    m_pressAnim->setEndValue(0.0);
    m_pressAnim->start();

    // Reset Hover if mouse left during drag
    if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
        m_hoverAnim->stop();
        m_hoverAnim->setEndValue(0.0);
        m_hoverAnim->start();
    }

    hideToolTip();

    triggerAction(SliderMove);
    update();
    emit sliderReleased();
}

void Slider::showToolTip() {
    if (!m_toolTip) {
        m_toolTip = new fluent::status_info::ToolTip(nullptr); // Top level window
    }
    m_toolTip->setText(QString::number(value()));
    updateToolTipPos();
    m_toolTip->show();
    m_toolTip->raise();
}

void Slider::hideToolTip() {
    if (m_toolTip) {
        m_toolTip->hide();
    }
}

void Slider::updateToolTipPos() {
    if (!m_toolTip || !m_toolTip->isVisible()) return;
    
    m_toolTip->setText(QString::number(value()));

    QPoint handlePos;
    int pos = valueToPixelPos(value());
    const int cy = height() / 2;
    const int cx = width() / 2;

    if (orientation() == Qt::Horizontal) {
        handlePos = QPoint(pos, cy);
    } else {
        handlePos = QPoint(cx, pos);
    }
    
    QPoint globalHandle = mapToGlobal(handlePos);
    
    // Position tooltip above (Horiz) or Right (Vertical)
    int tipW = m_toolTip->width();
    int tipH = m_toolTip->height();
    int spacing = themeSpacing().medium;

    QPoint tipPos;
    if (orientation() == Qt::Horizontal) {
        tipPos = QPoint(globalHandle.x() - tipW / 2, globalHandle.y() - tipH - spacing - m_handleSize/2);
    } else {
        // Vertical: Right side preferred, fallback to left if no space? 
        // WinUI usually puts it to the side.
        tipPos = QPoint(globalHandle.x() + m_handleSize/2 + spacing, globalHandle.y() - tipH / 2);
    }
    
    m_toolTip->move(tipPos);
}

int Slider::valueToPixelPos(int val) const {
    const int padding = m_handleSize / 2 + m_visualMargin; // + m_visualMargin margin to match sizeHint
    int available = 0;
    int start = 0;
    
    if (orientation() == Qt::Horizontal) {
        available = width() - 2 * padding; // Use full symmetrical padding
        start = padding;
    } else {
        available = height() - 2 * padding;
        start = height() - padding; // Start from bottom
    }

    if (maximum() == minimum()) return start;

    double percent = (double)(val - minimum()) / (double)(maximum() - minimum());
    
    if (orientation() == Qt::Horizontal) {
        return start + (int)(percent * available);
    } else {
        return start - (int)(percent * available);
    }
}

int Slider::pixelPosToRangeValue(int pos) const {
    const int padding = m_handleSize / 2 + m_visualMargin; 
    int available = 0;
    int relPos = 0;

    if (orientation() == Qt::Horizontal) {
        available = width() - 2 * padding;
        relPos = pos - padding;
    } else {
        available = height() - 2 * padding;
        int bottom = height() - padding;
        relPos = bottom - pos;
    }

    if (available <= 0) return minimum();

    double percent = (double)relPos / (double)available;
    percent = std::clamp(percent, 0.0, 1.0);

    return minimum() + (int)(percent * (maximum() - minimum()));
}

void Slider::drawHorizontal(QPainter& p, const QStyleOptionSlider& opt) {
    const auto& colors = themeColors();
    const auto& radius = themeRadius();

    // Brand design language + theme-aware interaction veil, mirroring Button::paintEvent.
    // The veil DARKENS light surfaces and LIGHTENS dark ones so hover/press stay visible in
    // both App themes. zh_CN: 品牌设计语言 + 主题感知交互薄层,与 Button::paintEvent 一致。
    // 薄层在浅色面变暗、深色面变亮,使 hover/press 在明暗两主题下都可见。
    const auto lang = themeDesignLanguage();
    const bool darkTheme = effectiveTheme() == fluent::FluentElement::Dark;
    const auto veil = [darkTheme](int a) { return darkTheme ? QColor(255, 255, 255, a) : QColor(0, 0, 0, a); };

    const int cy = height() / 2;
    const int padding = m_handleSize / 2 + m_visualMargin;

    // Per-language track thickness: M3 wants a thick two-tone rail (expressive look), macOS a hairline
    // one; Fluent keeps its token height. The painted thickness stays well inside the existing widget
    // thickness (max(handleSize,trackHeight)+2*margin), so sizeHint/geometry is untouched.
    // zh_CN: 按设计语言的轨道厚度:M3 用更粗的双色轨道(expressive 外观)、macOS 发丝轨道,Fluent 保持原
    // Token 高度。绘制厚度始终落在既有控件厚度内,sizeHint/几何不变。
    int trackThickness = m_trackHeight;
    if (lang == DesignMaterial) trackThickness = 12;      // M3 thick rounded rail. zh_CN: M3 更粗的圆角轨道。
    else if (lang == DesignCupertino) trackThickness = 3; // macOS hairline rail. zh_CN: macOS 发丝轨道。

    // 1. Track geometry. zh_CN: 计算轨道几何区域。
    QRect trackRect(padding, cy - trackThickness / 2, width() - 2 * padding, trackThickness);

    int handleX = valueToPixelPos(opt.sliderPosition);
    QPointF center(handleX, cy);

    qreal filledWidth = handleX - padding;
    QRectF filledRect(padding, cy - trackThickness / 2, filledWidth, trackThickness);

    // 2. State colors. zh_CN: 确定状态颜色。
    // M3 inactive = secondary-container (a soft, desaturated accent tint), NOT a neutral gray — that
    // two-tone accent rail is the expressive cue (material-3.md §5). We derive the tint from accentDefault
    // (controlTertiary stays neutral in the M3 preset). macOS inactive = neutral tertiary gray; Fluent
    // keeps controlAltSecondary. zh_CN: M3 未填充段 = secondary-container(柔和、低饱和的强调色调),而非中性灰
    // ——这条双色强调轨道是 expressive 标志(material-3.md §5)。从 accentDefault 派生该色调(M3 预设里
    // controlTertiary 仍是中性)。macOS 未填充段 = 中性 tertiary 灰;Fluent 保持 controlAltSecondary。
    QColor trackBg = isEnabled() ? colors.controlAltSecondary : colors.controlDisabled;
    if (isEnabled() && lang == DesignMaterial) {
        // Soft accent tint ≈ secondary-container: accent flattened onto the surface at low alpha so it
        // reads as a light desaturated accent in light theme and a muted accent in dark.
        // zh_CN: 柔和强调色调 ≈ secondary-container:低透明度的强调色叠在表面上,浅色下为浅淡强调、深色下为暗哑强调。
        trackBg = colors.accentDefault;
        trackBg.setAlpha(darkTheme ? 0x4D : 0x33); // ~30% dark / ~20% light. zh_CN: 深色约 30%、浅色约 20%。
    } else if (isEnabled() && lang == DesignCupertino) {
        trackBg = colors.controlTertiary;
    }
    QColor trackFg = isEnabled() ? colors.accentDefault : colors.accentDisabled;

    // M3 leaves a small gap between the bar handle and each track segment; carve that gap out of both
    // the inactive and active rails so the handle reads as a separate element (material-3.md §5).
    // zh_CN: M3 在条形手柄与每段轨道之间留出小间隙;从未填充与已填充轨道两侧各挖出该间隙,使手柄成为独立元素。
    const qreal m3Gap = (lang == DesignMaterial) ? 4.0 : 0.0;
    QRectF inactiveRect = trackRect;
    if (m3Gap > 0.0) inactiveRect.setLeft(qMin<qreal>(handleX + m3Gap, trackRect.right()));
    if (m3Gap > 0.0) filledRect.setRight(qMax<qreal>(handleX - m3Gap, trackRect.left()));

    // 3. Paint the track background (the inactive portion). zh_CN: 绘制轨道背景(未填充段)。
    p.setPen(Qt::NoPen);
    p.setBrush(trackBg);
    p.drawRoundedRect(m3Gap > 0.0 ? inactiveRect : QRectF(trackRect), trackThickness/2.0, trackThickness/2.0);

    // 4. Paint the filled track. zh_CN: 绘制已填充轨道。
    if (filledRect.width() > 0.0) {
        p.setBrush(trackFg);
        p.drawRoundedRect(filledRect, trackThickness/2.0, trackThickness/2.0);
    }

    // 5. Animated thumb geometry. zh_CN: 拇指动画几何计算。
    qreal baseRadius = m_handleSize / 2.0;

    if (lang == DesignMaterial) {
        // Material 3 "expressive" handle: a vertical accent BAR/pill — a few px wide, taller than the
        // track, fully rounded — which is the distinctive M3 cue (material-3.md §5). A translucent accent
        // state-layer halo sits behind it on hover/press (§4, primary @ ~8/10%). The bar height stays
        // within the existing widget thickness, so geometry is unchanged.
        // zh_CN: Material 3「expressive」手柄:竖直的强调色条/胶囊——几像素宽、比轨道更高、完全圆角——
        // 这是 M3 的标志性提示(material-3.md §5)。hover/press 时其后绘制半透明强调色 state-layer 光晕
        // (§4,primary 约 8/10%)。条高保持在既有控件厚度内,几何不变。
        QColor barColor = isEnabled() ? colors.accentDefault : colors.accentDisabled;
        const qreal barW = 4.0;                                   // ≈4 dp wide bar. zh_CN: 约 4dp 宽。
        const qreal barH = qMin<qreal>(m_handleSize, height() - 2.0 * m_visualMargin); // taller than track. zh_CN: 高于轨道。
        QRectF barRect(center.x() - barW / 2.0, cy - barH / 2.0, barW, barH);

        // State-layer halo: circular primary veil behind the handle on hover/press. zh_CN: state-layer 光晕:
        // hover/press 时手柄后的圆形 primary 薄层。
        // The desired halo radius (baseRadius*1.6 = 16px at the default 20px handle) is LARGER than the
        // half-thickness of a default-thickness slider (sizeHint thickness 24 → half = 12), so an unclamped
        // ellipse pokes past the top/bottom edges and the widget clip flattens it into a hard-edged band —
        // the reported Material "background" artifact. Clamp the radius to the room actually available on
        // BOTH axes (distance from center to the nearest edge, minus the AA margin) so the halo always lies
        // fully inside the widget; never grow it beyond the design 1.6× value.
        // zh_CN: 目标光晕半径(baseRadius*1.6 = 默认 20px 手柄下为 16px)大于默认厚度滑块的半厚(sizeHint 厚度
        // 24 → 半厚 12),未夹紧的椭圆会越过上下边缘,被控件裁剪压成硬边色带——即所报告的 Material「背景」瑕疵。
        // 将半径夹到两轴实际可用空间(中心到最近边缘的距离,再减抗锯齿边距),使光晕始终完全落在控件内;且永不
        // 超过设计的 1.6× 值。
        qreal stateAlphaRatio = std::max(m_hoverRatio * 0.8, m_pressRatio); // hover ~8%, press ~10%. zh_CN: hover 约 8%,press 约 10%。
        if (isEnabled() && stateAlphaRatio > 0.01) {
            const qreal roomX = std::min<qreal>(center.x(), width() - center.x()) - m_visualMargin;
            const qreal roomY = std::min<qreal>(center.y(), height() - center.y()) - m_visualMargin;
            const qreal haloRadius = std::min({baseRadius * 1.6, roomX, roomY});
            if (haloRadius > 0.0) {
                QColor halo = colors.accentDefault;
                halo.setAlpha(static_cast<int>(0x1A * stateAlphaRatio)); // up to ~10%. zh_CN: 最高约 10%。
                p.setBrush(halo);
                p.setPen(Qt::NoPen);
                p.drawEllipse(center, haloRadius, haloRadius);
            }
        }
        p.setBrush(barColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(barRect, barW / 2.0, barW / 2.0);
    } else if (lang == DesignCupertino) {
        // macOS: a larger WHITE round knob with a hairline strokeStrong border + soft drop shadow; the
        // knob is clearly larger than the thin track (that contrast is the macOS cue, macos.md §4).
        // Hover/press apply a very subtle theme-aware veil over the knob.
        // zh_CN: macOS:更大的白色圆形旋钮,带 strokeStrong 发丝边框 + 柔和投影;旋钮明显大于细轨道
        // (这种对比是 macOS 的标志,macos.md §4)。hover/press 在旋钮上叠加极轻的主题感知薄层。
        QColor knobFill = isEnabled() ? QColor(Qt::white) : colors.controlDisabled;
        QColor knobBorder = isEnabled() ? colors.strokeStrong : colors.strokeDivider;
        // Soft drop shadow under the knob (a faint dark ellipse offset down). zh_CN: 旋钮下方的柔和投影
        // (略向下偏移的淡色椭圆)。
        if (isEnabled()) {
            QColor shadow(0, 0, 0, 40);
            p.setBrush(shadow);
            p.setPen(Qt::NoPen);
            p.drawEllipse(center + QPointF(0, 1.0), baseRadius + 0.5, baseRadius + 0.5);
        }
        p.setBrush(knobFill);
        p.setPen(QPen(knobBorder, 1));
        p.drawEllipse(center, baseRadius, baseRadius);
        // Subtle press/hover veil on the knob, visible on the white fill in both themes. zh_CN: 旋钮上的
        // 细微 hover/press 薄层,在两主题的白色填充上均可见。
        if (isEnabled()) {
            int v = static_cast<int>(std::max(m_hoverRatio * 18.0, m_pressRatio * 34.0));
            if (v > 0) {
                p.setBrush(veil(v));
                p.setPen(Qt::NoPen);
                p.drawEllipse(center, baseRadius, baseRadius);
            }
        }
    } else {
    // DesignFluent (default): unchanged WinUI thumb — white outer ring, accent inner dot.
    // zh_CN: 默认 Fluent,WinUI 拇指处理不变——白色外圈、强调色内圆点。
    // Matches the horizontal painter: white outer ring, accent inner dot that
    // grows from 0.45 (rest) to 0.7 (hover).
    // zh_CN: 与水平绘制一致——白色外圈、蓝色内圈扩展，0.45（静止）→ 0.7（悬停）。
    qreal innerScale = 0.45 + (0.25 * m_hoverRatio);
    qreal innerRadius = baseRadius * innerScale;

    // 6. Thumb colors. zh_CN: 确定拇指视觉颜色。
    // Outer ring: bgSolid fill (white in light theme) erases the track behind it;
    // painting a filled outer circle then the accent inner circle reads as a border.
    // zh_CN: 外圈用 bgSolid 填充（亮色主题为白色）以遮住轨道；先画填充外圆、再画
    // 蓝色实心内圆，视觉上即“边框”。

    QColor outerFillColor = colors.bgSolid; // White in the light theme. zh_CN: 亮色主题下为白色。
    QColor outerBorderColor = colors.strokeStrong; // Thin light-grey ring. zh_CN: 浅灰色细环。
    QColor innerColor = trackFg; // Accent color. zh_CN: Accent 颜色。

    if (!isEnabled()) {
        innerColor = colors.textDisabled;
        outerBorderColor = colors.strokeDivider;
    } else {
        if (m_pressRatio > 0.5) {
            innerColor = colors.accentTertiary;
            outerBorderColor = colors.strokeStrong; // Keep the ring. zh_CN: 保持边框。
        } else if (m_hoverRatio > 0.5) {
            innerColor = colors.accentSecondary;
        }
    }

    // 7. Paint the outer circle: filled container with a thin ring that masks
    // the track behind, producing the "white border" look.
    // zh_CN: 绘制外圆（白色填充带细边框），形成“白色外边框”遮罩，挡住背后的轨道。
    p.setBrush(outerFillColor);
    p.setPen(QPen(outerBorderColor, 1));
    p.drawEllipse(center, baseRadius, baseRadius);

    // 8. Paint the inner accent circle. zh_CN: 绘制内层圆形（蓝色中心）。
    p.setBrush(innerColor);
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, innerRadius, innerRadius);
    }

    // 9. Paint the ticks. zh_CN: 绘制刻度线。
    if (opt.tickPosition != QSlider::NoTicks && m_hoverRatio > 0.1) {
       int steps = (opt.maximum - opt.minimum) / opt.tickInterval;
        if (steps > 0 && steps < 100) {
             QColor tickColor = colors.textSecondary; 
             p.setPen(tickColor);
             for (int i = 0; i <= steps; ++i) {
                 int val = opt.minimum + i * opt.tickInterval;
                 int x = valueToPixelPos(val);
                 int ty = (opt.tickPosition == QSlider::TicksAbove) ? (trackRect.top() - 4) : (trackRect.bottom() + 4);
                 p.drawLine(x, ty, x, ty + 2);
             }
        }
    }
}

void Slider::drawVertical(QPainter& p, const QStyleOptionSlider& opt) {
    const auto& colors = themeColors();
    const auto& radius = themeRadius();

    // Brand design language + theme-aware interaction veil (see drawHorizontal). zh_CN: 品牌设计语言
    // + 主题感知交互薄层(见 drawHorizontal)。
    const auto lang = themeDesignLanguage();
    const bool darkTheme = effectiveTheme() == fluent::FluentElement::Dark;
    const auto veil = [darkTheme](int a) { return darkTheme ? QColor(255, 255, 255, a) : QColor(0, 0, 0, a); };

    const int cx = width() / 2;
    const int padding = m_handleSize / 2 + m_visualMargin;

    // Per-language track thickness, matching drawHorizontal. The painted thickness stays inside the
    // existing widget thickness, so sizeHint/geometry is untouched. zh_CN: 按设计语言的轨道厚度,与
    // drawHorizontal 一致。绘制厚度始终落在既有控件厚度内,sizeHint/几何不变。
    int trackThickness = m_trackHeight;
    if (lang == DesignMaterial) trackThickness = 12;      // M3 thick rounded rail. zh_CN: M3 更粗的圆角轨道。
    else if (lang == DesignCupertino) trackThickness = 3; // macOS hairline rail. zh_CN: macOS 发丝轨道。

    // 1. Track geometry. zh_CN: 计算轨道几何区域。
    QRect trackRect(cx - trackThickness / 2, padding, trackThickness, height() - 2 * padding);

    int handleY = valueToPixelPos(opt.sliderPosition);
    QPointF center(cx, handleY);

    qreal filledHeight = trackRect.bottom() - handleY;
    QRectF filledRect(cx - trackThickness / 2, handleY, trackThickness, filledHeight);

    // 2. State colors. zh_CN: 确定状态颜色。
    // M3 inactive = secondary-container soft accent tint (derived from accentDefault), NOT neutral gray;
    // macOS inactive = neutral tertiary gray; Fluent keeps controlAltSecondary. (See drawHorizontal.)
    // zh_CN: M3 未填充段 = secondary-container 柔和强调色调(由 accentDefault 派生),而非中性灰;
    // macOS 未填充段 = 中性 tertiary 灰;Fluent 保持 controlAltSecondary。(见 drawHorizontal。)
    QColor trackBg = isEnabled() ? colors.controlAltSecondary : colors.controlDisabled;
    if (isEnabled() && lang == DesignMaterial) {
        trackBg = colors.accentDefault;
        trackBg.setAlpha(darkTheme ? 0x4D : 0x33); // ~30% dark / ~20% light. zh_CN: 深色约 30%、浅色约 20%。
    } else if (isEnabled() && lang == DesignCupertino) {
        trackBg = colors.controlTertiary;
    }
    QColor trackFg = isEnabled() ? colors.accentDefault : colors.accentDisabled;

    // M3 gap between bar handle and each track segment (see drawHorizontal). zh_CN: M3 条形手柄与各轨道段
    // 之间的间隙(见 drawHorizontal)。
    const qreal m3Gap = (lang == DesignMaterial) ? 4.0 : 0.0;
    QRectF inactiveRect = trackRect;
    if (m3Gap > 0.0) inactiveRect.setBottom(qMax<qreal>(handleY - m3Gap, trackRect.top()));
    if (m3Gap > 0.0) filledRect.setTop(qMin<qreal>(handleY + m3Gap, trackRect.bottom()));

    // 3. Paint the track background (the inactive portion). zh_CN: 绘制轨道背景(未填充段)。
    p.setPen(Qt::NoPen);
    p.setBrush(trackBg);
    p.drawRoundedRect(m3Gap > 0.0 ? inactiveRect : QRectF(trackRect), trackThickness/2.0, trackThickness/2.0);

    // 4. Paint the filled track. zh_CN: 绘制已填充轨道。
    if (filledRect.height() > 0.0) {
        p.setBrush(trackFg);
        p.drawRoundedRect(filledRect, trackThickness/2.0, trackThickness/2.0);
    }

    // 5. Animated thumb geometry. zh_CN: 拇指动画几何计算。
    qreal baseRadius = m_handleSize / 2.0;

    if (lang == DesignMaterial) {
        // Material 3 "expressive" handle: a bar/pill perpendicular to the track (horizontal here, since
        // the track is vertical) — taller than the track is wide, a few px thick, fully rounded — plus a
        // primary state-layer halo on hover/press (material-3.md §4/§5). Stays within widget thickness.
        // zh_CN: Material 3「expressive」手柄:垂直于轨道的条/胶囊(此处为水平,因轨道竖直)——比轨道宽更长、
        // 几像素厚、完全圆角——并在 hover/press 时叠加 primary state-layer 光晕(material-3.md §4/§5)。
        // 保持在控件厚度内。
        QColor barColor = isEnabled() ? colors.accentDefault : colors.accentDisabled;
        const qreal barH = 4.0;                                   // ≈4 dp thick bar. zh_CN: 约 4dp 厚。
        const qreal barW = qMin<qreal>(m_handleSize, width() - 2.0 * m_visualMargin); // wider than track. zh_CN: 宽于轨道。
        QRectF barRect(cx - barW / 2.0, center.y() - barH / 2.0, barW, barH);

        // Clamp the halo to fit fully within the widget on both axes (see drawHorizontal): the desired
        // baseRadius*1.6 radius exceeds the half-thickness of a default-thickness slider, so an unclamped
        // ellipse would be clipped into a hard band. zh_CN: 将光晕夹到两轴均完全落在控件内(见 drawHorizontal):
        // 目标 baseRadius*1.6 半径大于默认厚度滑块的半厚,未夹紧的椭圆会被裁剪成硬色带。
        qreal stateAlphaRatio = std::max(m_hoverRatio * 0.8, m_pressRatio); // hover ~8%, press ~10%. zh_CN: hover 约 8%,press 约 10%。
        if (isEnabled() && stateAlphaRatio > 0.01) {
            const qreal roomX = std::min<qreal>(center.x(), width() - center.x()) - m_visualMargin;
            const qreal roomY = std::min<qreal>(center.y(), height() - center.y()) - m_visualMargin;
            const qreal haloRadius = std::min({baseRadius * 1.6, roomX, roomY});
            if (haloRadius > 0.0) {
                QColor halo = colors.accentDefault;
                halo.setAlpha(static_cast<int>(0x1A * stateAlphaRatio)); // up to ~10%. zh_CN: 最高约 10%。
                p.setBrush(halo);
                p.setPen(Qt::NoPen);
                p.drawEllipse(center, haloRadius, haloRadius);
            }
        }
        p.setBrush(barColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(barRect, barH / 2.0, barH / 2.0);
    } else if (lang == DesignCupertino) {
        // macOS: larger WHITE round knob, hairline strokeStrong border + soft drop shadow; knob clearly
        // larger than the thin track. Subtle theme-aware veil on hover/press. zh_CN: macOS:更大的白色圆形
        // 旋钮,strokeStrong 发丝边框 + 柔和投影;旋钮明显大于细轨道。hover/press 叠加细微主题感知薄层。
        QColor knobFill = isEnabled() ? QColor(Qt::white) : colors.controlDisabled;
        QColor knobBorder = isEnabled() ? colors.strokeStrong : colors.strokeDivider;
        if (isEnabled()) {
            QColor shadow(0, 0, 0, 40);
            p.setBrush(shadow);
            p.setPen(Qt::NoPen);
            p.drawEllipse(center + QPointF(0, 1.0), baseRadius + 0.5, baseRadius + 0.5);
        }
        p.setBrush(knobFill);
        p.setPen(QPen(knobBorder, 1));
        p.drawEllipse(center, baseRadius, baseRadius);
        if (isEnabled()) {
            int v = static_cast<int>(std::max(m_hoverRatio * 18.0, m_pressRatio * 34.0));
            if (v > 0) {
                p.setBrush(veil(v));
                p.setPen(Qt::NoPen);
                p.drawEllipse(center, baseRadius, baseRadius);
            }
        }
    } else {
    // DesignFluent (default): unchanged WinUI thumb. zh_CN: 默认 Fluent,WinUI 拇指处理不变。
    // Matches the horizontal painter: white outer ring, accent inner dot that
    // grows from 0.45 (rest) to 0.7 (hover).
    // zh_CN: 与水平绘制一致——白色外圈、蓝色内圈扩展，0.45（静止）→ 0.7（悬停）。
    qreal innerScale = 0.45 + (0.25 * m_hoverRatio);
    qreal innerRadius = baseRadius * innerScale;

    // 6. Thumb colors. zh_CN: 确定拇指视觉颜色。
    QColor outerFillColor = colors.bgSolid;
    QColor outerBorderColor = colors.strokeStrong;
    QColor innerColor = trackFg;

    if (!isEnabled()) {
        innerColor = colors.textDisabled;
        outerBorderColor = colors.strokeDivider;
    } else {
        if (m_pressRatio > 0.5) {
            innerColor = colors.accentTertiary;
        } else if (m_hoverRatio > 0.5) {
            innerColor = colors.accentSecondary;
        }
    }

    // 7. Paint the outer circle. zh_CN: 绘制外层圆形。
    p.setBrush(outerFillColor);
    p.setPen(QPen(outerBorderColor, 1));
    p.drawEllipse(center, baseRadius, baseRadius);

    // 8. Paint the inner circle. zh_CN: 绘制内层圆形。
    p.setBrush(innerColor);
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, innerRadius, innerRadius);
    }
}

} // namespace fluent::basicinput

