#ifndef RATINGCONTROL_H
#define RATINGCONTROL_H

#include "../../compatibility/InputEventCompat.h"
#include <QWidget>
#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

class QKeyEvent;
class QFocusEvent;

namespace fluent::basicinput {

/**
 * @brief Star rating control supporting half-star precision, placeholder preview, and optional clearing.
 * zh_CN: 支持半星精度、占位预览及可选清空的星级评分控件。
 */
class RatingControl : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Selected rating value committed by the user.
     * zh_CN: 用户已提交的选中评分值。
     */
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    /**
     * @brief Rating value previewed when no final value is selected.
     * zh_CN: 未选择最终评分时预览的占位评分。
     */
    Q_PROPERTY(double placeholderValue READ placeholderValue WRITE setPlaceholderValue NOTIFY placeholderValueChanged)
    /**
     * @brief Supplemental text displayed next to the rating glyphs.
     * zh_CN: 显示在评分图形旁的辅助说明文本。
     */
    Q_PROPERTY(QString caption READ caption WRITE setCaption NOTIFY captionChanged)
    /**
     * @brief Whether the current rating can be cleared by user interaction.
     * zh_CN: 用户是否可以清空当前评分。
     */
    Q_PROPERTY(bool isClearEnabled READ isClearEnabled WRITE setIsClearEnabled NOTIFY isClearEnabledChanged)
    /**
     * @brief Whether user interaction is blocked while display remains visible.
     * zh_CN: 是否阻止用户交互但保留显示。
     */
    Q_PROPERTY(bool isReadOnly READ isReadOnly WRITE setIsReadOnly NOTIFY isReadOnlyChanged)
    /**
     * @brief Maximum rating value exposed by the control.
     * zh_CN: 控件允许的最大评分值。
     */
    Q_PROPERTY(int maxRating READ maxRating WRITE setMaxRating NOTIFY maxRatingChanged)
    /**
     * @brief Rating glyph size in pixels.
     * zh_CN: 评分图形尺寸，单位为像素。
     */
    Q_PROPERTY(int starSize READ starSize WRITE setStarSize NOTIFY starSizeChanged)
    /**
     * @brief Fluent Typography preset used for text.
     * zh_CN: 文本使用的预设字号样式。
     */
    Q_PROPERTY(Typography::FontRole fontRole READ fontRole WRITE setFontRole NOTIFY fontRoleChanged)
    /**
     * @brief Fluent Typography preset used for the rating caption.
     * zh_CN: 评分说明文本使用的预设字号样式。
     */
    Q_PROPERTY(Typography::FontRole captionFontRole
        READ captionFontRole
        WRITE setCaptionFontRole
        NOTIFY captionFontRoleChanged)

public:
    explicit RatingControl(QWidget* parent = nullptr);

    void onThemeUpdated() override;

    double value() const { return m_value; }
    void setValue(double value);

    double placeholderValue() const { return m_placeholderValue; }
    void setPlaceholderValue(double value);

    QString caption() const { return m_caption; }
    void setCaption(const QString& caption);

    bool isClearEnabled() const { return m_isClearEnabled; }
    void setIsClearEnabled(bool enabled);

    bool isReadOnly() const { return m_isReadOnly; }
    void setIsReadOnly(bool readOnly);

    int maxRating() const { return m_maxRating; }
    void setMaxRating(int rating);

    int starSize() const { return m_starSize; }
    void setStarSize(int size);

    Typography::FontRole fontRole() const { return m_fontRole; }
    void setFontRole(Typography::FontRole role);

    Typography::FontRole captionFontRole() const { return m_captionFontRole; }
    void setCaptionFontRole(Typography::FontRole role);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void valueChanged(double value);
    void placeholderValueChanged(double value);
    void captionChanged(const QString& caption);
    void isClearEnabledChanged(bool enabled);
    void isReadOnlyChanged(bool readOnly);
    void maxRatingChanged(int rating);
    void starSizeChanged(int size);
    void fontRoleChanged();
    void captionFontRoleChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    double keyboardStepTarget(int direction) const;
    double ratingFromPosition(int x) const;
    QRectF starRect(int index) const;
    int starsAreaWidth() const;
    int contentInset() const;
    QSize iconCellSize() const;

    double m_value = -1.0; // -1 = 未设置
    double m_placeholderValue = 0.0;
    QString m_caption;
    bool m_isClearEnabled = true;
    bool m_isReadOnly = false;
    int m_maxRating = 5;
    int m_starSize = 16;
    int m_itemSpacing = 4;
    Typography::FontRole m_fontRole = Typography::FontRole::Body;
    Typography::FontRole m_captionFontRole = Typography::FontRole::Caption;

    double m_hoverValue = -1.0; // 悬停预览值
    bool m_isHovered = false;
    bool m_isPressed = false;
    bool m_keyboardFocusVisible = false;
};

} // namespace fluent::basicinput

#endif // RATINGCONTROL_H
