#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include <QMargins>
#include <QWidget>
#include <QString>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "design/Spacing.h"
#include "design/Typography.h"

class QTextEdit;
class QPainter;
class QPaintEvent;

namespace fluent::scrolling { class ScrollBar; }

namespace fluent::textfields {

/**
 * @brief Fluent multi-line text input with line-count based sizing.
 * zh_CN: 支持按可见行数计算尺寸的 Fluent 多行文本输入框。
 *
 * TextEdit hosts QTextEdit-style editing while exposing Fluent frame, typography,
 * content margins, focused border, and min/max visible line metrics.
 * zh_CN: TextEdit 承载 QTextEdit 式编辑，并暴露 Fluent 外框、排版、内容边距、
 * 聚焦边线以及最小/最大可见行数参数。
 */
class TextEdit : public QWidget, public FluentElement, public QMLPlus {
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
     * @brief Visual line slot height used by the text editor.
     * zh_CN: 文本编辑器使用的视觉行槽高度。
     */
    Q_PROPERTY(int lineHeight READ lineHeight WRITE setLineHeight NOTIFY layoutMetricsChanged)
    /**
     * @brief Minimum number of visible text lines.
     * zh_CN: 最少可见文本行数。
     */
    Q_PROPERTY(int minVisibleLines READ minVisibleLines WRITE setMinVisibleLines NOTIFY layoutMetricsChanged)
    /**
     * @brief Maximum number of visible text lines before scrolling.
     * zh_CN: 滚动前最多可见文本行数。
     */
    Q_PROPERTY(int maxVisibleLines READ maxVisibleLines WRITE setMaxVisibleLines NOTIFY layoutMetricsChanged)

public:
    explicit TextEdit(QWidget* parent = nullptr);

    // 文本相关 API
    void setPlainText(const QString& text);
    QString toPlainText() const;
    void clear();

    void setPlaceholderText(const QString& text);
    QString placeholderText() const;

    void setReadOnly(bool readOnly);
    bool isReadOnly() const;

    ::fluent::scrolling::ScrollBar* verticalScrollBar() const;

    void setFocus();
    void setFocus(Qt::FocusReason reason);

    void onThemeUpdated() override;

    QMargins contentMargins() const { return m_contentMargins; }
    void setContentMargins(const QMargins& margins);

    QString fontRole() const { return m_fontRole; }
    void setFontRole(const QString& role);

    int focusedBorderWidth() const { return m_focusedBorderWidth; }
    void setFocusedBorderWidth(int width);

    int unfocusedBorderWidth() const { return m_unfocusedBorderWidth; }
    void setUnfocusedBorderWidth(int width);

    int lineHeight() const { return m_lineHeight; }
    void setLineHeight(int height);

    int minVisibleLines() const { return m_minVisibleLines; }
    void setMinVisibleLines(int lines);

    int maxVisibleLines() const { return m_maxVisibleLines; }
    void setMaxVisibleLines(int lines);

signals:
    void textChanged();
    void cursorPositionChanged();
    void selectionChanged();
    void contentMarginsChanged();
    void fontRoleChanged();
    void focusedBorderWidthChanged();
    void unfocusedBorderWidthChanged();
    void layoutMetricsChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void applyThemeStyle();
    void paintFrame(QPainter& painter);
    void updateHeightForContent();

    /**
     * @brief 设置 rootFrame margin + block bottomMargin 实现垂直居中，
     *        并设置左右 viewport margins。
     */
    void applyBlockCenterFormat();

    QTextEdit*                    m_editor      = nullptr;
    ::fluent::scrolling::ScrollBar* m_vScrollBar  = nullptr;
    bool m_updatingFormat = false;
    bool m_scrollEnabled  = false;

    QMargins m_contentMargins   = QMargins(::Spacing::Padding::TextFieldHorizontal,
                                           ::Spacing::Padding::TextFieldVertical,
                                           ::Spacing::Padding::TextFieldHorizontal,
                                           ::Spacing::Padding::TextFieldVertical);
    QString  m_fontRole         = Typography::FontRole::Body;
    bool     m_isHovered        = false;
    bool     m_isFocused        = false;
    int      m_focusedBorderWidth   = ::Spacing::Border::Focused;
    int      m_unfocusedBorderWidth = ::Spacing::Border::Normal;
    int      m_lineHeight           = ::Spacing::ControlHeight::Standard;
    int      m_minVisibleLines      = 1;
    int      m_maxVisibleLines      = 4;
    QString  m_placeholderText;
};

} // namespace fluent::textfields

#endif // TEXTEDIT_H
