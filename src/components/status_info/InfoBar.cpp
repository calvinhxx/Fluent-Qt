#include "InfoBar.h"
#include "components/status_info/private/StatusPresentationAccessibility_p.h"

#include <QEvent>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QtGlobal>

#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"

namespace fluent::status_info {

namespace {
constexpr int kMinimumWidth = 240;
constexpr int kActionGap = 8;
constexpr int kCloseContentGap = 12;
constexpr int kMultiLineActionGap = 12;
constexpr int kTextLineHeight = 20;
constexpr int kCloseIconSize = Typography::IconSize::Standard;

void setLabelColor(fluent::textfields::Label* label, const QColor& color)
{
    if (!label) return;
    // Color via the label's OWN style sheet rather than its palette: when an InfoBar sits under an
    // ancestor style sheet — e.g. the gallery's GallerySampleCard installs QStyleSheetStyle over its
    // whole subtree — a palette WindowText color is silently dropped, so the title/message rendered
    // near-black in dark theme regardless of severity. A style-sheet color wins over the ancestor
    // style sheet. zh_CN: 用标签自身样式表上色而非 palette：当 InfoBar 位于带样式表的祖先下
    //（如画廊的 GallerySampleCard 会在整个子树安装 QStyleSheetStyle），palette 的 WindowText 会被丢弃，
    // 导致标题/正文在深色主题里无视严重级别渲染成近黑。样式表颜色可越过祖先样式表生效。
    label->setStyleSheet(QStringLiteral("color: rgba(%1, %2, %3, %4); background: transparent;")
                             .arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha()));
}
} // namespace

InfoBar::InfoBar(QWidget* parent)
    : QWidget(parent)
{
    detail::ensureStatusPresentationAccessibilityFactory();
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    initializeChildren();
    updateThemeColors();
    updateLabels();
    updateChildVisibility();
    updateCloseButtonState();
}

InfoBar::~InfoBar()
{
    if (m_actionDestroyedConnection) {
        disconnect(m_actionDestroyedConnection);
    }
}

void InfoBar::setIsOpen(bool open)
{
    if (m_isOpen == open) return;
    m_isOpen = open;
    setHidden(!m_isOpen);
    updateChildVisibility();
    updateGeometry();
    update();
    detail::notifyInfoBarAccessibilityOpenChanged(this);
    emit isOpenChanged(m_isOpen);
}

void InfoBar::setTitle(const QString& title)
{
    if (m_title == title) return;
    m_title = title;
    updateLabels();
    updateGeometry();
    updateChildGeometry();
    detail::notifyInfoBarAccessibilityContentChanged(this);
    emit titleChanged(m_title);
}

void InfoBar::setMessage(const QString& message)
{
    if (m_message == message) return;
    m_message = message;
    updateLabels();
    updateGeometry();
    updateChildGeometry();
    detail::notifyInfoBarAccessibilityContentChanged(this);
    emit messageChanged(m_message);
}

void InfoBar::setSeverity(InfoBarSeverity severity)
{
    if (m_severity == severity) return;
    m_severity = severity;
    updateThemeColors();
    update();
    detail::notifyInfoBarAccessibilitySeverityChanged(this);
    emit severityChanged(m_severity);
}

void InfoBar::setIsClosable(bool closable)
{
    if (m_isClosable == closable) return;
    m_isClosable = closable;
    updateChildVisibility();
    updateCloseButtonState();
    updateGeometry();
    updateChildGeometry();
    detail::notifyInfoBarAccessibilityDismissChanged(this);
    emit isClosableChanged(m_isClosable);
}

void InfoBar::setIsIconVisible(bool visible)
{
    if (m_isIconVisible == visible) return;
    m_isIconVisible = visible;
    updateChildVisibility();
    updateGeometry();
    updateChildGeometry();
    update();
    emit isIconVisibleChanged(m_isIconVisible);
}

