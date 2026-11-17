#ifndef SCROLLBAR_H
#define SCROLLBAR_H

#include "compatibility/InputEventCompat.h"
#include <QScrollBar>
#include <QEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QShowEvent>
#include <QTimer>
#include <QPropertyAnimation>
#include <QMargins>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::scrolling {

/**
 * @brief Fluent overlay-style scrollbar with configurable thickness and auto-hide fade animation.
 * Wraps QScrollBar with a compact painted track/thumb that fades in on interaction or value changes and fades out when idle.
 * zh_CN: 带自动淡入淡出和可调厚度的 Fluent 覆盖式滚动条。
 * 基于 QScrollBar 实现紧凑自绘滑块，在交互或滚动值变化时淡入，空闲后自动淡出隐藏。
 */
class ScrollBar : public QScrollBar, public FluentElement, public QMLPlus {
    Q_OBJECT

    /**
     * @brief Scrollbar thickness in pixels.
     * zh_CN: 滚动条厚度，单位为像素。
     */
    Q_PROPERTY(int thickness READ thickness WRITE setThickness NOTIFY thicknessChanged)
    /**
     * @brief Current scrollbar opacity used by fade animation.
     * zh_CN: 淡入淡出动画使用的当前滚动条不透明度。
     */
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
    explicit ScrollBar(Qt::Orientation orientation, QWidget* parent = nullptr);
    explicit ScrollBar(QWidget* parent = nullptr);
    ~ScrollBar() override;

    int thickness() const { return m_thickness; }
    void setThickness(int thickness);

    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal value);

signals:
    void thicknessChanged();

protected:
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void paintEvent(QPaintEvent* event) override;

    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;

    void onThemeUpdated() override;

private:
    void init();

    int m_thickness = 7;       // Total bar width/height (px).
    int m_minThumbLength = 16; // Minimum thumb length (px).
    QMargins m_thumbPadding = QMargins(1, 1, 1, 1);

    bool m_isHovered = false;
    bool m_isPressed = false;

    qreal m_opacity = 0.0;
    QPropertyAnimation* m_opacityAnim = nullptr;
    QTimer* m_autoHideTimer = nullptr;

    void ensureAnimation();
    void showWithAutoHide();
};

} // namespace fluent::scrolling

#endif // SCROLLBAR_H
