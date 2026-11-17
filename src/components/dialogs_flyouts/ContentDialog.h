#ifndef CONTENTDIALOG_H
#define CONTENTDIALOG_H

#include "Dialog.h"

#include <QPointer>

namespace fluent::basicinput {
class Button;
}
namespace fluent::textfields {
class Label;
}

namespace fluent::dialogs_flyouts {

/**
 * @brief WinUI-style content dialog with title, content area, and action buttons.
 * zh_CN: 具备标题、内容区和动作按钮的 WinUI 风格内容对话框。
 *
 * Extends Dialog with primary, secondary, and close buttons, button-text
 * properties, default-button accent highlighting, and anchored content layout.
 * zh_CN: 在 Dialog 基础上增加主按钮、次按钮、关闭按钮及其文本配置、默认按钮强调样式和内容区布局。
 */
class ContentDialog : public Dialog {
    Q_OBJECT
    /**
     * @brief Title text shown in the dialog header.
     * zh_CN: 对话框标题文本。
     */
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    /**
     * @brief Text displayed by the primary command button.
     * zh_CN: 主命令按钮显示的文本。
     */
    Q_PROPERTY(QString primaryButtonText
        READ primaryButtonText
        WRITE setPrimaryButtonText
        NOTIFY primaryButtonTextChanged)
    /**
     * @brief Text displayed by the secondary command button.
     * zh_CN: 次命令按钮显示的文本。
     */
    Q_PROPERTY(QString secondaryButtonText
        READ secondaryButtonText
        WRITE setSecondaryButtonText
        NOTIFY secondaryButtonTextChanged)
    /**
     * @brief Text displayed by the close command button.
     * zh_CN: 关闭命令按钮显示的文本。
     */
    Q_PROPERTY(QString closeButtonText READ closeButtonText WRITE setCloseButtonText NOTIFY closeButtonTextChanged)
    /**
     * @brief Command button highlighted as the default action.
     * zh_CN: 作为默认动作高亮显示的命令按钮。
     */
    Q_PROPERTY(int defaultButton READ defaultButton WRITE setDefaultButton NOTIFY defaultButtonChanged)

public:
    enum ContentDialogButton { None = 0, Primary, Secondary, Close };
    Q_ENUM(ContentDialogButton)

    static constexpr int ResultNone = QDialog::Rejected;    // 0
    static constexpr int ResultPrimary = QDialog::Accepted; // 1
    static constexpr int ResultSecondary = 2;

    explicit ContentDialog(QWidget* parent = nullptr);

    QString title() const;
    void setTitle(const QString& text);

    QString primaryButtonText() const;
    void setPrimaryButtonText(const QString& text);

    QString secondaryButtonText() const;
    void setSecondaryButtonText(const QString& text);

    QString closeButtonText() const;
    void setCloseButtonText(const QString& text);

    int defaultButton() const;
    void setDefaultButton(int btn);

    QWidget* content() const;
    void setContent(QWidget* widget);

    /**
     * @brief Overrides the bottom button-bar height (default 68 px).
     * zh_CN: 覆盖底部按钮栏高度（默认 68 px）。
     */
    void setButtonBarHeight(int px);

    void onThemeUpdated() override;

signals:
    void primaryButtonClicked();
    void secondaryButtonClicked();
    void closeButtonClicked();
    void titleChanged(const QString& text);
    void primaryButtonTextChanged(const QString& text);
    void secondaryButtonTextChanged(const QString& text);
    void closeButtonTextChanged(const QString& text);
    void defaultButtonChanged(int btn);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void setupInternalLayout();
    void updateButtonBar();
    void updateContentAnchors();
    void ensureContentFits();

    fluent::textfields::Label* m_titleLabel = nullptr;
    QPointer<QWidget> m_contentWidget;
    QWidget* m_buttonBar = nullptr;

    fluent::basicinput::Button* m_primaryBtn = nullptr;
    fluent::basicinput::Button* m_secondaryBtn = nullptr;
    fluent::basicinput::Button* m_closeBtn = nullptr;

    int m_defaultButton = None;
    int m_buttonBarHeight = 68; // Default mirrors kButtonBarHeight. zh_CN: 默认与 kButtonBarHeight 一致。
};

} // namespace fluent::dialogs_flyouts

#endif // CONTENTDIALOG_H