void InfoBar::setSingleLine(bool singleLine)
{
    if (m_singleLine == singleLine) return;
    m_singleLine = singleLine;
    if (m_messageLabel) {
        m_messageLabel->setWordWrap(!m_singleLine);
    }
    updateLabels();
    updateGeometry();
    updateChildGeometry();
    emit singleLineChanged(m_singleLine);
}

void InfoBar::setPreferredWidth(int width)
{
    if (width <= 0 || m_preferredWidth == width) return;
    m_preferredWidth = width;
    updateGeometry();
    updateChildGeometry();
    emit preferredWidthChanged(m_preferredWidth);
}

void InfoBar::setSingleLineHeight(int height)
{
    if (height <= 0 || m_singleLineHeight == height) return;
    m_singleLineHeight = height;
    updateGeometry();
    updateChildGeometry();
    emit singleLineHeightChanged(m_singleLineHeight);
}

void InfoBar::setMultiLineMinHeight(int height)
{
    if (height <= 0 || m_multiLineMinHeight == height) return;
    m_multiLineMinHeight = height;
    updateGeometry();
    updateChildGeometry();
    emit multiLineMinHeightChanged(m_multiLineMinHeight);
}

void InfoBar::setMultiLineActionMinHeight(int height)
{
    if (height <= 0 || m_multiLineActionMinHeight == height) return;
    m_multiLineActionMinHeight = height;
    updateGeometry();
    updateChildGeometry();
    emit multiLineActionMinHeightChanged(m_multiLineActionMinHeight);
}

void InfoBar::setActionWidget(QWidget* widget)
{
    if (m_actionWidget == widget) return;

    if (m_actionDestroyedConnection) {
        disconnect(m_actionDestroyedConnection);
        m_actionDestroyedConnection = {};
    }

    if (m_actionWidget) {
        m_actionWidget->hide();
        m_actionWidget->setParent(nullptr);
    }

    m_actionWidget = widget;
    if (m_actionWidget) {
        m_actionWidget->setParent(this);
        m_actionWidget->setObjectName(m_actionWidget->objectName().isEmpty()
            ? QStringLiteral("InfoBarActionWidget")
            : m_actionWidget->objectName());
        m_actionDestroyedConnection = connect(
            m_actionWidget,
            &QObject::destroyed,
            this,
            [this]() {
                m_actionDestroyedConnection = {};
                m_actionWidget = nullptr;
                updateChildVisibility();
                updateGeometry();
                updateChildGeometry();
                detail::notifyInfoBarAccessibilityStructureChanged(
                    this);
                emit actionWidgetChanged(nullptr);
            });
    }

    updateChildVisibility();
    updateGeometry();
    updateChildGeometry();
    detail::notifyInfoBarAccessibilityStructureChanged(this);
    emit actionWidgetChanged(m_actionWidget);
}

void InfoBar::setContentMargins(const QMargins& margins)
{
    if (m_contentMargins == margins) return;
    m_contentMargins = margins;
    updateGeometry();
    updateChildGeometry();
    update();
    emit contentMarginsChanged(m_contentMargins);
}

void InfoBar::setCloseButtonSize(int size)
{
    if (size <= 0 || m_closeButtonSize == size) return;
    m_closeButtonSize = size;
    if (m_closeButton) m_closeButton->setFixedSize(m_closeButtonSize, m_closeButtonSize);
    updateGeometry();
    updateChildGeometry();
    emit closeButtonSizeChanged(m_closeButtonSize);
}

void InfoBar::setCloseButtonAccessibleName(const QString& name)
{
    if (m_closeButtonAccessibleName == name)
        return;
    m_closeButtonAccessibleName = name;
    if (m_closeButton)
        m_closeButton->setAccessibleName(
            m_closeButtonAccessibleName.isEmpty()
                ? detail::infoBarDismissAccessibleName()
                : m_closeButtonAccessibleName);
    emit closeButtonAccessibleNameChanged(m_closeButtonAccessibleName);
}

void InfoBar::setIconTextSpacing(int spacing)
{
    const int normalizedSpacing = qMax(0, spacing);
    if (m_iconTextSpacing == normalizedSpacing) return;
    m_iconTextSpacing = normalizedSpacing;
    updateGeometry();
    updateChildGeometry();
    emit iconTextSpacingChanged(m_iconTextSpacing);
}

