#ifndef NUMBERBOX_H
#define NUMBERBOX_H

#include "compatibility/InputEventCompat.h"
#include "LineEdit.h"

#include <QSize>

class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QResizeEvent;

namespace fluent::basicinput {
class RepeatButton;
}

namespace fluent::textfields {

/**
 * @brief Numeric LineEdit with value bounds, expression input, and spin
 * buttons. zh_CN: 支持数值边界、表达式输入和步进按钮的数值输入框。
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
     * @note Combined parenthesis and power nesting is limited to 128 levels.
     * zh_CN: 括号和幂运算的合计嵌套深度最多为 128 层。
     */
    Q_PROPERTY(bool acceptsExpression READ acceptsExpression WRITE setAcceptsExpression NOTIFY acceptsExpressionChanged)
    /**
     * @brief Placement mode for hidden, compact, or inline spin buttons.
     * zh_CN: 步进按钮的隐藏、紧凑或内联布局模式。
     */
    Q_PROPERTY(SpinButtonPlacementMode spinButtonPlacementMode
        READ spinButtonPlacementMode
        WRITE setSpinButtonPlacementMode
        NOTIFY spinButtonPlacementModeChanged)
    /**
     * @brief Size of external spin buttons.
     * zh_CN: 外部步进按钮的尺寸。
     */
    Q_PROPERTY(QSize spinButtonSize READ spinButtonSize WRITE setSpinButtonSize NOTIFY spinButtonSizeChanged)
    /**
     * @brief Size of inline spin buttons.
     * zh_CN: 内联步进按钮的尺寸。
     */
    Q_PROPERTY(QSize inlineSpinButtonSize
        READ inlineSpinButtonSize
        WRITE setInlineSpinButtonSize
        NOTIFY inlineSpinButtonSizeChanged)
    /**
     * @brief Right-side inset reserved between inline spin buttons and the frame.
     * zh_CN: 内联步进按钮与输入框右边框之间预留的间距。
     */
    Q_PROPERTY(int spinButtonRightMargin
        READ spinButtonRightMargin
        WRITE setSpinButtonRightMargin
        NOTIFY spinButtonRightMarginChanged)
    /**
     * @brief Width reserved for compact spin buttons inside the input surface.
     * zh_CN: 紧凑模式下输入表面为步进按钮预留的宽度。
     */
    Q_PROPERTY(int compactSpinButtonReservedWidth
        READ compactSpinButtonReservedWidth
        WRITE setCompactSpinButtonReservedWidth
        NOTIFY compactSpinButtonReservedWidthChanged)
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
    Q_PROPERTY(int spinButtonIconSize
        READ spinButtonIconSize
        WRITE setSpinButtonIconSize
        NOTIFY spinButtonIconSizeChanged)
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
    /** @brief Enables the separate horizontal drag handle; text selection stays unchanged.
     * zh_CN: 启用独立的水平拖拽柄，保留原有文本选择行为。 */
    Q_PROPERTY(bool scrubbingEnabled READ isScrubbingEnabled WRITE setScrubbingEnabled NOTIFY scrubbingEnabledChanged)
    /** @brief Value units per logical pixel; must be finite and positive.
     * zh_CN: 每逻辑像素对应的数值增量，必须有限且为正数。 */
    Q_PROPERTY(double scrubSensitivity READ scrubSensitivity WRITE setScrubSensitivity NOTIFY scrubSensitivityChanged)
    /** @brief Whether a drag transaction is active. zh_CN: 当前是否正在进行拖拽编辑。 */
    Q_PROPERTY(bool scrubbing READ isScrubbing NOTIFY scrubbingChanged)

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

    bool isScrubbingEnabled() const { return m_scrubbingEnabled; }
    void setScrubbingEnabled(bool enabled);
    double scrubSensitivity() const { return m_scrubSensitivity; }
    void setScrubSensitivity(double sensitivity);
    bool isScrubbing() const { return m_scrubbing; }
    /** @brief Logical handle geometry, empty while disabled. zh_CN: 拖拽柄的逻辑坐标区域，未启用时为空。 */
    QRect scrubHandleRect() const;
    /** @brief Rolls back an active drag, including an initially empty value.
     * zh_CN: 回滚当前拖拽，包括恢复开始时的空值。 */
    Q_SLOT void cancelScrub();

    /** @brief Commits current text using the numeric bounds and expression policy.
     * Invalid input retains its text and sets value to NaN; unchanged formatted
     * text preserves the original precision. No external model is modified.
     * zh_CN: 按数值范围和表达式规则提交当前文本；无效输入保留文本并将值设为 NaN，
     * 未编辑的格式化文本保留原始精度，不修改外部模型。 */
    void commitInput();

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
    void scrubbingEnabledChanged(bool enabled);
    void scrubSensitivityChanged(double sensitivity);
    void scrubbingChanged(bool scrubbing);
    /** @brief One transaction starts; valueChanged still reports each live value.
     * zh_CN: 一次编辑开始；valueChanged 仍报告每次实时值变化。 */
    void scrubStarted(double initialValue);
    /** @brief Ends once; cancellation restores initialValue before this signal.
     * zh_CN: 一次编辑结束；取消时在此信号之前恢复初始值。 */
    void scrubFinished(double initialValue, double finalValue, bool canceled);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintFrame(QPainter& painter) override;

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
    void setInvalidValueFromText();
    void stepBy(double delta);
    double normalizedStepStart() const;
    double normalizeValue(double value) const;
    double applyFormatStep(double value) const;
    QString formatValue(double value) const;
    void setFormattedText(const QString& text);
    bool parseInputText(const QString& input, double* result) const;
    bool setValueInternal(double value, bool updateText, bool keepUserTextWhenNaN);
    void paintInputFrame(QPainter& painter);
    void paintHeader(QPainter& painter);
    bool hasSpinnerButtonsVisible() const;
    int inlineSpinnerWidth() const;
    int compactExpandedSpinnerWidth() const;
    void finishScrub(bool canceled);

    bool m_scrubbingEnabled = false;
    bool m_scrubbing = false;
    double m_scrubSensitivity = 1.0;
    double m_scrubInitialValue = 0.0;
    QString m_scrubInitialText;
    QPoint m_scrubStart;
    double m_scrubBase = 0.0;

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

    ::fluent::basicinput::RepeatButton* m_spinUpButton = nullptr;
    ::fluent::basicinput::RepeatButton* m_spinDownButton = nullptr;
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

    static constexpr int kHeaderHeight = 20;
    static constexpr int kHeaderGap = 8;
    static constexpr int kMinimumWidth = 124;
};

} // namespace fluent::textfields

#endif // NUMBERBOX_H
