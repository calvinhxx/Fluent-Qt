#include "RadioButton.h"
#include <QPainter>
#include <QPropertyAnimation>
#include "design/Typography.h"

namespace view::basicinput {

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
    // 重置 hover 缩放
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

    // ── 1. 外圈 ──────────────────────────────────────────────────
    QRectF circleRect(0, 0, m_circleSize, m_circleSize);

    QColor outerBg, outerBorder;

    if (!enabled) {
        outerBg = colors.controlDisabled;
        outerBorder = colors.strokeDivider;
    } else if (checked) {
        // WinUI 3: 选中态外圈用 Accent 填充，无独立描边
        outerBg = isPressed ? colors.accentTertiary
                            : (isHover ? colors.accentSecondary : colors.accentDefault);
        outerBorder = Qt::transparent;
    } else {
        // 未选中：普通控件底色 + 描边
        outerBg = isPressed ? colors.controlTertiary
                            : (isHover ? colors.controlSecondary : colors.controlDefault);
        outerBorder = isHover ? colors.strokeStrong : colors.strokeDefault;
    }

    // 绘制外圈底色（圆形）
    painter.setPen(Qt::NoPen);
    painter.setBrush(outerBg);
    painter.drawEllipse(circleRect);

    // 绘制外圈描边（仅未选中态）
    if (outerBorder != Qt::transparent) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(outerBorder, 1.0));
        painter.drawEllipse(circleRect.adjusted(0.5, 0.5, -0.5, -0.5));
    }

    // ── 2. 内圆点（选中态）──────────────────────────────────────
    if (checked || m_checkProgress < 1.0) {
        QColor dotColor = enabled ? colors.textOnAccent : colors.textDisabled;

        // 内圆点直径 ≈ 外圈的 50%，WinUI 3 典型比例
        const qreal dotDiameter = m_circleSize * 0.5;

        // checkProgress 驱动选中/取消动画，dotScale 驱动 hover 缩放
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

    // ── 3. 文本 ──────────────────────────────────────────────────
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
        m_dotScaleAnimation->setEndValue(1.2); // hover 时内圆点放大 20%
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

} // namespace view::basicinput
