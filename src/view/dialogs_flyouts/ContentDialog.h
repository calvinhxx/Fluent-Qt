#ifndef CONTENTDIALOG_H
#define CONTENTDIALOG_H

#include "Dialog.h"

namespace view::basicinput { class Button; }
namespace view::textfields { class Label; }

namespace view::dialogs_flyouts {

/**
 * @brief WinUI-style content dialog with title, content, and command buttons.
 * zh_CN: 具备标题、内容和命令按钮的 WinUI 风格内容对话框。
 *
 * ContentDialog specializes Dialog with primary, secondary, and close actions,
 * button text APIs, default-button selection, and standard content hosting.
 * zh_CN: ContentDialog 在 Dialog 基础上增加主按钮、次按钮、关闭按钮、按钮文本 API、
 * 默认按钮选择和标准内容承载。
 */
class ContentDialog : public Dialog {
    Q_OBJECT
    /**
     * @brief Title text displayed by the surface.
     * zh_CN: 界面显示的标题文本。
     */
    Q_PROPERTY(QString title READ title WRITE setTitle)
    /**
     * @brief Text displayed by the primary command button.
     * zh_CN: 主命令按钮显示的文本。
     */
    Q_PROPERTY(QString primaryButtonText READ primaryButtonText WRITE setPrimaryButtonText)
    /**
     * @brief Text displayed by the secondary command button.
     * zh_CN: 次命令按钮显示的文本。
     */
    Q_PROPERTY(QString secondaryButtonText READ secondaryButtonText WRITE setSecondaryButtonText)
    /**
     * @brief Text displayed by the close command button.
     * zh_CN: 关闭命令按钮显示的文本。
     */
    Q_PROPERTY(QString closeButtonText READ closeButtonText WRITE setCloseButtonText)
    /**
     * @brief Command button selected as the default action.
     * zh_CN: 作为默认操作的命令按钮。
     */
    Q_PROPERTY(int defaultButton READ defaultButton WRITE setDefaultButton)

public:
    enum ContentDialogButton { None = 0, Primary, Secondary, Close };
    Q_ENUM(ContentDialogButton)

    static constexpr int ResultNone      = QDialog::Rejected; // 0
    static constexpr int ResultPrimary   = QDialog::Accepted; // 1
    static constexpr int ResultSecondary = 2;

    explicit ContentDialog(QWidget *parent = nullptr);

    // --- Title ---
    QString title() const;
    void setTitle(const QString& text);

    // --- Button text ---
    QString primaryButtonText() const;
    void setPrimaryButtonText(const QString& text);

    QString secondaryButtonText() const;
    void setSecondaryButtonText(const QString& text);

    QString closeButtonText() const;
    void setCloseButtonText(const QString& text);

    // --- DefaultButton ---
    int  defaultButton() const;
    void setDefaultButton(int btn);

    // --- Content ---
    QWidget* content() const;
    void setContent(QWidget* widget);

    void onThemeUpdated() override;

signals:
    void primaryButtonClicked();
    void secondaryButtonClicked();
    void closeButtonClicked();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setupInternalLayout();
    void updateButtonBar();
    void updateContentAnchors();

    view::textfields::Label* m_titleLabel   = nullptr;
    QWidget*                     m_contentWidget = nullptr;
    QWidget*                     m_buttonBar     = nullptr;

    view::basicinput::Button*    m_primaryBtn    = nullptr;
    view::basicinput::Button*    m_secondaryBtn  = nullptr;
    view::basicinput::Button*    m_closeBtn      = nullptr;

    int m_defaultButton = None;
};

} // namespace view::dialogs_flyouts

#endif // CONTENTDIALOG_H
