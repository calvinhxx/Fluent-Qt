#ifndef INFOBAR_H
#define INFOBAR_H

#include <QColor>
#include <QMargins>
#include <QString>
#include <QWidget>

#include "design/Typography.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QResizeEvent;
class QEvent;
class QPainter;

namespace fluent::basicinput {
class Button;
}

namespace fluent::textfields {
class Label;
}

namespace fluent::status_info {

/**
 * @brief Fluent inline notification bar with severity, actions, and close behavior.
 * zh_CN: 带严重级别、操作区和关闭行为的 Fluent 内联通知栏。
 *
 * InfoBar displays title and message content with optional action widgets,
 * severity icons, close affordance, and open-state animation.
 * zh_CN: InfoBar 展示标题和消息内容，并支持可选操作控件、严重级别图标、关闭入口和打开态动画。
 */
class InfoBar : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Whether the popup, drawer, or notification surface is open.
     * zh_CN: 弹层、抽屉或通知表面是否处于打开状态。
     */
    Q_PROPERTY(bool isOpen READ isOpen WRITE setIsOpen NOTIFY isOpenChanged)
    /**
     * @brief Title text displayed by the surface.
     * zh_CN: 界面显示的标题文本。
     */
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    /**
     * @brief Message body text displayed by the notification surface.
     * zh_CN: 通知表面显示的消息正文。
     */
    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged)
    /**
     * @brief Severity level used by notification colors and icon.
     * zh_CN: 通知颜色和图标使用的严重级别。
     */
    Q_PROPERTY(InfoBarSeverity severity READ severity WRITE setSeverity NOTIFY severityChanged)
    /**
     * @brief Whether the close affordance is visible and active.
     * zh_CN: 关闭入口是否可见且可用。
     */
    Q_PROPERTY(bool isClosable READ isClosable WRITE setIsClosable NOTIFY isClosableChanged)
    /**
     * @brief Whether the severity icon is visible.
     * zh_CN: 严重级别图标是否可见。
     */
    Q_PROPERTY(bool isIconVisible READ isIconVisible WRITE setIsIconVisible NOTIFY isIconVisibleChanged)
    /**
     * @brief Whether the notification uses compact single-line layout.
     * zh_CN: 通知是否使用紧凑单行布局。
     */
    Q_PROPERTY(bool singleLine READ singleLine WRITE setSingleLine NOTIFY singleLineChanged)
    /**
     * @brief Preferred width used by the notification layout.
     * zh_CN: 通知布局使用的首选宽度。
     */
    Q_PROPERTY(int preferredWidth READ preferredWidth WRITE setPreferredWidth NOTIFY preferredWidthChanged)
    /**
     * @brief Height used by single-line notification layout.
     * zh_CN: 单行通知布局使用的高度。
     */
    Q_PROPERTY(int singleLineHeight READ singleLineHeight WRITE setSingleLineHeight NOTIFY singleLineHeightChanged)
    /**
     * @brief Minimum height used by multi-line notification layout.
     * zh_CN: 多行通知布局使用的最小高度。
     */
    Q_PROPERTY(int multiLineMinHeight READ multiLineMinHeight WRITE setMultiLineMinHeight NOTIFY multiLineMinHeightChanged)
    /**
     * @brief Minimum height when multi-line layout includes actions.
     * zh_CN: 多行布局包含操作区时使用的最小高度。
     */
    Q_PROPERTY(int multiLineActionMinHeight READ multiLineActionMinHeight WRITE setMultiLineActionMinHeight NOTIFY multiLineActionMinHeightChanged)
    /**
     * @brief Optional action widget hosted by the notification.
     * zh_CN: 通知承载的可选操作控件。
     */
    Q_PROPERTY(QWidget* actionWidget READ actionWidget WRITE setActionWidget NOTIFY actionWidgetChanged)
    /**
     * @brief Margins applied around the control content area.
     * zh_CN: 控件内容区域周围的边距。
     */
    Q_PROPERTY(QMargins contentMargins READ contentMargins WRITE setContentMargins NOTIFY contentMarginsChanged)
    /**
     * @brief Size of the close button.
     * zh_CN: 关闭按钮尺寸。
     */
    Q_PROPERTY(int closeButtonSize READ closeButtonSize WRITE setCloseButtonSize NOTIFY closeButtonSizeChanged)
    /**
     * @brief Spacing between icon and text content.
     * zh_CN: 图标与文本内容之间的间距。
     */
    Q_PROPERTY(int iconTextSpacing READ iconTextSpacing WRITE setIconTextSpacing NOTIFY iconTextSpacingChanged)
    /**
     * @brief Spacing between title and message text.
     * zh_CN: 标题与消息文本之间的间距。
     */
    Q_PROPERTY(int titleMessageSpacing READ titleMessageSpacing WRITE setTitleMessageSpacing NOTIFY titleMessageSpacingChanged)
    /**
     * @brief Corner radius used by the component background.
     * zh_CN: 组件背景使用的圆角半径。
     */
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY cornerRadiusChanged)
    /**
     * @brief Background size reserved for the severity icon.
     * zh_CN: 严重级别图标背景保留尺寸。
     */
    Q_PROPERTY(int severityIconSize READ severityIconSize WRITE setSeverityIconSize NOTIFY severityIconSizeChanged)
    /**
     * @brief Glyph size used by the severity icon.
     * zh_CN: 严重级别图标字符尺寸。
     */
    Q_PROPERTY(int severityIconGlyphSize READ severityIconGlyphSize WRITE setSeverityIconGlyphSize NOTIFY severityIconGlyphSizeChanged)
    /**
     * @brief Inset applied to severity icon background painting.
     * zh_CN: 严重级别图标背景绘制内缩量。
     */
    Q_PROPERTY(int severityIconBackgroundInset READ severityIconBackgroundInset WRITE setSeverityIconBackgroundInset NOTIFY severityIconBackgroundInsetChanged)
    /**
     * @brief Typography role used by title text.
     * zh_CN: 标题文本使用的排版角色。
     */
    Q_PROPERTY(QString titleFontRole READ titleFontRole WRITE setTitleFontRole NOTIFY titleFontRoleChanged)
    /**
     * @brief Typography role used by message text.
     * zh_CN: 消息文本使用的排版角色。
     */
    Q_PROPERTY(QString messageFontRole READ messageFontRole WRITE setMessageFontRole NOTIFY messageFontRoleChanged)
    /**
     * @brief Icon glyph used for informational severity.
     * zh_CN: 信息级别使用的图标字符。
     */
    Q_PROPERTY(QString informationalIconGlyph READ informationalIconGlyph WRITE setInformationalIconGlyph NOTIFY informationalIconGlyphChanged)
    /**
     * @brief Icon glyph used for success severity.
     * zh_CN: 成功级别使用的图标字符。
     */
    Q_PROPERTY(QString successIconGlyph READ successIconGlyph WRITE setSuccessIconGlyph NOTIFY successIconGlyphChanged)
    /**
     * @brief Icon glyph used for warning severity.
     * zh_CN: 警告级别使用的图标字符。
     */
    Q_PROPERTY(QString warningIconGlyph READ warningIconGlyph WRITE setWarningIconGlyph NOTIFY warningIconGlyphChanged)
    /**
     * @brief Icon glyph used for error severity.
     * zh_CN: 错误级别使用的图标字符。
     */
    Q_PROPERTY(QString errorIconGlyph READ errorIconGlyph WRITE setErrorIconGlyph NOTIFY errorIconGlyphChanged)

