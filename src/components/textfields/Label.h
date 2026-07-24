#ifndef LABEL_H
#define LABEL_H

#include <QLabel>
#include <Qt>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::status_info { class ToolTip; }

namespace fluent::textfields {

/**
 * @brief Fluent typography label with optional text elision.
 * zh_CN: 支持可选文本省略的 Fluent 排版标签。
 *
 * Label maps text styling to Fluent typography roles while preserving QLabel
 * alignment, text content, and standard widget behavior.
 * zh_CN: Label 将文本样式映射到 Fluent 排版角色，同时保留 QLabel 的对齐、文本内容和标准控件行为。
 */
class Label : public QLabel, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Full, non-elided label text exposed through the derived meta-object.
     * zh_CN: 通过派生类元对象暴露的完整、未省略标签文本。
     */
    Q_PROPERTY(QString text READ text WRITE setText)
    /**
     * @brief Fluent typography role applied to label text.
     * zh_CN: 应用到标签文本的 Fluent 排版角色。
     */
    Q_PROPERTY(QString fluentTypography READ fluentTypography WRITE setFluentTypography NOTIFY typographyChanged)
    /**
     * @brief Elide mode used when label text overflows.
     * zh_CN: 标签文本溢出时使用的省略模式。
     */
    Q_PROPERTY(Qt::TextElideMode textElideMode READ textElideMode WRITE setTextElideMode NOTIFY textElideModeChanged)

public:
    explicit Label(const QString& text, QWidget* parent = nullptr);
    explicit Label(QWidget* parent = nullptr);
    ~Label() override;

    QString text() const { return m_fullText; }
    void setText(const QString& text);

    QString fluentTypography() const { return m_styleName; }
    void setFluentTypography(const QString& styleName);

    Qt::TextElideMode textElideMode() const { return m_textElideMode; }
    void setTextElideMode(Qt::TextElideMode mode);

    bool isTextElided() const { return m_isTextElided; }

    /**
     * @brief How the label resolves its text color on theme changes.
     * zh_CN: 标签在主题切换时如何解析文本颜色。
     *
     * Default keeps the legacy palette-based coloring (QPalette::WindowText = textPrimary), which is
     * correct as long as no ancestor has a style sheet. Any explicit role colors the text through the
     * label's OWN style sheet instead, so an ancestor style sheet (which installs QStyleSheetStyle and
     * makes Qt ignore the child palette) can't drop it — the fix for value/status labels that sit on a
     * styled preview surface and rendered near-black in dark theme.
     * zh_CN: Default 保持原有基于 palette 的上色(QPalette::WindowText = textPrimary),仅当没有祖先样式表时正确。
     * 指定任一角色则改用标签自身的样式表上色,使祖先样式表(会安装 QStyleSheetStyle 并让 Qt 忽略子 palette)无法丢弃它
     * ——修复那些坐在带样式表的预览面上、深色主题里渲染成近黑的取值/状态标签。
     */
    enum class TextColorRole { Default, Primary, Secondary, Tertiary, Disabled, OnAccent, Accent };

    TextColorRole textColorRole() const { return m_textColorRole; }
    void setTextColorRole(TextColorRole role);

    void setFont(const QFont& font);

    void onThemeUpdated() override;

signals:
    void typographyChanged();
    void textElideModeChanged(Qt::TextElideMode mode);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void applyTypographyFont();
    void applyTextColor();
    QColor resolveTextColor() const;
    void updateRenderedText();
    int availableTextWidth() const;
    void ensureElideToolTip();
    void showElideToolTip();
    void hideElideToolTip();
    void positionElideToolTip();

    QString m_styleName = "Body";
    QString m_fullText;
    TextColorRole m_textColorRole = TextColorRole::Default;
    Qt::TextElideMode m_textElideMode = Qt::ElideNone;
    bool m_customFont = false;
    bool m_isTextElided = false;
    fluent::status_info::ToolTip* m_elideToolTip = nullptr;
};

} // namespace fluent::textfields

#endif // LABEL_H
