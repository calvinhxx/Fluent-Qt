#ifndef LABEL_H
#define LABEL_H

#include "compatibility/InputEventCompat.h"
#include <QLabel>
#include <Qt>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::status_info {
class ToolTip;
}
class QContextMenuEvent;

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
     * @brief Full, non-elided label text.
     * zh_CN: 完整（未经省略截断）的标签文本。
     */
    Q_PROPERTY(QString text READ text WRITE setText)
    /**
     * @brief Fluent typography role applied to label text.
     * zh_CN: 应用到标签文本的 Fluent 排版角色。
     */
    Q_PROPERTY(Typography::FontRole fluentTypography
        READ fluentTypography
        WRITE setFluentTypography
        NOTIFY typographyChanged)
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

    Typography::FontRole fluentTypography() const { return m_fontRole; }
    void setFluentTypography(Typography::FontRole role);

    Qt::TextElideMode textElideMode() const { return m_textElideMode; }
    void setTextElideMode(Qt::TextElideMode mode);

    bool isTextElided() const { return m_isTextElided; }

    /**
     * @brief Text color source used on theme updates.
     *
     * Default uses QPalette::WindowText. Any explicit role sets the label's own
     * style sheet so an ancestor style sheet cannot override the color.
     * zh_CN: 主题更新时文本颜色的设置方式：Default 使用 QPalette::WindowText，指定明确角色时
     * 改用自身样式表设置颜色，避免被祖先样式表覆盖。
     */
    enum class TextColorRole { Default, Primary, Secondary, Tertiary, Disabled, OnAccent, Accent, Critical };

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
    void contextMenuEvent(QContextMenuEvent* event) override;

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

    Typography::FontRole m_fontRole = Typography::FontRole::Body;
    QString m_fullText;
    TextColorRole m_textColorRole = TextColorRole::Default;
    Qt::TextElideMode m_textElideMode = Qt::ElideNone;
    bool m_customFont = false;
    bool m_isTextElided = false;
    fluent::status_info::ToolTip* m_elideToolTip = nullptr;
};

} // namespace fluent::textfields

#endif // LABEL_H
