#include "InfoBadge.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QHideEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QShowEvent>
#include <QSizePolicy>
#include <QVariant>
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

#if QT_CONFIG(accessibility)

class InfoBadgeAccessible final : public QAccessibleWidget {
public:
    explicit InfoBadgeAccessible(InfoBadge* badge)
        : QAccessibleWidget(badge, QAccessible::StaticText)
    {
    }

    QString text(QAccessible::Text type) const override
    {
        auto* badge = qobject_cast<InfoBadge*>(widget());
        if (!badge)
            return {};

        if (type == QAccessible::Name) {
            const QString explicitName = badge->accessibleName();
            if (!explicitName.isEmpty())
                return explicitName;
        }
        if ((type == QAccessible::Name
             || type == QAccessible::Value)
            && badge->effectiveDisplayMode()
                == InfoBadge::InfoBadgeDisplayMode::Value
            && badge->value() >= 0) {
            return QString::number(badge->value());
        }
        return QAccessibleWidget::text(type);
    }
};

QAccessibleInterface* infoBadgeAccessibilityFactory(
    const QString&, QObject* object)
{
    auto* badge = qobject_cast<InfoBadge*>(object);
    return badge ? new InfoBadgeAccessible(badge) : nullptr;
}

void ensureInfoBadgeAccessibilityFactory()
{
    static const bool installed = []() {
        QAccessible::installFactory(
            infoBadgeAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
}

#else

void ensureInfoBadgeAccessibilityFactory()
{
}

#endif
} // namespace

InfoBadge::InfoBadge(QWidget* parent)
    : QWidget(parent)
    , m_valueFontRole(Typography::FontRole::Caption)
{
    ensureInfoBadgeAccessibilityFactory();
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    updateThemeColors();
}

void InfoBadge::setValue(int value)
{
    const int normalizedValue = std::max(-1, value);
    if (m_value == normalizedValue) return;
    m_value = normalizedValue;
    invalidateLayoutAndPaint();
    emit valueChanged(m_value);
    notifyAccessibleValueChanged();
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
    notifyAccessibleValueChanged();
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

void InfoBadge::setValueFontRole(Typography::FontRole role)
{
    if (m_valueFontRole == role) return;
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
    QColor fillColor = effectiveBackgroundColor();
    QColor textColor = effectiveForegroundColor();

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
    } else if (event->type() == QEvent::ParentChange) {
        notifyAccessibleParentReordered();
    }
}

void InfoBadge::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    notifyAccessibleParentReordered();
}

void InfoBadge::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    notifyAccessibleParentReordered();
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
    const auto& colors = themeColorsRef();
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
    const auto& colors = themeColorsRef();
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

void InfoBadge::notifyAccessibleValueChanged()
{
#if QT_CONFIG(accessibility)
    const QVariant value =
        effectiveDisplayMode() == InfoBadgeDisplayMode::Value
            && m_value >= 0
        ? QVariant(m_value)
        : QVariant();
    QAccessibleValueChangeEvent valueEvent(this, value);
    QAccessible::updateAccessibility(&valueEvent);

    QAccessibleEvent nameEvent(this, QAccessible::NameChanged);
    QAccessible::updateAccessibility(&nameEvent);

    if (QWidget* parent = parentWidget()) {
        QAccessibleEvent parentEvent(
            parent, QAccessible::VisibleDataChanged);
        QAccessible::updateAccessibility(&parentEvent);
    }
#endif
}

void InfoBadge::notifyAccessibleParentReordered()
{
#if QT_CONFIG(accessibility)
    if (QWidget* parent = parentWidget()) {
        QAccessibleEvent event(
            parent, QAccessible::ObjectReorder);
        QAccessible::updateAccessibility(&event);
    }
#endif
}

} // namespace fluent::status_info
