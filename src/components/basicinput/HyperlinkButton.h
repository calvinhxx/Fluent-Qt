#ifndef HYPERLINKBUTTON_H
#define HYPERLINKBUTTON_H

#include "Button.h"
#include <QUrl>

namespace fluent::basicinput {

namespace detail {
class HyperlinkButtonAccessible;
}

/**
 * @brief Text-styled navigation button with optional URL and hover underline.
 * zh_CN: 文本链接样式的导航按钮，支持关联 URL 与悬停下划线。
 */
class HyperlinkButton : public Button {
    Q_OBJECT
    /**
     * @brief Target URL activated by the hyperlink button.
     * zh_CN: 超链接按钮激活的目标 URL。
     */
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
    /**
     * @brief Whether the hyperlink text draws an underline while hovered.
     * zh_CN: 悬停时是否为超链接文本绘制下划线。
     */
    Q_PROPERTY(bool showUnderline READ showUnderline WRITE setShowUnderline NOTIFY showUnderlineChanged)

public:
    explicit HyperlinkButton(const QString& text = "", QWidget* parent = nullptr);
    explicit HyperlinkButton(const QString& text, const QUrl& url, QWidget* parent = nullptr);

    QUrl url() const { return m_url; }
    void setUrl(const QUrl& url);

    bool showUnderline() const { return m_showUnderline; }
    void setShowUnderline(bool show);

    void onThemeUpdated() override;

signals:
    void urlChanged();
    void showUnderlineChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    friend class detail::HyperlinkButtonAccessible;

    QUrl m_url;
    bool m_showUnderline = false; // 默认悬停不显示下划线
    bool m_accessibilityVisited = false;
    bool m_keyboardFocusVisible = false;
};

} // namespace fluent::basicinput

#endif // HYPERLINKBUTTON_H