void InfoBar::setTitleMessageSpacing(int spacing)
{
    const int normalizedSpacing = qMax(0, spacing);
    if (m_titleMessageSpacing == normalizedSpacing) return;
    m_titleMessageSpacing = normalizedSpacing;
    updateGeometry();
    updateChildGeometry();
    emit titleMessageSpacingChanged(m_titleMessageSpacing);
}

void InfoBar::setCornerRadius(int radius)
{
    const int normalizedRadius = qMax(0, radius);
    if (m_cornerRadius == normalizedRadius) return;
    m_cornerRadius = normalizedRadius;
    update();
    emit cornerRadiusChanged(m_cornerRadius);
}

void InfoBar::setSeverityIconSize(int size)
{
    if (size <= 0 || m_severityIconSize == size) return;
    m_severityIconSize = size;
    updateGeometry();
    updateChildGeometry();
    update();
    emit severityIconSizeChanged(m_severityIconSize);
}

void InfoBar::setSeverityIconGlyphSize(int size)
{
    if (size <= 0 || m_severityIconGlyphSize == size) return;
    m_severityIconGlyphSize = size;
    update();
    emit severityIconGlyphSizeChanged(m_severityIconGlyphSize);
}

void InfoBar::setSeverityIconBackgroundInset(int inset)
{
    const int normalizedInset = qMax(0, inset);
    if (m_severityIconBackgroundInset == normalizedInset) return;
    m_severityIconBackgroundInset = normalizedInset;
    update();
    emit severityIconBackgroundInsetChanged(m_severityIconBackgroundInset);
}

void InfoBar::setTitleFontRole(Typography::FontRole role)
{
    if (m_titleFontRole == role) return;
    m_titleFontRole = role;
    updateLabels();
    updateGeometry();
    updateChildGeometry();
    emit titleFontRoleChanged(m_titleFontRole);
}

void InfoBar::setMessageFontRole(Typography::FontRole role)
{
    if (m_messageFontRole == role) return;
    m_messageFontRole = role;
    updateLabels();
    updateGeometry();
    updateChildGeometry();
    emit messageFontRoleChanged(m_messageFontRole);
}

void InfoBar::setInformationalIconGlyph(const QString& glyph)
{
    if (m_informationalIconGlyph == glyph) return;
    m_informationalIconGlyph = glyph;
    update();
    emit informationalIconGlyphChanged(m_informationalIconGlyph);
}

void InfoBar::setSuccessIconGlyph(const QString& glyph)
{
    if (m_successIconGlyph == glyph) return;
    m_successIconGlyph = glyph;
    update();
    emit successIconGlyphChanged(m_successIconGlyph);
}

void InfoBar::setWarningIconGlyph(const QString& glyph)
{
    if (m_warningIconGlyph == glyph) return;
    m_warningIconGlyph = glyph;
    update();
    emit warningIconGlyphChanged(m_warningIconGlyph);
}

void InfoBar::setErrorIconGlyph(const QString& glyph)
{
    if (m_errorIconGlyph == glyph) return;
    m_errorIconGlyph = glyph;
    update();
    emit errorIconGlyphChanged(m_errorIconGlyph);
}

QSize InfoBar::sizeHint() const
{
    if (!m_isOpen) return QSize(0, 0);
    if (m_singleLine) return QSize(m_preferredWidth, m_singleLineHeight);
    return QSize(m_preferredWidth, multiLineContentHeight());
}

QSize InfoBar::minimumSizeHint() const
{
    if (!m_isOpen) return QSize(0, 0);
    return QSize(qMin(kMinimumWidth, m_preferredWidth), m_singleLineHeight);
}

void InfoBar::onThemeUpdated()
{
    updateThemeColors();
    updateLabels();
    updateCloseButtonState();
    if (m_actionWidget) m_actionWidget->update();
    update();
}

