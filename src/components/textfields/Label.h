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
    void updateRenderedText();
    int availableTextWidth() const;
    void ensureElideToolTip();
    void showElideToolTip();
    void hideElideToolTip();
    void positionElideToolTip();

    QString m_styleName = "Body";
    QString m_fullText;
    Qt::TextElideMode m_textElideMode = Qt::ElideNone;
    bool m_customFont = false;
    bool m_isTextElided = false;
    fluent::status_info::ToolTip* m_elideToolTip = nullptr;
};

} // namespace fluent::textfields

#endif // LABEL_H
