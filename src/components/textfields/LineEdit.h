#ifndef LINEEDIT_H
#define LINEEDIT_H

#include "compatibility/InputEventCompat.h"
#include <QLineEdit>
#include <QMargins>
#include <QPoint>
#include <QRectF>
#include <QString>
#include <memory>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/WidgetOwnership.h"
#include "design/Spacing.h"
#include "design/Typography.h"

class QPainter;
class QPaintEvent;
class QResizeEvent;
class QContextMenuEvent;

namespace fluent::basicinput {
class Button;
}
namespace fluent::detail {
class FieldTrailingContent;
}

namespace fluent::textfields {

/**
 * @brief Fluent single-line text input with clear button and focus underline.
 * zh_CN: 带清除按钮和焦点下划线的 Fluent 单行文本输入框。
 *
 * LineEdit wraps QLineEdit styling with token-driven margins, typography, frame
 * visibility, clear-button geometry, and focused/unfocused border widths.
 * zh_CN: LineEdit 在 QLineEdit 外增加 token 驱动的边距、排版、外框可见性、
 * 清除按钮几何和聚焦/非聚焦边线宽度。
 */
class LineEdit : public QLineEdit, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Content margins whose horizontal values include Qt's internal text inset.
     * zh_CN: 内容边距；水平值已包含 Qt 内部文字留白，零值不会重复增加原生边距。
     */
    Q_PROPERTY(QMargins contentMargins READ contentMargins WRITE setContentMargins NOTIFY contentMarginsChanged)
    /**
     * @brief Fluent typography role used for text rendering.
     * zh_CN: 文本绘制使用的 Fluent 排版角色。
     */
    Q_PROPERTY(Typography::FontRole fontRole READ fontRole WRITE setFontRole NOTIFY fontRoleChanged)
    /**
     * @brief Whether the trailing clear button is enabled.
     * zh_CN: 尾部清除按钮是否启用。
     */
    Q_PROPERTY(bool clearButtonEnabled
        READ isClearButtonEnabled
        WRITE setClearButtonEnabled
        NOTIFY clearButtonEnabledChanged)
    /**
     * @brief Size of the clear button, defaulting to 24 logical pixels.
     * zh_CN: 清除按钮尺寸，默认为 24 个逻辑像素。
     */
    Q_PROPERTY(int clearButtonSize READ clearButtonSize WRITE setClearButtonSize NOTIFY clearButtonSizeChanged)
    /**
     * @brief Pixel offset applied to the clear button.
     * zh_CN: 清除按钮应用的像素偏移。
     */
    Q_PROPERTY(QPoint clearButtonOffset
        READ clearButtonOffset
        WRITE setClearButtonOffset
        NOTIFY clearButtonOffsetChanged)
    /**
     * @brief Bottom border width while focused.
     * zh_CN: 聚焦时底部边框宽度。
     */
    Q_PROPERTY(int focusedBorderWidth
        READ focusedBorderWidth
        WRITE setFocusedBorderWidth
        NOTIFY focusedBorderWidthChanged)
    /**
     * @brief Bottom border width while not focused; defaults to zero (no underline).
     * zh_CN: 未聚焦时底部边框宽度，默认零表示不绘制下划线。
     */
    Q_PROPERTY(int unfocusedBorderWidth
        READ unfocusedBorderWidth
        WRITE setUnfocusedBorderWidth
        NOTIFY unfocusedBorderWidthChanged)
    /**
     * @brief Whether LineEdit paints its input frame and focus underline.
     * zh_CN: LineEdit 是否绘制输入框外框和焦点下划线。
     */
    Q_PROPERTY(bool frameVisible READ isFrameVisible WRITE setFrameVisible NOTIFY frameVisibleChanged)
    Q_PROPERTY(QString trailingText READ trailingText WRITE setTrailingText NOTIFY trailingTextChanged)
    Q_PROPERTY(QWidget* trailingWidget READ trailingWidget NOTIFY trailingWidgetChanged)
    Q_PROPERTY(fluent::WidgetOwnership trailingWidgetOwnership
        READ trailingWidgetOwnership
        NOTIFY trailingWidgetOwnershipChanged)
    Q_PROPERTY(QString clearButtonAccessibleName
        READ clearButtonAccessibleName
        WRITE setClearButtonAccessibleName
        NOTIFY clearButtonAccessibleNameChanged)
    Q_PROPERTY(Qt::FocusPolicy clearButtonFocusPolicy
        READ clearButtonFocusPolicy
        WRITE setClearButtonFocusPolicy
        NOTIFY clearButtonFocusPolicyChanged)

public:
    explicit LineEdit(QWidget* parent = nullptr);
    ~LineEdit() override;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

    QMargins contentMargins() const { return m_contentMargins; }
    void setContentMargins(const QMargins& margins);

    Typography::FontRole fontRole() const { return m_fontRole; }
    void setFontRole(Typography::FontRole role);

