#include "InfoBar.h"
#include "components/foundation/private/SurfacePainter_p.h"

#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
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
}

InfoBar::InfoBar(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    setAccessibleName(QStringLiteral("InfoBar"));
    initializeChildren();
    updateThemeColors();
    updateLabels();
    updateChildVisibility();
    updateCloseButtonState();
}

void InfoBar::setIsOpen(bool open)
{
    if (m_isOpen == open) return;
    m_isOpen = open;
    setHidden(!m_isOpen);
    updateChildVisibility();
    updateGeometry();
    update();
    emit isOpenChanged(m_isOpen);
}

void InfoBar::setTitle(const QString& title)
{
    if (m_title == title) return;
    m_title = title;
    updateLabels();
    updateGeometry();
    updateChildGeometry();
    emit titleChanged(m_title);
}

void InfoBar::setMessage(const QString& message)
{
    if (m_message == message) return;
    m_message = message;
    updateLabels();
    updateGeometry();
    updateChildGeometry();
    emit messageChanged(m_message);
}

void InfoBar::setSeverity(InfoBarSeverity severity)
{
    if (m_severity == severity) return;
    m_severity = severity;
    updateThemeColors();
    update();
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
    }

    updateChildVisibility();
    updateGeometry();
    updateChildGeometry();
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

void InfoBar::setTitleFontRole(const QString& role)
{
    if (role.isEmpty() || m_titleFontRole == role) return;
    m_titleFontRole = role;
    updateLabels();
    updateGeometry();
    updateChildGeometry();
    emit titleFontRoleChanged(m_titleFontRole);
}

void InfoBar::setMessageFontRole(const QString& role)
{
    if (role.isEmpty() || m_messageFontRole == role) return;
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

    // Per-design-language container treatment. DesignFluent is the original WinUI rendering and MUST stay
    // byte-for-byte unchanged: a rounded container (m_cornerRadius) with the tonal severity background +
    // strokeCard hairline, then a filled circular severity badge with a white glyph. Material/macOS drop
    // the filled badge in favor of a tint-on-glyph idiom over their own container shapes.
    // zh_CN: 按设计语言区分容器处理。DesignFluent 为原始 WinUI 渲染,必须逐字节保持不变:圆角容器
    //（m_cornerRadius）+ 色调严重级别背景 + strokeCard 发丝描边,再绘制填充圆形徽标 + 白色字形。
    // Material/macOS 放弃填充徽标,改用各自容器形状上的「字形着色」范式。
    const DesignLanguage lang = themeDesignLanguage();

    if (lang == DesignFluent) {
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
        return;
    }

    // ─── Material 3 / macOS shared resolution ──────────────────────────────────
    const auto colors = themeColors();
    // Guard every optional fill against the invalid-QColor trap: a default-constructed QColor is INVALID
    // yet alpha()==255, so setBrush(invalidColor) paints SOLID OPAQUE BLACK. Severity tonal backgrounds
    // come from the seeded tables and are valid, but we still verify before painting. zh_CN: 用 isValid()
    // 防御无效 QColor 陷阱(默认构造的 QColor 无效却 alpha==255,setBrush 会涂成不透明纯黑)。
    QColor fill = severityBackgroundColor();
    if (!fill.isValid()) fill = Qt::transparent;
    // Brand icon tint uses the semantic systemXxx foreground (not severityColor(), which substitutes the
    // app accent for Informational); fall back to textPrimary if the token is unset. zh_CN: 品牌图标着色用
    // 语义 systemXxx 前景色(而非 severityColor()——其对 Informational 用 app 强调色替代);token 缺省时回退 textPrimary。
    QColor tint;
    switch (m_severity) {
        case Success: tint = colors.systemSuccess; break;
        case Warning: tint = colors.systemCaution; break;
        case Error: tint = colors.systemCritical; break;
        case Informational:
        default: tint = colors.systemInfo; break;
    }
    if (!tint.isValid()) tint = colors.textPrimary;

    qreal radius = 12.0;
    QColor border = Qt::transparent;

    if (lang == DesignMaterial) {
        // Material 3 (≈ Snackbar/Banner): a rounded tonal container at radius ~12 carries the severity
        // through its systemXxxBg fill; no left accent badge, and the border is suppressed because the
        // tonal fill already reads the severity. The leading icon is tinted with the severity color.
        // zh_CN: Material 3:radius~12 的色调容器以 systemXxxBg 填充承载严重级别;无左侧徽标,描边抑制
        //（色调填充已表达严重级别）。前导图标用严重级别色着色。
        const int overlayRadius = themeRadius().overlay;
        radius = (overlayRadius > 0) ? qMax(overlayRadius, 12) : 12.0;
        border = Qt::transparent;
    } else { // DesignCupertino
        // macOS: a quiet bezel — rounded container at radius ~10 with a subtle severity fill and a
        // hairline strokeStrong border; icon tinted with the severity color. No loud full-width band,
        // no left accent strip. zh_CN: macOS:安静的 bezel——radius~10 圆角容器 + 轻微严重级别填充 +
        // strokeStrong 发丝描边;图标用严重级别色着色。无整宽强调带,无左侧强调条。
        radius = (themeRadius().control > 0) ? qMax(themeRadius().control, 10) : 10.0;
        border = colors.strokeStrong;
    }

    fluent::painting::RoundedSurfacePaint surface;
    surface.fill = fill;
    surface.border = border;
    surface.radius = radius;
    fluent::painting::paintRoundedSurface(painter, QRectF(rect()), surface);

    if (!m_isIconVisible) return;

    // Tint the leading icon glyph (no filled circular badge). zh_CN: 着色前导图标字形(无填充圆形徽标)。
    const QColor glyphColor = isEnabled() ? tint : m_disabledTextColor;
    drawSeverityGlyphTinted(painter, badgeRect(), glyphColor);
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
    const auto colors = themeColors();
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
    const auto colors = themeColors();
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
    QFont font(Typography::FontFamily::FluentIcons);
    font.setPixelSize(m_severityIconGlyphSize);
    return font;
}

