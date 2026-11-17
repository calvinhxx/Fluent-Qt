#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include "compatibility/InputEventCompat.h"
#include <QMargins>
#include <QRect>
#include <QWidget>
#include <QString>
#include <QTextCursor>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "design/Spacing.h"
#include "design/Typography.h"

class QTextEdit;
class QPainter;
class QPaintEvent;

namespace fluent::scrolling {
class ScrollBar;
}

namespace fluent::textfields {

/**
 * @brief Fluent multi-line text input with line-count based sizing.
 * Provides QTextEdit editing with Fluent framing, typography, margins, and min/max visible lines.
 * zh_CN: 支持按可见行数自适应高度的 Fluent 多行文本输入框，提供统一的外框、字号样式、边距及最小/最大行数控制。
 */
class TextEdit : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Minimum insets around the editable text; vertical margins fit inside
     *        lineHeight and expand the control only when exceeding it.
     * zh_CN: 文本四周的最小内边距；垂直边距优先利用行高富余空间，超出时才撑高控件。
     */
    Q_PROPERTY(QMargins contentMargins READ contentMargins WRITE setContentMargins NOTIFY contentMarginsChanged)
    /**
     * @brief Fluent Typography preset used for text.
     * zh_CN: 文本使用的预设字号样式。
     */
    Q_PROPERTY(Typography::FontRole fontRole READ fontRole WRITE setFontRole NOTIFY fontRoleChanged)
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
     * @brief Visual line slot height used by the text editor.
     * zh_CN: 文本编辑器使用的视觉行槽高度。
     */
    Q_PROPERTY(int lineHeight READ lineHeight WRITE setLineHeight NOTIFY layoutMetricsChanged)
    /**
     * @brief Minimum visible lines; raising it above the maximum raises both.
     * zh_CN: 最少可见文本行数；设置值高于最大值时会同步提高两者。
     */
    Q_PROPERTY(int minVisibleLines READ minVisibleLines WRITE setMinVisibleLines NOTIFY layoutMetricsChanged)
    /**
     * @brief Maximum visible lines; lowering it below the minimum lowers both.
     * zh_CN: 滚动前最多可见文本行数；设置值低于最小值时会同步降低两者。
     */
    Q_PROPERTY(int maxVisibleLines READ maxVisibleLines WRITE setMaxVisibleLines NOTIFY layoutMetricsChanged)
    /**
     * @brief Whether Tab advances focus instead of inserting a tab character.
     * zh_CN: Tab 键是否移动焦点而不是插入制表符。
     */
    Q_PROPERTY(bool tabChangesFocus READ tabChangesFocus WRITE setTabChangesFocus NOTIFY tabChangesFocusChanged)
    /**
     * @brief Whether boundary wheel input may continue to an enclosing scroller.
     * zh_CN: 边界滚轮输入是否允许继续传递给外层滚动容器。
     */
    Q_PROPERTY(bool scrollChainingEnabled
        READ isScrollChainingEnabled
        WRITE setScrollChainingEnabled
        NOTIFY scrollChainingEnabledChanged)

public:
    explicit TextEdit(QWidget* parent = nullptr);

    /** @brief Whether content controls the height; disable to fill a parent layout.
     * zh_CN: 是否由内容决定高度；关闭后填充父布局分配的空间。 */
    Q_PROPERTY(bool autoResizeEnabled
        READ isAutoResizeEnabled
        WRITE setAutoResizeEnabled
        NOTIFY autoResizeEnabledChanged)
    bool isAutoResizeEnabled() const { return m_autoResizeEnabled; }
    void setAutoResizeEnabled(bool enabled);
    /** @brief Whether the editor paints an input frame and background.
     * zh_CN: 是否绘制输入框边框和背景。 */
    Q_PROPERTY(bool frameVisible READ isFrameVisible WRITE setFrameVisible NOTIFY frameVisibleChanged)
    bool isFrameVisible() const { return m_frameVisible; }
    void setFrameVisible(bool visible);
    /** @brief Returns the current editing cursor, including its selection.
     * zh_CN: 返回包含选区的当前编辑光标。 */
    QTextCursor textCursor() const;
    /** @brief Current caret rectangle in widget coordinates, including scroll offsets (unclipped if scrolled offscreen).
     * zh_CN: 当前光标在本控件坐标系中的未裁剪矩形（包含滚动偏移，滚出视口时可能位于控件外）。 */
    Q_PROPERTY(QRect cursorRect READ cursorRect NOTIFY cursorRectChanged)
    QRect cursorRect() const;
    /** @brief Sets a cursor belonging to this editor's document.
     * zh_CN: 设置属于本编辑器文档的光标。 */
    void setTextCursor(const QTextCursor& cursor);

