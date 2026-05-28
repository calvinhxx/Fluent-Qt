#include "DebugOverlay.h"
#include <QPainter>
#include <QEvent>
#include <QPen>

DebugOverlay::DebugOverlay(QWidget *target, const QColor &color, QWidget *parent)
    : QWidget(parent ? parent : (target ? target->parentWidget() : nullptr))
    , m_target(target)
    , m_color(color)
{
    if (!m_target) return;

    // 设置属性：不接受鼠标事件（穿透）、不抢占焦点
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    
    // 安装事件过滤器，监听目标控件的移动和缩放
    m_target->installEventFilter(this);
    
    // 初始对齐
    updateGeometryToTarget();
    show();
}

bool DebugOverlay::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_target) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
            updateGeometryToTarget();
        } else if (event->type() == QEvent::Hide) {
            hide();
        } else if (event->type() == QEvent::Show) {
            show();
            updateGeometryToTarget();
        }
    }
    return QWidget::eventFilter(obj, event);
}

void DebugOverlay::updateGeometryToTarget() {
    if (m_target) {
        // 将高亮框的大小和位置设置得与目标一致
        // 注意：这里假设 overlay 和 target 在同一个父控件下
        setGeometry(m_target->geometry());
        raise(); // 确保高亮框在最上层
    }
}

void DebugOverlay::paintEvent(QPaintEvent *) {
    if (!m_target) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 画一个描边框
    QPen pen(m_color, m_thickness);
    // 习惯上使用虚线更容易看清对齐情况
    pen.setStyle(Qt::DashLine); 
    painter.setPen(pen);
    
    painter.drawRect(QRectF(0, 0, width() - m_thickness, height() - m_thickness));
}