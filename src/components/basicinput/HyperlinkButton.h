#ifndef HYPERLINKBUTTON_H
#define HYPERLINKBUTTON_H

#include "Button.h"
#include <QUrl>

namespace fluent::basicinput {

/**
 * @brief Button-styled hyperlink entry with URL and underline control.
 * zh_CN: 具备 URL 和下划线控制的按钮式超链接入口。
 *
 * HyperlinkButton keeps a lightweight command surface for navigation actions
 * while preserving Fluent text-button styling and optional underline feedback.
 * zh_CN: HyperlinkButton 为导航操作提供轻量命令表面，并保留 Fluent 文本按钮风格
 * 以及可选下划线反馈。
 */
class HyperlinkButton : public Button {
    Q_OBJECT
    /**
     * @brief Target URL activated by the hyperlink button.
     * zh_CN: 超链接按钮激活的目标 URL。
     */
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
    /**
     * @brief Whether the hyperlink text draws an underline.
     * zh_CN: 超链接文本是否绘制下划线。
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

private:
    QUrl m_url;
    bool m_showUnderline = false; // 默认悬停不显示下划线
};

} // namespace fluent::basicinput

#endif // HYPERLINKBUTTON_H
