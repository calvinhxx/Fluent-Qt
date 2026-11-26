#ifndef CHECKBOX_H
#define CHECKBOX_H

#include <QCheckBox>
#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

class QPropertyAnimation;
class QFocusEvent;
class QKeyEvent;
class QMouseEvent;

namespace fluent::basicinput {

/**
 * @brief Fluent check box with animated check and mixed states.
 * zh_CN: 支持半选与选中态动画的 Fluent 复选框。
 */
class CheckBox : public QCheckBox, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Normalized check-state animation progress.
     * zh_CN: 归一化的选中态动画进度。
     */
    Q_PROPERTY(qreal checkProgress READ checkProgress WRITE setCheckProgress)
    /**
     * @brief Checkbox box size in pixels.
     * zh_CN: 复选框方框尺寸，单位为像素。
     */
    Q_PROPERTY(int boxSize READ boxSize WRITE setBoxSize NOTIFY boxSizeChanged)
    /**
     * @brief Leading margin before the checkbox box.
     * zh_CN: 复选框方框左侧前置间距。
     */
    Q_PROPERTY(int boxMargin READ boxMargin WRITE setBoxMargin NOTIFY boxMarginChanged)
    /**
     * @brief Spacing between control glyph and text content.
     * zh_CN: 控件图形与文本内容之间的间距。
     */
    Q_PROPERTY(int textGap READ textGap WRITE setTextGap NOTIFY textGapChanged)
    /**
     * @brief Whether hover paints a subtle row background.
     * zh_CN: 悬停时是否绘制轻量背景。
     */
    Q_PROPERTY(bool hoverBackgroundEnabled
        READ hoverBackgroundEnabled
        WRITE setHoverBackgroundEnabled
        NOTIFY hoverBackgroundEnabledChanged)

public:
    explicit CheckBox(const QString& text = "", QWidget* parent = nullptr);
    explicit CheckBox(QWidget* parent = nullptr);

    void onThemeUpdated() override;

    qreal checkProgress() const { return m_checkProgress; }
    void setCheckProgress(qreal progress);

    int boxSize() const { return m_boxSize; }
    void setBoxSize(int size);

    int boxMargin() const { return m_boxMargin; }
    void setBoxMargin(int margin);

    int textGap() const { return m_textGap; }
    void setTextGap(int gap);

    bool hoverBackgroundEnabled() const { return m_hoverBackgroundEnabled; }
    void setHoverBackgroundEnabled(bool enabled);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void boxSizeChanged();
    void boxMarginChanged();
    void textGapChanged();
    void hoverBackgroundEnabledChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void nextCheckState() override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void initAnimation();

    qreal m_checkProgress = 1.0;
    int m_boxSize = 20;
    int m_boxMargin = 8;
    int m_textGap = 8;
    bool m_hoverBackgroundEnabled = false;
    QPropertyAnimation* m_checkAnimation = nullptr;
    bool m_keyboardFocusVisible = false;
};

} // namespace fluent::basicinput

#endif // CHECKBOX_H
