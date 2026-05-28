#ifndef RADIOBUTTON_H
#define RADIOBUTTON_H

#include <QRadioButton>
#include <QFont>
#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"

class QPropertyAnimation;

namespace view::basicinput {

/**
 * @brief Fluent radio button with animated selection feedback.
 * zh_CN: 带选中动画反馈的 Fluent 单选按钮。
 *
 * RadioButton preserves QRadioButton grouping behavior while painting the ring,
 * selected dot, text spacing, and hover state through the component theme.
 * zh_CN: RadioButton 保留 QRadioButton 的分组语义，并通过组件主题绘制圆环、
 * 选中圆点、文本间距和悬停状态。
 */
class RadioButton : public QRadioButton, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Normalized check-state animation progress.
     * zh_CN: 归一化的选中态动画进度。
     */
    Q_PROPERTY(qreal checkProgress READ checkProgress WRITE setCheckProgress)
    /**
     * @brief Animated scale of the selected radio dot.
     * zh_CN: 单选按钮选中圆点的动画缩放值。
     */
    Q_PROPERTY(qreal dotScale READ dotScale WRITE setDotScale)
    /**
     * @brief Outer radio circle diameter in pixels.
     * zh_CN: 单选按钮外圈直径，单位为像素。
     */
    Q_PROPERTY(int circleSize READ circleSize WRITE setCircleSize NOTIFY circleSizeChanged)
    /**
     * @brief Spacing between control glyph and text content.
     * zh_CN: 控件图形与文本内容之间的间距。
     */
    Q_PROPERTY(int textGap READ textGap WRITE setTextGap NOTIFY textGapChanged)
    /**
     * @brief Font used for radio button text.
     * zh_CN: 单选按钮文本使用的字体。
     */
    Q_PROPERTY(QFont textFont READ textFont WRITE setTextFont NOTIFY textFontChanged)

public:
    explicit RadioButton(const QString& text = "", QWidget* parent = nullptr);
    explicit RadioButton(QWidget* parent = nullptr);

    void onThemeUpdated() override;

    qreal checkProgress() const { return m_checkProgress; }
    void setCheckProgress(qreal progress);

    qreal dotScale() const { return m_dotScale; }
    void setDotScale(qreal scale);

    int circleSize() const { return m_circleSize; }
    void setCircleSize(int size);

    int textGap() const { return m_textGap; }
    void setTextGap(int gap);

    QFont textFont() const { return m_textFont; }
    void setTextFont(const QFont& font);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void circleSizeChanged();
    void textGapChanged();
    void textFontChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void nextCheckState() override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void initAnimation();

    qreal m_checkProgress = 1.0;
    qreal m_dotScale = 1.0;
    int m_circleSize = 20;
    int m_textGap = 8;
    QFont m_textFont;
    QPropertyAnimation* m_checkAnimation = nullptr;
    QPropertyAnimation* m_dotScaleAnimation = nullptr;
};

} // namespace view::basicinput

#endif // RADIOBUTTON_H