    // Text APIs. zh_CN: 文本相关 API。
    void setPlainText(const QString& text);
    /**
     * @brief Appends a plain-text paragraph at the end, even when read-only.
     * Preserves the cursor and selection, appends empty strings as blank lines, and auto-scrolls only if already at the bottom.
     * zh_CN: 在末尾追加纯文本段落（只读模式可用，空串也会换行）；保留光标与选区，仅在原本位于底部时自动滚动到底部。
     */
    void appendPlainText(const QString& text);
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

    Typography::FontRole fontRole() const { return m_fontRole; }
    void setFontRole(Typography::FontRole role);

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

    bool tabChangesFocus() const { return m_tabChangesFocus; }
    void setTabChangesFocus(bool enabled);

    bool isScrollChainingEnabled() const { return m_scrollChainingEnabled; }
    void setScrollChainingEnabled(bool enabled);

signals:
    void autoResizeEnabledChanged();
    void frameVisibleChanged();
    void textChanged();
    void cursorPositionChanged();
    /** @brief Emitted after the caret's effective geometry changes; updates are coalesced.
     * zh_CN: 光标的实际几何发生变化后发出，同一轮布局更新会合并通知。 */
    void cursorRectChanged();
    void selectionChanged();
    void contentMarginsChanged();
    void fontRoleChanged();
    void focusedBorderWidthChanged();
    void unfocusedBorderWidthChanged();
    void layoutMetricsChanged();
    void tabChangesFocusChanged();
    void scrollChainingEnabledChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void applyEditorPalette();
    void updateEditorGeometry();
    void applyThemeStyle();
    void paintFrame(QPainter& painter);
    void scheduleHeightForContentUpdate();
    void updateHeightForContent();
    void scheduleCursorRectUpdate();

    /**
     * @brief Applies requested text insets, remaining vertical centering, and
     *        viewport margins.
     * zh_CN: 应用文本内边距、剩余垂直居中空间及 viewport margin。
     */
    void applyBlockCenterFormat();

    QTextEdit* m_editor = nullptr;
    bool m_autoResizeEnabled = true;
    QSizePolicy m_autoResizePolicy;
    bool m_frameVisible = true;
    ::fluent::scrolling::ScrollBar* m_vScrollBar = nullptr;
    bool m_updatingFormat = false;
    bool m_updatingHeight = false;
    bool m_heightUpdateScheduled = false;
    bool m_scrollEnabled = false;
    bool m_cursorRectUpdateScheduled = false;
    QRect m_lastCursorRect;

    QMargins m_contentMargins = QMargins(::Spacing::Padding::TextFieldHorizontal,
        ::Spacing::Padding::TextFieldVertical,
        ::Spacing::Padding::TextFieldHorizontal,
        ::Spacing::Padding::TextFieldVertical);
    Typography::FontRole m_fontRole = Typography::FontRole::Body;
    bool m_isHovered = false;
    bool m_isFocused = false;
    int m_focusedBorderWidth = ::Spacing::Border::Focused;
    int m_unfocusedBorderWidth = 0;
    int m_lineHeight = ::Spacing::ControlHeight::Standard;
    int m_minVisibleLines = 1;
    int m_maxVisibleLines = 4;
    bool m_tabChangesFocus = false;
    bool m_scrollChainingEnabled = false;
    QString m_placeholderText;
};

} // namespace fluent::textfields

#endif // TEXTEDIT_H