    bool isClearButtonEnabled() const { return m_clearButtonEnabled; }
    void setClearButtonEnabled(bool enabled);

    int clearButtonSize() const { return m_clearButtonSize; }
    void setClearButtonSize(int size);

    QPoint clearButtonOffset() const { return m_clearButtonOffset; }
    void setClearButtonOffset(const QPoint& offset);

    int focusedBorderWidth() const { return m_focusedBorderWidth; }
    void setFocusedBorderWidth(int width);

    int unfocusedBorderWidth() const { return m_unfocusedBorderWidth; }
    void setUnfocusedBorderWidth(int width);

    bool isFrameVisible() const { return m_frameVisible; }
    void setFrameVisible(bool visible);

    QString trailingText() const;
    /** @brief Displays a plain-text suffix without changing the editable value; replaces hosted content.
     * zh_CN: 显示纯文字尾缀，不改变可编辑值，并替换已承载的控件内容。 */
    void setTrailingText(const QString& text);
    QWidget* trailingWidget() const;
    WidgetOwnership trailingWidgetOwnership() const;
    /** @brief Hosts one trailing widget using explicit ownership; rejects the host and its ancestors.
     * zh_CN: 按显式所有权承载一个尾部控件；拒绝宿主自身及祖先控件。
     * Take the widget before changing its ownership policy. Read-only fields preserve custom actions.
     * zh_CN: 改变所有权策略前须先取出控件；只读字段保留自定义操作。 */
    bool setTrailingWidget(QWidget* widget, WidgetOwnership ownership);
    /** @brief Detaches the custom trailing widget and transfers it to the caller.
     * zh_CN: 脱离并将自定义尾部控件交回调用方。 */
    QWidget* takeTrailingWidget();
    /** @brief Releases custom trailing content according to its recorded ownership policy.
     * zh_CN: 按记录的所有权策略释放自定义尾部内容。 */
    void releaseTrailingWidget();
    QString clearButtonAccessibleName() const;
    void setClearButtonAccessibleName(const QString& name);
    Qt::FocusPolicy clearButtonFocusPolicy() const;
    void setClearButtonFocusPolicy(Qt::FocusPolicy policy);

signals:
    void contentMarginsChanged();
    void fontRoleChanged();
    void clearButtonEnabledChanged();
    void clearButtonSizeChanged();
    void clearButtonOffsetChanged();
    void focusedBorderWidthChanged();
    void unfocusedBorderWidthChanged();
    void frameVisibleChanged();
    void trailingTextChanged();
    void trailingWidgetChanged();
    void trailingWidgetOwnershipChanged();
    void clearButtonAccessibleNameChanged();
    void clearButtonFocusPolicyChanged();

protected:
    bool event(QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    /** @brief Paints the input surface when frameVisible is enabled.
     * zh_CN: frameVisible 启用时绘制输入表面，派生输入框可复用同一开关。 */
    virtual void paintFrame(QPainter& painter);
    /** @brief Paints custom input bounds using inherited frame widths and theme tokens.
     * zh_CN: 为自定义输入区域绘制边框，统一使用继承的线宽配置与主题令牌。 */
    void paintInputFrame(QPainter& painter,
        const QRectF& bounds,
        bool focused,
        bool hovered,
        bool pressed = false,
        bool invalid = false);
    virtual QRect trailingContentBounds() const { return rect(); }
    virtual void trailingContentGeometryChanged();
    int trailingContentWidth() const { return m_trailingContentWidth; }
    QSize trailingContentSizeHint() const;
    void updateTrailingContentGeometry();
    virtual ::fluent::basicinput::Button* clearActionButton() const { return m_clearButton; }

private:
    void applyThemeStyle();
    void applyTextPadding();
    void applyThemePalette();
    void updateClearButtonVisibility();
    void updateClearButtonGeometry();
    QMargins chromeContentMargins() const;

    QMargins m_contentMargins = QMargins(::Spacing::Padding::TextFieldHorizontal,
        ::Spacing::Padding::TextFieldVertical,
        ::Spacing::Padding::TextFieldHorizontal,
        ::Spacing::Padding::TextFieldVertical);
    Typography::FontRole m_fontRole = Typography::FontRole::Body;
    ::fluent::basicinput::Button* m_clearButton = nullptr;
    bool m_clearButtonEnabled = true;
    int m_clearButtonSize = 24;
    QPoint m_clearButtonOffset = QPoint(::Spacing::XSmall, 0);
    int m_focusedBorderWidth = ::Spacing::Border::Focused;
    int m_unfocusedBorderWidth = 0;
    bool m_frameVisible = true;
    bool m_isHovered = false;
    bool m_isFocused = false;
    std::unique_ptr<::fluent::detail::FieldTrailingContent> m_trailingContent;
    int m_trailingContentWidth = 0;
    bool m_updatingTrailingContent = false;
};

} // namespace fluent::textfields

#endif // LINEEDIT_H
