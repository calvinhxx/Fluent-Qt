#ifndef LINEEDIT_H
#define LINEEDIT_H

#include <QLineEdit>
#include <QMargins>
#include <QPoint>
#include <QString>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "design/Spacing.h"
#include "design/Typography.h"

class QPainter;
class QPaintEvent;
class QResizeEvent;

namespace fluent::basicinput { class Button; }

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
     * @brief Margins applied around the control content area.
     * zh_CN: 控件内容区域周围的边距。
     */
    Q_PROPERTY(QMargins contentMargins READ contentMargins WRITE setContentMargins NOTIFY contentMarginsChanged)
    /**
     * @brief Fluent typography role used for text rendering.
     * zh_CN: 文本绘制使用的 Fluent 排版角色。
     */
    Q_PROPERTY(QString fontRole READ fontRole WRITE setFontRole NOTIFY fontRoleChanged)
    /**
     * @brief Whether the trailing clear button is enabled.
     * zh_CN: 尾部清除按钮是否启用。
     */
    Q_PROPERTY(bool clearButtonEnabled READ isClearButtonEnabled WRITE setClearButtonEnabled NOTIFY clearButtonEnabledChanged)
    /**
     * @brief Size of the clear button.
     * zh_CN: 清除按钮尺寸。
     */
    Q_PROPERTY(int clearButtonSize READ clearButtonSize WRITE setClearButtonSize NOTIFY clearButtonSizeChanged)
    /**
     * @brief Pixel offset applied to the clear button.
     * zh_CN: 清除按钮应用的像素偏移。
     */
    Q_PROPERTY(QPoint clearButtonOffset READ clearButtonOffset WRITE setClearButtonOffset NOTIFY clearButtonOffsetChanged)
    /**
     * @brief Bottom border width while focused.
     * zh_CN: 聚焦时底部边框宽度。
     */
    Q_PROPERTY(int focusedBorderWidth READ focusedBorderWidth WRITE setFocusedBorderWidth NOTIFY focusedBorderWidthChanged)
    /**
     * @brief Bottom border width while not focused.
     * zh_CN: 未聚焦时底部边框宽度。
     */
    Q_PROPERTY(int unfocusedBorderWidth READ unfocusedBorderWidth WRITE setUnfocusedBorderWidth NOTIFY unfocusedBorderWidthChanged)
    /**
     * @brief Whether LineEdit paints its input frame and focus underline.
     * zh_CN: LineEdit 是否绘制输入框外框和焦点下划线。
     */
    Q_PROPERTY(bool frameVisible READ isFrameVisible WRITE setFrameVisible NOTIFY frameVisibleChanged)

public:
    explicit LineEdit(QWidget* parent = nullptr);

    void onThemeUpdated() override;

    QMargins contentMargins() const { return m_contentMargins; }
    void setContentMargins(const QMargins& margins);

    QString fontRole() const { return m_fontRole; }
    void setFontRole(const QString& role);

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

signals:
    void contentMarginsChanged();
    void fontRoleChanged();
    void clearButtonEnabledChanged();
    void clearButtonSizeChanged();
    void clearButtonOffsetChanged();
    void focusedBorderWidthChanged();
    void unfocusedBorderWidthChanged();
    void frameVisibleChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void applyThemeStyle();
    void paintFrame(QPainter& painter);
    void updateClearButtonVisibility();
    void updateClearButtonGeometry();

    QMargins m_contentMargins  = QMargins(::Spacing::Padding::TextFieldHorizontal,
                                          ::Spacing::Padding::TextFieldVertical,
                                          ::Spacing::Padding::TextFieldHorizontal,
                                          ::Spacing::Padding::TextFieldVertical);
    QString  m_fontRole        = Typography::FontRole::Body;
    ::fluent::basicinput::Button* m_clearButton = nullptr;
    bool     m_clearButtonEnabled = true;
    int      m_clearButtonSize    = 22;
    QPoint   m_clearButtonOffset  = QPoint(::Spacing::XSmall, 0);
    int      m_focusedBorderWidth   = ::Spacing::Border::Focused;
    int      m_unfocusedBorderWidth = ::Spacing::Border::Normal;
    bool     m_frameVisible = true;
    bool     m_isHovered = false;
    bool     m_isFocused = false;
};

} // namespace fluent::textfields

#endif // LINEEDIT_H
