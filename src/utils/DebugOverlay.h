#ifndef DEBUGOVERLAY_H
#define DEBUGOVERLAY_H

#include <QWidget>
#include <QPointer>
#include <QColor>

/**
 * @brief Draws a non-interactive debug outline over a target widget.
 * zh_CN: 在目标控件上方绘制不响应鼠标事件的调试描边。
 *
 * DebugOverlay assumes the target and overlay share the same parent widget so
 * the target geometry can be reused directly.
 * zh_CN: DebugOverlay 假设目标控件和 overlay 处于同一个父控件下，因此可以直接
 * 复用目标控件的 geometry。
 */
class DebugOverlay : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Creates an outline that tracks the target widget.
     * zh_CN: 创建一个跟随目标控件位置和尺寸的调试描边。
     */
    explicit DebugOverlay(QWidget *target, const QColor &color = Qt::red, QWidget *parent = nullptr);
    
    void setColor(const QColor &c) { m_color = c; update(); }
    void setThickness(int t) { m_thickness = t; update(); }

protected:
    // Tracks target move/show/hide/resize events without taking mouse input.
    // zh_CN: 追踪目标控件移动、显示、隐藏和缩放事件，同时保持鼠标穿透。
    bool eventFilter(QObject *obj, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void updateGeometryToTarget();

    QPointer<QWidget> m_target; // Weak reference; the target may be destroyed first.
    QColor m_color;
    int m_thickness = 1;
};

#endif // DEBUGOVERLAY_H
