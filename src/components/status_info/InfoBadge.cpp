#include "InfoBadge.h"

#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

#include "design/Typography.h"

namespace fluent::status_info {

namespace {
constexpr qreal kOpacityEpsilon = 0.001;

bool nearlyEqual(qreal left, qreal right)
{
    return std::abs(left - right) < kOpacityEpsilon;
}
}

InfoBadge::InfoBadge(QWidget* parent)
    : QWidget(parent)
    , m_valueFontRole(Typography::FontRole::Caption)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAccessibleName(QStringLiteral("InfoBadge"));
    updateThemeColors();
}

void InfoBadge::setValue(int value)
{
    const int normalizedValue = std::max(-1, value);
    if (m_value == normalizedValue) return;
    m_value = normalizedValue;
    invalidateLayoutAndPaint();
    emit valueChanged(m_value);
}

void InfoBadge::setIconGlyph(const QString& glyph)
{
    if (m_iconGlyph == glyph) return;
    m_iconGlyph = glyph;
    invalidateLayoutAndPaint();
    emit iconGlyphChanged(m_iconGlyph);
}

void InfoBadge::setDisplayMode(InfoBadgeDisplayMode mode)
{
    if (m_displayMode == mode) return;
    m_displayMode = mode;
    invalidateLayoutAndPaint();
    emit displayModeChanged(m_displayMode);
}

void InfoBadge::setStatus(InfoBadgeStatus status)
{
    if (m_status == status) return;
    m_status = status;
    updateThemeColors();
    update();
    emit statusChanged(m_status);
}

void InfoBadge::setBadgeOpacity(qreal opacity)
{
    const qreal normalizedOpacity = qBound<qreal>(0.0, opacity, 1.0);
    if (nearlyEqual(m_badgeOpacity, normalizedOpacity)) return;
    m_badgeOpacity = normalizedOpacity;
    update();
    emit badgeOpacityChanged(m_badgeOpacity);
}

void InfoBadge::setCustomBackgroundColor(const QColor& color)
{
    if (m_customBackgroundColor == color) return;
    m_customBackgroundColor = color;
    updateThemeColors();
    update();
    emit customBackgroundColorChanged(m_customBackgroundColor);
}

void InfoBadge::setCustomTextColor(const QColor& color)
{
    if (m_customTextColor == color) return;
    m_customTextColor = color;
    updateThemeColors();
    update();
    emit customTextColorChanged(m_customTextColor);
}

void InfoBadge::setValueFontRole(const QString& role)
{
    if (role.isEmpty() || m_valueFontRole == role) return;
    m_valueFontRole = role;
    invalidateLayoutAndPaint();
    emit valueFontRoleChanged(m_valueFontRole);
}

void InfoBadge::setBeaconDiameter(int diameter)
{
    if (diameter <= 0 || m_beaconDiameter == diameter) return;
    m_beaconDiameter = diameter;
    invalidateLayoutAndPaint();
    emit beaconDiameterChanged(m_beaconDiameter);
}

void InfoBadge::setBadgeHeight(int height)
{
    if (height <= 0 || m_badgeHeight == height) return;
    m_badgeHeight = height;
    invalidateLayoutAndPaint();
    emit badgeHeightChanged(m_badgeHeight);
}

void InfoBadge::setValueHorizontalPadding(int padding)
{
    const int normalizedPadding = qMax(0, padding);
    if (m_valueHorizontalPadding == normalizedPadding) return;
    m_valueHorizontalPadding = normalizedPadding;
    invalidateLayoutAndPaint();
    emit valueHorizontalPaddingChanged(m_valueHorizontalPadding);
}

void InfoBadge::setIconGlyphSize(int size)
{
    if (size <= 0 || m_iconGlyphSize == size) return;
    m_iconGlyphSize = size;
    update();
    emit iconGlyphSizeChanged(m_iconGlyphSize);
}

void InfoBadge::setIconFontFamily(const QString& family)
{
    if (family.isEmpty() || m_iconFontFamily == family) return;
    m_iconFontFamily = family;
    update();
    emit iconFontFamilyChanged(m_iconFontFamily);
}

void InfoBadge::setBadgeBackgroundInset(int inset)
{
    const int normalizedInset = qMax(0, inset);
    if (m_badgeBackgroundInset == normalizedInset) return;
    m_badgeBackgroundInset = normalizedInset;
    update();
    emit badgeBackgroundInsetChanged(m_badgeBackgroundInset);
}

