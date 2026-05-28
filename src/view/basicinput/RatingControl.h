#ifndef RATINGCONTROL_H
#define RATINGCONTROL_H

#include <QWidget>
#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"

namespace view::basicinput {

/**
 * @brief Fluent rating selector with pointer-driven value selection.
 * zh_CN: 通过指针选择评分值的 Fluent 评分控件。
 *
 * RatingControl exposes value, placeholder, caption, read-only, clear behavior,
 * and maximum rating count while keeping star rendering theme-driven.
 * zh_CN: RatingControl 暴露评分值、占位值、说明文本、只读、清空行为和最大评分数，
 * 星形绘制由主题 token 驱动。
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
     * @brief Fluent typography role used for text rendering.
     * zh_CN: 文本绘制使用的 Fluent 排版角色。
     */
    Q_PROPERTY(QString fontRole READ fontRole WRITE setFontRole NOTIFY fontRoleChanged)
    /**
     * @brief Fluent typography role used for the rating caption.
     * zh_CN: 评分说明文本使用的 Fluent 排版角色。
     */
    Q_PROPERTY(QString captionFontRole READ captionFontRole WRITE setCaptionFontRole NOTIFY captionFontRoleChanged)

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

    QString fontRole() const { return m_fontRole; }
    void setFontRole(const QString& role);

    QString captionFontRole() const { return m_captionFontRole; }
    void setCaptionFontRole(const QString& role);

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
    void leaveEvent(QEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;

private:
    double ratingFromPosition(int x) const;
    QRectF starRect(int index) const;
    int starsAreaWidth() const;
    QSize iconCellSize() const;

    double m_value = -1.0;            // -1 = 未设置
    double m_placeholderValue = 0.0;
    QString m_caption;
    bool m_isClearEnabled = true;
    bool m_isReadOnly = false;
    int m_maxRating = 5;
    int m_starSize = 16;
    int m_itemSpacing = 4;
    QString m_fontRole = "Body";
    QString m_captionFontRole = "Caption";

    double m_hoverValue = -1.0;       // 悬停预览值
    bool m_isHovered = false;
    bool m_isPressed = false;
};

} // namespace view::basicinput

#endif // RATINGCONTROL_H