public:
    enum InfoBarSeverity { Informational, Success, Warning, Error };
    Q_ENUM(InfoBarSeverity)

    explicit InfoBar(QWidget* parent = nullptr);
    ~InfoBar() override = default;

    bool isOpen() const { return m_isOpen; }
    void setIsOpen(bool open);

    QString title() const { return m_title; }
    void setTitle(const QString& title);

    QString message() const { return m_message; }
    void setMessage(const QString& message);

    InfoBarSeverity severity() const { return m_severity; }
    void setSeverity(InfoBarSeverity severity);

    bool isClosable() const { return m_isClosable; }
    void setIsClosable(bool closable);

    bool isIconVisible() const { return m_isIconVisible; }
    void setIsIconVisible(bool visible);

    bool singleLine() const { return m_singleLine; }
    void setSingleLine(bool singleLine);

    int preferredWidth() const { return m_preferredWidth; }
    void setPreferredWidth(int width);

    int singleLineHeight() const { return m_singleLineHeight; }
    void setSingleLineHeight(int height);

    int multiLineMinHeight() const { return m_multiLineMinHeight; }
    void setMultiLineMinHeight(int height);

    int multiLineActionMinHeight() const { return m_multiLineActionMinHeight; }
    void setMultiLineActionMinHeight(int height);

    QWidget* actionWidget() const { return m_actionWidget; }
    void setActionWidget(QWidget* widget);

    QMargins contentMargins() const { return m_contentMargins; }
    void setContentMargins(const QMargins& margins);

    int closeButtonSize() const { return m_closeButtonSize; }
    void setCloseButtonSize(int size);

    int iconTextSpacing() const { return m_iconTextSpacing; }
    void setIconTextSpacing(int spacing);

    int titleMessageSpacing() const { return m_titleMessageSpacing; }
    void setTitleMessageSpacing(int spacing);

    int cornerRadius() const { return m_cornerRadius; }
    void setCornerRadius(int radius);

    int severityIconSize() const { return m_severityIconSize; }
    void setSeverityIconSize(int size);

    int severityIconGlyphSize() const { return m_severityIconGlyphSize; }
    void setSeverityIconGlyphSize(int size);

    int severityIconBackgroundInset() const { return m_severityIconBackgroundInset; }
    void setSeverityIconBackgroundInset(int inset);

    QString titleFontRole() const { return m_titleFontRole; }
    void setTitleFontRole(const QString& role);

    QString messageFontRole() const { return m_messageFontRole; }
    void setMessageFontRole(const QString& role);

    QString informationalIconGlyph() const { return m_informationalIconGlyph; }
    void setInformationalIconGlyph(const QString& glyph);

    QString successIconGlyph() const { return m_successIconGlyph; }
    void setSuccessIconGlyph(const QString& glyph);

    QString warningIconGlyph() const { return m_warningIconGlyph; }
    void setWarningIconGlyph(const QString& glyph);

    QString errorIconGlyph() const { return m_errorIconGlyph; }
    void setErrorIconGlyph(const QString& glyph);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