void InfoBadge::setContentOffset(const QPoint& offset)
{
    if (m_contentOffset == offset) return;
    m_contentOffset = offset;
    update();
    emit contentOffsetChanged(m_contentOffset);
}

InfoBadge::InfoBadgeDisplayMode InfoBadge::effectiveDisplayMode() const
{
    if (m_displayMode != InfoBadgeDisplayMode::Auto) return m_displayMode;
    if (m_value >= 0) return InfoBadgeDisplayMode::Value;
    if (!m_iconGlyph.isEmpty()) return InfoBadgeDisplayMode::Icon;
    return InfoBadgeDisplayMode::Dot;
}

QSize InfoBadge::effectiveBadgeSize() const
{
    switch (effectiveDisplayMode()) {
        case InfoBadgeDisplayMode::Dot:
            return QSize(m_beaconDiameter, m_beaconDiameter);
        case InfoBadgeDisplayMode::Icon:
            return QSize(m_badgeHeight, m_badgeHeight);
        case InfoBadgeDisplayMode::Value: {
            const QString text = valueText();
            if (text.isEmpty()) return QSize(m_badgeHeight, m_badgeHeight);
            const QFontMetrics metrics(valueFont());
            const int textWidth = metrics.horizontalAdvance(text);
            const int width = qMax(m_badgeHeight, textWidth + m_valueHorizontalPadding);
            return QSize(width, m_badgeHeight);
        }
        case InfoBadgeDisplayMode::Auto:
        default:
            return QSize(m_beaconDiameter, m_beaconDiameter);
    }
}

QColor InfoBadge::effectiveBackgroundColor() const
{
    if (!isEnabled()) return m_disabledBackgroundColor;
    return m_backgroundColor;
}

QColor InfoBadge::effectiveForegroundColor() const
{
    if (!isEnabled()) return m_disabledForegroundColor;
    return m_foregroundColor;
}

QSize InfoBadge::sizeHint() const
{
    return effectiveBadgeSize();
}

QSize InfoBadge::minimumSizeHint() const
{
    return sizeHint();
}

void InfoBadge::onThemeUpdated()
{
    updateThemeColors();
    updateGeometry();
    update();
}

void InfoBadge::paintEvent(QPaintEvent*)
{
    const QSize badgeSize = effectiveBadgeSize();
    if (badgeSize.isEmpty() || width() <= 0 || height() <= 0 || m_badgeOpacity <= 0.0) return;

    QRectF badgeRect(
        (width() - badgeSize.width()) / 2.0,
        (height() - badgeSize.height()) / 2.0,
        badgeSize.width(),
        badgeSize.height());
    badgeRect = badgeRect.intersected(QRectF(rect()));
    if (!badgeRect.isValid() || badgeRect.isEmpty()) return;

    // Design-language branch. DesignFluent stays byte-for-byte unchanged (uses the cached
    // effectiveBackground/ForegroundColor exactly as before). Material 3 and macOS adopt the same
    // fully-rounded badge that Fluent already paints (circle for the dot, pill = height/2 radius for
    // numbered/value), but force WHITE on-color text and read RED (systemCritical) by default on macOS
    // for the Attention status (the canonical macOS notification/dock badge is system red).
    // zh_CN: 设计语言分支。DesignFluent 逐字节保持不变(沿用缓存的 effectiveBackground/ForegroundColor)。
    // Material 3 与 macOS 复用 Fluent 已绘制的全圆角徽标(提示点为圆形,数值徽标为 height/2 胶囊),但强制
    // 白色 on-color 文字,且 macOS 在 Attention 状态下默认读作红色(systemCritical,即 macOS 通知/程序坞徽标)。
    const DesignLanguage lang = themeDesignLanguage();
    QColor fillColor = effectiveBackgroundColor();
    QColor textColor = effectiveForegroundColor();
    if (lang != DesignFluent && isEnabled()) {
        const auto& colors = themeColors();
        // Background: honor a custom override first; otherwise the per-state semantic color, with macOS
        // defaulting the neutral Attention state to system red. zh_CN: 背景:优先自定义覆盖;否则按状态语义
        // 取色,macOS 在中性 Attention 状态下默认系统红。
        if (m_customBackgroundColor.isValid()) {
            fillColor = m_customBackgroundColor;
        } else if (lang == DesignCupertino && m_status == InfoBadgeStatus::Attention) {
            fillColor = colors.systemCritical;
        } else {
            fillColor = statusBackgroundColor();
        }
        // Foreground: guaranteed white on the colored fill (M3 + macOS badges are white-on-color),
        // unless the caller set a custom text color. Init to a real color so the invalid-QColor trap
        // (alpha()==255 on an unassigned color → opaque black) can never fire. zh_CN: 前景:彩色填充上
        // 保证白色(M3/macOS 徽标为白底彩),除非调用方设置自定义文本色。初始化为真实颜色,避免无效 QColor
        // 陷阱(未赋值颜色 alpha()==255 → 不透明黑)。
        textColor = m_customTextColor.isValid() ? m_customTextColor : QColor(Qt::white);
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(m_badgeOpacity);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);

    const qreal maxInset = qMax<qreal>(0.0, qMin(badgeRect.width(), badgeRect.height()) / 2.0 - 0.5);
    const qreal backgroundInset = qMin<qreal>(m_badgeBackgroundInset, maxInset);
    const QRectF backgroundRect = badgeRect.adjusted(
        backgroundInset,
        backgroundInset,
        -backgroundInset,
        -backgroundInset);

    if (qFuzzyCompare(backgroundRect.width(), backgroundRect.height())) {
        painter.drawEllipse(backgroundRect);
    } else {
        const qreal radius = backgroundRect.height() / 2.0;
        painter.drawRoundedRect(backgroundRect, radius, radius);
    }

    const InfoBadgeDisplayMode mode = effectiveDisplayMode();
    if (mode == InfoBadgeDisplayMode::Dot) return;

    painter.setPen(textColor);
    painter.setBrush(Qt::NoBrush);

    if (mode == InfoBadgeDisplayMode::Icon && !m_iconGlyph.isEmpty()) {
        if (m_iconFontFamily == Typography::FontFamily::FluentIcons) {
            painter.setPen(textColor);
            painter.save();
            if (!m_contentOffset.isNull())
                painter.translate(m_contentOffset);
            Typography::Icons::paintGlyph(
                painter, badgeRect, m_iconGlyph, m_iconGlyphSize, Qt::AlignCenter);
            painter.restore();
        } else {
            drawCenteredTextPath(painter, badgeRect, iconFont(), m_iconGlyph, textColor);
        }
        return;
    }

    if (mode == InfoBadgeDisplayMode::Value) {
        const QString text = valueText();
        if (text.isEmpty()) return;
        drawCenteredTextPath(painter, badgeRect, valueFont(), text, textColor);
    }
}

