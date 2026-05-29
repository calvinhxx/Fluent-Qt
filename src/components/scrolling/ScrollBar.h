#ifndef SCROLLBAR_H
#define SCROLLBAR_H

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
 * @brief Fluent scrollbar with custom thickness and opacity animation.
 * zh_CN: 具备自定义厚度和透明度动画的 Fluent 滚动条。
 *
 * ScrollBar wraps QScrollBar behavior with a compact painted surface for Fluent
 * scroll indicators and transient visibility patterns.
 * zh_CN: ScrollBar 在 QScrollBar 行为外包裹紧凑自绘表面，用于 Fluent 滚动指示器
 * 和临时显隐模式。
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
    explicit ScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr);
    explicit ScrollBar(QWidget *parent = nullptr);
    ~ScrollBar() override;

    int thickness() const { return m_thickness; }
    void setThickness(int thickness);

    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal value);

signals:
    void thicknessChanged();

protected:
    // 尺寸提示：厚度由 thickness 控制，长度沿用 QScrollBar 默认逻辑
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void paintEvent(QPaintEvent *event) override;

    void enterEvent(FluentEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

    void onThemeUpdated() override;

private:
    void init();

    // --- 尺寸与视觉参数（避免在 cpp 中硬编码 magic number） ---
    int      m_thickness      = 7;                     ///< 控件实际厚度（像素），始终占位此宽度/高度
    int      m_minThumbLength = 16;                    ///< Thumb 的最小长度（像素）
    QMargins m_thumbPadding   = QMargins(1, 1, 1, 1);  ///< Thumb 与背景轨道的左右/上下间距

    bool m_isHovered = false;
    bool m_isPressed = false;

    // Overlay 淡入/淡出
    qreal               m_opacity       = 0.0;
    QPropertyAnimation* m_opacityAnim   = nullptr;
    QTimer*             m_autoHideTimer = nullptr;

    void ensureAnimation();
    void showWithAutoHide();
};

} // namespace fluent::scrolling

#endif // SCROLLBAR_H
