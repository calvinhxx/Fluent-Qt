#include "RadioButton.h"
#include <QPainter>
#include <QPropertyAnimation>
#include "design/Typography.h"

namespace fluent::basicinput {

RadioButton::RadioButton(const QString& text, QWidget* parent)
    : QRadioButton(text, parent) {
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::ArrowCursor);

    m_textFont = themeFont("Body").toQFont();
    setFont(m_textFont);
    initAnimation();
}

RadioButton::RadioButton(QWidget* parent)
    : RadioButton("", parent) {
}

void RadioButton::initAnimation() {
    m_checkAnimation = new QPropertyAnimation(this, "checkProgress");
    m_checkAnimation->setDuration(themeAnimation().fast);
    m_checkAnimation->setEasingCurve(themeAnimation().decelerate);

    m_dotScaleAnimation = new QPropertyAnimation(this, "dotScale");
    m_dotScaleAnimation->setDuration(themeAnimation().fast);
    m_dotScaleAnimation->setEasingCurve(themeAnimation().decelerate);
}

void RadioButton::setCheckProgress(qreal progress) {
    m_checkProgress = progress;
    update();
}

void RadioButton::setDotScale(qreal scale) {
    if (!qFuzzyCompare(m_dotScale, scale)) {
        m_dotScale = scale;
        update();
    }
}

void RadioButton::setCircleSize(int size) {
    if (m_circleSize != size) {
        m_circleSize = size;
        updateGeometry();
        update();
        emit circleSizeChanged();
    }
}

void RadioButton::setTextGap(int gap) {
    if (m_textGap != gap) {
        m_textGap = gap;
        updateGeometry();
        update();
        emit textGapChanged();
    }
}

void RadioButton::setTextFont(const QFont& font) {
    if (m_textFont != font) {
        m_textFont = font;
        setFont(m_textFont);
        updateGeometry();
        update();
        emit textFontChanged();
    }
}

void RadioButton::nextCheckState() {
    QRadioButton::nextCheckState();
    // Reset the hover scale. zh_CN: 重置 hover 缩放。
    m_dotScale = 1.0;
    if (m_checkAnimation) {
        m_checkAnimation->stop();
        m_checkAnimation->setStartValue(0.0);
        m_checkAnimation->setEndValue(1.0);
        m_checkAnimation->start();
    }
}

void RadioButton::onThemeUpdated() {
    updateGeometry();
    update();
}

QSize RadioButton::sizeHint() const {
    QFontMetrics fm(m_textFont);
    int w = m_circleSize;
    if (!text().isEmpty()) {
        w += m_textGap + fm.horizontalAdvance(text());
    }
    int h = qMax(m_circleSize, fm.height());
    return QSize(w, h);
}

QSize RadioButton::minimumSizeHint() const {
    return sizeHint();
}