void InfoBadge::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange) {
        update();
    }
}

QString InfoBadge::valueText() const
{
    return m_value >= 0 ? QString::number(m_value) : QString();
}

QFont InfoBadge::valueFont() const
{
    QFont font = themeFont(m_valueFontRole).toQFont();
    font.setBold(false);
    return font;
}

QFont InfoBadge::iconFont() const
{
    QFont font(m_iconFontFamily);
    font.setPixelSize(m_iconGlyphSize);
    return font;
}

void InfoBadge::drawCenteredTextPath(QPainter& painter, const QRectF& targetRect, const QFont& font, const QString& text, const QColor& color) const
{
    if (text.isEmpty() || targetRect.isEmpty()) return;

    QPainterPath textPath;
    textPath.addText(QPointF(0, 0), font, text);
    const QRectF textBounds = textPath.boundingRect();
    if (textBounds.isEmpty()) return;

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(color.isValid() ? color : effectiveForegroundColor());
    painter.translate(
        targetRect.center().x() - textBounds.center().x() + m_contentOffset.x(),
        targetRect.center().y() - textBounds.center().y() + m_contentOffset.y());
    painter.drawPath(textPath);
    painter.restore();
}

QColor InfoBadge::statusBackgroundColor() const
{
    const auto& colors = themeColors();
    switch (m_status) {
        case InfoBadgeStatus::Informational:
            return colors.systemInfo;
        case InfoBadgeStatus::Caution:
            return colors.systemCaution;
        case InfoBadgeStatus::Success:
            return colors.systemSuccess;
        case InfoBadgeStatus::Critical:
            return colors.systemCritical;
        case InfoBadgeStatus::Attention:
        default:
            return colors.accentDefault;
    }
}

void InfoBadge::updateThemeColors()
{
    const auto& colors = themeColors();
    m_backgroundColor = m_customBackgroundColor.isValid()
        ? m_customBackgroundColor
        : statusBackgroundColor();
    m_foregroundColor = m_customTextColor.isValid()
        ? m_customTextColor
        : colors.textOnAccent;
    m_disabledBackgroundColor = colors.accentDisabled;
    m_disabledForegroundColor = colors.textDisabled;
}

void InfoBadge::invalidateLayoutAndPaint()
{
    updateGeometry();
    update();
}

} // namespace fluent::status_info