void InfoBar::paintEvent(QPaintEvent*)
{
    if (!m_isOpen) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
        const QRectF frameRect = rect().adjusted(0, 0, -1, -1);
        painter.setPen(QPen(m_strokeColor, 1.0));
        painter.setBrush(m_backgroundColor);
        painter.drawRoundedRect(frameRect, m_cornerRadius, m_cornerRadius);

        if (!m_isIconVisible) return;

        const QRectF badge = badgeRect();
        const QRectF badgeBase = badge.adjusted(
            m_severityIconBackgroundInset,
            m_severityIconBackgroundInset,
            -m_severityIconBackgroundInset,
            -m_severityIconBackgroundInset);
        painter.setPen(Qt::NoPen);
        painter.setBrush(isEnabled() ? severityColor() : m_disabledBadgeColor);
        painter.drawEllipse(badgeBase);

        drawSeverityGlyph(painter, badgeBase);
}

void InfoBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateChildGeometry();
}

void InfoBar::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange) {
        updateLabels();
        updateCloseButtonState();
        update();
        detail::notifyInfoBarAccessibilityEnabledChanged(this);
    }
}

QRect InfoBar::badgeRect() const
{
    const int y = m_singleLine
        ? (height() - severityIconSlotHeight()) / 2 + (severityIconSlotHeight() - m_severityIconSize) / 2
        : m_contentMargins.top() + (severityIconSlotHeight() - m_severityIconSize) / 2;
    return QRect(m_contentMargins.left(), y, m_severityIconSize, m_severityIconSize);
}

int InfoBar::contentLeft() const
{
    int left = m_contentMargins.left();
    if (m_isIconVisible) {
        left += m_severityIconSize + m_iconTextSpacing;
    }
    return left;
}

int InfoBar::contentRight() const
{
    if (m_isClosable) return qMax(0, closeButtonX() - kCloseContentGap);
    return qMax(0, width() - m_contentMargins.right());
}

int InfoBar::closeButtonX() const
{
    return qMax(0, width() - m_contentMargins.right() - m_closeButtonSize);
}

int InfoBar::severityIconSlotHeight() const
{
    return qMax(kTextLineHeight, m_severityIconSize);
}

int InfoBar::availableTextWidth() const
{
    return qMax(0, contentRight() - contentLeft());
}

int InfoBar::measuredMessageHeight(int width) const
{
    if (m_message.isEmpty()) return 0;
    QFontMetrics metrics(themeFont(m_messageFontRole).toQFont());
    if (m_singleLine) return kTextLineHeight;

    const QRect bounds = metrics.boundingRect(
        QRect(0, 0, qMax(1, width), 10000),
        Qt::TextWordWrap,
        m_message);
    return qMax(kTextLineHeight, bounds.height());
}

int InfoBar::actionHeight() const
{
    if (!m_actionWidget) return 0;
    return qMax(0, m_actionWidget->sizeHint().height());
}

int InfoBar::multiLineContentHeight() const
{
    const int textWidth = qMax(1, m_preferredWidth
        - m_contentMargins.left()
        - (m_isIconVisible ? m_severityIconSize + m_iconTextSpacing : 0)
        - (m_isClosable ? m_closeButtonSize + kCloseContentGap : m_contentMargins.right())
        - m_contentMargins.right());
    const int messageHeight = measuredMessageHeight(textWidth);
    int height = m_contentMargins.top() + kTextLineHeight + messageHeight + m_contentMargins.bottom();
    if (m_actionWidget) {
        height += kMultiLineActionGap + actionHeight();
        return qMax(height, m_multiLineActionMinHeight);
    }
    return qMax(height, m_multiLineMinHeight);
}

QColor InfoBar::severityBackgroundColor() const
{
    const auto& colors = themeColorsRef();
    switch (m_severity) {
        case Success: return colors.systemSuccessBg;
        case Warning: return colors.systemCautionBg;
        case Error: return colors.systemCriticalBg;
        case Informational:
        default: return colors.systemInfoBg;
    }
}

