#ifndef CHECKBOX_H
#define CHECKBOX_H

#include <QCheckBox>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QPropertyAnimation;

namespace fluent::basicinput {

/**
 * @brief Fluent checkbox with animated check-state painting.
 * zh_CN: 带选中态动画的 Fluent 复选框。
 *
 * CheckBox keeps QCheckBox interaction semantics while drawing the box,
 * mixed state, hover treatment, and text spacing with Fluent design tokens.
 * zh_CN: CheckBox 保留 QCheckBox 的交互语义，并通过 Fluent 设计 token 自绘方框、
 * 半选状态、悬停反馈和文本间距。
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
    Q_PROPERTY(bool hoverBackgroundEnabled READ hoverBackgroundEnabled WRITE setHoverBackgroundEnabled NOTIFY hoverBackgroundEnabledChanged)

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

    // Size hints for the layout system. zh_CN: 提供给布局系统的尺寸提示。
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

private:
    void initAnimation();

    qreal m_checkProgress = 1.0; 
    int m_boxSize = 20; // Defaults to 20px. zh_CN: 默认 20px。
    int m_boxMargin = 8; // Defaults to 8px. zh_CN: 默认 8px。
    int m_textGap = 8; // Defaults to 8px. zh_CN: 默认 8px。
    bool m_hoverBackgroundEnabled = false; // Hover fill off by default. zh_CN: 默认不启用 hover 背景。
    QPropertyAnimation* m_checkAnimation = nullptr;
};

} // namespace fluent::basicinput

#endif // CHECKBOX_H
