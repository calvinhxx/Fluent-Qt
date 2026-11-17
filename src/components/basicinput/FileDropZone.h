#ifndef FLUENTQT_COMPONENTS_BASICINPUT_FILEDROPZONE_H
#define FLUENTQT_COMPONENTS_BASICINPUT_FILEDROPZONE_H

#include "compatibility/InputEventCompat.h"
#include <QList>
#include <QUrl>
#include <QWidget>
#include <array>

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
 * @brief Drag-and-drop area for local file URLs with an application-handled browse button.
 * Emits dropped local file URLs without inspecting or uploading files; mixed local/remote drags are rejected.
 * The embedded browse button is the sole tab stop and activates on Space or Return.
 * zh_CN: 支持本地文件拖放与外部浏览触发的文件入口控件。
 * 仅透传本地文件 URL（混入远程 URL 时整批拒绝），不打开、校验或上传文件；文件对话框与存储由应用自行管理。
 * 内置选择按钮是唯一 Tab 焦点停靠点，支持空格或回车触发。
 */
class FileDropZone : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
    Q_PROPERTY(QString hintText READ hintText WRITE setHintText NOTIFY hintTextChanged)
    Q_PROPERTY(QString browseText READ browseText WRITE setBrowseText NOTIFY browseTextChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage WRITE setErrorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool dragActive READ isDragActive NOTIFY dragActiveChanged)

public:
    explicit FileDropZone(QWidget* parent = nullptr);

    QString title() const { return m_title; }
    void setTitle(const QString& title);

    QString description() const { return m_description; }
    void setDescription(const QString& description);

    /** @brief Optional format or size limit hint text. zh_CN: 可选的格式或大小限制提示文案。 */
    QString hintText() const { return m_hintText; }
    void setHintText(const QString& text);

    QString browseText() const { return m_browseText; }
    void setBrowseText(const QString& text);

    /**
     * @brief Validation error text that temporarily replaces description until cleared.
     * Subsequent drops do not clear it automatically.
     * zh_CN: 校验错误提示（清空前会替换说明文本；新的拖放不会自动清除该提示）。
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
