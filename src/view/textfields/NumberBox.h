#ifndef NUMBERBOX_H
#define NUMBERBOX_H

#include "LineEdit.h"

#include <QSize>

class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QResizeEvent;

namespace view::basicinput { class RepeatButton; }

namespace view::textfields {

/**
 * @brief Numeric LineEdit with value bounds, expression input, and spin buttons.
 * zh_CN: 支持数值边界、表达式输入和步进按钮的数值输入框。
 *
 * NumberBox builds on LineEdit while exposing numeric formatting, small/large
 * increments, spin-button placement, compact metrics, and display precision.
 * zh_CN: NumberBox 基于 LineEdit，并暴露数值格式、小/大步进、步进按钮位置、
 * 紧凑布局参数和显示精度。
 */
class NumberBox : public LineEdit {
    Q_OBJECT
    /**
     * @brief Current numeric value committed by the input.
     * zh_CN: 输入框当前提交的数值。
     */
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    /**
     * @brief Lower bound used to clamp typed and stepped values.
     * zh_CN: 用于限制输入值和步进值的下边界。
     */
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY minimumChanged)
    /**
     * @brief Upper bound used to clamp typed and stepped values.
     * zh_CN: 用于限制输入值和步进值的上边界。
     */
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY maximumChanged)
    /**
     * @brief Increment used by one spin-button click or small keyboard step.
     * zh_CN: 单次步进按钮点击或小键盘步进使用的增量。
     */
    Q_PROPERTY(double smallChange READ smallChange WRITE setSmallChange NOTIFY smallChangeChanged)
    /**
     * @brief Increment used by large-step keyboard actions.
     * zh_CN: 大步进键盘操作使用的增量。
     */
    Q_PROPERTY(double largeChange READ largeChange WRITE setLargeChange NOTIFY largeChangeChanged)
    /**
     * @brief Optional label text displayed above the numeric input row.
     * zh_CN: 显示在数值输入行上方的可选标签文本。
     */
    Q_PROPERTY(QString header READ header WRITE setHeader NOTIFY headerChanged)
    /**
     * @brief Whether typed arithmetic expressions are evaluated before commit.
     * zh_CN: 输入的算术表达式是否在提交前求值。
     */
    Q_PROPERTY(bool acceptsExpression READ acceptsExpression WRITE setAcceptsExpression NOTIFY acceptsExpressionChanged)
    /**
     * @brief Placement mode for hidden, compact, or inline spin buttons.
     * zh_CN: 步进按钮的隐藏、紧凑或内联布局模式。
     */
    Q_PROPERTY(SpinButtonPlacementMode spinButtonPlacementMode READ spinButtonPlacementMode WRITE setSpinButtonPlacementMode NOTIFY spinButtonPlacementModeChanged)
    /**
     * @brief Button size used when spin buttons are rendered outside the text field.
     * zh_CN: 步进按钮绘制在文本框外侧时使用的按钮尺寸。
     */
    Q_PROPERTY(QSize spinButtonSize READ spinButtonSize WRITE setSpinButtonSize NOTIFY spinButtonSizeChanged)
    /**
     * @brief Button size used when spin buttons are rendered inside the text field.
     * zh_CN: 步进按钮绘制在文本框内部时使用的按钮尺寸。
     */
    Q_PROPERTY(QSize inlineSpinButtonSize READ inlineSpinButtonSize WRITE setInlineSpinButtonSize NOTIFY inlineSpinButtonSizeChanged)
    /**
     * @brief Right-side inset reserved between inline spin buttons and the frame.
     * zh_CN: 内联步进按钮与输入框右边框之间预留的间距。
     */
    Q_PROPERTY(int spinButtonRightMargin READ spinButtonRightMargin WRITE setSpinButtonRightMargin NOTIFY spinButtonRightMarginChanged)
    /**
     * @brief Width reserved for compact spin buttons inside the input surface.
     * zh_CN: 紧凑模式下输入表面为步进按钮预留的宽度。
     */
    Q_PROPERTY(int compactSpinButtonReservedWidth READ compactSpinButtonReservedWidth WRITE setCompactSpinButtonReservedWidth NOTIFY compactSpinButtonReservedWidthChanged)
    /**
     * @brief Spacing between adjacent spin buttons.
     * zh_CN: 相邻步进按钮之间的间距。
     */
    Q_PROPERTY(int spinButtonSpacing READ spinButtonSpacing WRITE setSpinButtonSpacing NOTIFY spinButtonSpacingChanged)
    /**
     * @brief Gap between text content and inline spin-button chrome.
     * zh_CN: 文本内容与内联步进按钮外观之间的间距。
     */
    Q_PROPERTY(int spinButtonTextGap READ spinButtonTextGap WRITE setSpinButtonTextGap NOTIFY spinButtonTextGapChanged)
    /**
     * @brief Icon glyph size used by spin-button chevrons.
     * zh_CN: 步进按钮箭头图标使用的字符尺寸。
     */
    Q_PROPERTY(int spinButtonIconSize READ spinButtonIconSize WRITE setSpinButtonIconSize NOTIFY spinButtonIconSizeChanged)
    /**
     * @brief Number of fractional digits used when formatting the value.
     * zh_CN: 格式化数值时保留的小数位数。
     */
    Q_PROPERTY(int displayPrecision READ displayPrecision WRITE setDisplayPrecision NOTIFY displayPrecisionChanged)
    /**
     * @brief Step used to normalize display rounding after value changes.
     * zh_CN: 数值变化后用于规范显示舍入的步长。
     */
    Q_PROPERTY(double formatStep READ formatStep WRITE setFormatStep NOTIFY formatStepChanged)