void RadioButton::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColors();

    bool isHover = underMouse();
    bool isPressed = isDown();
    bool enabled = isEnabled();
    bool checked = isChecked();

    // Per-brand shape + interaction, mirroring Button::paintEvent's design-language branch.
    // zh_CN: 与 Button::paintEvent 的设计语言分支一致,按品牌切换形状与交互。
    const auto lang = themeDesignLanguage();   // inherited from FluentElement. zh_CN: 继承自 FluentElement。
    const bool darkTheme = effectiveTheme() == fluent::FluentElement::Dark;
    // Theme-aware veil: darkens light surfaces, lightens dark ones — keeps hover/press visible in BOTH
    // App themes. zh_CN: 主题感知薄层:浅色面变暗、深色面变亮,使 hover/press 在明暗两主题下都可见。
    const auto veil = [darkTheme](int a) { return darkTheme ? QColor(255, 255, 255, a) : QColor(0, 0, 0, a); };
    const auto withAlpha = [](QColor c, int a) { c.setAlpha(a); return c; };

    // ── 1. Outer ring. zh_CN: 外圈 ───────────────────────────────
    QRectF circleRect(0, 0, m_circleSize, m_circleSize);

    // checkProgress drives check/uncheck; dotScale drives hover scaling. Shared by all languages.
    // zh_CN: checkProgress 驱动选中/取消动画,dotScale 驱动 hover 缩放。各设计语言共用。
    qreal dotScale = checked ? m_checkProgress : (1.0 - m_checkProgress);
    dotScale *= m_dotScale;

    if (lang == DesignMaterial) {
        // Material 3 (§5 Radio): two concentric accent circles. SELECTED = a 2dp accent ring + a filled
        // accent center dot (≈10dp, ~50% of the inner area) with a clear gap. UNSELECTED = a 2dp
        // on-surface-variant (strokeStrong) ring, no dot. Hover/press raise a circular 40dp state-layer
        // halo behind the radio: primary-tinted when selected, on-surface veil when not (§4).
        // zh_CN: Material 3(§5 Radio):两个同心强调色圆环。选中=2dp 强调色外环 + 居中实心强调色圆点
        // (约 10dp,约占内圈面积 50%,留明显间隙);未选中=2dp on-surface-variant(strokeStrong)描边环,无圆点;
        // hover/press 在控件后方升起圆形 40dp state-layer 光晕:选中时为 primary 色,未选中时为 on-surface 薄层(§4)。
        const qreal ringWidth = 2.0;
        QColor ringColor = !enabled ? colors.strokeDivider
                                    : (checked ? colors.accentDefault : colors.strokeStrong);

        // State-layer halo: a circular 40dp halo behind the ring on hover/press. Per §4 it is the
        // primary color when selected and the on-surface (theme-aware veil) color when not, at ~8% hover /
        // ~10% press. zh_CN: state-layer 光晕:hover/press 时控件后方的圆形 40dp 光晕。依 §4,选中时取 primary 色、
        // 未选中时取 on-surface(主题感知薄层)色,透明度约 8% hover / 10% press。
        if (enabled && (isHover || isPressed)) {
            const int a = isPressed ? 0x1A : 0x14; // ~10% / ~8% alpha. zh_CN: 约 10% / 8% 透明度。
            const QColor haloColor = checked ? withAlpha(colors.accentDefault, a) : veil(a);
            // Clamp the halo so its ellipse lies FULLY inside the widget's rect() — never clipped into a
            // hard-edged band by the backing-store clip. The ring sits flush-left, so the largest radius
            // that keeps the circle within all four edges is the min distance from the ring center to each
            // edge (here ≈ the ring radius). zh_CN: 将光晕收紧到完全落在控件 rect() 内,避免被裁成硬边色带。
            // 圆环贴左对齐,故最大半径取圆心到四边距离的最小值(此处约等于圆环半径)。
            const QPointF c = circleRect.center();
            const qreal haloRadius = qMin(qMin(c.x(), width() - c.x()),
                                          qMin(c.y(), height() - c.y()));
            painter.setPen(Qt::NoPen);
            painter.setBrush(haloColor);
            painter.drawEllipse(c, haloRadius, haloRadius);
        }

        // Outer ring stroke. zh_CN: 外环描边。
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(ringColor, ringWidth));
        const qreal inset = ringWidth / 2.0;
        painter.drawEllipse(circleRect.adjusted(inset, inset, -inset, -inset));

        // Center dot (checked): filled accent, ≈10dp (~50% of the 20dp ring, leaving a clear gap to the
        // 2dp stroke). zh_CN: 内圆点(选中):实心强调色,约 10dp(占 20dp 外环约 50%,与 2dp 描边留明显间隙)。
        if ((checked || m_checkProgress < 1.0) && dotScale > 0.0) {
            const qreal dotDiameter = m_circleSize * 0.5; // ≈10dp on the 20dp ring. zh_CN: 20dp 外环上约 10dp。
            qreal d = dotDiameter * dotScale;
            QRectF dotRect(circleRect.center().x() - d / 2, circleRect.center().y() - d / 2, d, d);
            painter.setPen(Qt::NoPen);
            painter.setBrush(enabled ? colors.accentDefault : colors.textDisabled);
            painter.drawEllipse(dotRect);
        }
    } else if (lang == DesignCupertino) {
        // macOS: SELECTED = a filled accent circle with a small WHITE center pip. UNSELECTED = a control
        // (white) fill plus a thin 1px stroke. Hover/press = a subtle veil over the whole circle.
        // zh_CN: macOS:选中=实心强调色圆 + 居中白色小点;未选中=控件(白)填充 + 1px 细描边;hover/press=整圆上叠加薄层。
        QColor circleFill = !enabled ? colors.controlDisabled
                                     : (checked ? colors.accentDefault : colors.controlDefault);
        QColor circleBorder = !enabled ? colors.strokeDivider
                                       : (checked ? colors.accentDefault.darker(112) : colors.strokeStrong);

        painter.setPen(Qt::NoPen);
        painter.setBrush(circleFill);
        painter.drawEllipse(circleRect);

        // Subtle veil over the circle for hover/press feedback (both themes). zh_CN: hover/press 时整圆叠加薄层(明暗皆可见)。
        if (enabled && (isHover || isPressed)) {
            painter.setBrush(veil(isPressed ? 0x33 : 0x1C)); // ~20% / ~11%. zh_CN: 约 20% / 11%。
            painter.drawEllipse(circleRect);
        }

        // Hairline border. zh_CN: 发丝描边。
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(circleBorder, 1.0));
        painter.drawEllipse(circleRect.adjusted(0.5, 0.5, -0.5, -0.5));

        // White center pip (checked). zh_CN: 居中白色小点(选中)。
        if ((checked || m_checkProgress < 1.0) && dotScale > 0.0) {
            const qreal dotDiameter = m_circleSize * 0.4; // a small pip. zh_CN: 较小的圆点。
            qreal d = dotDiameter * dotScale;
            QRectF dotRect(circleRect.center().x() - d / 2, circleRect.center().y() - d / 2, d, d);
            painter.setPen(Qt::NoPen);
            painter.setBrush(enabled ? QColor(Qt::white) : colors.textDisabled);
            painter.drawEllipse(dotRect);
        }
    } else {
        // DesignFluent (default): unchanged WinUI treatment. zh_CN: 默认 Fluent,WinUI 处理不变。
        QColor outerBg, outerBorder;

        if (!enabled) {
            outerBg = colors.controlDisabled;
            outerBorder = colors.strokeDivider;
        } else if (checked) {
            // WinUI 3: the checked ring fills with accent and has no outline.
            // zh_CN: 选中态外圈用 Accent 填充，无独立描边。
            outerBg = isPressed ? colors.accentTertiary
                                : (isHover ? colors.accentSecondary : colors.accentDefault);
            outerBorder = Qt::transparent;
        } else {
            // Unchecked: standard control fill plus outline. zh_CN: 未选中——普通控件底色 + 描边。
            outerBg = isPressed ? colors.controlTertiary
                                : (isHover ? colors.controlSecondary : colors.controlDefault);
            outerBorder = isHover ? colors.strokeStrong : colors.strokeDefault;
        }

        // Paint the ring fill (circle). zh_CN: 绘制外圈底色（圆形）。
        painter.setPen(Qt::NoPen);
        painter.setBrush(outerBg);
        painter.drawEllipse(circleRect);

        // Paint the ring outline (unchecked only). zh_CN: 绘制外圈描边（仅未选中态）。
        if (outerBorder != Qt::transparent) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(outerBorder, 1.0));
            painter.drawEllipse(circleRect.adjusted(0.5, 0.5, -0.5, -0.5));
        }

        // ── 2. Inner dot (checked). zh_CN: 内圆点（选中态）──────────
        if (checked || m_checkProgress < 1.0) {
            QColor dotColor = enabled ? colors.textOnAccent : colors.textDisabled;

            // Dot diameter ≈ 50% of the ring, the typical WinUI 3 ratio.
            // zh_CN: 内圆点直径约为外圈的 50%，WinUI 3 典型比例。
            const qreal dotDiameter = m_circleSize * 0.5;

            // checkProgress drives check/uncheck; dotScale drives hover scaling.
            // zh_CN: checkProgress 驱动选中/取消动画，dotScale 驱动 hover 缩放。
            qreal scale = checked ? m_checkProgress : (1.0 - m_checkProgress);
            scale *= m_dotScale;
            if (scale > 0.0) {
                qreal d = dotDiameter * scale;
                QRectF dotRect(circleRect.center().x() - d / 2,
                               circleRect.center().y() - d / 2,
                               d, d);
                painter.setPen(Qt::NoPen);
                painter.setBrush(dotColor);
                painter.drawEllipse(dotRect);
            }
        }
    }

    // ── 3. Text. zh_CN: 文本 ─────────────────────────────────────
    if (!text().isEmpty()) {
        painter.setFont(m_textFont);
        painter.setPen(enabled ? colors.textPrimary : colors.textDisabled);
        QRectF textRect = rect().adjusted(m_circleSize + m_textGap, 0, 0, 0);
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());
    }
}

void RadioButton::enterEvent(FluentEnterEvent* event) {
    QRadioButton::enterEvent(event);
    if (isChecked() && isEnabled()) {
        m_dotScaleAnimation->stop();
        m_dotScaleAnimation->setStartValue(m_dotScale);
        m_dotScaleAnimation->setEndValue(1.2); // The dot grows 20% on hover. zh_CN: hover 时内圆点放大 20%。
        m_dotScaleAnimation->start();
    }
}

void RadioButton::leaveEvent(QEvent* event) {
    QRadioButton::leaveEvent(event);
    if (isEnabled()) {
        m_dotScaleAnimation->stop();
        m_dotScaleAnimation->setStartValue(m_dotScale);
        m_dotScaleAnimation->setEndValue(1.0);
        m_dotScaleAnimation->start();
    }
}

} // namespace fluent::basicinput