QColor InfoBar::severityColor() const
{
    const auto& colors = themeColorsRef();
    switch (m_severity) {
        case Success: return colors.systemSuccess;
        case Warning: return colors.systemCaution;
        case Error: return colors.systemCritical;
        case Informational:
        default: return colors.accentDefault;
    }
}

QString InfoBar::severityGlyph() const
{
    switch (m_severity) {
        case Success: return m_successIconGlyph;
        case Warning: return m_warningIconGlyph;
        case Error: return m_errorIconGlyph;
        case Informational:
        default: return m_informationalIconGlyph;
    }
}

QFont InfoBar::severityIconFont() const
{
    return Typography::Icons::font(m_severityIconGlyphSize);
}

void InfoBar::drawSeverityGlyph(QPainter& painter, const QRectF& targetRect) const
{
    const QString glyph = severityGlyph();
    if (glyph.isEmpty() || targetRect.isEmpty()) return;

    // Badge12 semantic aliases historically pointed at 16 px *circle* drawings
    // (checkmark_circle / error_circle). Those read as a second ring inside the
    // filled Fluent badge. Resolve a compact optical variant and paint with
    // drawText so DirectWrite hinting stays active.
    // zh_CN: Badge12 语义别名曾指向 16 px「带圈」字形（checkmark_circle / error_circle），
    // 画在填充徽标圆内会像第二层环。解析紧凑光学变体并用 drawText 绘制，保留 DirectWrite hinting。
    painter.save();
    painter.setPen(isEnabled() ? m_badgeForegroundColor : m_disabledTextColor);
    painter.setBrush(Qt::NoBrush);
    Typography::Icons::paintGlyph(
        painter, targetRect, glyph, m_severityIconGlyphSize, Qt::AlignCenter);
    painter.restore();
}

void InfoBar::initializeChildren()
{
    m_titleLabel = new fluent::textfields::Label(this);
    m_titleLabel->setObjectName(QStringLiteral("InfoBarTitleLabel"));
    m_titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_messageLabel = new fluent::textfields::Label(this);
    m_messageLabel->setObjectName(QStringLiteral("InfoBarMessageLabel"));
    m_messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_messageLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_messageLabel->setTextElideMode(Qt::ElideRight);
    m_messageLabel->setWordWrap(false);

    m_closeButton = new fluent::basicinput::Button(this);
    m_closeButton->setObjectName(QStringLiteral("InfoBarCloseButton"));
    m_closeButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_closeButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_closeButton->setIconGlyph(Typography::Icons::ChromeClose, kCloseIconSize);
    m_closeButton->setFixedSize(m_closeButtonSize, m_closeButtonSize);
    m_closeButton->setAccessibleName(
        m_closeButtonAccessibleName.isEmpty()
            ? detail::infoBarDismissAccessibleName()
            : m_closeButtonAccessibleName);
    connect(m_closeButton, &QPushButton::clicked, this, [this]() {
        if (!m_isClosable || !isEnabled()) return;
        QPointer<InfoBar> guard(this);
        setIsOpen(false);
        if (guard && !guard->isOpen())
            emit guard->closed();
    });
}

