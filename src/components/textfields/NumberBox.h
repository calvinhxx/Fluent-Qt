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

#include <memory>

namespace compatibility::detail {
class ScopedWidgetCursor;
}

namespace fluent::basicinput {
class RepeatButton;
}

namespace fluent::textfields {

/**
 * @brief Numeric LineEdit with value bounds, expression evaluation, and spin buttons.
 * zh_CN: 支持数值边界、表达式求值与步进按钮的数值输入框。
 *
 * Extends LineEdit with numeric formatting, small/large step increments,
 * spin-button placement, compact metrics, and display precision.
 * zh_CN: 基于 LineEdit 扩展数值格式化、大小步进、步进按钮布局模式、紧凑尺寸参数与显示精度。
 */
class NumberBox : public LineEdit {
    Q_OBJECT
    /**
     * @brief Current numeric value committed by the input.
     * zh_CN: 输入框当前提交的数值。
     */
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    /** @brief Allows an empty committed value (NaN) by default. When disabled, empty or invalid input
     * restores the committed number. An initially empty field starts at zero clamped to the range.
     * zh_CN: 默认允许空值（NaN）。关闭后，空白或无效输入在提交时恢复为已提交数值；原本为空时取范围内最接近零的值。
     */
    Q_PROPERTY(bool emptyAllowed READ isEmptyAllowed WRITE setEmptyAllowed NOTIFY emptyAllowedChanged)
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
     * @brief Placement of spin buttons. Split places minus and plus on opposite sides of the value.
     * zh_CN: 步进按钮布局。Split 将减号和加号放在数值两侧。
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
     * @brief Width reserved for compact spin buttons inside the input box.
     * zh_CN: 紧凑模式下输入框内为步进按钮预留的宽度。
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
    enum class SpinButtonPlacementMode { Hidden, Compact, Inline, Split };
    Q_ENUM(SpinButtonPlacementMode)

    explicit NumberBox(QWidget* parent = nullptr);
    ~NumberBox() override;

    double value() const { return m_value; }
    void setValue(double value);

    bool isEmptyAllowed() const { return m_emptyAllowed; }
    void setEmptyAllowed(bool allowed);

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

    /** @brief Parses and commits the current text using bounds and expression rules.
     *
     * With emptyAllowed, invalid input keeps its text and sets value to NaN. Otherwise the last value is restored.
     * zh_CN: 按数值范围与表达式规则解析并提交当前文本；允许空值时无效输入保留文本且 value 为 NaN，否则恢复上次的数值。 */
    void commitInput();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

signals:
    void valueChanged(double value);
    void emptyAllowedChanged(bool allowed);
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
    /** @brief Emitted when a drag scrub starts; valueChanged() continues to report live changes.
     * zh_CN: 拖拽数值编辑开始时发出；过程中 valueChanged() 仍实时上报数值变化。 */
    void scrubStarted(double initialValue);
    /** @brief Emitted when a drag scrub ends; cancellation restores initialValue first.
     * zh_CN: 拖拽数值编辑结束时发出；取消时会先恢复到 initialValue。 */
    void scrubFinished(double initialValue, double finalValue, bool canceled);

protected:
    QRect trailingContentBounds() const override;
    void trailingContentGeometryChanged() override;
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
    std::unique_ptr<::compatibility::detail::ScopedWidgetCursor> m_pointerCursor;
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
    void paintHeader(QPainter& painter);
    bool hasSpinnerButtonsVisible() const;
    int inlineSpinnerWidth() const;
    int compactExpandedSpinnerWidth() const;
    void finishScrub(bool canceled, bool overHandle = false);
    void updatePointerCursor(const QPoint& position);

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
    bool m_emptyAllowed = true;
    bool m_acceptsExpression = false;
    Qt::Alignment m_alignmentBeforeSplit = Qt::AlignLeading | Qt::AlignVCenter;
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