signals:
    void isOpenChanged(bool open);
    void titleChanged(const QString& title);
    void messageChanged(const QString& message);
    void severityChanged(InfoBarSeverity severity);
    void isClosableChanged(bool closable);
    void isIconVisibleChanged(bool visible);
    void singleLineChanged(bool singleLine);
    void preferredWidthChanged(int width);
    void singleLineHeightChanged(int height);
    void multiLineMinHeightChanged(int height);
    void multiLineActionMinHeightChanged(int height);
    void actionWidgetChanged(QWidget* widget);
    void contentMarginsChanged(const QMargins& margins);
    void closeButtonSizeChanged(int size);
    void iconTextSpacingChanged(int spacing);
    void titleMessageSpacingChanged(int spacing);
    void cornerRadiusChanged(int radius);
    void severityIconSizeChanged(int size);
    void severityIconGlyphSizeChanged(int size);
    void severityIconBackgroundInsetChanged(int inset);
    void titleFontRoleChanged(const QString& role);
    void messageFontRoleChanged(const QString& role);
    void informationalIconGlyphChanged(const QString& glyph);
    void successIconGlyphChanged(const QString& glyph);
    void warningIconGlyphChanged(const QString& glyph);
    void errorIconGlyphChanged(const QString& glyph);
    void closed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    QRect badgeRect() const;
    int contentLeft() const;
    int contentRight() const;
    int closeButtonX() const;
    int severityIconSlotHeight() const;
    int availableTextWidth() const;
    int measuredMessageHeight(int width) const;
    int actionHeight() const;
    int multiLineContentHeight() const;
    QColor severityBackgroundColor() const;
    QColor severityColor() const;
    QString severityGlyph() const;
    QFont severityIconFont() const;
    void drawSeverityGlyph(QPainter& painter, const QRectF& targetRect) const;
    // Material/macOS: paint the severity glyph tinted with a brand color, sized to fill the icon slot
    // (no circular badge backing). zh_CN: Material/macOS:用品牌色着色绘制严重级别字形,撑满图标槽
    //（无圆形徽标衬底）。
    void drawSeverityGlyphTinted(QPainter& painter, const QRectF& targetRect, const QColor& color) const;
    void initializeChildren();
    void updateChildGeometry();
    void updateLabels();
    void updateThemeColors();
    void updateChildVisibility();
    void updateCloseButtonState();

    bool m_isOpen = true;
    QString m_title = QStringLiteral("Title");
    QString m_message;
    InfoBarSeverity m_severity = Informational;
    bool m_isClosable = true;
    bool m_isIconVisible = true;
    bool m_singleLine = true;
    int m_preferredWidth = 600;
    int m_singleLineHeight = 50;
    int m_multiLineMinHeight = 110;
    int m_multiLineActionMinHeight = 158;
    QMargins m_contentMargins = QMargins(15, 13, 15, 15);
    int m_closeButtonSize = 32;
    int m_iconTextSpacing = 13;
    int m_titleMessageSpacing = 12;
    int m_cornerRadius = 3;
    int m_severityIconSize = 16;
    int m_severityIconGlyphSize = 10;
    int m_severityIconBackgroundInset = 1;

    QWidget* m_actionWidget = nullptr;
    fluent::textfields::Label* m_titleLabel = nullptr;
    fluent::textfields::Label* m_messageLabel = nullptr;
    fluent::basicinput::Button* m_closeButton = nullptr;

    QString m_titleFontRole = Typography::FontRole::BodyStrong;
    QString m_messageFontRole = Typography::FontRole::Body;
    QString m_informationalIconGlyph = Typography::Icons::AsteriskBadge12;
    QString m_successIconGlyph = Typography::Icons::CheckmarkBadge12;
    QString m_warningIconGlyph = Typography::Icons::ImportantBadge12;
    QString m_errorIconGlyph = Typography::Icons::ErrorBadge12;

    QColor m_backgroundColor;
    QColor m_strokeColor;
    QColor m_textColor;
    QColor m_disabledTextColor;
    QColor m_disabledBadgeColor;
    QColor m_badgeForegroundColor;
};

} // namespace fluent::status_info

#endif // INFOBAR_H