void InfoBar::drawSeverityGlyph(QPainter& painter, const QRectF& targetRect) const
{
    const QString glyph = severityGlyph();
    if (glyph.isEmpty() || targetRect.isEmpty()) return;

    QPainterPath glyphPath;
    glyphPath.addText(QPointF(0, 0), severityIconFont(), glyph);
    const QRectF glyphBounds = glyphPath.boundingRect();
    if (glyphBounds.isEmpty()) return;

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(isEnabled() ? m_badgeForegroundColor : m_disabledTextColor);
    painter.translate(
        targetRect.center().x() - glyphBounds.center().x(),
        targetRect.center().y() - glyphBounds.center().y());
    painter.drawPath(glyphPath);
    painter.restore();
}

void InfoBar::drawSeverityGlyphTinted(QPainter& painter, const QRectF& targetRect, const QColor& color) const
{
    const QString glyph = severityGlyph();
    if (glyph.isEmpty() || targetRect.isEmpty() || !color.isValid()) return;

    // The Fluent badge glyph is intentionally tiny (m_severityIconGlyphSize ~10) because it sits inside a
    // filled circle. Without that circle the tinted glyph should fill the icon slot, so scale to the slot
    // height. zh_CN: Fluent 徽标字形刻意很小（~10）因为它在填充圆内。去掉圆后,着色字形应撑满图标槽,
    // 故按槽高缩放。
    QFont font(Typography::FontFamily::FluentIcons);
    font.setPixelSize(qMax(1, qRound(targetRect.height())));

    QPainterPath glyphPath;
    glyphPath.addText(QPointF(0, 0), font, glyph);
    const QRectF glyphBounds = glyphPath.boundingRect();
    if (glyphBounds.isEmpty()) return;

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.translate(
        targetRect.center().x() - glyphBounds.center().x(),
        targetRect.center().y() - glyphBounds.center().y());
    painter.drawPath(glyphPath);
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
    m_closeButton->setAccessibleName(QStringLiteral("Close InfoBar"));
    connect(m_closeButton, &QPushButton::clicked, this, [this]() {
        if (!m_isClosable || !isEnabled()) return;
        setIsOpen(false);
        emit closed();
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
    const auto colors = themeColors();
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