public:
    enum class SpinButtonPlacementMode { Hidden, Compact, Inline };
    Q_ENUM(SpinButtonPlacementMode)

    explicit NumberBox(QWidget* parent = nullptr);

    double value() const { return m_value; }
    void setValue(double value);

    double minimum() const { return m_minimum; }
    void setMinimum(double minimum);

    double maximum() const { return m_maximum; }
    void setMaximum(double maximum);
    void setRange(double minimum, double maximum);

    double smallChange() const { return m_smallChange; }
    void setSmallChange(double change);

    double largeChange() const { return m_largeChange; }
    void setLargeChange(double change);

    QString header() const { return m_header; }
    void setHeader(const QString& header);

    bool acceptsExpression() const { return m_acceptsExpression; }
    void setAcceptsExpression(bool accepts);

    SpinButtonPlacementMode spinButtonPlacementMode() const { return m_spinButtonPlacementMode; }
    void setSpinButtonPlacementMode(SpinButtonPlacementMode mode);

    QSize spinButtonSize() const { return m_spinButtonSize; }
    void setSpinButtonSize(const QSize& size);

    QSize inlineSpinButtonSize() const { return m_inlineSpinButtonSize; }
    void setInlineSpinButtonSize(const QSize& size);

    int spinButtonRightMargin() const { return m_spinButtonRightMargin; }
    void setSpinButtonRightMargin(int margin);

    int compactSpinButtonReservedWidth() const { return m_compactSpinButtonReservedWidth; }
    void setCompactSpinButtonReservedWidth(int width);

    int spinButtonSpacing() const { return m_spinButtonSpacing; }
    void setSpinButtonSpacing(int spacing);

    int spinButtonTextGap() const { return m_spinButtonTextGap; }
    void setSpinButtonTextGap(int gap);

    int spinButtonIconSize() const { return m_spinButtonIconSize; }
    void setSpinButtonIconSize(int size);

    int displayPrecision() const { return m_displayPrecision; }
    void setDisplayPrecision(int precision);

    double formatStep() const { return m_formatStep; }
    void setFormatStep(double step);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

signals:
    void valueChanged(double value);
    void minimumChanged(double minimum);
    void maximumChanged(double maximum);
    void smallChangeChanged(double change);
    void largeChangeChanged(double change);
    void headerChanged();
    void acceptsExpressionChanged(bool accepts);
    void spinButtonPlacementModeChanged(SpinButtonPlacementMode mode);
    void spinButtonSizeChanged(const QSize& size);
    void inlineSpinButtonSizeChanged(const QSize& size);
    void spinButtonRightMarginChanged(int margin);
    void compactSpinButtonReservedWidthChanged(int width);
    void spinButtonSpacingChanged(int spacing);
    void spinButtonTextGapChanged(int gap);
    void spinButtonIconSizeChanged(int size);
    void displayPrecisionChanged(int precision);
    void formatStepChanged(double step);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QRect inputRect() const;
    int inputTop() const;
    int totalPreferredHeight() const;

    void initializeSpinnerButtons();
    void updateSpinButtonIcons();
    void updateChildGeometry();
    void updateSpinnerState();
    void updateTextMarginsForChrome();
    void updateHeaderTextMargins();
    void commitInput();
    void setInvalidValueFromText();
    void stepBy(double delta);
    double normalizedStepStart() const;
    double normalizeValue(double value) const;
    double applyFormatStep(double value) const;
    QString formatValue(double value) const;
    bool parseInputText(const QString& input, double* result) const;
    bool setValueInternal(double value, bool updateText, bool keepUserTextWhenNaN);
    void paintInputFrame(QPainter& painter);
    void paintHeader(QPainter& painter);
    bool hasSpinnerButtonsVisible() const;
    int inlineSpinnerWidth() const;
    int compactExpandedSpinnerWidth() const;

    QString m_header;
    double m_value;
    double m_minimum;
    double m_maximum;
    double m_smallChange = 1.0;
    double m_largeChange = 10.0;
    bool m_acceptsExpression = false;
    SpinButtonPlacementMode m_spinButtonPlacementMode = SpinButtonPlacementMode::Hidden;
    int m_displayPrecision = -1;
    double m_formatStep = 0.0;

    ::view::basicinput::RepeatButton* m_spinUpButton = nullptr;
    ::view::basicinput::RepeatButton* m_spinDownButton = nullptr;
    bool m_hovered = false;
    bool m_focused = false;
    bool m_pressed = false;
    bool m_spinnerHovered = false;
    bool m_spinnerPressed = false;

    QSize m_spinButtonSize = QSize(24, 14);
    QSize m_inlineSpinButtonSize = QSize(24, 20);
    int m_spinButtonRightMargin = 4;
    int m_compactSpinButtonReservedWidth = 14;
    int m_spinButtonSpacing = 2;
    int m_spinButtonTextGap = 2;
    int m_spinButtonIconSize = 10;

    static constexpr int kInputHeight = 32;
    static constexpr int kHeaderHeight = 20;
    static constexpr int kHeaderGap = 8;
    static constexpr int kMinimumWidth = 124;
};

} // namespace view::textfields

#endif // NUMBERBOX_H