void InfoBar::updateChildGeometry()
{
    if (!m_isOpen || width() <= 0 || height() <= 0) return;

    if (m_closeButton) {
        const int closeY = m_singleLine
            ? (height() - m_closeButtonSize) / 2
            : qMax(0, m_contentMargins.top() - (m_closeButtonSize - severityIconSlotHeight()) / 2);
        m_closeButton->setGeometry(closeButtonX(), closeY, m_closeButtonSize, m_closeButtonSize);
    }

    const int left = contentLeft();
    const int right = contentRight();
    const int contentWidth = qMax(0, right - left);

    if (m_singleLine) {
        const int textY = (height() - kTextLineHeight) / 2;
        QFontMetrics titleMetrics(themeFont(m_titleFontRole).toQFont());
        const int desiredTitleWidth = m_title.isEmpty() ? 0 : titleMetrics.horizontalAdvance(m_title);
        const int titleWidth = qMin(desiredTitleWidth, contentWidth);
        m_titleLabel->setGeometry(left, textY, titleWidth, kTextLineHeight);

        int messageLeft = left + titleWidth;
        if (titleWidth > 0 && !m_message.isEmpty()) messageLeft += m_titleMessageSpacing;

        int actionWidth = 0;
        int actionWidgetHeight = 0;
        if (m_actionWidget) {
            const QSize actionSize = m_actionWidget->sizeHint();
            actionWidth = qMax(0, actionSize.width());
            actionWidgetHeight = qMax(kTextLineHeight, actionSize.height());
        }

        int messageRight = right;
        if (m_actionWidget && actionWidth > 0) {
            const int actionX = qMax(messageLeft, right - actionWidth);
            const int actionY = (height() - actionWidgetHeight) / 2;
            m_actionWidget->setGeometry(actionX, actionY, actionWidth, actionWidgetHeight);
            messageRight = actionX - kActionGap;
        }

        const int messageWidth = qMax(0, messageRight - messageLeft);
        m_messageLabel->setGeometry(messageLeft, textY, messageWidth, kTextLineHeight);
        m_messageLabel->setText(m_message);
        return;
    }

    m_titleLabel->setGeometry(left, m_contentMargins.top(), contentWidth, kTextLineHeight);
    m_messageLabel->setTextElideMode(Qt::ElideNone);
    m_messageLabel->setText(m_message);
    m_messageLabel->setWordWrap(true);
    const int messageHeight = measuredMessageHeight(contentWidth);
    m_messageLabel->setGeometry(left, m_contentMargins.top() + kTextLineHeight, contentWidth, messageHeight);

    if (m_actionWidget) {
        const QSize actionSize = m_actionWidget->sizeHint();
        const int actionWidth = qMin(actionSize.width(), contentWidth);
        const int widgetHeight = qMax(kTextLineHeight, actionSize.height());
        const int actionY = m_contentMargins.top() + kTextLineHeight + messageHeight + kMultiLineActionGap;
        m_actionWidget->setGeometry(left, actionY, actionWidth, widgetHeight);
    }
}

void InfoBar::updateLabels()
{
    if (!m_titleLabel || !m_messageLabel) return;

    m_titleLabel->setFont(themeFont(m_titleFontRole).toQFont());
    m_messageLabel->setFont(themeFont(m_messageFontRole).toQFont());

    const QColor text = isEnabled() ? m_textColor : m_disabledTextColor;
    setLabelColor(m_titleLabel, text);
    setLabelColor(m_messageLabel, text);

    m_titleLabel->setText(m_title);
    if (m_singleLine) {
        m_messageLabel->setTextElideMode(Qt::ElideRight);
        m_messageLabel->setText(m_message);
        m_messageLabel->setWordWrap(false);
    } else {
        m_messageLabel->setTextElideMode(Qt::ElideNone);
        m_messageLabel->setText(m_message);
        m_messageLabel->setWordWrap(true);
    }
}

void InfoBar::updateThemeColors()
{
    const auto& colors = themeColorsRef();
    m_backgroundColor = severityBackgroundColor();
    m_strokeColor = colors.strokeCard;
    if (m_strokeColor.alpha() < 15) m_strokeColor.setAlpha(15);
    m_textColor = colors.textPrimary;
    m_disabledTextColor = colors.textDisabled;
    m_disabledBadgeColor = colors.accentDisabled;
    m_badgeForegroundColor = colors.textOnAccent;
}

void InfoBar::updateChildVisibility()
{
    if (m_titleLabel) m_titleLabel->setVisible(m_isOpen);
    if (m_messageLabel) m_messageLabel->setVisible(m_isOpen);
    if (m_closeButton) m_closeButton->setVisible(m_isOpen && m_isClosable);
    if (m_actionWidget) m_actionWidget->setVisible(m_isOpen);
}

void InfoBar::updateCloseButtonState()
{
    if (!m_closeButton) return;
    m_closeButton->setEnabled(isEnabled() && m_isClosable);
    m_closeButton->update();
}

} // namespace fluent::status_info
