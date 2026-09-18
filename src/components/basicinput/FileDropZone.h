#ifndef FLUENTQT_COMPONENTS_BASICINPUT_FILEDROPZONE_H
#define FLUENTQT_COMPONENTS_BASICINPUT_FILEDROPZONE_H

#include <QList>
#include <QUrl>
#include <QWidget>
#include <array>

#include "compatibility/QtCompat.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QHideEvent;
class QMimeData;
class QVariantAnimation;

namespace fluent {
class FontIcon;
namespace textfields {
class Label;
}
} // namespace fluent

namespace fluent::basicinput {

class Button;

/**
 * @brief File entry surface with local-URL drop and application-owned browsing.
 * zh_CN: 支持本地 URL 拖放、由应用负责文件选择的文件入口。
 *
 * The component emits URLs without opening, inspecting, validating, or uploading
 * files. The application owns its picker, validation rules, and file model.
 * A mixed local/remote URL drag is rejected as a whole. Keyboard users activate
 * the embedded browse button with Space or Return; it is the only tab stop.
 * zh_CN: 控件仅发送 URL，不打开、检查、校验或上传文件。文件选择器、校验规则和
 * 文件模型由应用持有。本地与远程 URL 混合拖入时整批拒绝。内置选择按钮是唯一
 * Tab 停靠点，支持空格或回车激活。
 */
class FileDropZone : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
    Q_PROPERTY(QString hintText READ hintText WRITE setHintText NOTIFY hintTextChanged)
    Q_PROPERTY(QString browseText READ browseText WRITE setBrowseText NOTIFY browseTextChanged)
    Q_PROPERTY(
        QString errorMessage READ errorMessage WRITE setErrorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool dragActive READ isDragActive NOTIFY dragActiveChanged)

public:
    explicit FileDropZone(QWidget* parent = nullptr);

    QString title() const { return m_title; }
    void setTitle(const QString& title);

    QString description() const { return m_description; }
    void setDescription(const QString& description);

    /** @brief Optional application-owned limits or format hint. zh_CN: 可选的应用限制或格式提示。 */
    QString hintText() const { return m_hintText; }
    void setHintText(const QString& text);

    QString browseText() const { return m_browseText; }
    void setBrowseText(const QString& text);

    /**
     * @brief Validation feedback that replaces the description until cleared.
     * zh_CN: 在清空前替换说明文本的校验反馈。
     *
     * A new drop does not implicitly clear application-owned feedback.
     * zh_CN: 新的拖放不会隐式清除应用设置的反馈。
     */
    QString errorMessage() const { return m_errorMessage; }
    void setErrorMessage(const QString& message);
    void clearError();

    bool isDragActive() const { return m_dragActive; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    void onThemeUpdated() override;

signals:
    void titleChanged(const QString& title);
    void descriptionChanged(const QString& description);
    void hintTextChanged(const QString& text);
    void browseTextChanged(const QString& text);
    void errorMessageChanged(const QString& message);
    void dragActiveChanged(bool active);
    /** @brief Requests an application-provided file picker. zh_CN: 请求应用提供的文件选择器。 */
    void browseRequested();
    /**
     * @brief Delivers local URLs in their original order after an accepted copy drop.
     * zh_CN: 接受复制拖放后，按原始顺序发送本地 URL。
     */
    void filesDropped(const QList<QUrl>& urls);

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    using VisualState = std::array<qreal, 3>;

    static QList<QUrl> localUrls(const QMimeData* data);
    int contentHeight(int width) const;
    void layoutContent();
    void updatePresentation();
    void updateAccessibleText();
    void setDragActive(bool active, int fileCount = 0);
    void transitionToState(bool animated = true);
    void applyVisualState();
    bool resetInteraction();

    QString m_title;
    QString m_description;
    QString m_hintText;
    QString m_browseText;
    QString m_errorMessage;
    QString m_autoAccessibleName;
    QString m_autoAccessibleDescription;
    bool m_dragActive = false;
    bool m_hovered = false;
    bool m_keyboardFocusVisible = false;
    int m_dragFileCount = 0;

    FontIcon* m_icon = nullptr;
    textfields::Label* m_titleLabel = nullptr;
    textfields::Label* m_descriptionLabel = nullptr;
    textfields::Label* m_hintLabel = nullptr;
    Button* m_browseButton = nullptr;
    QRect m_iconTileRect;
    QVariantAnimation* m_transition = nullptr;
    VisualState m_visual{0.0, 0.0, 0.0};
    VisualState m_transitionStart{0.0, 0.0, 0.0};
    VisualState m_transitionEnd{0.0, 0.0, 0.0};
};

} // namespace fluent::basicinput

#endif // FLUENTQT_COMPONENTS_BASICINPUT_FILEDROPZONE_H
