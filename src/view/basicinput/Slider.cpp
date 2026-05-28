#include "Slider.h"
#include "view/textfields/Label.h"
#include "view/status_info/ToolTip.h"

#include <QPainter>
#include <QStyleOptionSlider>
#include <QStyle>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <algorithm>

#include "design/Typography.h"

namespace view::basicinput {

using namespace view::textfields;

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
        m_toolTip = new view::status_info::ToolTip(nullptr); // Top level window
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

    const int cy = height() / 2;
    const int padding = m_handleSize / 2 + m_visualMargin;
    
    // 1. 计算轨道几何区域
    QRect trackRect(padding, cy - m_trackHeight / 2, width() - 2 * padding, m_trackHeight);
    
    int handleX = valueToPixelPos(opt.sliderPosition);
    QPointF center(handleX, cy);

    qreal filledWidth = handleX - padding;
    QRectF filledRect(padding, cy - m_trackHeight / 2, filledWidth, m_trackHeight);

    // 2. 确定状态颜色
    QColor trackBg = isEnabled() ? colors.controlAltSecondary : colors.controlDisabled;
    QColor trackFg = isEnabled() ? colors.accentDefault : colors.accentDisabled;
    
    // 3. 绘制轨道背景
    p.setPen(Qt::NoPen);
    p.setBrush(trackBg);
    p.drawRoundedRect(trackRect, m_trackHeight/2.0, m_trackHeight/2.0);

    // 4. 绘制已填充轨道
    if (filledWidth > 0.0) {
        p.setBrush(trackFg);
        p.drawRoundedRect(filledRect, m_trackHeight/2.0, m_trackHeight/2.0);
    }

    // 5. 拇指动画几何计算
    qreal baseRadius = m_handleSize / 2.0; 
    
    // 与水平绘制保持一致: 白色外圈，蓝色内圈扩展
    // 缩小尺寸: 0.45 (静止) -> 0.7 (悬停)
    qreal innerScale = 0.45 + (0.25 * m_hoverRatio); 
    qreal innerRadius = baseRadius * innerScale; 

    // 6. 确定拇指视觉颜色
    // 外层边框颜色: 白色（或者 bgSolid 用于清除轨道痕迹）
    // 实际上，为了看起来像 "边框"，我们先画一个填充背景色的外圆，再在内部画蓝色实心圆
    
    QColor outerFillColor = colors.bgSolid; // 亮色主题下为白色
    QColor outerBorderColor = colors.strokeStrong; // 浅灰色细环
    QColor innerColor = trackFg; // Accent 颜色

    if (!isEnabled()) {
        innerColor = colors.textDisabled;
        outerBorderColor = colors.strokeDivider;
    } else {
        if (m_pressRatio > 0.5) {
            innerColor = colors.accentTertiary;
            outerBorderColor = colors.strokeStrong; // 保持边框
        } else if (m_hoverRatio > 0.5) {
            innerColor = colors.accentSecondary;
        }
    }
    
    // 7. 绘制外层圆形 (容器 - 白色填充带细边框)
    // 这样产生 "白色外边框" 遮罩效果，挡住背后的轨道
    p.setBrush(outerFillColor);
    p.setPen(QPen(outerBorderColor, 1));
    p.drawEllipse(center, baseRadius, baseRadius);

    // 8. 绘制内层圆形 (蓝色中心)
    p.setBrush(innerColor);
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, innerRadius, innerRadius);
    
    // 9. 绘制刻度线 (Ticks)
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

    const int cx = width() / 2;
    const int padding = m_handleSize / 2 + m_visualMargin;
    
    // 1. 计算轨道几何区域
    QRect trackRect(cx - m_trackHeight / 2, padding, m_trackHeight, height() - 2 * padding);
    
    int handleY = valueToPixelPos(opt.sliderPosition);
    QPointF center(cx, handleY);

    qreal filledHeight = trackRect.bottom() - handleY; 
    QRectF filledRect(cx - m_trackHeight / 2, handleY, m_trackHeight, filledHeight);

    // 2. 确定状态颜色
    QColor trackBg = isEnabled() ? colors.controlAltSecondary : colors.controlDisabled;
    QColor trackFg = isEnabled() ? colors.accentDefault : colors.accentDisabled;

    // 3. 绘制轨道背景
    p.setPen(Qt::NoPen);
    p.setBrush(trackBg);
    p.drawRoundedRect(trackRect, m_trackHeight/2.0, m_trackHeight/2.0);

    // 4. 绘制已填充轨道
    if (filledHeight > 0.0) {
        p.setBrush(trackFg);
        p.drawRoundedRect(filledRect, m_trackHeight/2.0, m_trackHeight/2.0);
    }
    
    // 5. 拇指动画几何计算
    qreal baseRadius = m_handleSize / 2.0; 
    
    // 与水平绘制保持一致: 白色外圈，蓝色内圈扩展
    // 缩小尺寸: 0.45 (静止) -> 0.7 (悬停)
    qreal innerScale = 0.45 + (0.25 * m_hoverRatio); 
    qreal innerRadius = baseRadius * innerScale; 

    // 6. 确定拇指视觉颜色
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
    
    // 7. 绘制外层圆形
    p.setBrush(outerFillColor);
    p.setPen(QPen(outerBorderColor, 1));
    p.drawEllipse(center, baseRadius, baseRadius);

    // 8. 绘制内层圆形
    p.setBrush(innerColor);
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, innerRadius, innerRadius);
}

} // namespace view::basicinput

